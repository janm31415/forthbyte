#pragma once

#include "buffer.h"
#include <string>
#include <vector>

enum e_operation
  {
  op_editing,
  op_exit,
  op_help,
  op_open,
  op_save,
  op_query_save,
  op_new
  };

struct app_state
  {
  file_buffer buffer;
  file_buffer operation_buffer;
  text snarf_buffer;
  line message;
  int64_t scroll_row, operation_scroll_row;    
  e_operation operation;  
  std::vector<e_operation> operation_stack;
  };


struct engine
  {
  app_state state;

  engine(int argc, char** argv);
  ~engine();

  void run();

  };

