#include "cigcore.h"
#include "cigcorem.h"
#include <string.h>
#include <assert.h>

cig__macro_ctx_st cig__macro_ctx = { 0 };

static cig_context *current = NULL;
static cig_set_clip_callback set_clip = NULL;

#ifdef DEBUG
static cig_layout_breakpoint_callback_t layout_breakpoint_callback = NULL;
static bool requested_layout_step_mode = false;
#endif

/*  Forward delcarations */
static M_OPTIONAL(cig_state*) find_state(cig_id);
static M_OPTIONAL(cig_scroll_state_t*) find_scroll_state(cig_id);
static M_OPTIONAL(cig_focus*) find_focus_state(cig_id);
static M_OPTIONAL(cig_state *) enable_state();
static void handle_frame_hover(cig_frame*);
static void push_clip(cig_frame*);
static void pop_clip();
static cig_r calculate_rect_in_parent(cig_r, const cig_frame*);
static cig_r align_rect_in_parent(cig_r, cig_r, const cig_params*);
static bool next_layout_rect(cig_r, cig_frame*, cig_r*);
static cig_frame* push_frame(cig_r, cig_i, cig_params, bool (*)(cig_r, cig_r, cig_params*, cig_r*));
static void move_to_next_row(cig_params*);
static void move_to_next_column(cig_params*);
static double get_attribute_value_of_relative_to(cig_pin_attribute, cig_pin_attribute, double, cig_frame*, cig_frame*);

M_INLINED bool cig_v_valid(cig_v v) {
  return !(v.x == INT_MIN || v.y == INT_MIN); 
}

