#include "raylib.h"
#include "rlgl.h"
#include "cig.h"
#include "win95.h"
#include "cigext.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define UNPACK_RECT(R) R.x, R.y, R.w, R.h
#define RAYLIB_RECT(R) (Rectangle ) { R.x, R.y, R.w, R.h } 
#define RAYLIB_VEC2(V) (Vector2) { V.x, V.y }

static const int raylib_key_table[CIG__KEY_COUNT] = {
  [CIG_KEY_NONE] = KEY_NULL,

  /* Letters */
  [CIG_KEY_A] = KEY_A,
  [CIG_KEY_B] = KEY_B,
  [CIG_KEY_C] = KEY_C,
  [CIG_KEY_D] = KEY_D,
  [CIG_KEY_E] = KEY_E,
  [CIG_KEY_F] = KEY_F,
  [CIG_KEY_G] = KEY_G,
  [CIG_KEY_H] = KEY_H,
  [CIG_KEY_I] = KEY_I,
  [CIG_KEY_J] = KEY_J,
  [CIG_KEY_K] = KEY_K,
  [CIG_KEY_L] = KEY_L,
  [CIG_KEY_M] = KEY_M,
  [CIG_KEY_N] = KEY_N,
  [CIG_KEY_O] = KEY_O,
  [CIG_KEY_P] = KEY_P,
  [CIG_KEY_Q] = KEY_Q,
  [CIG_KEY_R] = KEY_R,
  [CIG_KEY_S] = KEY_S,
  [CIG_KEY_T] = KEY_T,
  [CIG_KEY_U] = KEY_U,
  [CIG_KEY_V] = KEY_V,
  [CIG_KEY_W] = KEY_W,
  [CIG_KEY_X] = KEY_X,
  [CIG_KEY_Y] = KEY_Y,
  [CIG_KEY_Z] = KEY_Z,

  /* Numbers */
  [CIG_KEY_0] = KEY_ZERO,
  [CIG_KEY_1] = KEY_ONE,
  [CIG_KEY_2] = KEY_TWO,
  [CIG_KEY_3] = KEY_THREE,
  [CIG_KEY_4] = KEY_FOUR,
  [CIG_KEY_5] = KEY_FIVE,
  [CIG_KEY_6] = KEY_SIX,
  [CIG_KEY_7] = KEY_SEVEN,
  [CIG_KEY_8] = KEY_EIGHT,
  [CIG_KEY_9] = KEY_NINE,

  /* Keypad */
  [CIG_KEY_KP_0] = KEY_KP_0,
  [CIG_KEY_KP_1] = KEY_KP_1,
  [CIG_KEY_KP_2] = KEY_KP_2,
  [CIG_KEY_KP_3] = KEY_KP_3,
  [CIG_KEY_KP_4] = KEY_KP_4,
  [CIG_KEY_KP_5] = KEY_KP_5,
  [CIG_KEY_KP_6] = KEY_KP_6,
  [CIG_KEY_KP_7] = KEY_KP_7,
  [CIG_KEY_KP_8] = KEY_KP_8,
  [CIG_KEY_KP_9] = KEY_KP_9,
  [CIG_KEY_KP_DECIMAL] = KEY_KP_DECIMAL,
  [CIG_KEY_KP_DIVIDE] = KEY_KP_DIVIDE,
  [CIG_KEY_KP_MULTIPLY] = KEY_KP_MULTIPLY,
  [CIG_KEY_KP_SUBTRACT] = KEY_KP_SUBTRACT,
  [CIG_KEY_KP_ADD] = KEY_KP_ADD,
  [CIG_KEY_KP_ENTER] = KEY_KP_ENTER,
  [CIG_KEY_KP_EQUAL] = KEY_KP_EQUAL,

  /* Controls */
  [CIG_KEY_ENTER] = KEY_ENTER,
  [CIG_KEY_ESCAPE] = KEY_ESCAPE,
  [CIG_KEY_BACKSPACE] = KEY_BACKSPACE,
  [CIG_KEY_TAB] = KEY_TAB,
  [CIG_KEY_SPACE] = KEY_SPACE,

  /* Navigation */
  [CIG_KEY_LEFT] = KEY_LEFT,
  [CIG_KEY_RIGHT] = KEY_RIGHT,
  [CIG_KEY_UP] = KEY_UP,
  [CIG_KEY_DOWN] = KEY_DOWN,

  [CIG_KEY_HOME] = KEY_HOME,
  [CIG_KEY_END] = KEY_END,
  [CIG_KEY_PAGE_UP] = KEY_PAGE_UP,
  [CIG_KEY_PAGE_DOWN] = KEY_PAGE_DOWN,
  [CIG_KEY_INSERT] = KEY_INSERT,
  [CIG_KEY_DELETE] = KEY_DELETE,

  /* Modifiers */
  [CIG_KEY_LSHIFT] = KEY_LEFT_SHIFT,
  [CIG_KEY_RSHIFT] = KEY_RIGHT_SHIFT,
  [CIG_KEY_LCTRL] = KEY_LEFT_CONTROL,
  [CIG_KEY_RCTRL] = KEY_RIGHT_CONTROL,
  [CIG_KEY_LALT] = KEY_LEFT_ALT,
  [CIG_KEY_RALT] = KEY_RIGHT_ALT,

  /* Symbols */
  [CIG_KEY_MINUS] = KEY_MINUS,
  [CIG_KEY_EQUAL] = KEY_EQUAL,
  [CIG_KEY_PERIOD] = KEY_PERIOD,
  [CIG_KEY_COMMA] = KEY_COMMA,

  /* Function keys */
  [CIG_KEY_F1] = KEY_F1,
  [CIG_KEY_F2] = KEY_F2,
  [CIG_KEY_F3] = KEY_F3,
  [CIG_KEY_F4] = KEY_F4,
  [CIG_KEY_F5] = KEY_F5,
  [CIG_KEY_F6] = KEY_F6,
  [CIG_KEY_F7] = KEY_F7,
  [CIG_KEY_F8] = KEY_F8,
  [CIG_KEY_F9] = KEY_F9,
  [CIG_KEY_F10] = KEY_F10,
  [CIG_KEY_F11] = KEY_F11,
  [CIG_KEY_F12] = KEY_F12,
};

