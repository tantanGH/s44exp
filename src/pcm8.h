#ifndef __H_PCM8__
#define __H_PCM8__

int32_t pcm8_play(int16_t channel, uint32_t mode, uint32_t size, void* addr);
int32_t pcm8_play_array_chain(int16_t channel, uint32_t mode, int16_t count, void* addr);
int32_t pcm8_play_linked_array_chain(int16_t channel, uint32_t mode, void* addr);
int32_t pcm8_set_channel_mode(int16_t channel, uint32_t mode);
int32_t pcm8_get_data_length(int16_t channel);
int32_t pcm8_get_channel_mode(int16_t channel);
int32_t pcm8_stop();
int32_t pcm8_pause();
int32_t pcm8_resume();
int32_t pcm8_set_polyphonic_mode(int16_t mode);
int32_t pcm8_keepchk();

#endif