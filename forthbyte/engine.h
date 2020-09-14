#pragma once

#include "buffer.h"

struct app_state
  {
  file_buffer buffer;
  int64_t scroll_row;  
  text snarf_buffer;
  };


struct engine
  {
  app_state state;

  engine(int w, int h, int argc, char** argv);
  ~engine();

  void run();

  };