M_INLINED int limit(int v, const int minv_or_zero, const int maxv_or_zero) {
  if (maxv_or_zero > 0) { v = M_MIN(maxv_or_zero, v); }
  if (minv_or_zero > 0) { v = M_MAX(minv_or_zero, v); }
  return v;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ALIGN_OF(T) _Alignof(T)
#else
    #define ALIGN_OF(T) sizeof(void*) /* fallback */
#endif

/*  ┌─────────────┐
    │ CORE LAYOUT │
    └─────────────┘ */

void cig_init_context(cig_context *context) {
  register int i;

  context->frame_stack = INIT_STACK(cig_frame_ref);
  context->buffers = INIT_STACK(cig_buffer_element_t);
  context->input = (cig_input_state_t) {
    { 0 },
    .key_repeat_rate = CIG_DEFAULT_KEY_REPEAT_RATE
  };
  context->next_id = 0;
  context->tick = 1;
  context->delta_time = 0.f;
  context->elapsed_time = 0.f;
  context->frames.high = 0;
  context->top_focus = NULL;

  for (i = 0; i < CIG_STATES_MAX; ++i) {
    context->state_list[i].id = 0;
    context->state_list[i].last_tick = context->tick;
    context->state_list[i].value.memory.bytes = NULL;
  }
  
  for (i = 0; i < CIG_SCROLLABLE_ELEMENTS_MAX; ++i) {
    context->scroll_elements[i].id = 0;
    context->scroll_elements[i].last_tick = context->tick;
  }

  for (i = 0; i < CIG_FOCUSABLE_ELEMENTS_MAX; ++i) {
    context->focus_elements[i].id = 0;
    context->focus_elements[i].last_tick = context->tick;
  }
}

void cig_begin_layout(
  cig_context *context,
  const cig_buffer_ref buffer,
  const cig_r rect,
  const float delta_time
) {
  int i;

  current = context;

  current->frame_stack.clear(&current->frame_stack);
  current->buffers.clear(&current->buffers);
  current->delta_time = delta_time;
  current->elapsed_time += delta_time;
  current->default_insets = cig_i_zero();

#ifdef DEBUG
  if (requested_layout_step_mode && current->step_mode == false) {
    requested_layout_step_mode = false;
    current->step_mode = true;
  } else if (current->step_mode) {
    current->step_mode = false;
  }
#endif

  const cig_id root_id = current->next_id ? current->next_id : cig_hash("root");

  current->frames.elements[0] = (cig_frame) {
    .id = root_id,
    .rect = cig_r_make(0, 0, rect.w, rect.h),
    .clipped_rect = rect,
    .absolute_rect = rect,
    .absolute_clipped_rect = rect,
    .insets = cig_i_zero(),
    ._layout_function = NULL,
    ._parent = NULL,
    ._layout_params = (cig_params) { 0 },
    ._last_tick = current->tick,
    ._flags = OPEN
  };
  current->frame_stack.push(&current->frame_stack, &current->frames.elements[0]);
  current->frames.high = M_MAX(current->frames.high, 1);

  cig_push_buffer(buffer);
  current->next_id = 0;

  for (i = 0; i < CIG__KEY_COUNT; ++i) {
    current->input.key.code[i].listener_prev_tick = current->input.key.code[i].listener_this_tick;
  }

#ifdef DEBUG
  cig_trigger_layout_breakpoint(cig_r_zero(), cig_r_make(0, 0, rect.w, rect.h));
#endif
}

static cig_focus*
find_focus_for_id(cig_id id)
{
  int i;

  for (i = 0; i < CIG_FOCUSABLE_ELEMENTS_MAX; ++i) {
    if (current->focus_elements[i].id == id) {
      return &current->focus_elements[i].value;
    }
  }

  return NULL;
}

static void
update_focus_chain(cig_focus *start, bool focused)
{
  if (!start) { return; }
  cig_focus *f = start;
  while (f) {
    f->active = focused;
    f->last_change_tick = current->tick + 1;
    f = f->parent;
  }
}

void
cig_end_layout()
{
  register unsigned int i, j;

  for (i = 0; i < CIG_STATES_MAX; ++i) {
    if (current->state_list[i].last_tick != current->tick) {
      current->state_list[i].value.active = false;
      if (current->state_list[i].value.memory.bytes) {
        current->allocator.tracked_bytes -= current->state_list[i].value.memory.size;

        if (current->allocator.free) {
          current->allocator.free(current->allocator.ud, current->state_list[i].value.memory.bytes);
        }

        current->state_list[i].value.memory.bytes = NULL;
        current->state_list[i].value.memory.size = 0;
      }
    }
  }

  for (i = 1, j = 1; i < current->frames.high; ++i) {
    if (current->frames.elements[i]._last_tick == current->tick) {
      current->frames.elements[j++] = current->frames.elements[i];
    }
  }

  /* Update hover target based on last iteration */
  if (current->input.pointer.locked == false) {
    if (current->input.pointer._hover_prev_tick != current->input.pointer._hover_this_tick) {
      current->input.pointer._click_count = 0;
    }
    current->input.pointer._hover_prev_tick = current->input.pointer._hover_this_tick;
    current->input.pointer._hover_this_tick = 0;
  }

  /* Update focus target based on last iteration */
  if (current->input._focus_target_this > 0) {
    current->input._focus_target = current->input._focus_target_this;
    current->input._focus_target_this = 0;

    cig_focus *new_focus = find_focus_for_id(current->input._focus_target);

    if (new_focus && new_focus != current->top_focus) {
      /* Unfocus previous and focus new */
      update_focus_chain(current->top_focus, false);
      update_focus_chain(new_focus, true);

      current->top_focus = new_focus;
    }
  }

  for (i = 0; i < CIG__KEY_COUNT; ++i) {
    switch (current->input.key.code[i].state) {
    case CIG_KEY_PRESSED | CIG_KEY_CLICKED:
      {
        current->input.key.code[i].state &= ~(CIG_KEY_CLICKED);
        current->input.key.code[i].repeat_timer = 0.f;
        current->input.key.code[i].last_update_at = current->tick + 1;
        /* Fallthrough */
      }

    case CIG_KEY_PRESSED:
    case CIG_KEY_PRESSED | CIG_KEY_REPEATED:
      current->input.key.code[i].repeat_timer += current->delta_time;

      if (current->input.key.code[i].repeat_timer >= current->input.key_repeat_rate - 0.001f) {
        current->input.key.code[i].state |= CIG_KEY_REPEATED;
        current->input.key.code[i].repeat_timer = 0.f;
      } else {
        current->input.key.code[i].state &= ~(CIG_KEY_REPEATED);
      }
      
      break;

    case CIG_KEY_RELEASED:
      {
        current->input.key.code[i].state = CIG_KEY_IDLE;
        current->input.key.code[i].owned_by = 0;
        break;
      }

    default:
      break;
    }
  }

  current->frames.high = j;

  ++current->tick;
}

cig_r cig_layout_rect() {
  return current->frames.elements[0].absolute_rect;
}

cig_buffer_ref cig_buffer() {
  return current->buffers.peek_ref(&current->buffers, 0)->buffer;
}

cig_frame* cig_push_frame_args(cig_args args) {
  return push_frame(args.rect, args.insets, args.params, args.builder);
}

cig_frame* cig_push_frame(const cig_r rect) {
  return push_frame(rect, current->default_insets, (cig_params){ 0 }, NULL);
}

cig_frame* cig_push_frame_insets(const cig_r rect, const cig_i insets) {
  return push_frame(rect, insets, (cig_params){ 0 }, NULL);
}

cig_frame* cig_push_frame_insets_params(const cig_r rect, const cig_i insets, const cig_params params) {
  return push_frame(rect, insets, params, NULL);
}

cig_frame* cig_push_layout_function(
  bool (*layout_function)(const cig_r, const cig_r, cig_params *, cig_r *),
  const cig_r rect,
  const cig_i insets,
  cig_params params
) {
  return push_frame(rect, insets, params, layout_function);
}

cig_frame* cig_pop_frame() {
  cig_frame *popped_frame = stack_cig_frame_ref_pop(cig_frame_stack());
  popped_frame->_flags &= ~OPEN;
  if (popped_frame->_flags & CLIPPED) {
    pop_clip();
  }
  cig__macro_ctx.last_closed = popped_frame;
  return popped_frame;
}

void cig_set_default_insets(cig_i insets) {
  current->default_insets = insets;
}

cig_frame* cig_current() {
  return stack_cig_frame_ref_peek(cig_frame_stack(), 0);
}

cig_r cig_convert_relative_rect(const cig_r rect) {
  const cig_buffer_element_t *buffer_element = current->buffers.peek_ref(&current->buffers, 0);
  const cig_frame *frame = cig_current();
  
  return cig_r_offset(
    rect,
    frame->absolute_rect.x + frame->insets.left - buffer_element->origin.x,
    frame->absolute_rect.y + frame->insets.top - buffer_element->origin.y
  );
}

cig_frame_ref_stack_t* cig_frame_stack() {
  return &current->frame_stack;
}

/*  ┌───────┐
    │ STATE │
    └───────┘ */

M_OPTIONAL(void*)
cig_memory_allocation(size_t *size)
{
  cig_state *state = enable_state();
  
  if (!state) {
    return NULL;
  }

  if (size) {
    *size = state->memory.size;
  }

  return state->memory.bytes;
}

M_OPTIONAL(void*)
cig_memory_allocate(size_t bytes)
{
  cig_state *state = enable_state();
  
  if (!state) {
    return NULL;
  }

  if (state->memory.bytes) {
    /* Resize memory */
    if (state->memory.size != bytes && current->allocator.realloc) {
      current->allocator.tracked_bytes -= state->memory.size;
      current->allocator.tracked_bytes += bytes;
      state->memory.bytes = current->allocator.realloc(current->allocator.ud, state->memory.bytes, state->memory.size, bytes);
      state->memory.size = bytes;
    }
    return state->memory.bytes;
  }

  current->allocator.tracked_bytes += bytes;
  state->memory.bytes = current->allocator.alloc(current->allocator.ud, bytes, ALIGN_OF(max_align_t));
  state->memory.size = bytes;

  return state->memory.bytes;
}

void
cig_memory_free()
{
  cig_state *state = cig_current()->_state;

  if (state && state->memory.bytes) {
    current->allocator.tracked_bytes -= state->memory.size;

    if (current->allocator.free) {
      current->allocator.free(current->allocator.ud, state->memory.bytes); /* Free */
    }

    state->memory.bytes = NULL;
    state->memory.size = 0;
  }
}

size_t
cig_tracked_bytes(void)
{
  return current->allocator.tracked_bytes;
}


/*  ┌──────────────────────────────┐
    │ TEMPORARY BUFFERS (ADVANCED) │
    └──────────────────────────────┘ */

void cig_push_buffer(const cig_buffer_ref buffer) {
  current->buffers.push(&current->buffers, (cig_buffer_element_t) {
    .buffer = buffer,
    .absolute_rect = cig_current()->absolute_rect,
    .origin = current->buffers.size == 0
      ? cig_v_zero()
      : cig_v_make(cig_current()->absolute_rect.x, cig_current()->absolute_rect.y),
     .clip_rects = INIT_STACK(cig_clip_rect_t)
  });
}

void cig_pop_buffer() {
  M_UNUSED(current->buffers.pop_ref(&current->buffers));
}

/*  ┌────────────────────────────┐
    │ INPUT, INTERACTION & FOCUS │
    └────────────────────────────┘ */

void
cig_set_pointer_position(cig_v position)
{
  current->input.pointer.position = position;

  /* Root frame has not had a hit check performed yet */
  handle_frame_hover(cig_current());
}

void
cig_set_pointer_state(cig_input_action_type action_mask)
{
  /**
   * Record action mask from previous interation and update current.
   * We'll use it to compare and detect changes and generate events.
   */
  const cig_input_action_type prev_action_mask = current->input.pointer.action_mask;

  current->input.pointer.action_mask = action_mask;

  /**
   * Click state is tracked regardless of action type and works by simply comparing
   * the mask to previous value and detecting changes. Actual action type check
   * is performed in `cig_pressed` and `cig_clicked`.
   */
  const bool is_primary_action_started = action_mask & CIG_INPUT_PRIMARY_ACTION && !(prev_action_mask & CIG_INPUT_PRIMARY_ACTION);
  const bool is_secondary_action_started = action_mask & CIG_INPUT_SECONDARY_ACTION && !(prev_action_mask & CIG_INPUT_SECONDARY_ACTION);
  const bool is_primary_action_ended = prev_action_mask & CIG_INPUT_PRIMARY_ACTION && !(action_mask & CIG_INPUT_PRIMARY_ACTION);
  const bool is_secondary_action_ended = prev_action_mask & CIG_INPUT_SECONDARY_ACTION && !(action_mask & CIG_INPUT_SECONDARY_ACTION);

  current->input.pointer.click_state = (!action_mask && prev_action_mask)
    ? (current->elapsed_time - current->input.pointer._press_start_time) <= CIG_CLICK_EXPIRE_IN_SECONDS
      ? ENDED
      : EXPIRED
    : (is_primary_action_started || is_secondary_action_started)
      ? current->input.pointer.click_state == BEGAN
        ? NEITHER
        : BEGAN
      : NEITHER;

  /* Keep track which action began last */
  if (is_primary_action_started) {
    current->input.pointer.last_action_began = CIG_INPUT_PRIMARY_ACTION;
  } else if (is_secondary_action_started) {
    current->input.pointer.last_action_began = CIG_INPUT_SECONDARY_ACTION;
  } else {
    current->input.pointer.last_action_began = 0;
  }

  /* Keep track which action ended last */
  if (is_primary_action_ended) {
    current->input.pointer.last_action_ended = CIG_INPUT_PRIMARY_ACTION;
  } else if (is_secondary_action_ended) {
    current->input.pointer.last_action_ended = CIG_INPUT_SECONDARY_ACTION;
  } else {
    current->input.pointer.last_action_ended = 0;
  }

  switch (current->input.pointer.click_state) {
  case NEITHER:
    {
      if (current->input.pointer._click_count && current->elapsed_time - current->input.pointer._click_end_time > CIG_CLICK_EXPIRE_IN_SECONDS) {
        current->input.pointer._click_count = 0;
      }
    } break;

  case BEGAN:
    {
      current->input.pointer._press_start_time = current->elapsed_time;
      current->input.pointer._press_target_id = current->input.pointer._hover_prev_tick;
    } break;

  case ENDED:
    {
      current->input.pointer._click_count ++;
      current->input.pointer._click_end_time = current->elapsed_time;
    } break;

  case EXPIRED:
    {
      current->input.pointer._click_count = 0;
    } break;
  }

  /* Root frame has not had a hit check performed yet */
  handle_frame_hover(cig_current());
}

void
cig_set_key_state(cig_key_code key, bool pressed)
{
  assert(key >= 0 && key < CIG__KEY_COUNT);

  if (pressed) {
    if (!(current->input.key.code[key].state & CIG_KEY_PRESSED)) {
      current->input.key.code[key].state = CIG_KEY_PRESSED | CIG_KEY_CLICKED;
      current->input.key.code[key].last_update_at = current->tick;
    }
  } else {
    if (current->input.key.code[key].state & CIG_KEY_PRESSED) {
      current->input.key.code[key].state = CIG_KEY_RELEASED;
      current->input.key.code[key].last_update_at = current->tick;
    }
  }
}

float
cig_set_key_repeat_rate(float rate)
{
  float current_rate = current->input.key_repeat_rate;
  current->input.key_repeat_rate = rate;
  return current_rate;
}

cig_input_state_t *cig_input_state() {
  return &current->input;
}

void cig_enable_interaction() {
  cig_frame *frame = cig_current();
  if (frame->_flags & INTERACTIBLE) {
    return;
  }
  frame->_flags |= INTERACTIBLE;
  if (frame->_flags & SUBTREE_INCLUSIVE_HOVER) {
    current->input.pointer._hover_this_tick = frame->id;
  }
}

bool cig_hovered() {
  /*  See `handle_frame_hover` */
  return current->input.pointer.locked == false && (cig_current()->_flags & INTERACTIBLE) && cig_current()->id == current->input.pointer._hover_prev_tick;
}

cig_input_action_type cig_pressed(
  const cig_input_action_type actions,
  const cig_press_flags options
) {
  if (!cig_hovered()) {
    return 0;
  }

  const cig_input_action_type action_mask = actions & current->input.pointer.action_mask;

  if (action_mask && ((options & CIG_PRESS_INSIDE) == false || (options & CIG_PRESS_INSIDE && current->input.pointer._press_target_id == cig_current()->id))) {
    return action_mask;
  } else {
    return 0;
  }
}

cig_input_action_type cig_clicked(
  const cig_input_action_type actions,
  const cig_click_flags options
) {
  if (!cig_hovered()) {
    return 0;
  }

  cig_input_action_type result;

  if (options & CIG_CLICK_ON_PRESS) {
    if (current->input.pointer.click_state == BEGAN && (result = actions & current->input.pointer.action_mask)) {
      return result;
    }
  } else {
    const unsigned int required_clicks = options & CIG_CLICK_DOUBLE ? 2 : 1;
    if (
      (current->input.pointer.click_state == ENDED && current->input.pointer._click_count >= required_clicks) ||
      (current->input.pointer.click_state == EXPIRED && !(options & CIG_CLICK_EXPIRE) && required_clicks == 1)
    ) {
      if (options & CIG_CLICK_STARTS_INSIDE && current->input.pointer._press_target_id != cig_current()->id) {
        return 0;
      }
      if ((result = actions & current->input.pointer.last_action_ended)) {
        current->input.pointer._click_count = 0;
        return result;
      }
    }
  }

  return 0;
}

cig_input_drag_state
cig_dragged(cig_input_action_type actions)
{
  /* Ignore other draggable elements once something has started dragging */
  if (current->input.pointer.drag.state > CIG_DRAG_STATE_INACTIVE && current->input.pointer.drag.id != cig_current()->id) {
    return 0;
  }

  /* *****************
     Update drag state */

  if (actions & current->input.pointer.action_mask) { /* Mouse buttons down: */
    switch (current->input.pointer.drag.state) {
    case CIG_DRAG_STATE_INACTIVE: {
      if (cig_pressed(actions, CIG_PRESS_INSIDE)) {
        current->input.pointer.drag.state = CIG_DRAG_STATE_READY;
        current->input.pointer.drag._start_position_absolute = current->input.pointer.position;
        current->input.pointer.drag.change_total = cig_v_zero();
        current->input.pointer.drag.change_last_frame = cig_v_zero();
        current->input.pointer.drag.id = cig_current()->id;
      }
      break;
    }

    case CIG_DRAG_STATE_READY:
    case CIG_DRAG_STATE_WAITING_MOVE: {
      const cig_v change = cig_v_sub(current->input.pointer.position, current->input.pointer.drag._start_position_absolute);
      if (cig_v_length(change) >= 2) {
        current->input.pointer.drag.state = CIG_DRAG_STATE_BEGAN;
        current->input.pointer.drag.change_total = change;
        current->input.pointer.drag.change_last_frame = change;
      } else {
        current->input.pointer.drag.state = CIG_DRAG_STATE_WAITING_MOVE;
      }
      break;
    }

    case CIG_DRAG_STATE_BEGAN:
    case CIG_DRAG_STATE_MOVED:
    case CIG_DRAG_STATE_IDLE: {
      const cig_v change = cig_v_sub(current->input.pointer.position, current->input.pointer.drag._start_position_absolute);
      if (cig_v_equals(current->input.pointer.drag.change_total, change)) {
        current->input.pointer.drag.state = CIG_DRAG_STATE_IDLE;
        current->input.pointer.drag.change_last_frame = cig_v_zero();
      } else {
        current->input.pointer.drag.state = CIG_DRAG_STATE_MOVED;
        current->input.pointer.drag.change_last_frame = cig_v_sub(change, current->input.pointer.drag.change_total);
        current->input.pointer.drag.change_total = change;
      }
      break;
    }

    default: break;
    }
  } else { /* Mouse buttons up: */
    if (current->input.pointer.drag.state > CIG_DRAG_STATE_READY && current->input.pointer.drag.state < CIG_DRAG_STATE_ENDED) {
      /* When button is released, the mouse may still have moved compared to last frame.
         In that case a final 'MOVED' state is emitted, followed by 'ENDED' on the next iteration */
      const cig_v change = cig_v_sub(current->input.pointer.position, current->input.pointer.drag._start_position_absolute);
      if (!cig_v_valid(current->input.pointer.drag._start_position_absolute) || cig_v_equals(current->input.pointer.drag.change_total, change)) {
        current->input.pointer.drag.state = CIG_DRAG_STATE_ENDED;
        current->input.pointer.drag.change_last_frame = cig_v_zero();
      } else {
        current->input.pointer.drag.state = CIG_DRAG_STATE_MOVED;
        current->input.pointer.drag.change_last_frame = cig_v_sub(change, current->input.pointer.drag.change_total);
        current->input.pointer.drag.change_total = change;
        current->input.pointer.drag._start_position_absolute = (cig_v) { INT_MIN, INT_MIN };
      }
    } else {
      current->input.pointer.drag.state = CIG_DRAG_STATE_INACTIVE;
      current->input.pointer.drag.id = 0;
      current->input.pointer.locked = false;
    }
  }

  return current->input.pointer.drag.state;
}

M_DISCARDABLE(cig_input_key_state)
cig_key(cig_key_code key)
{
  if (!key) {
    return CIG_KEY_IDLE;
  }

  current->input.key.code[key].listener_this_tick = cig_current()->id;

  if (!current->input.key.code[key].owned_by && current->input.key.code[key].state != 0 && current->input.key.code[key].listener_prev_tick == cig_current()->id) {
    current->input.key.code[key].owned_by = cig_current()->id;
  }

  if (current->input.key.code[key].owned_by == cig_current()->id) {
    return current->input.key.code[key].state;
  } else {
    return CIG_KEY_IDLE;
  }
}

bool
cig_key_poll(cig_key_code* key, cig_input_key_state* state)
{
  static int i = 0;
  static cig_id current_id = 0;

  const cig_id this_id = cig_current()->id;

  if (this_id != current_id) {
    current_id = this_id;
    i = 0;
  }

  for (; i < CIG__KEY_COUNT; ++i) {
    current->input.key.code[i].listener_this_tick = this_id;

    if (!current->input.key.code[i].owned_by && current->input.key.code[i].state != 0 && current->input.key.code[i].listener_prev_tick == this_id) {
      current->input.key.code[i].owned_by = this_id;
    }

    if (current->input.key.code[i].owned_by == this_id) {
      if (key) {
        *key = (cig_key_code)i;
      }
      if (state) {
        *state = current->input.key.code[i].state;
      }
      ++i;
      return true;
    }
  }

  current_id = 0;

  return false;
}

bool
cig_key_raw_pressed(cig_key_code key)
{
  return current->input.key.code[key].state & CIG_KEY_PRESSED;
}

bool
cig_key_raw_clicked(cig_key_code key)
{
  return current->input.key.code[key].state & CIG_KEY_CLICKED;
}

bool
cig_key_raw_repeated(cig_key_code key)
{
  return current->input.key.code[key].state & CIG_KEY_REPEATED;
}

bool
cig_key_raw_released(cig_key_code key)
{
  return current->input.key.code[key].state & CIG_KEY_RELEASED;
}

bool
cig_key_raw_poll(cig_key_code* key, cig_input_key_state* state)
{
  static int i = 0;
  static cig_id current_id = 0;

  if (cig_current()->id != current_id) {
    current_id = cig_current()->id;
    i = 0;
  }

  for (; i < CIG__KEY_COUNT; ++i) {
    if (current->input.key.code[i].last_update_at == current->tick) {
      if (key) {
        *key = (cig_key_code)i;
      }
      if (state) {
        *state = current->input.key.code[i].state;
      }
      ++i;
      return true;
    }
  }

  current_id = 0;

  return false;
}


/*  ┌───────┐
    │ FOCUS │
    └───────┘ */

/* Find the first available focus object navigating up the frame hierarchy, NULL if none found. */
static cig_focus*
find_next_focus(const cig_frame *frame)
{
  cig_frame *parent = frame->_parent;

  while (parent) {
    if (parent->_flags & FOCUSABLE) {
      return parent->_focus;
    } else {
      parent = parent->_parent;
    }
  }

  return NULL;
}

bool
cig_enable_focus(bool* state)
{
  cig_frame *frame = cig_current();
  cig_focus *parent_focus = find_next_focus(frame);

  frame->_flags |= FOCUSABLE;
  frame->_focus = find_focus_state(frame->id);
  frame->_focus->parent = parent_focus;

  if (frame->_flags & HOVER && current->input.pointer.click_state == BEGAN) {
    current->input._focus_target_this = frame->id;
  }

  if (state) {
    if (frame->_focus->last_change_tick == current->tick) {
      *state = frame->_focus->active;
    } else if (parent_focus && !parent_focus->active && frame->_focus->active) {
      frame->_focus->active = false;
      frame->_focus->last_change_tick = current->tick;
      *state = frame->_focus->active;
    } else if (*state && !frame->_focus->active) {
      current->input._focus_target_this = frame->id;
    } else if (!*state && frame->_focus->active) {
      frame->_focus->active = false;
      frame->_focus->last_change_tick = current->tick;
    }
  }

  return cig_focused();
}

bool
cig_focused(void)
{
  /* Get object providing focus info (self or some ancestor) */
  const cig_focus *focus = cig_current()->_focus
    ? cig_current()->_focus
    : find_next_focus(cig_current());

  if (!focus) {
    return false;
  }

  return focus->active && (!focus->parent || focus->parent->active);
}

bool
cig_gained_focus(void)
{
  /* Get object providing focus info (self or some ancestor) */
  const cig_focus *focus = cig_current()->_focus
    ? cig_current()->_focus
    : find_next_focus(cig_current());
  
  if (!focus) {
    return false;
  }

  return focus->active && focus->last_change_tick == current->tick;
}

bool
cig_lost_focus(void)
{
  /* Get object providing focus info (self or some ancestor) */
  const cig_focus *focus = cig_current()->_focus
    ? cig_current()->_focus
    : find_next_focus(cig_current());
  
  if (!focus) {
    return false;
  }

  return !focus->active && focus->last_change_tick == current->tick;
}


/*  ┌───────────┐
    │ SCROLLING │
    └───────────┘ */

bool cig_enable_scroll(cig_scroll_state_t *state) {
  cig_frame *frame = cig_current();
  frame->_scroll_state = state ? state : find_scroll_state(frame->id);
  cig_enable_clipping();
  return frame->_scroll_state != NULL;
}

cig_scroll_state_t* cig_scroll_state() {
  return cig_current()->_scroll_state;
}

void cig_set_offset(cig_v offset) {
  assert(cig_scroll_state());
  cig_scroll_state()->offset = offset;
}

void cig_change_offset(cig_v delta) {
  assert(cig_scroll_state());
  cig_scroll_state()->offset = cig_v_add(cig_scroll_state()->offset, delta);
}

cig_v cig_offset() {
  assert(cig_scroll_state());
  return cig_scroll_state()->offset;
}

/*  ┌────────────────┐
    │ LAYOUT HELPERS │
    └────────────────┘ */

cig_r cig_build_rect(size_t n, cig_pin refs[]) {
  register size_t i;
  int32_t x0, y0, x1, y1, w, h, cx, cy;
  double a = 1;
  uint32_t attrs = 0;
  cig_pin pin;

  cig_frame *cur = cig_current();

  for (i = 0; i < n; ++i) {
    pin = refs[i];

    cig_pin_attribute attr = (pin.attribute == UNSPECIFIED
      ? pin.relation_attribute
      : pin.attribute) & ~INSET_ATTRIBUTE;

    cig_pin_attribute rel_attr = pin.relation_attribute == UNSPECIFIED
      ? pin.attribute
      : pin.relation_attribute;

    assert(rel_attr);

    if (pin.relation) {
      assert(pin.relation->_flags & OPEN || pin.relation->_flags & RETAINED);
    }

    double v = get_attribute_value_of_relative_to(attr, rel_attr, pin.value, pin.relation, cur);
    attrs |= M_BIT(attr);

    switch (attr) {
    case LEFT: x0 = v; break;
    case RIGHT: x1 = v; break;
    case TOP: y0 = v; break;
    case BOTTOM: y1 = v; break;
    case WIDTH: w = v; break;
    case HEIGHT: h = v; break;
    case CENTER_X: cx = v; break;
    case CENTER_Y: cy = v; break;
    case ASPECT: a = v; break;

    default:
      break;
    }
  }

  /* Calculate missing values based on what we have */

  if (!(attrs & M_BIT(WIDTH)) && (attrs & M_BIT(ASPECT))) {
    if (attrs & M_BIT(HEIGHT)) {
      w = round(h * a);
      attrs |= M_BIT(WIDTH);
    } else if (attrs & M_BIT(TOP) && attrs & M_BIT(BOTTOM)) {
      w = round((y1 - y0) * a);
      attrs |= M_BIT(WIDTH);
    }
  }

  if (!(attrs & M_BIT(HEIGHT)) && (attrs & M_BIT(ASPECT))) {
    if (attrs & M_BIT(WIDTH)) {
      h = round(w / a);
      attrs |= M_BIT(HEIGHT);
    } else if (attrs & M_BIT(LEFT) && attrs & M_BIT(RIGHT)) {
      h = round((x1 - x0) / a);
      attrs |= M_BIT(HEIGHT);
    }
  }

  if (!(attrs & M_BIT(LEFT))) {
    if (attrs & M_BIT(RIGHT) && attrs & M_BIT(WIDTH)) {
      x0 = x1 - w;
    } else if ((attrs & M_BIT(CENTER_X)) && (attrs & M_BIT(WIDTH))) {
      x0 = cx - (w * 0.5);
    } else if ((attrs & M_BIT(CENTER_X)) && (attrs & M_BIT(RIGHT))) {
      x0 = cx - (x1 - cx);
    } else if (attrs & M_BIT(WIDTH)) {
      x0 = 0;
    } else {
      assert(false);
    }
    attrs |= M_BIT(LEFT);
  } 

  if (!(attrs & M_BIT(RIGHT))) {
    if (attrs & M_BIT(LEFT) && attrs & M_BIT(WIDTH)) {
      x1 = x0 + w;
    } else if ((attrs & M_BIT(CENTER_X)) && (attrs & M_BIT(WIDTH))) {
      x1 = cx + (w * 0.5);
    } else if ((attrs & M_BIT(CENTER_X)) && (attrs & M_BIT(LEFT))) {
      x1 = cx + (cx - x0);
    } else {
      assert(false);
    }
    attrs |= M_BIT(RIGHT);
  }

  if (!(attrs & M_BIT(TOP))) {
    if (attrs & M_BIT(BOTTOM) && attrs & M_BIT(HEIGHT)) {
      y0 = y1 - h;
    } else if ((attrs & M_BIT(CENTER_Y)) && (attrs & M_BIT(HEIGHT))) {
      y0 = cy - (h * 0.5);
    } else if ((attrs & M_BIT(CENTER_Y)) && (attrs & M_BIT(BOTTOM))) {
      y0 = cy - (y1 - cy);
    } else if (attrs & M_BIT(HEIGHT)) {
      y0 = 0;
    } else {
      assert(false);
    }
    attrs |= M_BIT(TOP);
  } 

  if (!(attrs & M_BIT(BOTTOM))) {
    if (attrs & M_BIT(TOP) && attrs & M_BIT(HEIGHT)) {
      y1 = y0 + h;
    } else if ((attrs & M_BIT(CENTER_Y)) && (attrs & M_BIT(HEIGHT))) {
      y1 = cy + (h * 0.5);
    } else if ((attrs & M_BIT(CENTER_Y)) && (attrs & M_BIT(TOP))) {
      y1 = cy + (cy - y0);
    } else {
      assert(false);
    }
    attrs |= M_BIT(BOTTOM);
  }

  return cig_r_make(x0, y0, x1 - x0, y1 - y0);
}

void cig_disable_culling() {
  cig_current()->_layout_params.flags |= CIG_LAYOUT_DISABLE_CULLING;
}

void cig_enable_clipping() {
  push_clip(cig_current());
}

void cig_set_next_id(cig_id id) {
  current->next_id = id;
}

unsigned int cig_depth() {
  return cig_frame_stack()->size;
}

cig_id cig_hash(const char *str) {
  /*  http://www.cse.yorku.ca/~oz/hash.html */
  register cig_id hash = 5381;
  register int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* === hash * 33 + c */
  }
  return hash;
}

