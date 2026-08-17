#include "components/button.h"
#include "cigext.h"

cig_font_ref button_font = NULL;
cig_text_color_ref button_title_color = NULL;
bool button_is_pressed = false;

bool
standard_button(cig_r rect, const char *title)
{
  bool clicked = false;
  
  CIG(rect) {
    cig_enable_interaction();
    
    const bool pressed = cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_INSIDE) || button_is_pressed;
    
    cig_fill_style(get_style(STYLE_BUTTON), pressed ? CIG_STYLE_APPLY_PRESS : 0);
    
    if (cig_push_frame_insets(RECT_AUTO,  pressed ? cig_i_make(2, 3, 1, 2) : cig_i_make(1, 1, 2, 2))) {
      cig_draw_label((cig_text_properties) {
        .font = button_font ? button_font : get_font(FONT_REGULAR),
        .color = button_title_color,
        .max_lines = 1,
        .overflow = CIG_TEXT_SHOW_ELLIPSIS
      }, title);
      cig_pop_frame();
    }
    
    clicked = cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_DEFAULT_OPTIONS);
  }

  button_is_pressed = false;
  
  return clicked;
}

bool
icon_button(cig_r rect, image_id_t image_id)
{
  bool clicked = false;
  
  CIG(rect) {
    cig_enable_interaction();
    
    const bool pressed = cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_INSIDE) || button_is_pressed;
    
    cig_fill_style(get_style(STYLE_BUTTON), pressed ? CIG_STYLE_APPLY_PRESS : 0);
    
    if (cig_push_frame_insets(RECT_AUTO, pressed ? cig_i_make(3, 3, 1, 1) : cig_i_make(2, 2, 2, 2))) {
      cig_draw_image(get_image(image_id), CIG_IMAGE_MODE_CENTER);
      cig_pop_frame();
    }
    
    clicked = cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_DEFAULT_OPTIONS);
  }

  button_is_pressed = false;
  
  return clicked;
}

bool
checkbox(cig_r rect, bool *value, const char *text)
{
  bool toggled = false;
  CIG_HSTACK(
    rect,
    CIG_PARAMS({
      CIG_SPACING(5)
    })
  ) {
    cig_enable_interaction();

    if (!value) {
      value = CIG_MEM_INIT(sizeof(bool)) {
        *value = false;
      }
    }

    const bool pressed = cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_DEFAULT_OPTIONS);

    if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_DEFAULT_OPTIONS)) {
      *value = !*value;
    }

    CIG(RECT_AUTO_W(13)) {
      CIG(RECT_CENTERED_VERTICALLY(RECT_SIZED(13, 13))) {
        cig_fill_color(pressed ? get_color(COLOR_DIALOG_BACKGROUND) : get_color(COLOR_WHITE));
        cig_fill_style(get_style(STYLE_INNER_BEVEL_NO_FILL), 0);
        cig_draw_line(cig_v_make(CIG_SX+2, CIG_SY+1), cig_v_make(CIG_SX+11, CIG_SY+1), get_color(COLOR_BLACK), 1);
        cig_draw_line(cig_v_make(CIG_SX+2, CIG_SY+1), cig_v_make(CIG_SX+2, CIG_SY+11), get_color(COLOR_BLACK), 1);
        cig_draw_line(cig_v_make(CIG_SX+1, CIG_SY+11), cig_v_make(CIG_SX+12, CIG_SY+11), get_color(COLOR_DIALOG_BACKGROUND), 1);
        cig_draw_line(cig_v_make(CIG_SX+12, CIG_SY+11), cig_v_make(CIG_SX+12, CIG_SY+1), get_color(COLOR_DIALOG_BACKGROUND), 1);

        if (*value) {
          cig_draw_image(get_image(IMAGE_CHECKMARK), CIG_IMAGE_MODE_CENTER);
        }
      }
    }
    CIG(_) {
      cig_draw_label((cig_text_properties) {
        .alignment.horizontal = CIG_TEXT_ALIGN_LEFT
      }, text);
    }
  }
  return toggled;
}

bool
taskbar_button(cig_r rect, const char *title, int icon, bool selected)
{
  bool clicked = false;
  
  CIG(rect, CIG_INSETS(cig_i_make(4, 2, 4, 2))) {
    cig_enable_interaction();
    
    const bool pressed = cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_INSIDE);
    clicked = cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_DEFAULT_OPTIONS);

    cig_fill_style(get_style(STYLE_BUTTON), selected ? CIG_STYLE_APPLY_SELECTION : (pressed ? CIG_STYLE_APPLY_PRESS : 0));
    
    CIG_HSTACK(
      _,
      CIG_INSETS(selected ? cig_i_make(0, 1, 0, -1) : cig_i_zero()),
      CIG_PARAMS({
        CIG_SPACING(2)
      })
    ) {
      if (icon >= 0) {
        CIG(RECT_AUTO_W(16)) {
          cig_draw_image(get_image(icon), CIG_IMAGE_MODE_CENTER);
        }
      }
      if (title) {
        CIG(_) {
          cig_draw_label((cig_text_properties) {
            .font = get_font(selected ? FONT_BOLD : FONT_REGULAR),
            .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
            .max_lines = 1,
            .overflow = CIG_TEXT_SHOW_ELLIPSIS
          }, title);
        }
      }
    }
  }
  
  return clicked;
}
