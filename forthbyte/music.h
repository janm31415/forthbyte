#pragma once

#include <stdint.h>
#include <vector>
#include <fstream>

#include <chrono>

class compiler;

class music
  {
  public:
    music(compiler* c);
    ~music();

    void play();

    void stop();

    void reset_timer();

    uint64_t get_timer() const;

    void set_sample_rate(uint32_t sample_rate);

    uint32_t get_sample_rate() const { return _sample_rate; }

    uint64_t get_estimated_timer_based_on_clock() const;

    std::chrono::high_resolution_clock::time_point get_starting_point_clock();

    void set_float() { _float = true; }

    void set_byte() { _float = false; }

    bool is_float() const { return _float; }

    bool is_byte() const { return !_float; }

    void record(unsigned char* stream, int len);

    uint32_t channels() const { return _channels; }

    int32_t run(uint64_t t, int c);
    int32_t run_left(uint64_t t);
    int32_t run_right(uint64_t t);

  private:
    uint32_t _sample_rate;
    uint16_t _samples_per_go;
    uint32_t _channels;
    std::chrono::high_resolution_clock::time_point _start;

    bool _playing;
    bool _float;
    FILE* out;
    long data_chunk_pos;
    compiler* _comp;
    int32_t _left_value;
  };