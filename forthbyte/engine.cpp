#include "engine.h"
#include "buffer.h"
#include "clipboard.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#include "keyboard.h"

#include <jtk/file_utils.h>

#define TAB_SPACE 8

void get_editor_window_size(int& rows, int& cols)
  {
  getmaxyx(stdscr, rows, cols);
  rows -= 2;
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
    case 9:
    {
    return 32; break;
    }
    case 10: {
    return 32; break;
    }
    case 13: {
    return 32; break;
    }
    default: (uint16_t)character;
    }
  }

position get_actual_position(app_state state)
  {
  return get_actual_position(state.buffer);
  }

app_state draw(app_state state)
  {
  erase();

  int maxrow, maxcol;
  get_editor_window_size(maxrow, maxcol);  

  position current;
  current.row = state.scroll_row;

  position cursor = get_actual_position(state);

  bool has_selection = state.buffer.start_selection != std::nullopt;

  std::vector<chtype> attribute_stack;

  attrset(A_NORMAL);
  attribute_stack.push_back(A_NORMAL);

  if (has_selection && state.buffer.start_selection->row < state.scroll_row)
    {
    attron(A_REVERSE);
    attribute_stack.push_back(A_REVERSE);
    }
 
  //if (cursor.col > state.buffer.content[cursor.row].size())
  //  cursor.col = state.buffer.content[cursor.row].size();

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

    auto it = state.buffer.content[current.row].begin();
    auto it_end = state.buffer.content[current.row].end();

    for (; it != it_end; ++it)
      {
      if (current.col >= maxcol)
        break;

      if (current == cursor)
        {    
        attribute_stack.push_back(A_REVERSE);
        if (has_selection && (cursor < state.buffer.start_selection))
          {
          attribute_stack.push_back(A_REVERSE);
          }
        attron(A_REVERSE);
        }
      if (has_selection && (current == state.buffer.start_selection) && (state.buffer.start_selection < cursor))
        {
        attribute_stack.push_back(A_REVERSE);
        attron(A_REVERSE);
        }

      move((int)r, (int)current.col);
      auto character = *it;
      uint32_t cwidth = character_width(character, current.col);
      for (uint32_t cnt = 0; cnt < cwidth; ++cnt)
        {
        addch(character_to_pdc_char(character, cnt));
        }

      if (current == cursor)
        {
        attribute_stack.pop_back();
        if (has_selection && (state.buffer.start_selection < cursor))
          attribute_stack.pop_back();
        attrset(attribute_stack.back());
        }
      if (has_selection && (current == state.buffer.start_selection) && (cursor < state.buffer.start_selection))
        {
        attribute_stack.pop_back();
        attrset(attribute_stack.back());
        }

      ++current.col;
      }

    if ((current == cursor))// && (current.row == state.buffer.content.size() - 1) && (current.col == state.buffer.content.back().size()))// only occurs on last line, last position
      {
      move((int)r, (int)current.col);
      assert(current.row == state.buffer.content.size() - 1);
      assert(current.col == state.buffer.content.back().size());
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }

    ++current.row;
    }



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

app_state cancel_selection(app_state state)
  {
  if (!keyb_data.selecting)
    state.buffer = clear_selection(state.buffer);
  return state;
  }

app_state move_left(app_state state)
  {
  state = cancel_selection(state);
  if (state.buffer.content.empty())
    return state;
  position actual = get_actual_position(state);
  if (actual.col == 0)
    {
    if (state.buffer.pos.row > 0)
      {
      --state.buffer.pos.row;
      state.buffer.pos.col = state.buffer.content[state.buffer.pos.row].size();
      }
    }
  else
    state.buffer.pos.col = actual.col-1;
  return check_scroll_position(state);
  }

app_state move_right(app_state state)
  {
  state = cancel_selection(state);
  if (state.buffer.content.empty())
    return state;
  if (state.buffer.pos.col < (int64_t)state.buffer.content[state.buffer.pos.row].size()-1)
    ++state.buffer.pos.col;
  else if ((state.buffer.pos.row+1) < state.buffer.content.size())
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

app_state move_up(app_state state)
  {
  state = cancel_selection(state);
  if (state.buffer.pos.row > 0)
    --state.buffer.pos.row;
  return check_scroll_position(state);
  }

app_state move_down(app_state state)
  {
  state = cancel_selection(state);
  if ((state.buffer.pos.row + 1) < state.buffer.content.size())
    {
    ++state.buffer.pos.row;
    }
  return check_scroll_position(state);
  }

app_state move_page_up(app_state state)
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

app_state move_page_down(app_state state)
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

app_state move_home(app_state state)
  {
  state = cancel_selection(state);
  state.buffer.pos.col = 0;
  return state;
  }

app_state move_end(app_state state)
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

app_state text_input(app_state state, const char* txt)
  {
  std::string t(txt);
  state.buffer = insert(state.buffer, t);
  return check_scroll_position(state);
  }

app_state backspace(app_state state)
  {
  state.buffer = erase(state.buffer);
  return check_scroll_position(state);
  }

app_state del(app_state state)
  {
  state.buffer = erase_right(state.buffer);
  return check_scroll_position(state);
  }

std::optional<app_state> exit(app_state state)
  {
  return std::nullopt;
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
  if (state.buffer.undo_redo_index == state.buffer.history.size()) // first time undo
    {
    state.buffer = push_undo(state.buffer);
    --state.buffer.undo_redo_index;
    }
  if (state.buffer.undo_redo_index)
    {
    --state.buffer.undo_redo_index;
    snapshot ss = state.buffer.history[(uint32_t)state.buffer.undo_redo_index];
    state.buffer.content = ss.content;
    state.buffer.pos = ss.pos;
    state.buffer.modification_mask = ss.modification_mask;
    state.buffer.start_selection = ss.start_selection;
    state.buffer.history = state.buffer.history.push_back(ss);
    }
  return state;
  }

app_state redo(app_state state)
  {
  if (state.buffer.undo_redo_index + 1 < state.buffer.history.size())
    {
    ++state.buffer.undo_redo_index;
    snapshot ss = state.buffer.history[(uint32_t)state.buffer.undo_redo_index];
    state.buffer.content = ss.content;
    state.buffer.pos = ss.pos;
    state.buffer.modification_mask = ss.modification_mask;
    state.buffer.start_selection = ss.start_selection;
    state.buffer.history = state.buffer.history.push_back(ss);
    }
  return state;
  }

app_state copy_to_snarf_buffer(app_state state)
  {
  state.snarf_buffer = get_selection(state.buffer);
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
  state.buffer = insert(state.buffer, state.snarf_buffer);
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
          resize_term(new_w, new_h);
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
          case SDLK_RETURN: return text_input(state, "\n");
          case SDLK_BACKSPACE: return backspace(state);
          case SDLK_DELETE: return del(state);
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            break;
          keyb_data.selecting = true;
          if (state.buffer.start_selection == std::nullopt)
            state.buffer.start_selection = get_actual_position(state);
          return state;
          }
          case SDLK_c:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL)) // undo
            {
            return copy_to_snarf_buffer(state);
            }
          }
          case SDLK_v:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL)) // undo
            {
            return paste_from_snarf_buffer(state);
            }
          }
          case SDLK_y:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL)) // undo
            {
            return redo(state);
            }
          break;
          }
          case SDLK_z: 
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL)) // undo
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
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(16.0));
    }
  }

engine::engine(int w, int h, int argc, char** argv)
  {
  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();

  if (argc > 1)
    state.buffer = read_from_file(std::string(argv[1]));
  else
    state.buffer = make_empty_buffer();
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