static cig_context ctx = { 0 };

static struct font_store {
  Font font;
  int baseline_offset;
} fonts[__FONT_COUNT];
static Color colors[__COLOR_COUNT];
static int panel_styles[__STYLE_COUNT];
static Texture2D images[__IMAGE_COUNT];
static Shader blue_dither_shader;
static bool dithering_shader_enabled = false;
static bool is_verbose = false;
static RenderTexture2D render_texture;

static struct {
  RenderTexture2D *stack[8];
  int depth;
} rt_stack = { 0 };

static struct {
  struct {
    RenderTexture2D *texture;
    cig_v size;
  } items[8];
  int count;
} rt_resize_queue = { 0 };

/*  Core API */
static void set_clip_rect(cig_buffer_ref, cig_r, bool);

/*  Text API */
static void render_text(
  const char *,
  size_t,
  cig_r,
  cig_font_ref,
  cig_text_color_ref,
  cig_text_style
);
static cig_v measure_text(
  const char *,
  size_t,
  cig_font_ref,
  cig_text_style
);
static cig_font_info_st font_query(cig_font_ref);

/* Gfx API */
static void draw_image(cig_buffer_ref, cig_r, cig_r, cig_image_ref, cig_image_mode);
static cig_v measure_image(cig_image_ref);
static void draw_style(cig_style_ref, cig_r, cig_style_modifiers);
static void draw_rectangle(cig_color_ref, cig_color_ref, cig_r, unsigned int);
static void draw_line(cig_color_ref, cig_v, cig_v, float);
static void draw_polygon(cig_v, cig_v*, size_t, cig_color_ref);

#ifdef DEBUG
static RenderTexture2D debug_texture;
static void layout_breakpoint(cig_r, cig_r);
#endif

void* get_font(font_id_t id) {
  return &fonts[id];
}

void* get_color(color_id_t id) {
  return &colors[id];
}

void* get_image(image_id_t id) {
  return &images[id];
}

void* get_style(style_id_t id) {
  return &panel_styles[id];
}

void renderer_enable_blue_selection_dithering(bool enabled) {
  dithering_shader_enabled = enabled;
}

cig_buffer_ref
framebuffer_create(int w, int h)
{
  RenderTexture2D *tex2d = malloc(sizeof(RenderTexture2D));
  *tex2d = LoadRenderTexture(w, h);
  SetTextureFilter(tex2d->texture, TEXTURE_FILTER_POINT);
  return tex2d;
}

void
framebuffer_free(cig_buffer_ref buffer)
{
  UnloadRenderTexture(*(RenderTexture2D *)buffer);
  free(buffer);
}

bool
framebuffer_resize_if_needed(cig_buffer_ref buffer, int w, int h)
{
  RenderTexture2D *tex = (RenderTexture2D *)buffer;
  int step = 50; // make configurable?

  int nw = ((w + step) / step) * step;
  int nh = ((h + step) / step) * step;

  if (nw > tex->texture.width || nh > tex->texture.height) {
    /* Size up */
    // printf("Resize UP from %d, %d to %d, %d (%d, %d)\n", tex->texture.width, tex->texture.height, nw, nh, w, h);
    rt_resize_queue.items[rt_resize_queue.count].texture = tex;
    rt_resize_queue.items[rt_resize_queue.count].size = cig_v_make(nw, nh);
    rt_resize_queue.count ++;
    return true;
  } else if (nw < (int)tex->texture.width - step || nh < (int)tex->texture.height - step) {
    /* Size down */
    // printf("Resize DOWN from %d, %d to %d, %d (%d, %d)\n", tex->texture.width, tex->texture.height, nw, nh, w, h);
    rt_resize_queue.items[rt_resize_queue.count].texture = tex;
    rt_resize_queue.items[rt_resize_queue.count].size = cig_v_make(nw, nh);
    rt_resize_queue.count ++;
    return true;
  }

  return false;
}

