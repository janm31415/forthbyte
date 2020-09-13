#include "buffer.h"

#include <fstream>

#include <jtk/file_utils.h>


file_buffer read_from_file(const std::string& filename)
  {
  using namespace jtk;
  file_buffer fb;
  fb.name = filename;
  fb.pos.row = fb.pos.col = 0;
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