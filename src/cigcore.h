#ifndef CIG_CORE_INCLUDED
#define CIG_CORE_INCLUDED

#include "ciglimit.h"
#include "cigkeys.h"
#include "types/insets.h"
#include "types/rect.h"
#include "types/stack.h"
#include <common/macros.h>
#include <common/vec2.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*  ┌──────────────────────────┐
    │ PUBLIC TYPE DECLARATIONS │
    └──────────────────────────┘ */

/*  These macros declare a templated type essentially */
DECLARE_VEC2_T  (int32_t, cig_v)
DECLARE_INSETS_T(int32_t, cig_i)
DECLARE_RECT_T  (int32_t, cig_r, cig_v, cig_i)

/*  A couple of option bits we can use with rect components */
#define CIG__AUTO_BIT M_BIT(30)
#define CIG__REL_BIT M_BIT(29)

/*  Indicates the size of this element will be auto-calculated.
    When used in standard layout frames, AUTO essentially means to fill parent.
    In stack and grid builders it's more flexible and mostly follows parent's layout rules.
    You can pass in an additional value using CIG_REL to modify the final size.

    Eg. CIG_AUTO(CIG_REL(2)) to get a rectangle that's double the size */
#define CIG_AUTO(...) CIG__AUTO_X(,##__VA_ARGS__, CIG__AUTO_1(__VA_ARGS__), CIG__AUTO_0(__VA_ARGS__))

#define CIG__AUTO_0() CIG__AUTO_BIT
#define CIG__AUTO_1(VALUE) (VALUE < 0 ? (VALUE & ~CIG__AUTO_BIT) : (CIG__AUTO_BIT | VALUE))
#define CIG__AUTO_X(__X__,A,FUNC,...) FUNC

/*  Allows setting a relative position or size. Always relative to parent size,
    or in case of stacks and grids, the size available to the next element. */
#define CIG__REL_PRECISION 100000
#define CIG_REL(PERCENTAGE) (PERCENTAGE < 0 ? ((int)(PERCENTAGE * CIG__REL_PRECISION) & ~CIG__REL_BIT) : (CIG__REL_BIT | (int)(PERCENTAGE * CIG__REL_PRECISION)))

/*  Full AUTO rect */
#define RECT_AUTO cig_r_make(0, 0, CIG_AUTO(), CIG_AUTO())

/*  Full AUTO rect, but with overridable width or height */
#define RECT_AUTO_W(W) cig_r_make(0, 0, W, CIG_AUTO())
#define RECT_AUTO_H(H) cig_r_make(0, 0, CIG_AUTO(), H)

/*  Standard rectangle helpers */
#define RECT(X, Y, W, H) cig_r_make(X, Y, W, H)
#define RECT_SIZED(W, H) cig_r_make(0, 0, W, H)

/*  All-relative rectangle. All values should be in the range [0.0 ... 1.0] */
#define RECT_REL(X, Y, W, H) cig_r_make(CIG_REL(X), CIG_REL(Y), CIG_REL(W), CIG_REL(H))

#define CIG_FILL CIG_REL(1.0)
#define RECT_FILL cig_r_make(0, 0, CIG_FILL, CIG_FILL)

#define NO_INSETS cig_i_zero()

#define CIG_CLICK_EXPIRE_IN_SECONDS 0.5f

#define CIG_DEFAULT_KEY_REPEAT_RATE 0.25f

/*  All layout element get a unique ID that tries to be unique across frames, but no promises.
    See `cig_next_id` how to definitely keep things consistent */
typedef uintptr_t cig_id;

/*  Opaque pointer to a buffer/screen/texture/etc to be renderered into */
typedef void* cig_buffer_ref;

typedef void (*cig_set_clip_callback)(cig_buffer_ref, cig_r, bool);