void renderer_push_buffer(cig_buffer_ref buffer)
{
  RenderTexture2D *tex = (RenderTexture2D *)buffer;
  rt_stack.stack[rt_stack.depth++] = tex;
  BeginTextureMode(*tex);
}

void renderer_pop_buffer(cig_r rect)
{
  rt_stack.depth -= 1;

  RenderTexture2D *src = rt_stack.stack[rt_stack.depth];

  if (rt_stack.depth > 0) {
    RenderTexture2D *dst = rt_stack.stack[rt_stack.depth-1];

    BeginTextureMode(*dst);

    DrawTexturePro(
      src->texture,
      (Rectangle) { 0, src->texture.height - rect.h, rect.w, -rect.h }, // (float)tex->texture.width, (float)-tex->texture.height },
      (Rectangle) { rect.x, rect.y, rect.w, rect.h },
      (Vector2) { 0, 0 },
      0,
      WHITE
    );
  } else {
    EndTextureMode();

    DrawTexturePro(
      src->texture,
      (Rectangle) { 0, 0, rect.w, -rect.h }, // (float)tex->texture.width, (float)-tex->texture.height },
      (Rectangle) { rect.x, rect.y, GetScreenWidth(), GetScreenHeight() },
      (Vector2) { 0, 0 },
      0,
      WHITE
    );
  }
}

void
renderer_clear(void)
{
  ClearBackground((Color) { 0, 0, 0, 0 });
}

static void
process_render_texture_resize_queue()
{
  int i;

  for (i = 0; i < rt_resize_queue.count; ++i) {
    UnloadRenderTexture(*rt_resize_queue.items[i].texture);
    *rt_resize_queue.items[i].texture = LoadRenderTexture(rt_resize_queue.items[i].size.x, rt_resize_queue.items[i].size.y);
    SetTextureFilter(rt_resize_queue.items[i].texture->texture, TEXTURE_FILTER_POINT);
  }

  rt_resize_queue.count = 0;
}

M_INLINED void load_texture(Texture2D *dst, const char *path)
{
  *dst = LoadTexture(path);
  SetTextureFilter(*dst, TEXTURE_FILTER_POINT);
}

static void*
demo_alloc(void *ud, size_t size, size_t align)
{
  void *new_bytes = malloc(size);
  assert(new_bytes);
  if (is_verbose) {
    printf("[MEM] alloc %lld bytes (0x%p)\n", size, new_bytes);
  }
  memset(new_bytes, 0, size);
  return new_bytes;
}

static void*
demo_realloc(void *ud, void *ptr, size_t old_size, size_t new_size)
{
  if (is_verbose) {
    printf("[MEM] realloc %lld bytes -> %lld bytes (0x%p)\n", old_size, new_size, ptr);
  }
  void *new_bytes = realloc(ptr, new_size);
  assert(new_bytes);
  memset(new_bytes + old_size, 0, new_size - old_size);
  return new_bytes;
}

static void
demo_free(void *ud, void *ptr)
{
  assert(ptr);
  if (is_verbose) {
    printf("[MEM] free (0x%p)\n", ptr);
  }
  free(ptr);
}

static int win95_w, win95_h;
static double scale;

static void
set_up_render_textures()
{
  UnloadRenderTexture(render_texture);
  render_texture = LoadRenderTexture(win95_w, win95_h);
  SetTextureFilter(render_texture.texture, TEXTURE_FILTER_POINT);

#ifdef DEBUG
  debug_texture = LoadRenderTexture(render_texture.texture.width, render_texture.texture.height);
  SetTextureFilter(render_texture.texture, TEXTURE_FILTER_POINT);
#endif
}

static void
check_alt_enter()
{
  if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
    int ray_w, ray_h;
    if (IsWindowFullscreen()) {
      ray_w = 1280;
      ray_h = 960;
    } else {
      int display = GetCurrentMonitor(); 
      ray_w = GetMonitorWidth(display);
      ray_h = GetMonitorHeight(display);
    }

    win95_w = ray_w * scale;
    win95_h = ray_h * scale;

    ToggleFullscreen();
    SetWindowSize(ray_w, ray_h);

    set_up_render_textures();
  }
}

