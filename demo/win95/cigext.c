#include "cigext.h"

static cig_draw_style_callback style_callback = NULL;
static cig_draw_rectangle_callback draw_rectangle = NULL;
static cig_draw_line_callback draw_line = NULL;
static cig_draw_polygon_fn draw_polygon = NULL;

void
cig_assign_draw_style(cig_draw_style_callback fp)
{
  style_callback = fp;
}

void
cig_assign_draw_rectangle(cig_draw_rectangle_callback fp)
{
  draw_rectangle = fp;
}

void
cig_assign_draw_line(cig_draw_line_callback fp)
{
  draw_line = fp;
}

void
cig_assign_draw_polygon(cig_draw_polygon_fn fn)
{
  draw_polygon = fn;
}

void
cig_fill_style(cig_style_ref style, cig_style_modifiers modifiers)
{
  if (!style_callback) { /* Log an error? */ return; }
  style_callback(style, cig_absolute_rect(), modifiers);
}

void
cig_fill_color(cig_color_ref color)
{
  if (!draw_rectangle) { /* Log an error? */ return; }
  draw_rectangle(color, 0, cig_absolute_rect(), 0);
}

void
cig_draw_line(cig_v p0, cig_v p1, cig_color_ref color, float thickness)
{
  if (!draw_line) { /* Log an error? */ return; }
  draw_line(color, p0, p1, thickness);
}

void
cig_draw_rect(cig_r rect, cig_color_ref fill_color, cig_color_ref outline_color, float thickness)
{
  if (!draw_rectangle) { /* Log an error? */ return; }
  draw_rectangle(fill_color, outline_color, rect, thickness);
}

void
cig_draw_polygon(cig_v origin, cig_v *points, size_t n, cig_color_ref fill)
{
  if (!draw_polygon) { /* Log an error? */ return; }
  draw_polygon(origin, points, n, fill);
}

bool
cig_focused_keys(cig_key_code k[], size_t n, cig_input_key_state state)
{
  size_t i;

  if (!cig_focused()) {
    return false;
  }

  const bool shift_pressed = cig_key_raw_pressed(CIG_KEY_LSHIFT) || cig_key_raw_pressed(CIG_KEY_RSHIFT);
  const bool ctrl_pressed = cig_key_raw_pressed(CIG_KEY_LCTRL) || cig_key_raw_pressed(CIG_KEY_RCTRL);
  const bool alt_pressed = cig_key_raw_pressed(CIG_KEY_LALT) || cig_key_raw_pressed(CIG_KEY_RALT);

  for (i = 0; i < n; ++i) {
    int ki = (int)k[i];

    if (!ki) {
      continue;
    }

    if (ki >= CIG_KEY_MODIFIER_ALT) {
      if (ki & CIG_KEY_MODIFIER_SHIFT && !shift_pressed) {
        return false;
      }

      if (ki & CIG_KEY_MODIFIER_CTRL && !ctrl_pressed) {
        return false;
      }

      if (ki & CIG_KEY_MODIFIER_ALT && !alt_pressed) {
        return false;
      }
    } else if (shift_pressed || ctrl_pressed || alt_pressed) {
      return false;
    }

    /* Remove application-level bits from the keycode before passing to CIG */
    ki &= ~CIG_KEY_MODIFIER_SHIFT;
    ki &= ~CIG_KEY_MODIFIER_CTRL;
    ki &= ~CIG_KEY_MODIFIER_ALT;

    if (cig_key((cig_key_code)ki) & state) {
      return true;
    }
  }

  return false;
}
