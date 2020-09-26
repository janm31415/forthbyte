#include "colors.h"

#include <curses.h>
#include <stdint.h>

namespace
  {
  short conv_rgb(int clr)
    {
    float frac = (float)clr / 255.f;
    return (short)(1000.f*frac);
    }

  struct rgb
    {
    rgb(uint32_t v) : r(v & 255), g((v >> 8) & 255), b((v >> 16) & 255)
      {
      }
    rgb(int red, int green, int blue) : r(red), g(green), b(blue) {}
    int r, g, b;
    };

  void init_color(short id, rgb value)
    {
    ::init_color(id, conv_rgb(value.r), conv_rgb(value.g), conv_rgb(value.b));
    }

  enum jed_colors
    {
    jed_editor_text = 16,
    jed_editor_bg = 17,
    jed_editor_tag = 18,
    jed_command_text = 19,
    jed_command_bg = 20,
    jed_command_tag = 21,
    jed_title_text = 22,
    jed_title_bg = 23,

    jed_editor_text_bold = 24,
    jed_editor_bg_bold = 25,
    jed_editor_tag_bold = 26,
    jed_command_text_bold = 27,
    jed_command_bg_bold = 28,
    jed_command_tag_bold = 29,
    jed_title_text_bold = 30,
    jed_title_bg_bold = 31,

    jed_comment = 32,
    jed_string = 33,
    jed_keyword = 34,
    jed_keyword_2 = 35
    };
  }

void init_colors()
  {
  uint32_t color_editor_text = 0xffc0c0c0;
  uint32_t color_editor_background = 0xff000000;
  uint32_t color_editor_tag = 0xfff18255;
  uint32_t color_editor_text_bold = 0xffffffff;
  uint32_t color_editor_background_bold = 0xff000000;
  uint32_t color_editor_tag_bold = 0xffff9b73;
  uint32_t color_command_text = 0xff000000;
  uint32_t color_command_background = 0xffc0c0c0;
  uint32_t color_command_tag = 0xfff18255;
  uint32_t color_titlebar_text = 0xff000000;
  uint32_t color_titlebar_background = 0xffffffff;
  uint32_t color_comment = 0xff64c385;
  uint32_t color_string = 0xff6464db;
  uint32_t color_keyword = 0xffff8080;
  uint32_t color_keyword_2 = 0xffffc0c0;

  init_color(jed_title_text, rgb(color_titlebar_text));
  init_color(jed_title_bg, rgb(color_titlebar_background));
  init_color(jed_title_text_bold, rgb(color_titlebar_text));
  init_color(jed_title_bg_bold, rgb(color_titlebar_background));

  init_color(jed_editor_text, rgb(color_editor_text));
  init_color(jed_editor_bg, rgb(color_editor_background));
  init_color(jed_editor_tag, rgb(color_editor_tag));

  init_color(jed_command_text, rgb(color_command_text));
  init_color(jed_command_bg, rgb(color_command_background));
  init_color(jed_command_tag, rgb(color_command_tag));

  init_color(jed_editor_text_bold, rgb(color_editor_text_bold));
  init_color(jed_editor_bg_bold, rgb(color_editor_background_bold));
  init_color(jed_editor_tag_bold, rgb(color_editor_tag_bold));

  init_color(jed_command_text_bold, rgb(color_command_text));
  init_color(jed_command_bg_bold, rgb(color_command_background));
  init_color(jed_command_tag_bold, rgb(color_command_tag));

  init_pair(default_color, jed_editor_text, jed_editor_bg);
  //init_pair(command_color, jed_command_text, jed_command_bg);
  init_pair(multiline_tag, jed_editor_tag, jed_editor_bg);
  //init_pair(multiline_tag_command, jed_command_tag, jed_command_bg);
  //
  //init_pair(scroll_bar_b_editor, jed_command_bg, jed_editor_bg);
  //init_pair(scroll_bar_f_editor, jed_editor_tag, jed_editor_bg);
  //
  //init_pair(title_bar, jed_title_text, jed_title_bg);

  init_color(jed_comment, rgb(color_comment));
  init_color(jed_string, rgb(color_string));
  init_color(jed_keyword, rgb(color_keyword));
  init_color(jed_keyword_2, rgb(color_keyword_2));

  init_pair(comment_color, jed_comment, jed_editor_bg);
  init_pair(string_color, jed_string, jed_editor_bg);
  init_pair(keyword_color, jed_keyword, jed_editor_bg);
  init_pair(keyword_2_color, jed_keyword_2, jed_editor_bg);


  }