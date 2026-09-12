#ifndef CIG_EXTENSIONS_INCLUDED
#define CIG_EXTENSIONS_INCLUDED

#include "cigcore.h"

/**
 * ┌────────────────────────────────────────────────────────────────────────────────┐
 * │ GRAPHICS EXTENSIONS                                                            │
 * │                                                                                │
 * │ Includes graphical extensions you can call to draw rectangles, lines and fill  │
 * │ with panels of certain style. This follows CIG conventions but is not part of  │
 * │ the library itself. It's just an example, and you can avoid opaque color and   │
 * │ style references if you hardcode to a particular backend.                      │
 * └────────────────────────────────────────────────────────────────────────────────┘
 */

typedef void* cig_color_ref;
typedef void* cig_style_ref;

typedef enum M_PACKED {
  CIG_STYLE_APPLY_HOVER = M_BIT(0),
  CIG_STYLE_APPLY_PRESS = M_BIT(1),
  CIG_STYLE_APPLY_SELECTION = M_BIT(2),
  CIG_STYLE_APPLY_FOCUS = M_BIT(3),
} cig_style_modifiers;

typedef void (*cig_draw_style_callback)(cig_style_ref, cig_r, cig_style_modifiers);
typedef void (*cig_draw_rectangle_callback)(cig_color_ref, cig_color_ref, cig_r, unsigned int);
typedef void (*cig_draw_line_callback)(cig_color_ref, cig_v, cig_v, float);
typedef void (*cig_draw_polygon_fn)(cig_v, cig_v*, size_t, cig_color_ref);

/*  ┌───────────────────┐
    │ BACKEND CALLBACKS │
    └───────────────────┘ */

void cig_assign_draw_style(cig_draw_style_callback);

void cig_assign_draw_rectangle(cig_draw_rectangle_callback);

void cig_assign_draw_line(cig_draw_line_callback);

void cig_assign_draw_polygon(cig_draw_polygon_fn);

/*  ┌───────────────────┐
    │ 2D DRAW FUNCTIONS │
    └───────────────────┘ */

/*  Fills current frame with the given style */
void cig_fill_style(cig_style_ref, cig_style_modifiers);

/*  Fills current frame with color */
void cig_fill_color(cig_color_ref);

void cig_draw_line(cig_v, cig_v, cig_color_ref, float);

void cig_draw_rect(cig_r, cig_color_ref, cig_color_ref, float);

void cig_draw_polygon(cig_v, cig_v*, size_t, cig_color_ref);


/**
 * ┌────────────────────────────────────────────────────────────────────────────────┐
 * │ INPUT EXTENSIONS                                                               │
 * │                                                                                │
 * │ Core input API supports a simple button query. In some cases we want to read   │
 * │ multiple keys at the same time (1), so for convenience we have a function that │
 * │ checks focus and accepts an array of keycodes.                                 │
 * │                                                                                │
 * │ Checking for CTRL / SHIFT / ALT combos is also common, so we have modifers     │
 * │ for that as well.                                                              │
 * │                                                                                │
 * │ (1) Calculator demo has several keys bound do the same function.               │
 * └────────────────────────────────────────────────────────────────────────────────┘
 */

#define CIG_KEYS(KEYS...) M_ARRAYC(cig_key_code, KEYS)

typedef enum {
  CIG_KEY_MODIFIER_SHIFT = M_BIT(30),
  CIG_KEY_MODIFIER_CTRL = M_BIT(29),
  CIG_KEY_MODIFIER_ALT = M_BIT(28)
} cig_key_modifier;

bool cig_focused_keys(cig_key_code[], size_t, cig_input_key_state);

#endif
