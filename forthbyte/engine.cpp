#include "engine.h"
#include "clipboard.h"

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

namespace
  {
  int font_width, font_height;
  }

void get_editor_window_size(int& rows, int& cols)
  {
  getmaxyx(stdscr, rows, cols);
  rows -= 4;
  }

uint32_t character_width(uint32_t character, int64_t col)
  {
  switch (character)
    {
    case 9: return TAB_SPACE - (col % TAB_SPACE);
    default: return 1;
    }
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


void draw_title_bar(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  attrset(A_NORMAL);
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


void draw_line(line ln, position& current, position cursor, std::vector<chtype>& attribute_stack, int r, int xoffset, int maxcol, std::optional<position> start_selection)
  {
  bool has_selection = start_selection != std::nullopt;

  auto it = ln.begin();
  auto it_end = ln.end();

  for (; it != it_end; ++it)
    {
    if (current.col >= maxcol)
      break;

    if (current == cursor)
      {
      attribute_stack.push_back(A_REVERSE);
      if (has_selection && (cursor < start_selection))
        {
        attribute_stack.push_back(A_REVERSE);
        }
      attron(A_REVERSE);
      }
    if (has_selection && (current == start_selection) && (start_selection < cursor))
      {
      attribute_stack.push_back(A_REVERSE);
      attron(A_REVERSE);
      }

    move((int)r, (int)current.col + xoffset);
    auto character = *it;
    uint32_t cwidth = character_width(character, current.col);
    for (uint32_t cnt = 0; cnt < cwidth; ++cnt)
      {
      addch(character_to_pdc_char(character, cnt));
      }

    if (current == cursor)
      {
      attroff(attribute_stack.back());
      attribute_stack.pop_back();
      if (has_selection && (start_selection < cursor))
        {
        attroff(attribute_stack.back());
        attribute_stack.pop_back();
        }
      attron(attribute_stack.back());
      }
    if (has_selection && (current == start_selection) && (cursor < start_selection))
      {
      attroff(attribute_stack.back());
      attribute_stack.pop_back();
      attron(attribute_stack.back());
      }

    ++current.col;
    }
  }

std::string get_operation_text(e_operation op)
  {
  switch (op)
    {
    case op_open: return std::string("Open file: ");
    default: return std::string();
    }
  }

void draw_help_line(const std::string& text, int r, int sz)
  {
  attrset(A_NORMAL);
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
    draw_help_line(line1, rows - 2, cols);
    draw_help_line(line2, rows - 1, cols);
    }
  if (state.operation == op_open)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  }

app_state draw(app_state state)
  {
  erase();

  draw_title_bar(state);

  int offset_x = 0;
  int offset_y = 1;

  int maxrow, maxcol;
  get_editor_window_size(maxrow, maxcol);

  position current;
  current.row = state.scroll_row;

  position cursor = get_actual_position(state.buffer);

  bool has_selection = state.buffer.start_selection != std::nullopt;

  std::vector<chtype> attribute_stack;

  attrset(A_NORMAL);
  attribute_stack.push_back(A_NORMAL);

  if (has_selection && state.buffer.start_selection->row < state.scroll_row)
    {
    attron(A_REVERSE);
    attribute_stack.push_back(A_REVERSE);
    }

  for (int r = 0; r < maxrow; ++r)
    {
    current.col = 0;
    if (current.row >= state.buffer.content.size())
      {
      if (state.buffer.content.empty()) // file is empty, draw cursor
        {
        attron(A_REVERSE);
        addch(' ');
        attroff(A_REVERSE);
        }
      break;
      }
    

    draw_line(state.buffer.content[current.row], current, cursor, attribute_stack, r + offset_y, offset_x, maxcol, state.buffer.start_selection);

    if ((current == cursor))// && (current.row == state.buffer.content.size() - 1) && (current.col == state.buffer.content.back().size()))// only occurs on last line, last position
      {
      move((int)r + offset_y, (int)current.col + offset_x);
      assert(current.row == state.buffer.content.size() - 1);
      assert(current.col == state.buffer.content.back().size());
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }

    ++current.row;
    }

  if (state.operation != op_editing)
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    cursor = get_actual_position(state.operation_buffer);
    current.col = 0;
    current.row = 0;
    std::string txt = get_operation_text(state.operation);
    move((int)rows - 3, 0);
    attrset(A_BOLD);
    attribute_stack.push_back(A_BOLD);
    for (auto ch : txt)
      addch(ch);

    int cols_available = maxcol - txt.length();
    int off_x = offset_x + txt.length();
    if (!state.operation_buffer.content.empty())
      draw_line(state.operation_buffer.content[0], current, cursor, attribute_stack, rows-3, off_x, cols_available, state.operation_buffer.start_selection);
    if ((current == cursor))
      {
      move((int)rows - 3, (int)current.col + off_x);
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }
    attribute_stack.pop_back();
    attrset(attribute_stack.back());
    }
  else
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int message_length = (int)state.message.size();
    int offset = (cols - message_length) / 2;
    if (offset > 0)
      {
      attrset(A_BOLD);
      for (auto ch : state.message)
        {
        move(rows - 3, offset);
        addch(ch);
        ++offset;
        }
      }
    }

  draw_help_text(state);

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
  if (state.buffer.content.empty())
    return state;
  position actual = get_actual_position(state.buffer);
  if (actual.col == 0)
    {
    if (state.buffer.pos.row > 0)
      {
      --state.buffer.pos.row;
      state.buffer.pos.col = state.buffer.content[state.buffer.pos.row].size();
      }
    }
  else
    state.buffer.pos.col = actual.col - 1;
  return check_scroll_position(state);
  }

