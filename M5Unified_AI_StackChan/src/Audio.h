#ifndef _AUDIO_H
#define _AUDIO_H

//#include "I2S.h"
#include <Arduino.h>

// 16bit, monoral, 16000Hz,  linear PCM
class Audio {
  // static constexpr const size_t record_number = 256;
  // static constexpr const size_t record_length = 200;
  // static constexpr const size_t record_size = record_number * record_length;
  // static constexpr const size_t record_samplerate = 16000;
  static const int headerSize = 44;
//  static size_t rec_record_idx;
  // static const int i2sBufferSize = 6000;
  // char i2sBuffer[i2sBufferSize];
  void CreateWavHeader(byte* header, int waveDataSize);

public:
//  static constexpr const size_t record_number = 200;
  static constexpr const size_t record_number = 400;
//  static constexpr const size_t record_length = 200;
  static constexpr const size_t record_length = 150;
  static constexpr const size_t record_size = record_number * record_length;
  static constexpr const size_t record_samplerate = 16000;
//  static size_t rec_record_idx;
  static const int wavDataSize = record_number * record_length * 2;                   // It must be multiple of dividedWavDataSize. Recording time is about 1.9 second.
//  static const int dividedWavDataSize = i2sBufferSize/4;
//  static int16_t *wavData;                                         // It's divided. Because large continuous memory area can't be allocated in esp32.
  int16_t *wavData;                                         // It's divided. Because large continuous memory area can't be allocated in esp32.
  byte paddedHeader[headerSize + 4] = {0};                // The size must be multiple of 3 for Base64 encoding. Additional byte size must be even because wave data is 16bit.

  Audio();
  ~Audio();
  void Record();
};

#endif // _AUDIO_H
