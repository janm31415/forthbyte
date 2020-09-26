#pragma once

#include <stdint.h>
#include <vector>

#include <chrono>

class compiler;

class music
  {
  public:
    music();
    ~music();

    void fill_buffer(compiler& c);

    void play(compiler& c);

    void stop();

    void reset_timer();

    void swap_buffers();

    void init_buffer_for_playing(compiler& c);

    uint64_t get_timer() const;

    void set_sample_rate(uint32_t sample_rate);

    uint32_t get_sample_rate() const { return _sample_rate; }

    uint64_t get_estimated_timer_based_on_clock() const;

    std::chrono::high_resolution_clock::time_point get_starting_point_clock();

    void set_float() { _float = true; }

    void set_byte() { _float = false; }

    bool is_float() const { return _float; }

    bool is_byte() const { return !_float; }

  private:
    uint32_t _sample_rate;
    uint32_t _samples_per_go;
    uint32_t _buffer_width;
    uint64_t _t;
    std::chrono::high_resolution_clock::time_point _start;

    std::vector<unsigned char> _buffer_to_play, _buffer_to_fill;
    bool _buffer_is_filled;
    bool _playing;
    bool _float;
  };