bool
cig_is_vertical_layout()
{
  const cig_frame *frame = cig_current();

  if (frame->_layout_function) {
    const bool vertical_axis = frame->_layout_params.axis & CIG_LAYOUT_AXIS_VERTICAL;
    const bool horizontal_axis = frame->_layout_params.axis & CIG_LAYOUT_AXIS_HORIZONTAL;

    if (vertical_axis && horizontal_axis) {
      if (frame->_layout_params.direction == CIG_LAYOUT_DIRECTION_VERTICAL) {
        return true;
      } else {
        return false;
      }
    } else {
      return vertical_axis;
    }
  }

  return false;
}

void cig_empty() {
  cig_push_frame(RECT_AUTO);
  cig_pop_frame();
}

void cig_spacer(const int size) {
  cig_push_frame(cig_is_vertical_layout() ? RECT_AUTO_H(size) : RECT_AUTO_W(size));
  cig_pop_frame();
}

bool cig_default_layout_builder(
  const cig_r container, /* Rect into which sub-frames are laid out */
  const cig_r rect,      /* Proposed rect, generally from CIG_FILL */
  cig_params *prm,
  cig_r *result
) {
  const bool h_axis = prm->axis & CIG_LAYOUT_AXIS_HORIZONTAL;
  const bool v_axis = prm->axis & CIG_LAYOUT_AXIS_VERTICAL;
  const bool is_grid = h_axis && v_axis;

  int x = prm->_h_pos,
      y = prm->_v_pos,
      w,
      h;

  if (h_axis) {
    if (CIG_IS_AUTO(rect.w)) {
      if (prm->width > 0) {
        w = CIG_ANY_VALUE(rect.w, prm->width);
      } else if (prm->columns) {
        w = CIG_ANY_VALUE(rect.w, (container.w - ((prm->columns - 1) * prm->spacing.x)) / prm->columns);
      } else if (is_grid && prm->_h_size && prm->direction == CIG_LAYOUT_DIRECTION_VERTICAL) {
        w = CIG_ANY_VALUE(rect.w, prm->_h_size);
      } else {
        w = CIG_ANY_VALUE(rect.w, container.w - prm->_h_pos);
      }
    } else {
      w = CIG_IS_REL(rect.w) ? CIG_REL_VALUE(rect.w, container.w - prm->_h_pos) : rect.w;
    }
  } else {
    w = CIG_ANY_VALUE(rect.w, container.w - prm->_h_pos);

    /*  Reset any remaining horizontal positioning in case we modify axis mid-layout */
    prm->_h_pos = 0;
    prm->_h_size = 0;
  }

  if (v_axis) {
    if (CIG_IS_AUTO(rect.h)) {
      if (prm->height > 0) {
        h = CIG_ANY_VALUE(rect.h, prm->height);
      } else if (prm->rows) {
        h = CIG_ANY_VALUE(rect.h, (container.h - ((prm->rows - 1) * prm->spacing.y)) / prm->rows);
      } else if (is_grid && prm->_v_size && prm->direction == CIG_LAYOUT_DIRECTION_HORIZONTAL) {
        h = CIG_ANY_VALUE(rect.h, prm->_v_size);
      } else {
        h = CIG_ANY_VALUE(rect.h, container.h - prm->_v_pos);
      }
    } else {
      h = CIG_IS_REL(rect.h) ? CIG_REL_VALUE(rect.h, container.h - prm->_v_pos) : rect.h;
    }
  } else {
    h = CIG_ANY_VALUE(rect.h, container.h - prm->_v_pos);

    /*  Reset any remaining vertical positioning in case we modify axis mid-layout */
    prm->_v_pos = 0;
    prm->_v_size = 0;
  }

  w = limit(w, prm->size_min.width, prm->size_max.width);
  h = limit(h, prm->size_min.height, prm->size_max.height);

  if (h_axis && v_axis) {
    const bool minimum_limit = prm->flags & CIG_LAYOUT_MINIMUM_LIMIT;

    /* Can we fit the new frame onto current axis? */
    switch (prm->direction) {
      case CIG_LAYOUT_DIRECTION_HORIZONTAL: {
        if ((prm->limit.horizontal && prm->_count.h_cur == prm->limit.horizontal) || (prm->_h_pos + w > container.w && !minimum_limit)) {
          move_to_next_row(prm);
          x = 0;
          y = prm->_v_pos;
        }
        
        prm->_h_pos += (w + prm->spacing.x);
        prm->_h_size = M_MAX(prm->_h_size, w);
        prm->_v_size = M_MAX(prm->_v_size, h);
        prm->_count.h_cur ++;
        
        if (prm->_h_pos >= container.w && !minimum_limit) {
          move_to_next_row(prm);
        }
      } break;
      case CIG_LAYOUT_DIRECTION_VERTICAL: {
        if ((prm->limit.vertical && prm->_count.v_cur == prm->limit.vertical) || (prm->_v_pos + h > container.h && !minimum_limit)) {
          move_to_next_column(prm);
          y = 0;
          x = prm->_h_pos;
        }

        prm->_v_pos += (h + prm->spacing.y);
        prm->_h_size = M_MAX(prm->_h_size, w);
        prm->_v_size = M_MAX(prm->_v_size, h);
        prm->_count.v_cur ++;

        if (prm->_v_pos >= container.h && !minimum_limit) {
          move_to_next_column(prm);
        }
      } break;
      default: break;
    }
  } else if (h_axis) {
    if (prm->limit.horizontal && prm->_count.h_cur == prm->limit.horizontal) {
      return false;
    }
    prm->_h_pos += (w + prm->spacing.x);
    prm->_h_size = M_MAX(prm->_h_size, w);
    prm->_count.h_cur ++;
  } else if (v_axis) {
    if (prm->limit.vertical && prm->_count.v_cur == prm->limit.vertical) {
      return false;
    }
    prm->_v_pos += (h + prm->spacing.y);
    prm->_v_size = M_MAX(prm->_v_size, h);
    prm->_count.v_cur ++;
  }

  *result = align_rect_in_parent(cig_r_make(x, y, w, h), container, prm);
  
  return true;
}

