#include "wordwiz.h"
#include "components/button.h"
#include "components/menu.h"
#include "cigext.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define LETTER_APPEARANCE_TIME 0.35f

enum M_PACKED {
  GAME_NEW,
  GAME_QUIT
};

typedef enum M_PACKED {
  GUESS_WRONG = 1,
  GUESS_MISPLACED,
  GUESS_CORRECT
} letter_result;

typedef struct {
  char *word;
  char word_uppercase[6];
  int guesses_made;
  struct {
    char word[6];
    letter_result results[5];
  } guesses[6];
  char input[6];
  struct {
    bool active;
    int index;
    float time;
  } animation;
  letter_result keyboard[26];
  enum M_PACKED {
    GAME_LOST = 1,
    GAME_WON = 2
  } final_result;
  struct {
    bool visible;
    float time;
  } wrong_guess;
} game_data_st;

static win95_menu menu_game;
static char (*words)[6];
static char (*all_words)[6];
static int word_count = 0, all_words_count = 0;

M_INLINED void game_reset(game_data_st *game) {
  int i;
  game->guesses_made = 0;
  game->final_result = 0;
  game->word = words[rand() % word_count];
  for (i = 0; i < 5; ++i) {
    game->word_uppercase[i] = toupper(game->word[i]);
  } game->word_uppercase[5] = '\0';
  game->animation.active = false;
  game->wrong_guess.visible = false;
  // printf("Current word: %s\n", game->word);
  memset(game->input, 0, 6);
  memset(game->keyboard, 0, 26);
}

M_INLINED void append_input(game_data_st *game, const char *letter) {
  if (game->animation.active || game->final_result) { return; }
  size_t n = strlen(game->input);
  if (n == 5) { return; }
  game->input[n] = tolower(letter[0]);
}

M_INLINED void trim_input(game_data_st *game) {
  if (game->animation.active || game->final_result) { return; }
  size_t n = strlen(game->input);
  if (!n) { return; }
  game->input[n-1] = '\0';
}

M_INLINED color_id_t result_color(letter_result result) {
  switch (result) {
  case GUESS_WRONG: return COLOR_WINDOW_INACTIVE_TITLEBAR;
  case GUESS_MISPLACED: return COLOR_YELLOW;
  case GUESS_CORRECT: return COLOR_GREEN;
  default: return COLOR_DIALOG_BACKGROUND;
  }
}

static void check_guess(const char *guess, const char *word, letter_result *result) {
  register int i, j;
  char tmp[6];
  strcpy(tmp, word);

  result[0] = GUESS_WRONG;
  result[1] = GUESS_WRONG;
  result[2] = GUESS_WRONG;
  result[3] = GUESS_WRONG;
  result[4] = GUESS_WRONG;

  /* Find all correct letters */
  for (i = 0; i < 5; ++i) {
    if (guess[i] == tmp[i]) {
      result[i] = GUESS_CORRECT;
      tmp[i] = ' ';
    }
  }

  /* Find correct but misplaced letters */
  for (i = 0; i < 5; ++i) {
    for (j = 0; j < 5; ++j) {
      if (tmp[j] == ' ') { continue; }
      if (guess[i] == word[j] && result[i] != GUESS_CORRECT) {
        result[i] = GUESS_MISPLACED;
        tmp[j] = ' ';
        break;
      }
    }
  }
}

static bool check_word_validity(const char *word) {
  register int i = 0;
  for (i = 0; i < word_count; ++i) {
    if (!strcmp(word, words[i])) {
      return true;
    }
  }
  for (i = 0; i < all_words_count; ++i) {
    if (!strcmp(word, all_words[i])) {
      return true;
    }
  }
  return false;
}

static void submit_word(game_data_st *game) {
  if (game->animation.active || game->final_result) { return; }
  size_t n = strlen(game->input);
  if (n < 5) { return; }
  if (!check_word_validity(game->input)) {
    game->wrong_guess.visible = true;
    game->wrong_guess.time = 1.5f;
    return;
  }
  int g = game->guesses_made;
  strcpy(game->guesses[g].word, game->input);
  check_guess(game->input, game->word, game->guesses[g].results);
  game->guesses_made ++;
  game->animation.active = true;
  game->animation.index = 0;
  game->animation.time = LETTER_APPEARANCE_TIME;
  if (!strcmp(game->input, game->word)) {
    game->final_result = GAME_WON;
  } else if (game->guesses_made == 6) {
    game->final_result = GAME_LOST;
  }
  memset(game->input, 0, 6);
}

