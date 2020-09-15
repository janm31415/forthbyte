#pragma once

#include <immutable/vector.h>
#include <string>
#include <optional>
#include <stdint.h>


typedef immutable::vector<wchar_t, false, 5> line;
typedef immutable::vector<immutable::vector<wchar_t, false, 5>, false, 5> text;

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

struct snapshot
  {
  text content;
  position pos;
  std::optional<position> start_selection;
  uint8_t modification_mask;
  };

struct file_buffer
  {
  text content;
  std::string name;
  position pos;
  std::optional<position> start_selection;
  immutable::vector<snapshot, false> history;
  uint64_t undo_redo_index;
  uint8_t modification_mask;
  };

bool has_selection(file_buffer fb);

position get_actual_position(file_buffer fb);

file_buffer make_empty_buffer();

file_buffer read_from_file(const std::string& filename);

file_buffer save_to_file(bool& success, file_buffer fb, const std::string& filename);

file_buffer start_selection(file_buffer fb);

file_buffer clear_selection(file_buffer fb);

file_buffer insert(file_buffer fb, const std::string& txt, bool save_undo = true);

file_buffer insert(file_buffer fb, text txt, bool save_undo = true);

file_buffer erase(file_buffer fb, bool save_undo = true);

file_buffer erase_right(file_buffer fb, bool save_undo = true);

file_buffer push_undo(file_buffer fb);

text get_selection(file_buffer fb);

file_buffer undo(file_buffer fb);

file_buffer redo(file_buffer fb);

file_buffer select_all(file_buffer fb);