cig_frame* cig_push_hstack(cig_r rect, cig_i insets, cig_params params) {
  params.axis = CIG_LAYOUT_AXIS_HORIZONTAL;
  return cig_push_layout_function(&cig_default_layout_builder, rect, insets, params);
}

cig_frame* cig_push_vstack(cig_r rect, cig_i insets , cig_params params) {
  params.axis = CIG_LAYOUT_AXIS_VERTICAL;
  return cig_push_layout_function(&cig_default_layout_builder, rect, insets, params);
}

cig_frame* cig_push_grid(cig_r rect, cig_i insets, cig_params params) {
  params.axis = CIG_LAYOUT_AXIS_HORIZONTAL | CIG_LAYOUT_AXIS_VERTICAL;
  if (params.direction == CIG_LAYOUT_DIRECTION_DEFAULT) {
    params.direction = CIG_LAYOUT_DIRECTION_HORIZONTAL;
  }
  return cig_push_layout_function(&cig_default_layout_builder, rect, insets, params);
}

/*  ┌─────────┐
    │ UTILITY │
    └─────────┘ */

float cig_delta_time() { return current->delta_time; }

float cig_elapsed_time() { return current->elapsed_time; }


/*  ┌───────────────────┐
    │ BACKEND CALLBACKS │
    └───────────────────┘ */

