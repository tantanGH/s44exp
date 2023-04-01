#ifndef __H_KMD__
#define __H_KMD__

#include <stdint.h>
#include <stddef.h>

#define KMD_POS_X_MIN (0)
#define KMD_POS_X_MAX (30)
#define KMD_POS_Y_MIN (0)
#define KMD_POS_Y_MAX (2)
#define KMD_MAX_MESSAGE_LEN (62)
#define KMD_MAX_LINE_LEN (256)

typedef struct {
  int16_t pos_x;
  int16_t pos_y;
  int16_t active;
  uint32_t start_msec;
  uint32_t end_msec;
  uint8_t message[ KMD_MAX_MESSAGE_LEN + 1 ];
} KMD_EVENT;

typedef struct {
  int16_t large;
  int16_t vsync;
  int16_t cursor_pos_y;
  size_t current_event_ofs;
  size_t num_events;
  KMD_EVENT* events;
  uint8_t tag_title[ KMD_MAX_MESSAGE_LEN + 1 ];
  uint8_t tag_artist[ KMD_MAX_MESSAGE_LEN + 1 ];
  uint8_t tag_album[ KMD_MAX_MESSAGE_LEN + 1 ];
  uint8_t tag_artwork[ KMD_MAX_MESSAGE_LEN + 1 ];
} KMD_HANDLE;

int32_t kmd_init(KMD_HANDLE* kmd, FILE* fp, int16_t large, int16_t vsync);
void kmd_close(KMD_HANDLE* kmd);
void kmd_preserve_cursor_position(KMD_HANDLE* kmd);
KMD_EVENT* kmd_next_event(KMD_HANDLE* kmd);
void kmd_print_event_message(KMD_HANDLE* kmd, KMD_EVENT* event);
void kmd_erase_event_message(KMD_HANDLE* kmd, KMD_EVENT* event);
void kmd_clear_messages(KMD_HANDLE* kmd);
void kmd_deactivate_events(KMD_HANDLE* kmd, uint32_t elapsed);
void kmd_activate_current_event(KMD_HANDLE* kmd, uint32_t elapsed);

#endif