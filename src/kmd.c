#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <jstring.h>
#include <iocslib.h>
#include "himem.h"
#include "crtc.h"
#include "kmd.h"

static uint8_t BLANKS[] = "                                                                                                      ";

//
//  initialize kmd handle
//
int32_t kmd_init(KMD_HANDLE* kmd, FILE* fp, int16_t large, int16_t vsync) {

  // default return code
  int32_t rc = -1;

  // reset attributes
  if (kmd == NULL) goto exit;
  kmd->large = large;
  kmd->vsync = vsync;
  kmd->cursor_pos_y = -1;
  kmd->current_event_ofs = 0;
  kmd->num_events = 0;
  kmd->events = NULL;
  kmd->tag_title[0] = '\0';
  kmd->tag_artist[0] = '\0';
  kmd->tag_album[0] = '\0';
  kmd->tag_artwork[0] = '\0';

  // KMD file header check
  if (fp == NULL) goto exit;
  static uint8_t line[ KMD_MAX_LINE_LEN ];
  if (fgets(line, KMD_MAX_LINE_LEN, fp) == NULL) goto exit;
  if (memcmp(line, "KMD100", 6) != 0) goto exit;

  // pass 1 - count events
  size_t num_events = 0;
  while (fgets(line, KMD_MAX_LINE_LEN, fp) != NULL) {
    if (line[0] == 'x') num_events++;
  }

  // allocate the necessary memory
  kmd->num_events = num_events;
  kmd->events = (KMD_EVENT*)himem_malloc(sizeof(KMD_EVENT) * kmd->num_events, 0);
  if (kmd->events == NULL) goto exit;

  // pass 2 - read events
  size_t i = 0;
  fseek(fp, 0, SEEK_SET);
  while (fgets(line, KMD_MAX_LINE_LEN, fp) != NULL) {
    if (line[0] == 'x') {
      KMD_EVENT* e = &(kmd->events[i]);
      int16_t x,y,s0,s1,s2,e0,e1,e2;
      if (sscanf(line, "x%hd,y%hd,s%hd:%hd:%hd,e%hd:%hd:%hd,", &x, &y, &s0, &s1, &s2, &e0, &e1, &e2) == 8) {
        uint8_t* m0 = jstrchr(line,'"');
        uint8_t* m1 = jstrrchr(line,'"');
        if (m0 != NULL && m1 != NULL && m0 < m1) {
          size_t m_len = m1 - m0 - 1;
          if (m_len > KMD_MAX_MESSAGE_LEN) m_len = KMD_MAX_MESSAGE_LEN;
          if (s0 == 99 && s1 == 59 && s2 == 99 && e0 == 99 && e1 == 59 && e2 == 99) {
            if (memcmp(m0 + 1, "TIT2:", 5) == 0) {
              memcpy(kmd->tag_title, m0 + 6, m_len - 5);
              kmd->tag_title[ m_len ] = '\0';
            } else if (memcmp(m0 + 1, "TPE1:", 5) == 0) {
              memcpy(kmd->tag_artist, m0 + 6, m_len - 5);
              kmd->tag_artist[ m_len ] = '\0';
            } else if (memcmp(m0 + 1, "TALB:", 5) == 0) {
              memcpy(kmd->tag_album, m0 + 6, m_len - 5);
              kmd->tag_album[ m_len ] = '\0';
            } else if (memcmp(m0 + 1, "APIC:", 5) == 0) {
              memcpy(kmd->tag_artwork, m0 + 6, m_len - 5);
              kmd->tag_artwork[ m_len ] = '\0';
            }
          } else {
            e->pos_x = x;
            e->pos_y = y;
            e->active = 0;
            e->start_msec = s0 * 60000 + s1 * 1000 + s2 * 10;
            e->end_msec = e0 * 60000 + e1 * 1000 + e2 * 10;
            memcpy(e->message, m0 + 1, m_len);
            e->message[ m_len ] = '\0';
            i++;
          }
        }
      }
    }
  }

  kmd->num_events = i;

  rc = 0;

exit:
  return rc;
}

//
//  close kmd handle
//
void kmd_close(KMD_HANDLE* kmd) {
  // reclaim buffer
  if (kmd->events != NULL) {
    himem_free(kmd->events, 0);
    kmd->events = NULL;
  }
}

//
//  preserve cursor Y pos
//
void kmd_preserve_cursor_position(KMD_HANDLE* kmd) {
  kmd->cursor_pos_y = B_LOCATE(-1,-1) & 0xff;
}

