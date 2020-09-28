#include "engine.h"
#include "clipboard.h"

#include "colors.h"
#include "keyboard.h"

#include <jtk/file_utils.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#define TAB_SPACE 8


#define DEFAULT_COLOR (A_NORMAL | COLOR_PAIR(default_color))

namespace
  {
  int font_width, font_height;
  }

void get_editor_window_size(int& rows, int& cols)
  {
  getmaxyx(stdscr, rows, cols);
  rows -= 5;
  --cols;
  }

uint16_t character_to_pdc_char(uint32_t character, uint32_t char_id)
  {
  if (character > 65535)
    return '?';
  switch (character)
    {
    case 9: return 32;
    case 10: return 32;
    case 13: return 32;
    default: return (uint16_t)character;
    }
  }

void write_center(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    size_t offset = (out.length() - in.length()) / 2;
    for (size_t j = 0; j < in.length(); ++j)
      out[offset + j] = in[j];
    }
  }

void write_right(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    size_t offset = (out.length() - in.length());
    for (size_t j = 0; j < in.length(); ++j)
      out[offset + j] = in[j];
    }
  }

void write_left(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    for (size_t j = 0; j < in.length(); ++j)
      out[j] = in[j];
    }
  }

void draw_music_info(app_state state, const music& m)
  {
  if (!state.playing)
    return;

  attron(A_ITALIC);
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  move(rows - 3, 0);
  clrtoeol();
  
  std::stringstream str;
  str << "Sample rate: " << m.get_sample_rate();
  str << "  ";
  if (m.is_byte())
    str << "ByteBeat   ";
  else
    str << "FloatBeat  ";
  str << "t: " << m.get_timer();
  std::string line = str.str();
  line = line.substr(0, cols);
  while (line.length() < cols)
    line.push_back(' ');
  for (auto s : line)
    addch(s);
  attroff(A_ITALIC);
  refresh();
  }

void draw_title_bar(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  attrset(DEFAULT_COLOR);
  attron(A_REVERSE);
  attron(A_BOLD);
  std::wstring title_bar(cols, ' ');

  std::wstring filename = L"file: " + (state.buffer.name.empty() ? std::wstring(L"<noname>") : jtk::convert_string_to_wstring(state.buffer.name));
  write_center(title_bar, filename);

  if ((state.buffer.modification_mask & 1) == 1)
    write_right(title_bar, L" Modified ");

  for (int i = 0; i < cols; ++i)
    {
    move(0, i);
    addch(title_bar[i]);
    }
  }

#define MULTILINEOFFSET 10

struct keyword_data
  {
  std::vector<std::wstring> keywords_1, keywords_2;
  };

