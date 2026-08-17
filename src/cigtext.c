#include "cigtext.h"
#include "cigcorem.h"
#include "utf8.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_TAG_NAME_LEN 16
#define MAX_TAG_VALUE_LEN 32

#define IS_CODEPOINT_NEWLINE(CP) (CP == 0x0A)
#define IS_CODEPOINT_SPACE(CP) (CP == 0x20)

typedef struct {
  unsigned short w, h;
} bounds_t;

typedef struct {
  bool opened_or_closed,
       open,
       terminating,
       _naming;
  char name[MAX_TAG_NAME_LEN],
       value[MAX_TAG_VALUE_LEN];
} tag_parser_t;

typedef struct {
  bool reading;
  size_t start;
  struct {
    char *str;
    size_t index;
    utf8_string slice;
    cig_v bounds;
  } last_fitting;
} span_run;

/* Scope is for containing all the necessary variables for processing
   text and creating spans out of them. They are encapsulated here
   so that multiple `label_process_string` calls could be made.
   This will be needed for displaying larger gap_buffer based texts. */
typedef struct {
  struct {
    cig_font_ref fonts[4];
    size_t count;
  } font_stack;
  struct {
    cig_text_color_ref colors[4];
    size_t count;
  } color_stack;
  tag_parser_t tag_parser;
  span_run run;
  cig_font_info_st base_font_info;
  utf8_string utext;
  utf8_char_iter iter;
  utf8_char ch;
  uint32_t cp;
  int32_t line_width;
  size_t i,
         newlines,
         line_count;
  cig_text_style style;
  const bool wrap_width;
} scope_st;

static cig_draw_text_callback render_callback = NULL;
static cig_measure_text_callback measure_callback = NULL;
static cig_query_font_callback font_query = NULL;
static cig_font_ref default_font = 0;
static cig_text_color_ref default_text_color = 0;
static char printf_buf[CIG_LABEL_PRINTF_BUF_LENGTH];

static void
label_prepare(
  cig_label*,
  cig_text_properties*
);

static void
label_reset(
  cig_label*,
  cig_text_properties*
);

static void
label_process_string(
  cig_label*,
  scope_st*,
  cig_text_properties*,
  cig_v,
  const char*
);

static void render_spans(cig_span *, size_t, cig_font_ref, cig_text_color_ref, cig_text_horizontal_alignment, cig_text_vertical_alignment, bounds_t, int);
static void wrap_text(utf8_string *, size_t, cig_v *, cig_text_overflow, cig_font_ref, cig_font_ref, cig_text_color_ref, cig_text_style, int32_t, cig_span *);
static bool tag_parser_append(tag_parser_t*, utf8_char, uint32_t);

static void
scope_apply_tag(scope_st*);

static bool
scope_process_tags(scope_st*);

static cig_span*
scope_add_label_span(
  scope_st*,
  cig_label*,
  utf8_string,
  cig_font_ref,
  cig_text_color_ref,
  cig_text_style,
  cig_v,
  size_t
);

/*  ┌───────────────────┐
    │ BACKEND CALLBACKS │
    └───────────────────┘ */

void cig_assign_draw_text(cig_draw_text_callback callback) {
  render_callback = callback;
}

void cig_assign_measure_text(cig_measure_text_callback callback) {
  measure_callback = callback;
}

void cig_assign_query_font(cig_query_font_callback callback) {
  font_query = callback;
}

/*  ┌──────────────┐
    │ TEXT DISPLAY │
    └──────────────┘ */

void
cig_set_default_font(cig_font_ref font)
{
  default_font = font;
}

void
cig_set_default_text_color(cig_text_color_ref color)
{
  default_text_color = color;
}

cig_font_info_st
cig_font_info(cig_font_ref font)
{
  return font_query(font);
}