void
cig_set_allocator(cig_context* context, cig_allocator allocator)
{
  context->allocator = allocator;
}

void cig_assign_set_clip(cig_set_clip_callback fp) {
  set_clip = fp;
}

/*  ┌────────────────────┐
    │ INTERNAL FUNCTIONS │
    └────────────────────┘ */

M_INLINED cig_r calculate_rect_in_parent(const cig_r rect, const cig_frame *parent) {
  const cig_r content_rect = cig_r_inset(parent->rect, parent->insets);

  return align_rect_in_parent(cig_r_make(
    /*  When X or Y component have REL flag set, they are relative to W & H respectively.
        AUTO is not taken into consideration here */
    CIG_IS_REL(rect.x) ? CIG_REL_VALUE(rect.x, content_rect.w) : rect.x,
    CIG_IS_REL(rect.y) ? CIG_REL_VALUE(rect.y, content_rect.h) : rect.y,
    limit(
      CIG_ANY_VALUE(rect.w, content_rect.w),
      parent->_layout_params.size_min.width,
      parent->_layout_params.size_max.width
    ),
    limit(
      CIG_ANY_VALUE(rect.h, content_rect.h),
      parent->_layout_params.size_min.height,
      parent->_layout_params.size_max.height
    )
  ), content_rect, &parent->_layout_params);
}

