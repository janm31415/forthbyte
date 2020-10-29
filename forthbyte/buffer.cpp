#include "buffer.h"

#include <fstream>

#include <jtk/file_utils.h>

#include "utils.h"

file_buffer make_empty_buffer()
  {
  file_buffer fb;
  fb.pos.row = fb.pos.col = 0;
  fb.xpos = 0;
  fb.start_selection = std::nullopt;
  fb.modification_mask = 0;
  fb.undo_redo_index = 0;
  fb.rectangular_selection = false;
  return fb;
  }

namespace
  {
  inline bool local_remove_quotes(std::string& cmd)
    {
    bool has_quotes = false;
    while (cmd.size() >= 2 && cmd.front() == '"' && cmd.back() == '"')
      {
      cmd.erase(cmd.begin());
      cmd.pop_back();
      has_quotes = true;
      }
    return has_quotes;
    }
  }

file_buffer read_from_file(std::string filename)
  {
  using namespace jtk;
  local_remove_quotes(filename); 
  file_buffer fb = make_empty_buffer();
  fb.name = filename;  
  if (file_exists(filename))
    {
#ifdef _WIN32
    std::wstring wfilename = convert_string_to_wstring(filename);
#else
    std::string wfilename(filename);
#endif
    auto f = std::ifstream{ wfilename };
    auto trans_lines = fb.content.transient();
    try
      {
      while (!f.eof())
        {
        std::string file_line;
        std::getline(f, file_line);
        auto trans = line().transient();
        auto it = file_line.begin();
        auto it_end = file_line.end();
        utf8::utf8to16(it, it_end, std::back_inserter(trans));
        if (!f.eof())
          trans.push_back('\n');
        trans_lines.push_back(trans.persistent());
        }
      }
    catch (...)
      {
      while (!trans_lines.empty())
        trans_lines.pop_back();
      f.seekg(0);
      while (!f.eof())
        {
        std::string file_line;
        std::getline(f, file_line);
        auto trans = line().transient();
        auto it = file_line.begin();
        auto it_end = file_line.end();
        for (; it != it_end; ++it)
          {
          trans.push_back(ascii_to_utf16(*it));
          }
        if (!f.eof())
          trans.push_back('\n');
        trans_lines.push_back(trans.persistent());
        }
      }
    fb.content = trans_lines.persistent();
    f.close();
    }
  else if (is_directory(filename))
    {
    std::wstring wfilename = convert_string_to_wstring(fb.name);
    std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' to '/'
    if (wfilename.back() != L'/')
      wfilename.push_back(L'/');
    fb.name = convert_wstring_to_string(wfilename);
    auto trans_lines = fb.content.transient();

    line dots;
    dots = dots.push_back(L'.');
    dots = dots.push_back(L'.');
    dots = dots.push_back(L'\n');
    trans_lines.push_back(dots);

    auto items = get_subdirectories_from_directory(filename, false);
    std::sort(items.begin(), items.end(), [](const std::string& lhs, const std::string& rhs)
      {
      const auto result = std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
      return result.second != rhs.cend() && (result.first == lhs.cend() || tolower(*result.first) < tolower(*result.second));
      });
    for (auto& item : items)
      {
      item = get_filename(item);
      auto witem = convert_string_to_wstring(item);
      line ln;
      auto folder_content = ln.transient();
      for (auto ch : witem)
        folder_content.push_back(ch);
      folder_content.push_back(L'/');
      folder_content.push_back(L'\n');
      trans_lines.push_back(folder_content.persistent());
      }
    items = get_files_from_directory(filename, false);
    std::sort(items.begin(), items.end(), [](const std::string& lhs, const std::string& rhs)
      {
      const auto result = std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
      return result.second != rhs.cend() && (result.first == lhs.cend() || tolower(*result.first) < tolower(*result.second));
      });
    for (auto& item : items)
      {
      item = get_filename(item);
      auto witem = convert_string_to_wstring(item);
      line ln;
      auto folder_content = ln.transient();
      for (auto ch : witem)
        folder_content.push_back(ch);
      folder_content.push_back(L'\n');
      trans_lines.push_back(folder_content.persistent());
      }
    fb.content = trans_lines.persistent();
    }

  return fb;
  }

file_buffer save_to_file(bool& success, file_buffer fb, const std::string& filename)
  {
#ifdef _WIN32
  std::wstring wfilename = jtk::convert_string_to_wstring(filename); // filenames are in utf8 encoding
#else
  std::string wfilename(filename);
#endif
  success = false;
  auto f = std::ofstream{ wfilename };
  if (f.is_open())
    {
    for (auto ln : fb.content)
      {
      std::string str;
      auto it = ln.begin();
      auto it_end = ln.end();
      str.reserve(std::distance(it, it_end));
      utf8::utf16to8(it, it_end, std::back_inserter(str));
      f << str;
      }
    f.close();
    success = true;
    fb.modification_mask = 0;
    auto thistory = fb.history.transient();
    for (uint32_t idx = 0; idx < thistory.size(); ++idx)
      {
      auto h = thistory[idx];
      h.modification_mask = 1;
      thistory.set(idx, h);
      }
    fb.history = thistory.persistent();
    }
  return fb;
  }

