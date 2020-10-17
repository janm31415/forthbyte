#include "utils.h"

#include <jtk/file_utils.h>
#include <algorithm>


namespace
  {
  std::vector<uint16_t> build_ascii_to_utf16_vector()
    {
    std::vector<uint16_t> vec(256, 0);
    for (int i = 0; i < 128; ++i)
      vec[i] = (uint16_t)i;

    vec[128] = 0x00C7;
    vec[129] = 0x00FC;
    vec[130] = 0x00E9;
    vec[131] = 0x00E2;
    vec[132] = 0x00E4;
    vec[133] = 0x00E0;
    vec[134] = 0x00E5;
    vec[135] = 0x00E7;
    vec[136] = 0x00EA;
    vec[137] = 0x00EB;
    vec[138] = 0x00E8;
    vec[139] = 0x00EF;
    vec[140] = 0x00EE;
    vec[141] = 0x00EC;
    vec[142] = 0x00C4;
    vec[143] = 0x00C5;
    vec[144] = 0x00C9;
    vec[145] = 0x00E6;
    vec[146] = 0x00C6;
    vec[147] = 0x00F4;
    vec[148] = 0x00F6;
    vec[149] = 0x00F2;
    vec[150] = 0x00FB;
    vec[151] = 0x00F9;
    vec[152] = 0x00FF;
    vec[153] = 0x00D6;
    vec[154] = 0x00DC;
    vec[155] = 0x00A2;
    vec[156] = 0x00A3;
    vec[157] = 0x00A5;
    vec[158] = 0x20A7;
    vec[159] = 0x0192;
    vec[160] = 0x00E1;
    vec[161] = 0x00ED;
    vec[162] = 0x00F3;
    vec[163] = 0x00FA;
    vec[164] = 0x00F1;
    vec[165] = 0x00D1;
    vec[166] = 0x00AA;
    vec[167] = 0x00BA;
    vec[168] = 0x00BF;
    vec[169] = 0x2310;
    vec[170] = 0x00AC;
    vec[171] = 0x00BD;
    vec[172] = 0x00BC;
    vec[173] = 0x00A1;
    vec[174] = 0x00AB;
    vec[175] = 0x00BB;
    vec[176] = 0x2591;
    vec[177] = 0x2592;
    vec[178] = 0x2593;
    vec[179] = 0x2502;
    vec[180] = 0x2524;
    vec[181] = 0x2561;
    vec[182] = 0x2562;
    vec[183] = 0x2556;
    vec[184] = 0x2555;
    vec[185] = 0x2563;
    vec[186] = 0x2551;
    vec[187] = 0x2557;
    vec[188] = 0x255D;
    vec[189] = 0x255C;
    vec[190] = 0x255B;
    vec[191] = 0x2510;
    vec[192] = 0x2514;
    vec[193] = 0x2534;
    vec[194] = 0x252C;
    vec[195] = 0x251C;
    vec[196] = 0x2500;
    vec[197] = 0x253C;
    vec[198] = 0x255E;
    vec[199] = 0x255F;
    vec[200] = 0x255A;
    vec[201] = 0x2554;
    vec[202] = 0x2569;
    vec[203] = 0x2566;
    vec[204] = 0x2560;
    vec[205] = 0x2550;
    vec[206] = 0x256C;
    vec[207] = 0x2567;
    vec[208] = 0x2568;
    vec[209] = 0x2564;
    vec[210] = 0x2565;
    vec[211] = 0x2559;
    vec[212] = 0x2558;
    vec[213] = 0x2552;
    vec[214] = 0x2553;
    vec[215] = 0x256B;
    vec[216] = 0x256A;
    vec[217] = 0x2518;
    vec[218] = 0x250C;
    vec[219] = 0x2588;
    vec[220] = 0x2584;
    vec[221] = 0x258C;
    vec[222] = 0x2590;
    vec[223] = 0x2580;
    vec[224] = 0x03B1;
    vec[225] = 0x00DF;
    vec[226] = 0x0393;
    vec[227] = 0x03C0;
    vec[228] = 0x03A3;
    vec[229] = 0x03C3;
    vec[230] = 0x00B5;
    vec[231] = 0x03C4;
    vec[232] = 0x03A6;
    vec[233] = 0x0398;
    vec[234] = 0x03A9;
    vec[235] = 0x03B4;
    vec[236] = 0x221E;
    vec[237] = 0x03C6;
    vec[238] = 0x03B5;
    vec[239] = 0x2229;
    vec[240] = 0x2261;
    vec[241] = 0x00B1;
    vec[242] = 0x2265;
    vec[243] = 0x2264;
    vec[244] = 0x2320;
    vec[245] = 0x2321;
    vec[246] = 0x00F7;
    vec[247] = 0x2248;
    vec[248] = 0x00B0;
    vec[249] = 0x2219;
    vec[250] = 0x00B7;
    vec[251] = 0x221A;
    vec[252] = 0x207F;
    vec[253] = 0x00B2;
    vec[254] = 0x25A0;
    vec[255] = 0x00A0;
    return vec;
    }

  std::vector<std::wstring> split_wstring_by_wchar(std::wstring str, wchar_t separator)
    {
    std::vector<std::wstring> out;
    while (!str.empty())
      {
      auto it = str.find_first_of(separator);
      if (it == std::wstring::npos)
        {
        out.push_back(str);
        str.clear();
        }
      else
        {
        out.push_back(str.substr(0, it));
        str.erase(0, it + 1);
        }
      }
    for (auto& p : out)
      {
      std::replace(p.begin(), p.end(), L'\\', L'/');
      if (p.back() != L'/')
        p.push_back(L'/');
      }
    return out;
    }
    
  std::string getenv(const std::string& name)
    {
  #ifdef _WIN32
    std::wstring ws = jtk::convert_string_to_wstring(name);
    wchar_t* path = _wgetenv(ws.c_str());
    if (!path)
      return nullptr;
    std::wstring wresult(path);
    std::string out = jtk::convert_wstring_to_string(wresult);
  #else
    std::string out(getenv(name.c_str()));
  #endif
    return out;
    }
  }