/*
Returns an x offset (let's call it multiline_offset_x) such that
  int x = (int)current.col + multiline_offset_x + wide_characters_offset;
equals the x position in the screen of where the next character should come.
This makes it possible to further fill the line with spaces after calling "draw_line".
*/
int draw_line(int& wide_characters_offset, file_buffer fb, position& current, position cursor, position buffer_pos, position underline, chtype base_color, int r, int xoffset, int maxcol, std::optional<position> start_selection, bool rectangular, bool active, const keyword_data& kd, const env_settings& senv)
  {
  auto tt = get_text_type(fb, current.row);

  line ln = fb.content[current.row];

  wide_characters_offset = 0;
  bool has_selection = (start_selection != std::nullopt) && (cursor.row >= 0) && (cursor.col >= 0);

  int64_t len = line_length_up_to_column(ln, maxcol - 1, senv);

  bool multiline = (cursor.row == current.row) && (len >= (maxcol - 1));
  int64_t multiline_ref_col = cursor.col;

  if (!multiline && has_selection)
    {
    if (!rectangular)
      {
      if (start_selection->row == current.row || cursor.row == current.row)
        {
        multiline_ref_col = ln.size();
        if (start_selection->row == current.row && start_selection->col < multiline_ref_col)
          multiline_ref_col = start_selection->col;
        if (cursor.row == current.row && cursor.col < multiline_ref_col)
          multiline_ref_col = cursor.col;
        if (multiline_ref_col < ln.size())
          {
          multiline = (len >= (maxcol - 1));
          }
        }
      else if ((start_selection->row > current.row && cursor.row < current.row) || (start_selection->row < current.row && cursor.row > current.row))
        {
        multiline_ref_col = 0;
        multiline = (len >= (maxcol - 1));
        }
      }
    else
      {
      int64_t min_col = start_selection->col;
      int64_t min_row = start_selection->row;
      int64_t max_col = buffer_pos.col;
      int64_t max_row = buffer_pos.row;
      if (max_col < min_col)
        std::swap(max_col, min_col);
      if (max_row < min_row)
        std::swap(max_row, min_row);
      if (current.row >= min_row && current.row <= max_row)
        {
        multiline_ref_col = min_col;
        if (multiline_ref_col >= (maxcol - 1) && (multiline_ref_col < ln.size()))
          multiline = true;
        }
      }
    }


  auto it = ln.begin();
  auto it_end = ln.end();

  int page = 0;

  if (multiline)
    {
    int pagewidth = maxcol - 2 - MULTILINEOFFSET;
    int64_t len_to_cursor = line_length_up_to_column(ln, multiline_ref_col - 1, senv);
    page = len_to_cursor / pagewidth;
    if (page != 0)
      {
      if (len_to_cursor == multiline_ref_col - 1) // no characters wider than 1 so far.
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        it += offset;
        current.col += offset;
        xoffset -= offset;
        }
      else
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        current.col = get_col_from_line_length(ln, offset, senv);
        int64_t length_done = line_length_up_to_column(ln, current.col - 1, senv);
        it += current.col;
        wide_characters_offset = length_done - (current.col - 1);
        xoffset -= current.col + wide_characters_offset;
        }
      move((int)r, (int)current.col + xoffset + wide_characters_offset);
      attron(COLOR_PAIR(multiline_tag));
      addch('$');
      attron(base_color);
      ++xoffset;
      --maxcol;
      }
    --maxcol;
    }

  int drawn = 0;
  auto current_tt = tt.back();
  assert(current_tt.first == 0);
  tt.pop_back();

  int next_word_read_length_remaining = 0;
  bool keyword_type_1 = false;
  bool keyword_type_2 = false;

  for (; it != it_end; ++it)
    {
    if (next_word_read_length_remaining > 0)
      --next_word_read_length_remaining;
    if (drawn >= maxcol)
      break;

    while (!tt.empty() && tt.back().first <= current.col)
      {
      current_tt = tt.back();
      tt.pop_back();
      }

    if (!(kd.keywords_1.empty() && kd.keywords_2.empty()) && current_tt.second == tt_normal && next_word_read_length_remaining == 0)
      {
      keyword_type_1 = false;
      keyword_type_2 = false;
      std::wstring next_word = read_next_word(it, it_end);
      next_word_read_length_remaining = next_word.length();
      auto it = std::lower_bound(kd.keywords_1.begin(), kd.keywords_1.end(), next_word);
      if (it != kd.keywords_1.end() && *it == next_word)
        keyword_type_1 = true;
      else
        {
        it = std::lower_bound(kd.keywords_2.begin(), kd.keywords_2.end(), next_word);
        if (it != kd.keywords_2.end() && *it == next_word)
          keyword_type_2 = true;
        }
      }

    switch (current_tt.second)
      {
      case tt_normal:
      {
      if (keyword_type_1)
        attron(COLOR_PAIR(keyword_color));
      else if (keyword_type_2)
        attron(COLOR_PAIR(keyword_2_color));
      else
        attron(base_color);
      break;
      }
      case tt_string: attron(COLOR_PAIR(string_color)); break;
      case tt_comment: attron(COLOR_PAIR(comment_color)); break;
      }

    if (active && in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
      attron(A_REVERSE);
    else
      attroff(A_REVERSE);

    if (!has_selection && (current == cursor))
      {
      attron(A_REVERSE);
      }

    attroff(A_UNDERLINE | A_ITALIC);
    if ((current == cursor) && valid_position(fb, underline))
      attron(A_UNDERLINE | A_ITALIC);
    if (current == underline)
      attron(A_UNDERLINE | A_ITALIC);

    move((int)r, (int)current.col + xoffset + wide_characters_offset);
    auto character = *it;
    uint32_t cwidth = character_width(character, current.col + wide_characters_offset, senv);
    for (uint32_t cnt = 0; cnt < cwidth && drawn < maxcol; ++cnt)
      {
      addch(character_to_pdc_char(character, cnt));
      ++drawn;
      }
    wide_characters_offset += cwidth - 1;
    ++current.col;
    }

  if (!in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
    attroff(A_REVERSE);

  if (multiline && (it != it_end))
    {
    attroff(A_REVERSE);
    attron(COLOR_PAIR(multiline_tag));
    addch('$');
    attron(base_color);
    ++xoffset;
    }

  return xoffset;
  }

std::string get_operation_text(e_operation op)
  {
  switch (op)
    {
    case op_open: return std::string("Open file: ");
    case op_save: return std::string("Save file: ");
    case op_query_save: return std::string("Save file: ");
    default: return std::string();
    }
  }

void draw_help_line(const std::string& text, int r, int sz)
  {
  attrset(DEFAULT_COLOR);
  move(r, 0);
  int length = (int)text.length();
  if (length > sz)
    length = sz;

  for (int i = 0; i < length; ++i)
    {
    if (i % 10 == 0 || i % 10 == 1)
      attron(A_REVERSE);
    addch(text[i]);
    if (i % 10 == 0 || i % 10 == 1)
      attroff(A_REVERSE);
    }
  }

void draw_help_text(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  if (state.operation == op_editing)
    {
    static std::string line1("^N New    ^O Open   ^S Save   ^C Copy   ^V Paste  ^Z Undo   ^Y Redo   ^A Sel/all");
    static std::string line2("^H Help   ^X Exit   ^B Build  ^P Play   ^R Restart^E Export");
    static std::string line3("^H Help   ^X Exit   ^B Build  ^P Pause  ^R Restart^E Export");
    draw_help_line(line1, rows - 2, cols);
    if (state.playing)
      draw_help_line(line3, rows - 1, cols);
    else
      draw_help_line(line2, rows - 1, cols);
    }
  if (state.operation == op_open)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_save)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_query_save)
    {
    static std::string line1("^X Cancel ^Y Yes    ^N No");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_help)
    {
    static std::string line1("^X Back");
    draw_help_line(line1, rows - 2, cols);
    }
  }

