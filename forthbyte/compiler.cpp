#include "compiler.h"
#include <sstream>

compiler::compiler()
  {

  }

compiler::~compiler()
  {

  }

void compiler::compile_byte(const std::string& script, const preprocess_settings& sett)
  {
  using namespace forth;
  auto words = tokenize(script);
  interpr_int = forth::interpreter<int64_t>();
  interpr_int.make_variable("t");
  interpr_int.make_variable("sr");
  interpr_int.make_variable("c");
  interpr_int.set_variable_value("sr", sett._sample_rate);
  prog_int = interpr_int.parse(words);
  stereo_int = _program_byte_is_stereo();
  }

void compiler::compile_float(const std::string& script, const preprocess_settings& sett)
  {
  using namespace forth;
  auto words = tokenize(script);
  interpr_double = forth::interpreter<double>();
  interpr_double.make_variable("t");
  interpr_double.make_variable("sr");
  interpr_double.make_variable("c");
  interpr_double.set_variable_value("sr", sett._sample_rate);
  prog_double = interpr_double.parse(words);
  stereo_double = _program_float_is_stereo();
  }

void compiler::init_memory_byte(const std::vector<std::string>& mem)
  {
  int index = 0;
  for (const auto& val : mem)
    {
    std::stringstream str;
    str << val;
    int64_t v;
    str >> v;
    interpr_int.memory_stack[index] = v;
    ++index;
    if (index >= interpr_int.memory_stack.size())
      index = 0;
    }
  }

void compiler::init_memory_float(const std::vector<std::string>& mem)
  {
  int index = 0;
  for (const auto& val : mem)
    {
    std::stringstream str;
    str << val;
    double v;
    str >> v;
    interpr_double.memory_stack[index] = v;
    ++index;
    if (index >= interpr_double.memory_stack.size())
      index = 0;
    }
  }

unsigned char compiler::run_byte(int64_t t, int c)
  {
  interpr_int.globals[0] = t;
  interpr_int.globals[2] = c;
  interpr_int.eval(prog_int);
  int64_t val = interpr_int.pop();
  return (unsigned char)(val & 255);
  }

double compiler::run_float(int64_t t, int c)
  {
  interpr_double.globals[0] = (double)t;
  interpr_double.globals[2] = (double)c;
  interpr_double.eval(prog_double);
  double val = interpr_double.pop();
  return val;
  }

bool compiler::_program_byte_is_stereo()
  {
  auto it = interpr_int.variables.find(std::string("c"));
  assert(it != interpr_int.variables.end());
  for (const auto& st : prog_int.statements)
    {
    if (std::holds_alternative<forth::interpreter<int64_t, 256>::Variable>(st))
      {
      if (std::get<forth::interpreter<int64_t, 256>::Variable>(st).index == it->second)
        return true;
      }
    }
  return false;
  }

bool compiler::_program_float_is_stereo()
  {
  auto it = interpr_double.variables.find(std::string("c"));
  assert(it != interpr_double.variables.end());
  for (const auto& st : prog_double.statements)
    {
    if (std::holds_alternative<forth::interpreter<double, 256>::Variable>(st))
      {
      if (std::get<forth::interpreter<double, 256>::Variable>(st).index == it->second)
        return true;
      }
    }
  return false;
  }