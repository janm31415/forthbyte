#pragma once

#include <immutable/vector.h>
#include <vector>
#include <string>
#include <optional>
#include <stdint.h>


typedef immutable::vector<wchar_t, false, 5> line;
typedef immutable::vector<immutable::vector<wchar_t, false, 5>, false, 5> text;
typedef immutable::vector<uint8_t, false, 5> lexer_status;

#define lexer_normal 0
#define lexer_inside_multiline_comment 1
#define lexer_inside_multiline_string 2

struct position
  {
  position() : row(-1), col(-1) {}
  position(int64_t r, int64_t c) : row(r), col(c) {}
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

  bool operator <= (const position& other) const
    {
    return (*this < other) | (*this == other);
    }

  bool operator > (const position& other) const
    {
    return other < *this;
    }

  bool operator >= (const position& other) const
    {
    return other <= *this;
    }

  };

struct snapshot
  {
  text content;
  lexer_status lex;
  position pos;
  std::optional<position> start_selection;
  uint8_t modification_mask;
  bool rectangular_selection;
  };

struct syntax_settings
  {
  syntax_settings() : uses_quotes_for_chars(false), should_highlight(false) {}
  std::string multiline_begin, multiline_end, single_line, multistring_begin, multistring_end;
  bool uses_quotes_for_chars;
  bool should_highlight;
  };

struct file_buffer
  {
  text content;
  lexer_status lex;
  immutable::vector<snapshot, false> history;
  syntax_settings syntax;
  std::string name;  
  position pos;
  int64_t xpos;
  std::optional<position> start_selection;  
  uint64_t undo_redo_index;
  uint8_t modification_mask;
  bool rectangular_selection;
  };

struct env_settings
  {
  int tab_space;
  bool show_all_characters;
  };

uint32_t character_width(uint32_t character, int64_t x_pos, const env_settings& s);

int64_t line_length_up_to_column(line ln, int64_t column, const env_settings& s);

int64_t get_col_from_line_length(line ln, int64_t length, const env_settings& s);

int64_t get_x_position(file_buffer fb, const env_settings& s);

bool in_selection(file_buffer fb, position current, position cursor, position buffer_pos, std::optional<position> start_selection, bool rectangular, const env_settings& s);

bool has_selection(file_buffer fb);

bool has_rectangular_selection(file_buffer fb);

bool has_trivial_rectangular_selection(file_buffer fb, const env_settings& s);

bool has_nontrivial_selection(file_buffer fb, const env_settings& s);

void get_rectangular_selection(int64_t& min_row, int64_t& max_row, int64_t& min_x, int64_t& max_x, file_buffer fb, position p1, position p2, const env_settings& s);

position get_actual_position(file_buffer fb);

file_buffer make_empty_buffer();

file_buffer read_from_file(std::string filename);

file_buffer save_to_file(bool& success, file_buffer fb, const std::string& filename);

file_buffer start_selection(file_buffer fb);

file_buffer clear_selection(file_buffer fb);

file_buffer insert(file_buffer fb, const std::string& txt, const env_settings& s, bool save_undo = true);

file_buffer insert(file_buffer fb, std::wstring wtxt, const env_settings& s, bool save_undo = true);

file_buffer insert(file_buffer fb, text txt, const env_settings& s, bool save_undo = true);

file_buffer erase(file_buffer fb, const env_settings& s, bool save_undo = true);

file_buffer erase_right(file_buffer fb, const env_settings& s, bool save_undo = true);

file_buffer push_undo(file_buffer fb);

text get_selection(file_buffer fb, const env_settings& s);

file_buffer undo(file_buffer fb, const env_settings& s);

file_buffer redo(file_buffer fb, const env_settings& s);

file_buffer select_all(file_buffer fb, const env_settings& s);

file_buffer move_left(file_buffer fb, const env_settings& s);

file_buffer move_right(file_buffer fb, const env_settings& s);

file_buffer move_up(file_buffer fb, const env_settings& s);

file_buffer move_down(file_buffer fb, const env_settings& s);

file_buffer move_page_up(file_buffer fb, int64_t rows, const env_settings& s);

file_buffer move_page_down(file_buffer fb, int64_t rows, const env_settings& s);

file_buffer move_home(file_buffer fb, const env_settings& s);

file_buffer move_end(file_buffer fb, const env_settings& s);

file_buffer update_position(file_buffer fb, position pos, const env_settings& s);

std::string buffer_to_string(file_buffer fb);

position get_last_position(text txt);

position get_last_position(file_buffer fb);

text to_text(const std::string& txt);

text to_text(std::wstring wtxt);

std::string to_string(text txt);

std::wstring to_wstring(text txt);

file_buffer find_text(file_buffer fb, text txt);

file_buffer find_text(file_buffer fb, const std::wstring& wtxt);

file_buffer find_text(file_buffer fb, const std::string& txt);

position get_next_position(text txt, position pos);

position get_next_position(file_buffer fb, position pos);

position get_previous_position(text txt, position pos);

position get_previous_position(file_buffer fb, position pos);

bool valid_position(text txt, position pos);

bool valid_position(file_buffer fb, position pos);

uint8_t get_end_of_line_lexer_status(file_buffer fb, int64_t row);

file_buffer init_lexer_status(file_buffer fb);

file_buffer update_lexer_status(file_buffer fb, int64_t row);

file_buffer update_lexer_status(file_buffer fb, int64_t from_row, int64_t to_row);

enum text_type
  {
  tt_normal,
  tt_comment,
  tt_string
  };

/*
first index in pair equals the column where the text type (second index in pair) starts.
The text type is valid till the next index or the end of the line.
*/
std::vector<std::pair<int64_t, text_type>> get_text_type(file_buffer fb, int64_t row);

/*
When selecting ( you want to find the corresponding ).
This method looks for corresponding tokens, inside the range (minrow, maxrow).
*/
position find_corresponding_token(file_buffer fb, position tokenpos, int64_t minrow, int64_t maxrow);

std::wstring read_next_word(line::const_iterator it, line::const_iterator it_end);

position get_indentation_at_row(file_buffer fb, int64_t row);

std::string get_row_indentation_pattern(file_buffer fb, position pos);