/*  Structure containing parameters passed to layout function */
typedef struct {
  /*  One or more axis which a builder uses to position children */
  enum M_PACKED {
    CIG__NOAXIS = 0,
    CIG_LAYOUT_AXIS_HORIZONTAL = M_BIT(1),
    CIG_LAYOUT_AXIS_VERTICAL = M_BIT(2),
    CIG_LAYOUT_AXIS_BOTH = CIG_LAYOUT_AXIS_HORIZONTAL | CIG_LAYOUT_AXIS_VERTICAL
  } axis;

  /*  Direction in which the layout flows. Used by default grid builder */
  enum M_PACKED {
    CIG_LAYOUT_DIRECTION_DEFAULT = 0,
    CIG_LAYOUT_DIRECTION_HORIZONTAL,
    CIG_LAYOUT_DIRECTION_VERTICAL
  } direction;

  struct {
    enum M_PACKED {
      CIG_LAYOUT_ALIGNS_LEFT = 0,
      CIG_LAYOUT_ALIGNS_CENTER,
      CIG_LAYOUT_ALIGNS_RIGHT
    } horizontal;
    enum M_PACKED {
      CIG_LAYOUT_ALIGNS_TOP = 0,
      CIG_LAYOUT_ALIGNS_MIDDLE,
      CIG_LAYOUT_ALIGNS_BOTTOM
    } vertical;
  } alignment;

  /* Horizontal and vertical spacing between elements. Use is determined by type of layout function */
  cig_v spacing;

  /*  Some common parameters the layout builder could use */
  int16_t width, height, columns, rows;

  /*  Limits how many elements can be added per axis on in total.
      Total number of elements is checked in `cig_push_frame` but horizontal
      and vertical limits are only used in the default stack/grid builder */
  struct {
    int16_t horizontal, vertical, total;
  } limit;

  struct {
    int32_t width, height;
  } size_max, size_min;

  /*  Some basic layout flags */
  enum M_PACKED {
    CIG_LAYOUT_DISABLE_CULLING = M_BIT(0),
    CIG_LAYOUT_MINIMUM_LIMIT = M_BIT(1)
  } flags;

  /*  Opaque pointer for passing custom data to a custom layout builder */
  void *custom_data;

  /*__PRIVATE__*/
  int32_t _h_pos, _v_pos, _h_size, _v_size;
  struct {
    int16_t h_cur, v_cur, total;
  } _count; /* h_ and v_cur are only counted in stacks/grids */
} cig_params;

typedef struct {
  uint8_t *bytes;
  size_t size;
} cig_memory_st;

typedef struct {
  bool active;
  cig_memory_st memory;
} cig_state;

typedef struct cig_focus {
  bool active;
  unsigned int last_change_tick;
  struct cig_focus *parent;
} cig_focus;

/* */
typedef struct {
  /* Scroll offset from top-left corner */
  cig_v offset;
  /* Amount you can scroll until bottom-right corner is flush with parent. Always >= 0 */
  cig_v distance;
  /* Total size of scrollable content */
  cig_v bounds;
} cig_scroll_state_t;

typedef struct {
  cig_r rect;
  cig_i insets;
  cig_params params;
  bool (*builder)(cig_r, cig_r, cig_params*, cig_r*);
} cig_args;

typedef enum M_PACKED {
  CIG_FRAME_APPEARED = 1,
  CIG_FRAME_VISIBLE
} cig_frame_visibility;

/* */
typedef struct cig_frame {
  cig_id id;
  cig_r rect,                   /* Relative rect */
        clipped_rect,           /* Relative clipped rect */
        absolute_rect,          /* Screen-space rect */
        absolute_clipped_rect,  /* Screen-space rect */
        content_rect;           /* Relative rect bounding the content */
  cig_i insets;                 /* Insets affect child elements within this element */
  cig_frame_visibility visibility;

  /*__PRIVATE__*/      
  bool (*_layout_function)(cig_r, cig_r, cig_params*, cig_r*);
  cig_scroll_state_t *_scroll_state;
  cig_state *_state;
  cig_focus *_focus;
  struct cig_frame *_parent;
  cig_params _layout_params;
  unsigned int _id_counter, _last_tick;
  enum M_PACKED {
    /* */
    OPEN = M_BIT(0),
    /* This element has hover */
    HOVER = M_BIT(1),
    /* This element or one if its descendants has hover */
    SUBTREE_INCLUSIVE_HOVER = M_BIT(2),
    /* Clipping is enabled for this element */
    CLIPPED = M_BIT(3),
    /**/
    INTERACTIBLE = M_BIT(4),
    /* Element is focusable and affects child focus query */
    FOCUSABLE = M_BIT(5),
    /**/
    RETAINED = M_BIT(6)
  } _flags;
} cig_frame;

typedef enum M_PACKED {
  CIG_INPUT_PRIMARY_ACTION = M_BIT(0),
  CIG_INPUT_SECONDARY_ACTION = M_BIT(1),
  CIG_INPUT_ACTION_ANY = CIG_INPUT_PRIMARY_ACTION | CIG_INPUT_SECONDARY_ACTION
} cig_input_action_type;

