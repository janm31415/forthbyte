#include "engine.h"
#include "clipboard.h"

#include "colors.h"
#include "keyboard.h"
#include "preprocessor.h"
#include "utils.h"

#include <jtk/file_utils.h>
#include <jtk/pipe.h>

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
  
bool ctrl_pressed()
  {
#if defined(__APPLE__)
  if (keyb.is_down(SDLK_LGUI) || keyb.is_down(SDLK_RGUI))
    return true;
#endif
  return (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL));
  }

bool alt_pressed()
  {
  return (keyb.is_down(SDLK_LALT) || keyb.is_down(SDLK_RALT));
  }

bool shift_pressed()
  {
  return (keyb.is_down(SDLK_LSHIFT) || keyb.is_down(SDLK_RSHIFT));
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
  if (state.paused)
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
    case op_export: return std::string("Export wav file: ");
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
    static std::string line1("^N New    ^O Open   ^S Save   ^W Save as^C Copy   ^V Paste  ^Z Undo   ^Y Redo   ");
    static std::string line2("^H Help   ^X Exit   ^B Build  ^P Play   ^K Stop   ^R Restart^E Export ^A Sel/all");
    static std::string line3("^H Help   ^X Exit   ^B Build  ^P Pause  ^K Stop   ^R Restart^E Export ^A Sel/all");
    draw_help_line(line1, rows - 2, cols);
    if (state.playing && !state.paused)
      draw_help_line(line3, rows - 1, cols-1);
    else
      draw_help_line(line2, rows - 1, cols-1);
    }
  if (state.operation == op_open)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_export)
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

    std::string in = "! @ + - * / & | ^ >> << not sin cos % < > <= >= = <> dup pick drop 2dup over nip tuck swap rot -rot min max pow atan2 negate tan log exp sqrt floor ceil abs";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "t sr c : ; #samplerate #byte #float #initmemory";
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
  state.operation_buffer = move_home(state.operation_buffer, state.senv);
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

  state.operation_buffer = move_end(state.operation_buffer, state.senv);
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
  std::string indentation("\n");
  indentation.append(get_row_indentation_pattern(state.buffer, state.buffer.pos));
  return text_input(state, indentation.c_str());
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
  fb.syntax.should_highlight = true;
  return fb;
  }


app_state compile_buffer(app_state state, compiler& c, music& m)
  {
  try
    {
    auto sett = preprocess(state.buffer.content);
    m.set_sample_rate(sett._sample_rate);
    if (sett._float)
      {
      c.compile_float(buffer_to_string(state.buffer), sett);
      c.init_memory_float(sett.init_memory);
      m.set_float();
      }
    else
      {
      c.compile_byte(buffer_to_string(state.buffer), sett);
      c.init_memory_byte(sett.init_memory);
      m.set_byte();
      }
    
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
    std::string error_message = "[File " + filename + " not found]";
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
    std::string message = "[Opened file " + filename + "]";
    state.message = string_to_line(message);
    }
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);
  state = compile_buffer(state, c, m);
  return state;
  }


app_state put(app_state state)
  {
  std::string filename = state.buffer.name;
  bool success = false;
  state.buffer = save_to_file(success, state.buffer, filename);
  if (success)
    {
    state.buffer.name = filename;
    std::string message = "[Saved file " + filename + "]";
    state.message = string_to_line(message);
    }
  else
    {
    std::string error_message = "[Error saving file " + filename + "]";
    state.message = string_to_line(error_message);
    }
  return state;
  }
  
app_state export_file(app_state state, music& m)
  {
  m.set_session_filename(state.export_location);
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
    std::string message = "[Saved file " + filename + "]";
    state.message = string_to_line(message);
    }
  else
    {
    std::string error_message = "[Error saving file " + filename + "]";
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
      case op_export: state = export_file(state, m); break;
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
  
app_state export_location(app_state state)
  {
  state.operation = op_export;
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, state.export_location, state.senv, false);
  return state;
  }
  
