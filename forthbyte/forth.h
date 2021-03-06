#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <sstream>
#include <variant>
#include <vector>
#include <cmath>

namespace forth
  {

  struct token
    {
    enum e_type
      {
      T_WORD,
      T_VALUE,
      T_COLON,
      T_SEMICOLON
      };

    e_type type;
    std::string value;
    int line_nr;
    int column_nr;

    token(e_type i_type, const std::string& v, int i_line_nr, int i_column_nr) : type(i_type), value(v), line_nr(i_line_nr), column_nr(i_column_nr) {}
    };

  std::vector<token> tokenize(const std::string& str);

  template <class T, int N = 256>
  class interpreter
    {
    public:

      interpreter();


      typedef void(interpreter::* primitive_fun_ptr)();

      struct Value
        {
        T val;
        };

      struct Primitive
        {
        primitive_fun_ptr fun;
        };

      struct Variable
        {
        int index;
        };

      typedef std::variant<Value, Primitive, Variable> Statement;
      typedef std::vector<Statement> Statements;

      struct Definition
        {
        Statements statements;
        std::string name;
        };

      struct Program
        {
        Statements statements;
        };

      typedef std::map<std::string, Statements> Dictionary;

      Dictionary dictionary;

      Value parse_value(std::vector<token>& tokens);      
      Statements parse_word(std::vector<token>& tokens);
      Definition parse_definition(std::vector<token>& tokens);
      Program parse(std::vector<token>& tokens);

      void make_variable(const std::string& name);

      void set_variable_value(const std::string& name, T value);

      T top();
      T second();
      T pop();
      void push(T val);

      void primitive_add();
      void primitive_sub();
      void primitive_mul();
      void primitive_div();
      void primitive_left_shift();
      void primitive_right_shift();
      void primitive_and();
      void primitive_or();
      void primitive_xor();
      void primitive_not();
      void primitive_sin();
      void primitive_cos();
      void primitive_mod();
      void primitive_less();
      void primitive_greater();
      void primitive_leq();
      void primitive_geq();
      void primitive_eq();
      void primitive_neq();
      void primitive_dup();
      void primitive_pick();
      void primitive_drop();
      void primitive_2dup();
      void primitive_over();
      void primitive_nip();
      void primitive_tuck();
      void primitive_swap();
      void primitive_rot();
      void primitive_mrot();
      void primitive_min();
      void primitive_max();
      void primitive_pow();
      void primitive_atan2();
      void primitive_negate();
      void primitive_tan();
      void primitive_log();
      void primitive_exp();
      void primitive_sqrt();
      void primitive_floor();
      void primitive_ceil();
      void primitive_abs();
      void primitive_fetch();
      void primitive_store();
      void primitive_return_stack_push();
      void primitive_return_stack_pop();

      void eval(const Program& prog);

      typedef std::map<std::string, primitive_fun_ptr> primitive_map;
      primitive_map primitives;

      typedef std::map<std::string, int> variable_map;
      variable_map variables;

      std::array<T, N> stack;
      int stack_pointer;
      std::array<T, N> globals;
      int variable_index;
      std::array<T, N> memory_stack;
      std::array<T, N> return_stack;
      int return_stack_pointer;
    };

  namespace details
    {

    enum error_type
      {
      bad_syntax,
      no_tokens,
      value_expected,
      word_expected,
      unknown_word,
      expected_token,
      definition_in_definition,
      variable_overflow,
      unknown_variable
      };

    inline void _throw_error(int line_nr, int column_nr, error_type t, std::string extra)
      {
      std::stringstream str;
      str << "error:";
      if (line_nr >= 0)
        str << line_nr << ":";
      if (column_nr >= 0)
        str << column_nr << ":";
      str << " ";
      switch (t)
        {
        case bad_syntax:
          str << "Bad syntax";
          break;
        case no_tokens:
          str << "I expect more tokens at the end";
          break;
        case value_expected:
          str << "I expect a value";
          break;
        case word_expected:
          str << "I expect a word";
          break;
        case unknown_word:
          str << "Unknown word";
          break;
        case expected_token:
          str << "I expect a token";
          break;
        case definition_in_definition:
          str << "A definition inside a definition is not allowed";
          break;
        case variable_overflow:
          str << "No memory anymore for variables";
          break;
        case unknown_variable:
          str << "Unknown variable";
          break;
        }
      if (!extra.empty())
        str << "-> " << extra;
      throw std::logic_error(str.str());
      }

    inline token _take(std::vector<token>& tokens)
      {
      if (tokens.empty())
        {
        _throw_error(-1, -1, no_tokens, "");
        }
      token t = tokens.back();
      tokens.pop_back();
      return t;
      }

    inline void _require(std::vector<token>& tokens, std::string required)
      {
      if (tokens.empty())
        {
        _throw_error(-1, -1, expected_token, required);
        }
      auto t = _take(tokens);
      if (t.value != required)
        {
        _throw_error(t.line_nr, t.column_nr, expected_token, required);
        }
      }

    inline int _is_number(int* is_real, int* is_scientific, const char* value)
      {
      if (value[0] == '\0')
        return 0;
      int i = 0;
      if (value[0] == 'e' || value[0] == 'E')
        return 0;
      if (value[0] == '-' || value[0] == '+')
        {
        ++i;
        if (value[1] == '\0')
          return 0;
        }
      *is_real = 0;
      *is_scientific = 0;
      const char* s = value + i;
      while (*s != '\0')
        {
        if (isdigit((unsigned char)(*s)) == 0)
          {
          if ((*s == '.') && (*is_real == 0) && (*is_scientific == 0))
            *is_real = 1;
          else if ((*s == 'e' || *s == 'E') && (*is_scientific == 0))
            {
            *is_scientific = 1;
            *is_real = 1;
            if (*(s + 1) == '\0')
              return 0;
            if (*(s + 1) == '-' || *(s + 1) == '+')
              {
              ++s;
              }
            if (*(s + 1) == '\0')
              return 0;
            }
          else
            return 0;
          }
        ++s;
        }
      return 1;
      }

    inline bool _ignore_character(const char& ch)
      {
      return (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r');
      }

    inline bool _hex_to_uint64_t(uint64_t& hexvalue, const std::string& h)
      {
      hexvalue = 0;
      if (h.empty())
        return false;
      int i = (int)h.size() - 1;
      int j = 0;
      for (; i >= 0; --i)
        {
        int v = h[i] >= 'A' ? (h[i] >= 'a' ? ((int)h[i] - 87) : ((int)h[i] - 55)) : ((int)h[i] - 48);
        if (v > 15 || v < 0)
          return false;
        hexvalue |= (((uint64_t)v) << (4 * j));
        ++j;
        }

      return true;
      }

    inline void _treat_buffer(std::string& buff, std::vector<token>& tokens, int line_nr, int column_nr)
      {
      if (!buff.empty() && buff[0] != '\0')
        {
        int is_real;
        int is_scientific;
        if (_is_number(&is_real, &is_scientific, buff.c_str()))
          tokens.emplace_back(token::T_VALUE, buff, line_nr, column_nr - (int)buff.length());
        else if (buff.size() > 2 && buff[0] == '0' && buff[1] == 'x')
          {
          uint64_t hexvalue;
          std::string remainder = buff.substr(2);
          if (_hex_to_uint64_t(hexvalue, remainder))
            {
            std::stringstream ss;
            ss << hexvalue;
            tokens.emplace_back(token::T_VALUE, ss.str(), line_nr, column_nr - (int)buff.length());
            }
          else
            tokens.emplace_back(token::T_WORD, buff, line_nr, column_nr - (int)buff.length());

          }
        else
          tokens.emplace_back(token::T_WORD, buff, line_nr, column_nr - (int)buff.length());
        }
      buff.clear();
      }
    }

  inline std::vector<token> tokenize(const std::string& str)
    {
    using namespace details;

    std::vector<token> tokens;
    std::string buff;

    const char* s = str.c_str();
    const char* s_end = str.c_str() + str.length();

    int line_nr = 1;
    int column_nr = 1;

    while (s < s_end)
      {
      if (_ignore_character(*s))
        {
        _treat_buffer(buff, tokens, line_nr, column_nr);

        while (_ignore_character(*s))
          {
          if (*s == '\n')
            {
            ++line_nr;
            column_nr = 0;
            }
          ++s;
          ++column_nr;
          }
        }

      const char* s_copy = s;

      switch (*s)
        {

        case '/': 
        {
        const char* t = s;
        ++t;
        if (t && *t == '/') // treat as single line comment
          {
          _treat_buffer(buff, tokens, line_nr, column_nr);
          while (*s && *s != '\n') // comment, so skip till end of the line
            ++s;
          ++s;
          ++line_nr;
          column_nr = 1;
          }
        else if (t && *t == '*') // treat as multiline comment
          {
          _treat_buffer(buff, tokens, line_nr, column_nr);
          ++s;
          bool done = false;
          while (*s && !done)
            {
            if (*s == '\n')
              {
              ++line_nr;
              column_nr = 0;
              }
            if (*s == '*')
              {
              t = s;
              ++t;
              if (t && *t == '/')
                {
                done = true;
                ++s;
                ++column_nr;
                }
              }
            ++s;
            ++column_nr;
            }
          }
        break;        
        }
        case ':':
        {
        _treat_buffer(buff, tokens, line_nr, column_nr);
        tokens.emplace_back(token::T_COLON, ":", line_nr, column_nr);
        ++s;
        ++column_nr;
        break;
        }
        case ';':
        {
        _treat_buffer(buff, tokens, line_nr, column_nr);
        tokens.emplace_back(token::T_SEMICOLON, ";", line_nr, column_nr);
        ++s;
        ++column_nr;
        break;
        }       
        case '#': // can be used for preprocessing
        {
        _treat_buffer(buff, tokens, line_nr, column_nr);
        while (*s && *s != '\n') // preprocessor stuff, so skip till end of the line
          ++s;
        ++s;
        ++line_nr;
        column_nr = 1;
        }
        }

      if (s_copy == s)
        {
        buff += *s;
        ++s;
        ++column_nr;
        }

      } // while (s < s_end)
    _treat_buffer(buff, tokens, line_nr, column_nr);

    return tokens;
    }

  template <class T, int N>
  interpreter<T, N>::interpreter() : stack_pointer(0), variable_index(0), return_stack_pointer(0)
    {
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("+", &interpreter::primitive_add));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("-", &interpreter::primitive_sub));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("*", &interpreter::primitive_mul));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("/", &interpreter::primitive_div));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("<<", &interpreter::primitive_left_shift));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>(">>", &interpreter::primitive_right_shift));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("&", &interpreter::primitive_and));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("|", &interpreter::primitive_or));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("^", &interpreter::primitive_xor));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("not", &interpreter::primitive_not));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("sin", &interpreter::primitive_sin));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("cos", &interpreter::primitive_cos));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("%", &interpreter::primitive_mod));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("<", &interpreter::primitive_less));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>(">", &interpreter::primitive_greater));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("<=", &interpreter::primitive_leq));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>(">=", &interpreter::primitive_geq));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("=", &interpreter::primitive_eq));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("<>", &interpreter::primitive_neq));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("dup", &interpreter::primitive_dup));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("pick", &interpreter::primitive_pick));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("drop", &interpreter::primitive_drop));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("2dup", &interpreter::primitive_2dup));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("over", &interpreter::primitive_over));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("nip", &interpreter::primitive_nip));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("tuck", &interpreter::primitive_tuck));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("swap", &interpreter::primitive_swap));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("rot", &interpreter::primitive_rot));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("-rot", &interpreter::primitive_mrot));   
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("min", &interpreter::primitive_min));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("max", &interpreter::primitive_max));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("pow", &interpreter::primitive_pow));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("atan2", &interpreter::primitive_atan2));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("negate", &interpreter::primitive_negate));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("tan", &interpreter::primitive_tan));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("log", &interpreter::primitive_log));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("exp", &interpreter::primitive_exp));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("sqrt", &interpreter::primitive_sqrt));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("floor", &interpreter::primitive_floor));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("ceil", &interpreter::primitive_ceil));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("abs", &interpreter::primitive_abs));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("@", &interpreter::primitive_fetch));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("!", &interpreter::primitive_store));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>(">r", &interpreter::primitive_return_stack_push));
    primitives.insert(std::pair<std::string, primitive_fun_ptr>("r>", &interpreter::primitive_return_stack_pop));
    }

  template <class T, int N>
  typename interpreter<T, N>::Value interpreter<T, N>::parse_value(std::vector<token>& tokens)
    {
    using namespace details;
    auto t = _take(tokens);
    if (t.type != token::T_VALUE)
      _throw_error(t.line_nr, t.column_nr, value_expected, "");
    Value v;
    std::stringstream str;
    str << t.value;
    str >> v.val;
    return v;
    }

  template <class T, int N>
  typename interpreter<T, N>::Statements interpreter<T, N>::parse_word(std::vector<token>& tokens)
    {
    using namespace details;
    auto t = _take(tokens);
    if (t.type != token::T_WORD)
      _throw_error(t.line_nr, t.column_nr, word_expected, "");    
    Statements stmts;
    auto it0 = variables.find(t.value);
    if (it0 != variables.end())
      {
      Variable v;
      v.index = it0->second;
      stmts.push_back(v);
      return stmts;
      }
    auto it = dictionary.find(t.value);
    if (it != dictionary.end())
      return it->second;    
    auto it2 = primitives.find(t.value);
    if (it2 != primitives.end())
      {
      Primitive p;
      p.fun = it2->second;
      stmts.push_back(p);
      }
    else
      {
      _throw_error(t.line_nr, t.column_nr, unknown_word, t.value);
      }
    return stmts;
    }

  template <class T, int N>
  typename interpreter<T, N>::Definition interpreter<T, N>::parse_definition(std::vector<token>& tokens)
    {
    using namespace details;
    _require(tokens, ":");
    auto name_token = _take(tokens);
    if (name_token.type != token::T_WORD)
      _throw_error(name_token.line_nr, name_token.column_nr, word_expected, name_token.value);
    Definition def;
    def.name = name_token.value;
    while (!tokens.empty() && tokens.back().type != token::T_SEMICOLON)
      {
      const token& t = tokens.back();
      switch (t.type)
        {
        case token::T_WORD:
        {
        auto stmts = parse_word(tokens);
        def.statements.insert(def.statements.end(), stmts.begin(), stmts.end());
        break;
        }
        case token::T_VALUE:
        {
        def.statements.push_back(parse_value(tokens));
        break;
        }
        case token::T_COLON:
        {
        _throw_error(t.line_nr, t.column_nr, definition_in_definition, "");
        break;
        }
        default:
        {
        _throw_error(t.line_nr, t.column_nr, bad_syntax, "");
        break;
        }
        }
      }
    _require(tokens, ";");
    return def;
    }

  template <class T, int N>
  void interpreter<T, N>::make_variable(const std::string& name)
    {    
    using namespace details;
    if (variable_index >= N)
      _throw_error(-1, -1, variable_overflow, "");
    variables[name] = variable_index;
    ++variable_index;
    }

  template <class T, int N>
  void interpreter<T, N>::set_variable_value(const std::string& name, T value)
    {
    using namespace details;
    auto it = variables.find(name);
    if (it != variables.end())
      {
      globals[it->second] = value;
      }
    else
      _throw_error(-1, -1, unknown_variable, name);
    }

  template <class T, int N>
  typename interpreter<T, N>::Program interpreter<T, N>::parse(std::vector<token>& tokens)
    {
    using namespace details;

    Program prog;
    std::reverse(tokens.begin(), tokens.end());

    while (!tokens.empty())
      {
      const token& t = tokens.back();
      switch (t.type)
        {
        case token::T_WORD:
        {
        auto stmts = parse_word(tokens);
        prog.statements.insert(prog.statements.end(), stmts.begin(), stmts.end());
        break;
        }
        case token::T_VALUE:
        {
        prog.statements.push_back(parse_value(tokens));
        break;
        }
        case token::T_COLON:
        {
        auto def = parse_definition(tokens);
        dictionary[def.name] = def.statements;
        break;
        }
        default:
        {
        _throw_error(t.line_nr, t.column_nr, bad_syntax, "");
        break;
        }
        }
      }

    return prog;
    }

  template <class T, int N>
  void interpreter<T, N>::eval(const Program& prog)
    {
    for (const auto& s : prog.statements)
      {
      if (std::holds_alternative<Value>(s))
        push(std::get<Value>(s).val);
      else if (std::holds_alternative<Primitive>(s))
        {
        primitive_fun_ptr ptr = std::get<Primitive>(s).fun;
        (this->*ptr)();
        }
      else if (std::holds_alternative<Variable>(s))
        push(globals[std::get<Variable>(s).index]);
      }
    }

  template <class T>
  struct true_value
    {
    inline static T value = (T)0xffffffffffffffff;
    };

  template <>
  struct true_value<double>
    {
    inline static double value = 1.0;
    };

  template <>
  struct true_value<float>
    {
    inline static double value = 1.f;
    };

  template <class T>
  inline T not_value(T input)
    {
    return (T)(~(uint64_t)input);
    }

  template <>
  inline double not_value(double input)
    {
    return input == 0.0 ? 1.0 : 0.0;
    }

  template <>
  inline float not_value(float input)
    {
    return input == 0.f ? 1.f : 0.f;
    }

  template <class T>
  inline T modulo(T a, T b)
    {
    return a % b;
    }

  template <>
  inline double modulo(double a, double b)
    {
    return fmod(a, b);
    }

  template <>
  inline float modulo(float a, float b)
    {
    return fmod(a, b);
    }

  template <class T, int N>
  inline T interpreter<T, N>::top()
    {
    return stack_pointer ? stack[stack_pointer-1] : stack[N-1];
    }

  template <class T, int N>
  inline T interpreter<T, N>::second()
    {
    return stack_pointer>1 ? stack[stack_pointer - 2] : stack[stack_pointer + N - 2];
    }

  template <class T, int N>
  inline T interpreter<T, N>::pop()
    {
    --stack_pointer;
    if (stack_pointer < 0)
      stack_pointer = N - 1;
    return stack[stack_pointer];
    }

  template <class T, int N>
  inline void interpreter<T, N>::push(T val)
    {
    stack[stack_pointer] = val;
    ++stack_pointer;
    if (stack_pointer >= N)
      stack_pointer = 0;
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_add()
    {
    T b = pop();
    T a = pop();
    push(a + b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_sub()
    {
    T b = pop();
    T a = pop();
    push(a - b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_mul()
    {
    T b = pop();
    T a = pop();
    if (a == 0 || b == 0) // multiplying with infinity gives 0
      push(0);
    else
      push(a * b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_div()
    {
    T b = pop();
    T a = pop();
    if (b)
      push(a / b);
    else
      push(std::numeric_limits<T>::infinity());
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_left_shift()
    {
    T b = pop();
    T a = pop();
    push((T)((uint64_t)a << (uint64_t)b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_right_shift()
    {
    T b = pop();
    T a = pop();
    push((T)((uint64_t)a >> (uint64_t)b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_and()
    {
    T b = pop();
    T a = pop();
    push((T)((uint64_t)a & (uint64_t)b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_or()
    {
    T b = pop();
    T a = pop();
    push((T)((uint64_t)a | (uint64_t)b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_xor()
    {
    T b = pop();
    T a = pop();
    push((T)((uint64_t)a ^ (uint64_t)b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_not()
    {
    T a = pop();
    push(not_value(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_sin()
    {
    T a = pop();
    push((T)std::sin(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_cos()
    {
    T a = pop();
    push((T)std::cos(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_mod()
    {
    T b = pop();
    T a = pop();
    push(modulo(a, b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_less()
    {
    T b = pop();
    T a = pop();
    if (a < b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_greater()
    {
    T b = pop();
    T a = pop();
    if (a > b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_leq()
    {
    T b = pop();
    T a = pop();
    if (a <= b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_geq()
    {
    T b = pop();
    T a = pop();
    if (a >= b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_eq()
    {
    T b = pop();
    T a = pop();
    if (a == b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_neq()
    {
    T b = pop();
    T a = pop();
    if (a != b)
      push(true_value<T>::value);
    else
      push((T)0);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_dup()
    {
    // (x -- xx)
    T a = top();
    push(a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_pick()
    {
    T a = pop();
    int64_t sp = stack_pointer - (int64_t)a - 1;    
    while (sp < 0)
      sp += N;
    push(stack[sp % N]);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_drop()
    {
    // (x --)
    --stack_pointer;
    if (stack_pointer < 0)
      stack_pointer = N - 1;
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_2dup()
    {
    //(xy -- xyxy)
    T a = top();
    T b = second();
    push(b);
    push(a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_over()
    {
    //(xy -- xyx)
    T a = second();
    push(a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_nip()
    {
    //(xy -- y)
    T a = pop();
    stack[stack_pointer ? stack_pointer-1 : N-1] = a;
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_tuck()
    {
    // (xy -- yxy)
    T b = pop();
    T a = pop();
    push(b);
    push(a);
    push(b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_swap()
    {
    // (xy -- yx)
    T b = pop();
    T a = pop();
    push(b);
    push(a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_rot()
    {
    // (abc -- bca)
    T c = pop();
    T b = pop();
    T a = pop();
    push(b);
    push(c);
    push(a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_mrot()
    {
    // (abc -- cab)
    T c = pop();
    T b = pop();
    T a = pop();
    push(c);
    push(a);
    push(b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_min()
    {
    T b = pop();
    T a = pop();
    if (a < b)
      push(a);
    else
      push(b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_max()
    {
    T b = pop();
    T a = pop();
    if (a > b)
      push(a);
    else
      push(b);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_pow()
    {
    T b = pop();
    T a = pop();
    push((T)std::pow(a, b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_atan2()
    {
    T b = pop();
    T a = pop();
    push((T)std::atan2(a, b));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_negate()
    {
    T a = pop();
    push((T)-a);
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_tan()
    {
    T a = pop();
    push((T)std::tan(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_log()
    {
    T a = pop();
    push((T)std::log(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_exp()
    {
    T a = pop();
    push((T)std::exp(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_sqrt()
    {
    T a = pop();
    push((T)std::sqrt(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_floor()
    {
    T a = pop();
    push((T)std::floor(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_ceil()
    {
    T a = pop();
    push((T)std::ceil(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_abs()
    {
    T a = pop();
    push((T)std::abs(a));
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_fetch()
    {
    /*
    (addr -- value_from_addr)
     */
    T a = pop();
    int index = ((int)a) % N;
    push(memory_stack[index]);
    }
    
  template <class T, int N>
  void interpreter<T, N>::primitive_store()
    {
    /*
    (x addr -- )
    */
    
    T b = pop();
    T a = pop();
    int index = ((int)b) % N;
    memory_stack[index] = a;
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_return_stack_push()
    {
    T a = pop();
    return_stack[return_stack_pointer] = a;
    ++return_stack_pointer;
    if (return_stack_pointer >= N)
      return_stack_pointer = 0;
    }

  template <class T, int N>
  void interpreter<T, N>::primitive_return_stack_pop()
    {
    --return_stack_pointer;
    if (return_stack_pointer < 0)
      return_stack_pointer = N - 1;
    push(return_stack[return_stack_pointer]);
    }
  } // namespace forth
