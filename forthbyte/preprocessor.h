#pragma once

#include "buffer.h"
#include <stdint.h>
#include <string>
#include <vector>

struct preprocess_settings
  {
  bool _float;
  uint64_t _sample_rate;
  std::vector<std::string> init_memory;
  };

preprocess_settings preprocess(text code);