M_DISCARDABLE(cig_label *) 
cig_draw_label(cig_text_properties props, const char *text, ...) 
{
  register const cig_r absolute_rect = cig_r_inset(cig_absolute_rect(), cig_current()->insets);

  const char *str;
  cig_label *label;

  if (!(label = cig_mem_read(NULL))) {
    label = cig_mem_alloc(NULL, sizeof(cig_label) + sizeof(cig_span[CIG_LABEL_SPANS_MAX]));
    label->available_spans = CIG_LABEL_SPANS_MAX;
  }

  label_prepare(label, &props);

  if (props.flags & CIG_TEXT_FORMATTED) {
    va_list args;
    va_start(args, text);
    vsprintf(printf_buf, text, args);
    va_end(args);
    str = printf_buf;
  } else {
    str = text;
  }

  const cig_v max_bounds = cig_r_size(absolute_rect);
  const cig_id hash = cig_hash(str) + (cig_id)props.font + CIG_TINYHASH(max_bounds.x, max_bounds.y);

  if (label->hash != hash) {
    label->hash = hash;
    label_reset(label, &props);

    utf8_string utext = make_utf8_string(str);

    scope_st scope = (scope_st) {
      .base_font_info = font_query(label->font),
      .utext = utext,
      .iter = make_utf8_char_iter(utext),
      .line_count = 1,
      .wrap_width = (max_bounds.x > 0) && !(props.flags & CIG_TEXT_HORIZONTAL_WRAP_DISABLED)
    };

    label_process_string(label, &scope, &props, max_bounds, text);
  }

  if (render_callback) {
    render_spans(
      label->spans,
      label->span_count,
      label->font,
      label->color,
      label->alignment.horizontal,
      label->alignment.vertical,
      (bounds_t) { label->bounds.w, label->bounds.h },
      label->line_spacing
    );
  }

  return label;
}

cig_label*
cig_label_prepare(
  cig_label *label,
  cig_v max_bounds,
  cig_text_properties props,
  const char *text,
  ...
) {
  const char *str;
  label_prepare(label, &props);

  if (props.flags & CIG_TEXT_FORMATTED) {
    va_list args;
    va_start(args, text);
    vsprintf(printf_buf, text, args);
    va_end(args);
    str = printf_buf;
  } else {
    str = text;
  }

  const cig_id hash = cig_hash(str) + (cig_id)props.font + CIG_TINYHASH(max_bounds.x, max_bounds.y);

  if (label->hash != hash) {
    label->hash = hash;
    label_reset(label, &props);

    utf8_string utext = make_utf8_string(str);

    scope_st scope = (scope_st) {
      .base_font_info = font_query(label->font),
      .utext = utext,
      .iter = make_utf8_char_iter(utext),
      .line_count = 1,
      .wrap_width = (max_bounds.x > 0) && !(props.flags & CIG_TEXT_HORIZONTAL_WRAP_DISABLED)
    };

    label_process_string(label, &scope, &props, max_bounds, text);
  }

  return label;
}

void cig_label_draw(cig_label *label) {
  if (render_callback) {
    render_spans(
      label->spans,
      label->span_count,
      label->font,
      label->color,
      label->alignment.horizontal,
      label->alignment.vertical,
      (bounds_t) { label->bounds.w, label->bounds.h },
      label->line_spacing
    );
  }
}

cig_v cig_measure_raw_text(
  cig_font_ref font,
  cig_text_style style,
  const char *text
) {
  utf8_string utf8_str = make_utf8_string(text);
  cig_font_ref _font = font ? font : default_font;
  return measure_callback(utf8_str.str, utf8_str.byte_len, _font, style);
}

cig_v cig_measure_raw_text_formatted(
  cig_font_ref font,
  cig_text_style style,
  const char *text,
  ...
) {
  va_list args;
  va_start(args, text);
  vsprintf(printf_buf, text, args);
  va_end(args);
  utf8_string utf8_str = make_utf8_string(printf_buf);
  cig_font_ref _font = font ? font : default_font;
  return measure_callback(utf8_str.str, utf8_str.byte_len, _font, style);
}

void cig_draw_raw_text(
  cig_v position,
  cig_v bounds,
  cig_font_ref font,
  cig_text_style style,
  cig_text_color_ref color,
  const char *text
) {
  utf8_string utf8_str = make_utf8_string(text);
  cig_font_ref _font = font ? font : default_font;
  cig_text_color_ref _color = color ? color : default_text_color;
  cig_v _bounds = (bounds.x || bounds.y) ? bounds : measure_callback(utf8_str.str, utf8_str.byte_len, _font, style);
  render_callback(utf8_str.str, utf8_str.byte_len, cig_r_make(position.x, position.y, _bounds.x, _bounds.y), _font, _color, style);
}

