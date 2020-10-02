#pragma once

#include "forth.h"
#include "preprocessor.h"

#include <string>
#include <stdint.h>

class compiler
  {
  public:
    compiler();
    ~compiler();

    void compile_byte(const std::string& script, const preprocess_settings& sett);
    void compile_float(const std::string& script, const preprocess_settings& sett);
    void init_memory_byte(const std::vector<std::string>& mem);
    void init_memory_float(const std::vector<std::string>& mem);

    bool stereo_byte() const { return stereo_int; }
    bool stereo_float() const { return stereo_double; }

    unsigned char run_byte(int64_t t, int c);
    double run_float(int64_t t, int c);

  private:
    bool _program_byte_is_stereo();
    bool _program_float_is_stereo();

  private:
    forth::interpreter<int64_t, 256> interpr_int;
    forth::interpreter<double, 256> interpr_double;

    forth::interpreter<int64_t, 256>::Program prog_int;
    forth::interpreter<double, 256>::Program prog_double;

    bool stereo_int;
    bool stereo_double;
  };
