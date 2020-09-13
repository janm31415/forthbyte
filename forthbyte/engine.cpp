#include "engine.h"
#include "buffer.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#include "keyboard.h"

#define TAB_SPACE 8

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

app_state draw(app_state state)
  {
  erase();

  int maxrow, maxcol;
  getmaxyx(stdscr, maxrow, maxcol);

  position current = state.scroll_pos;

  position cursor = state.buffer.pos;
  if (cursor.col > state.buffer.content[cursor.row].size())
    cursor.col = state.buffer.content[cursor.row].size();

  for (int r = 0; r < maxrow - 2; ++r)
    {
    current.col = 0;
    if (current.row >= state.buffer.content.size())
      break;

    auto it = state.buffer.content[current.row].begin();
    auto it_end = state.buffer.content[current.row].end();

    for (; it != it_end; ++it)
      {
      if (current.col >= maxcol)
        break;

      if (current == cursor)
        {
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
        attroff(A_REVERSE);
        }
      ++current.col;
      }
    ++current.row;
    }

  curs_set(0);
  refresh();

  return state;
  }

app_state move_left(app_state state)
  {
  if (state.buffer.pos.col == 0)
    {
    if (state.buffer.pos.row > 0)
      {
      --state.buffer.pos.row;
      state.buffer.pos.col = state.buffer.content[state.buffer.pos.row].size();
      }
    }
  else
    --state.buffer.pos.col;
  return state;
  }

app_state move_right(app_state state)
  {
  if (state.buffer.pos.col < state.buffer.content[state.buffer.pos.row].size())
    ++state.buffer.pos.col;
  else if ((state.buffer.pos.row+1) < state.buffer.content.size())
    {
    state.buffer.pos.col = 0;
    ++state.buffer.pos.row;
    }
  return state;
  }

app_state move_up(app_state state)
  {
  if (state.buffer.pos.row > 0)
    --state.buffer.pos.row;
  return state;
  }

app_state move_down(app_state state)
  {
  if ((state.buffer.pos.row + 1) < state.buffer.content.size())
    {
    ++state.buffer.pos.row;
    }
  return state;
  }

std::optional<app_state> exit(app_state state)
  {
  return std::nullopt;
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

        case SDL_KEYDOWN:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LEFT: return move_left(state);
          case SDLK_RIGHT: return move_right(state);
          case SDLK_DOWN: return move_down(state);
          case SDLK_UP: return move_up(state);

          } // switch (event.key.keysym.sym)
        break;
        } // case SDL_KEYDOWN:
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
  state.scroll_pos.row = state.scroll_pos.col = 0;
  state.word_wrap_row = 0;
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