typedef enum M_PACKED {
  /* All quiet on the front-end */
  CIG_DRAG_STATE_INACTIVE,

  /**
   * When input is first pressed mouse location is saved but
   * drag is not recignized yet. Active frame ID is saved.
   */
  CIG_DRAG_STATE_READY,

  /* Ready for tracking, but has not exceeded minimum delta (2pt) yet */
  CIG_DRAG_STATE_WAITING_MOVE,

  /**
   * Tracking exceeded some threshold and draging is activated.
   * Movement delta can be read.
   */
  CIG_DRAG_STATE_BEGAN,

  /* Tracking position has changed compared to last value */
  CIG_DRAG_STATE_MOVED,

  /* No change in drag position compared to last frame */
  CIG_DRAG_STATE_IDLE,

  /* Button released and input tracking stops. Movement delta can be read. */
  CIG_DRAG_STATE_ENDED
} cig_input_drag_state;

/* Key state bit flags */
typedef enum M_PACKED {
  /* No status to read. Default state. */
  CIG_KEY_IDLE = 0,

  /* Key was released. Flag is only set for one iteration. */
  CIG_KEY_RELEASED = M_BIT(1),

  /* Key is pressed. Flag is set the whole duration the key is held down. */
  CIG_KEY_PRESSED = M_BIT(2),

  /* Key is first clicked. Flag is only set for one iteration. */
  CIG_KEY_CLICKED = M_BIT(3),

  /* Key is held. Flag is set for one iteration, periodically. */
  CIG_KEY_REPEATED = M_BIT(4)
} cig_input_key_state;

typedef struct {
  /**
   * Pointer struct contains pointer position, click and drag state details.
   */
  struct {
    cig_input_action_type action_mask,
                          last_action_began,
                          last_action_ended;
    cig_v position;

    enum M_PACKED {
      NEITHER,  /* Button was neither pressed or released */
      BEGAN,    /* Button was pressed down (click started) */
      ENDED,    /* Button was released this (click ended) */
      EXPIRED   /* Button was held longer than deemed appropriate */
    } click_state;

    struct {
      cig_input_drag_state state;
      cig_id id;
      cig_v change_total,
            change_last_frame;

      /*_PRIVATE_*/
      cig_v _start_position_absolute;
    } drag;

    /**
     * Locked: Hover check is disabled.
     * 
     * Set to TRUE by widgets that want exclusive use of drag state.
     * A scrollbar thumb for example where buttons and other elements
     * should not be highlighted even if hovered while moving.
     * 
     * Reset to FALSE when drag ends.
     */
    bool locked;

    /* __PRIVATE__ */
    cig_id _press_target_id,    /* Element that was hovered when button press began */
           _hover_prev_tick,
           _hover_this_tick;
    float _press_start_time,
          _click_end_time;
    unsigned int _click_count;
  } pointer;

  struct {
    struct {
      uint8_t state;
      cig_id owned_by,
             listener_prev_tick,
             listener_this_tick;
      unsigned int last_update_at;
      float repeat_timer;
    } code[CIG__KEY_COUNT];
  } key;

  /*_PRIVATE_*/
  cig_id _focus_target_this,
         _focus_target;
  float key_repeat_rate;
} cig_input_state_t;

typedef enum M_PACKED {
  /*  `CIG_PRESS_INSIDE` option specifies whether the press has to start
      within the bounds of this element. Otherwise it can start outside,
      and the element will reflect pressed state as soon as mouse moves onto it */
  CIG_PRESS_INSIDE = M_BIT(0),
  CIG_PRESS_DEFAULT_OPTIONS = CIG_PRESS_INSIDE
} cig_press_flags;

typedef enum M_PACKED {
  CIG_CLICK_STARTS_INSIDE = M_BIT(0),
  CIG_CLICK_ON_PRESS = M_BIT(1),
  CIG_CLICK_EXPIRE = M_BIT(2),
  CIG_CLICK_DOUBLE = M_BIT(3),
  CIG_CLICK_DEFAULT_OPTIONS = CIG_CLICK_STARTS_INSIDE
} cig_click_flags;

