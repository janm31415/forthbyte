#include "buffer.h"

#include <fstream>

#include <jtk/file_utils.h>

file_buffer make_empty_buffer()
  {
  file_buffer fb;
  fb.pos.row = fb.pos.col = 0;
  fb.start_selection = std::nullopt;
  fb.modification_mask = 0;
  fb.undo_redo_index = 0;
  return fb;
  }

file_buffer read_from_file(const std::string& filename)
  {
  using namespace jtk;
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
    std::string file_line;
    while (std::getline(f, file_line))
      {
      auto trans = line().transient();
      auto it = file_line.begin();
      auto it_end = file_line.end();
      utf8::utf8to16(it, it_end, std::back_inserter(trans));
      if (!f.eof())
        trans.push_back('\n');
      trans_lines.push_back(trans.persistent());
      }
    fb.content = trans_lines.persistent();
    f.close();
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

position get_actual_position(file_buffer fb)
  {
  position out = fb.pos;
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

bool has_selection(file_buffer fb)
  {
  return (fb.start_selection && (*fb.start_selection != fb.pos));
  }

file_buffer start_selection(file_buffer fb)
  {
  fb.start_selection = get_actual_position(fb);
  return fb;
  }

file_buffer clear_selection(file_buffer fb)
  {
  fb.start_selection = std::nullopt;
  return fb;
  }

file_buffer push_undo(file_buffer fb)
  {
  snapshot ss;
  ss.content = fb.content;
  ss.pos = fb.pos;
  ss.start_selection = fb.start_selection;
  ss.modification_mask = fb.modification_mask;
  fb.history = fb.history.push_back(ss);
  fb.undo_redo_index = fb.history.size();
  return fb;
  }

file_buffer insert(file_buffer fb, const std::string& txt, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  if (has_selection(fb))
    fb = erase(fb, false);

  fb.start_selection = std::nullopt;

  fb.modification_mask |= 1;

  std::wstring wtxt = jtk::convert_string_to_wstring(txt);
  auto pos = get_actual_position(fb);

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
      fb.pos.col = fb.content.back().size();
      if (input.back() == L'\n')
        {
        fb.content = fb.content.push_back(line());
        ++fb.pos.row;
        fb.pos.col = 0;
        pos = fb.pos;
        }
      }
    else if (input.back() == L'\n')
      {
      auto first_part = fb.content[pos.row].take(pos.col);
      auto second_part = fb.content[pos.row].drop(pos.col);
      fb.content = fb.content.set(pos.row, first_part.insert(pos.col, input));
      fb.content = fb.content.insert(pos.row + 1, second_part);
      ++fb.pos.row;
      fb.pos.col = 0;
      pos = fb.pos;
      }
    else
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].insert(pos.col, input));
      fb.pos.col += input.size();
      }
    }

  return fb;
  }

file_buffer insert(file_buffer fb, text txt, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  if (has_selection(fb))
    fb = erase(fb, false);

  fb.start_selection = std::nullopt;

  fb.modification_mask |= 1;

  if (fb.content.empty())
    {
    fb.content = txt;
    return fb;
    }

  if (txt.empty())
    return fb;

  auto pos = get_actual_position(fb);

  auto ln1 = fb.content[pos.row].take(pos.col);
  auto ln2 = fb.content[pos.row].drop(pos.col);

  if (!txt.back().empty() && txt.back().back() == L'\n')
    {
    txt = txt.push_back(line());
    }

  if (txt.size() == 1)
    {
    auto new_line = ln1 + txt[0] + ln2;
    fb.content = fb.content.set(pos.row, new_line);
    fb.pos.col = ln1.size() + txt[0].size();
    }
  else
    {
    fb.pos.col = txt.back().size();
    fb.pos.row += txt.size() - 1;
    ln1 = ln1 + txt[0];
    ln2 = txt.back() + ln2;
    txt = txt.set(0, ln1);
    txt = txt.set(txt.size() - 1, ln2);
    fb.content = fb.content.take(pos.row) + txt + fb.content.drop(pos.row + 1);
    }

  return fb;
  }

