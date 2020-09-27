#include "music.h"
#include "compiler.h"

#include <SDL.h>
#include <stdint.h>

namespace
  {

  static unsigned char* audio_pos;
  static uint32_t audio_len;
  static uint64_t current_t;

  void my_audio_callback(void *userdata, unsigned char* stream, int len)
    {
    if (audio_len == 0)
      return;

    len = (len > audio_len ? audio_len : len);
    SDL_memcpy(stream, audio_pos, len); 					// simply copy from one buffer into the other
    ((music*)userdata)->record(stream, len);
    audio_pos += len;
    audio_len -= len;

    current_t += len ;

    if (audio_len == 0)
      ((music*)userdata)->swap_buffers();
    }

  }

music::music() : _sample_rate(8000), _t(0), _samples_per_go(1024), _buffer_width(1), _buffer_is_filled(false),
_playing(false), _float(false), out(nullptr)
  {
  _buffer_to_play.resize(_samples_per_go*_buffer_width, 127);
  _buffer_to_fill.resize(_samples_per_go*_buffer_width, 127);
  _start = std::chrono::high_resolution_clock::now();
  }

music::~music()
  {
  stop();  
  }

void music::fill_buffer(compiler& c)
  {
  if (_playing && !_buffer_is_filled)
    {
    if (_float)
      {
      for (uint64_t cnt = 0; cnt < _samples_per_go*_buffer_width; ++cnt)
        {
        double d = c.run_float(_t);
        d = (d + 1.0)*128.0;
        int res = (int)std::floor(d);
        if (res < 0)
          res = 0;
        if (res > 255)
          res = 255;
        _buffer_to_fill[cnt] = (unsigned char)res;
        ++_t;
        }
      }
    else
      {
      for (uint64_t cnt = 0; cnt < _samples_per_go*_buffer_width; ++cnt)
        {
        auto ch = c.run_byte(_t);
        _buffer_to_fill[cnt] = ch;
        ++_t;
        }
      }
    _buffer_is_filled = true;
    }
  }

void music::swap_buffers()
  {
  std::swap(_buffer_to_fill, _buffer_to_play);
  _buffer_is_filled = false;
  audio_pos = _buffer_to_play.data();
  audio_len = _buffer_to_play.size();
  }

void music::init_buffer_for_playing(compiler& c)
  {
  _playing = true;
  _buffer_is_filled = false;
  fill_buffer(c);
  swap_buffers();
  }

void music::play(compiler& c)
  {
  stop();
  init_buffer_for_playing(c);

  SDL_AudioSpec wav_spec;
  wav_spec.callback = my_audio_callback;
  wav_spec.userdata = this;
  wav_spec.channels = 1;
  wav_spec.format = AUDIO_U8;
  wav_spec.freq = _sample_rate;
  wav_spec.padding = 0;
  wav_spec.samples = _samples_per_go;
  wav_spec.silence = 127;
  wav_spec.size = 0;

  if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
    printf("Couldn't open audio: %s\n", SDL_GetError());
    exit(-1);
    }

  out = fopen("session.wav", "wb");
  if (out)
    {
    fwrite("RIFF----WAVEfmt ", 16, 1, out);
    uint32_t val32 = 16;
    fwrite(&val32, 4, 1, out);
    uint16_t val16 = 1;
    fwrite(&val16, 2, 1, out);
    val16 = 1;
    fwrite(&val16, 2, 1, out);
    val32 = _sample_rate;
    fwrite(&val32, 4, 1, out);
    val32 = _samples_per_go;
    fwrite(&val32, 4, 1, out);
    val16 = 1;
    fwrite(&val16, 2, 1, out);
    val16 = 8;
    fwrite(&val16, 2, 1, out);
    data_chunk_pos = ftell(out);
    fwrite("data----", 8, 1, out);

    }

  _start = std::chrono::high_resolution_clock::now();
  SDL_PauseAudio(0);

  }

void music::stop()
  {
  _playing = false;
  if (_t > audio_len)
    _t -= audio_len;
  SDL_CloseAudio();
  if (out)
    {
    long file_length = ftell(out);
    fseek(out, data_chunk_pos + 4, SEEK_SET);
    uint32_t value = file_length - data_chunk_pos + 8;
    fwrite(&value, 4, 1, out);
    fseek(out, 4, SEEK_SET);
    value = file_length - 8;
    fwrite(&value, 4, 1, out);
    fclose(out);
    out = nullptr;
    }
  }

void music::reset_timer()
  {
  _start = std::chrono::high_resolution_clock::now();
  _t = 0;
  current_t = _t;
  _buffer_is_filled = false;
  }

uint64_t music::get_timer() const
  {
  return current_t;
  }

void music::set_sample_rate(uint32_t sample_rate)
  {
  stop();
  _sample_rate = sample_rate;
  }

uint64_t music::get_estimated_timer_based_on_clock() const
  {
  auto tic = std::chrono::high_resolution_clock::now();
  double seconds = double(std::chrono::duration_cast<std::chrono::microseconds>(tic - _start).count()) / 1000000.0;

  auto ticks = (double)_sample_rate * seconds;
  return (uint64_t)ticks;
  }

std::chrono::high_resolution_clock::time_point music::get_starting_point_clock()
  {
  return _start;
  }

void music::record(unsigned char* stream, int len)
  {
  if (out)
    {
    fwrite((const void*)stream, 1, len, out);
    }
  }