void cig_draw_raw_text_formatted(
  cig_v position,
  cig_v bounds,
  cig_font_ref font,
  cig_text_style style,
  cig_text_color_ref color,
  const char *text,
  ...
) {
  va_list args;
  va_start(args, text);
  vsprintf(printf_buf, text, args);
  va_end(args);
  utf8_string utf8_str = make_utf8_string(printf_buf);
  cig_font_ref _font = font ? font : default_font;
  cig_text_color_ref _color = color ? color : default_text_color;
  cig_v _bounds = (bounds.x || bounds.y) ? bounds : measure_callback(utf8_str.str, utf8_str.byte_len, _font, style);
  render_callback(utf8_str.str, utf8_str.byte_len, cig_r_make(position.x, position.y, _bounds.x, _bounds.y), _font, _color, style);
}

/*  ┌────────────────────┐
    │ INTERNAL FUNCTIONS │
    └────────────────────┘ */

/* For setting values from props that don't affect how spans are laid out:
    - Color
    - Horizontal & vertical alignment within parent */
static void
label_prepare(
  cig_label *label,
  cig_text_properties *props
) {
  label->color = props->color
    ? props->color
    : default_text_color;

  label->alignment.horizontal = props->alignment.horizontal == CIG_TEXT_ALIGN_DEFAULT
    ? CIG_TEXT_ALIGN_CENTER
    : props->alignment.horizontal;

  label->alignment.vertical = props->alignment.vertical == CIG_TEXT_ALIGN_DEFAULT
    ? CIG_TEXT_ALIGN_MIDDLE
    : props->alignment.vertical;
}

/* Resets label counters for parsing text */
static void
label_reset(
  cig_label *label,
  cig_text_properties *props
) {
    label->span_count = 0;
    label->bounds.w = 0;
    label->bounds.h = 0;
    label->line_spacing = props->line_spacing;
    label->font = props->font ? props->font : default_font;
}

/* Process single piece of text and append all created spans to the label.
   This can be called multiple times with multiple strings. */
