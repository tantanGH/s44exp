#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <jstring.h>
#include <stat.h>
#include <doslib.h>
#include <iocslib.h>

// devices
#include "keyboard.h"
#include "crtc.h"
#include "himem.h"

// drivers
#include "pcm8.h"
#include "pcm8a.h"
#include "pcm8pp.h"

// codec
#include "adpcm_encode.h"
#include "raw_decode.h"
#include "wav_decode.h"
#include "ym2608_decode.h"

// artwork
#ifdef JPEG_SUPPORT
#include "jpeg_decode.h"
#endif
#include "bmp_decode.h"

// application
#include "kmd.h"
#include "s44exp.h"

// original function key mode
static int32_t g_funckey_mode = -1;

// abort vector handler
static void abort_application() {

  // stop ADPCM
  ADPCMMOD(0);

  // cursor on
  C_CURON();

  // funckey mode
  if (g_funckey_mode >= 0) {
    C_FNKMOD(g_funckey_mode);
  }
  
  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

  B_PRINT("aborted.\n");

  exit(1);
}

// show help message
static void show_help_message() {
  printf("usage: s44exp [options] <input-file[.pcm|.sXX|.mXX|.aXX|.nXX|.wav]>\n");
  printf("options:\n");
  printf("     -v[n] ... volume (1-15, default:7)\n");
  printf("     -l[n] ... loop count (none:endless, default:1)\n");
  printf("     -t[n] ... album art display brightness (1-100, default:off)\n");
  printf("     -x    ... full screen\n");
  printf("     -c    ... clear screen after full screen playback\n");
//  printf("     -s    ... wait vsync for KMD display\n");
  printf("\n");
  printf("     -i <indirect-file> ... playlist indirect file\n");
  printf("     -s    ... shuffle play\n");
  printf("\n");
  printf("     -b<n> ... buffer size [x 64KB] (2-96,default:4)\n");
//  printf("\n");
//  printf("     -f    ... do not use .s44/.a44/.wav as mp3 playback cache\n");
//  printf("     -a    ... use S44EXP for ADPCM encoding\n");
//  printf("     -z    ... use little endian for .s44/.m44\n");
  printf("     -h    ... show help message\n");
}

// main
int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = 1;

  // credit
  printf("S44EXP.X - ADPCM/PCM/WAV player for X680x0 version " VERSION " by tantan\n");

  // parse command line options
  uint8_t* pcm_file_name = NULL;
  int16_t playback_driver = DRIVER_S44EXP;
  int16_t playback_volume = 7;
  int16_t loop_count = 1;
  int16_t pic_brightness = 0;
  int16_t full_screen = 0;
  int16_t clear_screen = 0;
  int16_t num_chains = 4;
  int16_t use_high_memory = 0;
  int16_t use_little_endian = 0;
  int16_t wait_vsync = 0;
  int16_t shuffle_play = 0;
  int32_t adpcm_output_freq = 15625;

  // play list
  static PCM_FILE pcm_files[ MAX_PCM_FILES ];
  int16_t num_pcm_files = 0;
  uint8_t* indirect_file_names = NULL;

  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 'v') {
        playback_volume = atoi(argv[i]+2);
        if (playback_volume < 1 || playback_volume > 15 || strlen(argv[i]) < 3) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'l') {
        loop_count = atoi(argv[i]+2);
      } else if (argv[i][1] == 't') {
        pic_brightness = atoi(argv[i]+2);
        if (pic_brightness < 0 || pic_brightness > 100 || strlen(argv[i]) < 3) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'x') {
        full_screen = 1;
      } else if (argv[i][1] == 'c') {
        clear_screen = 1;
      } else if (argv[i][1] == 's') {
        shuffle_play = 1;
      } else if (argv[i][1] == 'b') {
        num_chains = atoi(argv[i]+2);
        if (num_chains < 2 || num_chains > 96) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 's') {
        wait_vsync = 1;

//      } else if (argv[i][1] == 'u') {
//        if (!himem_isavailable()) {
//          printf("error: high memory driver is not installed.\n");
//          goto exit;
//        }
//        use_high_memory = 1;
//      } else if (argv[i][1] == 'z') {
//        use_little_endian = 1;
//      } else if (argv[i][1] == 'o') {
//        int16_t out_freq = atoi(argv[i]+2);
//        if (out_freq == 2) {
//          adpcm_output_freq = 7812;
//        } else if (out_freq == 1) {
//          adpcm_output_freq = 10417;
//        } else {
//          adpcm_output_freq = 15625;
//        }

      } else if (argv[i][1] == 'i' && i+1 < argc) {

        // indirect file
        int16_t count = 0;
        FILE* fp = fopen(argv[i+1], "r");
        if (fp != NULL) {

          // phase1: count lines
          static uint8_t line[ MAX_PATH_LEN + 1 ];
          while (fgets(line, MAX_PATH_LEN, fp) != NULL) {

            for (int16_t i = 0; i < MAX_PATH_LEN; i++) {
              if (line[i] <= ' ') {
                line[i] = '\0';
              }
            }

            if (strlen(line) < 5) continue;

            count++;
          }
          fclose(fp);
          fp = NULL;

          if (count > MAX_PCM_FILES) {
            printf("error: too many pcm files in the indirect file.\n");
            goto exit;
          }

          // phase2: read lines
          indirect_file_names = himem_malloc( MAX_PATH_LEN * count, 0 );
          fp = fopen(argv[i+1], "r");
          while (fgets(line, MAX_PATH_LEN, fp) != NULL) {

            for (int16_t i = 0; i < MAX_PATH_LEN; i++) {
              if (line[i] <= ' ') {
                line[i] = '\0';
              }
            }

            if (strlen(line) < 5) continue;

            int16_t volume = playback_volume;
            for (int16_t i = 0; i < MAX_PATH_LEN; i++) {
              if (line[i] == ',') {
                if (i+2 < MAX_PATH_LEN && line[i+1] == 'v') {
                  int16_t v = atoi(line+i+2);
                  if (v >= 1 && v <= 12) volume = v;
                }
                line[i] = '\0';
                break;
              }
            }
     
            pcm_files[ num_pcm_files ].file_name = indirect_file_names + MAX_PATH_LEN * num_pcm_files;
            strcpy(pcm_files[ num_pcm_files ].file_name, line);
            pcm_files[ num_pcm_files ].volume = volume;
            num_pcm_files++;

          }

          fclose(fp);
          fp = NULL;

        }
        i++;

      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
//      if (pcm_file_name != NULL) {
//        printf("error: multiple files are not supported.\n");
//        goto exit;
//      }
//      pcm_file_name = argv[i];
      if (strlen(argv[i]) >= 5) {
        pcm_files[ num_pcm_files ].file_name = argv[i];
        pcm_files[ num_pcm_files ].volume = playback_volume;
        num_pcm_files++;
      }
    }
  }