uint16_t ascii_to_utf16(unsigned char ch)
  {
  static std::vector<uint16_t> m = build_ascii_to_utf16_vector();
  return m[ch];
  }

std::string get_file_path(const std::string& filename)
  {
  if (!jtk::get_folder(filename).empty())
    {
    if (jtk::file_exists(filename))
      return filename;
    if (jtk::is_directory(jtk::get_folder(filename)))
      return filename;
    }

#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  GetCurrentDirectoryW(MAX_PATH, buf);
  std::wstring wbuf(buf);
  std::replace(wbuf.begin(), wbuf.end(), '\\', '/'); // replace all '\\' by '/'
  std::string dir = jtk::convert_wstring_to_string(wbuf);
#else
  char buf[PATH_MAX];
  getcwd(buf, sizeof(buf));
  std::string dir(buf);
#endif
  auto possible_executables = jtk::get_files_from_directory(dir, false);
  for (const auto& path : possible_executables)
    {
    auto f = jtk::get_filename(path);
    if (f == filename || jtk::remove_extension(f) == filename)
      {
      return path;
      }
    }

  auto executable_path = jtk::get_folder(jtk::get_executable_path());
  possible_executables = jtk::get_files_from_directory(executable_path, false);
  for (const auto& path : possible_executables)
    {
    auto f = jtk::get_filename(path);
    if (f == filename || jtk::remove_extension(f) == filename)
      {
      return path;
      }
    }

  std::string path = getenv(std::string("PATH"));

#if defined(__APPLE__)
  path.append(std::string(":/usr/local/bin"));
#endif
    
#ifdef _WIN32
  auto path_list = split_wstring_by_wchar(jtk::convert_string_to_wstring(path), L';');
#else
  auto path_list = split_wstring_by_wchar(jtk::convert_string_to_wstring(path), L':');
#endif
  
  for (const auto& folder_in_path : path_list)
    {
    auto possible_files = jtk::get_files_from_directory(jtk::convert_wstring_to_string(folder_in_path), false);
    for (const auto& possible_file : possible_files)
      {
      auto f = jtk::get_filename(possible_file);
      if (f == filename || jtk::remove_extension(f) == filename)
        {
        return possible_file;
        }
      }
    }
  return "";
  }
