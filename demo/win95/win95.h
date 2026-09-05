#ifndef CIG_WIN95_DEMO_INCLUDED
#define CIG_WIN95_DEMO_INCLUDED

#include "cig.h"
#include "system/application.h"
#include "system/window_manager.h"
#include "system/resources.h"
#include "system/backend.h"

#define APP_WINDOW_TITLE "Winlose 95"
#define WIN95_APPS_MAX 16
#define TASKBAR_H 28

#define CENTER_APP_WINDOW(W, H) cig_r_make((cig_layout_rect().w-W)*0.5, (cig_layout_rect().h-H-TASKBAR_H)*0.5, W, H)

typedef struct {
  /*__PRIVATE__*/
  application_t *applications[WIN95_APPS_MAX];
  window_manager_t window_manager;
  size_t running_apps;
  bool running;
} win95_t;

void
win95_initialize(win95_t *);

void
win95_shut_down();

bool
win95_run();

void
win95_open_app(application_t);

void
win95_close_application(application_t*);

application_t*
win95_find_open_app(const char*);

void
win95_show_about_window();

#endif
