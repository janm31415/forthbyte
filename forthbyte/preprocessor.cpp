#include "preprocessor.h"
#include <sstream>

#include <jtk/file_utils.h>


preprocess_settings preprocess(text code)
  {
  preprocess_settings out;
  bool _float = true;
  uint64_t sample_rate = 8000;

  auto it = code.begin();
  auto it_end = code.end();
  for (; it != it_end; ++it)
    {
    auto ln = *it;
    auto line_it = ln.begin();
    auto line_it_end = ln.end();
    while (line_it != line_it_end && (*line_it == L' ' || *line_it == L'\t'))
      ++line_it;
    std::wstring first_word = read_next_word(line_it, line_it_end);
    if (first_word == L"#samplerate")
      {
      line_it += first_word.length();
      while (line_it != line_it_end && (*line_it == L' ' || *line_it == L'\t'))
        ++line_it;
      std::wstring second_word = read_next_word(line_it, line_it_end);
      std::wstringstream str;
      str << second_word;
      str >> sample_rate;
      }
    else if (first_word == L"#byte")
      {
      _float = false;
      }
    else if (first_word == L"#float")
      {
      _float = true;
      }
    else if (first_word == L"#initmemory")
      {
      std::wstring current_word = first_word;
      while (!current_word.empty())
        {
        line_it += current_word.length();
        while (line_it != line_it_end && (*line_it == L' ' || *line_it == L'\t'))
          ++line_it;
        current_word = read_next_word(line_it, line_it_end);
        if (!current_word.empty())
          {
          out.init_memory.push_back(jtk::convert_wstring_to_string(current_word));
          }
        }
      }
    else if (!first_word.empty() && first_word[0] == L'#')
      {
      std::stringstream str;
      str << "Unknown preprocessor directive: " << jtk::convert_wstring_to_string(first_word);
      throw std::logic_error(str.str());
      }
    }
    
  out._float = _float;
  out._sample_rate = sample_rate;
  return out;
  }

