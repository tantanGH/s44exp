#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "wav_decode.h"

//
//  init wav decoder handle
//
int32_t wav_decode_init(WAV_DECODE_HANDLE* wav) {

  int32_t rc = -1;

  wav->sample_rate = -1;
  wav->channels = -1;
  wav->byte_rate = -1;
  wav->block_align = -1;
  wav->bits_per_sample = -1;
  wav->duration = -1;

  wav->resample_counter = 0;
 
  rc = 0;

exit:
  return rc;
}

//
//  close decoder handle
//
void wav_decode_close(WAV_DECODE_HANDLE* wav) {

}

//
//  parse wav header
//
int32_t wav_decode_parse_header(WAV_DECODE_HANDLE* wav, FILE* fp) {

  int32_t rc = -1;
  size_t bytes_read = 0;

  // ChunkID
  uint8_t buf[5];
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  buf[4] = '\0';
  if (strcmp(buf, "RIFF") != 0) {
    printf("error: incorrect wav format.\n");
    goto exit;
  }

  // ChunkSize
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);

  // Format
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  buf[4] = '\0';
  if (strcmp(buf, "WAVE") != 0) {
    printf("error: incorrect wav format.\n");
    goto exit;
  }

  // Subchunk1ID
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  buf[4] = '\0';

  // check if we have a JUNK Chunk
  if (strcmp(buf, "JUNK") == 0) {
    size_t bytes_junk = fread(buf, sizeof(uint8_t), 4, fp);
    buf[4] = '\0';
    bytes_junk += buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
    if (fseek(fp, bytes_read + bytes_junk, SEEK_SET) != 0) {
      printf("error: wav seek error.\n");
      goto exit;
    }
    bytes_read += bytes_junk;
//    bytes_expected += bytes_junk + 4;
    // now really read the fmt chunk
    bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
    buf[4] = '\0';
  }

  // get the fmt chunk
  if (strcmp(buf, "fmt ") != 0) {
    printf("error: wav fmt chunk read error.\n");
    goto exit;
  }

  // Subchunk1Size
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  int16_t format = buf[0];
  if (format != 16 && format != 18) {
    printf("error: wav unknown format (%d).\n", format);
    goto exit;
  }
  if (buf[1] != 0 || buf[2] != 0 || buf[3] != 0) {
    printf("error: wav Subchunk1Size error.\n");
    goto exit;
  }

  // AudioFormat
  bytes_read += fread(buf, sizeof(uint8_t), 2, fp);
  if (buf[0] != 1 || buf[1] != 0) {
    printf("error: wav AudioFormat error.\n");
    goto exit;
  }

  // NumChannels
  bytes_read += fread(buf, sizeof(uint8_t), 2, fp);
  wav->channels = buf[0] + (buf[1] << 8);
  if (wav->channels != 1 && wav->channels != 2) {
    printf("error: wav unsupported channels (%d).\n", wav->channels);
    goto exit;
  }

  // SampleRate
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  wav->sample_rate = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
  if (wav->sample_rate != 32000 && wav->sample_rate != 44100 && wav->sample_rate != 48000) {
    printf("error: wav unsupported sample rate (%d).\n", wav->sample_rate);
    goto exit;
  }

  // ByteRate
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  wav->byte_rate = buf[0] + (buf[1] <<8) + (buf[2] << 16) + (buf[3] << 24);

  // BlockAlign
  bytes_read += fread(buf, sizeof(uint8_t), 2, fp);
  wav->block_align = buf[0] + (buf[1] << 8);

  // BitsPerSample
  bytes_read += fread(buf, sizeof(uint8_t), 2, fp);
  wav->bits_per_sample = buf[0] + (buf[1] << 8);
  if (wav->bits_per_sample != 16) {
    printf("error: wav unsupported bit per sample (%d).\n", wav->bits_per_sample);
    goto exit;
  }

  // skip 2 bytes if format is 18
  if (format == 18) {
    bytes_read += fread(buf, sizeof(uint8_t), 2, fp);
  }

  // Subchunk2ID
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  buf[4] = '\0';
  while (strcmp(buf, "data") != 0) {
    if (feof(fp) || ferror(fp)) {
      printf("error: wav Shubchunk2 read error.\n");
      goto exit;
    }
    size_t bytes_junk = fread(buf, sizeof(uint8_t), 4, fp);
    buf[4] = '\0';
    bytes_junk += buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
    if (fseek(fp, bytes_read + bytes_junk, SEEK_SET) != 0) {
      printf("error: wav seek error.\n");
      goto exit;
    }
    bytes_read += bytes_junk;
    //bytes_expected += bytes_junk+ 4;
    bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
    buf[4] = '\0';
  }

  // Subchunk2Size
  bytes_read += fread(buf, sizeof(uint8_t), 4, fp);
  wav->duration = (buf[0] + (buf[1]<<8) + (buf[2]<<16) + (buf[3]<<24)) / wav->block_align;

  rc = bytes_read;

exit:
  return rc;
}

//
//  resampling with endian conversion
//
size_t wav_decode_resample(WAV_DECODE_HANDLE* wav, int16_t* resample_buffer, int32_t resample_freq, int16_t* source_buffer, size_t source_buffer_len, int16_t gain) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (wav->channels == 2) {

    while (source_buffer_ofs < source_buffer_len) {
    
      // down sampling
      wav->resample_counter += resample_freq;
      if (wav->resample_counter < wav->sample_rate) {
        source_buffer_ofs += wav->channels;     // skip
        continue;
      }

      wav->resample_counter -= wav->sample_rate;
    
      // little endian
      uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
      int16_t lch = (int16_t)(source_buffer_uint8[0] + source_buffer_uint8[1] * 256);
      int16_t rch = (int16_t)(source_buffer_uint8[2] + source_buffer_uint8[3] * 256);
      int16_t x = ((int32_t)(lch + rch)) / 2 / gain;
      resample_buffer[ resample_buffer_ofs++ ] = x;
      source_buffer_ofs += 2;

    }

  } else {

    while (source_buffer_ofs < source_buffer_len) {
  
      // down sampling
      wav->resample_counter += resample_freq;
      if (wav->resample_counter < wav->sample_rate) {
        source_buffer_ofs += wav->channels;     // skip
        continue;
      }

      wav->resample_counter -= wav->sample_rate;

      // little endian
      uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
      int16_t mch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
      resample_buffer[ resample_buffer_ofs++ ] = mch / gain;
      source_buffer_ofs += 1;

    }

  }

  return resample_buffer_ofs;
}

//
//  endian conversion only
//
size_t wav_decode_convert_endian(WAV_DECODE_HANDLE* wav, int16_t* resample_buffer, int16_t* source_buffer, size_t source_buffer_len) {

  // resampling
  size_t source_buffer_ofs = 0;
  size_t resample_buffer_ofs = 0;
  
  if (wav->channels == 2) {
  
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
  
      // little endian
      uint8_t* source_buffer_uint8 = (uint8_t*)(&(source_buffer[ source_buffer_ofs ]));
      int16_t mch = (int16_t)(source_buffer_uint8[1] * 256 + source_buffer_uint8[0]);
      resample_buffer[ resample_buffer_ofs++ ] = mch;
      source_buffer_ofs += 1;

    }

  }

  return resample_buffer_ofs;
}
