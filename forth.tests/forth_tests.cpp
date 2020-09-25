#include "forth_tests.h"
#include "test_assert.h"

#include <forthbyte/forth.h>

#include <iostream>

using namespace forth;

namespace
  {
  bool equal(const token& t, const std::string& value, const token::e_type& type)
    {
    if (t.type != type)
      return false;
    if (t.value != value)
      return false;
    return true;
    }
  }

void test_tokenize()
  {
  auto words = tokenize("double 1 3.4 : ; ( bla bla \n\t    bla\n) tureluut 1.e-8 4509.3498.234234 \\ this is comment");

  TEST_EQ(8, (int)words.size());

  TEST_ASSERT(equal(words[0], "double", token::T_WORD));
  TEST_ASSERT(equal(words[1], "1", token::T_VALUE));
  TEST_ASSERT(equal(words[2], "3.4", token::T_VALUE));
  TEST_ASSERT(equal(words[3], ":", token::T_COLON));
  TEST_ASSERT(equal(words[4], ";", token::T_SEMICOLON));
  TEST_ASSERT(equal(words[5], "tureluut", token::T_WORD));
  TEST_ASSERT(equal(words[6], "1.e-8", token::T_VALUE));
  TEST_ASSERT(equal(words[7], "4509.3498.234234", token::T_WORD));
  }

void test_parse_value()
  {
  auto words = tokenize("1 2");
  TEST_EQ(2, (int)words.size());
  interpreter<int> interpr;
  auto prog = interpr.parse(words);
  TEST_EQ(2, (int)prog.statements.size());
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[0]));
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[1]));
  TEST_EQ(1, std::get<interpreter<int>::Value>(prog.statements[0]).val);
  TEST_EQ(2, std::get<interpreter<int>::Value>(prog.statements[1]).val);
  }

void test_parse_value_floating()
  {
  auto words = tokenize("1.2 2");
  TEST_EQ(2, (int)words.size());
  interpreter<float> interpr;
  auto prog = interpr.parse(words);
  TEST_EQ(2, (int)prog.statements.size());
  TEST_ASSERT(std::holds_alternative<interpreter<float>::Value>(prog.statements[0]));
  TEST_ASSERT(std::holds_alternative<interpreter<float>::Value>(prog.statements[1]));
  TEST_EQ(1.2f, std::get<interpreter<float>::Value>(prog.statements[0]).val);
  TEST_EQ(2.f, std::get<interpreter<float>::Value>(prog.statements[1]).val);
  }

void test_parse_add()
  {
  auto words = tokenize("1 2 +");
  TEST_EQ(3, (int)words.size());
  interpreter<int> interpr;
  auto prog = interpr.parse(words);
  TEST_EQ(3, (int)prog.statements.size());
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[0]));
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[1]));
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Primitive>(prog.statements[2]));
  TEST_EQ(1, std::get<interpreter<int>::Value>(prog.statements[0]).val);
  TEST_EQ(2, std::get<interpreter<int>::Value>(prog.statements[1]).val);
  }

void test_parse_definition()
  {
  auto words = tokenize(": three 1 2 + ; three");
  TEST_EQ(7, (int)words.size());
  interpreter<int> interpr;
  auto prog = interpr.parse(words);
  TEST_EQ(3, (int)prog.statements.size());
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[0]));
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Value>(prog.statements[1]));
  TEST_ASSERT(std::holds_alternative<interpreter<int>::Primitive>(prog.statements[2]));
  TEST_EQ(1, std::get<interpreter<int>::Value>(prog.statements[0]).val);
  TEST_EQ(2, std::get<interpreter<int>::Value>(prog.statements[1]).val);
  }

void test_parse_variable()
  {
  auto words = tokenize("t"); 
  TEST_EQ(1, (int)words.size());
  interpreter<float> interpr;
  interpr.make_variable("t");
  auto prog = interpr.parse(words);
  TEST_EQ(1, (int)prog.statements.size());
  TEST_ASSERT(std::holds_alternative<interpreter<float>::Variable>(prog.statements[0]));
  }

void test_eval_add()
  {
  auto words = tokenize("1 2 +");
  interpreter<int> interpr;
  auto prog = interpr.parse(words);
  for (int i = 0; i < 3; ++i)
    {
    interpr.eval(prog);
    int res = interpr.pop();
    TEST_EQ(3, res);
    TEST_EQ(0, interpr.stack_pointer);
    }
  }

void run_all_forth_tests()
  {
  test_tokenize();
  test_parse_value();
  test_parse_value_floating();
  test_parse_add();
  test_parse_definition();
  test_parse_variable();
  test_eval_add();
  }