position get_actual_position(file_buffer fb, position pos)
  {
  position out = pos;
  if (out.row < 0 || out.col < 0)
    return out;
  if (fb.pos.row >= fb.content.size())
    {
    assert(fb.content.empty());
    out.col = out.row = 0;
    return out;
    }
  if (out.col >= fb.content[fb.pos.row].size())
    {
    if (out.row == fb.content.size() - 1) // last row
      {
      if (!fb.content.back().empty())
        {
        if (fb.content.back().back() == L'\n')
          out.col = fb.content[out.row].size() - 1;
        else
          out.col = fb.content[out.row].size();
        }
      else
        out.col = fb.content[out.row].size();
      }
    else
      {
      out.col = (int64_t)fb.content[out.row].size() - 1;
      if (out.col < 0)
        out.col = 0;
      }
    }
  return out;
  }

position get_actual_position(file_buffer fb)
  {
  return get_actual_position(fb, fb.pos);
  }

uint32_t character_width(uint32_t character, int64_t x_pos, const env_settings& s)
  {
  switch (character)
    {
    case 9: return s.tab_space - (x_pos % s.tab_space);
    case 10: return s.show_all_characters ? 2 : 1;
    case 13: return s.show_all_characters ? 2 : 1;
    default: return 1;
    }
  }

int64_t line_length_up_to_column(line ln, int64_t column, const env_settings& s)
  {
  int64_t length = 0;
  int64_t col = 0;
  for (int64_t i = 0; i <= column && i < ln.size(); ++i)
    {
    uint32_t w = character_width(ln[i], col, s);
    length += w;
    col += w;
    }
  return length;
  }

int64_t get_col_from_line_length(line ln, int64_t length, const env_settings& s)
  {
  int64_t le = 0;
  int64_t col = 0;
  int64_t out = 0;
  for (int i = 0; le < length && i < ln.size(); ++i)
    {
    uint32_t w = character_width(ln[i], col, s);
    le += w;
    col += w;
    ++out;
    }
  return out;
  }

bool in_selection(file_buffer fb, position current, position cursor, position buffer_pos, std::optional<position> start_selection, bool rectangular, const env_settings& s)
  {
  bool has_selection = start_selection != std::nullopt;
  if (has_selection)
    {
    if (!rectangular)
      return ((start_selection <= current && current <= cursor) || (cursor <= current && current < start_selection));

    int64_t minx, maxx, minrow, maxrow;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *start_selection, buffer_pos, s);

    int64_t xpos = line_length_up_to_column(fb.content[current.row], current.col - 1, s);

    return (minrow <= current.row && current.row <= maxrow && minx <= xpos && xpos <= maxx);
    }
  return false;
  }

bool has_selection(file_buffer fb)
  {
  if (fb.start_selection && (*fb.start_selection != fb.pos))
    {
    if (*fb.start_selection == get_actual_position(fb))
      return false;
    return true;
    }
  return false;
  }

bool has_rectangular_selection(file_buffer fb)
  {
  return fb.rectangular_selection && has_selection(fb);
  }

bool has_trivial_rectangular_selection(file_buffer fb, const env_settings& s)
  {
  if (has_rectangular_selection(fb))
    {
    int64_t minrow, maxrow, mincol, maxcol;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, *fb.start_selection, fb.pos, s);
    return mincol == maxcol;
    }
  return false;
  }

bool has_nontrivial_selection(file_buffer fb, const env_settings& s)
  {
  if (has_selection(fb))
    {
    if (!fb.rectangular_selection)
      return true;
    int64_t minrow, maxrow, mincol, maxcol;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, *fb.start_selection, fb.pos, s);
    return mincol != maxcol;
    }
  return false;
  }

file_buffer start_selection(file_buffer fb)
  {
  fb.start_selection = get_actual_position(fb);
  return fb;
  }

file_buffer clear_selection(file_buffer fb)
  {
  fb.start_selection = std::nullopt;
  fb.rectangular_selection = false;
  return fb;
  }

file_buffer push_undo(file_buffer fb)
  {
  snapshot ss;
  ss.content = fb.content;
  ss.lex = fb.lex;
  ss.pos = fb.pos;
  ss.start_selection = fb.start_selection;
  ss.modification_mask = fb.modification_mask;
  ss.rectangular_selection = fb.rectangular_selection;
  fb.history = fb.history.push_back(ss);
  fb.undo_redo_index = fb.history.size();
  return fb;
  }

void get_rectangular_selection(int64_t& min_row, int64_t& max_row, int64_t& min_x, int64_t& max_x, file_buffer fb, position p1, position p2, const env_settings& s)
  {
  min_x = line_length_up_to_column(fb.content[p1.row], p1.col - 1, s);
  max_x = line_length_up_to_column(fb.content[p2.row], p2.col - 1, s);
  min_row = p1.row;
  max_row = p2.row;
  if (max_x < min_x)
    std::swap(max_x, min_x);
  if (max_row < min_row)
    std::swap(max_row, min_row);
  }


