#include "clock.h"
#include "cigext.h"
#include "system/resources.h"
#include "system/backend.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#define PI_F 3.14159265358979323846f
#define DEG_RAD (PI_F/180.0f)

struct clock_state {
  cig_buffer_ref buffer;
  float time;
};

M_INLINED
cig_v point_rotate(cig_v p, float a)
{
  const double cos_theta = cos(a);
  const double sin_theta = sin(a);
  
  return cig_v_make(
    p.x * cos_theta - p.y * sin_theta,
    p.x * sin_theta + p.y * cos_theta
  );
}

static void
draw_filled_poly_rotated(cig_v origin, cig_v *points, size_t n, cig_color_ref fill, float angle)
{
  int i;
  float rads = angle*DEG_RAD;

  for (i = 0; i < n; ++i) {
    points[i] = point_rotate(points[i], rads);
  }

  cig_draw_polygon(origin, points, n, fill);
}

static void
draw_segmented_line_rotated(cig_v origin, cig_v *points, size_t n, cig_color_ref color, float angle, float thickness)
{
  int i;
  float rads = angle*DEG_RAD;

  for (i = 0; i < n - 1; ++i) {
    cig_v p0 = point_rotate(points[i], rads);
    cig_v p1 = point_rotate(points[i+1], rads);

    cig_draw_line(
      cig_v_make(p0.x + origin.x, p0.y + origin.y),
      cig_v_make(p1.x + origin.x, p1.y + origin.y),
      color,
      thickness
    );
  }
}

static void
draw_hand(cig_v center, float width, float length, float tail_length, float angle)
{
  const bool shadow_direction_flip = angle > 180;
  const cig_v shadow_offset = cig_v_make(0, 2);

  draw_filled_poly_rotated(
    cig_v_add(center, shadow_offset),
    M_ARRAYC(
      cig_v,
      cig_v_make(-width, 0),
      cig_v_make(0, tail_length),
      cig_v_make(width, 0),
      cig_v_make(width, 0),
      cig_v_make(0, -length),
      cig_v_make(-width, 0)
    ),
    get_color(COLOR_CLOCK_HAND_SHADOW),
    angle + (shadow_direction_flip ? -0.5 : 0.5)
  );

  draw_filled_poly_rotated(
    center,
    M_ARRAYC(
      cig_v,
      cig_v_make(-width, 0),
      cig_v_make(0, tail_length),
      cig_v_make(width, 0),
      cig_v_make(width, 0),
      cig_v_make(0, -length),
      cig_v_make(-width, 0)
    ),
    get_color(COLOR_CLOCK_HAND),
    angle
  );

  draw_segmented_line_rotated(
      center,
      M_ARRAYC(
        cig_v,
        cig_v_make(0, -length),
        cig_v_make(shadow_direction_flip ? width : -width, 0),
        cig_v_make(0, tail_length)
      ),
      get_color(COLOR_WHITE),
      angle,
      1
  );
}

static void
draw_clock()
{
  renderer_clear();

  const cig_r rect = cig_rect();
  const cig_v center = cig_v_make(rect.w / 2, rect.h / 2);
  const float hand_radius = M_MIN(rect.w, rect.h) / 2;
  const float hour_dot_size = M_MAX(3, M_MIN(11, hand_radius * 0.05));
  const float dot_dist_from_center = hand_radius - (hour_dot_size / 2);
  const float angle_per_dot = (360 / (12 * 5));

  time_t t = time(NULL);
  struct tm *ct = localtime(&t);

  int i;
  for (i = 0; i < 12 * 5; ++i) {
    cig_v dot_pos = cig_v_make(
      roundf(center.x + cos(i * angle_per_dot * DEG_RAD) * dot_dist_from_center),
      roundf(center.y + sin(i * angle_per_dot * DEG_RAD) * dot_dist_from_center)
    );

    if (i % 5 == 0) {
      cig_v v0 = cig_v_make(
        dot_pos.x - (int)(hour_dot_size / 2),
        dot_pos.y - (int)(hour_dot_size / 2)
      );
      cig_v v1 = cig_v_add(v0, cig_v_make(hour_dot_size, 0));
      cig_v v2 = cig_v_add(v1, cig_v_make(0, hour_dot_size));
      cig_v v3 = cig_v_add(v0, cig_v_make(0, hour_dot_size));

      cig_draw_rect(cig_r_make(v0.x, v0.y, hour_dot_size, hour_dot_size), get_color(COLOR_CLOCK_HAND), NULL, 0);
      cig_draw_line(v0, cig_v_sub(v1, cig_v_make(1, 0)), get_color(COLOR_LIGHT_CYAN), 1);
      cig_draw_line(v0, v3, get_color(COLOR_LIGHT_CYAN), 1);
      cig_draw_line(cig_v_add(v3, cig_v_make(0, -1)), cig_v_sub(v2, cig_v_make(0, 1)), get_color(COLOR_BLACK), 1);
      cig_draw_line(v1, v2, get_color(COLOR_BLACK), 1);
    } else {
      cig_draw_rect(cig_r_make(dot_pos.x - 1, dot_pos.y - 1, 2, 2), get_color(COLOR_DARK_GRAY), NULL, 0);
      cig_draw_rect(cig_r_make(dot_pos.x, dot_pos.y, 2, 2), get_color(COLOR_WHITE), NULL, 0);
      cig_draw_rect(cig_r_make(dot_pos.x, dot_pos.y, 1, 1), get_color(COLOR_LIGHT_GRAY), NULL, 0);
    }

    /* Hour hand */

    float hour_hand_base_size = hand_radius * 0.6f;
    float hour_hand_width = hour_hand_base_size * 0.125f;
    float hour_hand_height = hour_hand_base_size;
    float hour_hand_height_tail = hour_hand_base_size * 0.2f;
    float hour_angle_deg = 360 * ((float)((ct->tm_hour % 12) * 60 + ct->tm_min) / 720);

    draw_hand(center, hour_hand_width, hour_hand_height, hour_hand_height_tail, hour_angle_deg);

    /* Minute hand */

    float minute_hand_base_size = hand_radius * 0.8f;
    float minute_hand_width = minute_hand_base_size * 0.07f;
    float minute_hand_height = minute_hand_base_size;
    float minute_hand_height_tail = minute_hand_base_size * 0.22f;
    float minute_angle_deg = 360 * (float)ct->tm_min / 60;

    draw_hand(center, minute_hand_width, minute_hand_height, minute_hand_height_tail, minute_angle_deg);

    /* Second hand */

    float second_hand_length = hand_radius * 0.82f;
    float second_angle_deg = 360 * (float)ct->tm_sec / 60;

    {
      cig_v end_point = point_rotate(cig_v_make(0, -second_hand_length), second_angle_deg*DEG_RAD);
      cig_draw_line(center, cig_v_make(center.x + end_point.x, center.y + end_point.y), get_color(COLOR_CLOCK_SECONDS_HAND), 1);
    }

    // {
    //   Vector2 end_point = Vector2Rotate((Vector2) { 0, -second_hand_length * 0.1f }, second_angle_deg*DEG_RAD);
    //   cig_draw_line(RAYLIB_VEC2(center), (Vector2) { center.x + rotatedP.x, center.y + rotatedP.y }, (Color) { 255, 127, 127, 255 });
    // }
  }
}

