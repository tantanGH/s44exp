#ifndef __H_S44EXP__
#define __H_S44EXP__

#define VERSION "1.2.0 (2023/04/22)"

#define REG_DMAC_CH2_CSR (0xE84080 + 0x00)
#define REG_DMAC_CH3_BAR (0xE840C0 + 0x1C)

#define MAX_PATH_LEN (256)

#define MAX_PCM_FILES (256)

#define MAX_CHAINS (128)
#define CHAIN_TABLE_BUFFER_BYTES (0xff00)

#define FREAD_STAGING_BUFFER_BYTES (65536*4)

#define PCM8_TYPE_NONE    (0)
#define PCM8_TYPE_PCM8    (1)
#define PCM8_TYPE_PCM8A   (2)
#define PCM8_TYPE_PCM8PP  (3)

#define DRIVER_S44EXP  (0)
#define DRIVER_PCM8A   (1)
#define DRIVER_PCM8PP  (2)

#define FORMAT_ADPCM   (0)
#define FORMAT_RAW     (1)
#define FORMAT_YM2608  (2)
#define FORMAT_WAV     (3)
//#define FORMAT_MP3     (4)

typedef struct {
  void* buffer;
  uint16_t buffer_bytes;
  void* next;
} CHAIN_TABLE;

typedef struct {
  uint8_t* file_name;
  int16_t volume;
} PCM_FILE;

#endif