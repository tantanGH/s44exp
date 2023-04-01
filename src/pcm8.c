#include <stdint.h>
#include <string.h>
#include <iocslib.h>
#include "pcm8.h"

//
//  play in normal mode ($000x)
//
int32_t pcm8_play(int16_t channel, uint32_t mode, uint32_t size, void* addr) {

  register uint32_t reg_d0 asm ("d0") = 0x0000 + channel;
  register uint32_t reg_d1 asm ("d1") = mode;
  register uint32_t reg_d2 asm ("d2") = size;
  register uint32_t reg_a1 asm ("a1") = (uint32_t)addr;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    : "r"   (reg_d1),   // input operand
      "r"   (reg_d2),   // input operand
      "r"   (reg_a1)    // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  play in array chain mode ($001x)
//
int32_t pcm8_play_array_chain(int16_t channel, uint32_t mode, int16_t count, void* addr) {

  register uint32_t reg_d0 asm ("d0") = 0x0010 + channel;
  register uint32_t reg_d1 asm ("d1") = mode;
  register uint32_t reg_d2 asm ("d2") = count;
  register uint32_t reg_a1 asm ("a1") = (uint32_t)addr;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    : "r"   (reg_d1),   // input operand
      "r"   (reg_d2),   // input operand
      "r"   (reg_a1)    // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  play in linked array chain mode ($002x)
//
int32_t pcm8_play_linked_array_chain(int16_t channel, uint32_t mode, void* addr) {

  register uint32_t reg_d0 asm ("d0") = 0x0020 + channel;
  register uint32_t reg_d1 asm ("d1") = mode;
  register uint32_t reg_a1 asm ("a1") = (uint32_t)addr;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    : "r"   (reg_d1),   // input operand
      "r"   (reg_a1)    // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  set channel mode ($007x)
//
int32_t pcm8_set_channel_mode(int16_t channel, uint32_t mode) {

  register uint32_t reg_d0 asm ("d0") = 0x0070 + channel;
  register uint32_t reg_d1 asm ("d1") = mode;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    : "r"   (reg_d1)    // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  get data length ($008x)
//
int32_t pcm8_get_data_length(int16_t channel) {

  register uint32_t reg_d0 asm ("d0") = 0x0080 + channel;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    :                   // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  get channel mode ($009x)
//
int32_t pcm8_get_channel_mode(int16_t channel) {

  register uint32_t reg_d0 asm ("d0") = 0x0090 + channel;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    :                   // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  stop all channels ($0100)
//
int32_t pcm8_stop() {

  register uint32_t reg_d0 asm ("d0") = 0x0100;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    :                   // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  pause all channels ($0101)
//
int32_t pcm8_pause() {

  register uint32_t reg_d0 asm ("d0") = 0x0101;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    :                   // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  resume all channels ($0102)
//
int32_t pcm8_resume() {

  register uint32_t reg_d0 asm ("d0") = 0x0102;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    :                   // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  set polyphonic mode ($01fc)
//
int32_t pcm8_set_polyphonic_mode(int16_t mode) {

  register uint32_t reg_d0 asm ("d0") = 0x01fc;
  register uint32_t reg_d1 asm ("d1") = mode;

  asm volatile (
    "trap #2\n"         // trap #2
    : "+r"  (reg_d0)    // output (&input) operand
    : "r"   (reg_d1)    // input operand
    :                   // clobbered register
  );

  return reg_d0;
}

//
//  pcm8 keep check
//
int32_t pcm8_keepchk() {

  uint8_t eye_catch_addr_bytes[4];
  for (int16_t i = 0; i < 4; i++) {
    eye_catch_addr_bytes[i] = B_BPEEK((uint8_t*)(0x0088 + i));
  }
  uint8_t* eye_catch_addr = *((uint8_t**)eye_catch_addr_bytes) - 8;

  uint8_t eye_catch[8];
  for (int16_t i = 0; i < 8; i++) {
    eye_catch[i] = B_BPEEK((uint8_t*)(eye_catch_addr + i));
  }

  return (memcmp(eye_catch, "PCM8/048", 8) == 0) ? 1 : 0;
}