namespace
  {
  std::vector<std::wstring> break_string(std::string in)
    {
    std::vector<std::wstring> out;
    while (!in.empty())
      {
      auto splitpos = in.find_first_of(' ');
      std::string keyword = in.substr(0, splitpos);
      if (splitpos != std::string::npos)
        in.erase(0, splitpos + 1);
      else
        in.clear();
      out.push_back(jtk::convert_string_to_wstring(keyword));
      }
    return out;
    }

  keyword_data _generate_keyword_data()
    {
    keyword_data kd;

    std::string in = "t + - * / & | ^ >> << not sin cos % < > <= >= = <> dup pick drop 2dup over nip tuck swap rot -rot min max pow atan2 negate tan log exp sqrt floor ceil abs";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = ": ; #samplerate #byte #float";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }
  }

const keyword_data& get_keyword_data()
  {
  static keyword_data kd = _generate_keyword_data();
  return kd;
  }

void draw_buffer(file_buffer fb, int64_t scroll_row, const env_settings& senv)
  {
  int offset_x = 1;
  int offset_y = 1;

  int maxrow, maxcol;
  get_editor_window_size(maxrow, maxcol);

  position current;
  current.row = scroll_row;

  position cursor = get_actual_position(fb);

  bool has_nontrivial_selection = (fb.start_selection != std::nullopt) && (fb.start_selection != fb.pos);

  position underline(-1, -1);
  if (!has_nontrivial_selection)
    {
    underline = find_corresponding_token(fb, cursor, current.row, current.row + maxrow - 1);
    }

  const keyword_data& kd = get_keyword_data();

  attrset(DEFAULT_COLOR);
  
  int r = 0;
  for (; r < maxrow; ++r)
    {
    current.col = 0;
    if (current.row >= fb.content.size())
      {
      if (fb.content.empty()) // file is empty, draw cursor
        {
        move((int)r + offset_y, (int)current.col + offset_x);
        attron(A_REVERSE);
        addch(' ');
        attroff(A_REVERSE);
        }
      break;
      }

    //int draw_line(int& wide_characters_offset, file_buffer fb, position& current, position cursor, position buffer_pos, position underline, chtype base_color, int r, int xoffset, int maxcol, std::optional<position> start_selection, bool rectangular, bool active, const keyword_data& kd, const env_settings& senv)

    int wide_characters_offset = 0;    
    int multiline_offset_x = draw_line(wide_characters_offset, fb, current, cursor, fb.pos, underline, DEFAULT_COLOR, r + offset_y, offset_x, maxcol, fb.start_selection, fb.rectangular_selection, true, kd, senv);

    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if (!has_nontrivial_selection && (current == cursor))
      {
      move((int)r + offset_y, x);
      assert(current.row == fb.content.size() - 1);
      assert(current.col == fb.content.back().size());
      attron(A_REVERSE);
      addch(' ');
      ++x;
      ++current.col;
      }
    attroff(A_REVERSE);
    while (x < maxcol)
      {
      move((int)r + offset_y, (int)x);
      addch(' ');
      ++current.col;
      ++x;
      }

    ++current.row;
    }
  }