typedef enum M_PACKED {
  UNSPECIFIED = 0,

  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
  WIDTH,
  HEIGHT,
  CENTER_X,
  CENTER_Y,
  ASPECT,

  /* Option modifiers */
  INSET_ATTRIBUTE = M_BIT(31),

  LEFT_INSET = LEFT | INSET_ATTRIBUTE,
  RIGHT_INSET = RIGHT | INSET_ATTRIBUTE,
  TOP_INSET = TOP | INSET_ATTRIBUTE,
  BOTTOM_INSET = BOTTOM | INSET_ATTRIBUTE,
} cig_pin_attribute;

typedef struct {
  cig_pin_attribute attribute;
  double value;
  cig_frame *relation;
  cig_pin_attribute relation_attribute;
} cig_pin;

typedef cig_r cig_clip_rect_t;
#define STACK_CAPACITY_cig_clip_rect_t CIG_BUFFER_CLIP_REGIONS_MAX
DECLARE_ARRAY_STACK_T(cig_clip_rect_t)

typedef struct {
  cig_buffer_ref buffer;
  cig_r absolute_rect;
  cig_v origin;
  cig_clip_rect_t_stack_t clip_rects;
} cig_buffer_element_t;

typedef cig_frame* cig_frame_ref;
#define STACK_CAPACITY_cig_frame_ref CIG_NESTED_ELEMENTS_MAX
DECLARE_ARRAY_STACK_T(cig_frame_ref)

#define STACK_CAPACITY_cig_buffer_element_t CIG_BUFFERS_MAX
DECLARE_ARRAY_STACK_T(cig_buffer_element_t)

typedef struct {
  void *(*alloc)  (void *ud, size_t size, size_t align);
  void *(*realloc)(void *ud, void *ptr, size_t old_size, size_t new_size);
  void  (*free)   (void *ud, void *ptr);
  void *ud;
  size_t tracked_bytes;
} cig_allocator;

/*  A single instance of CIG. Use one for each game state?
    Should be considered an opaque type! */
typedef struct {
  /*  __PRIVATE__ */
  cig_allocator allocator;
  cig_frame_ref_stack_t frame_stack;
  cig_buffer_element_t_stack_t buffers;
  cig_input_state_t input;
  cig_i default_insets;
  cig_id next_id;
  float delta_time,
        elapsed_time;
  unsigned int tick;
  struct {
    cig_id id;
    unsigned int last_tick;
    cig_scroll_state_t value;
  } scroll_elements[CIG_SCROLLABLE_ELEMENTS_MAX];
  struct {
    cig_id id;
    cig_state value;
    unsigned int last_tick;
  } state_list[CIG_STATES_MAX];
  struct {
    cig_id id;
    unsigned int last_tick;
    cig_focus value;
  } focus_elements[CIG_FOCUSABLE_ELEMENTS_MAX];
  struct {
    cig_frame elements[CIG_ELEMENTS_MAX];
    size_t high;
  } frames;
  cig_focus *top_focus;
#ifdef DEBUG
  bool step_mode;
#endif
} cig_context;

/*  ┌─────────────┐
    │ CORE LAYOUT │
    └─────────────┘ */

/*  Call this once to initalize (or reset) the context */
void cig_init_context(cig_context*);

/* */
void cig_begin_layout(cig_context*, M_OPTIONAL(cig_buffer_ref), cig_r, float);

/* */
void cig_end_layout();

/* @return Screen-space rectangle of current CIG context */
cig_r cig_layout_rect();

/*  Returns an opaque pointer to the current buffer where drawing operations would take place */
cig_buffer_ref cig_buffer();

/*  Pushes a new frame with args struct containing all relevant data.
    @return Reference to new element if rect is visible within current container, NULL otherwise */
cig_frame * cig_push_frame_args(cig_args);

/*  Pushes a new frame with default insets (see `cig_set_default_insets`) to layout stack.
    @return Reference to new element if rect is visible within current container, NULL otherwise */
cig_frame * cig_push_frame(cig_r);

/*  Push a new frame with custom insets to layout stack.
    @return Reference to new element if rect is visible within current container, NULL otherwise */
cig_frame * cig_push_frame_insets(cig_r, cig_i);

/*  Push a new frame with custom insets and params to layout stack.
    @return Reference to new element if rect is visible within current container, NULL otherwise */
cig_frame * cig_push_frame_insets_params(cig_r, cig_i, cig_params);

