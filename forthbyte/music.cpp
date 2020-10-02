#include "music.h"
#include "compiler.h"

#include <SDL.h>
#include <stdint.h>

#include <cassert>

namespace
  {

//#define USE_MIX

  static unsigned char* audio_pos;
  static uint32_t audio_len;
  static uint64_t current_t;
  static uint64_t last_sample = 0;
  static int32_t last_result[2] = { 0,0 };
  static int32_t volume = 64;
  static unsigned char mixsrc[4096 * 4]; // * 2 for uint16 and * 2 for channels

  void my_audio_callback(void *userdata, unsigned char* stream, int len)
    {
    music* m = (music*)userdata;
    for (uint32_t i = 0; i < len / m->channels(); i += 2)
      {
      uint64_t sample = (current_t*m->get_sample_rate()) / 44100;
      int32_t result[2];
      if (sample == last_sample)
        {
        result[0] = last_result[0];
        result[1] = last_result[1];
        }
      else
        {
        result[0] = m->run_left(sample);
        result[1] = m->run_right(sample);
        last_sample = sample;
        last_result[0] = result[0];
        last_result[1] = result[1];
        }

      for (uint32_t j = 0; j < m->channels(); ++j)
        {
#ifdef USE_MIX
        int16_t* store = (int16_t*)&mixsrc[m->channels() * i + 2 * j];
#else
        int16_t* store = (int16_t*)&stream[m->channels() * i + 2*j];
#endif
        *store = (int16_t)result[j];
        }

      ++current_t;
      }
#ifdef USE_MIX
    SDL_memset(stream, 0, len);
    SDL_MixAudioFormat(stream, mixsrc, AUDIO_S16SYS, len, SDL_MIX_MAXVOLUME / 2);
#endif

    m->record(stream, len);

    }

  }

music::music(compiler* c) : _sample_rate(8000), _samples_per_go(4096), 
_playing(false), _float(true), out(nullptr), _channels(2), _comp(c)
  {
  _start = std::chrono::high_resolution_clock::now();
  }

music::~music()
  {
  stop();  
  }

int32_t music::run(uint64_t t, int c)
  {
  int32_t result;
  if (_float)
    {
    //result = (int32_t)std::floor(_comp->run_float(t, c)*128.0) * volume;
    result = (int32_t)std::floor(_comp->run_float(t, c)*128.0*volume);
    }
  else
    {
    result = ((int32_t)_comp->run_byte(t, c) - 128) * volume;
    }
  return result;
  }

int32_t music::run_left(uint64_t t)
  {
  _left_value = run(t, 0);
  return _left_value;
  }

int32_t music::run_right(uint64_t t)
  {
  bool stereo = _float ? _comp->stereo_float() : _comp->stereo_byte();
  return stereo ? run(t, 1) : _left_value;
  }

void music::play()
  {
  stop();

  SDL_AudioSpec wav_spec;
  SDL_zero(wav_spec);
  wav_spec.callback = my_audio_callback;
  wav_spec.userdata = this;
  wav_spec.channels = (uint8_t)_channels;
  wav_spec.format = AUDIO_S16SYS;
  wav_spec.freq = 44100;
  wav_spec.padding = 0;
  wav_spec.samples = _samples_per_go;   
  if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
    printf("Couldn't open audio: %s\n", SDL_GetError());
    exit(-1);
    }

  out = fopen("session.wav", "wb");
  if (out)
    {
    fwrite("RIFF----WAVEfmt ", 16, 1, out);
    uint32_t val32 = 16; // no extension data
    fwrite(&val32, 4, 1, out);
    uint16_t val16 = 1; // PCM - integer samples
    fwrite(&val16, 2, 1, out);
    val16 = (uint16_t)_channels; // two channels
    fwrite(&val16, 2, 1, out);
    val32 = 44100;// _sample_rate; // samples per second (Hz)
    fwrite(&val32, 4, 1, out);
    val32 = 44100*16* _channels /8; // (Sample Rate * BitsPerSample * Channels) / 8
    fwrite(&val32, 4, 1, out);
    val16 = (uint16_t)(2* _channels); // data block size (size of two integer samples, one for each channel)
    fwrite(&val16, 2, 1, out);
    val16 = 16; // number of bits per sample (use a multiple of 8)
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
  current_t = 0;
  }

uint64_t music::get_timer() const
  {
  return current_t;
  }

void music::set_sample_rate(uint32_t sample_rate)
  {
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