bool load_dictionary(const char *path, char (**list)[6], int *count) {
  register int n = 0, len = 1024;
  FILE *fp;
  char line[6];

  fp = fopen(path, "r");

  if (!fp) {
    return false;
  }

  *list = malloc(len*sizeof(**list));

  while (fgets(line, 6, fp)) {
    if (line[0] == '\0' || line[0] == '\n') { continue; }
    strcpy((*list)[n++], line);
    if (n == len) {
      len += 1024;
      *list = realloc(*list, len*sizeof(**list));
    }
  }

  *count = n;
  *list = realloc(*list, n*sizeof(**list));

  fclose(fp);

  return true;
}

static void game_menu_handler(win95_menu *menu, menu_group *group, menu_item *item) {
  switch (item->id) {
  case GAME_NEW:
    {
      game_reset((game_data_st *)item->data);
    } break;
  case GAME_QUIT:
    {
      window_send_message((window_t *)item->data, WINDOW_CLOSE);
    } break;

  default: break;
  }
}

static bool
keyboard_button(game_data_st *game, cig_r rect, const char *key, cig_key_code keycode)
{
  int letter_index = keycode - CIG_KEY_A;
  bool clicked = false;

  CIG(rect, cig_i_uniform(4)) {
    cig_enable_interaction();
    
    const bool show_press = cig_pressed(CIG_INPUT_PRIMARY_ACTION, CIG_PRESS_INSIDE) || cig_focused_keys(CIG_KEYS(keycode), CIG_KEY_PRESSED);
    
    cig_fill_style(get_style(STYLE_BUTTON), show_press ? CIG_STYLE_APPLY_PRESS : 0);

    if (letter_index >= 0 && letter_index < 26 && game->keyboard[letter_index] > 0) {
      CIG(_) {
        cig_fill_color(get_color(result_color(game->keyboard[letter_index])));
      }
    }
    
    if (cig_push_frame_insets(RECT_AUTO, show_press ? cig_i_make(2, 3, 1, 2) : cig_i_make(1, 1, 2, 2))) {
      cig_draw_label((cig_text_properties) {
        .font = get_font(FONT_BOLD),
        .color = get_color(COLOR_WINDOW_ACTIVE_TITLEBAR)
      }, key);
      cig_pop_frame();
    }
    
    if (cig_clicked(CIG_INPUT_PRIMARY_ACTION, CIG_CLICK_STARTS_INSIDE | CIG_CLICK_ON_PRESS) || cig_focused_keys(CIG_KEYS(keycode), CIG_KEY_CLICKED)) {
      clicked = true;
    }
  }

  return clicked;
}

static void game_menubar(window_t *wnd) {
  menu_setup(&menu_game, "Game", DROPDOWN, &game_menu_handler, 2, (menu_group[]) {
    {
      .items = {
        .count = 1,
        .list = {
          { .id = GAME_NEW, .title = "New word", .data = wnd->data }
        }
      }
    },
    {
      .items = {
        .count = 1,
        .list = {
          { .id = GAME_QUIT, .title = "Quit", .data = wnd }
        }
      }
    }
  });

  menubar(1, (win95_menu*[]) {
    &menu_game
  });
}

