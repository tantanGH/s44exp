#ifndef __H_RAW_DECODE__
#define __H_RAW_DECODE__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {

  int32_t sample_rate;
  int16_t channels;
  int16_t little_endian;

  size_t resample_counter;

} RAW_DECODE_HANDLE;

int32_t raw_decode_init(RAW_DECODE_HANDLE* pcm, int32_t sample_rate, int16_t channels, int16_t little_endian);
void raw_decode_close(RAW_DECODE_HANDLE* pcm);
size_t raw_decode_resample(RAW_DECODE_HANDLE* pcm, int16_t* resample_buffer, int32_t resample_freq, int16_t* source_buffer, size_t source_buffer_len, int16_t gain);
size_t raw_decode_convert_endian(RAW_DECODE_HANDLE* pcm, int16_t* resample_buffer, int16_t* source_buffer, size_t source_buffer_len);

#endif