M_INLINED cig_r align_rect_in_parent(cig_r rect, cig_r parent_rect, const cig_params *prm) {
  switch (prm->alignment.horizontal) {
  case CIG_LAYOUT_ALIGNS_CENTER: {
    rect.x = (parent_rect.w - rect.w) * 0.5;
  } break;
  case CIG_LAYOUT_ALIGNS_RIGHT: {
    rect.x = (parent_rect.w - (rect.x+rect.w));
  } break;
  default: break;
  }

  switch (prm->alignment.vertical) {
  case CIG_LAYOUT_ALIGNS_CENTER: {
    rect.y = (parent_rect.h - rect.h) * 0.5;
  } break;
  case CIG_LAYOUT_ALIGNS_BOTTOM: {
    rect.y = (parent_rect.h - (rect.y+rect.h));
  } break;
  default: break;
  }

  return rect;
}

M_INLINED bool next_layout_rect(const cig_r proposed, cig_frame *parent, cig_r *result) {
  if (parent->_layout_function) {
    return (*parent->_layout_function)(
      cig_r_inset(parent->rect, parent->insets),
      proposed,
      &parent->_layout_params,
      result
    );
  } else {
    *result = calculate_rect_in_parent(proposed, parent);
    return true;
  }
}

static cig_frame* push_frame(
  const cig_r rect,
  const cig_i insets,
  cig_params params,
  bool (*layout_function)(cig_r, cig_r, cig_params*, cig_r*)
) {
  size_t i;
  cig_frame *f;

  cig_buffer_element_t *current_buffer = current->buffers.peek_ref(&current->buffers, 0);
  cig_frame *top = cig_current();

  if (top->_layout_params.limit.total > 0 && top->_layout_params._count.total == top->_layout_params.limit.total) {
    goto failure;
  }

  cig_r next;
  if (!next_layout_rect(rect, top, &next)) {
    top->_id_counter ++;
    goto failure;
  }

  top->content_rect = cig_r_containing(top->content_rect, next);

  if (top->_scroll_state) {
    next = cig_r_offset(next, -top->_scroll_state->offset.x, -top->_scroll_state->offset.y);

    const int32_t dx = (top->content_rect.x + top->content_rect.w) - top->rect.w;
    const int32_t dy = (top->content_rect.y + top->content_rect.h) - top->rect.h;
    top->_scroll_state->distance = cig_v_make(M_MAX(0, dx), M_MAX(0, dy));
    top->_scroll_state->bounds = cig_v_make(top->content_rect.w, top->content_rect.h);
  }

  if (!(top->_layout_params.flags & CIG_LAYOUT_DISABLE_CULLING)
    && !cig_r_intersects(top->rect, cig_r_offset(next, top->rect.x+top->insets.left, top->rect.y+top->insets.top))) {
    top->_id_counter ++;
    goto failure;
  }

  const cig_id next_id = current->next_id
    ? current->next_id
    : (top->id + CIG_TINYHASH((top->id+top->_id_counter++), cig_depth()));

  cig_frame *new_frame = NULL;
  cig_frame_visibility previous_visibility = 0;

  /* 1. Try find a retained frame */
  for (i = 1; i < current->frames.high; ++i) {
    f = &current->frames.elements[i];

    if (f->_flags & RETAINED && f->id == next_id) {
      new_frame = f;

      if (new_frame->_last_tick != current->tick - 1) {
        previous_visibility = new_frame->visibility = 0;
      } else {
        previous_visibility = new_frame->visibility;
      }

      goto insert_frame;
    }
  }

  /* 2. Try find an available frame */
  for (i = current->frame_stack.size; i < current->frames.high; ++i) {
    f = &current->frames.elements[i];
    
    if (!(f->_flags & RETAINED) && !(f->_flags & OPEN)) {
      new_frame = f;
      goto insert_frame;
    }
  }

  /* 3. */
  if (current->frames.high < CIG_ELEMENTS_MAX) {
    new_frame = &current->frames.elements[current->frames.high++];
    goto insert_frame;
  }

  if (!new_frame) {
    goto failure;
  }

  insert_frame:

  top->_layout_params._count.total ++;

  const cig_r absolute_rect = cig_convert_relative_rect(next);
  const cig_r current_clip_rect = !current_buffer->clip_rects.size
    ? current_buffer->absolute_rect
    : current_buffer->clip_rects.peek(&current_buffer->clip_rects, 0);
  const cig_r clipped_absolute_rect = cig_r_union(absolute_rect, current_clip_rect);

  *new_frame = (cig_frame) {
    .id = next_id,
    .rect = next,
    .clipped_rect = cig_r_offset(clipped_absolute_rect, -absolute_rect.x + next.x, -absolute_rect.y + next.y),
    .absolute_rect = absolute_rect,
    .absolute_clipped_rect = clipped_absolute_rect,
    .insets = insets,
    .visibility = M_MIN(CIG_FRAME_VISIBLE, previous_visibility + 1),
    ._layout_function = layout_function,
    ._layout_params = params,
    ._parent = top,
    ._last_tick = current->tick,
    ._flags = OPEN
  };

  current->frame_stack.push(&current->frame_stack, new_frame);
  current->next_id = 0;

  if (cig__macro_ctx.open) { *cig__macro_ctx.open = new_frame; }
  if (cig__macro_ctx.retain) { M_UNUSED(cig_retain(new_frame)); }
  cig__macro_ctx.open = NULL;
  cig__macro_ctx.retain = 0;
  cig__macro_ctx.last_closed = NULL;

  handle_frame_hover(new_frame);

#ifdef DEBUG
  cig_trigger_layout_breakpoint(top->absolute_rect, absolute_rect);
#endif

  return new_frame;

  failure:
  if (cig__macro_ctx.open) { *cig__macro_ctx.open = NULL; }
  cig__macro_ctx.open = NULL;
  cig__macro_ctx.retain = 0;
  cig__macro_ctx.last_closed = NULL;
  return NULL;
}