app_state draw(app_state state, const music& m)
  {
  erase();

  draw_title_bar(state);

  if (state.operation == op_help)
    {
    state.operation_buffer.pos.col = -1;
    state.operation_buffer.pos.row = -1;
    draw_buffer(state.operation_buffer, state.operation_scroll_row, state.senv);
    }
  else
    draw_buffer(state.buffer, state.scroll_row, state.senv);

  if (state.operation != op_editing && state.operation != op_help)
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    auto cursor = get_actual_position(state.operation_buffer);
    position current;
    current.col = 0;
    current.row = 0;
    std::string txt = get_operation_text(state.operation);
    move((int)rows - 4, 0);
    attrset(DEFAULT_COLOR);   
    for (auto ch : txt)
      addch(ch);
    int maxrow, maxcol;
    get_editor_window_size(maxrow, maxcol);
    int cols_available = maxcol - txt.length();
    int off_x = txt.length();
    int wide_characters_offset = 0;
    int multiline_offset_x = txt.length();
    keyword_data kd;
    if (!state.operation_buffer.content.empty())
      multiline_offset_x = draw_line(wide_characters_offset, state.operation_buffer, current, cursor, state.operation_buffer.pos, position(-1, -1), DEFAULT_COLOR | A_BOLD, rows - 4, multiline_offset_x, cols_available, state.operation_buffer.start_selection, state.operation_buffer.rectangular_selection, true, kd, state.senv);
    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if ((current == cursor))
      {
      move((int)rows - 4, (int)x);
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }
    }
  else
    {
    attrset(DEFAULT_COLOR);
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int message_length = (int)state.message.size();
    int offset = (cols - message_length) / 2;
    if (offset > 0)
      {
      for (auto ch : state.message)
        {
        move(rows - 4, offset);
        addch(ch);
        ++offset;
        }
      }
    }

  draw_help_text(state);

  draw_music_info(state, m);

  curs_set(0);
  refresh();

  return state;
  }

app_state check_scroll_position(app_state state)
  {
  int rows, cols;
  get_editor_window_size(rows, cols);
  if (state.scroll_row > state.buffer.pos.row)
    state.scroll_row = state.buffer.pos.row;
  else if (state.scroll_row + rows <= state.buffer.pos.row)
    {
    state.scroll_row = state.buffer.pos.row - rows + 1;
    }
  return state;
  }

app_state check_operation_scroll_position(app_state state)
  {
  int rows, cols;
  get_editor_window_size(rows, cols);
  int64_t lastrow = (int64_t)state.operation_buffer.content.size() - 1;
  if (lastrow < 0)
    lastrow = 0;

  if (state.operation_scroll_row + rows > lastrow + 1)
    state.operation_scroll_row = lastrow - rows + 1;
  if (state.operation_scroll_row < 0)
    state.operation_scroll_row = 0;
  return state;
  }

app_state check_operation_buffer(app_state state)
  {
  if (state.operation_buffer.content.size() > 1)
    state.operation_buffer.content = state.operation_buffer.content.take(1);
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state cancel_selection(app_state state)
  {
  if (!keyb_data.selecting)
    {
    if (state.operation == op_editing)
      state.buffer = clear_selection(state.buffer);
    else
      state.operation_buffer = clear_selection(state.operation_buffer);
    }
  return state;
  }

app_state move_left_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_left(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state move_left_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  position actual = get_actual_position(state.operation_buffer);
  if (actual.col > 0)
    state.operation_buffer.pos.col = actual.col - 1;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_left(app_state state)
  {
  if (state.operation == op_editing)
    return move_left_editor(state);
  return move_left_operation(state);
  }

app_state move_right_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_right(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state move_right_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  if (state.operation_buffer.pos.col < (int64_t)state.operation_buffer.content[0].size())
    ++state.operation_buffer.pos.col;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_right(app_state state)
  {
  if (state.operation == op_editing)
    return move_right_editor(state);
  return move_right_operation(state);
  }

app_state move_up_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_up(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state move_up_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_scroll_row > 0)
    --state.operation_scroll_row;
  return state;
  }

app_state move_up(app_state state)
  {
  if (state.operation == op_editing)
    return move_up_editor(state);
  else if (state.operation == op_help)
    return move_up_operation(state);
  return state;
  }

app_state move_down_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_down(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state move_down_operation(app_state state)
  {
  state = cancel_selection(state);
  ++state.operation_scroll_row;
  return check_operation_scroll_position(state);
  }

app_state move_down(app_state state)
  {
  if (state.operation == op_editing)
    return move_down_editor(state);
  else if (state.operation == op_help)
    return move_down_operation(state);
  return state;
  }

app_state move_page_up_editor(app_state state)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols);

  state.scroll_row -= rows - 1;
  if (state.scroll_row < 0)
    state.scroll_row = 0;

  state.buffer = move_page_up(state.buffer, rows - 1, state.senv);

  return check_scroll_position(state);
  }

app_state move_page_up_operation(app_state state)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols);

  state.operation_scroll_row -= rows - 1;
  if (state.operation_scroll_row < 0)
    state.operation_scroll_row = 0;

  return state;
  }

