#include "compiler.h"

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
  interpr_int.set_variable_value("sr", sett._sample_rate);
  prog_int = interpr_int.parse(words);
  }

void compiler::compile_float(const std::string& script, const preprocess_settings& sett)
  {
  using namespace forth;
  auto words = tokenize(script);
  interpr_double = forth::interpreter<double>();
  interpr_double.make_variable("t");
  interpr_double.make_variable("sr");
  interpr_double.set_variable_value("sr", sett._sample_rate);
  prog_double = interpr_double.parse(words);
  }

unsigned char compiler::run_byte(int64_t t)
  {
  interpr_int.globals[0] = t;
  interpr_int.eval(prog_int);
  int64_t val = interpr_int.pop();
  return (unsigned char)(val & 255);
  }

double compiler::run_float(int64_t t)
  {
  interpr_double.globals[0] = (double)t;
  interpr_double.eval(prog_double);
  double val = interpr_double.pop();
  return val;
  }
