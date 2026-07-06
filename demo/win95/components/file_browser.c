#include "components/file_browser.h"
#include "cigext.h"
#include <string.h>

static cig_frame*
large_file_icon(
  int icon,
  const char *title,
  color_id_t text_color,
  bool is_selected,
  bool *did_double_click,
  bool *did_select
)
{
  CIG_RETAIN(CIG_VSTACK(RECT_AUTO, CIG_INSETS(cig_i_make(2, 2, 2, 0)), CIG_PARAMS({
    CIG_SPACING(6)
  })) {
    cig_enable_interaction();

    if (did_select && cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_DEFAULT_OPTIONS)) {
      *did_select = true;
    }

    if (did_double_click) {
      *did_double_click = cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_DEFAULT_OPTIONS | CIG_CLICK_DOUBLE);
    }

    CIG(RECT_AUTO_H(32)) {
      if (is_selected) { enable_blue_selection_dithering(true); }
      cig_draw_image(get_image(icon), CIG_IMAGE_MODE_TOP);
      if (is_selected) { enable_blue_selection_dithering(false); }
    }

    /* Label with 3 lines */
    cig_label *label = cig_memory_allocate(CIG_LABEL_SIZEOF(3));
    label->available_spans = 3;

    /* We need to 'prepare' the label here to know how large of a rectangle
       to draw around it when the icon is selected */
    cig_label_prepare(label, CIG_SIZE_INSET, (cig_text_properties) {
        .color = is_selected ? get_color(COLOR_WHITE) : get_color(text_color),
        .alignment.vertical = CIG_TEXT_ALIGN_TOP
      }, title);

    CIG(_) {
      if (is_selected) {
        int text_x_in_parent = (CIG_W - label->bounds.w) * 0.5;
        cig_draw_rect(
          cig_r_make(CIG_SX+text_x_in_parent-1, CIG_SY-1, label->bounds.w+2, label->bounds.h+2),
          get_color(COLOR_WINDOW_ACTIVE_TITLEBAR),
          0,
          1
        );
      }
      cig_label_draw(label);
    }
  })

  return CIG_LAST();
}

/* Rectangle between 2 points in any relation */
static cig_r
rect_of(cig_v p0, cig_v p1)
{
  int32_t min_x = M_MIN(p0.x, p1.x);
  int32_t min_y = M_MIN(p0.y, p1.y);
  int32_t max_x = M_MAX(p0.x, p1.x);
  int32_t max_y = M_MAX(p0.y, p1.y);
  return cig_r_make(min_x, min_y, max_x-min_x, max_y-min_y);
}

typedef struct {
  color_id_t text_color;
  size_t count;
  bool selected[32];
  int *number_selected;
  struct {
    bool active;
    cig_v start;
    cig_r relative_rect;
  } drag_selection;
} file_browser_data_t;


/**
 * ┌────────┐
 * │ PUBLIC │
 * └────────┘
 */

bool
begin_file_browser(
  cig_r rect,
  int direction,
  color_id_t text_color,
  int *number_selected
)
{
  if (!cig_push_grid(RECT_AUTO, cig_i_zero(), (cig_params) {
    .width = 75,
    .height = 75,
    .direction = direction,
    .limit.horizontal = 4,
    .flags = CIG_LAYOUT_MINIMUM_LIMIT
  })) {
    return false;
  }

  cig_retain(cig_current());

  if (number_selected) {
    *number_selected = 0;
  }

  file_browser_data_t *data = cig_memory_allocate(sizeof(file_browser_data_t));
  data->text_color = text_color;
  data->number_selected = number_selected;
  data->count = 0;

  if (cig_visibility() == CIG_FRAME_APPEARED) {
    /* Clear file selection when file browser becomes visible */
    memset(data->selected, 0, sizeof(bool[32]));
  }

  cig_disable_culling();
  cig_enable_interaction();
  cig_enable_scroll(NULL);
  // cig_enable_focus(NULL);

  if (cig_focused()) {
    switch (cig_dragged(CIG_INPUT_PRIMARY_ACTION)) {
    case CIG_DRAG_STATE_READY:
      /**
       * Deselect everything when dragging is ready (first clicked).
       * Ignore when click also brought element to focus.
       */
      if (!cig_gained_focus()) {
        memset(data->selected, 0, sizeof(bool[32]));
      }
      break;

    case CIG_DRAG_STATE_BEGAN:
      cig_input_state()->pointer.locked = true;
      data->drag_selection.active = true;
      data->drag_selection.start = cig_v_sub(cig_input_state()->pointer.drag._start_position_absolute, cig_v_make(CIG_SX, CIG_SY));
      /* FALLTHROUGH */

    case CIG_DRAG_STATE_MOVED:
      data->drag_selection.relative_rect = rect_of(
        data->drag_selection.start,
        cig_v_add(data->drag_selection.start, cig_input_state()->pointer.drag.change_total)
      );
      break;

    case CIG_DRAG_STATE_ENDED:
      data->drag_selection.active = false;
      break;

    default: break;
    }
  }

  return true;
}

bool
file_item(image_id_t image, const char *title)
{
  file_browser_data_t *data = CIG_MEM_READ(file_browser_data_t);
  cig_frame *file_frame;
  size_t index = data->count++;
  bool did_double_click = false, did_select = false;

  if (!(file_frame = large_file_icon(image,
                                     title,
                                     data->text_color,
                                     cig_focused() && data->selected[index],
                                     &did_double_click,
                                     &did_select))) {
    return false;
  }

  if (data->drag_selection.active) {
    data->selected[index] = cig_r_intersects(data->drag_selection.relative_rect, file_frame->rect);
  } else {
    if (did_select) { /* Single selection made */
      memset(data->selected, 0, sizeof(bool[32]));
      data->selected[index] = true;
    }
  }

  return did_double_click;
}

void end_file_browser() {
  int i;
  file_browser_data_t *data = CIG_MEM_READ(file_browser_data_t);

  if (data->number_selected) {
    for (i = 0; i < 32; ++i) {
      if (data->selected[i]) {
        *(data->number_selected) += 1;
      }
    }
  }

  if (data->drag_selection.active) {
   cig_draw_rect(
      cig_r_offset(data->drag_selection.relative_rect, CIG_SX, CIG_SY),
      NULL,
      get_color(COLOR_BLACK),
      1
    );
  }

  cig_pop_frame();
}