app_state move_page_up(app_state state)
  {
  if (state.operation == op_editing)
    return move_page_up_editor(state);
  else if (state.operation == op_help)
    return move_page_up_operation(state);
  return state;
  }

app_state move_page_down_editor(app_state state)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols);
  state.scroll_row += rows - 1;
  if (state.scroll_row + rows >= state.buffer.content.size())
    state.scroll_row = (int64_t)state.buffer.content.size() - rows + 1;
  if (state.scroll_row < 0)
    state.scroll_row = 0;
  state.buffer = move_page_down(state.buffer, rows - 1, state.senv);
  return check_scroll_position(state);
  }

app_state move_page_down_operation(app_state state)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols);
  state.operation_scroll_row += rows - 1;
  return check_operation_scroll_position(state);
  }

app_state move_page_down(app_state state)
  {
  if (state.operation == op_editing)
    return move_page_down_editor(state);
  else if (state.operation == op_help)
    return move_page_down_operation(state);
  return state;
  }

app_state move_home_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_home(state.buffer, state.senv);
  return state;
  }

app_state move_home_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state = cancel_selection(state);
  state.operation_buffer.pos.col = 0;
  return state;
  }

app_state move_home(app_state state)
  {
  if (state.operation == op_editing)
    return move_home_editor(state);
  return move_home_operation(state);
  }

app_state move_end_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_end(state.buffer, state.senv);
  return state;
  }

app_state move_end_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;

  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;

  state.operation_buffer.pos.col = (int64_t)state.operation_buffer.content[0].size();
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_end(app_state state)
  {
  if (state.operation == op_editing)
    return move_end_editor(state);
  return move_end_operation(state);
  }


app_state tab_editor(app_state state)
  {
  std::string t("\t");
  state.buffer = insert(state.buffer, t, state.senv);
  return state;
  }

app_state tab_operation(app_state state)
  {
  std::string t("\t");
  state.operation_buffer = insert(state.operation_buffer, t, state.senv);
  return state;
  }

app_state tab(app_state state)
  {
  if (state.operation == op_editing)
    return tab_editor(state);
  return tab_operation(state);
  }

app_state text_input_editor(app_state state, const char* txt)
  {
  std::string t(txt);
  state.buffer = insert(state.buffer, t, state.senv);
  return check_scroll_position(state);
  }

app_state text_input_operation(app_state state, const char* txt)
  {
  if (state.operation == op_help)
    return state;

  std::string t(txt);
  state.operation_buffer = insert(state.operation_buffer, t, state.senv);
  return check_operation_buffer(state);
  }

app_state text_input(app_state state, const char* txt)
  {
  if (state.operation == op_editing)
    return text_input_editor(state, txt);
  return text_input_operation(state, txt);
  }

app_state backspace_editor(app_state state)
  {
  state.buffer = erase(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state backspace_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state.operation_buffer = erase(state.operation_buffer, state.senv);
  return check_operation_buffer(state);
  }

app_state backspace(app_state state)
  {
  if (state.operation == op_editing)
    return backspace_editor(state);
  return backspace_operation(state);
  }

app_state del_editor(app_state state)
  {
  state.buffer = erase_right(state.buffer, state.senv);
  return check_scroll_position(state);
  }

app_state del_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state.operation_buffer = erase_right(state.operation_buffer, state.senv);
  return check_operation_buffer(state);
  }

app_state del(app_state state)
  {
  if (state.operation == op_editing)
    return del_editor(state);
  return del_operation(state);
  }

app_state ret_editor(app_state state)
  {
  return text_input(state, "\n");
  }

