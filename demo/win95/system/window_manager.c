#include "win95.h"
#include "system/window_manager.h"
#include "system/application.h"

#include <time.h>

static void
window_manager_close(window_manager_t*, window_t*);

static void
window_manager_maximize(window_manager_t*, window_t*);

static void
window_manager_minimize(window_manager_t*, window_t*);

static M_OPTIONAL(window_t*)
window_manager_active_window(window_manager_t *this)
{
  size_t i;

  for (i = 0; i < this->count; ++i) {
    if (this->order[i]->focused) {
      return this->order[i];
    }
  }

  return NULL;
}

window_t* window_manager_create(window_manager_t *manager, struct application_t *app, window_t wnd)
{
  static uint32_t _counter = 0;

  if (!_counter) { _counter = time(NULL); }

  size_t i;
  if (!wnd.id) { wnd.id = rand(); }

  if (wnd.flags & IS_UNIQUE_WINDOW) {
    window_t *existing_wnd = window_manager_find_id(manager, wnd.id);

    if (existing_wnd) {
      window_manager_bring_to_front(manager, existing_wnd);
      return existing_wnd;
    }
  } else {
    wnd.id = CIG_TINYHASH(wnd.id, _counter++);
  }

  for (i = 0; i < WIN95_OPEN_WINDOWS_MAX; ++i) {
    if (manager->windows[i].id) { continue; }
    wnd.owner = app;
    wnd.manager = (struct window_manager_t *)manager;

    wnd.focused = true;
    manager->windows[i] = wnd;
    manager->order[manager->count++] = &manager->windows[i];
    return manager->order[manager->count-1];
  }

  return NULL;
}

void
window_manager_process(window_manager_t *this)
{
  /* Make sure focused window is the topmost one */
  if (this->count > 0 && this->order[this->count-1]->focused == false) {
    window_manager_bring_to_front(this, window_manager_active_window(this));
  }

  size_t i;

  for (i = 0; i < this->count; ++i) {
    window_t *wnd = this->order[i];

    if (wnd->flags & IS_MINIMIZED) {
      wnd->updates = 0;
      continue;
    }

    if (!window_begin(wnd)) {
      wnd->updates = 0;
      continue;
    }

    if (wnd->proc) {
      cig_set_next_id(wnd->id + cig_hash("content"));
      wnd->proc(wnd);
    }

    wnd->updates = 0;

    if (wnd->last_message) {
      switch (wnd->last_message) {
      case WINDOW_CLOSE:
        {
          window_end(wnd);
          window_manager_close(this, wnd);
          continue;
        }
      case WINDOW_MAXIMIZE:
        {
          window_manager_maximize(this, wnd);
          break;
        }
      case WINDOW_MINIMIZE:
        {
          window_manager_minimize(this, wnd);
          break;
        }
      default:
        break;
      }
      wnd->last_message = 0;
    }

    window_end(wnd);
  }
}

void window_manager_bring_to_front(window_manager_t *manager, window_t *wnd)
{
  size_t i, j;
  if (!wnd) { return; }
  for (i = 0; i < manager->count; ++i) {
    if (manager->order[i] == wnd) {
      if (wnd->flags & IS_MINIMIZED) {
        wnd->flags &= ~IS_MINIMIZED;
        wnd->updates |= WINDOW_DID_RESTORE;
      }
      /* Move everyting back by 1 and set the window as last element in the order */
      for (j = i; j < manager->count - 1; ++j) {
        manager->order[j] = manager->order[j+1];
        manager->order[j]->focused = false;
      }
      manager->order[manager->count-1] = wnd;
      wnd->focused = true;
      break;
    }
  }
}

window_t* window_manager_find_primary_window(window_manager_t *manager, struct application_t *app) {
  size_t i;
  for (i = 0; i < manager->count; ++i) {
    if (manager->order[i]->owner == app && manager->order[i]->flags & IS_PRIMARY_WINDOW) {
      return manager->order[i];
    }
  }
  return NULL;
}

window_t* window_manager_find_id(window_manager_t *manager, cig_id id) {
  size_t i;
  for (i = 0; i < manager->count; ++i) {
    if (manager->order[i]->id == id) {
      return manager->order[i];
    }
  }
  return NULL;
}

static void
window_manager_close(window_manager_t *manager, window_t *wnd)
{
  size_t i, j;
  if (wnd->owner && wnd->flags & IS_PRIMARY_WINDOW && wnd->owner->flags & KILL_WHEN_PRIMARY_WINDOW_CLOSED) {
    win95_close_application(wnd->owner);
  }
  if (wnd->on_close) {
    wnd->on_close(wnd);
  }
  if (wnd->data) {
    free(wnd->data);
    wnd->data = NULL;
  }
  wnd->id = 0;
  for (i = 0; i < manager->count; ++i) {
    if (manager->order[i] == wnd) {
      for (j = i+1; j < manager->count; ++j) {
        manager->order[j-1] = manager->order[j];
      }
      manager->count--;
      break;
    }
  }
}

static void
window_manager_maximize(window_manager_t *manager, window_t *wnd)
{
  if (wnd->flags & IS_MAXIMIZED) {
    wnd->flags &= ~IS_MAXIMIZED;
    wnd->updates |= WINDOW_DID_RESTORE;
    wnd->rect = wnd->rect_before_maximized;
  } else {
    wnd->flags |= IS_MAXIMIZED;
    wnd->updates |= WINDOW_DID_MAXIMIZE;
    wnd->rect_before_maximized = wnd->rect;
    wnd->rect = cig_r_make(0, 0, cig_layout_rect().w, cig_layout_rect().h - TASKBAR_H);
  }
}

static void
window_manager_minimize(window_manager_t *manager, window_t *wnd)
{
  if (wnd->flags & IS_MINIMIZED) {
    return;
  }
  /* TODO: Actually Win95 focuses on whatever was focused *before* selecting
   * this window. So either the desktop or some some window.
   */
  wnd->focused = false;
  wnd->flags |= IS_MINIMIZED;
}