int
main(int argc, const char *argv[])
{
  srand(time(NULL));

  bool run_fullscreen = false;
  int i, ray_w, ray_h;

  for (i = 1; i < argc; ++i) {
    if (!strcmp("-f", argv[i])) {
      run_fullscreen = true;
    } else if (!strcmp("-v", argv[i])) {
      is_verbose = true;
    }
  }

  SetTraceLogLevel(LOG_WARNING);

  if (run_fullscreen) {
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, APP_WINDOW_TITLE);
    ray_w = GetMonitorWidth(GetCurrentMonitor());
    ray_h = GetMonitorHeight(GetCurrentMonitor());
    scale = 0.5;
  } else {
    ray_w = 1280;
    ray_h = 960;
    scale = 0.5;
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(ray_w, ray_h, APP_WINDOW_TITLE);
  }

  win95_w = ray_w * scale;
  win95_h = ray_h * scale;

  SetTargetFPS(90);
  SetExitKey(0);

  // TODO: Would be neater to package multiple sizes and reference these simply as "name@16" or something
  load_texture(&images[IMAGE_BRIGHT_YELLOW_PATTERN], "res/images/light_yellow_pattern.png");
  load_texture(&images[IMAGE_GRAY_DITHER], "res/images/gray_dither.png");
  load_texture(&images[IMAGE_START_ICON], "res/images/start.png");
  load_texture(&images[IMAGE_LOGO_TEXT], "res/images/logotext.png");
  load_texture(&images[IMAGE_START_SIDEBAR], "res/images/start_sidebar.png");
  load_texture(&images[IMAGE_MY_COMPUTER_16], "res/images/my_computer.png");
  load_texture(&images[IMAGE_MY_COMPUTER_32], "res/images/my_computer_32.png");
  load_texture(&images[IMAGE_TIP_OF_THE_DAY], "res/images/tip_of_the_day.png");
  load_texture(&images[IMAGE_CHECKMARK], "res/images/check.png");
  load_texture(&images[IMAGE_CROSS], "res/images/cross.png");
  load_texture(&images[IMAGE_MAXIMIZE], "res/images/maximize.png");
  load_texture(&images[IMAGE_MINIMIZE], "res/images/minimize.png");
  load_texture(&images[IMAGE_RESTORE], "res/images/restore.png");
  load_texture(&images[IMAGE_SCROLL_UP], "res/images/scroll_up.png");
  load_texture(&images[IMAGE_SCROLL_DOWN], "res/images/scroll_down.png");
  load_texture(&images[IMAGE_SCROLL_LEFT], "res/images/scroll_left.png");
  load_texture(&images[IMAGE_SCROLL_RIGHT], "res/images/scroll_right.png");
  load_texture(&images[IMAGE_WELCOME_APP_ICON], "res/images/welcome.png");
  load_texture(&images[IMAGE_BIN_EMPTY], "res/images/bin_empty.png");
  load_texture(&images[IMAGE_BIN_EMPTY_16], "res/images/bin_16.png");
  load_texture(&images[IMAGE_DRIVE_A_16], "res/images/drive_a_16.png");
  load_texture(&images[IMAGE_DRIVE_A_32], "res/images/drive_a_32.png");
  load_texture(&images[IMAGE_DRIVE_C_16], "res/images/drive_c_16.png");
  load_texture(&images[IMAGE_DRIVE_C_32], "res/images/drive_c_32.png");
  load_texture(&images[IMAGE_DRIVE_D_16], "res/images/drive_d_16.png");
  load_texture(&images[IMAGE_DRIVE_D_32], "res/images/drive_d_32.png");
  load_texture(&images[IMAGE_CONTROLS_FOLDER_16], "res/images/controls_folder_16.png");
  load_texture(&images[IMAGE_CONTROLS_FOLDER_32], "res/images/controls_folder_32.png");
  load_texture(&images[IMAGE_PRINTERS_FOLDER_16], "res/images/printers_folder_16.png");
  load_texture(&images[IMAGE_PRINTERS_FOLDER_32], "res/images/printers_folder_32.png");
  load_texture(&images[IMAGE_DIAL_UP_FOLDER_16], "res/images/dial_up_folder_16.png");
  load_texture(&images[IMAGE_DIAL_UP_FOLDER_32], "res/images/dial_up_folder_32.png");
  load_texture(&images[IMAGE_RESIZE_HANDLE], "res/images/resize_handle.png");
  load_texture(&images[IMAGE_MENU_CHECK], "res/images/menu_check.png");
  load_texture(&images[IMAGE_MENU_CHECK_INVERTED], "res/images/menu_check_inverted.png");
  load_texture(&images[IMAGE_MENU_RADIO], "res/images/menu_radio.png");
  load_texture(&images[IMAGE_MENU_RADIO_INVERTED], "res/images/menu_radio_inverted.png");
  load_texture(&images[IMAGE_MENU_ARROW], "res/images/menu_arrow.png");
  load_texture(&images[IMAGE_MENU_ARROW_INVERTED], "res/images/menu_arrow_inverted.png");
  load_texture(&images[IMAGE_PROGRAM_FOLDER_24], "res/images/program_folder_24.png");
  load_texture(&images[IMAGE_PROGRAM_FOLDER_16], "res/images/program_folder_16.png");
  load_texture(&images[IMAGE_DOCUMENTS_24], "res/images/documents_24.png");
  load_texture(&images[IMAGE_SETTINGS_24], "res/images/settings_24.png");
  load_texture(&images[IMAGE_FIND_24], "res/images/find_24.png");
  load_texture(&images[IMAGE_HELP_24], "res/images/help_24.png");
  load_texture(&images[IMAGE_RUN_24], "res/images/run_24.png");
  load_texture(&images[IMAGE_SHUT_DOWN_24], "res/images/shut_down_24.png");
  load_texture(&images[IMAGE_MAIL_16], "res/images/mail_16.png");
  load_texture(&images[IMAGE_MSDOS_16], "res/images/msdos_16.png");
  load_texture(&images[IMAGE_MSN_16], "res/images/msn_16.png");
  load_texture(&images[IMAGE_EXPLORER_16], "res/images/explorer_16.png");
  load_texture(&images[IMAGE_CALCULATOR_16], "res/images/calculator_16.png");
  load_texture(&images[IMAGE_NOTEPAD_16], "res/images/notepad_16.png");
  load_texture(&images[IMAGE_PAINT_16], "res/images/paint_16.png");
  load_texture(&images[IMAGE_WORDWIZ_16], "res/images/wordwiz_16.png");
  load_texture(&images[IMAGE_CLOCK_32], "res/images/clock_32.png");
  load_texture(&images[IMAGE_CLOCK_16], "res/images/clock_16.png");

  blue_dither_shader = LoadShader(0, "res/shaders/blue_dither.fs");

  fonts[FONT_REGULAR].font = LoadFont("res/fonts/winr.fnt");
  fonts[FONT_REGULAR].baseline_offset = -2;
  
  fonts[FONT_BOLD].font = LoadFont("res/fonts/winb.fnt");
  fonts[FONT_BOLD].baseline_offset = -2;
  
  fonts[FONT_TIMES_NEW_ROMAN_32_BOLD].font = LoadFont("res/fonts/tnr32b.fnt");
  fonts[FONT_TIMES_NEW_ROMAN_32_BOLD].baseline_offset = -6;
  
  fonts[FONT_ARIAL_BLACK_32].font = LoadFont("res/fonts/arbl32.fnt");
  fonts[FONT_ARIAL_BLACK_32].baseline_offset = -7;
  
  fonts[FONT_FRANKLIN_GOTHIC_BOOK_32].font = LoadFont("res/fonts/gothbook32.fnt");
  fonts[FONT_FRANKLIN_GOTHIC_BOOK_32].baseline_offset = -8;

  SetTextureFilter(fonts[FONT_REGULAR].font.texture, TEXTURE_FILTER_POINT);
  SetTextureFilter(fonts[FONT_BOLD].font.texture, TEXTURE_FILTER_POINT);
  SetTextureFilter(fonts[FONT_TIMES_NEW_ROMAN_32_BOLD].font.texture, TEXTURE_FILTER_POINT);
  SetTextureFilter(fonts[FONT_ARIAL_BLACK_32].font.texture, TEXTURE_FILTER_POINT);
  SetTextureFilter(fonts[FONT_FRANKLIN_GOTHIC_BOOK_32].font.texture, TEXTURE_FILTER_POINT);
  
  colors[COLOR_DEBUG] = (Color) { 255, 0, 255, 255 };
  colors[COLOR_BLACK] = (Color) { 0, 0, 0, 255 };
  colors[COLOR_WHITE] = (Color) { 255, 255, 255, 255 };
  colors[COLOR_YELLOW] = (Color) { 255, 255, 0, 255 };
  colors[COLOR_GREEN] = (Color) { 0, 128, 0, 255 };
  colors[COLOR_RED] = (Color) { 255, 0, 0, 255 };
  colors[COLOR_MAROON] = (Color) { 128, 0, 0, 255 };
  colors[COLOR_BLUE] = (Color) { 0, 0, 255, 255 };
  colors[COLOR_NAVY] = (Color) { 0, 0, 128, 255 };
  colors[COLOR_LIGHT_CYAN] = (Color) { 0, 255, 255, 255 };
  colors[COLOR_DARK_GRAY] = (Color) { 127, 127, 127, 255 };
  colors[COLOR_LIGHT_GRAY] = (Color) { 192, 192, 192, 255 };
  colors[COLOR_DESKTOP_BG] = (Color) { 0, 127, 127, 255 };
  colors[COLOR_DIALOG_BACKGROUND] = (Color) { 192, 192, 192, 255 };
  colors[COLOR_WINDOW_ACTIVE_TITLEBAR] = (Color) { 0, 0, 127, 255 };
  colors[COLOR_WINDOW_INACTIVE_TITLEBAR] = (Color) { 127, 127, 127, 255 };
  colors[COLOR_CLOCK_HAND_SHADOW] = (Color) { 127, 127, 127, 255 };
  colors[COLOR_CLOCK_HAND] = (Color) { 0, 127, 127, 255 };
  colors[COLOR_CLOCK_SECONDS_HAND] = (Color) { 63, 63, 63, 255 };
  
  for (i = 0; i < __STYLE_COUNT; ++i) { panel_styles[i] = i; }

  cig_init_context(&ctx);

  cig_set_allocator(&ctx, (cig_allocator) {
    .alloc = demo_alloc,
    .realloc = demo_realloc,
    .free = demo_free,
    .ud = NULL
  });

  cig_assign_set_clip(&set_clip_rect);
  
  cig_assign_draw_text(&render_text);
  cig_assign_measure_text(&measure_text);
  cig_assign_query_font(&font_query);

  cig_set_default_font(&fonts[FONT_REGULAR]);
  cig_set_default_text_color(&colors[COLOR_BLACK]);

  cig_assign_draw_image(&draw_image);
  cig_assign_measure_image(&measure_image);
  cig_assign_draw_style(&draw_style);
  cig_assign_draw_rectangle(&draw_rectangle);
  cig_assign_draw_line(&draw_line);
  cig_assign_draw_polygon(&draw_polygon);