/**
 * Main window procedure. Pushes a custom buffer into which
 * the clockface is drawn. Drawing is performed once a second
 * or when the window resizes.
 */
static void
clock_window_proc(window_t *this)
{
  struct clock_state *state = (struct clock_state *)this->owner->data;

  state->time += cig_delta_time();

  CIG(_, cig_i_uniform(4)) {
    CIG(_) {
      renderer_push_buffer(state->buffer);

      /**
       * While not strictly necessary here, this pushes a new display buffer
       * for CIG, so that all subsequent draw calls would be performed in that.
       * 
       * We don't use any CIG/Win95 components here, but the custom clockface
       * draw function still picks it up and could be considered a component.
       */
      cig_push_buffer(state->buffer);

      if (state->time >= 1.f) {
        draw_clock();
        state->time = 0.f;
      } else if (this->updates & WINDOW_DID_RESIZE) {
        draw_clock();
      } else if (this->updates & WINDOW_DID_MAXIMIZE || this->updates & WINDOW_DID_RESTORE) {
        /**
         * HACK: This forces a redraw on the next iteration
         * 
         * Raylib render textures aren't very nice to work with when you're
         * in the middle of a texture draw (main screen buffer) and try to
         * resize another buffer. It causes flickering.
         * 
         * When we call `resize_buffer_if_needed` above, it actually just schedules
         * a resize at the end of the main loop, so it's resized by the next iteration.
         */
        state->time = 1.f;
        renderer_clear();
      }

      cig_pop_buffer();

      /* Pops and flushes the display buffer in our demo stack */
      renderer_pop_buffer(cig_absolute_rect());

      /**
       * Buffer is maintained with some additional margin so it wouldn't have
       * to be reallocated on every little size change. If it *did* resize
       * we schedule a redraw at the next iteration.
       */
      if (framebuffer_resize_if_needed(state->buffer, cig_rect().w, cig_rect().h)) {
        state->time = 1.f;
      }
    }
  }
}

static void
clock_on_kill(application_t *this)
{
  // Cleanup
  struct clock_state *state = (struct clock_state *)this->data;
  framebuffer_free(state->buffer);
}


/* --- PUBLIC */

application_t
clock_app()
{
  struct clock_state *state = malloc(sizeof(struct clock_state));
  memset(state, 0, sizeof(*state));

  /**
   * Allocating a framebuffer in the middle of the render pass causes Raylib
   * to flicker at this point, but it is what it is.
   */
  state->buffer = framebuffer_create(250, 250);
  state->time = 1.f;

  return (application_t) {
    .id = "clock",
    .windows = {
      (window_t) {
        .id = cig_hash("clock"),
        .proc = clock_window_proc,
        .data = NULL,
        .rect = CENTER_APP_WINDOW(220, 220),
        .min_size = cig_v_make(160, 160),
        .title = "Clock",
        .icon = IMAGE_CLOCK_16,
        .flags = IS_PRIMARY_WINDOW | IS_RESIZABLE
      }
    },
    .data = state,
    .on_kill = &clock_on_kill,
    .flags = KILL_WHEN_PRIMARY_WINDOW_CLOSED
  };
}
