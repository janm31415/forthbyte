#pragma once

#include <stdint.h>

struct gimp_image
  {
  uint32_t  	 width;
  uint32_t  	 height;
  uint32_t  	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  uint8_t 	   pixel_data[64 * 64 * 4 + 1];
  };

extern struct gimp_image fbicon;