#ifdef DEBUG
  cig_set_layout_breakpoint_callback(&layout_breakpoint);
#endif
  
  /*
   * Calling begin layout here, so that when win95 instance starts,
   * it already has a size reference to center some welcome windows into.
   */
  cig_begin_layout(&ctx, &render_texture, cig_r_make(0, 0, win95_w, win95_h), 0.f);

  win95_t win_instance = { 0 };
  win95_initialize(&win_instance);

  set_up_render_textures();

  bool running = true;

  while (!WindowShouldClose() && running) {
    check_alt_enter();

    if (IsWindowResized()) {
      win95_w = GetScreenWidth() * scale;
      win95_h = GetScreenHeight() * scale;
      set_up_render_textures();
    }

    BeginDrawing();

#ifdef DEBUG
    if (IsKeyPressed(KEY_F10)) {
      cig_enable_debug_stepper();
    }
#endif

    rt_stack.depth = 0;
    rt_resize_queue.count = 0;

    renderer_push_buffer(&render_texture);

#ifdef DEBUG
    ClearBackground((Color){0});
#endif

    cig_begin_layout(&ctx, &render_texture, cig_r_make(0, 0, win95_w, win95_h), GetFrameTime());
    
    /* Update [mouse] pointer position and button states */
    cig_set_pointer_position(cig_v_make(GetMouseX()*scale, GetMouseY()*scale));
    cig_set_pointer_state(
      (IsMouseButtonDown(MOUSE_BUTTON_LEFT)  ? CIG_INPUT_PRIMARY_ACTION   : 0) +
      (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? CIG_INPUT_SECONDARY_ACTION : 0)
    );

    /* Update key states (pressed / not pressed). CIG handles the rest, like repeating etc. */
    for (i = 0; i < CIG__KEY_COUNT; ++i) {
      cig_set_key_state((cig_key_code)i, IsKeyDown(raylib_key_table[i]));
    }

    running = win95_run();
    
    cig_end_layout();

    renderer_pop_buffer(cig_r_make(0, 0, render_texture.texture.width, render_texture.texture.height));

    EndDrawing();

    /* Process render texture resize queue */
    process_render_texture_resize_queue();
  }

  UnloadRenderTexture(render_texture); 
  CloseWindow();

  return 0;
}

