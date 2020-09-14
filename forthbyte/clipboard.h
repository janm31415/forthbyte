#pragma once

#include <string>

void copy_to_windows_clipboard(const std::string& text);

std::string get_text_from_windows_clipboard();