M_INLINED void
handle_frame_hover(cig_frame *frame)
{
  const bool click_began = current->input.pointer.click_state == BEGAN;
  bool focus_set = false;

  if (cig_r_contains(frame->absolute_clipped_rect, current->input.pointer.position)) {
    frame->_flags |= HOVER;
    frame->_flags |= SUBTREE_INCLUSIVE_HOVER;
    cig_frame *parent = frame->_parent;

    if (click_began && frame->_flags & FOCUSABLE) {
      current->input._focus_target_this = frame->id;
      focus_set = true;
    }

    while (parent) {
      if (!focus_set && parent->_flags & FOCUSABLE && click_began) {
        current->input._focus_target_this = parent->id;
        focus_set = true;
      }
      parent->_flags |= SUBTREE_INCLUSIVE_HOVER;
      parent = parent->_parent;
    }
  }
}

static M_OPTIONAL(cig_state*)
find_state(const cig_id id)
{
  int i, open = -1, stale = -1;

  /* Find a state with a matching ID, with no ID yet, or a stale state */
  for (i = 0; i < CIG_STATES_MAX; ++i) {
    if (current->state_list[i].id == id) {
      current->state_list[i].value.active = true;
      current->state_list[i].last_tick = current->tick;
      return &current->state_list[i].value;
    }
    else if (open < 0 && !current->state_list[i].id) {
      open = i;
    }
    else if (stale < 0 && !current->state_list[i].value.active) {
      stale = i;
    }
  }

  const int result = open >= 0 ? open : stale;

  if (result >= 0) {
    current->state_list[result].id = id;
    current->state_list[result].last_tick = current->tick;
    current->state_list[result].value.active = true;

    return &current->state_list[result].value;
  }

  return NULL;
}

static M_OPTIONAL(cig_scroll_state_t*)
find_scroll_state(const cig_id id)
{
  int i, open = -1, stale = -1;

  for (i = 0; i < CIG_SCROLLABLE_ELEMENTS_MAX; ++i) {
    if (current->scroll_elements[i].id == id) {
      current->scroll_elements[i].last_tick = current->tick;
      return &current->scroll_elements[i].value;
    }
    else if (current->scroll_elements[i].id == 0 && open < 0) {
      open = i;
    }
    else if (current->scroll_elements[i].last_tick < current->tick-1 && stale < 0) {
      stale = i;
    }
  }

  const int result = open >= 0 ? open : stale;

  if (result >= 0) {
    current->scroll_elements[result].id = id;
    current->scroll_elements[result].value.offset = cig_v_zero();
    current->scroll_elements[result].last_tick = current->tick;

    return &current->scroll_elements[result].value;
  }

  return NULL;
}

