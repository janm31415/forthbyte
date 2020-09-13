#include "keyboard.h"


keyboard_handler keyb;


keyboard_data::keyboard_data()
  {
  selecting = false; // is true when SHIFT is pressed for selection
  last_selection_was_upward = false;
  }

keyboard_data keyb_data;