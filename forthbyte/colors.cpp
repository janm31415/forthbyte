#include "colors.h"

#include <curses.h>

namespace
  {
  short conv_rgb(int clr)
    {
    float frac = (float)clr / 255.f;
    return (short)(1000.f*frac);
    }

  struct rgb
    {
    rgb(int red, int green, int blue) : r(red), g(green), b(blue) {}
    int r, g, b;
    };

  void init_color(short id, rgb value)
    {
    ::init_color(id, conv_rgb(value.r), conv_rgb(value.g), conv_rgb(value.b));
    }
  }

void init_colors()
  {
  rgb text_color(192, 192, 192);
  rgb bg_color(0, 0, 0);
  rgb multiline_tag_color(85, 130, 241);

  rgb text_color_bold(255, 255, 255);
  rgb bg_color_bold(0, 0, 0);
  rgb multiline_tag_color_bold(115, 155, 255);
  
  init_color(16, text_color);
  init_color(17, bg_color);
  init_color(18, multiline_tag_color);

  //A_BOLD is equivalent to OR operation with value 8
  init_color(24, text_color_bold);
  init_color(25, bg_color_bold);
  init_color(26, multiline_tag_color_bold);

  init_pair(default_color, 16, 17);
  init_pair(multiline_tag, 18, 17);
  }