//  if (pcm_file_name == NULL || strlen(pcm_file_name) < 5) {
  if (num_pcm_files == 0) {
    show_help_message();
    goto exit;
  }

  if (shuffle_play) {
    // randomize
    srand(ONTIME());
    for (int16_t i = 0; i < num_pcm_files * 10; i++) {
      int16_t a = rand() % num_pcm_files;
      int16_t b = rand() % num_pcm_files;
      uint8_t* c = pcm_files[ a ].file_name;
      int16_t v = pcm_files[ a ].volume;
      pcm_files[ a ].file_name = pcm_files[ b ].file_name;
      pcm_files[ a ].volume = pcm_files[ b ].volume;
      pcm_files[ b ].file_name = c;
      pcm_files[ b ].volume = v;
    }
  }

  // determine PCM8 type
  int16_t pcm8_type = PCM8_TYPE_NONE;
  if (pcm8pp_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8PP;
  } else if (pcm8a_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8A;
  } else if (pcm8_keepchk()) {
    pcm8_type = PCM8_TYPE_PCM8;
  }

  // playback driver selection
  if (pcm8_type == PCM8_TYPE_PCM8PP) {
    playback_driver = DRIVER_PCM8PP;
  } else if (pcm8_type == PCM8_TYPE_PCM8A) {
    playback_driver = DRIVER_PCM8A;
  } else {
    playback_driver = DRIVER_S44EXP;
  }

  // cursor off
  C_CUROFF();

  // set abort vectors
  uint32_t abort_vector1 = INTVCS(0xFFF1, (int8_t*)abort_application);
  uint32_t abort_vector2 = INTVCS(0xFFF2, (int8_t*)abort_application);  

  // enter supervisor mode
  if (pic_brightness > 0) {
    B_SUPER(0);
  }

  int16_t playback_index = 0;
  int16_t first_play = 1;

loop:

  // init crtc if album art is required
  if (pic_brightness > 0) {
    G_CLR_ON();
    crtc_set_extra_mode(0);
  }

  // full screen mode
  if (full_screen) {
    // function key display off
    g_funckey_mode = C_FNKMOD(-1);
    C_FNKMOD(3);
    C_CLS_AL();
    if (pic_brightness == 0) {
      G_CLR_ON();
    }
  }

  // for text plane 2 masking (for XM6g bug woraround, we cannot scroll position before updating)
  if (pic_brightness > 0) {
    TPALET2(4, 0x0001);
    TPALET2(5, TPALET2(1,-1));
    TPALET2(6, TPALET2(2,-1));
    TPALET2(7, TPALET2(3,-1));
    struct TXFILLPTR txfil = { 2, 0, 0, 768, 512, 0xffff };
    TXFILL(&txfil);
  }

  pcm_file_name = pcm_files[ playback_index ].file_name;
  playback_volume = pcm_files[ playback_index ].volume;

  // input pcm file name and extension
  uint8_t* pcm_file_exp = pcm_file_name + strlen(pcm_file_name) - 4;

  // input format check
  int16_t input_format = FORMAT_ADPCM;
  int32_t pcm_freq = 15625;
  int16_t pcm_channels = 1;
