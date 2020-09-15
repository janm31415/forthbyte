#pragma once

#include "buffer.h"
#include <string>

enum e_operation
  {
  op_editing,
  op_help,
  op_open,
  op_save
  };

struct app_state
  {
  file_buffer buffer;
  file_buffer operation_buffer;
  text snarf_buffer;
  line message;
  int64_t scroll_row, operation_scroll_row;    
  e_operation operation;  
  };


struct engine
  {
  app_state state;

  engine(int w, int h, int argc, char** argv);
  ~engine();

  void run();

  };

