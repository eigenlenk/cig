#ifndef CIG_WIN95_DEMO_APPLICATION_TYPE_INCLUDED
#define CIG_WIN95_DEMO_APPLICATION_TYPE_INCLUDED

#include "components/window.h"

typedef enum {
  APPLICATION_KILL = 1

} application_proc_result_t;

typedef application_proc_result_t (*application_proc_t)(struct application_t *);

typedef struct application_t {
  char id[32];
  application_proc_t proc;
  void (*on_kill)(struct application_t *);
  void *data;
  window_t windows[1];
  enum {
    KILL_WHEN_PRIMARY_WINDOW_CLOSED = (1 << 0)
  } flags;
} application_t;

#endif