/*  Push layout builder function to layout stack.
    @return Reference to new element if rect is visible within current container, NULL otherwise */
cig_frame * cig_push_layout_function(
  bool (*)(cig_r, cig_r, cig_params*, cig_r*),
  cig_r,
  cig_i,
  cig_params
);

/*  Pop and return the last element in the layout stack */
cig_frame* cig_pop_frame();

/*  Sets insets used by all consecutive `cig_push_frame` calls */
void cig_set_default_insets(cig_i);

/*  @return Current layout element */
cig_frame* cig_current();

/*  @return Current local rect relative to its parent */
M_INLINED cig_r cig_rect() { return cig_current()->rect; }

/*  @return Current local rect that's been clipped */
M_INLINED cig_r cig_clipped_rect() { return cig_current()->clipped_rect; }

/*  @return Current screen-space rect */
M_INLINED cig_r cig_absolute_rect() { return cig_current()->absolute_rect; }

/*  @return Relative bounding rect for current content */
M_INLINED cig_r cig_content_rect() { return cig_current()->content_rect; }

/* @return Current frame visibility status (appeared, visible, not visible) */
M_INLINED cig_frame_visibility cig_visibility() {
  cig_frame *open_frame = cig_current();
  assert(open_frame->_flags & RETAINED);
  return open_frame->visibility;
}

/*  Converts a relative rect to a screen-space rect */
cig_r cig_convert_relative_rect(cig_r);

/*  @return Pointer to the current layout element stack. Avoid accessing if possible. */
cig_frame_ref_stack_t* cig_frame_stack();

/*  ┌───────────────────────────┐
    │ STATE & MEMORY ALLOCATION │
    └───────────────────────────┘ */

M_INLINED M_OPTIONAL(cig_frame*) cig_retain(M_OPTIONAL(cig_frame*) frame) {
  if (frame) {
    frame->_flags |= RETAINED;
  }
  return frame;
}

/* Data allocated by and associated with the current element */
M_OPTIONAL(void*) cig_memory_allocation(size_t*);

/**
 * @brief Allocates memory for the current element using the configured allocator.
 * 
 * @param bytes - Amount of bytes to allocate for this element
 * 
 * @return Pointer to the new block of memory or NULL if memory could not be allocated
 */
M_OPTIONAL(void*) cig_memory_allocate(size_t bytes);

/**
 * Free memory associated with the current element
 */
void cig_memory_free();

/**/
size_t cig_tracked_bytes(void);

/*  ┌──────────────────────────────┐
    │ TEMPORARY BUFFERS (ADVANCED) │
    └──────────────────────────────┘ */

/*  Similar to `cig_begin_layout` where you start rendering into a new buffer,
    in the current element's coordinate system. All subsequent elements `absolute_rect`-s
    are relative to this buffer/screen/texture.
    
    This is mostly when you want to cache
    some more complex widget, like a large text view or similar. You can internally
    check whether you need to redraw or just re-render the old buffer/screen/texture */  
void cig_push_buffer(cig_buffer_ref);

/*  Pops the previously pushed buffer. Does not reset anything else about the state
    of the UI, unlike `cig_end_layout` */
void cig_pop_buffer();

/**
 * ┌────────────────────────────────────────────────────────────────────────────────┐
 * │ INPUT INTERACTION                                                              │
 * │                                                                                │
 * │ Input is/should be sampled at the start of each iteration.                     │
 * │                                                                                │
 * │ Which element may consume that input is based on ownership claims collected    │
 * │ during the previous iteration. During the current iteration, elements register │
 * │ claims for the next iteration. If multiple elements claim the same input, the  │
 * │ last registered claim wins.                                                    │
 * └────────────────────────────────────────────────────────────────────────────────┘
 */

/* Update pointer position */
void
cig_set_pointer_position(cig_v);

/* Update pointer state */
void
cig_set_pointer_state(cig_input_action_type);

/* Set key state (pressed or not) */
void cig_set_key_state(cig_key_code, bool);

/**
 * Sets how often the key is repeated, meaning when `CIG_KEY_REPEATED` flag is set.
 * Default: `CIG_DEFAULT_KEY_REPEAT_RATE`
 * 
 * @return: Current repeat rate prior to updating
 */
M_DISCARDABLE(float) cig_set_key_repeat_rate(float);

/*  @return Current input state as updated by last `cig_set_input_state` call */
M_OPTIONAL(cig_input_state_t*) cig_input_state();