static void label_process_string(
  cig_label *label,
  scope_st *scope,
  cig_text_properties *props,
  cig_v max_bounds,
  const char *str
) {
  while ((scope->ch = next_utf8_char(&scope->iter)).byte_len > 0 && (scope->cp = unicode_code_point(scope->ch))) {

    /*=================================================================
      1: Process tags
    ==================================================================*/

    if (scope_process_tags(scope)) {
      continue;
    }


    /*=================================================================
      2: Check for span termination

      Span is terminated by either:

        a) Newline character
        b) End of string
        c) Tag opening/closing
        d) Encountering a space character
          > Space ends the current word, and if it doesn't fit on the
          > current line, a previous space is used to break the line.
          > Additionally when overflow option is specified, the last
          > line will be wrapped by either truncating plainly or by
          > adding '...' at the end.

    ==================================================================*/

    const bool is_newline = IS_CODEPOINT_NEWLINE(scope->cp);
    const bool is_space = IS_CODEPOINT_SPACE(scope->cp);
    const bool is_end_of_string = (scope->iter.str == scope->iter.terminator);
    const bool is_terminating_span =
      (scope->i >= scope->run.start && (is_newline || scope->tag_parser.opened_or_closed))
      || (is_end_of_string && (scope->i = scope->utext.byte_len));

    if ((scope->wrap_width && is_space) || is_terminating_span) {
      /* Outta spans! */
      if (label->span_count == label->available_spans) {
        break;
      }

      /* Checks line limit */
      const bool can_continue_on_next_line = (!props->max_lines || scope->line_count < props->max_lines);

      /* Override color to apply, or 0 for default */
      const cig_text_color_ref color_override = scope->color_stack.count > 0
        ? scope->color_stack.colors[scope->color_stack.count-1] 
        : 0;
      
      /* Current font override or 0 for default */
      const cig_font_ref font_override = scope->font_stack.count > 0
        ? scope->font_stack.fonts[scope->font_stack.count-1]
        : 0;
      
      /* Actual font to use for display */
      const cig_font_ref display_font = font_override
        ? font_override
        : label->font;
      
      /* Current string slice length in number of characters */
      const size_t length = scope->i - scope->run.start;

      /* This span ends a line. When splitting a line at a space character, explicit
         newline is added. */
      const size_t newlines = is_newline ? 1 : is_space ? 1 : 0;

      utf8_string slice = length
        ? slice_utf8_string(scope->utext, scope->run.start, length)
        : (utf8_string) { .str = NULL, .byte_len = 0 };

      cig_v bounds = length
        ? measure_callback(slice.str, slice.byte_len, display_font, scope->style)
        : cig_v_zero();

      // if (length) {
      //   printf("slice length: |%.*s| (%d/%d)\n", (int)slice.byte_len, slice.str, scope->i, scope->run.start);
      // } else {
      //   if (is_newline) {
      //     printf("(empty newline)\n");
      //   }
      // }

      /* Width wrapping ON */
      if (scope->wrap_width) {
        /* Text still fits on current line */
        if (scope->line_width + bounds.x <= max_bounds.x) {
          /* Span ends here either by line change or EOS */
          if (is_terminating_span) {
            scope_add_label_span(
              scope,
              label,
              slice,
              font_override,
              color_override,
              props->style | scope->style,
              bounds,
              newlines
            );
          }
          else {
            /* This is run of text that's known to fit in the current bounds.
               We might come back to it when the next word doesn't fit. Until
               then we keep iterating until the end of next word to check again. */
            scope->run.last_fitting.str = (char*)scope->iter.str;
            scope->run.last_fitting.index = scope->i;
            scope->run.last_fitting.slice = slice;
            scope->run.last_fitting.bounds = bounds;
          }
        }
        else {
          /* The next word does not fit and we have a previously collected run of text that *did* fit */
          if (scope->run.last_fitting.str && can_continue_on_next_line) {
            /* Move string iterator back to the end of the last fitting run.
               That's the start of the new span. */
            scope->iter.str = scope->run.last_fitting.str;
            scope->i = scope->run.last_fitting.index;

            /* Create a span based on the last fitting run */
            scope_add_label_span(
              scope,
              label,
              scope->run.last_fitting.slice,
              font_override,
              color_override,
              props->style | scope->style,
              scope->run.last_fitting.bounds,
              1
            );
          }
          /* The next word does not fit, but it's the only word we've checked on this line so far:
             This word will either:

             - Overflow (with horizontal alignment):
                [acknowledg]ment
                ac[knowledgme]nt
                ackn[owledgment]

             - Or be clipped (in 2 modes) if it's on the last line:
                [acknowledg]
                [acknowl...] */
          else {
            /* Continue until the end of the string and let text overflow its bounds */
            if (!is_end_of_string && !can_continue_on_next_line && props->overflow == CIG_TEXT_OVERFLOW) {
              goto iterate_next;
            }

            /* We have an overflow mode set and no more lines available */
            if (props->overflow && !can_continue_on_next_line) {
              /* This will be configured by `wrap_text` function to contain '...' or similar */
              cig_span overflow_marker = { 0 };

              wrap_text(
                &slice,
                length,
                &bounds,
                props->overflow,
                display_font,
                font_override,
                color_override,
                props->style | scope->style,
                max_bounds.x,
                &overflow_marker
              );

              /* Add last span */
              scope_add_label_span(
                scope,
                label,
                slice,
                font_override,
                color_override,
                props->style | scope->style,
                bounds,
                0
              );

              /* Add overflow marker as well */
              if (overflow_marker.str) {
                if (label->span_count < label->available_spans) {
                  label->spans[label->span_count++] = overflow_marker;
                }
              }

              /* Stop parsing string here */
              break;
            }
            else {
              /* Add the only span on this line with overflow */
              scope_add_label_span(
                scope,
                label,
                slice,
                font_override,
                color_override,
                props->style | scope->style,
                bounds,
                newlines
              );
            }
          }
        }
      }

      /* Width wrapping OFF */
      else {
        scope_add_label_span(
          scope,
          label,
          slice,
          font_override,
          color_override,
          props->style | scope->style,
          bounds,
          newlines
        );
      }
    }

    iterate_next:
    scope->i += scope->ch.byte_len;
  }

  label->line_count = M_MAX(1, scope->line_count);
  label->bounds.h = (label->line_count * scope->base_font_info.height) + (label->line_count - 1) * label->line_spacing;
}

