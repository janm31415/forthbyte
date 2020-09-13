#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

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
  uint32_t rmask = 0x000000ff;
  uint32_t gmask = 0x0000ff00;
  uint32_t bmask = 0x00ff0000;

  initscr();
  }

  start_color();
  scrollok(stdscr, TRUE);

  PDC_set_title("forthbyte");

  //engine e(w, h, argc, argv, read_settings(get_file_in_executable_path("jam_settings.json").c_str()));
  //e.run();


  endwin();

  return 0;

  }