M_INLINED void set_clip_rect(cig_buffer_ref buffer, cig_r rect, bool reset) {
  if (reset) {
    EndScissorMode();
  } else {
    BeginScissorMode(UNPACK_RECT(rect));
  }
}

static char text_api_buffer[4096];

M_INLINED void render_text(
  const char *str,
  size_t len,
  cig_r rect,
  cig_font_ref font,
  cig_text_color_ref color,
  cig_text_style style
) {
  strncpy(text_api_buffer, str, len);
  text_api_buffer[len] = '\0';
  
  struct font_store *fs = (struct font_store*)font;

  // printf("render: |%s|\n", buf);
  
  // DrawRectangle(UNPACK_RECT(rect), GREEN);
  DrawTextEx(fs->font, text_api_buffer, (Vector2) { rect.x, rect.y }, fs->font.baseSize, 0, *(Color*)color);
}

M_INLINED cig_v measure_text(
  const char *str,
  size_t len,
  cig_font_ref font,
  cig_text_style style
) {
  strncpy(text_api_buffer, str, len);
  text_api_buffer[len] = '\0';
  
  struct font_store *fs = (struct font_store*)font;
  Vector2 bounds = MeasureTextEx(fs->font, text_api_buffer, fs->font.baseSize, 0);
  
  return cig_v_make(bounds.x, bounds.y);
}

M_INLINED cig_font_info_st font_query(cig_font_ref font_ref) {
  struct font_store *fs = (struct font_store*)font_ref;
  
  // printf("GLYPH PADDING %d\n", fs->font.glyphPadding);
  
  return (cig_font_info_st) {
    .height = fs->font.baseSize,
    .baseline_offset = fs->baseline_offset
  };
}

