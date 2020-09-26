#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

#include "engine.h"
#include "fbicon.h"

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


  /* Initialize PDCurses */

  {
  uint32_t rmask = 0x000000ff;
  uint32_t gmask = 0x0000ff00;
  uint32_t bmask = 0x00ff0000;

  uint32_t amask = (fbicon.bytes_per_pixel == 3) ? 0 : 0xff000000;
  pdc_icon = SDL_CreateRGBSurfaceFrom((void*)fbicon.pixel_data, fbicon.width,
    fbicon.height, fbicon.bytes_per_pixel * 8, fbicon.bytes_per_pixel*fbicon.width,
    rmask, gmask, bmask, amask);

  initscr();
  }

  start_color();
  scrollok(stdscr, TRUE);

  PDC_set_title("forthbyte beat machine");

  engine e(argc, argv);
  e.run();


  endwin();

  return 0;

  }