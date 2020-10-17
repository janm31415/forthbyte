#pragma once

#include <string>
#include <stdint.h>

uint16_t ascii_to_utf16(unsigned char ch);

std::string get_file_path(const std::string& filename);