M_INLINED void draw_style(cig_style_ref style_ref, cig_r rect, cig_style_modifiers modifiers) {
  const int style = *(int*)style_ref;
  
  switch (style) {
    case STYLE_STANDARD_DIALOG: {
      DrawRectangleRec(RAYLIB_RECT(rect), colors[COLOR_DIALOG_BACKGROUND]);

      DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w, rect.y + rect.h - 1, (Color) { 0, 0, 0, 255 });
      DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h - 1, (Color) { 0, 0, 0, 255 });
      DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x + 1, rect.y + 1, rect.x + rect.w - 2, rect.y + 1, (Color) { 255, 255, 255, 255 });
      DrawLine(rect.x + 2, rect.y + 1, rect.x + 2, rect.y + rect.h - 2, (Color) { 255, 255, 255, 255 });
    } break;
    
    case STYLE_BUTTON: {
      if (modifiers & CIG_STYLE_APPLY_SELECTION) {
        DrawTexturePro(images[IMAGE_GRAY_DITHER], (Rectangle) { 0, 0, rect.w-4, rect.h-5 }, (Rectangle) { rect.x+2, rect.y+3, rect.w-4, rect.h-5 }, (Vector2){ 0, 0 }, 0, WHITE);
        DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + 1, rect.y + 1, rect.x + 1, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 }); 
        DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 223, 223, 223, 255 });
        DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 223, 223, 223, 255 });
        DrawLine(rect.x + 2, rect.y + 1, rect.x + rect.w - 2, rect.y + 1, (Color){ 128, 128, 128, 255 });
        DrawLine(rect.x + 2, rect.y + 1, rect.x + 2, rect.y + rect.h - 2, (Color){ 128, 128, 128, 255 });
        DrawLine(rect.x + 2, rect.y + 2, rect.x + rect.w - 2, rect.y + 2, (Color){ 255, 255, 255, 255 });
      } else if (modifiers & CIG_STYLE_APPLY_PRESS) {
        DrawRectangle(UNPACK_RECT(rect), colors[COLOR_DIALOG_BACKGROUND]);
        DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + 1, rect.y + 1, rect.x + 1, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 }); 
        DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 223, 223, 223, 255 });
        DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 223, 223, 223, 255 });
        DrawLine(rect.x + 2, rect.y + 1, rect.x + rect.w - 2, rect.y + 1, (Color){ 128, 128, 128, 255 });
        DrawLine(rect.x + 2, rect.y + 1, rect.x + 2, rect.y + rect.h - 2, (Color){ 128, 128, 128, 255 });
      } else {
        DrawRectangle(UNPACK_RECT(rect), colors[COLOR_DIALOG_BACKGROUND]);
        DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x + 1, rect.y + 1, rect.x + 1, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 130, 130, 130, 255 });
        DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 130, 130, 130, 255 });
      }
    } break;
    
    case STYLE_LIGHT_YELLOW: {
      DrawTexturePro(images[IMAGE_BRIGHT_YELLOW_PATTERN], (Rectangle) { 0, 0, rect.w, rect.h }, (Rectangle) { rect.x, rect.y, rect.w, rect.h }, (Vector2){ 0, 0 }, 0, WHITE);
    } break;

    case STYLE_GRAY_DITHER: {
      DrawTexturePro(images[IMAGE_GRAY_DITHER], (Rectangle) { 0, 0, rect.w, rect.h }, (Rectangle) { rect.x, rect.y, rect.w, rect.h }, (Vector2){ 0, 0 }, 0, WHITE);
    } break;
    
    case STYLE_INNER_BEVEL_NO_FILL: {
      DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x + 1, rect.y, rect.x + 1, rect.y + rect.h - 1, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w , rect.y + rect.h - 1, (Color) { 255, 255, 255, 255 });
      DrawLine(rect.x + rect.w , rect.y, rect.x + rect.w , rect.y + rect.h, (Color) { 255, 255, 255, 255 });
    } break;

  case STYLE_FILES_CONTENT_BEVEL:
    {
      DrawLine(rect.x, rect.y, rect.x + rect.w - 1, rect.y, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x + 1, rect.y, rect.x + 1, rect.y + rect.h - 1, (Color) { 130, 130, 130, 255 });
      DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w , rect.y + rect.h - 1, (Color) { 255, 255, 255, 255 });
      DrawLine(rect.x + rect.w , rect.y, rect.x + rect.w , rect.y + rect.h, (Color) { 255, 255, 255, 255 });
      DrawLine(rect.x + 1, rect.y + 1, rect.x + rect.w - 2, rect.y + 1, (Color){ 0, 0, 0, 255 });
      DrawLine(rect.x + 2, rect.y + 2, rect.x + 2, rect.y + rect.h - 2, (Color){ 0, 0, 0, 255 });
      DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, colors[COLOR_DIALOG_BACKGROUND]);
      DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, colors[COLOR_DIALOG_BACKGROUND]);
    } break;

    case STYLE_SCROLL_BUTTON: {
      if (modifiers & CIG_STYLE_APPLY_PRESS) {
        DrawRectangle(UNPACK_RECT(rect), colors[COLOR_DIALOG_BACKGROUND]);
        DrawRectangleLinesEx(RAYLIB_RECT(rect), 1, (Color) { 128, 128, 128, 255 });
      } else {
        DrawRectangle(UNPACK_RECT(rect), colors[COLOR_DIALOG_BACKGROUND]);
        DrawLine(rect.x + 1, rect.y + 1, rect.x + rect.w - 2, rect.y + 1, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x + 2, rect.y + 2, rect.x + 2, rect.y + rect.h - 1, (Color){ 255, 255, 255, 255 });
        DrawLine(rect.x, rect.y + rect.h - 1, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h - 1, (Color){ 0, 0, 0, 255 });
        DrawLine(rect.x + 1, rect.y + rect.h - 2, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 130, 130, 130, 255 });
        DrawLine(rect.x + rect.w - 1, rect.y + 1, rect.x + rect.w - 1, rect.y + rect.h - 2, (Color){ 130, 130, 130, 255 });
      }
    } break;
  }
}