namespace
  {
  file_buffer insert_rectangular(file_buffer fb, std::wstring wtxt, const env_settings& s, bool save_undo)
    {
    fb.modification_mask |= 1;

    int64_t minrow, maxrow, minx, maxx;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *fb.start_selection, fb.pos, s);

    bool single_line = wtxt.find_first_of(L'\n') == std::wstring::npos;

    if (single_line)
      {
      line input;
      auto trans = input.transient();
      for (auto ch : wtxt)
        trans.push_back(ch);
      input = trans.persistent();

      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        }
      fb.start_selection->row = minrow;
      fb.pos.row = maxrow;

      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) + input.size();
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) + input.size();

      fb.rectangular_selection = true;
      }
    else
      {
      int64_t nr_lines = maxrow - minrow + 1;
      int64_t current_line = 0;
      while (!wtxt.empty() && current_line < nr_lines)
        {

        auto endline_position = wtxt.find_first_of(L'\n');
        if (endline_position != std::wstring::npos)
          ++endline_position;
        std::wstring wline = wtxt.substr(0, endline_position);
        wtxt.erase(0, endline_position);

        if (!wline.empty() && wline.back() == L'\n')
          wline.pop_back();

        line input;
        auto trans = input.transient();
        for (auto ch : wline)
          trans.push_back(ch);
        input = trans.persistent();

        int64_t r = minrow + current_line;
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        ++current_line;
        }
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      fb.pos.row = minrow;
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      }
    fb = update_lexer_status(fb, minrow, maxrow);
    return fb;
    }

  file_buffer insert_rectangular(file_buffer fb, text txt, const env_settings& s, bool save_undo)
    {
    return insert_rectangular(fb, to_wstring(txt), s, save_undo);
    /*
    if (txt.empty())
      return fb;

    fb.modification_mask |= 1;

    int64_t minrow, maxrow, minx, maxx;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *fb.start_selection, fb.pos, s);

    bool single_line = txt.size() == 1;

    if (single_line)
      {
      line input = txt[0];

      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        }
      fb.start_selection->row = minrow;
      fb.pos.row = maxrow;
      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) + input.size();
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) + input.size();
      fb.rectangular_selection = true;
      }
    else
      {
      int64_t nr_lines = maxrow - minrow + 1;
      int64_t current_line = 0;
      int64_t txt_line = 0;
      while (txt_line < txt.size() && current_line < nr_lines)
        {
        auto input = txt[txt_line];

        if (!input.empty() && input.back() == L'\n')
          input = input.pop_back();

        int64_t r = minrow + current_line;
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        ++current_line;
        ++txt_line;
        }
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      fb.pos.row = minrow;
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      }
    fb = update_lexer_status(fb, minrow, maxrow);
    return fb;
    */
    }
  }

int64_t get_x_position(file_buffer fb, const env_settings& s)
  {
  return fb.content.empty() ? 0 : line_length_up_to_column(fb.content[fb.pos.row], fb.pos.col - 1, s);
  }

file_buffer insert(file_buffer fb, std::wstring wtxt, const env_settings& s, bool save_undo)
  {
  if (wtxt.empty())
    return fb;
  if (save_undo)
    fb = push_undo(fb);

  if (has_nontrivial_selection(fb, s))
    fb = erase(fb, s, false);

  if (has_rectangular_selection(fb))
    return insert_rectangular(fb, wtxt, s, false);

  fb.start_selection = std::nullopt;

  fb.modification_mask |= 1;

  auto pos = get_actual_position(fb);
  int nr_of_lines_inserted = 0;
  int64_t first_insertion_row = pos.row;
  while (!wtxt.empty())
    {    
    auto endline_position = wtxt.find_first_of(L'\n');
    if (endline_position != std::wstring::npos)
      ++endline_position;
    std::wstring wline = wtxt.substr(0, endline_position);
    wtxt.erase(0, endline_position);

    line input;
    auto trans = input.transient();
    for (auto ch : wline)
      trans.push_back(ch);
    input = trans.persistent();

    if (pos.row == fb.content.size())
      {
      fb.content = fb.content.push_back(input);
      fb.lex = fb.lex.push_back(lexer_normal);
      fb.pos.col = fb.content.back().size();
      ++nr_of_lines_inserted;
      if (input.back() == L'\n')
        {
        fb.content = fb.content.push_back(line());
        fb.lex = fb.lex.push_back(lexer_normal);
        ++fb.pos.row;
        fb.pos.col = 0;
        pos = fb.pos;
        ++nr_of_lines_inserted;
        }
      }
    else if (input.back() == L'\n')
      {
      auto first_part = fb.content[pos.row].take(pos.col);
      auto second_part = fb.content[pos.row].drop(pos.col);
      fb.content = fb.content.set(pos.row, first_part.insert(pos.col, input));
      fb.content = fb.content.insert(pos.row + 1, second_part);
      fb.lex = fb.lex.insert(pos.row + 1, lexer_normal);
      ++fb.pos.row;
      fb.pos.col = 0;
      pos = fb.pos;
      ++nr_of_lines_inserted;
      }
    else
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].insert(pos.col, input));
      fb.pos.col += input.size();
      }
    }
  fb = update_lexer_status(fb, first_insertion_row, first_insertion_row + nr_of_lines_inserted);
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer insert(file_buffer fb, const std::string& txt, const env_settings& s, bool save_undo)
  {
  std::wstring wtxt = jtk::convert_string_to_wstring(txt);
  return insert(fb, wtxt, s, save_undo);
  }

file_buffer insert(file_buffer fb, text txt, const env_settings& s, bool save_undo)
  {
  return insert(fb, to_wstring(txt), s, save_undo);
  }

