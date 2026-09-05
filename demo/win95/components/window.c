#include "components/window.h"
#include "components/button.h"
#include "system/resources.h"
#include "cigext.h"

typedef enum {
  WINDOW_RESIZE_BOTTOM_RIGHT = 0,
  WINDOW_RESIZE_RIGHT,
  WINDOW_RESIZE_TOP_RIGHT,
  WINDOW_RESIZE_TOP,
  WINDOW_RESIZE_TOP_LEFT,
  WINDOW_RESIZE_LEFT,
  WINDOW_RESIZE_BOTTOM_LEFT,
  WINDOW_RESIZE_BOTTOM
} window_resize_edge_t;

static void
handle_window_resize(window_t *, window_resize_edge_t);

bool
window_begin(window_t *wnd)
{
  static struct {
    cig_r original_rect;
  } window_drag = { 0 };

  cig_set_next_id(wnd->id);
  
  const cig_i wnd_insets = (wnd->flags & IS_MAXIMIZED)
    ? cig_i_zero()
    : cig_i_uniform(wnd->flags & IS_RESIZABLE ? 4 : 3);

  if (!cig_push_frame_insets(wnd->rect, wnd_insets)) {
    return false;
  }

  if (wnd->flags & IS_MAXIMIZED) {
    cig_fill_color(get_color(COLOR_DIALOG_BACKGROUND));
  } else {
    cig_fill_style(get_style(STYLE_STANDARD_DIALOG), 0);
  }

  cig_enable_focus(&wnd->focused);

  /* Titlebar */
  CIG_HSTACK(
    RECT_AUTO_H(18),
    CIG_INSETS(cig_i_uniform(2)),
    CIG_PARAMS({
      CIG_SPACING(2),
      CIG_ALIGNMENT_HORIZONTAL(CIG_LAYOUT_ALIGNS_RIGHT)
    })
  ) {
    cig_fill_color(get_color(wnd->focused ? COLOR_WINDOW_ACTIVE_TITLEBAR : COLOR_WINDOW_INACTIVE_TITLEBAR));
    cig_enable_interaction();

    if (wnd->flags & IS_RESIZABLE) {
      if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_STARTS_INSIDE | CIG_CLICK_DOUBLE)) {
        window_send_message(wnd, WINDOW_MAXIMIZE);
      }
    }

    if ((wnd->flags & IS_MAXIMIZED) == false && wnd->focused) {
      switch (cig_dragged(CIG_INPUT_PRIMARY_ACTION)) {
      case CIG_DRAG_STATE_BEGAN:
        cig_input_state()->pointer.locked = true;
        window_drag.original_rect = wnd->rect;
        /* Fallthrough */

      case CIG_DRAG_STATE_MOVED:
      {
        const cig_r rect_before = wnd->rect;
        wnd->rect = cig_r_make(
          M_CLAMP(window_drag.original_rect.x + cig_input_state()->pointer.drag.change_total.x, -(wnd->rect.w - 50), cig_layout_rect().w - 30),
          M_CLAMP(window_drag.original_rect.y + cig_input_state()->pointer.drag.change_total.y, 0, cig_layout_rect().h - 50),
          wnd->rect.w,
          wnd->rect.h
        );
        if (!cig_r_equals(wnd->rect, rect_before)) {
          wnd->updates |= WINDOW_DID_MOVE;
        }
        break;
      }

      default: break;
      }
    }

    /*  This container is right-aligned, so x=0 will align the right edge to parent */
    if (icon_button(RECT_AUTO_W(16), IMAGE_CROSS)) {
      window_send_message(wnd, WINDOW_CLOSE);
    }

    if (wnd->flags & IS_RESIZABLE) {
      CIG_HSTACK(_W(16*2)) {
        if (icon_button(RECT_AUTO_W(16), IMAGE_MINIMIZE)) {
          window_send_message(wnd, WINDOW_MINIMIZE);
        }
        if (icon_button(RECT_AUTO_W(16), wnd->flags & IS_MAXIMIZED ? IMAGE_RESTORE : IMAGE_MAXIMIZE)) {
          window_send_message(wnd, WINDOW_MAXIMIZE);
        }
      }
    }

    CIG_HSTACK(_, CIG_PARAMS({
      CIG_SPACING(2)
    })) {
      if (wnd->icon >= 0) {
        CIG(RECT_AUTO_W(16)) {
          cig_draw_image(get_image(wnd->icon), CIG_IMAGE_MODE_CENTER);
        }
      }
      CIG(_) {
        cig_draw_label((cig_text_properties) {
          .font = get_font(FONT_BOLD),
          .color = wnd->focused ? get_color(COLOR_WHITE) : get_color(COLOR_DIALOG_BACKGROUND),
          .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
          .max_lines = 1,
          .overflow = CIG_TEXT_SHOW_ELLIPSIS 
        }, wnd->title);
      }
    }
  }

  /*
   * Add resizable edge and corner regions.
   * Resetting insets temporarily makes adding the resize regions easier to calculate.
   */
  if (!(wnd->flags & IS_MAXIMIZED)) {
    cig_current()->insets = cig_i_zero();

    if (wnd->flags & IS_RESIZABLE) {
      struct {
        cig_r rect;
        window_resize_edge_t edge;
      } resize_regions[10] = {
        { cig_r_make(CIG_W-4, 20, 4, CIG_H-(20+16)), WINDOW_RESIZE_RIGHT },
        { cig_r_make(CIG_W-4, 4, 4, 16), WINDOW_RESIZE_TOP_RIGHT },
        { cig_r_make(CIG_W-20, 0, 20, 4), WINDOW_RESIZE_TOP_RIGHT },
        { cig_r_make(20, 0, CIG_W-(20+20), 4), WINDOW_RESIZE_TOP },
        { cig_r_make(0, 4, 4, 16), WINDOW_RESIZE_TOP_LEFT },
        { cig_r_make(0, 0, 20, 4), WINDOW_RESIZE_TOP_LEFT },
        { cig_r_make(0, 20, 4, CIG_H-(20+16)), WINDOW_RESIZE_LEFT },
        { cig_r_make(0, CIG_H-20, 4, 16), WINDOW_RESIZE_BOTTOM_LEFT },
        { cig_r_make(0, CIG_H-4, 20, 4), WINDOW_RESIZE_BOTTOM_LEFT },
        { cig_r_make(20, CIG_H-4, CIG_W-(20+16), 4), WINDOW_RESIZE_BOTTOM },
      };

      int i;
      for (i = 0; i < 10; ++i) {
        CIG(resize_regions[i].rect) {
          cig_enable_interaction();
          handle_window_resize(wnd, resize_regions[i].edge);
        }
      }

      CIG(RECT(CIG_W_INSET-16, CIG_H_INSET-16, 16, 16)) {
        cig_enable_interaction();
        handle_window_resize(wnd, WINDOW_RESIZE_BOTTOM_RIGHT);
      }
    }

    cig_current()->insets = wnd_insets;
  }

  const int content_y = wnd->flags & IS_RESIZABLE ? 20 : 19;

  cig_push_frame(cig_r_make(0, content_y, CIG_AUTO(), CIG_H_INSET - content_y));
    
  return true;
}

