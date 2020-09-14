#pragma once

#include <immutable/vector.h>
#include <string>
#include <optional>
#include <stdint.h>


typedef immutable::vector<wchar_t, false, 5> line;
typedef immutable::vector<typename line, false, 5> text;

struct position
  {
  int64_t row, col;

  bool operator < (const position& other) const
    {
    return row < other.row || (row == other.row && col < other.col);
    }

  bool operator == (const position& other) const
    {
    return row == other.row && col == other.col;
    }

  bool  operator != (const position& other) const
    {
    return !(*this == other);
    }
  };

struct file_buffer
  {
  text content;
  std::string name;
  position pos;
  std::optional<position> start_selection;
  };

position get_actual_position(file_buffer fb);

file_buffer make_empty_buffer();

file_buffer read_from_file(const std::string& filename);

file_buffer start_selection(file_buffer fb);

file_buffer clear_selection(file_buffer fb);

file_buffer insert(file_buffer fb, const std::string& txt);

file_buffer erase(file_buffer fb);

file_buffer erase_right(file_buffer fb);