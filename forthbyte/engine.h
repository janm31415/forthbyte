#pragma once

#include "buffer.h"
#include "compiler.h"
#include "music.h"
#include <string>
#include <vector>

enum e_operation
  {
  op_editing,
  op_exit,
  op_export,
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
  env_settings senv;
  bool playing, paused;
  std::string export_location;
  };


struct engine
  {
  app_state state;
  compiler c;
  music m;

  engine(int argc, char** argv);
  ~engine();

  void run();

  };