char** alloc_arguments(const std::string& path, const std::vector<std::string>& parameters)
  {
  char** argv = new char*[parameters.size() + 2];
  argv[0] = const_cast<char*>(path.c_str());
  for (int j = 0; j < parameters.size(); ++j)
    argv[j + 1] = const_cast<char*>(parameters[j].c_str());
  argv[parameters.size() + 1] = nullptr;
  return argv;
  }

void free_arguments(char** argv)
  {
  delete[] argv;
  }

app_state copy_to_snarf_buffer(app_state state)
  {
  state.message = string_to_line("[Copy]");
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(state.buffer, state.senv);
  else
    state.snarf_buffer = get_selection(state.operation_buffer, state.senv);
#ifdef _WIN32
  std::wstring txt = to_wstring(state.snarf_buffer);
  copy_to_windows_clipboard(jtk::convert_wstring_to_string(txt));
#else
  std::string txt = to_string(state.snarf_buffer);
  int pipefd[3];
#if defined(__APPLE__)
  std::string pbcopy = get_file_path("pbcopy");
#else
  std::string pbcopy = get_file_path("xclip");
#endif
  char** argv = alloc_arguments(pbcopy, std::vector<std::string>());
  int err = jtk::create_pipe(pbcopy.c_str(), argv, nullptr, pipefd);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
    return state;
    }
  jtk::send_to_pipe(pipefd, txt.c_str());
  jtk::close_pipe(pipefd);
#endif
  return state;
  }
  
#ifndef _WIN32
std::string pbpaste()
  {
#if defined(__APPLE__)
  FILE* pipe = popen("pbpaste", "r");
#else
  FILE* pipe = popen("xclip -o", "r");
#endif
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while (!feof(pipe))
    {
    if (fgets(buffer, 128, pipe) != NULL)
      {
      result += buffer;
      }
    }
  pclose(pipe);
  return result;
  }
#endif

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
  std::string txt = pbpaste();
  if (state.operation == op_editing)
    state.buffer = insert(state.buffer, txt, state.senv);
  else
    state.operation_buffer = insert(state.operation_buffer, txt, state.senv);
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

