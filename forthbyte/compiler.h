#pragma once

#include "forth.h"

#include <string>
#include <stdint.h>

class compiler
  {
  public:
    compiler();
    ~compiler();

    void compile_byte(const std::string& script);
    void compile_float(const std::string& script);

    unsigned char run_byte(int64_t t);
    double run_float(int64_t t);

  private:
    forth::interpreter<int64_t> interpr_int;
    forth::interpreter<double> interpr_double;

    forth::interpreter<int64_t>::Program prog_int;
    forth::interpreter<double>::Program prog_double;
  };