// put text in 12x24/24x24 font
static void put_text24(uint16_t x, uint16_t y, uint16_t color, const uint8_t* text) {

  static struct FNTBUF fbs[ KMD_MAX_MESSAGE_LEN + 1 ];

  int16_t len = strlen(text);
  int16_t p = 0;

  for (int16_t i = 0; i < len && i < KMD_MAX_MESSAGE_LEN; i++) {

    struct FNTBUF* fb = &(fbs[p]);

    // SJIS code?
    uint16_t code = text[i];
    if (i < len-1 && (text[i] >= 0x81 && text[i] <= 0x9f) || (text[i] >= 0xe0 && text[i] <= 0xfc)) {
      code = text[i] * 256 + text[i+1];
      i++;
    }

    FNTGET(12, code, fb);
    if (fb->xl == 0) fb->xl = (code < 0x100) ? 12 : 24;      // IOCS bug?

    p++;
  }

  uint16_t fx = x;
  uint16_t fy = y;

  for (int16_t i = 0; i < p; i++) {

    struct FNTBUF* fb = &(fbs[i]);

    if (color & 0x01) {
      TCOLOR(1);
      TEXTPUT(fx, fy, fb);
    }
    if (color & 0x02) {
      TCOLOR(2);
      TEXTPUT(fx, fy, fb);
    }

    fx += fb->xl;
    if (fx >= 768) break;

  }
}

//
//  get next kmd event
//
KMD_EVENT* kmd_next_event(KMD_HANDLE* kmd) {
  if (kmd == NULL || kmd->events == NULL || kmd->current_event_ofs >= kmd->num_events) return NULL;
  KMD_EVENT* next_event = &(kmd->events[ kmd->current_event_ofs ]);
  kmd->current_event_ofs++;
  return next_event;
}

//
//  erase event message
//
void kmd_erase_event_message(KMD_HANDLE* kmd, KMD_EVENT* event) {
  static uint8_t xs[128];
  if (kmd != NULL && event != NULL) {
    if (kmd->large) {
      put_text24(16 + event->pos_x * 24, 16 + kmd->cursor_pos_y * 16 + event->pos_y * 16, 3, BLANKS + strlen(BLANKS) - strlen(event->message));
    } else {
      if (event->pos_y == 0) {
        sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x * 2 + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
        B_PRINT(xs);
      } else if (event->pos_y == 1) {
        sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x * 2 + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
        B_PRINT(xs);
      } else if (event->pos_y == 2) {
        sprintf(xs, "\n\r\x1b[%dC%s\x1b[1A\r", event->pos_x * 2 + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
        B_PRINT(xs);
      }
    }
  }
}

//
//  clear messages
//
void kmd_clear_messages(KMD_HANDLE* kmd) {
  if (kmd->large) {
    struct TXFILLPTR tfp = { 0, 16, 16 + kmd->cursor_pos_y * 16, 768 - 16, 24 * 2 + 16, 0x0000 };
    TXFILL(&tfp);
    struct TXFILLPTR tfp2 = { 1, 16, 16 + kmd->cursor_pos_y * 16, 768 - 16, 24 * 2 + 16, 0x0000 };
    TXFILL(&tfp2);
    B_PRINT("\r\x1b[1A\x1b[1A");
  } else {
    B_PRINT("\r\x1b[1B\x1b[0K\x1b[1A\x1b[0K\x1b[1A\x1b[0K\x1b[1A\x1b[0K");
  }
}

//
//  print event message
//
void kmd_print_event_message(KMD_HANDLE* kmd, KMD_EVENT* event) {
  static uint8_t xs[128];
  if (kmd != NULL && event != NULL) {
    if (kmd->vsync) WAIT_VBLANK;
    if (kmd->large) {
      put_text24(16 + event->pos_x * 24, 16 + kmd->cursor_pos_y * 16 + event->pos_y * 16, 3, event->message);
    } else {
      if (event->pos_y == 0) {
        sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x * 2 + 1, event->message);
        B_PRINT(xs);
      } else if (event->pos_y == 1) {
        sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x * 2 + 1, event->message);
        B_PRINT(xs);
      } else if (event->pos_y == 2) {
        sprintf(xs, "\n\r\x1b[%dC%s\x1b[1A\r", event->pos_x * 2 + 1, event->message);
        B_PRINT(xs);
      }
    }
  }
}

//
//  deactivate events
//
void kmd_deactivate_events(KMD_HANDLE* kmd, uint32_t elapsed) {
  if (kmd != NULL) {
    for (int16_t i = 0; i < kmd->current_event_ofs; i++) {
      KMD_EVENT* event = &(kmd->events[i]);
      if (event->active && event->end_msec <= elapsed) {
        kmd_erase_event_message(kmd, event);
        event->active = 0;
      }
    }
  }
}

//
//  activate current event
//
void kmd_activate_current_event(KMD_HANDLE* kmd, uint32_t elapsed) {
  if (kmd != NULL && kmd->current_event_ofs < kmd->num_events) {
    KMD_EVENT* event = &(kmd->events[ kmd->current_event_ofs ]);
    if (!event->active && event->start_msec <= elapsed) {
      kmd_print_event_message(kmd, event);
      event->active = 1;
      kmd->current_event_ofs++;
    }
  }
}