app_state move_left_operation(app_state state)
  {
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
  if (state.buffer.content.empty())
    return state;
  if (state.buffer.pos.col < (int64_t)state.buffer.content[state.buffer.pos.row].size() - 1)
    ++state.buffer.pos.col;
  else if ((state.buffer.pos.row + 1) < state.buffer.content.size())
    {
    state.buffer.pos.col = 0;
    ++state.buffer.pos.row;
    }
  else if (state.buffer.pos.col == (int64_t)state.buffer.content[state.buffer.pos.row].size() - 1)
    {
    ++state.buffer.pos.col;
    assert(state.buffer.pos.row == state.buffer.content.size() - 1);
    assert(state.buffer.pos.col == state.buffer.content.back().size());
    }
  return check_scroll_position(state);
  }

app_state move_right_operation(app_state state)
  {
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
  if (state.buffer.pos.row > 0)
    --state.buffer.pos.row;
  return check_scroll_position(state);
  }

app_state move_up(app_state state)
  {
  if (state.operation == op_editing)
    return move_up_editor(state);
  return state;
  }

app_state move_down_editor(app_state state)
  {
  state = cancel_selection(state);
  if ((state.buffer.pos.row + 1) < state.buffer.content.size())
    {
    ++state.buffer.pos.row;
    }
  return check_scroll_position(state);
  }

app_state move_down(app_state state)
  {
  if (state.operation == op_editing)
    return move_down_editor(state);
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

  state.buffer.pos.row -= rows - 1;
  if (state.buffer.pos.row < 0)
    state.buffer.pos.row = 0;

  return check_scroll_position(state);
  }

app_state move_page_up(app_state state)
  {
  if (state.operation == op_editing)
    return move_page_up_editor(state);
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
  state.buffer.pos.row += rows - 1;
  if (state.buffer.pos.row >= state.buffer.content.size())
    state.buffer.pos.row = (int64_t)state.buffer.content.size() - 1;
  if (state.buffer.pos.row < 0)
    state.buffer.pos.row = 0;
  return check_scroll_position(state);
  }

app_state move_page_down(app_state state)
  {
  if (state.operation == op_editing)
    return move_page_down_editor(state);
  return state;
  }

app_state move_home_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer.pos.col = 0;
  return state;
  }

app_state move_home_operation(app_state state)
  {
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
  if (state.buffer.content.empty())
    return state;

  state.buffer.pos.col = (int64_t)state.buffer.content[state.buffer.pos.row].size() - 1;
  if (state.buffer.pos.col < 0)
    state.buffer.pos.col = 0;

  if ((state.buffer.pos.row + 1) == state.buffer.content.size()) // last line
    {
    if (state.buffer.content.back().back() != L'\n')
      ++state.buffer.pos.col;
    }
  return state;
  }

