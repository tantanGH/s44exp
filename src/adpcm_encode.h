#ifndef __H_ADPCM_ENCODE__
#define __H_ADPCM_ENCODE__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
  int16_t step_index;
  int16_t last_estimate;
  size_t num_samples;
  size_t resample_counter;
  int16_t volume;
} ADPCM_ENCODE_HANDLE;

uint8_t msm6258v_encode(int16_t current_data, int16_t last_estimate, int16_t* step_index, int16_t* new_estimate);

int32_t adpcm_encode_init(ADPCM_ENCODE_HANDLE* adpcm, int16_t volume);
void adpcm_encode_close(ADPCM_ENCODE_HANDLE* adpcm);
int32_t adpcm_encode_resample(ADPCM_ENCODE_HANDLE* adpcm, uint8_t* adpcm_buffer, int32_t adpcm_freq, int16_t* pcm_buffer, size_t pcm_buffer_len, int32_t pcm_freq, int16_t pcm_channels, int16_t little_endian);

#endif