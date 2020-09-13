#pragma once

#include "buffer.h"

struct app_state
  {
  file_buffer buffer;
  position scroll_pos;
  int64_t word_wrap_row;
  };


struct engine
  {
  app_state state;

  engine(int w, int h, int argc, char** argv);
  ~engine();

  void run();

  };