file_buffer erase(file_buffer fb, const env_settings& s, bool save_undo)
  {
  if (fb.content.empty())
    return fb;

  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    auto pos = get_actual_position(fb);
    fb.pos = pos;
    fb.start_selection = std::nullopt;
    if (pos.col > 0)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col - 1));
      --fb.pos.col;
      fb = update_lexer_status(fb, pos.row);
      }
    else if (pos.row > 0)
      {
      fb.pos.col = (int64_t)fb.content[pos.row - 1].size() - 1;
      auto l = fb.content[pos.row - 1].pop_back() + fb.content[pos.row];
      fb.content = fb.content.erase(pos.row).set(pos.row - 1, l);
      fb.lex = fb.lex.erase(pos.row);
      --fb.pos.row;
      fb = update_lexer_status(fb, pos.row-1, pos.row);
      }
    fb.xpos = get_x_position(fb, s);
    }
  else
    {

    auto p2 = get_actual_position(fb);
    auto p1 = *fb.start_selection;
    if (p2 < p1)
      {
      std::swap(p1, p2);
      if (!fb.rectangular_selection)
        p2 = get_previous_position(fb, p2);
      }
    if (p1.row == p2.row)
      {
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      auto new_line = fb.content[p1.row].erase(p1.col, p2.col);
      fb.content = fb.content.set(p1.row, new_line);
      fb.pos.col = p1.col;
      fb.pos.row = p1.row;
      if (new_line.empty() && p1.row == (int64_t)fb.content.size() - 1)
        {
        //fb.content = fb.content.pop_back();
        //fb.lex = fb.lex.pop_back();
        //if (fb.content.empty())
        //  {
        //  fb.pos.col = 0;
        //  fb.pos.row = 0;
        //  }
        //else
        //  {
        //  --fb.pos.row;
        //  fb.pos.col = fb.content[fb.pos.row].empty() ? 0 : (int64_t)fb.content[fb.pos.row].size() - 1;
        //  }
        }
      else
        fb = erase_right(fb, s, false);
      }
    else
      {
      if (!fb.rectangular_selection)
        {
        fb.start_selection = std::nullopt;
        bool remove_line = false;
        int64_t tgt = p2.col + 1;
        if (tgt > fb.content[p2.row].size() - 1)
          remove_line = true;
        fb.content = fb.content.set(p2.row, fb.content[p2.row].erase(0, tgt));
        fb.content = fb.content.erase(p1.row + 1, remove_line ? p2.row + 1 : p2.row);
        fb.lex = fb.lex.erase(p1.row + 1, remove_line ? p2.row + 1 : p2.row);
        fb.content = fb.content.set(p1.row, fb.content[p1.row].erase(p1.col, fb.content[p1.row].size() - 1));
        //fb = update_lexer_status(fb, p1.row, p2.row);  << don't think it's necessary, but maybe...
        fb.pos.col = p1.col;
        fb.pos.row = p1.row;
        fb = erase_right(fb, s, false);
        }
      else
        {
        p1 = fb.pos;
        p2 = *fb.start_selection;
        int64_t minx, maxx, minrow, maxrow;
        get_rectangular_selection(minrow, maxrow, minx, maxx, fb, p1, p2, s);
        if (minx == maxx) // trivial rectangular selection
          {
          if (minx > 0)
            {
            for (int64_t r = minrow; r <= maxrow; ++r)
              {
              int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
              int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
              if (len == minx)
                fb.content = fb.content.set(r, fb.content[r].take(current_col - 1) + fb.content[r].drop(current_col));
              }
            fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) - 1;
            fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) - 1;
            }
          }
        else
          {
          for (int64_t r = minrow; r <= maxrow; ++r)
            {
            int64_t min_col = get_col_from_line_length(fb.content[r], minx, s);
            int64_t max_col = get_col_from_line_length(fb.content[r], maxx, s);
            int64_t len_min = line_length_up_to_column(fb.content[r], min_col - 1, s);
            int64_t len_max = line_length_up_to_column(fb.content[r], max_col - 1, s);
            if (len_min <= maxx && len_max >= minx)
              fb.content = fb.content.set(r, fb.content[r].take(min_col) + fb.content[r].drop(max_col + 1));
            }
          fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s);
          fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
          }
        fb = update_lexer_status(fb, minrow, maxrow);
        fb.xpos = get_x_position(fb, s);
        }
      }
    }
  return fb;
  }

file_buffer erase_right(file_buffer fb, const env_settings& s, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    if (fb.content.empty())
      return fb;
    auto pos = get_actual_position(fb);
    fb.pos = pos;
    fb.start_selection = std::nullopt;
    if (pos.col < (int64_t)fb.content[pos.row].size() - 1)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col));
      fb = update_lexer_status(fb, pos.row);
      }
    else if (fb.content[pos.row].empty())
      {
      if (pos.row != (int64_t)fb.content.size() - 1) // not last line
        {
        fb.content = fb.content.erase(pos.row);
        fb.lex = fb.lex.erase(pos.row);
        fb = update_lexer_status(fb, pos.row);
        }
      }
    else if (pos.row < (int64_t)fb.content.size() - 1)
      {
      auto l = fb.content[pos.row].pop_back() + fb.content[pos.row + 1];
      fb.content = fb.content.erase(pos.row + 1).set(pos.row, l);
      fb.lex = fb.lex.erase(pos.row + 1);
      fb = update_lexer_status(fb, pos.row, pos.row+1);
      }
    else if (pos.col == (int64_t)fb.content[pos.row].size() - 1)// last line, last item
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].pop_back());
      }
    fb.xpos = get_x_position(fb, s);
    return fb;
    }
  else
    {
    if (has_trivial_rectangular_selection(fb, s))
      {
      position p1 = fb.pos;
      position p2 = *fb.start_selection;
      int64_t minx, maxx, minrow, maxrow;
      get_rectangular_selection(minrow, maxrow, minx, maxx, fb, p1, p2, s);
      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx && (current_col < fb.content[r].size() - 1 || (current_col == fb.content[r].size() - 1 && r == fb.content.size() - 1)))
          fb.content = fb.content.set(r, fb.content[r].take(current_col) + fb.content[r].drop(current_col + 1));
        }
      fb = update_lexer_status(fb, minrow, maxrow);
      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s);
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      fb.xpos = get_x_position(fb, s);
      return fb;
      }
    return erase(fb, s, false);
    }
  }

