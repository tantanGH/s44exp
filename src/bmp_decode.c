#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "himem.h"
#include "bmp_decode.h"

#define GVRAM ((uint16_t*)0xC00000)

//
//  init BMP decode handler
//
int32_t bmp_decode_init(BMP_DECODE_HANDLE* bmp, int16_t brightness, int16_t half_size) {

  int32_t rc = -1;

  bmp->brightness = brightness;
  bmp->half_size = half_size;

  bmp->rgb555_r = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);
  bmp->rgb555_g = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);
  bmp->rgb555_b = (uint16_t*)himem_malloc(sizeof(uint16_t) * 256, 0);

  if (bmp->rgb555_r == NULL || bmp->rgb555_g == NULL || bmp->rgb555_b == NULL) goto exit;

  for (int16_t i = 0; i < 256; i++) {
    uint32_t c = (uint32_t)(i * 32 * brightness / 100) >> 8;
    bmp->rgb555_r[i] = (uint16_t)((c <<  6) + 1);
    bmp->rgb555_g[i] = (uint16_t)((c << 11) + 1);
    bmp->rgb555_b[i] = (uint16_t)((c <<  1) + 1);
  }

  rc = 0;

exit:
  return rc;
}

//
//  close BMP decode handler
//
void bmp_decode_close(BMP_DECODE_HANDLE* bmp) {
  if (bmp->rgb555_r != NULL) {
    himem_free(bmp->rgb555_r, 0);
    bmp->rgb555_r = NULL;
  }
  if (bmp->rgb555_g != NULL) {
    himem_free(bmp->rgb555_g, 0);
    bmp->rgb555_g = NULL;
  }
  if (bmp->rgb555_b != NULL) {
    himem_free(bmp->rgb555_b, 0);
    bmp->rgb555_b = NULL;
  }   
}

//
//  half size decode
//
static int32_t bmp_decode_exec_half(BMP_DECODE_HANDLE* bmp, uint8_t* bmp_buffer, size_t bmp_buffer_bytes) {

  int32_t rc = -1;

  if (bmp_buffer[0] != 0x42 || bmp_buffer[1] != 0x4d) goto exit;

  int32_t bmp_width  = bmp_buffer[18] + (bmp_buffer[19] << 8) + (bmp_buffer[20] << 16) + (bmp_buffer[21] << 24);
  int32_t bmp_height = bmp_buffer[22] + (bmp_buffer[23] << 8) + (bmp_buffer[24] << 16) + (bmp_buffer[25] << 24);

  int32_t bmp_bit_depth = bmp_buffer[28] + (bmp_buffer[29] << 8);
  if (bmp_bit_depth != 24) goto exit;
  
  int32_t ofs_y = ( 512 - bmp_height/2 ) / 2;
  int32_t ofs_x = ( 512 - bmp_width/2 ) / 2;

  int32_t pitch = 512;
  int32_t xmax = 511;

  uint8_t* bmp_bitmap = bmp_buffer + 54;

  size_t padding = (4 - ((bmp_width * 3) % 4)) % 4;

  for (int32_t y = bmp_height-1; y >= 0; y--) {
  
    if (y & 0x0001) {
      bmp_bitmap += 3 * bmp_width + padding;
      continue;
    }

    int32_t cy = ofs_y + y/2;
    if (cy < 0) {
      //bmp_bitmap += 3 * bmp_width + padding;
      //continue;
      break;
    }
    if (cy > 511) {
      bmp_bitmap += 3 * bmp_width + padding;
      continue;
    }

    uint16_t* gvram = GVRAM + cy * pitch;

    for (int32_t x = 0; x < bmp_width; x++) {

      if (x & 0x0001) {
        bmp_bitmap += 3;
        continue;
      }

      int32_t cx = ofs_x + x/2;
      if (cx < 0 || cx > xmax) {
        bmp_bitmap += 3;
        continue;
      }

      uint8_t b = *bmp_bitmap++;
      uint8_t g = *bmp_bitmap++;
      uint8_t r = *bmp_bitmap++;
      gvram[ cx ] = bmp->rgb555_g[ g ] | bmp->rgb555_r[ r ] | bmp->rgb555_b[ b ] | 1;

    }

    bmp_bitmap += padding;
  }

  rc = 0;

exit:

  return rc;
}

//
//  normal size decode
//
int32_t bmp_decode_exec(BMP_DECODE_HANDLE* bmp, uint8_t* bmp_buffer, size_t bmp_buffer_bytes) {

  if (bmp->half_size) {
    return bmp_decode_exec_half(bmp, bmp_buffer, bmp_buffer_bytes);
  }

  int32_t rc = -1;

  if (bmp_buffer[0] != 0x42 || bmp_buffer[1] != 0x4d) goto exit;

  int32_t bmp_width  = bmp_buffer[18] + (bmp_buffer[19] << 8) + (bmp_buffer[20] << 16) + (bmp_buffer[21] << 24);
  int32_t bmp_height = bmp_buffer[22] + (bmp_buffer[23] << 8) + (bmp_buffer[24] << 16) + (bmp_buffer[25] << 24);

  int32_t bmp_bit_depth = bmp_buffer[28] + (bmp_buffer[29] << 8);
  if (bmp_bit_depth != 24) goto exit;
  
  int32_t ofs_y = ( 512 - bmp_height ) / 2;
  int32_t ofs_x = ( 512 - bmp_width ) / 2;

  int32_t pitch = 512;
  int32_t xmax = 511;

  uint8_t* bmp_bitmap = bmp_buffer + 54;

  size_t padding = (4 - ((bmp_width * 3) % 4)) % 4;

  for (int32_t y = bmp_height-1; y >= 0; y--) {
  
    int32_t cy = ofs_y + y;
    if (cy < 0) {
      //bmp_bitmap += 3 * bmp_width + padding;
      //continue;
      break;
    }
    if (cy > 511) {
      bmp_bitmap += 3 * bmp_width + padding;
      continue;
    }

    uint16_t* gvram = GVRAM + cy * pitch;

    for (int32_t x = 0; x < bmp_width; x++) {

      int32_t cx = ofs_x + x;
      if (cx < 0 || cx > xmax) {
        bmp_bitmap += 3;
        continue;
      }

      uint8_t b = *bmp_bitmap++;
      uint8_t g = *bmp_bitmap++;
      uint8_t r = *bmp_bitmap++;
      gvram[ cx ] = bmp->rgb555_g[ g ] | bmp->rgb555_r[ r ] | bmp->rgb555_b[ b ] | 1;

    }

    bmp_bitmap += padding;
  }

  rc = 0;

exit:

  return rc;
}