line string_to_line(const std::string& txt)
  {
  line out;
  auto trans = out.transient();
  for (auto ch : txt)
    trans.push_back(ch);
  return trans.persistent();
  }

std::string clean_filename(std::string name)
  {
  while (!name.empty() && name.back() == ' ')
    name.pop_back();
  while (!name.empty() && name.front() == ' ')
    name.erase(name.begin());
  if (name.size() > 2 && name.front() == '"' && name.back() == '"')
    {
    name.erase(name.begin());
    name.pop_back();
    }
  return name;
  }

file_buffer set_multiline_comments(file_buffer fb)
  {
  fb.syntax.multiline_begin = "/*";
  fb.syntax.multiline_end = "*/";
  fb.syntax.single_line = "//";
  return fb;
  }


app_state compile_buffer(app_state state, compiler& c, music& m)
  {
  try
    {
    bool _float = true;
    uint64_t sample_rate = 8000;
    //preprocessor
    auto it = state.buffer.content.begin();
    auto it_end = state.buffer.content.end();
    for (; it != it_end; ++it)
      {
      auto ln = *it;
      auto line_it = ln.begin();
      auto line_it_end = ln.end();
      while (line_it != line_it_end && (*line_it == L' ' || *line_it == L'\t'))
        ++line_it;
      std::wstring first_word = read_next_word(line_it, line_it_end);
      if (first_word == L"#samplerate")
        {
        line_it += first_word.length();
        while (line_it != line_it_end && (*line_it == L' ' || *line_it == L'\t'))
          ++line_it;
        std::wstring second_word = read_next_word(line_it, line_it_end);
        std::wstringstream str;
        str << second_word;        
        str >> sample_rate;        
        }
      else if (first_word == L"#byte")
        {
        _float = false;
        }
      else if (first_word == L"#float")
        {
        _float = true;
        }
      else if (!first_word.empty() && first_word[0] == L'#')
        {
        std::stringstream str;
        str << "Unknown preprocessor directive: " << jtk::convert_wstring_to_string(first_word);
        throw std::logic_error(str.str());
        }
      }
    if (_float)
      c.compile_float(buffer_to_string(state.buffer));
    else
      c.compile_byte(buffer_to_string(state.buffer));
    if (_float)
      m.set_float();
    else
      m.set_byte();
    m.set_sample_rate(sample_rate);   
    state.message = string_to_line("[Build succeeded]");
    }
  catch (std::logic_error& e)
    {
    state.message = string_to_line(e.what());
    }
  return state;
  }

app_state open_file(app_state state, compiler& c, music& m)
  {
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' by '/'
  std::string filename = clean_filename(jtk::convert_wstring_to_string(wfilename));
  if (filename.find(' ') != std::string::npos)
    {
    filename.push_back('"');
    filename.insert(filename.begin(), '"');
    }
  if (!jtk::file_exists(filename))
    {
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string error_message = "File " + filename + " not found";
    state.message = string_to_line(error_message);
    }
  else
    {
    state.buffer = read_from_file(filename);
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string message = "Opened file " + filename;
    state.message = string_to_line(message);
    }
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);
  state = compile_buffer(state, c, m);
  return state;
  }

app_state save_file(app_state state)
  {
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' by '/'
  std::string filename = clean_filename(jtk::convert_wstring_to_string(wfilename));
  if (filename.find(' ') != std::string::npos)
    {
    filename.push_back('"');
    filename.insert(filename.begin(), '"');
    }
  bool success = false;
  state.buffer = save_to_file(success, state.buffer, filename);
  if (success)
    {
    state.buffer.name = filename;
    std::string message = "Saved file " + filename;
    state.message = string_to_line(message);
    }
  else
    {
    std::string error_message = "Error saving file " + filename;
    state.message = string_to_line(error_message);
    }
  return state;
  }

app_state make_save_buffer(app_state state);

std::optional<app_state> exit(app_state state)
  {
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_exit);
    return make_save_buffer(state);
    }
  else
    return std::nullopt;
  }

app_state make_new_buffer(app_state state)
  {
  state.buffer = make_empty_buffer();
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);
  state.scroll_row = 0;
  state.message = string_to_line("[New]");
  return state;
  }

std::optional<app_state> ret_operation(app_state state, compiler& c, music& m)
  {
  bool done = false;
  while (!done)
    {
    switch (state.operation)
      {
      case op_open: state = open_file(state, c, m); break;
      case op_save: state = save_file(state); break;
      case op_query_save: state = save_file(state); break;
      case op_new: state = make_new_buffer(state); break;
      case op_exit: return std::nullopt;
      default: break;
      }
    if (state.operation_stack.empty())
      {
      state.operation = op_editing;
      done = true;
      }
    else
      {
      state.operation = state.operation_stack.back();
      state.operation_stack.pop_back();
      }
    }
  return state;
  }