file_buffer erase(file_buffer fb, bool save_undo)
  {
  if (fb.content.empty())
    return fb;

  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    auto pos = get_actual_position(fb);
    if (pos.col > 0)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col - 1));
      --fb.pos.col;
      }
    else if (pos.row > 0)
      {
      fb.pos.col = (int64_t)fb.content[pos.row - 1].size() - 1;
      auto l = fb.content[pos.row - 1].pop_back() + fb.content[pos.row];
      fb.content = fb.content.erase(pos.row).set(pos.row - 1, l);
      --fb.pos.row;
      }
    }
  else
    {
    fb.start_selection = std::nullopt;
    auto p1 = get_actual_position(fb);
    auto p2 = *fb.start_selection;
    if (p2 < p1)
      std::swap(p1, p2);
    if (p1.row == p2.row)
      {
      fb.content = fb.content.set(p1.row, fb.content[p1.row].erase(p1.col, p2.col));
      fb.pos.col = p1.col;
      fb.pos.row = p1.row;
      fb = erase_right(fb, false);
      }
    else
      {
      bool remove_line = false;
      int64_t tgt = p2.col + 1;
      if (tgt > fb.content[p2.row].size() - 1)
        remove_line = true;
      fb.content = fb.content.set(p2.row, fb.content[p2.row].erase(0, tgt));
      fb.content = fb.content.erase(p1.row + 1, remove_line ? p2.row + 1 : p2.row);
      fb.content = fb.content.set(p1.row, fb.content[p1.row].erase(p1.col, fb.content[p1.row].size() - 1));
      fb.pos.col = p1.col;
      fb.pos.row = p1.row;
      fb = erase_right(fb, false);
      }
    }
  return fb;
  }

file_buffer erase_right(file_buffer fb, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    if (fb.content.empty())
      return fb;
    auto pos = get_actual_position(fb);
    if (pos.col < fb.content[pos.row].size() - 1)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col));
      }
    else if (pos.row < fb.content.size() - 1)
      {
      auto l = fb.content[pos.row].pop_back() + fb.content[pos.row + 1];
      fb.content = fb.content.erase(pos.row + 1).set(pos.row, l);
      }
    return fb;
    }
  else
    return erase(fb, false);
  }

text get_selection(file_buffer fb)
  {
  auto p1 = get_actual_position(fb);
  if (!has_selection(fb))
    {
    return text().push_back(line().push_back(fb.content[p1.row][p1.col]));
    //return text();
    }
  auto p2 = *fb.start_selection;
  if (p2 < p1)
    std::swap(p1, p2);

  if (p1.row == p2.row)
    {
    line ln = fb.content[p1.row].slice(p1.col, p2.col + 1);
    text t;
    t = t.push_back(ln);
    return t;
    }

  text out;
  auto trans = out.transient();

  line ln1 = fb.content[p1.row].drop(p1.col);
  trans.push_back(ln1);
  for (int64_t r = p1.row + 1; r < p2.row; ++r)
    trans.push_back(fb.content[r]);
  line ln2 = fb.content[p2.row].take(p2.col + 1);
  trans.push_back(ln2);

  out = trans.persistent();
  return out;
  }

file_buffer undo(file_buffer fb)
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
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.history = fb.history.push_back(ss);
    }
  return fb;
  }

file_buffer redo(file_buffer fb)
  {
  if (fb.undo_redo_index + 1 < fb.history.size())
    {
    ++fb.undo_redo_index;
    snapshot ss = fb.history[(uint32_t)fb.undo_redo_index];
    fb.content = ss.content;
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.history = fb.history.push_back(ss);
    }
  return fb;
  }

file_buffer select_all(file_buffer fb)
  {
  if (fb.content.empty())
    return fb;
  position pos;
  pos.row = 0;
  pos.col = 0;
  fb.start_selection = pos;
  fb.pos.row = fb.content.size() - 1;
  fb.pos.col = fb.content.back().size();
  return fb;
  }