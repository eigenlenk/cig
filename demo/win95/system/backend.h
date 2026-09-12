#ifndef CIG_WIN95_DEMO_BACKEND_API_INCLUDED
#define CIG_WIN95_DEMO_BACKEND_API_INCLUDED

#include <stdbool.h>

/* --- FRAMEBUFFER */

cig_buffer_ref framebuffer_create(int w, int h);

void framebuffer_free(cig_buffer_ref);

bool framebuffer_resize_if_needed(cig_buffer_ref, int w, int h);


/* --- RENDERER */

/* Applies a dark blue checkerboard dither to following image draw calls */
void renderer_enable_blue_selection_dithering(bool);

void renderer_push_buffer(cig_buffer_ref);

void renderer_pop_buffer(cig_r);

/* Clear currently active framebuffer */
void renderer_clear(void);

#endif