std::optional<app_state> ret(app_state state, compiler& c, music& m)
  {
  if (state.operation == op_editing)
    return ret_editor(state);
  return ret_operation(state, c, m);
  }

app_state clear_operation_buffer(app_state state)
  {
  state.operation_buffer.content = text();
  state.operation_buffer.history = immutable::vector<snapshot, false>();
  state.operation_buffer.undo_redo_index = 0;
  state.operation_buffer.start_selection = std::nullopt;
  state.operation_buffer.pos.row = 0;
  state.operation_buffer.pos.col = 0;
  state.operation_scroll_row = 0;
  return state;
  }

app_state make_save_buffer(app_state state)
  {
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, state.buffer.name, state.senv, false);
  return state;
  }

app_state new_buffer(app_state state)
  {
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_new);
    return make_save_buffer(state);
    }
  return make_new_buffer(state);
  }

std::optional<app_state> cancel(app_state state)
  {
  if (state.operation == op_editing)
    {
    return exit(state);
    }
  else
    {
    if (state.operation != op_help)
      state.message = string_to_line("[Cancelled]");
    state.operation = op_editing;
    state.operation_stack.clear();
    }
  return state;
  }

app_state stop_selection(app_state state)
  {
  if (keyb_data.selecting)
    {
    keyb_data.selecting = false;
    }
  return state;
  }

app_state undo(app_state state)
  {
  state.message = string_to_line("[Undo]");
  if (state.operation == op_editing)
    state.buffer = undo(state.buffer, state.senv);
  else
    state.operation_buffer = undo(state.operation_buffer, state.senv);
  return check_scroll_position(state);
  }

app_state redo(app_state state)
  {
  state.message = string_to_line("[Redo]");
  if (state.operation == op_editing)
    state.buffer = redo(state.buffer, state.senv);
  else
    state.operation_buffer = redo(state.operation_buffer, state.senv);
  return check_scroll_position(state);
  }

app_state copy_to_snarf_buffer(app_state state)
  {
  state.message = string_to_line("[Copy]");
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(state.buffer, state.senv);
  else
    state.snarf_buffer = get_selection(state.operation_buffer, state.senv);
#ifdef _WIN32
  std::wstring txt;
  for (const auto& ln : state.snarf_buffer)
    {
    for (const auto& ch : ln)
      txt.push_back(ch);
    }
  copy_to_windows_clipboard(jtk::convert_wstring_to_string(txt));
#endif
  return state;
  }

app_state paste_from_snarf_buffer(app_state state)
  {
  state.message = string_to_line("[Paste]");
#ifdef _WIN32
  auto txt = get_text_from_windows_clipboard();
  if (state.operation == op_editing)
    state.buffer = insert(state.buffer, txt, state.senv);
  else
    state.operation_buffer = insert(state.operation_buffer, txt, state.senv);
#else
  if (state.operation == op_editing)
    state.buffer = insert(state.buffer, state.snarf_buffer, state.senv);
  else
    state.operation_buffer = insert(state.operation_buffer, state.snarf_buffer, state.senv);
#endif
  return check_scroll_position(state);
  }

app_state select_all(app_state state)
  {
  state.message = string_to_line("[Select all]");
  if (state.operation == op_editing)
    state.buffer = select_all(state.buffer, state.senv);
  else
    state.operation_buffer = select_all(state.operation_buffer, state.senv);
  return check_scroll_position(state);
  }

app_state make_help_buffer(app_state state)
  {
  std::string txt = R"(Help
----

sdf
adsf
sdf
sdf
sadf
asdf
asdf
asdf
sadf
asdf
sadf
asdff
asdf
asdf
asdf
sadf
asdf
asdf
asdf
asdf
asd
fasdf
asd
fasd
fasd
f
)";
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, txt, state.senv, false);
  state.message = string_to_line("[Help]");
  return state;
  }

