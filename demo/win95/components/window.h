#ifndef CIG_WIN95_DEMO_WINDOW_INCLUDED
#define CIG_WIN95_DEMO_WINDOW_INCLUDED

#include "cig.h"

struct window_t;
struct window_manager_t;
struct application_t;

typedef enum {
  WINDOW_CLOSE = 1,
  WINDOW_MAXIMIZE,
  WINDOW_MINIMIZE
} window_message_t;

typedef enum {
  WINDOW_DID_MOVE = M_BIT(0),
  WINDOW_DID_RESIZE = M_BIT(1),
  WINDOW_DID_MAXIMIZE = M_BIT(2),
  WINDOW_DID_RESTORE = M_BIT(3)
} window_update;

typedef void (*win_proc_t)(struct window_t*);

typedef struct window_t {
  struct window_manager_t *manager;
  struct application_t *owner;
  cig_id id;
  win_proc_t proc;
  window_message_t last_message;
  window_update updates;
  void *data;
  void (*on_close)(struct window_t *);
  cig_r rect;
  cig_r rect_before_maximized;
  cig_v min_size;
  char *title;
  int icon;
  bool focused;
  enum {
    IS_PRIMARY_WINDOW = M_BIT(0),
    IS_UNIQUE_WINDOW = M_BIT(1), /* One instance of this window ID per app */
    IS_RESIZABLE = M_BIT(2),
    IS_MAXIMIZED = M_BIT(3),
    IS_MINIMIZED = M_BIT(4)
  } flags;
} window_t;

bool
window_begin(window_t*);

void
window_end(window_t*);

M_INLINED void
window_send_message(window_t *this, window_message_t msg)
{
  this->last_message = msg;
}

#endif