static void game_window_proc(window_t *this) {
  static const char *keyboard_layout[3][10] = {
    { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" },
    { "A", "S", "D", "F", "G", "H", "J", "K", "L" },
    { "Z", "X", "C", "V", "B", "N", "M" }
  };
static const cig_key_code keyboard_layout_keycodes[3][10] = {
    { CIG_KEY_Q, CIG_KEY_W, CIG_KEY_E, CIG_KEY_R, CIG_KEY_T, CIG_KEY_Y, CIG_KEY_U, CIG_KEY_I, CIG_KEY_O, CIG_KEY_P },
    { CIG_KEY_A, CIG_KEY_S, CIG_KEY_D, CIG_KEY_F, CIG_KEY_G, CIG_KEY_H, CIG_KEY_J, CIG_KEY_K, CIG_KEY_L },
    { CIG_KEY_Z, CIG_KEY_X, CIG_KEY_C, CIG_KEY_V, CIG_KEY_B, CIG_KEY_N, CIG_KEY_M }
  };
  static char *win_messages[6] = {
    "Bill Approved",
    "MS Certified",
    "Gamer Mode",
    "Smort",
    "Legacy Win",
    "Failed Successfully"
  };
  register int i, j, k;
  cig_frame *container = cig_current();
  cig_frame *word_grid;
  game_data_st *game = (game_data_st *)this->data;

  if (game->animation.active) {
    if (game->animation.time > 0) {
      game->animation.time -= cig_delta_time();
    } else {
      game->animation.time = LETTER_APPEARANCE_TIME;
      game->animation.index ++;
      if (game->animation.index > 5) {
        game->animation.active = false;
      }
    }
  }

  CIG(BUILD_RECT(
    PIN(LEFT_OF(container)),
    PIN(RIGHT_OF(container)),
    PIN(BOTTOM_OF(container)),
    PIN(TOP_OF(container), OFFSET_BY(20))
  )) {
    cig_fill_color(get_color(COLOR_DIALOG_BACKGROUND));
    cig_fill_style(get_style(STYLE_FILES_CONTENT_BEVEL), 0);

    /* 6 by 5 letter grid */
    CIG_RETAIN(word_grid, CIG(BUILD_RECT(
      PIN(LEFT, 70, LEFT_OF(cig_current())),
      PIN(RIGHT, 70, RIGHT_OF(cig_current())),
      PIN(TOP, 24, TOP_OF(cig_current())),
      PIN(ASPECT, 1)
    )) {
      const int input_length = strlen(game->input);
      char letter[2];

      CIG_GRID(_, CIG_PARAMS({
        CIG_COLUMNS(5),
        CIG_ROWS(6),
        CIG_SPACING(3)
      })) {
        for (j = 0; j < 6; ++j) {
          for (i = 0; i < 5; ++i) {
            CIG(_) {
              if (game->animation.active && j == game->guesses_made - 1) {
                if (i < game->animation.index) {
                  cig_fill_color(get_color(result_color(game->guesses[j].results[i])));
                  k = game->guesses[j].word[i]-97;
                  if (game->guesses[j].results[i] > game->keyboard[k]) {
                    game->keyboard[k] = game->guesses[j].results[i];
                  }
                } else {
                  cig_fill_color(get_color(COLOR_WHITE));
                }
              }
              if (j < game->guesses_made) {
                if (j < game->guesses_made - (game->animation.active ? 1 : 0)) {
                  cig_fill_color(get_color(result_color(game->guesses[j].results[i])));
                }
                snprintf(letter, 2, "%c", toupper(game->guesses[j].word[i]));
                cig_draw_label((cig_text_properties) {
                  .font = get_font(FONT_ARIAL_BLACK_32)
                }, letter);
              } else if (i < input_length && j == game->guesses_made) {
                cig_fill_color(get_color(COLOR_WHITE));
                snprintf(letter, 2, "%c", toupper(game->input[i]));
                cig_draw_label((cig_text_properties) {
                  .font = get_font(FONT_ARIAL_BLACK_32)
                }, letter);
              }

              cig_fill_style(get_style(STYLE_INNER_BEVEL_NO_FILL), 0);
            }
          }
        }
      }
    })

    if ((!game->animation.active && game->final_result) || game->wrong_guess.visible) {
      CIG(BUILD_RECT(
        PIN(CENTER_X_OF(word_grid)),
        PIN(CENTER_Y_OF(word_grid)),
        PIN(WIDTH, 280),
        PIN(HEIGHT, 44)
      )) {
        CIG(_) {
          if (game->wrong_guess.visible) {
            game->wrong_guess.time -= cig_delta_time();
            if (game->wrong_guess.time > 0.f) {
              cig_draw_rect(cig_absolute_rect(), get_color(COLOR_WINDOW_INACTIVE_TITLEBAR), get_color(COLOR_BLACK), 1.0);
              cig_draw_label((cig_text_properties) {
                .font = get_font(FONT_TIMES_NEW_ROMAN_32_BOLD),
                .color = get_color(COLOR_WHITE)
              }, "Nuh uh!");
            } else {
              game->wrong_guess.visible = false;
            }
          } else if (game->final_result == GAME_WON) {
            cig_draw_rect(cig_absolute_rect(), get_color(COLOR_WHITE), get_color(COLOR_BLACK), 1.0);
            cig_draw_label((cig_text_properties) {
              .font = get_font(FONT_TIMES_NEW_ROMAN_32_BOLD)
            }, win_messages[game->guesses_made-1]);
          } else {
            cig_draw_rect(cig_absolute_rect(), get_color(COLOR_WINDOW_INACTIVE_TITLEBAR), get_color(COLOR_BLACK), 1.0);
            cig_draw_label((cig_text_properties) {
              .font = get_font(FONT_TIMES_NEW_ROMAN_32_BOLD),
              .color = get_color(COLOR_WHITE)
            }, game->word_uppercase);
          }
        }
      }
    }

    const int key_w = 32;
    const int key_h = 28;
    const int key_spacing = 0;

    /* Virtual keyboard */
    CIG_VSTACK(BUILD_RECT(
      PIN(BELOW(word_grid), OFFSET_BY(10)),
      PIN(LEFT_OF(cig_current())),
      PIN(RIGHT_OF(cig_current())),
      PIN(BOTTOM_OF(cig_current()))
    ), CIG_INSETS(cig_i_uniform(8)), CIG_PARAMS({
      CIG_ALIGNMENT_HORIZONTAL(CIG_LAYOUT_ALIGNS_CENTER),
      CIG_HEIGHT(key_h),
      CIG_SPACING(key_spacing)
    })) {
      button_font = get_font(FONT_BOLD);

      /* Row 0: 10 keys */
      CIG_HSTACK(
        _W((10*key_w)+(9*key_spacing)),
        CIG_PARAMS({
          CIG_WIDTH(key_w),
          CIG_SPACING(key_spacing)
        })
      ) {
        for (i = 0; i < 10; ++i) {
          if (keyboard_button(game, _, keyboard_layout[0][i], keyboard_layout_keycodes[0][i])) {
            append_input(game, keyboard_layout[0][i]);
          }
        }
      }

      /* Row 1: 9 keys */
      CIG_HSTACK(
        _W((9*key_w)+(8*key_spacing)),
        CIG_PARAMS({
          CIG_WIDTH(key_w),
          CIG_SPACING(key_spacing)
        })
      ) {
        for (i = 0; i < 9; ++i) {
          if (keyboard_button(game, _, keyboard_layout[1][i], keyboard_layout_keycodes[1][i])) {
            append_input(game, keyboard_layout[1][i]);
          }
        }
      }

      /* Row 3: 7 keys + 2 specials */
      CIG_HSTACK(
        _W((10*key_w)+(9*key_spacing)), /* Same width as row 0 */
        CIG_PARAMS({
          CIG_WIDTH(key_w),
          CIG_SPACING(key_spacing)
        })
      ) {
        const int special_key_w = key_w * 1.5 + key_spacing;
        if (keyboard_button(game, _W(special_key_w), "ENTER", CIG_KEY_ENTER)) {
          submit_word(game);
        }
        for (i = 0; i < 7; ++i) {
          if (keyboard_button(game, _, keyboard_layout[2][i], keyboard_layout_keycodes[2][i])) {
            append_input(game, keyboard_layout[2][i]);
          }
        }
        button_is_pressed = cig_focused_keys(CIG_KEYS(CIG_KEY_BACKSPACE), CIG_KEY_PRESSED);
        if (icon_button(_W(special_key_w), IMAGE_CROSS) || cig_focused_keys(CIG_KEYS(CIG_KEY_BACKSPACE), CIG_KEY_CLICKED)) {
          trim_input(game);
        }
      }

      button_font = NULL;
    }
  }

  CIG(BUILD_RECT(
    PIN(LEFT_OF(container)),
    PIN(RIGHT_OF(container)),
    PIN(HEIGHT, 18),
    PIN(TOP_OF(container))
  )) {
    game_menubar(this);
  }
}

application_t wordwiz_app() {
  if (!word_count) {
    load_dictionary("res/words.txt", &words, &word_count);
  }
  if (!all_words_count) {
    load_dictionary("res/all_words.txt", &all_words, &all_words_count);
  }

  game_data_st *data = malloc(sizeof(game_data_st));
  game_reset(data);

  return (application_t) {
    .id = "wordwiz",
    .windows = {
      (window_t) {
        .id = cig_hash("wordwiz"),
        .proc = &game_window_proc,
        .data = data,
        .rect = CENTER_APP_WINDOW(360, 400),
        .title = "WordWiz",
        .icon = IMAGE_WORDWIZ_16,
        .flags = IS_PRIMARY_WINDOW
      }
    },
    .data = NULL,
    .flags = KILL_WHEN_PRIMARY_WINDOW_CLOSED
  };
}