//  int16_t use_mp3_cache = 0;
  if (stricmp(".pcm", pcm_file_exp) == 0) {
    input_format = FORMAT_ADPCM;
    pcm_freq = 15625;                 // fixed
    pcm_channels = 1;
    adpcm_output_freq = 15625;        // fixed
  } else if (stricmp(".s32", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 32000;
    pcm_channels = 2;
  } else if (stricmp(".s44", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 44100;
    pcm_channels = 2;
  } else if (stricmp(".s48", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 48000;
    pcm_channels = 2;
  } else if (stricmp(".m32", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 32000;
    pcm_channels = 1;
  } else if (stricmp(".m44", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 44100;
    pcm_channels = 1;
  } else if (stricmp(".m48", pcm_file_exp) == 0) {
    input_format = FORMAT_RAW;
    pcm_freq = 48000;
    pcm_channels = 1;
  } else if (stricmp(".a32", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 32000;
    pcm_channels = 2;
  } else if (stricmp(".a44", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 44100;
    pcm_channels = 2;
  } else if (stricmp(".a48", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 48000;
    pcm_channels = 2;
  } else if (stricmp(".n32", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 32000;
    pcm_channels = 1;
  } else if (stricmp(".n44", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 44100;
    pcm_channels = 1;
  } else if (stricmp(".n48", pcm_file_exp) == 0) {
    input_format = FORMAT_YM2608;
    pcm_freq = 48000;
    pcm_channels = 1;
  } else if (stricmp(".wav", pcm_file_exp) == 0) {
    input_format = FORMAT_WAV;
    pcm_freq = -1;
    pcm_channels = -1;
  } else {
    printf("error: unknown format file (%s).\n", pcm_file_name);
    goto exit;
  }

  // reset PCM8 / PCM8A / PCM8PP / IOCS ADPCM
  if (pcm8_type != PCM8_TYPE_NONE) {
    pcm8_pause();
    pcm8_stop();
  } else {
    ADPCMMOD(0);
  }

  // file read buffers
  void* fread_buffer = NULL;
  void* fread_staging_buffer = NULL;
  FILE* fp = NULL;

try:

  // check kmd file existence
  static uint8_t kmd_file_name[ MAX_PATH_LEN ];
  KMD_HANDLE kmd = { 0 };
  int16_t use_kmd = 0;
  struct stat kmd_stat_buf;
  strcpy(kmd_file_name, pcm_file_name);
  strcpy(kmd_file_name + strlen(kmd_file_name) - 4, ".kmd");
  if (stat(kmd_file_name, &kmd_stat_buf) == 0) {
    FILE* fp_kmd = fopen(kmd_file_name, "r");
    kmd_init(&kmd, fp_kmd, full_screen, wait_vsync);
    fclose(fp_kmd);
    use_kmd = 1;
  } else {
    kmd_file_name[0] = '\0';
    use_kmd = 0;
  }

  // init chain tables
  CHAIN_TABLE* chain_tables = (CHAIN_TABLE*)himem_malloc(sizeof(CHAIN_TABLE) * MAX_CHAINS, 0);
  if (chain_tables == NULL) {
    printf("error: out of memory. (cannot allocate chain tables in main memory)\n");
    goto catch;
  }
  for (int16_t i = 0; i < num_chains; i++) {
    chain_tables[i].buffer = NULL;
    chain_tables[i].buffer_bytes = 0;
    chain_tables[i].next = &(chain_tables[ ( i + 1 ) % num_chains ]);
  }
  for (int16_t i = 0; i < num_chains; i++) {
    chain_tables[i].buffer = himem_malloc(CHAIN_TABLE_BUFFER_BYTES, 0);
    if (chain_tables[i].buffer == NULL) {
      printf("error: out of memory. (cannot allocate chain table buffer in main memory)\n");
      goto catch;      
    }
  }

  // encoder
  ADPCM_ENCODE_HANDLE adpcm_encoder = { 0 };

  // decoders
  WAV_DECODE_HANDLE wav_decoder = { 0 };
  RAW_DECODE_HANDLE raw_decoder = { 0 };
  YM2608_DECODE_HANDLE ym2608_decoder = { 0 };

  // init adpcm (msm6258v) encoder
  if (adpcm_encode_init(&adpcm_encoder, playback_volume) != 0) {
    printf("error: ADPCM encoder initialization error.\n");
    goto catch;
  }

  // init raw pcm decoder if needed
  if (input_format == FORMAT_RAW) {
    if (raw_decode_init(&raw_decoder, pcm_freq, pcm_channels, use_little_endian) != 0) {
      printf("error: PCM decoder initialization error.\n");
      goto catch;
    }
  }

  // init adpcm (ym2608) decoder if needed
  if (input_format == FORMAT_YM2608) {
    if (ym2608_decode_init(&ym2608_decoder, pcm_freq * pcm_channels * 4, pcm_freq, pcm_channels) != 0) {
      printf("error: YM2608 adpcm decoder initialization error.\n");
      goto catch;
    }
  }

  // init wav decoder if needed
  if (input_format == FORMAT_WAV) {
    if (wav_decode_init(&wav_decoder) != 0) {
      printf("error: WAV decoder initialization error.\n");
      goto catch;
    }
  }

  // KMD artwork
  int16_t kmd_artwork = 0;
  if (pic_brightness > 0 && use_kmd && kmd.tag_artwork[0] != '\0') {
    printf("\rloading KMD album artwork...");
    static uint8_t artwork_file_name[ MAX_PATH_LEN ];
    artwork_file_name[0] = '\0';
    if (jstrchr(kmd.tag_artwork, '\\') != NULL || jstrchr(kmd.tag_artwork, '/') != NULL || jstrchr(kmd.tag_artwork, ':') != NULL) {
      strcpy(artwork_file_name, kmd.tag_artwork);
    } else {
      strcpy(artwork_file_name, kmd_file_name);
      uint8_t* c = jstrrchr(artwork_file_name, '\\');
      if (c == NULL) c = jstrrchr(artwork_file_name, '/');
      if (c == NULL) c = jstrrchr(artwork_file_name, ':');
      if (c != NULL) {
        strcpy(c + 1, kmd.tag_artwork);
      } else {
        strcpy(artwork_file_name, kmd.tag_artwork);
      }
    }
    FILE* fp_art = fopen(artwork_file_name, "rb");
    if (fp_art != NULL) {
      fseek(fp_art, 0, SEEK_END);
      size_t pic_data_len = ftell(fp_art);
      fseek(fp_art, 0, SEEK_SET);
      uint8_t* pic_data = (uint8_t*)himem_malloc(pic_data_len, 0);
      if (pic_data != NULL) {
        size_t read_len = 0;
        do {
          size_t len = fread(pic_data + read_len, sizeof(uint8_t), pic_data_len - read_len, fp_art);
          if (len == 0) break;
          read_len += len;
        } while (read_len < pic_data_len);
        if (read_len >= pic_data_len) {
          if (pic_data[0] == 0x42 && pic_data[1] == 0x4d) {
            BMP_DECODE_HANDLE bmp_decode;
            bmp_decode_init(&bmp_decode, pic_brightness, !full_screen);
            if (bmp_decode_exec(&bmp_decode, pic_data, pic_data_len) == 0) {
              SCROLL(0, 512-128, 0);
              SCROLL(1, 512-128, 0);
              SCROLL(2, 512-128, 0);
              SCROLL(3, 512-128, 0);
              struct TXFILLPTR txfil = { 2, 128, 0, 512, 512, 0x0000 };
              TXFILL(&txfil);
              kmd_artwork = 1;
            }
            bmp_decode_close(&bmp_decode);
#ifdef JPEG_SUPPORT
          } else if (pic_data[0] == 0xff && pic_data[1] == 0xd8) {
            JPEG_DECODE_HANDLE jpeg_decode;
            jpeg_decode_init(&jpeg_decode, pic_brightness, !full_screen);
            if (jpeg_decode_exec(&jpeg_decode, pic_data, pic_data_len) == 0) {
              SCROLL(0, 512-128, 0);
              SCROLL(1, 512-128, 0);
              SCROLL(2, 512-128, 0);
              SCROLL(3, 512-128, 0);
              struct TXFILLPTR txfil = { 2, 128, 0, 512, 512, 0x0000 };
              TXFILL(&txfil);
              kmd_artwork = 1;
            }
            jpeg_decode_close(&jpeg_decode);
#endif
          }
        }
        himem_free(pic_data, 0);
      }
      fclose(fp_art);
    }
    printf("\r\x1b[0K");
  }

  // open input file
  fp = fopen(pcm_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open input file (%s).\n", pcm_file_name);
    goto catch;
  }

  // read header part of WAV file
  size_t skip_offset = 0;
  if (input_format == FORMAT_WAV) {
    int32_t ofs = wav_decode_parse_header(&wav_decoder, fp);
    if (ofs < 0) {
      //printf("error: wav header parse error.\n");
      goto catch;
    }
    pcm_freq = wav_decoder.sample_rate;
    pcm_channels = wav_decoder.channels;
    skip_offset = ofs;
  }

  // check data content size
  fseek(fp, 0, SEEK_END);
  uint32_t pcm_data_size = ftell(fp) - skip_offset;
  fseek(fp, skip_offset, SEEK_SET);

  // allocate file read buffer
  //   mp3 ... full read
  //   pcm ... incremental (max 2 sec)
  size_t fread_buffer_len = 
//    input_format == FORMAT_MP3 ? 2 + pcm_data_size / sizeof(int16_t) : 
    input_format == FORMAT_YM2608 && (playback_driver == DRIVER_PCM8PP || playback_driver == DRIVER_PCM8A) ? CHAIN_TABLE_BUFFER_BYTES / 4 :
    pcm_freq * pcm_channels * 2;
  if (input_format != FORMAT_ADPCM) {   // ADPCM can be directly loaded to chain tables
//    fread_buffer = himem_malloc(fread_buffer_len * sizeof(int16_t), input_format == FORMAT_MP3 ? use_high_memory : 0);
    fread_buffer = himem_malloc(fread_buffer_len * sizeof(int16_t), 0);
    if (fread_buffer == NULL) {
      printf("\rerror: file read buffer memory allocation error.\n");
      goto catch;
    }
  }

  // describe PCM attributes
  if (first_play) {

    printf("\n");

    printf("File name     : %s\n", pcm_file_name);
    printf("Data size     : %d [bytes]\n", pcm_data_size);
    printf("Data format   : %s\n", 
      input_format == FORMAT_WAV ? "WAV" :
      input_format == FORMAT_YM2608 ? "ADPCM(YM2608)" :
      input_format == FORMAT_RAW && !use_little_endian ? "16bit signed raw PCM (big)" : 
      input_format == FORMAT_RAW &&  use_little_endian ? "16bit signed raw PCM (little)" :
      "ADPCM(MSM6258V)");

    // describe playback drivers
    printf("PCM driver    : %s\n",
      playback_driver == DRIVER_PCM8PP ? "PCM8PP" :
      playback_driver == DRIVER_PCM8A  ? "PCM8A"  :
      "S44EXP");
  
    if (input_format == FORMAT_ADPCM) {
      float pcm_1sec_size = pcm_freq * 0.5;
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", "mono");
      printf("PCM length    : %4.2f [sec]\n", (float)pcm_data_size / pcm_1sec_size);
      if (use_kmd) {
        if (kmd.tag_title[0]  != '\0') printf("KMD title     : %s\n", kmd.tag_title);
        if (kmd.tag_artist[0] != '\0') printf("KMD artist    : %s\n", kmd.tag_artist);
        if (kmd.tag_album[0]  != '\0') printf("KMD album     : %s\n", kmd.tag_album);
      }
    }

    if (input_format == FORMAT_RAW) {
      float pcm_1sec_size = pcm_freq * 2;
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");
      printf("PCM length    : %4.2f [sec]\n", (float)pcm_data_size / pcm_channels / pcm_1sec_size);
      if (use_kmd) {
        if (kmd.tag_title[0]  != '\0') printf("KMD title     : %s\n", kmd.tag_title);
        if (kmd.tag_artist[0] != '\0') printf("KMD artist    : %s\n", kmd.tag_artist);
        if (kmd.tag_album[0]  != '\0') printf("KMD album     : %s\n", kmd.tag_album);
      }
    }

    if (input_format == FORMAT_YM2608) {
      float pcm_1sec_size = pcm_freq * 0.5;
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");
      printf("PCM length    : %4.2f [sec]\n", (float)pcm_data_size / pcm_channels / pcm_1sec_size);
      if (use_kmd) {
        if (kmd.tag_title[0]  != '\0') printf("KMD title     : %s\n", kmd.tag_title);
        if (kmd.tag_artist[0] != '\0') printf("KMD artist    : %s\n", kmd.tag_artist);
        if (kmd.tag_album[0]  != '\0') printf("KMD album     : %s\n", kmd.tag_album);
      }
    }

    if (input_format == FORMAT_WAV) {
      printf("PCM frequency : %d [Hz]\n", pcm_freq);
      printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");
      printf("PCM length    : %4.2f [sec]\n", (float)wav_decoder.duration / pcm_freq);
      if (use_kmd) {
        if (kmd.tag_title[0]  != '\0') printf("KMD title     : %s\n", kmd.tag_title);
        if (kmd.tag_artist[0] != '\0') printf("KMD artist    : %s\n", kmd.tag_artist);
        if (kmd.tag_album[0]  != '\0') printf("KMD album     : %s\n", kmd.tag_album);
      }
    }

    printf("\n");

    first_play = 0;
  }

  // initial buffering
  int16_t end_flag = 0;
  for (int16_t i = 0; i < num_chains; i++) {

    if (end_flag) break;

    // check esc key to exit
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC || scan_code == KEY_SCAN_CODE_Q) {
        printf("\rcanceled.\x1b[0K");
        rc = 1;
        goto catch;
      }
    }

    printf("\rnow buffering (%d/%d) on %s ...", i+1, num_chains, 
      use_high_memory ? "high memory and main memory" : "main memory");

    if (playback_driver == DRIVER_PCM8PP) {
      
      if (input_format == FORMAT_ADPCM) {

        // ADPCM(MSM6258V) with PCM8PP
        size_t fread_len = 0;
        do {
          size_t len = fread(chain_tables[i].buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
        if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        chain_tables[i].buffer_bytes = fread_len;
  
      } else if (input_format == FORMAT_RAW) {

        // raw signed 16bit PCM with PCM8PP
        if (!use_little_endian) {
          // big endian ... direct read
          size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
          size_t fread_len = 0;
          do {
            size_t len = fread(chain_tables[i].buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < buffer_len);
          if (fread_len < buffer_len) {
            chain_tables[i].next = NULL;
            end_flag = 1;
          }
          chain_tables[i].buffer_bytes = fread_len * sizeof(int16_t);
        } else {
          // little endian ... need endian conversion
          size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < buffer_len);
          if (fread_len < buffer_len) {
            chain_tables[i].next = NULL;
            end_flag = 1;
          }
          // endian conversion
          size_t resampled_len = 
            raw_decode_convert_endian(&raw_decoder, chain_tables[i].buffer, fread_buffer, fread_len);
          chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);
        }

      } else if (input_format == FORMAT_YM2608) {

        // ADPCM(YM2608) with PCM8PP
        size_t fread_len = 0;
        do { 
          size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t decoded_bytes =
          ym2608_decode_exec_buffer(&ym2608_decoder, fread_buffer, fread_len, 
            chain_tables[i].buffer, CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t)) * sizeof(int16_t);
        chain_tables[i].buffer_bytes = decoded_bytes;

      } else if (input_format == FORMAT_WAV) {

        // WAV with PCM8PP
        size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < buffer_len);
        if (fread_len < buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

        // resample as endian conversion (wav is little only)
        size_t resampled_len = 
          wav_decode_convert_endian(&wav_decoder, chain_tables[i].buffer, fread_buffer, fread_len);
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);

      }

    } else if (playback_driver == DRIVER_PCM8A) {
      
      if (input_format == FORMAT_ADPCM) {

        // ADPCM(MSM6258V) with PCM8A
        size_t fread_len = 0;
        do {
          size_t len = fread(chain_tables[i].buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
        if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        chain_tables[i].buffer_bytes = fread_len;
  
      } else if (input_format == FORMAT_RAW) {

        // raw signed 16bit PCM (resampled) with PCM8A
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

        // resample with endian conversion and gain adjustment (PCM8A requires 12bit PCM data)
        size_t resampled_len = 
          raw_decode_resample(&raw_decoder, chain_tables[i].buffer, adpcm_output_freq, fread_buffer, fread_len, 16);
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);

      } else if (input_format == FORMAT_YM2608) {

        // ADPCM(YM2608) resampled with PCM8A
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

        // atop into decoder internal buffer
        size_t decoded_len = ym2608_decode_exec(&ym2608_decoder, fread_buffer, fread_len);

        // resample to chain table buffer
        size_t resampled_bytes = ym2608_decode_resample(&ym2608_decoder, chain_tables[i].buffer, adpcm_output_freq, 16) * sizeof(int16_t);
        chain_tables[i].buffer_bytes = resampled_bytes;

      } else if (input_format == FORMAT_WAV) {

        // WAV (resampled) with PCM8A
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp); 
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len); 
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        // resample and endian conversion
        size_t resampled_len = 
          wav_decode_resample(&wav_decoder, chain_tables[i].buffer, adpcm_output_freq, fread_buffer, fread_len, 16);
        chain_tables[i].buffer_bytes = resampled_len * sizeof(int16_t);

      }

    } else {

      if (input_format == FORMAT_ADPCM) {

        // ADPCM(MSM6258V)
        size_t fread_len = 0;
        do {
          size_t len = fread(chain_tables[i].buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
        if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        chain_tables[i].buffer_bytes = fread_len;

      } else if (input_format == FORMAT_RAW) {

        // raw signed 16bit PCM (resampled)
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp); 
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len); 
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t resampled_len = 
          adpcm_encode_resample(&adpcm_encoder, chain_tables[i].buffer, adpcm_output_freq, fread_buffer, fread_len, pcm_freq, pcm_channels, use_little_endian);
        chain_tables[i].buffer_bytes = resampled_len;

      } else if (input_format == FORMAT_YM2608) {

        // ADPCM(YM2608) resampled
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp); 
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len); 
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }

        // atop into decoder internal buffer
        size_t decoded_len = ym2608_decode_exec(&ym2608_decoder, fread_buffer, fread_len);

        // resample to chain table buffer
        size_t resampled_len = 
          adpcm_encode_resample(&adpcm_encoder, chain_tables[i].buffer, adpcm_output_freq, ym2608_decoder.decode_buffer, decoded_len, pcm_freq, pcm_channels, 0);
        chain_tables[i].buffer_bytes = resampled_len;

      } else if (input_format == FORMAT_WAV) {

        // WAV (resampled)
        size_t fread_len = 0;
        do {
          size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);  
          if (len == 0) break;
          fread_len += len;
        } while (fread_len < fread_buffer_len);
        if (fread_len < fread_buffer_len) {
          chain_tables[i].next = NULL;
          end_flag = 1;
        }
        size_t resampled_len = 
          adpcm_encode_resample(&adpcm_encoder, chain_tables[i].buffer, adpcm_output_freq, fread_buffer, fread_len, pcm_freq, pcm_channels, 1);
        chain_tables[i].buffer_bytes = resampled_len;

      }

    }

  }

#ifdef DEBUG
  for (int16_t i = 0; i < num_chains; i++) {
    printf("chain_tables[%d] self=%X,buffer=%X,buffer_bytes=%d,next=%X\n", i, &(chain_tables[i]), chain_tables[i].buffer, chain_tables[i].buffer_bytes, chain_tables[i].next);
  }
#endif

  // start playing
  if (playback_driver == DRIVER_PCM8PP) {

    int16_t pcm8pp_volume = playback_volume;
    int16_t pcm8pp_pan = 0x03;
    int16_t pcm8pp_freq = pcm_freq == 16000 && pcm_channels == 1 ? 0x09 :
                          pcm_freq == 22050 && pcm_channels == 1 ? 0x0a :
                          pcm_freq == 24000 && pcm_channels == 1 ? 0x0b :
                          pcm_freq == 32000 && pcm_channels == 1 ? 0x0c :
                          pcm_freq == 44100 && pcm_channels == 1 ? 0x0d :
                          pcm_freq == 48000 && pcm_channels == 1 ? 0x0e :
                          (pcm_freq == 8000 || pcm_freq == 11025 || pcm_freq == 12000) && pcm_channels == 1 ? 0x0f :
                          pcm_freq == 16000 && pcm_channels == 2 ? 0x19 :
                          pcm_freq == 22050 && pcm_channels == 2 ? 0x1a :
                          pcm_freq == 24000 && pcm_channels == 2 ? 0x1b :
                          pcm_freq == 32000 && pcm_channels == 2 ? 0x1c :
                          pcm_freq == 44100 && pcm_channels == 2 ? 0x1d :
                          pcm_freq == 48000 && pcm_channels == 2 ? 0x1e :
                          (pcm_freq == 8000 || pcm_freq == 11025 || pcm_freq == 12000) && pcm_channels == 2 ? 0x1f :
                          adpcm_output_freq < 8000  ? 0x02 :
                          adpcm_output_freq < 11000 ? 0x03 : 0x04;
    uint32_t pcm8pp_channel_mode = ( pcm8pp_volume << 16 ) | ( pcm8pp_freq << 8 ) | pcm8pp_pan;
    pcm8pp_play_linked_array_chain(0, pcm8pp_channel_mode, 1, pcm_freq * 256, &(chain_tables[0]));

  } else if (playback_driver == DRIVER_PCM8A) {

    if (input_format == FORMAT_ADPCM) {

      // disable PCM8A polyphonic mode and use IOCS. Otherwise buffer flip cannot be detected correctly.
      pcm8a_set_polyphonic_mode(0);  

      // use IOCS ADPCM
      int16_t freq = adpcm_output_freq < 8000  ? 2 :
                     adpcm_output_freq < 11000 ? 3 : 4;
      int32_t iocs_adpcm_mode = freq * 256 + 3;

      ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), iocs_adpcm_mode);

    } else {

      // must use polyphonic mode for 16bit PCM use
      pcm8a_set_polyphonic_mode(1);   
  
      int16_t pcm8a_volume = playback_volume;
      int16_t pcm8a_pan = 0x03;
      int16_t pcm8a_freq = adpcm_output_freq < 8000  ? ( input_format == FORMAT_ADPCM ? 0x02 : 0x12 ) :
                           adpcm_output_freq < 11000 ? ( input_format == FORMAT_ADPCM ? 0x03 : 0x13 ) : 
                                                       ( input_format == FORMAT_ADPCM ? 0x04 : 0x14 ) ;
      uint32_t pcm8a_channel_mode = ( pcm8a_volume << 16 ) | ( pcm8a_freq << 8 ) | pcm8a_pan;
      pcm8a_play_linked_array_chain(0, pcm8a_channel_mode, &(chain_tables[0]));

    }

  } else {

    if (pcm8_type == PCM8_TYPE_PCM8) {
      // disable PCM8 polyphonic mode and use IOCS. Otherwise buffer flip cannot be detected.
      pcm8_set_polyphonic_mode(0);    
    }

    // IOCS ADPCM mode
    int16_t freq = adpcm_output_freq < 8000  ? 2 :
                   adpcm_output_freq < 11000 ? 3 : 4;
    int32_t iocs_adpcm_mode = freq * 256 + 3;
    ADPCMLOT((struct CHAIN2*)(&chain_tables[0]), iocs_adpcm_mode);

  }

  B_PRINT("\rnow playing ... push [ESC]/[Q] key to quit. [SPACE] to pause. [RIGHT] to skip.\x1b[0K");
  int16_t paused = 0;
  uint32_t pause_time;

  // for kmd
  uint32_t play_start_time = ONTIME() * 10;
  if (use_kmd) {
    B_PRINT("\n\n");
    kmd_preserve_cursor_position(&kmd);
  }

  // dummy wait to make sure DMAC start (500 msec)
  for (int32_t t0 = ONTIME(); ONTIME() < t0 + 50;) {}

  int16_t current_chain = 0;
  int32_t pcm8pp_block_counter = 0;
  if (pcm8_type == PCM8_TYPE_PCM8PP) {
    pcm8pp_block_counter = pcm8pp_get_block_counter(0);
  }

  // for X68000Z DMAC BAR workaround
  void* dmac_bar = (void*)B_LPEEK((uint32_t*)REG_DMAC_CH3_BAR); 

  for (;;) {
   
    // check esc key to exit, space key to pause
    if (B_KEYSNS() != 0) {
      int16_t scan_code = B_KEYINP() >> 8;
      if (scan_code == KEY_SCAN_CODE_ESC || scan_code == KEY_SCAN_CODE_Q) {
        if (use_kmd) B_PRINT("\n\n");
        B_PRINT("\rstopped.\x1b[0K");
        rc = 1;
        break;
      } else if (scan_code == KEY_SCAN_CODE_RIGHT) {
        if (use_kmd) B_PRINT("\n\n");
        B_PRINT("\rskipped.\x1b[0K\n");
        rc = 2;
        break;
      } else if (playback_index > 0 && scan_code == KEY_SCAN_CODE_LEFT) {
        if (use_kmd) B_PRINT("\n\n");
        B_PRINT("\rbacked.\x1b[0K\n");
        rc = 3;
        break;
      } else if (scan_code == KEY_SCAN_CODE_SPACE) {
        if (paused) {
          if (playback_driver == DRIVER_PCM8PP) {
            pcm8pp_resume();
          } else if (playback_driver == DRIVER_PCM8A && input_format != FORMAT_ADPCM) {
            pcm8a_resume();
          } else {
            ADPCMMOD(2);
          }
          paused = 0;
          play_start_time += ONTIME() - pause_time - 50;   // adjust for KMD
        } else {
          if (playback_driver == DRIVER_PCM8PP) {
            pcm8pp_pause();
          } else if (playback_driver == DRIVER_PCM8A && input_format != FORMAT_ADPCM) {
            pcm8a_pause();
          } else {
            ADPCMMOD(1);
          }
          paused = 1;
          pause_time = ONTIME();
        }
      }
    }

    if (paused) continue;

    // exit if not playing
    if (playback_driver == DRIVER_PCM8PP) {
      if (pcm8pp_get_data_length(0) == 0) {
      //if (B_BPEEK(REG_DMAC_CH2_CSR) & 0x80) {   // ch2 dmac operation complete?
        if (end_flag) { 
          if (use_kmd) {
            kmd_clear_messages(&kmd);
          }
          B_PRINT("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          B_PRINT("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    } else if (playback_driver == DRIVER_PCM8A) { //} && input_format != FORMAT_ADPCM) {
      if (pcm8a_get_data_length(0) == 0) {
        if (end_flag) { 
          if (use_kmd) {
            kmd_clear_messages(&kmd);
          }
          B_PRINT("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          B_PRINT("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    } else {
      if (ADPCMSNS() == 0) {
        if (end_flag) {
          if (use_kmd) {
            kmd_clear_messages(&kmd);
          }
          B_PRINT("\rfinished.\x1b[0K");
          rc = 0;
        } else {
          B_PRINT("\rerror: buffer underrun detected.\x1b[0K");
          rc = 1;
        }
        break;
      }
    }

    // kmd display
    if (use_kmd) {
      uint32_t elapsed_msec = ONTIME() * 10 - play_start_time + 30;
      kmd_deactivate_events(&kmd, elapsed_msec);
      kmd_activate_current_event(&kmd, elapsed_msec);
    }

    // check buffer flip
    CHAIN_TABLE* cta = &(chain_tables[ current_chain ]);
    CHAIN_TABLE* ctb = &(chain_tables[ (current_chain - 1 + num_chains) % num_chains ]);
    int16_t buffer_flip = 0;
    if (playback_driver == DRIVER_PCM8PP) {
      int32_t bc = pcm8pp_get_block_counter(0);
      if (bc != pcm8pp_block_counter) {
        buffer_flip = 1;
        pcm8pp_block_counter = bc;
      }
    } else if (playback_driver == DRIVER_PCM8A) {
      void* cur_pcm8a_addr = pcm8a_get_access_address(0);
      if (cur_pcm8a_addr < cta->buffer || cur_pcm8a_addr >= cta->buffer + cta->buffer_bytes) {
        buffer_flip = 1;
#ifdef DEBUG
        printf("pcm8a=%X, ct0 buffer=%X - %X, ct1 buffer=%X - %X, ct2 buffer=%X - %X, ct3 buffer=%X - %X\n", cur_pcm8a_addr, 
        chain_tables[0].buffer, chain_tables[0].buffer + chain_tables[0].buffer_bytes,
        chain_tables[1].buffer, chain_tables[1].buffer + chain_tables[1].buffer_bytes,
        chain_tables[2].buffer, chain_tables[2].buffer + chain_tables[2].buffer_bytes, 
        chain_tables[3].buffer, chain_tables[3].buffer + chain_tables[3].buffer_bytes);
#endif
      }

    } else {
      void* cur_dmac_bar = (void*)B_LPEEK((uint32_t*)REG_DMAC_CH3_BAR);     // = next chain table pointer
//      if (cur_dmac_bar != cta->next) {    // this does not work with X68000Z EAK 1.13
      if (cur_dmac_bar != dmac_bar) {       // for X68000Z EAK 1.13 workaround
        buffer_flip = 1;
        dmac_bar = cur_dmac_bar;
#ifdef DEBUG
        printf("cur_bar=%X, cta->next=%X\n", cur_dmac_bar, cta->next);
#endif
      }
    }

    // process additional data if buffer flip happens
    if (!end_flag && buffer_flip) {

#ifdef DEBUG
      printf("buffer flip (current chain = %d)\n", current_chain);
#endif
      // cut link tantatively
      void* orig_ctb_next = ctb->next;
      ctb->next = NULL;

      if (playback_driver == DRIVER_PCM8PP) {

        if (input_format == FORMAT_ADPCM) {

          // ADPCM(MSM6258V) with PCM8PP
          size_t fread_len = 0;
          do {
            size_t len = fread(cta->buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
          if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
            cta->next = NULL;
            end_flag = 1;
          }
          cta->buffer_bytes = fread_len;
    
        } else if (input_format == FORMAT_RAW) {

          // raw signed 16bit PCM with PCM8PP
          if (!use_little_endian) {
            // big endian ... direct read
            size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
            size_t fread_len = 0;
            do {
              size_t len = fread(cta->buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);
//              printf("pcm8pp_block_counter=%d, current_chain=%d, len=%d, fread_len=%d, elapsed=%d\n",pcm8pp_block_counter,current_chain,len,fread_len,ONTIME()*10 - play_start_time);
              if (len == 0) break;
              fread_len += len;
            } while (fread_len < buffer_len);
            if (fread_len < buffer_len) {
              cta->next = NULL;
              end_flag = 1;
            }
            cta->buffer_bytes = fread_len * sizeof(int16_t);
          } else {
            // little endian ... need endian conversion with resampling method
            size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
            size_t fread_len = 0;
            do {
              size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);
              if (len == 0) break;
              fread_len += len;
            } while (fread_len < buffer_len);
            if (fread_len < buffer_len) {
              cta->next = NULL;
              end_flag = 1;
            }
            // endian conversion
            size_t resampled_len = 
              raw_decode_convert_endian(&raw_decoder, cta->buffer, fread_buffer, fread_len);
            cta->buffer_bytes = resampled_len * sizeof(int16_t);
          }

        } else if (input_format == FORMAT_YM2608) {

          // ADPCM(YM2608) with PCM8PP
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          size_t decoded_bytes =
            ym2608_decode_exec_buffer(&ym2608_decoder, fread_buffer, fread_len, 
              cta->buffer, CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t)) * sizeof(int16_t);
          cta->buffer_bytes = decoded_bytes;

        } else if (input_format == FORMAT_WAV) {

          // WAV with PCM8PP
          size_t buffer_len = CHAIN_TABLE_BUFFER_BYTES / sizeof(int16_t);
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < buffer_len);
          if (fread_len < buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          // endian conversion (wav is little only)
          size_t resampled_len = 
            wav_decode_convert_endian(&wav_decoder, cta->buffer, fread_buffer, fread_len);
          cta->buffer_bytes = resampled_len * sizeof(int16_t);

        }

      } else if (playback_driver == DRIVER_PCM8A) {

        if (input_format == FORMAT_ADPCM) {

          // ADPCM(MSM6258V)
          size_t fread_len = 0;
          do {
            size_t len = fread(cta->buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
          if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
            cta->next = NULL;
            end_flag = 1;
          }
          cta->buffer_bytes = fread_len;

        } else if (input_format == FORMAT_RAW) {

          // raw signed 16bit PCM (resampled) with PCM8A
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          size_t resampled_len = 
            raw_decode_resample(&raw_decoder, cta->buffer, adpcm_output_freq, (int16_t*)fread_buffer, fread_len, 16);
          cta->buffer_bytes = resampled_len * sizeof(int16_t);

        } else if (input_format == FORMAT_YM2608) {

          // ADPCM(YM2608) resampled with PCM8A
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          // atop into decoder internal buffer
          size_t decoded_bytes = ym2608_decode_exec(&ym2608_decoder, fread_buffer, fread_len);

          // resample to chain table buffer
          size_t resampled_bytes = ym2608_decode_resample(&ym2608_decoder, cta->buffer, adpcm_output_freq, 16) * sizeof(int16_t);
          cta->buffer_bytes = resampled_bytes;

        } else if (input_format == FORMAT_WAV) {

          // WAV (resampled) with PCM8A
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          size_t resampled_len = 
            wav_decode_resample(&wav_decoder, cta->buffer, adpcm_output_freq, (int16_t*)fread_buffer, fread_len, 16);
          cta->buffer_bytes = resampled_len * sizeof(int16_t);

        }

      } else {

        if (input_format == FORMAT_ADPCM) {

          // ADPCM(MSM6258V)
          size_t fread_len = 0;
          do {
            size_t len = fread(cta->buffer + fread_len, sizeof(uint8_t), CHAIN_TABLE_BUFFER_BYTES - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < CHAIN_TABLE_BUFFER_BYTES);
          if (fread_len < CHAIN_TABLE_BUFFER_BYTES) {
            cta->next = NULL;
            end_flag = 1;
          }
          cta->buffer_bytes = fread_len;

        } else if (input_format == FORMAT_RAW) {

          // raw signed 16bit PCM (resampled)
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          size_t resampled_len = 
            adpcm_encode_resample(&adpcm_encoder, cta->buffer, adpcm_output_freq, fread_buffer, fread_len, pcm_freq, pcm_channels, use_little_endian);
          cta->buffer_bytes = resampled_len;

        } else if (input_format == FORMAT_YM2608) {

          // ADPCM(YM2608) resampled
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(uint8_t), fread_buffer_len - fread_len, fp);
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }

          // atop into decoder internal buffer
          size_t decoded_len = ym2608_decode_exec(&ym2608_decoder, fread_buffer, fread_len);

          // resample to chain table buffer
          size_t resampled_len = 
            adpcm_encode_resample(&adpcm_encoder, cta->buffer, adpcm_output_freq, ym2608_decoder.decode_buffer, decoded_len, pcm_freq, pcm_channels, 0);
          cta->buffer_bytes = resampled_len;

        } else if (input_format == FORMAT_WAV) {

          // WAV (resampled)
          size_t fread_len = 0;
          do {
            size_t len = fread(fread_buffer + fread_len, sizeof(int16_t), fread_buffer_len - fread_len, fp);  
            if (len == 0) break;
            fread_len += len;
          } while (fread_len < fread_buffer_len);
          if (fread_len < fread_buffer_len) {
            cta->next = NULL;
            end_flag = 1;
          }
          size_t resampled_len = 
            adpcm_encode_resample(&adpcm_encoder, cta->buffer, adpcm_output_freq, fread_buffer, fread_len, pcm_freq, pcm_channels, 1);
          cta->buffer_bytes = resampled_len;

        }

      }

      // resume link
      ctb->next = orig_ctb_next;

      // increment focus chain
      current_chain = ( current_chain + 1 ) % num_chains;

    }

  }

catch:

  // dummy wait to make sure DMAC stop (200 msec)
  for (int32_t t0 = ONTIME(); ONTIME() < t0 + 20;) {}

  // reset driver
  if (playback_driver == DRIVER_PCM8PP) {
    pcm8pp_pause();
    pcm8pp_stop();
  } else if (playback_driver == DRIVER_PCM8A) {
    pcm8a_pause();
    pcm8a_stop();
  } else {
    ADPCMMOD(0);
  }

  // close input file
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  // reclaim file read buffers
  if (fread_staging_buffer != NULL) {
    himem_free(fread_staging_buffer, 0);
    fread_staging_buffer = NULL;
  }
  if (fread_buffer != NULL) {
//    himem_free(fread_buffer, input_format == FORMAT_MP3 ? use_high_memory : 0);
    himem_free(fread_buffer, 0);
    fread_buffer = NULL;
  }

  // close adpcm encoder
  adpcm_encode_close(&adpcm_encoder);

  // close raw decoder
  if (input_format == FORMAT_RAW) {
    raw_decode_close(&raw_decoder);
  }

  // close ym2608 decoder
  if (input_format == FORMAT_YM2608) {
    ym2608_decode_close(&ym2608_decoder);
  }

  // close wav decoder
  if (input_format == FORMAT_WAV) {
    wav_decode_close(&wav_decoder);
  }

  // enable pcm8 polyphonic mode
  if (pcm8_type != PCM8_TYPE_NONE) {
    if (pcm8_set_polyphonic_mode(-1) == 0) {
      pcm8_set_polyphonic_mode(1);
    }
  }

  // reclaim chain table buffers
  for (int16_t i = 0; i < num_chains; i++) {
    if (chain_tables[i].buffer != NULL) {
      himem_free(chain_tables[i].buffer, 0);
    }
  }
  himem_free(chain_tables, 0);

  // close kmd handle
  if (use_kmd) {
    kmd_close(&kmd);
  }

  // loop check
  if (rc == 0 || rc == 2) {
    if (num_pcm_files > 1) {
      // playlist mode
      playback_index++;
      if (playback_index < num_pcm_files) {
        first_play = 1;
        goto loop;
      }
      if (loop_count == 0 || --loop_count > 0) {
        playback_index = 0;
        first_play = 1;
        goto loop;
      }
    } else {
      // single play mode
      if (loop_count == 0 || --loop_count > 0) {
        goto loop;
      }
    }
  } else if (rc == 3) {
    playback_index--;
    first_play = 1;
    goto loop;
  }

  B_PRINT("\r\n");

exit:

  // reclaim indirect file buffer
  if (indirect_file_names != NULL) {
    himem_free(indirect_file_names, 0);
    indirect_file_names = NULL;
  }

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

  // reset scroll position
  if (pic_brightness > 0) {
    SCROLL(0, 0, 0);
    SCROLL(1, 0, 0);
    SCROLL(2, 0, 0);
    SCROLL(3, 0, 0);
    struct TXFILLPTR txfil = { 2, 0, 0, 768, 512, 0x0000 };
    TXFILL(&txfil);
    TPALET2(4,-2);
    TPALET2(5,-2);
    TPALET2(6,-2);
    TPALET2(7,-2);
  }

  // screen clear
  if (full_screen && clear_screen) {
    C_CLS_AL();
    G_CLR_ON();
  }

  // cursor on
  C_CURON();

  // function key mode
  if (g_funckey_mode >= 0) {
    C_FNKMOD(g_funckey_mode);
  }

  // resume abort vectors
  INTVCS(0xFFF1, (int8_t*)abort_vector1);
  INTVCS(0xFFF2, (int8_t*)abort_vector2);  

  return rc;
}