std::optional<app_state> process_input(app_state state, compiler& c, music& m)
  {
  SDL_Event event;
  auto tic = std::chrono::steady_clock::now();
  for (;;)
    {    
    while (SDL_PollEvent(&event))
      {
      keyb.handle_event(event);
      switch (event.type)
        {
        case SDL_WINDOWEVENT:
        {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
          {
          auto new_w = event.window.data1;
          auto new_h = event.window.data2;
          resize_term(new_h / font_height, new_w / font_width);
          return state;
          }
        break;
        }
        case SDL_TEXTINPUT:
        {
        return text_input(state, event.text.text);
        }
        case SDL_KEYDOWN:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LEFT: return move_left(state);
          case SDLK_RIGHT: return move_right(state);
          case SDLK_DOWN: return move_down(state);
          case SDLK_UP: return move_up(state);
          case SDLK_PAGEUP: return move_page_up(state);
          case SDLK_PAGEDOWN: return move_page_down(state);
          case SDLK_HOME: return move_home(state);
          case SDLK_END: return move_end(state);
          case SDLK_TAB: return tab(state);
          case SDLK_RETURN: return ret(state, c, m);
          case SDLK_BACKSPACE: return backspace(state);
          case SDLK_DELETE: return del(state);
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            break;
          keyb_data.selecting = true;
          if (state.operation == op_editing)
            {
            if (state.buffer.start_selection == std::nullopt)
              state.buffer.start_selection = get_actual_position(state.buffer);
            }
          else
            {
            if (state.operation_buffer.start_selection == std::nullopt)
              state.operation_buffer.start_selection = get_actual_position(state.operation_buffer);
            }
          return state;
          }
          case SDLK_a:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return select_all(state);
            }
          }
          case SDLK_b: // build
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return compile_buffer(state, c, m);
            }
          }
          case SDLK_c:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return copy_to_snarf_buffer(state);
            }
          }
          case SDLK_h:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_help;
            return make_help_buffer(state);
            }
          }
          case SDLK_p:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            if (state.playing)
              {
              state.message = string_to_line("[Pause]");
              m.stop();
              }
            else
              {
              state.message = string_to_line("[Play]");
              m.play();
              }
            state.playing = !state.playing;
            return state;
            }
          }
          case SDLK_n:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_query_save:
              {
              state.operation = state.operation_stack.back();
              state.operation_stack.pop_back();
              return ret(state, c, m);
              }
              default: return new_buffer(state);
              }
            }
          break;
          }
          case SDLK_o:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_open;
            return clear_operation_buffer(state);
            }
          }
          case SDLK_r:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.message = string_to_line("[Restart]");
            m.reset_timer();
            return state;
            }
          }
          case SDLK_s:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_save;
            return make_save_buffer(state);
            }
          }
          case SDLK_v:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return paste_from_snarf_buffer(state);
            }
          }
          case SDLK_x:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return cancel(state);
            }
          break;
          }
          case SDLK_y:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_query_save:
              {
              state.operation = op_save;
              return ret(state, c, m);
              }
              default: return redo(state);
              }
            }
          break;
          }
          case SDLK_z:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return undo(state);
            }
          break;
          }
          } // switch (event.key.keysym.sym)
        break;
        } // case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          }
        break;
        } // case SDLK_KEYUP:
        case SDL_QUIT: return exit(state);
        } // switch (event.type)
      }
    draw_music_info(state, m);
    SDL_UpdateWindowSurface(pdc_window);
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    }
  }

engine::engine(int argc, char** argv) : m(&c)
  {
  pdc_font_size = 17;
#ifdef _WIN32
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", pdc_font_size);
#elif defined(unix)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", pdc_font_size);
#elif defined(__APPLE__)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", pdc_font_size);
#endif

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;

  int w = font_width * 80;
  int h = font_height * 25;

  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();
  init_colors();
  bkgd(COLOR_PAIR(default_color));

  if (argc > 1)
    state.buffer = read_from_file(std::string(argv[1]));
  else
    state.buffer = make_empty_buffer();
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);  
  state.operation = op_editing;
  state.scroll_row = 0;
  state.operation_scroll_row = 0;
  state.playing = false;
  state.senv.show_all_characters = false;
  state.senv.tab_space = 8;
  state = compile_buffer(state, c, m);

  SDL_ShowCursor(1);
  SDL_SetWindowSize(pdc_window, w, h);

  SDL_DisplayMode DM;
  SDL_GetCurrentDisplayMode(0, &DM);

  SDL_SetWindowPosition(pdc_window, (DM.w - w) / 2, (DM.h - h) / 2);

  resize_term(h / font_height, w / font_width);

  }

engine::~engine()
  {

  }

void engine::run()
  {
  state = draw(state, m);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state, c, m))
    {
    state = *new_state;
    state = draw(state, m);

    SDL_UpdateWindowSurface(pdc_window);
    }
  }