Bytebeat is a type of music made from mathematical formulas, first discovered
by Viznut (http://viznut.fi/en/) in 2011.
The idea is that `t` represents a timer, infinitely increasing. In most cases
`t` increases 8000 times per second (for a 8000Hz bytebeat song), but you can
also let \ represent a 44100Hz timer if you like.
If you take the next formula with a 8000Hz timer

t  |  ( t % 5000 )
    
then a bytebeat generator will compute the above formula 8000 times per second
for the corresponding value of t. Next the result (which can be a large value)
is brought back to 8 bit by only retaining the remainder of the integer
division by 256. This 8-bit value is sent to the speakers.

Floatbeat is very similar to bytebeat. The difference is that the formula can
use floating point (decimal) computations, and the result is expected to be 
in the interval [-1, 1]. Again this value is reworked to a 8-bit value and
sent to the speakers.

Editor commands
---------------
^A        : Select all
^B        : Build the current buffer. If you are playing, the new compiled
            song will continue playing.
^C        : Copy to the clipboard (pbcopy on MacOs, xclip on Linux)            
^E        : Whenever you play, the song is recorded on disk to a wav file.
            With this command you can set the output file. If not set by 
            ^E, the default output file is session.wav in your forthbyte 
            binaries folder.
^N        : Make an empty buffer
^P        : Play / pause
^H        : Show tis help text
^K        : Stop playing
^O        : Open a file
^R        : Restart the timer `t`
^S        : Save the current file
^W        : Save the current file as 
^V        : Paste from the clipboard (pbpaste on MacOs, xclip on Linux)
^X        : Exit this application, or cancel the current operation
^Y        : Redo
^Z        : Undo


Glossary
--------

### Preprocessor directives

`#byte` use bytebeat
`#float` use floatbeat (this is the default)
`#samplerate nr` set the sample rate (default value is 8000)
`#initmemory a b c ... ` initializes the memory with the values given by
             `a`, `b`, `c`, ... . There are 256 memory spots available.

### Predefined variables

`t` the timer
`sr` the current samplerate that was set by the preprocessor directive
     `#samplerate`
`c` the current channel. It's possible to use stereo. The left channel
    has a value of `c` equal to 0, and the right channel has a value equal
    to 1.

### Forth words

The Forth stack has 256 entries. If you go over this amount, the counter
resets to zero, so you never can get a stack overflow. Similar for the
memory entries, or the Forth return stack.

`+` ( a b -- c )  Pops the two top values from the stack, and pushes their sum
                  on the stack.
`-` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  subtraction on the stack.
`*` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  multiplication on the stack.
`/` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  division on the stack.
`<<` ( a b -- c )  Pops the two top values from the stack, and pushes their
                   left shift on the stack.
`>>` ( a b -- c )  Pops the two top values from the stack, and pushes their
                   right shift on the stack.
`&` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  binary and on the stack.
`|` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  binary or on the stack.
`^` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  binary xor on the stack.
`%` ( a b -- c )  Pops the two top values from the stack, and pushes their
                  modulo on the stack.
`<` ( a b -- c )  Pops the two top values from the stack, compares them with
                  `<`, and puts 0 on the stack if the comparison fails, 1 
                  otherwise.
`>` ( a b -- c )  Pops the two top values from the stack, compares them with
                  `>`, and puts 0 on the stack if the comparison fails, 1 
                  otherwise.
`<=` ( a b -- c )  Pops the two top values from the stack, compares them with
                   `<=`, and puts 0 on the stack if the comparison fails, 1 
                   otherwise.
`>=` ( a b -- c )  Pops the two top values from the stack, compares them with 
                   `>=`, and puts 0 on the stack if the comparison fails, 1 
                   otherwise.
`=` ( a b -- c )  Pops the two top values from the stack, compares them with 
                  `=`, and puts 0 on the stack if the comparison fails, 1 
                  otherwise.
`<>` ( a b -- c )  Pops the two top values from the stack, compares them with 
                   `<>` (not equal), and puts 0 on the stack if the comparison 
                   fails, 1 otherwise.
`@` ( a -- b )  Pops the top value from the stack, gets the value at memory
                address `a`, and pushes this value on the stack.
`!` ( a b -- ) Pops the two top values from the stack, and stores value `a`
               at memory address `b`.
`>r` ( a -- ) Pops the top value from the stack, and moves it to the return
              stack. The return stack has 256 entries.
`r>` ( -- a ) Pops the top value from the return stack, and moves it to the
              regular stack. 
`: ;` Define a new word, e..g. `: twice 2 * ;` defines the word `twice`, so
      that `3 twice` equals `3 2 *` equals `6`.
`abs` ( a -- b ) Pops the top value from the stack, and pushes the absolue
                 value on the stack.
`atan2` ( a b -- c ) Pops the two top values from the stack, and pushes
                     atan2(a, b) on the stack.
`ceil` ( a -- b ) Pops the top value from the stack, rounds the value up,
                  and pushes this value on the stack.
`cos` ( a -- b ) Pops the top value from the stack, and pushes the cosine
                 on the stack.
`drop` ( a -- )  Pop the top element of the stack.
`dup` ( a -- a a )  Duplicate the value on the top of the stack.
`2dup` ( a b -- a b a b ) Duplicate the top two elements on the stack.
`exp` ( a -- b ) Pops the top value from the stack, and pushes the 
                 exponential on the stack.
`floor` ( a -- b ) Pops the top value from the stack, rounds the value down,
                   and pushes this value on the stack.
`log` ( a -- b ) Pops the top value from the stack, and pushes the logarithm
                 on the stack.
`max` ( a b -- c ) Pops the two top values from the stack, and pushes their
                   maximum on the stack.
`min` ( a b -- c ) Pops the two top values from the stack, and pushes their
                   minimum on the stack.
`negate` ( a -- b ) Pops the top value from the stack, and pushes the 
                    negative value on the stack.
`nip` ( a b -- b )  Drop the first item below the top of the stack.
`not` ( a -- b ) Pops the top value from the stack, and pushes the binary not
                 on the stack.
`over` ( a b -- a b a )  Duplicate the element under the top stack element.
`pick` ( a_b ... a_1 a_0 b -- a_b ... a_1 a_0 a_b )  Remove b from the stack
                                                     and copy a_b to the top
                                                     of the stack.
`pow` ( a b -- c ) Pops the two top values from the stack, and pushes `a` to
                   the power `b` on the stack.
`rot` ( a b c -- b c a ) Rotate the three top elements on the stack.
`-rot` ( a b c -- c a b ) Reverse rotate the three top elements on the stack.
`sin` ( a -- b ) Pops the top value from the stack, and pushes the sine on
                 the stack.
`sqrt` ( a -- b ) Pops the top value from the stack, and pushes the square
                  root on the stack.
`swap` ( a b -- b a ) Swaps the two top positions on the stack.
`tan` ( a -- b ) Pops the top value from the stack, and pushes the tangent
                 on the stack.
`tuck` ( a b -- b a b )  Copy the top stack item below the second stack item.
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
          case SDLK_KP_ENTER:
          case SDLK_RETURN: return ret(state, c, m);
          case SDLK_BACKSPACE: return backspace(state);
          case SDLK_DELETE: 
          {
          if (keyb.is_down(SDLK_LSHIFT) || keyb.is_down(SDLK_RSHIFT)) // copy
            {
            state = copy_to_snarf_buffer(state);
            }
          return del(state);
          }
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
          if (ctrl_pressed())
            {
            return select_all(state);
            }
          }
          case SDLK_b: // build
          {
          if (ctrl_pressed())
            {
            return compile_buffer(state, c, m);
            }
          }
          case SDLK_c:
          {
          if (ctrl_pressed())
            {
            return copy_to_snarf_buffer(state);
            }
          }
          case SDLK_e:
          {
          if (ctrl_pressed())
            {
            return export_location(state);
            }
          }
          case SDLK_h:
          {
          if (ctrl_pressed())
            {
            state.operation = op_help;
            return make_help_buffer(state);
            }
          }
          case SDLK_k:
          {
          if (ctrl_pressed())
            {
            state.message = string_to_line("[Stop]");
            state.playing = false;
            state.paused = false;
            m.stop();
            m.reset_timer();
            return state;
            }
          }
          case SDLK_p:
          {
          if (ctrl_pressed())
            {
            if (state.playing)
              {
              m.toggle_pause();
              if (m.is_paused())
                state.message = string_to_line("[Pause]");
              else
                state.message = string_to_line("[Play]");
              state.paused = m.is_paused();
              }
            else
              {
              m.play();
              state.message = string_to_line("[Play]");
              state.playing = true;
              state.paused = false;
              }
            return state;
            }
          }
          case SDLK_n:
          {
          if (ctrl_pressed())
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
          if (ctrl_pressed())
            {
            state.operation = op_open;
            return clear_operation_buffer(state);
            }
          }
          case SDLK_r:
          {
          if (ctrl_pressed())
            {
            state.message = string_to_line("[Restart]");
            m.reset_timer();
            return state;
            }
          }
          case SDLK_s:
          {
          if (ctrl_pressed())
            {
            if (state.buffer.name.empty())
              {
              state.operation = op_save;
              return make_save_buffer(state);
              }
            else
              {
              return put(state);
              }
            }
          }
          case SDLK_v:
          {
          if (ctrl_pressed())
            {
            return paste_from_snarf_buffer(state);
            }
          }
          case SDLK_w:
          {
          if (ctrl_pressed())
            {
            state.operation = op_save;
            return make_save_buffer(state);
            }
          }
          case SDLK_x:
          {
          if (ctrl_pressed())
            {
            return cancel(state);
            }
          break;
          }
          case SDLK_y:
          {
          if (ctrl_pressed())
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
          if (ctrl_pressed())
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
  state.export_location = jtk::get_folder(jtk::get_executable_path()) + std::string("session.wav");
  m.set_session_filename(state.export_location);
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);  
  state.operation = op_editing;
  state.scroll_row = 0;
  state.operation_scroll_row = 0;
  state.playing = false;
  state.paused = false;
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