text get_selection(file_buffer fb, const env_settings& s)
  {
  auto p2 = get_actual_position(fb);
  if (!has_selection(fb))
    {
    if (fb.content.empty() || fb.content[p2.row].empty())
      return text();
    return text().push_back(line().push_back(fb.content[p2.row][p2.col]));
    }
  auto p1 = *fb.start_selection;
  if (p2 < p1)
    {
    std::swap(p1, p2);
    if (!fb.rectangular_selection)
      p2 = get_previous_position(fb, p2);
    }

  if (p1.row == p2.row)
    {
    line ln = fb.content[p1.row].slice(p1.col, p2.col + 1);
    text t;
    t = t.push_back(ln);
    return t;
    }

  text out;
  auto trans = out.transient();

  if (!fb.rectangular_selection)
    {
    line ln1 = fb.content[p1.row].drop(p1.col);
    trans.push_back(ln1);
    for (int64_t r = p1.row + 1; r < p2.row; ++r)
      trans.push_back(fb.content[r]);
    line ln2 = fb.content[p2.row].take(p2.col + 1);
    trans.push_back(ln2);
    }
  else
    {
    p1 = fb.pos;
    p2 = *fb.start_selection;
    int64_t mincol, maxcol, minrow, maxrow;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, p1, p2, s);
    for (int64_t r = minrow; r <= maxrow; ++r)
      {
      auto ln = fb.content[r].take(maxcol + 1).drop(mincol);
      if ((r != maxrow) && (ln.empty() || ln.back() != L'\n'))
        ln = ln.push_back(L'\n');
      trans.push_back(ln);
      }
    }

  out = trans.persistent();
  return out;
  }

