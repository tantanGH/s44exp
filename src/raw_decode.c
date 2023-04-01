#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "raw_decode.h"

//
//  init raw pcm decoder handle
//
int32_t raw_decode_init(RAW_DECODE_HANDLE* pcm, int32_t sample_rate, int16_t channels, int16_t little_endian) {

  int32_t rc = -1;

  // baseline
  pcm->sample_rate = sample_rate;
  pcm->channels = channels;
  pcm->little_endian = little_endian;
  pcm->resample_counter = 0;
 
  rc = 0;

exit:
  return rc;
}

//
//  close decoder handle
//
void raw_decode_close(RAW_DECODE_HANDLE* pcm) {

}

//
//  resampling and endian conversion
//
size_t raw_decode_resample(RAW_DECODE_HANDLE* pcm, int16_t* resample_buffer, int32_t resample_freq, int16_t* source_buffer, size_t source_buffer_len, int16_t gain) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (pcm->channels == 2) {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
      
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;
      
        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t lch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        int16_t rch = (int16_t)(source_buffer_uint8[3] * 256 + source_buffer_uint8[2]);
        int16_t x = ((int32_t)(lch + rch)) / 2 / gain;
        resample_buffer[ resample_buffer_ofs++ ] = x;
        source_buffer_ofs += 2;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {
      
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;
      
        // big endian
        int16_t x = ( (int32_t)(source_buffer[ source_buffer_ofs ]) + (int32_t)(source_buffer[ source_buffer_ofs + 1 ]) ) / 2 / gain;
        resample_buffer[ resample_buffer_ofs++ ] = x;
        source_buffer_ofs += 2;

      }

    }

  } else {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
    
        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;

        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t mch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        resample_buffer[ resample_buffer_ofs++ ] = mch / gain;
        source_buffer_ofs += 1;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {

        // down sampling
        pcm->resample_counter += resample_freq;
        if (pcm->resample_counter < pcm->sample_rate) {
          source_buffer_ofs += pcm->channels;     // skip
          continue;
        }

        pcm->resample_counter -= pcm->sample_rate;

        // big endian
        resample_buffer[ resample_buffer_ofs++ ] = source_buffer[ source_buffer_ofs++ ] / gain;

      }

    }

  }

  return resample_buffer_ofs;
}

//
//  endian conversion only
//
size_t raw_decode_convert_endian(RAW_DECODE_HANDLE* pcm, int16_t* resample_buffer, int16_t* source_buffer, size_t source_buffer_len) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (pcm->channels == 2) {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
      
        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t lch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        int16_t rch = (int16_t)(source_buffer_uint8[3] * 256 + source_buffer_uint8[2]);
        resample_buffer[ resample_buffer_ofs++ ] = lch;
        resample_buffer[ resample_buffer_ofs++ ] = rch;
        source_buffer_ofs += 2;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {

        int16_t lch = source_buffer[ source_buffer_ofs++ ];
        int16_t rch = source_buffer[ source_buffer_ofs++ ];
        resample_buffer[ resample_buffer_ofs++ ] = lch;
        resample_buffer[ resample_buffer_ofs++ ] = rch;

      }

    }

  } else {

    if (pcm->little_endian) {

      while (source_buffer_ofs < source_buffer_len) {
    
        // little endian
        uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
        int16_t mch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
        resample_buffer[ resample_buffer_ofs++ ] = mch;
        source_buffer_ofs += 1;

      }

    } else {

      while (source_buffer_ofs < source_buffer_len) {

        // big endian
        resample_buffer[ resample_buffer_ofs++ ] = source_buffer[ source_buffer_ofs++ ];

      }

    }

  }

  return resample_buffer_ofs;
}
