#include "components/menu.h"
#include "cigext.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <float.h>

#define CHILD_MENU_DELAY 0.4f

struct menubar_state {
  menu_tracking_st tracking_state;
  win95_menu *last_hovered_menu;
};

struct menu_draw_state {
  win95_menu *presented_submenu;
  cig_v submenu_position;
  float submenu_delay;
};

void menubar(size_t n, win95_menu* menus[]) {
  register size_t i;

  /* */
  cig_enable_interaction();
  cig_disable_culling();

  struct menubar_state *state = CIG_MEM_INIT(sizeof(struct menubar_state)) {
    state->last_hovered_menu = NULL;
    state->tracking_state = 0;
  }

  bool any_menu_active = false;
  /**
   * Tracking state at the start of the call. May be modified halfway through,
   * so to keep things consistent a constant value is used until the next frame.
   */
  const menu_tracking_st current_tracking_state = state->tracking_state;

  CIG_HSTACK(RECT_AUTO, NO_INSETS) {
    for (i = 0; i < n; ++i) {
      /*
       * Calculate title label size. Using raw text API here because of how
       * relatively short these labels are. Raw text is drawn using screen position
       * and we must center it vertically ourselves.
       */
      cig_v text_size = cig_measure_raw_text(NULL, 0, menus[i]->title);

      CIG(RECT_AUTO_W(text_size.x + 6*2), cig_i_horizontal(6)) {
        cig_enable_interaction();
        cig_disable_culling();

        /*
         * The menubar works similarly to the Start menu/button: a menu appears when you click a button.
         * However, unlike the Start button, the menubar has multiple buttons (e.g., File, Edit, View),
         * in series all sharing a single tracking state.
         *
         * - If any menubar button is being tracked (i.e., clicked or held), we activate and show its menu.
         * - While tracking, we also monitor hover state to switch the active menu when the mouse moves
         *   over another menubar button — without requiring another click.
         */
        if (cig_hovered()) {
          state->last_hovered_menu = menus[i];
        }

        const bool current_menu_selected = state->last_hovered_menu == menus[i];

        if (current_menu_selected) {
          menu_track(&state->tracking_state, menus[i], (menu_presentation) {
            .position = { -cig_current()->insets.left, CIG_B },
            .origin = ORIGIN_TOP_LEFT,
          });

          if (current_tracking_state > 0) {
            any_menu_active = true;
            cig_fill_color(get_color(COLOR_WINDOW_ACTIVE_TITLEBAR));
          }
        }

        cig_draw_raw_text(
          cig_v_make(CIG_SX_INSET, CIG_SY_INSET + (CIG_H - text_size.y) * 0.5),
          text_size,
          NULL,         /* Font */
          0,            /* Text style modifiers */
          current_tracking_state > 0 && current_menu_selected
            ? get_color(COLOR_WHITE)
            : get_color(COLOR_BLACK),
          menus[i]->title
        );
      }
    }
  }

  /* None of the menu buttons were tracking */
  if (!any_menu_active) {
    state->last_hovered_menu = NULL;
  }
}

win95_menu * menu_setup(
  win95_menu *this,
  const char *title,
  menu_style style,
  void (*handler)(struct win95_menu *, struct menu_group *, struct menu_item *),
  size_t n,
  menu_group groups[]
) {
  register int i, j, w;
  char *item_title;

  this->title = title;
  this->style = style;
  this->groups.count = n;
  this->base_width = 0;
  this->handler = handler;
  memcpy(this->groups.list, groups, n * sizeof(menu_group));

  for (i = 0; i < n; ++i) {
    /* Find widest text element */
    for (j = 0; j < groups[i].items.count; ++j) {
      if (groups[i].items.list[j].type == CHILD_MENU) {
        item_title = (char *)((win95_menu *)groups[i].items.list[j].data)->title;
      } else {
        item_title = (char *)groups[i].items.list[j].title;
      }
      w = cig_measure_raw_text(NULL, 0, item_title).x;
      if (w > this->base_width) {
        this->base_width = w;
      }
    }
  }

  return this;
}