file_buffer undo(file_buffer fb, const env_settings& s)
  {
  if (fb.undo_redo_index == fb.history.size()) // first time undo
    {
    fb = push_undo(fb);
    --fb.undo_redo_index;
    }
  if (fb.undo_redo_index)
    {
    --fb.undo_redo_index;
    snapshot ss = fb.history[(uint32_t)fb.undo_redo_index];
    fb.content = ss.content;
    fb.lex = ss.lex;
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.rectangular_selection = ss.rectangular_selection;
    fb.history = fb.history.push_back(ss);
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer redo(file_buffer fb, const env_settings& s)
  {
  if (fb.undo_redo_index + 1 < fb.history.size())
    {
    ++fb.undo_redo_index;
    snapshot ss = fb.history[(uint32_t)fb.undo_redo_index];
    fb.content = ss.content;
    fb.lex = ss.lex;
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.rectangular_selection = ss.rectangular_selection;
    fb.history = fb.history.push_back(ss);
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer select_all(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  position pos;
  pos.row = 0;
  pos.col = 0;
  fb.start_selection = pos;
  fb.pos.row = fb.content.size() - 1;
  fb.pos.col = fb.content.back().size();
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_left(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  position actual = get_actual_position(fb);
  if (actual.col == 0)
    {
    if (fb.pos.row > 0)
      {
      --fb.pos.row;
      fb.pos.col = fb.content[fb.pos.row].size();
      }
    }
  else
    fb.pos.col = actual.col - 1;
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_right(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  if (fb.pos.col < (int64_t)fb.content[fb.pos.row].size() - 1)
    ++fb.pos.col;
  else if ((fb.pos.row + 1) < fb.content.size())
    {
    fb.pos.col = 0;
    ++fb.pos.row;
    }
  else if (fb.pos.col == (int64_t)fb.content[fb.pos.row].size() - 1)
    {
    ++fb.pos.col;
    assert(fb.pos.row == fb.content.size() - 1);
    assert(fb.pos.col == fb.content.back().size());
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_up(file_buffer fb, const env_settings& s)
  {
  if (fb.pos.row > 0)
    {
    --fb.pos.row;
    int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
    fb.pos.col = new_col;
    }
  return fb;
  }

file_buffer move_down(file_buffer fb, const env_settings& s)
  {
  if ((fb.pos.row + 1) < fb.content.size())
    {
    ++fb.pos.row;
    int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
    fb.pos.col = new_col;
    }
  return fb;
  }

file_buffer move_page_up(file_buffer fb, int64_t rows, const env_settings& s)
  {
  fb.pos.row -= rows;
  if (fb.pos.row < 0)
    fb.pos.row = 0;
  int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
  fb.pos.col = new_col;
  return fb;
  }

file_buffer move_page_down(file_buffer fb, int64_t rows, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  fb.pos.row += rows;
  if (fb.pos.row >= fb.content.size())
    fb.pos.row = fb.content.size() - 1;
  int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
  fb.pos.col = new_col;
  return fb;
  }

file_buffer move_home(file_buffer fb, const env_settings& s)
  {
  auto indented_pos = get_indentation_at_row(fb, fb.pos.row); 
  fb.pos.col = (fb.pos.col <= indented_pos.col) ? 0 : indented_pos.col;
  fb.xpos = fb.pos.col == 0 ? 0 : get_x_position(fb, s);
  return fb;
  }

file_buffer move_end(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;

  bool selecting = fb.start_selection != std::nullopt;

  fb.pos.col = (int64_t)fb.content[fb.pos.row].size() - 1;
  if (selecting)
    --fb.pos.col; // don't take the last \n
  if (fb.pos.col < 0)
    fb.pos.col = 0;

  if ((fb.pos.row + 1) == fb.content.size()) // last line
    {
    if (fb.content.back().empty())
      fb.pos.col = 0;
    else if (fb.content.back().back() != L'\n')
      ++fb.pos.col;
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

std::string to_string(text txt)
  {
  std::string out;
  for (auto ln : txt)
    {
    std::string str;
    auto it = ln.begin();
    auto it_end = ln.end();
    str.reserve(std::distance(it, it_end));
    utf8::utf16to8(it, it_end, std::back_inserter(str));
    out.append(str);
    }
  return out;
  }

std::wstring to_wstring(text txt)
  {
  std::wstring out;
  for (auto ln : txt)
    {
    std::wstring str;
    auto it = ln.begin();
    auto it_end = ln.end();
    str.reserve(std::distance(it, it_end));
    for (; it != it_end; ++it)
      str.push_back(*it);
    out.append(str);
    }
  return out;
  }

std::string buffer_to_string(file_buffer fb)
  {
  return to_string(fb.content);
  }

position get_last_position(text txt)
  {
  if (txt.empty())
    return position(0, 0);
  int64_t row = txt.size() - 1;
  if (txt.back().empty())
    return position(row, 0);
  if (txt.back().back() != L'\n')
    return position(row, txt.back().size());
  return position(row, txt.back().size() - 1);
  }

position get_last_position(file_buffer fb)
  {
  return get_last_position(fb.content);
  }

file_buffer update_position(file_buffer fb, position pos, const env_settings& s)
  {
  fb.pos = pos;
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

text to_text(std::wstring wtxt)
  {
  text out;
  auto transout = out.transient();
  while (!wtxt.empty())
    {
    auto endline_position = wtxt.find_first_of(L'\n');
    if (endline_position != std::wstring::npos)
      ++endline_position;
    std::wstring wline = wtxt.substr(0, endline_position);
    wtxt.erase(0, endline_position);

    line input;
    auto trans = input.transient();
    for (auto ch : wline)
      trans.push_back(ch);
    transout.push_back(trans.persistent());
    }
  return transout.persistent();
  }

text to_text(const std::string& txt)
  {
  return to_text(jtk::convert_string_to_wstring(txt));
  }

position get_next_position(text txt, position pos)
  {
  if (pos.row >= txt.size())
    return pos;
  ++pos.col;
  if (pos.col >= txt[pos.row].size())
    {
    ++pos.row;
    pos.col = 0;
    }
  if (pos.row >= txt.size())
    {
    return get_last_position(txt);
    }
  return pos;
  }

position get_next_position(file_buffer fb, position pos)
  {
  return get_next_position(fb.content, pos);
  }


position get_previous_position(text txt, position pos)
  {
  if (pos.row < 0)
    return pos;
  --pos.col;
  while (pos.col < 0 && pos.row >= 0)
    {
    --pos.row;
    if (pos.row >= 0)
      pos.col = (int64_t)txt[pos.row].size() - 1;
    }
  return pos;
  }

position get_previous_position(file_buffer fb, position pos)
  {
  return get_previous_position(fb.content, pos);
  }

file_buffer find_text(file_buffer fb, text txt)
  {
  if (txt.empty())
    return fb;
  if (fb.content.empty())
    return fb;
  fb.rectangular_selection = false;
  position lastpos = get_last_position(fb);
  position pos = fb.pos;
  position text_pos(0, 0);
  position lasttext = get_last_position(txt);
  position first_encounter;
  if (has_selection(fb) && fb.start_selection > pos)
    pos = *fb.start_selection;
  if (pos == lastpos)
    pos.col = pos.row = 0;
  pos = get_actual_position(fb, pos);
  wchar_t current_text_char = txt[text_pos.row][text_pos.col];
  wchar_t first_text_char = txt[text_pos.row][text_pos.col];
  while (pos != lastpos)
    {
    wchar_t current_char = fb.content[pos.row][pos.col];
    if (current_char == current_text_char)
      {
      if (text_pos.col == 0 && text_pos.row == 0)
        first_encounter = pos;
      text_pos = get_next_position(txt, text_pos);
      if (text_pos == lasttext)
        {
        fb.start_selection = first_encounter;
        fb.pos = pos;
        return fb;
        }
      current_text_char = txt[text_pos.row][text_pos.col];
      }
    else
      {
      current_text_char = first_text_char;
      text_pos = position(0, 0);
      }
    pos = get_next_position(fb, pos);
    }
  fb.pos = lastpos;
  fb.start_selection = std::nullopt;
  return fb;
  }

file_buffer find_text(file_buffer fb, const std::wstring& wtxt)
  {
  return find_text(fb, to_text(wtxt));
  }

file_buffer find_text(file_buffer fb, const std::string& txt)
  {
  return find_text(fb, jtk::convert_string_to_wstring(txt));
  }

std::wstring read_next_word(line::const_iterator it, line::const_iterator it_end)
  {
  std::wstring out;
  while (it != it_end && *it != L' ' && *it != L',' && *it != L'(' && *it != L'{' && *it != L')' && *it != L'}' && *it != L'[' && *it != L']'&& *it != L'\n' && *it != L'\t' && *it != L'\r')
    {
    out.push_back(*it);
    ++it;
    }
  return out;
  }

namespace
  {

  bool _is_next_word(line::const_iterator it, line::const_iterator it_end, const std::string& word)
    {
    if (word.empty())
      return false;
    if (*it == (wchar_t)word[0])
      {
      ++it;
      int i = 1;
      while (i < word.length() && (it != it_end) && (*it == (wchar_t)word[i]))
        {
        ++it; ++i;
        }
      return (i == word.length());
      }
    return false;
    }

  uint8_t _get_end_of_line_lexer_status(file_buffer fb, int64_t row, uint8_t status_at_begin_of_line)
    {
    uint8_t current_status = status_at_begin_of_line;
    line ln = fb.content[row];
    auto it = ln.begin();
    auto prev_it = it;
    auto prevprev_it = prev_it;
    auto it_end = ln.end();
    bool inside_single_line_comment = false;
    bool inside_single_line_string = false;
    bool inside_quotes = false;
    for (; it != it_end; ++it)
      {
      if (inside_single_line_comment)
        break;
      if (current_status == lexer_normal)
        {
        if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.multiline_begin))
          {
          current_status = lexer_inside_multiline_comment;
          it += fb.syntax.multiline_begin.length()-1;
          }
        else if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.multistring_begin))
          {
          current_status = lexer_inside_multiline_string;
          it += fb.syntax.multistring_begin.length()-1;
          }
        else if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.single_line))
          {
          inside_single_line_comment = true;
          }
        else if (!inside_quotes && *it == L'"' && (*prev_it != L'\\' || *prevprev_it == L'\\'))
          {
          inside_single_line_string = !inside_single_line_string;
          }
        else if (fb.syntax.uses_quotes_for_chars && !inside_single_line_string && *it == L'\'' && (*prev_it != L'\\' || *prevprev_it == L'\\'))
          {
          inside_quotes = !inside_quotes;
          }
        }
      else if (current_status == lexer_inside_multiline_comment)
        {
        if (_is_next_word(it, it_end, fb.syntax.multiline_end))
          {
          current_status = lexer_normal;
          it += fb.syntax.multiline_end.length()-1;
          }
        }
      else if (current_status == lexer_inside_multiline_string)
        {
        if (_is_next_word(it, it_end, fb.syntax.multistring_end))
          {
          current_status = lexer_normal;
          it += fb.syntax.multistring_end.length()-1;
          }
        }
      prevprev_it = prev_it;
      prev_it = it;
      }  
    return current_status;
    }

  }

uint8_t get_end_of_line_lexer_status(file_buffer fb, int64_t row)
  {
  return _get_end_of_line_lexer_status(fb, row, fb.lex[row]);
  }

file_buffer init_lexer_status(file_buffer fb)
  {
  if (fb.content.empty())
    return fb;
  lexer_status ls;
  auto trans = ls.transient();

  trans.push_back(lexer_normal);
  for (int64_t row = 1; row < fb.content.size(); ++row)
    {
    trans.push_back(_get_end_of_line_lexer_status(fb, row - 1, trans.back()));
    }

  fb.lex = trans.persistent();
  return fb;
  }

file_buffer update_lexer_status(file_buffer fb, int64_t row)
  {
  assert(!fb.content.empty());
  if (fb.syntax.single_line.empty() && fb.syntax.multiline_begin.empty() && fb.syntax.multistring_begin.empty())
    return fb;

  auto trans = fb.lex.transient();
  /*
  
  while (trans.size() < fb.content.size())
    trans.push_back(lexer_normal);
  while (trans.size() > fb.content.size())
    trans.pop_back();
    */
  assert(trans.size() == fb.content.size());

  for (int64_t r = row; r < fb.content.size()-1; ++r)
    {
    uint8_t eol = _get_end_of_line_lexer_status(fb, r, trans[r]);
    if (eol == trans[r + 1])
      break;
    trans.set(r + 1, eol);
    }

  fb.lex = trans.persistent();
  return fb;
  }

file_buffer update_lexer_status(file_buffer fb, int64_t from_row, int64_t to_row)
  {
  assert(!fb.content.empty());
  if (fb.syntax.single_line.empty() && fb.syntax.multiline_begin.empty() && fb.syntax.multistring_begin.empty())
    return fb;

  auto trans = fb.lex.transient();
  /*  while (trans.size() < fb.content.size())
    trans.push_back(lexer_normal);
  while (trans.size() > fb.content.size())
    trans.pop_back();
    */
  assert(trans.size() == fb.content.size());

  while (to_row >= fb.content.size() - 1)
    --to_row;

  int64_t r = from_row;
  for (; r <= to_row; ++r)
    {
    uint8_t eol = _get_end_of_line_lexer_status(fb, r, trans[r]);
    trans.set(r + 1, eol);
    }
  for (; r < fb.content.size() - 1; ++r)
    {
    uint8_t eol = _get_end_of_line_lexer_status(fb, r, trans[r]);
    if (eol == trans[r + 1])
      break;
    trans.set(r + 1, eol);
    }

  fb.lex = trans.persistent();
  return fb;
  }

std::vector<std::pair<int64_t, text_type>> get_text_type(file_buffer fb, int64_t row)
  {
  std::vector<std::pair<int64_t, text_type>> out;
  out.emplace_back((int64_t)0, (text_type)fb.lex[row]);
  //if (fb.syntax.single_line.empty() && fb.syntax.multiline_begin.empty() && fb.syntax.multistring_begin.empty())
  //  return out;
  if (!fb.syntax.should_highlight)
    return out;

  uint8_t current_status = fb.lex[row];
  line ln = fb.content[row];
  auto it = ln.begin();
  auto prev_it = it;
  auto prevprev_it = prev_it;
  auto it_end = ln.end();
  bool inside_single_line_comment = false;
  bool inside_single_line_string = false;
  bool inside_quotes = false;
  int64_t col = 0;
  for (; it != it_end; ++it, ++col)
    {
    if (inside_single_line_comment)
      break;
    if (current_status == lexer_normal)
      {
      if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.multiline_begin))
        {
        out.emplace_back((int64_t)col, tt_comment);
        current_status = lexer_inside_multiline_comment;
        it += fb.syntax.multiline_begin.length()-1;
        col += fb.syntax.multiline_begin.length()-1;
        }
      else if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.multistring_begin))
        {
        out.emplace_back((int64_t)col, tt_string);
        current_status = lexer_inside_multiline_string;
        it += fb.syntax.multistring_begin.length()-1;
        col += fb.syntax.multistring_begin.length()-1;
        }
      else if (!inside_single_line_string && !inside_quotes && _is_next_word(it, it_end, fb.syntax.single_line))
        {
        inside_single_line_comment = true;
        out.emplace_back((int64_t)col, tt_comment);
        }
      else if (!inside_quotes && *it == L'"' && (*prev_it != L'\\' || *prevprev_it == L'\\'))
        {                
        inside_single_line_string = !inside_single_line_string;
        if (inside_single_line_string)
          out.emplace_back((int64_t)col, tt_string);
        else
          out.emplace_back((int64_t)col+1, tt_normal);
        }
      else if (fb.syntax.uses_quotes_for_chars && !inside_single_line_string && *it == L'\'' && (*prev_it != L'\\' || *prevprev_it == L'\\'))
        {
        inside_quotes = !inside_quotes;
        if (inside_quotes)
          out.emplace_back((int64_t)col, tt_string);
        else
          out.emplace_back((int64_t)col + 1, tt_normal);
        }
      }
    else if (current_status == lexer_inside_multiline_comment)
      {
      if (_is_next_word(it, it_end, fb.syntax.multiline_end))
        {
        current_status = lexer_normal;
        it += fb.syntax.multiline_end.length()-1;
        col += fb.syntax.multiline_end.length()-1;
        out.emplace_back((int64_t)col+1, tt_normal);
        }
      }
    else if (current_status == lexer_inside_multiline_string)
      {
      if (_is_next_word(it, it_end, fb.syntax.multistring_end))
        {
        current_status = lexer_normal;
        it += fb.syntax.multistring_end.length()-1;
        col += fb.syntax.multistring_end.length()-1;
        out.emplace_back((int64_t)col+1, tt_normal);
        }
      }
    prevprev_it = prev_it;
    prev_it = it;
    }  

  std::reverse(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end(), [](const std::pair<int64_t, text_type>& left, const std::pair<int64_t, text_type>& right)
    {
    return left.first == right.first;
    })
  , out.end());

  return out;
  }

