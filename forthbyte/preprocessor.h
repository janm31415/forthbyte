#pragma once

#include "buffer.h"
#include <stdint.h>

struct preprocess_settings
  {
  bool _float;
  uint64_t _sample_rate;
  };

preprocess_settings preprocess(text code);