static void wrap_text(
  utf8_string *slice,
  size_t text_length,
  cig_v *bounds,
  cig_text_overflow overflow_mode,
  cig_font_ref display_font,
  cig_font_ref font_override,
  cig_text_color_ref color_override,
  cig_text_style style,
  int32_t max_width,
  cig_span *additional_span
) {
  switch (overflow_mode) {
    case CIG_TEXT_SHOW_ELLIPSIS: {
      /* How much of the text overflows? */
      const cig_v ellipsis_size = measure_callback("...", 3, display_font, style);
      const double overflow = bounds->x - (max_width - ellipsis_size.x);
      const double truncation_amount = 1.0 - (overflow / bounds->x);
      /* This is a guesstimate and may not still fit properly */
      int new_length = round((double)text_length * truncation_amount);
      *slice = slice_utf8_string(*slice, 0, new_length);
      *bounds = measure_callback(slice->str, slice->byte_len, display_font, style);
      while (new_length > 0 && bounds->x + ellipsis_size.x > max_width) { /* Shorten even more */
        new_length -= 1;
        *slice = slice_utf8_string(*slice, 0, new_length);
        *bounds = measure_callback(slice->str, slice->byte_len, display_font, style);
      }
      *additional_span = (cig_span) { 
        .str = "...",
        .font_override = font_override,
        .color_override = color_override,
        .bounds = { ellipsis_size.x, ellipsis_size.y },
        .byte_len = 3,
        .style_flags = style,
        .newlines = 0
      };
    } break;

    case CIG_TEXT_TRUNCATE: {
      /* How much of the text overflows? */
      const double overflow = bounds->x - max_width;
      const double truncation_amount = 1.0 - (overflow / bounds->x);
      int new_length = round((double)text_length * truncation_amount);
      *slice = slice_utf8_string(*slice, 0, new_length);
      *bounds = measure_callback(slice->str, slice->byte_len, display_font, style);
      while (new_length > 0 && bounds->x > max_width) { /* Shorten even more */
        new_length -= 1;
        *slice = slice_utf8_string(*slice, 0, new_length);
        *bounds = measure_callback(slice->str, slice->byte_len, display_font, style);
      }
    } break;

    default: break;
  }
}

static void render_spans(
  cig_span *first,
  size_t count,
  cig_font_ref base_font,
  cig_text_color_ref base_color,
  cig_text_horizontal_alignment horizontal_alignment,
  cig_text_vertical_alignment vertical_alignment,
  bounds_t bounds,
  int line_spacing
) {
  if (!count) { return; }

  register const cig_r absolute_rect = cig_r_inset(cig_absolute_rect(), cig_current()->insets);
  register int w, dx, dy;
  register cig_span *span, *line_start, *line_end, *last = first + (count-1);
  register const cig_font_info_st font_info = font_query(base_font);

  static double alignment_constant[3] = { 0, 0.5, 1 };

  dy = absolute_rect.y + (int)((absolute_rect.h - bounds.h) * alignment_constant[vertical_alignment-1]);
  line_start = span = first;
  w = 0;

  while (span <= last) {
    w += span->bounds.w;

    if (span->newlines || span == last) {
      line_end = span;

      if (!span->str) {
#ifdef DEBUG
        cig_trigger_layout_breakpoint(absolute_rect, cig_r_make(absolute_rect.x, dy, absolute_rect.w, font_info.height));
#endif
        goto append_newlines;
      }

      dx = absolute_rect.x + (int)((absolute_rect.w - w) * alignment_constant[horizontal_alignment-1]);

      for (span = line_start; span <= line_end; span++) {
        cig_font_info_st span_font_info = span->font_override ? font_query(span->font_override) : font_info;
        const cig_r span_rect = cig_r_make(
          dx,
          dy + (span->font_override ? ((font_info.height+font_info.baseline_offset)-(span_font_info.height+span_font_info.baseline_offset)) : 0),
          span->bounds.w,
          span->bounds.h
        );

        render_callback(
          span->str,
          span->byte_len,
          span_rect,
          span->font_override ? span->font_override : base_font,
          span->color_override ? span->color_override : base_color,
          span->style_flags
        );

#ifdef DEBUG
        cig_trigger_layout_breakpoint(absolute_rect, span_rect);
#endif

        dx += span->bounds.w;
      }

      append_newlines:
      dy += (line_end->newlines * (font_info.height + line_spacing));
      w = 0;
      span = line_start = line_end+1;

      continue;
    }

    span++;
  }
}