/*  Enables input tracking for the current layout element.
    Call this after a successful `cig_push_frame` call */
void cig_enable_interaction();

/*  Checks if the current layout element is the topmost element at current input position */
bool cig_hovered();

/*  Checks if the current element is hovered and the mouse button is pressed.
    See `cig_press_flags` declaration for more info */
cig_input_action_type cig_pressed(cig_input_action_type, cig_press_flags);

/*  Checks if the current element is hovered and mouse button was clicked or released
    depending on the options. See `cig_click_flags` declaration for more info */
cig_input_action_type cig_clicked(cig_input_action_type, cig_click_flags);

/* After `cig_pressed` has been called with the desired options, drag can be initiated
   and its state be read. This function only returns the state of the drag gesture.
   Details can be read from `cig_input_state().drag` structure */
cig_input_drag_state cig_dragged(cig_input_action_type);

/**
 * Checks and consumes given keycode. The last element to register for a key
 * during the last iteration is the consumer for the current iteration.
 * 
 * Key state transition: IDLE > (PRESSED + CLICKED) > (PRESSED + REPEATED)* > RELEASED > IDLE
 */
cig_input_key_state cig_key(cig_key_code);

/**
 * Similar to `cig_key()` but allows consuming all keys at once.
 *
 * @key: Key code for the next key in the queue. May be NULL.
 * @state: Key state for the next key in the queue. May be NULL.
 * 
 * @return: Returns FALSE when queue is exhausted
 */
bool cig_key_poll(cig_key_code* key, cig_input_key_state* state);

/* Reads raw key state, ignoring ownership and consumption */
bool cig_key_raw_pressed(cig_key_code);
bool cig_key_raw_clicked(cig_key_code);
bool cig_key_raw_repeated(cig_key_code);
bool cig_key_raw_released(cig_key_code);

/**
 * Reads next key from the queue, ignoring ownership and consumption.
 * 
 * @key: Key code for the next key in the queue. May be NULL.
 * @state: Key state for the next key in the queue. May be NULL.
 * 
 * @return: Returns FALSE when queue is exhausted
 */
bool cig_key_raw_poll(cig_key_code* key, cig_input_key_state* state);


/**
 * ┌─────────────────────────────────────────────────────────────────────────────────┐
 * │ FOCUS                                                                           │
 * │                                                                                 │
 * │ General purpose way of marking frames focusable (or selectable). Can be used    │
 * │ to mark active windows, input fields for receiving text inputs, etc.            │
 * │                                                                                 │
 * │ Internally, frame is focused when mouse button press while hovered.             │
 * │                                                                                 │
 * │ Focus propagates through the frame hierarchy. A child frame that does not       │
 * │ explicitly manage focus is still considered focused if one of its ancestors is. │
 * │ Similarly, all ancestors of a focused frame are marked as focused.              │
 * │                                                                                 │
 * │ Each frame can only have one focused child; others are automatically unfocused. │
 * │                                                                                 │
 * │ ! Acquiring focus is delayed by one layout iteration. This allows the library   │
 * │   to determine the topmost focused path through the hierarchy before applying   │
 * │   focus changes. Losing focus happens immediately and can be observed in the    │
 * │   same iteration.                                                               │
 * └─────────────────────────────────────────────────────────────────────────────────┘
 */

/**
 * Makes current frame focusable. Focus is gained by pressing mouse button in this frame
 * regardless of interaction being enabled or not; or focus can be set externally.
 * 
 * @state: Optional binding to set & observe the focus state externally. Passing NULL
 * allocates internal state.
 * 
 * @return: Whether this element is currently focused
 */
bool cig_enable_focus(bool *state);

/**
 * Checks whether current frame is actively focused or one of its ancestors has focus.
 * 
 * @return: Current focus state
 */
bool cig_focused(void);

/* Returns TRUE when current frame has just gained focus, FALSE otherwise. */
bool cig_gained_focus(void);

/* Returns TRUE when current frame has just lost focus, FALSE otherwise. */
bool cig_lost_focus(void);

/*  ┌───────────┐
    │ SCROLLING │
    └───────────┘ */

/*  Attempts to enable scrolling for the current layout element and allocate an
    internal scroll state that remains constant between ticks. You can also provide
    a pointer to the struct stored somewhere in your application layer. Pass NULL for
    the default behavior described above.
    
    The state pool is limited to `CIG_STATES_MAX` and if there are too many
    scrolling elements already, it may fail.
    
    @return TRUE if state could be allocated, FALSE otherwise */