void draw_rectangle(
  cig_color_ref fill_color,
  cig_color_ref border_color,
  cig_r rect,
  unsigned int border_width
) {
  if (fill_color) {
    DrawRectangle(UNPACK_RECT(rect), *(Color*)fill_color);
  }
  if (border_color && border_width > 0) {
    DrawRectangleLinesEx(RAYLIB_RECT(rect), border_width, *(Color*)border_color);
  }
}

void draw_line(
  cig_color_ref color,
  cig_v p0,
  cig_v p1,
  float thickness
) {
  if (thickness <= 1.f) {
    DrawLineV(RAYLIB_VEC2(p0), RAYLIB_VEC2(p1), *(Color*)color);
  } else {
    DrawLineEx(RAYLIB_VEC2(p0), RAYLIB_VEC2(p1), thickness, *(Color*)color);
  }
}

void
draw_polygon(cig_v origin, cig_v *points, size_t n, cig_color_ref color)
{
  int i;

  Color *fc = (Color*)color;

  rlBegin(RL_TRIANGLES);
  rlColor4ub(fc->r, fc->g, fc->b, fc->a);
  
  for (i = 0; i < n - 1; ++i) {
    rlVertex2f(origin.x, origin.y);
    rlVertex2f(origin.x + points[i].x, origin.y + points[i].y);
    rlVertex2f(origin.x + points[i+1].x, origin.y + points[i+1].y);
  }

  rlEnd();
}

M_INLINED cig_v measure_image(cig_image_ref image) {
  Texture2D *tex = (Texture2D *)image;
  return cig_v_make(tex->width, tex->height);
}

M_INLINED void draw_image(
  cig_buffer_ref buffer,
  cig_r container,
  cig_r rect,
  cig_image_ref image,
  cig_image_mode mode
) {
  Texture2D *tex = (Texture2D *)image;

  if (dithering_shader_enabled) {
    BeginShaderMode(blue_dither_shader);
  }

  DrawTexturePro(
    *tex,
    (Rectangle) { 0, 0, tex->width, tex->height },
    RAYLIB_RECT(rect),
    (Vector2) { 0, 0 },
    0,
    WHITE
  );

  if (dithering_shader_enabled) {
    EndShaderMode();
  }
}

#ifdef DEBUG

static void layout_breakpoint(cig_r container, cig_r rect) {
  /* Ends current render texture so we could draw things as they currently stand */
  EndTextureMode();

  // TODO: Active clip rect needs to be sent here as well, and maybe visualised somehow
  EndScissorMode();

  BeginDrawing();
  BeginTextureMode(debug_texture);
  ClearBackground((Color){ 0, 0, 0, 255 });
  DrawTexturePro(
    render_texture.texture,
    (Rectangle) { 0, 0, render_texture.texture.width, -render_texture.texture.height },
    (Rectangle) { 0, 0, render_texture.texture.width, render_texture.texture.height },
    (Vector2) { 0, 0 },
    0,
    WHITE
  );
  if (container.w > 0 && container.h > 0) {
    DrawRectangleLinesEx(RAYLIB_RECT(container), 1, (Color) { 128, 0, 0, 255 });
  }
  if (rect.w > 0 && rect.h > 0) {
    DrawRectangleLinesEx(RAYLIB_RECT(rect), 1, (Color) { 255, 0, 0, 255 });
  }
  EndTextureMode();
  DrawTexturePro(
    debug_texture.texture,
    (Rectangle) { 0, 0, debug_texture.texture.width, -debug_texture.texture.height },
    (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() },
    (Vector2) { 0, 0 },
    0,
    WHITE
  );
  EndDrawing();

  /* Go back to previous render texture to continue with our application */
  BeginTextureMode(render_texture);

  /* You can continue automatically */
  // WaitTime(0.5);

  /* Or manually */
  while (1) {
    PollInputEvents();
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressedRepeat(KEY_SPACE)) {
      break;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
      cig_disable_debug_stepper();
      break;
    } else {
      WaitTime(1.0/60);
    }
  }
}

#endif
