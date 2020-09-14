#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

#include "engine.h"

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#ifdef _WIN32
#include <windows.h>
#endif



int main(int argc, char** argv)
  {

  /* Initialize SDL */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
    std::cout << "Could not initialize SDL" << std::endl;
    exit(1);
    }
  SDL_GL_SetSwapInterval(1);
  atexit(SDL_Quit);

  int w = 640 * 2;
  int h = 480 * 2;

  /* Initialize PDCurses */

  {

  initscr();
  }

  start_color();
  scrollok(stdscr, TRUE);

  PDC_set_title("forthbyte beat machine");

  engine e(w, h, argc, argv);
  e.run();


  endwin();

  return 0;

  }