bool cig_enable_scroll(cig_scroll_state_t*);

/*  @return Current scroll state objet, or NULL if scrolling is not enabled */
M_OPTIONAL(cig_scroll_state_t*) cig_scroll_state();

/*  Set scroll offset values */
void cig_set_offset(cig_v);

/*  Change scroll offset values */
void cig_change_offset(cig_v);

/*  @return Scroller offset */
cig_v cig_offset();

/*  ┌────────────────┐
    │ LAYOUT HELPERS │
    └────────────────┘ */

cig_r cig_build_rect(size_t, cig_pin[]);

/*  By default, when adding rects that are completely outside the bounds
    of the parent, `cig_push_frame` calls return NULL. You can disable that
    behavior with this */
void cig_disable_culling();

/*  Enables clipping for the current layout element */
void cig_enable_clipping();

/*  Normally element ID is auto-calculated and may vary from tick to tick.
    This sets an explicit Id for the next `cig_push_frame` call.
    See `cig_hash` for generating an ID from a string */
void cig_set_next_id(cig_id);

/*  Depth of the current layout stack */
unsigned int cig_depth();

/*  Generates an ID from a string */
cig_id cig_hash(const char *str);

/*  Determines if current layout direction is vertical or not. False when undeterminable */
bool cig_is_vertical_layout();

/*  Pushes and pops an empty frame to trigger a layout function to allocate space.
    Useful when you have a stack or grid and want to trigger a new line or column */
void cig_empty();

/* */
void cig_spacer(int size);

/*  Default layout function for stack and grid type */
bool cig_default_layout_builder(cig_r, cig_r, cig_params*, cig_r*);

cig_frame* cig_push_hstack(cig_r, cig_i, cig_params);

cig_frame* cig_push_vstack(cig_r, cig_i, cig_params);

cig_frame* cig_push_grid(cig_r, cig_i, cig_params);

/*  ┌─────────┐
    │ UTILITY │
    └─────────┘ */

float cig_delta_time();

float cig_elapsed_time();

/*  ┌───────────────────┐
    │ BACKEND CALLBACKS │
    └───────────────────┘ */

void cig_set_allocator(cig_context*, cig_allocator);

void cig_assign_set_clip(cig_set_clip_callback);

#ifdef DEBUG

/*  ┌────────────┐
    │ DEBUG MODE │
    └────────────┘ */

typedef void (*cig_layout_breakpoint_callback_t)(cig_r, cig_r);

void cig_set_layout_breakpoint_callback(cig_layout_breakpoint_callback_t);

/*  Starts stepping through the hierarchy starting on next layout pass*/
void cig_enable_debug_stepper();

/*  Can be called during step-through to cancel and go back to real-time rendering */
void cig_disable_debug_stepper();

/*  If step mode is active, triggers a breakpoint you can use to
    visualize the layout as it currently stands. Two rectangles
    indicate what is being laid out into what */
void cig_trigger_layout_breakpoint(cig_r container, cig_r rect);

#endif

/*  ┌────────────────────┐
    │ AUTO & REL READERS │
    └────────────────────┘ */

/*  Is CIG__AUTO_BIT set? For negative numbers we invert the mask because two's complement */
#define CIG_IS_AUTO(N) \
  (((N < 0 ? N ^ CIG__AUTO_BIT : N) & CIG__AUTO_BIT))

/*  Is CIG__REL_BIT set? For negative numbers we invert the mask because two's complement */
#define CIG_IS_REL(N) \
  ((N < 0 ? N ^ CIG__REL_BIT : N) & CIG__REL_BIT)

/*  Clear option bits and get REL value */
#define CIG_REL_VALUE(N, BASE) \
  round(((N < 0 ? (N | CIG__AUTO_BIT | CIG__REL_BIT) : (N & ~(CIG__AUTO_BIT | CIG__REL_BIT))) * (1.0/CIG__REL_PRECISION)) * BASE)

/*  Clear option bits and get REL or AUTO value if option set */
#define CIG_ANY_VALUE(N, BASE) \
  (CIG_IS_REL(N) ? CIG_REL_VALUE(N, BASE) : (CIG_IS_AUTO(N) ? BASE : N))

#endif