static M_OPTIONAL(cig_focus*)
find_focus_state(const cig_id id)
{
  int i, open = -1, stale = -1;

  for (i = 0; i < CIG_SCROLLABLE_ELEMENTS_MAX; ++i) {
    if (current->focus_elements[i].id == id) {
      current->focus_elements[i].last_tick = current->tick;
      return &current->focus_elements[i].value;
    }
    else if (current->focus_elements[i].id == 0 && open < 0) {
      open = i;
    }
    else if (current->focus_elements[i].last_tick < current->tick-1 && stale < 0) {
      stale = i;
    }
  }

  const int result = open >= 0 ? open : stale;

  if (result >= 0) {
    current->focus_elements[result].id = id;
    current->focus_elements[result].value = (cig_focus) { 0 };
    current->focus_elements[result].last_tick = current->tick;

    return &current->focus_elements[result].value;
  }

  return NULL;
}

M_INLINED M_OPTIONAL(cig_state *) enable_state() {
  cig_frame *frame = cig_current();
  if (frame->_state) {
    return frame->_state;
  }
  return (frame->_state = find_state(frame->id));
}

static void push_clip(cig_frame *frame) {
  if (!(frame->_flags & CLIPPED)) {
    cig_buffer_element_t *buffer_element = current->buffers.peek_ref(&current->buffers, 0);
    cig_clip_rect_t_stack_t *clip_rects = &buffer_element->clip_rects;
    /* Clip against current clip rect, or just use the absolute frame of current buffer */
    cig_r clip_rect = cig_r_union(frame->absolute_rect, !clip_rects->size
      ? buffer_element->absolute_rect
      : clip_rects->peek(clip_rects, 0)
    );
    clip_rects->push(clip_rects, clip_rect);

    if (set_clip) {
      set_clip(cig_buffer(), clip_rect, false);
    }

    frame->_flags |= CLIPPED;
  }
}

static void pop_clip() {
  cig_buffer_element_t *buf_element = current->buffers.peek_ref(&current->buffers, 0);
  cig_clip_rect_t_stack_t *clip_rects = &buf_element->clip_rects;
  M_UNUSED(clip_rects->pop_ref(clip_rects));

  if (set_clip) {
    if (!clip_rects->size) {
      set_clip(cig_buffer(), buf_element->absolute_rect, true);
    } else {
      set_clip(cig_buffer(), clip_rects->peek(clip_rects, 0), false);
    }
  }
}

M_INLINED void move_to_next_row(cig_params *prm) {
  prm->_h_pos = 0;
  prm->_v_pos += (prm->_v_size + prm->spacing.y);
  prm->_h_size = 0;
  prm->_v_size = 0;
  prm->_count.h_cur = 0;
}

M_INLINED void move_to_next_column(cig_params *prm) {
  prm->_v_pos = 0;
  prm->_h_pos += (prm->_h_size + prm->spacing.x);
  prm->_h_size = 0;
  prm->_v_size = 0;
  prm->_count.v_cur = 0;
}

M_INLINED double
value_or_relative_value_of(int32_t value, int32_t relative_to, bool negate)
{
  const double v = CIG_IS_REL(value) ? CIG_REL_VALUE(value, relative_to) : value;
  return negate ? -v : v;
}

M_INLINED double
resolve_edge_attribute(
  cig_pin_attribute attribute,
  cig_pin_attribute relative_attribute,
  double value,
  cig_frame *of_frame, 
  cig_frame *relative_to_frame
)
{
  assert(of_frame);
  const cig_pin_attribute attr = relative_attribute & ~INSET_ATTRIBUTE;
  const bool is_inset = (relative_attribute & INSET_ATTRIBUTE);
  const cig_r r0 = is_inset
    ? cig_r_inset(of_frame->absolute_rect, of_frame->insets)
    : of_frame->absolute_rect;
  const cig_r r1 = cig_r_inset(relative_to_frame->absolute_rect, relative_to_frame->insets);

  switch (attr) {
  case LEFT:
    return value_or_relative_value_of(value, r0.w, attribute == RIGHT)
      + r0.x
      - r1.x;

  case RIGHT:
    return value_or_relative_value_of(value, r0.w, attribute == RIGHT)
      + r0.w
      + r0.x
      - r1.x;

  case TOP:
    return value_or_relative_value_of(value, r0.h, attribute == BOTTOM)
      + r0.y
      - r1.y;

  case BOTTOM:
    return value_or_relative_value_of(value, r0.h, attribute == BOTTOM)
      + r0.h
      + r0.y
      - r1.y;

  default:
    assert(false);
  }
}

M_INLINED double
get_attribute_value_of_relative_to(
  cig_pin_attribute attribute,
  cig_pin_attribute relative_attribute,
  double original_value,
  cig_frame *of_frame, 
  cig_frame *relative_to_frame
)
{
  const cig_pin_attribute attr = relative_attribute & ~INSET_ATTRIBUTE;
  double value = original_value;
  switch (attr) {
  case LEFT:
  case RIGHT:
  case TOP:
  case BOTTOM:
    return resolve_edge_attribute(
      attribute,
      relative_attribute,
      original_value,
      of_frame
        ? of_frame
        : relative_to_frame,
      relative_to_frame
    );

  case WIDTH:
    if (CIG_IS_REL((int32_t)value)) {
      assert(of_frame);
      return CIG_REL_VALUE((int32_t)value, of_frame->rect.w);
    } else {
      return value + (of_frame ? of_frame->rect.w : 0);
    }

  case HEIGHT:
    if (CIG_IS_REL((int32_t)value)) {
      assert(of_frame);
      return CIG_REL_VALUE((int32_t)value, of_frame->rect.h);
    } else {
      return value + (of_frame ? of_frame->rect.h : 0);
    }

  case CENTER_X:
    assert(of_frame);
    return value_or_relative_value_of(value, of_frame->absolute_rect.w, false)
      + (of_frame->absolute_rect.w * 0.5)
      + of_frame->absolute_rect.x
      - relative_to_frame->absolute_rect.x;

  case CENTER_Y:
    assert(of_frame);
    return value_or_relative_value_of(value, of_frame->absolute_rect.h, false)
      + (of_frame->absolute_rect.h * 0.5)
      + of_frame->absolute_rect.y
      - relative_to_frame->absolute_rect.y;

  case ASPECT:
    return of_frame
      ? ((double)of_frame->absolute_rect.w / of_frame->absolute_rect.h)
      : original_value;

  default:
    break;
  }

  return original_value;
}

#ifdef DEBUG

/*  ┌────────────┐
    │ DEBUG MODE │
    └────────────┘ */

void cig_set_layout_breakpoint_callback(cig_layout_breakpoint_callback_t fp) {
  layout_breakpoint_callback = fp;
}

void cig_enable_debug_stepper() {
  requested_layout_step_mode = true;
}

void cig_disable_debug_stepper() {
  current->step_mode = false;
}

void cig_trigger_layout_breakpoint(cig_r container, cig_r rect) {
  if (current->step_mode && layout_breakpoint_callback) {
    layout_breakpoint_callback(container, rect);
  }
}

#endif