void menu_draw(win95_menu *this, menu_presentation presentation, bool *prevent_close) {
  register int i, j;
  menu_group *group;
  menu_item *item;
  win95_menu *child_menu;
  char *item_title;
  bool *bool_to_toggle = NULL;
  struct {
    cig_i insets,
          stack_insets;
    int16_t height,
            icon_width,
            icon_title_spacing,
            after_title_spacing;
  } size_info;

  const cig_i panel_insets = cig_i_uniform(3);

  switch (this->style) {
  case DROPDOWN:
    size_info.insets = cig_i_horizontal(6);
    size_info.height = 18;
    size_info.icon_width = 8;
    size_info.icon_title_spacing = 6;
    size_info.after_title_spacing = 15;
    size_info.stack_insets = cig_i_zero();
    break;
  case START:
    size_info.insets = cig_i_horizontal(6);
    size_info.height = 32;
    size_info.icon_width = 32;
    size_info.icon_title_spacing = 6;
    size_info.after_title_spacing = 19;
    /* Space for the vertical Windows 95 text */
    size_info.stack_insets = cig_i_make(21, 0, 0, 0);
    break;
  case START_SUBMENU:
    size_info.insets = cig_i_horizontal(6);
    size_info.height = 22;
    size_info.icon_width = 16;
    size_info.icon_title_spacing = 6;
    size_info.after_title_spacing = 19;
    size_info.stack_insets = cig_i_zero();
    break;
  }

  int panel_width =
      panel_insets.left
    + size_info.stack_insets.left
    + size_info.insets.left
    + size_info.icon_width +
    + size_info.icon_title_spacing
    + this->base_width
    + size_info.after_title_spacing
    + size_info.insets.right
    + size_info.stack_insets.right
    + panel_insets.right;

  int panel_height =
      panel_insets.top
    + panel_insets.bottom
    + size_info.stack_insets.top
    + size_info.stack_insets.bottom;

  for (i = 0; i < this->groups.count; ++i) {
    panel_height += this->groups.list[i].items.count * size_info.height;
    if (i < this->groups.count - 1) {
      panel_height += 8; /* Separator */
    }
  }

  cig_v menu_pos = presentation.position;

  if (presentation.origin == ORIGIN_BOTTOM_LEFT) {
    menu_pos.y -= panel_height;
  }

  cig_set_next_id(cig_hash(this->title));
  CIG(
    RECT(menu_pos.x, menu_pos.y, panel_width, panel_height),
    CIG_INSETS(panel_insets)
  ) {
    cig_retain(cig_current());

    struct menu_draw_state *draw_state = CIG_MEM_INIT(sizeof(struct menu_draw_state)) {
      draw_state->presented_submenu = NULL;
      draw_state->submenu_delay = 0;
    }

    cig_fill_style(get_style(STYLE_STANDARD_DIALOG), 0);

    cig_enable_interaction();

    if (cig_visibility() == CIG_FRAME_APPEARED || cig_hovered()) {
      /* Hovering non-content (edges, separators) */
      draw_state->presented_submenu = NULL;
    }
    
    if (this->style == START) {
      CIG(_W(size_info.stack_insets.left)) {
        cig_fill_color(get_color(COLOR_WINDOW_INACTIVE_TITLEBAR));
        cig_draw_image(get_image(IMAGE_START_SIDEBAR), CIG_IMAGE_MODE_BOTTOM);
      }
    }

    CIG_VSTACK(_, CIG_INSETS(size_info.stack_insets), CIG_PARAMS({
      CIG_HEIGHT(size_info.height)
    })) {
      for (i = 0; i < this->groups.count; ++i) {
        group = &this->groups.list[i];

        for (j = 0; j < group->items.count; ++j) {
          item = &group->items.list[j];

          CIG(_) {
            cig_enable_interaction();

            const bool item_hovered = cig_hovered();
            bool draws_highlight = item_hovered;

            if (item->type == CHILD_MENU && (child_menu = (win95_menu *)item->data)) {
              item_title = (char *)child_menu->title;
            } else {
              item_title = (char *)item->title;
            }

            if (item_hovered) {
              if (item->type == CHILD_MENU) {
                if (draw_state->presented_submenu != child_menu) {
                  draw_state->submenu_delay = CHILD_MENU_DELAY;
                }
                draw_state->presented_submenu = child_menu;
                draw_state->submenu_position = cig_v_make(size_info.stack_insets.left + CIG_R_INSET - 2, CIG_Y - 3);
              } else {
                draw_state->presented_submenu = NULL;
              }
            } else {
              if (item->type == CHILD_MENU) {
                draws_highlight = draw_state->presented_submenu == child_menu;
              }
            }

            if (draws_highlight) {
              cig_fill_color(get_color(COLOR_WINDOW_ACTIVE_TITLEBAR));
            }

            if (item->type != DISABLED && item->type != CHILD_MENU) {
              if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, 0)) {
                if (item->type == TOGGLE) {
                  bool *bool_value = (bool *)item->data;
                  /**
                   * Boolean will be toggled at the end of scope as this value is used to draw
                   * checkmark, and switching it halway through would show incorrect state.
                   */
                  if (bool_value) { bool_to_toggle = bool_value; }
                }
                if (item->_handler) {
                  item->_handler();
                } else if (item->handler) {
                  item->handler(item);
                } else if (group->handler) {
                  group->handler(group, item);
                } else if (this->handler) {
                  this->handler(this, group, item);
                } else {
                  printf("Note: No menu handler configured for '%s'\n", this->title);
                }
              }
            } else if (prevent_close) {
              /* Windows 95 keeps menus open when you click on a disabled or sub-menu item */
              if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, 0)) {
                /* Immeditately open sub-menu */
                if (item->type == CHILD_MENU) {
                  draw_state->submenu_delay = 0;
                }
                *prevent_close = true;
              } 
            }

            CIG_HSTACK(_, size_info.insets) {
              CIG(_W(size_info.icon_width)) {
                if (item->type == TOGGLE) {
                  bool *bool_value = (bool *)item->data;
                  if (bool_value && *bool_value) {
                    cig_draw_image(get_image(item_hovered ? IMAGE_MENU_CHECK_INVERTED : IMAGE_MENU_CHECK), CIG_IMAGE_MODE_CENTER);
                  }
                } else if (item->type == RADIO_ON) {
                  cig_draw_image(get_image(item_hovered ? IMAGE_MENU_RADIO_INVERTED : IMAGE_MENU_RADIO), CIG_IMAGE_MODE_CENTER);
                } else if (item->icon) {
                  cig_draw_image(get_image(item->icon), CIG_IMAGE_MODE_CENTER);
                }
              }

              CIG(_W(size_info.icon_title_spacing)) {} /* Spacer */

              CIG(_W(this->base_width)) {
                cig_draw_label((cig_text_properties) {
                  .color = item->type == DISABLED
                    ? get_color(COLOR_WINDOW_INACTIVE_TITLEBAR)
                    : draws_highlight
                      ? get_color(COLOR_WHITE)
                      : get_color(COLOR_BLACK),
                  .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
                }, item_title);
              }

              if (item->type == CHILD_MENU) {
                CIG(_W(size_info.after_title_spacing)) {
                  cig_draw_image(get_image(draws_highlight ? IMAGE_MENU_ARROW_INVERTED : IMAGE_MENU_ARROW), CIG_IMAGE_MODE_RIGHT);
                }
              }
            }
          }
        }

        if (i < this->groups.count - 1) {
          CIG(_H(8)) {
            CIG(RECT_CENTERED_VERTICALLY(_H(2))) { /* Separator */
              cig_fill_style(get_style(STYLE_INNER_BEVEL_NO_FILL), 0);
            }
          }
        }
      }
    }

    if (bool_to_toggle) {
      *bool_to_toggle = !*bool_to_toggle;
    }

    if (draw_state->presented_submenu) {
      if (draw_state->submenu_delay > FLT_EPSILON) {
        draw_state->submenu_delay -= cig_delta_time();
      } else {
        draw_state->submenu_delay = 0.f;

        menu_draw(draw_state->presented_submenu, (menu_presentation) {
          .position = draw_state->submenu_position,
          .origin = ORIGIN_TOP_LEFT
        }, prevent_close);
      }
    }
  }
}