void
window_end(window_t *wnd)
{
  cig_pop_frame(); /* Content frame */
  cig_pop_frame(); /* Window panel */
}

/*
 * ┌──────────┐
 * │ INTERNAL │
 * └──────────┘
 */

static void
handle_window_resize(window_t *wnd, window_resize_edge_t edge)
{
  static struct {
    window_resize_edge_t active_edge;
    cig_r original_rect;
  } window_resize = { 0 };

  const struct {
    int x, y, w, h;
  } edge_adjustments[8] = {
    { 0, 0, 1, 1 }, /* WINDOW_RESIZE_BOTTOM_RIGHT */
    { 0, 0, 1, 0 }, /* WINDOW_RESIZE_RIGHT */
    { 0, 1, 1, 0 }, /* WINDOW_RESIZE_TOP_RIGHT */
    { 0, 1, 0, 0 }, /* WINDOW_RESIZE_TOP */
    { 1, 1, 0, 0 }, /* WINDOW_RESIZE_TOP_LEFT */
    { 1, 0, 0, 0 }, /* WINDOW_RESIZE_LEFT */
    { 1, 0, 0, 1 }, /* WINDOW_RESIZE_BOTTOM_LEFT */
    { 0, 0, 0, 1 }, /* WINDOW_RESIZE_BOTTOM */
  };

  if (!wnd->focused) {
    return;
  }

  switch (cig_dragged(CIG_INPUT_PRIMARY_ACTION)) {
  case CIG_DRAG_STATE_BEGAN:
    cig_input_state()->pointer.locked = true;
    window_resize.original_rect = wnd->rect;
    window_resize.active_edge = edge;
    /* Fallthrough */

  case CIG_DRAG_STATE_MOVED:
  {
      const cig_r rect_before = wnd->rect;
      const int dx = cig_input_state()->pointer.drag.change_total.x,
                dy = cig_input_state()->pointer.drag.change_total.y;

      if (edge_adjustments[edge].x) {
        wnd->rect.w = M_MAX(wnd->min_size.x, window_resize.original_rect.w - dx);
        wnd->rect.x = window_resize.original_rect.x + (window_resize.original_rect.w - wnd->rect.w);
      }
      if (edge_adjustments[edge].y) {
        wnd->rect.h = M_MAX(wnd->min_size.y, window_resize.original_rect.h - dy);
        wnd->rect.y = window_resize.original_rect.y + (window_resize.original_rect.h - wnd->rect.h);
      }
      if (edge_adjustments[edge].w) {
        wnd->rect.w = M_MAX(wnd->min_size.x, window_resize.original_rect.w + dx);
      }
      if (edge_adjustments[edge].h) {
        wnd->rect.h = M_MAX(wnd->min_size.y, window_resize.original_rect.h + dy);
      }

      if (!cig_r_equals(wnd->rect, rect_before)) {
        wnd->updates |= WINDOW_DID_RESIZE;
      }
    break;
  }

  default: break;
  }
}