static bool
tag_parser_append(tag_parser_t *this, utf8_char ch, uint32_t cp)
{
  if (!this->open) {
    if (cp == 0x3C) { /* < */
      this->open = true;
      this->terminating = false;
      this->_naming = true;
      strcpy(this->name, "");
      strcpy(this->value, "");
      return (this->opened_or_closed = true);
    }
  } else {
    if (cp == 0x2F) { /* / */
      this->terminating = true;
    }
    else if (cp == 0x3E) { /* > */
      this->open = false;
      return (this->opened_or_closed = true);
    }
    else if (cp == 0x3D && this->_naming) { /* = */
      this->_naming = false; /* Switch to collecting tag value */
    }
    else if (IS_CODEPOINT_SPACE(cp)) {
      /* Ignore spaces */
    }
    else {
      if (this->_naming) {
        snprintf(this->name+strlen(this->name), MAX_TAG_NAME_LEN-strlen(this->name), "%.*s", (int)ch.byte_len, ch.str);
      } else {
        snprintf(this->value+strlen(this->value), MAX_TAG_VALUE_LEN-strlen(this->value), "%.*s", (int)ch.byte_len, ch.str);
      }
    }
  }

  return (this->opened_or_closed = false);
}

static void
scope_apply_tag(scope_st *scope)
{
  const tag_parser_t *tag = &scope->tag_parser;

  if (!strcasecmp(tag->name, "font")) {
    if (tag->terminating) {
      scope->font_stack.count--;
    }
    else {
      scope->font_stack.fonts[scope->font_stack.count++] = (cig_font_ref)strtoull(tag->value, NULL, 16);
    }
  }
  else if (!strcasecmp(tag->name, "color")) {
    if (tag->terminating) {
      scope->color_stack.count--;
    }
    else {
      scope->color_stack.colors[scope->color_stack.count++] = (cig_text_color_ref)strtoull(tag->value, NULL, 16);
    }
  }
  else if (!strcasecmp(tag->name, "b")) {
    if (tag->terminating) {
      scope->style &= ~CIG_TEXT_BOLD;
    }
    else {
      scope->style |= CIG_TEXT_BOLD;
    }
  }
  else if (!strcasecmp(tag->name, "i")) {
    if (tag->terminating) {
      scope->style &= ~CIG_TEXT_ITALIC;
    }
    else {
      scope->style |= CIG_TEXT_ITALIC;
    }
  }
  else if (!strcasecmp(tag->name, "u")) {
    if (tag->terminating) {
      scope->style &= ~CIG_TEXT_UNDERLINE;
    }
    else {
      scope->style |= CIG_TEXT_UNDERLINE;
    }
  }
  else if (!strcasecmp(tag->name, "s") || !strcasecmp(tag->name, "del")) {
    if (tag->terminating) {
      scope->style &= ~CIG_TEXT_STRIKETHROUGH;
    }
    else {
      scope->style |= CIG_TEXT_STRIKETHROUGH;
    }
  } else { /* Log warning? */ }
}

static bool
scope_process_tags(scope_st *scope)
{
  if (tag_parser_append(&scope->tag_parser, scope->ch, scope->cp)) {
    if (!scope->tag_parser.open) { /* Tag closed */
      scope_apply_tag(scope);
    }

    if (!scope->run.reading) {
      scope->i += scope->ch.byte_len;
      return true;
    }
  }
  else if (scope->tag_parser.open) {
    scope->i += scope->ch.byte_len;
    return true;
  }
  else if (!scope->run.reading) {
    scope->run.reading = true;
    scope->run.start = scope->i;
  }

  return false;
}

static cig_span*
scope_add_label_span(
  scope_st *scope,
  cig_label *label,
  utf8_string slice,
  cig_font_ref font_override,
  cig_text_color_ref color_override,
  cig_text_style style,
  cig_v bounds,
  size_t newline_count
) {
  if (label->span_count == label->available_spans) {
    return NULL;
  }

  label->spans[label->span_count++] = (cig_span) { 
    .str = slice.str,
    .font_override = font_override,
    .color_override = color_override,
    .bounds = { bounds.x, bounds.y },
    .byte_len = (unsigned short)slice.byte_len,
    .style_flags = style,
    .newlines = newline_count
  };

  scope->run.last_fitting.str = NULL;
  scope->run.reading = false;
  scope->line_width += bounds.x;
  scope->line_count += newline_count;
  label->bounds.w = M_MAX(label->bounds.w, scope->line_width);
  scope->line_width = 0;

  // printf("\t+ Added span (%d newlines)\n", (int)newline_count);

  return &label->spans[label->span_count-1];
}
