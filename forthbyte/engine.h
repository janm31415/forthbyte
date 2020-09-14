#pragma once

#include "buffer.h"

enum e_operation
  {
  op_editing,
  op_open,
  op_saveas
  };

struct app_state
  {
  file_buffer buffer;
  file_buffer operation_buffer;
  int64_t scroll_row;  
  text snarf_buffer;
  e_operation operation;
  };


struct engine
  {
  app_state state;

  engine(int w, int h, int argc, char** argv);
  ~engine();

  void run();

  };