app_state move_end_operation(app_state state)
  {
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

app_state text_input_editor(app_state state, const char* txt)
  {
  std::string t(txt);
  state.buffer = insert(state.buffer, t);
  return check_scroll_position(state);
  }

app_state text_input_operation(app_state state, const char* txt)
  {
  std::string t(txt);
  state.operation_buffer = insert(state.operation_buffer, t);
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
  state.buffer = erase(state.buffer);
  return check_scroll_position(state);
  }

app_state backspace_operation(app_state state)
  {
  state.operation_buffer = erase(state.operation_buffer);
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
  state.buffer = erase_right(state.buffer);
  return check_scroll_position(state);
  }

app_state del_operation(app_state state)
  {
  state.operation_buffer = erase_right(state.operation_buffer);
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

app_state open_file(app_state state)
  {
  state.operation = op_editing;
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' by '/'
  std::string filename = jtk::convert_wstring_to_string(wfilename);
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
  return state;
  }

app_state ret_operation(app_state state)
  {
  switch (state.operation)
    {
    case op_open: return open_file(state);
    default: break;
    }
  state.operation = op_editing;
  return state;
  }

app_state ret(app_state state)
  {
  if (state.operation == op_editing)
    return ret_editor(state);
  return ret_operation(state);
  }

std::optional<app_state> exit(app_state state)
  {
  return std::nullopt;
  }

std::optional<app_state> cancel(app_state state)
  {
  if (state.operation == op_editing)
    return exit(state);
  else
    {
    state.message = string_to_line("[Cancelled]");
    state.operation = op_editing;
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
  if (state.operation == op_editing)
    state.buffer = undo(state.buffer);
  else
    state.operation_buffer = undo(state.operation_buffer);
  return check_scroll_position(state);
  }

app_state redo(app_state state)
  {
  if (state.operation == op_editing)
    state.buffer = redo(state.buffer);
  else
    state.operation_buffer = redo(state.operation_buffer);
  return check_scroll_position(state);
  }

app_state copy_to_snarf_buffer(app_state state)
  {
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(state.buffer);
  else
    state.snarf_buffer = get_selection(state.operation_buffer);
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
  if (state.operation == op_editing)
    state.buffer = insert(state.buffer, state.snarf_buffer);
  else
    state.operation_buffer = insert(state.operation_buffer, state.snarf_buffer);
  return check_scroll_position(state);
  }

app_state select_all(app_state state)
  {
  if (state.operation == op_editing)
    state.buffer = select_all(state.buffer);
  else
    state.operation_buffer = select_all(state.operation_buffer);
  return check_scroll_position(state);
  }

app_state clear_operation_buffer(app_state state)
  {
  state.operation_buffer.content = text();
  state.operation_buffer.history = immutable::vector<snapshot, false>();
  state.operation_buffer.undo_redo_index = 0;
  state.operation_buffer.start_selection = std::nullopt;
  state.operation_buffer.pos.row = 0;
  state.operation_buffer.pos.col = 0;
  return state;
  }

std::optional<app_state> process_input(app_state state)
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
          case SDLK_RETURN: return ret(state);
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
          case SDLK_c:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return copy_to_snarf_buffer(state);
            }
          }
          case SDLK_o:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL)) 
            {
            state.operation = op_open;
            return clear_operation_buffer(state);
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
            return redo(state);
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
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    }
  }

engine::engine(int w, int h, int argc, char** argv)
  {
  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();

  if (argc > 1)
    state.buffer = read_from_file(std::string(argv[1]));
  else
    state.buffer = make_empty_buffer();
  state.operation = op_editing;
  state.scroll_row = 0;
  }

engine::~engine()
  {

  }

void engine::run()
  {
  state = draw(state);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state))
    {
    state = *new_state;
    state = draw(state);

    SDL_UpdateWindowSurface(pdc_window);
    }
  }