/*
 * Function that tracks either:
 * - Click start and click end
 * - Press start and press release
 */
menu_tracking_st menu_track(menu_tracking_st *tracking, win95_menu *menu, menu_presentation presentation) {
  /* CLICK or PRESS to activate tracking */
  if (*tracking == NOT_TRACKING && cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_INSIDE)) {
    *tracking = BY_PRESS;
  } else if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_STARTS_INSIDE | CIG_CLICK_EXPIRE)) {
    *tracking = *tracking != BY_CLICK ? BY_CLICK : NOT_TRACKING;
  }

  bool prevent_close = false;

  if (*tracking) {
    menu_draw(menu, presentation, &prevent_close);
  }

  /* We want to keep the menu open when clicking (releasing mouse button) on disabled item or sub-menu */
  if (prevent_close) {
    /* 'Press' tracking is converted to 'click' tracking */
    if (*tracking == BY_PRESS) {
      *tracking = BY_CLICK;
    }
    return *tracking;
  }

  if (*tracking == BY_CLICK) {
    if (cig_input_state()->pointer.click_state == EXPIRED) {
      *tracking = NOT_TRACKING;
    }
    else if (cig_input_state()->pointer.click_state == BEGAN && !(cig_current()->_flags & SUBTREE_INCLUSIVE_HOVER)) {
      /* Mouse button was pressed down while outside the menu button and any of its children (menus) */
      *tracking = NOT_TRACKING;
    }
    else if (cig_input_state()->pointer.click_state == ENDED && !(cig_current()->_flags & HOVER) && cig_current()->_flags & SUBTREE_INCLUSIVE_HOVER) {
      /* Click was made, but inside one of its children (menus) and not the menu button itself */
      *tracking = NOT_TRACKING;
    }
  }
  else if (*tracking == BY_PRESS && cig_input_state()->pointer.action_mask == 0) {
    /* Mouse button was released while in PRESS tracking mode */
    *tracking = NOT_TRACKING;
  }

  return *tracking;
}