bool valid_position(text txt, position pos)
  {
  if (pos.row < 0 || pos.col < 0)
    return false;
  if (pos.row >= txt.size())
    return false;
  if (pos.col >= txt[pos.row].size())
    return false;
  return true;
  }

bool valid_position(file_buffer fb, position pos)
  {
  return valid_position(fb.content, pos);
  }

position find_corresponding_token(file_buffer fb, position tokenpos, int64_t minrow, int64_t maxrow)
  {  
  if (!valid_position(fb, tokenpos))
    return position(-1, -1);
  wchar_t token = fb.content[tokenpos.row][tokenpos.col];
  wchar_t corresponding_token = 0;
  bool forward = true;
  switch (token)
    {
    case L'(':  corresponding_token = L')'; break;
    case L')':  corresponding_token = L'('; forward = false;  break;
    case L'{':  corresponding_token = L'}'; break;
    case L'}':  corresponding_token = L'{'; forward = false;  break;
    case L'[':  corresponding_token = L']'; break;
    case L']':  corresponding_token = L'['; forward = false;  break;
    }
  if (corresponding_token == 0)
    return position(-1, -1);
  int count = 0;
  if (forward)
    {
    auto last_pos = get_last_position(fb);      
    if (tokenpos == last_pos)
      return position(-1, -1);
    position current = get_next_position(fb, tokenpos);
    while (current.row <= maxrow && valid_position(fb, current))
      {
      wchar_t current_token = fb.content[current.row][current.col];
      if (current_token == token)
        ++count;
      if (current_token == corresponding_token)
        {
        if (count == 0)
          return current;
        else
          --count;
        }
      current = get_next_position(fb, current);
      }
    }
  else
    {
    auto first_pos = position(0, 0);
    if (tokenpos == first_pos)
      return position(-1, -1);
    position current = get_previous_position(fb, tokenpos);
    while (current.row >= minrow && valid_position(fb, current))
      {
      wchar_t current_token = fb.content[current.row][current.col];
      if (current_token == token)
        ++count;
      if (current_token == corresponding_token)
        {
        if (count == 0)
          return current;
        else
          --count;
        }
      current = get_previous_position(fb, current);
      }
    }
  return position(-1, -1);
  }

position get_indentation_at_row(file_buffer fb, int64_t row)
  {
  position out(row, 0);
  auto ln = fb.content[row];
  auto maxcol = ln.size();
  while (out.col < maxcol && (ln[out.col] == L' ' || ln[out.col] == L'\t'))
    ++out.col;
  return out;
  }

std::string get_row_indentation_pattern(file_buffer fb, position pos)
  {
  std::string out;
  if (pos.row >= fb.content.size())
    return out;
  auto ln = fb.content[pos.row];
  for (int64_t c = pos.col; c < ln.size(); ++c)
    {
    if (ln[c] != ' ' && ln[c] != '\n')
      return out;
    }
  int64_t col = 0;
  while (col < pos.col && col < ln.size() && (ln[col] == L' ' || ln[col] == L'\t'))
    {
    out.push_back((char)ln[col]);
    ++col;
    }    
  return out;
  }
