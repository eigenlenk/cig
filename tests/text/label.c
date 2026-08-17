#include "unity.h"
#include "fixture.h"
#include "cigtext.h"
#include "cigcorem.h"
#include "asserts.h"
#include "allocator.h"
#include "utf8.h"

TEST_GROUP(text_label);

static cig_context ctx;
static int text_measure_calls;
static struct {
  cig_r rects[16];
  char strings[16][128];
  size_t render_count;
} spans;

M_INLINED void text_render(
  const char *str,
  size_t len,
  cig_r rect,
  cig_font_ref font,
  cig_text_color_ref color,
  cig_text_style style
) {
  int i = spans.render_count++;
  spans.rects[i] = rect;
  // printf("RENDER: %.*s (%d) IN %d, %d, %d, %d\n", len, str, (uint32_t)len, rect.x, rect.y, rect.w, rect.h);
  sprintf(spans.strings[i], "%.*s", (uint32_t)len, str);
}

M_INLINED cig_v text_measure(
  const char *str,
  size_t len,
  cig_font_ref font,
  cig_text_style style
) {
  utf8_string slice = (utf8_string) { str, len };
  text_measure_calls ++;
  // printf("MEASURE: %.*s = %d\n", len, str, utf8_char_count(slice));
  return cig_v_make(utf8_char_count(slice), 1);
}

M_INLINED cig_font_info_st font_query(cig_font_ref font_ref) {
  return (cig_font_info_st) {
    .height = 1,
    .baseline_offset = 0
  };
}

TEST_SETUP(text_label) {
  cig_init_context(&ctx);

  cig_assign_draw_text(&text_render);
  cig_assign_measure_text(&text_measure);
  cig_assign_query_font(&font_query);

  set_up_test_allocator(&ctx);

  text_measure_calls = 0;
}

TEST_TEAR_DOWN(text_label) {}

static void begin() {
  /*  In the context of these tests we work with a terminal/text-mode where
      bounds and positions are calculated in number of characters rather than pixels */
	cig_begin_layout(&ctx, NULL, cig_r_make(0, 0, 80, 25), 0.1f); /* 80 x 25 character terminal */

  spans.render_count = 0;
}

static void end() {
	cig_end_layout();
}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(text_label, single) {  
  register int i;
  /*  Runing to iterations to test that text is measured only once
      and cached data is used on consecutive layout passes */
  for (i = 0; i < 2; ++i) {
    begin();

    /*  Label centers text both horizontally and vertically by default.
        This label will consist of a single span */
    cig_label *label = cig_draw_label((cig_text_properties) { 0 }, "Olá mundo!");

    /*  Span is an atomic text component, a piece of text that runs until
        the horizontal bounds of the label, or until some property of the
        text changes (font, color, link etc.) */
    TEST_ASSERT_EQUAL(1, spans.render_count);
    TEST_ASSERT_EQUAL(1, label->line_count);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(35, 12, 10, 1), spans.rects[0]);

    end();
  }

  /*  Measure is called after every space, among others, to keep a reference
      until the text no longer fits */
  TEST_ASSERT_EQUAL(2, text_measure_calls);
}

TEST(text_label, single_trailing_newlines)
{  
  begin();

  /* Trailing newline characters increase line count */
  cig_label *label = cig_draw_label((cig_text_properties) { 0 }, "Olá mundo!\n");

  TEST_ASSERT_EQUAL(1, spans.render_count);
  TEST_ASSERT_EQUAL(2, label->line_count);
  TEST_ASSERT_EQUAL_RECT(cig_r_make(35, 11, 10, 1), spans.rects[0]);

  end();
}

TEST(text_label, multiline) {
  register int i;
  for (i = 0; i < 2; ++i) {
    begin();

    cig_label *label = cig_draw_label((cig_text_properties) { 0 }, "Olá mundo!\nHello world!\n\nTere maailm!");

    TEST_ASSERT_EQUAL(3, spans.render_count);
    TEST_ASSERT_EQUAL_INT(4, label->span_count);
    TEST_ASSERT_EQUAL_INT(4, label->line_count);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(35, 10, 10, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(34, 11, 12, 1), spans.rects[1]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(34, 13, 12, 1), spans.rects[2]);

    end();
  }

  TEST_ASSERT_EQUAL(6, text_measure_calls);
}

TEST(text_label, span_limit)
{
  begin();

  /* We allocate 2 spans/lines. Third line in the text is not added to the label */
  cig_label *label = cig_mem_alloc(NULL, CIG_LABEL_SIZEOF(2));
  label->available_spans = 2;

  cig_label_prepare(label, cig_v_make(15, 3), (cig_text_properties) { 0 }, "Olá mundo!\nHello world!\n\nTere maailm!");

  TEST_ASSERT_EQUAL(2, label->span_count);
  TEST_ASSERT_EQUAL_INT(3, label->line_count);

  end();
}

TEST(text_label, horizontal_alignment_left) {
  begin();

  CIG(RECT_AUTO_H(1)) {
    cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 5, 1), spans.rects[0]);
  }
}

TEST(text_label, horizontal_alignment_center) {
  begin();

  CIG(RECT_AUTO_H(1)) {
    cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_CENTER
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(37, 0, 5, 1), spans.rects[0]);
  }
}

TEST(text_label, horizontal_alignment_right) {
  begin();

  CIG(RECT_AUTO_H(1)) {
    cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_RIGHT
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(75, 0, 5, 1), spans.rects[0]);
  }
}

TEST(text_label, vertical_alignment_top) {
  begin();

  CIG(RECT_AUTO_W(5)) {
    cig_draw_label((cig_text_properties) {
      .alignment.vertical = CIG_TEXT_ALIGN_TOP
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 5, 1), spans.rects[0]);
  }
}

TEST(text_label, vertical_alignment_middle) {
  begin();

  CIG(RECT_AUTO_W(5)) {
    cig_draw_label((cig_text_properties) {
      .alignment.vertical = CIG_TEXT_ALIGN_MIDDLE
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 12, 5, 1), spans.rects[0]);
  }
}

TEST(text_label, vertical_alignment_bottom) {
  begin();

  CIG(RECT_AUTO_W(5)) {
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.vertical = CIG_TEXT_ALIGN_BOTTOM
    }, "Label");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 24, 5, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
}

TEST(text_label, forced_line_change) {
  begin();

  CIG(cig_r_make(0, 0, 8, 2)) {
    /*  Left aligned text */
    CIG(_) {
      cig_label *label = cig_draw_label((cig_text_properties) {
        .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
        .alignment.vertical = CIG_TEXT_ALIGN_TOP
      }, "Olá mundo!");

      /*  ╔════════╗  
          ║Olá_____║  
          ║mundo!__║  
          ╚════════╝ */
      TEST_ASSERT_EQUAL_INT(3, text_measure_calls);
      TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 3, 1), spans.rects[0]);
      TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 1, 6, 1), spans.rects[1]);
      TEST_ASSERT_EQUAL_INT(2, label->line_count);
    }

    /*  Centered text */
    CIG(_) {
      cig_label *label = cig_draw_label((cig_text_properties) {
        .alignment.vertical = CIG_TEXT_ALIGN_TOP
      }, "Hello world!");

      /*  ╔════════╗  
          ║_Hello__║  
          ║_world!_║  
          ╚════════╝ */
      TEST_ASSERT_EQUAL_RECT(cig_r_make(1, 0, 5, 1), spans.rects[2]);
      TEST_ASSERT_EQUAL_RECT(cig_r_make(1, 1, 6, 1), spans.rects[3]);
      TEST_ASSERT_EQUAL_INT(2, label->line_count);
    }
    
    /*  Right aligned text */
    CIG(_) {
      cig_label *label = cig_draw_label((cig_text_properties) {
        .alignment.horizontal = CIG_TEXT_ALIGN_RIGHT,
        .alignment.vertical = CIG_TEXT_ALIGN_TOP
      }, "Tere maailm!");

      /*  ╔════════╗  
          ║____Tere║  
          ║_maailm!║  
          ╚════════╝ */
      TEST_ASSERT_EQUAL_RECT(cig_r_make(4, 0, 4, 1), spans.rects[4]);
      TEST_ASSERT_EQUAL_RECT(cig_r_make(1, 1, 7, 1), spans.rects[5]);
      TEST_ASSERT_EQUAL_INT(2, label->line_count);
    }
  }
}

TEST(text_label, prepare_single_long_word) {
  begin();

  cig_label *label = cig_mem_alloc(NULL, CIG_LABEL_SIZEOF(4));
  label->available_spans = 4;
  cig_label_prepare(label, cig_v_make(7, 1), (cig_text_properties) { 0 }, "Foobarbaz");

  TEST_ASSERT_EQUAL_INT(9, label->bounds.w);
  TEST_ASSERT_EQUAL_INT(1, label->bounds.h);
  TEST_ASSERT_EQUAL_INT(1, label->line_count);

  CIG(cig_r_make(0, 0, 7, 1)) {
    cig_label_draw(label);

    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 0, 9, 1), spans.rects[0]);
  }
}

TEST(text_label, prepare_multiple_long_words) {
  begin();

  cig_label *label = cig_mem_alloc(NULL, CIG_LABEL_SIZEOF(4));
  label->available_spans = 4;
  cig_label_prepare(label, cig_v_make(7, 4), (cig_text_properties) { 0 }, "Foobarbaz barbazfoo bazfoobar\n");

  TEST_ASSERT_EQUAL_INT(9, label->bounds.w);
  TEST_ASSERT_EQUAL_INT(4, label->bounds.h);
  TEST_ASSERT_EQUAL_INT(4, label->line_count);

  CIG(cig_r_make(0, 0, 7, 3)) {
    cig_label_draw(label);

    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 0, 9, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 1, 9, 1), spans.rects[1]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 2, 9, 1), spans.rects[2]);
  }
}

TEST(text_label, prepare_multiple_long_words_with_newlines) {
  begin();

  cig_label *label = cig_mem_alloc(NULL, CIG_LABEL_SIZEOF(4));
  label->available_spans = 4;
  cig_label_prepare(label, cig_v_make(7, 4), (cig_text_properties) { 0 }, "Foobarbaz\nbarbazfoo\nbazfoobar");

  TEST_ASSERT_EQUAL_INT(9, label->bounds.w);
  TEST_ASSERT_EQUAL_INT(3, label->bounds.h);
  TEST_ASSERT_EQUAL_INT(3, label->line_count);

  CIG(cig_r_make(0, 0, 7, 3)) {
    cig_label_draw(label);

    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 0, 9, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 1, 9, 1), spans.rects[1]);
    TEST_ASSERT_EQUAL_RECT(cig_r_make(-1, 2, 9, 1), spans.rects[2]);
  }
}


TEST(text_label, prepare_horizontal_wrap_disabled)
{
  begin();

  cig_label *label = cig_mem_alloc(NULL, CIG_LABEL_SIZEOF(10));
  label->available_spans = 10;

  /* Line of text is allowed to go outside the maximum bounds provided.
     Only a newline character or EOS can end the line. */
  cig_label_prepare(
    label,
    cig_v_make(10, 1),
    (cig_text_properties) {
      .flags = CIG_TEXT_HORIZONTAL_WRAP_DISABLED
    },
    "Olá mundo! Hello world! Tere maailm!"
  );

  TEST_ASSERT_EQUAL(1, label->span_count);
  TEST_ASSERT_EQUAL(1, label->line_count);
  TEST_ASSERT_EQUAL(37, label->spans[0].byte_len);

  end();
}

TEST(text_label, overflow_enabled_left_aligned)
{
  begin();
  CIG(RECT(0, 0, 16, 1)) {
    /* Text overflow is enabled by default */
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
      .max_lines = 1,
    }, "This text is going places");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 25, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_STRING("This text is going places", spans.strings[0]);
    TEST_ASSERT_EQUAL_INT(1, label->span_count);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
  end();
}

TEST(text_label, overflow_enabled_center_aligned)
{
  begin();
  CIG(RECT(0, 0, 10, 1)) {
    /* Text overflow is enabled by default */
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_CENTER,
      .max_lines = 1,
    }, "Acknowledgment");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(-2, 0, 14, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_STRING("Acknowledgment", spans.strings[0]);
    TEST_ASSERT_EQUAL_INT(1, label->span_count);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
  end();
}

TEST(text_label, overflow_enabled_right_aligned)
{
  begin();
  CIG(RECT(0, 0, 10, 1)) {
    /* Text overflow is enabled by default */
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_RIGHT,
      .max_lines = 1,
    }, "Acknowledgment");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(-4, 0, 14, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_STRING("Acknowledgment", spans.strings[0]);
    TEST_ASSERT_EQUAL_INT(1, label->span_count);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
  end();
}

TEST(text_label, single_line_overflow_truncate)
{
  begin();
  CIG(RECT(0, 0, 16, 1)) {
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
      .max_lines = 1,
      .overflow = CIG_TEXT_TRUNCATE
    }, "Text becomes truncated");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 16, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_STRING("Text becomes tru", spans.strings[0]);
    TEST_ASSERT_EQUAL_INT(1, label->span_count);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
  end();
}

TEST(text_label, single_line_overflow_ellipsis_ignores_newlines)
{
  begin();
  CIG(RECT(0, 0, 16, 1)) {
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
      .max_lines = 1,
      .overflow = CIG_TEXT_SHOW_ELLIPSIS
    }, "Lorem ipsum dolor sit\n\n");

    TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 13, 1), spans.rects[0]);
    TEST_ASSERT_EQUAL_STRING("Lorem ipsum d", spans.strings[0]);
    TEST_ASSERT_EQUAL_STRING("...", spans.strings[1]);
    TEST_ASSERT_EQUAL_INT(2, label->span_count);
    TEST_ASSERT_EQUAL_INT(1, label->line_count);
  }
  end();
}

TEST(text_label, multiline_overflow_truncate)
{
  begin();
  CIG(RECT(0, 0, 18, 2)) {
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
      .max_lines = 2,
      .overflow = CIG_TEXT_TRUNCATE
    }, "Lorem ipsum dolor sit amet, consectetur.");

    TEST_ASSERT_EQUAL_STRING("Lorem ipsum dolor", spans.strings[0]);
    TEST_ASSERT_EQUAL_STRING("sit amet, consecte", spans.strings[1]);
    TEST_ASSERT_EQUAL_INT(2, label->span_count);
    TEST_ASSERT_EQUAL_INT(2, label->line_count);
  }
  end();
}

TEST(text_label, multiline_overflow_ellipsis)
{
  begin();
  CIG(RECT(0, 0, 18, 2)) {
    cig_label *label = cig_draw_label((cig_text_properties) {
      .alignment.horizontal = CIG_TEXT_ALIGN_LEFT,
      .max_lines = 2,
      .overflow = CIG_TEXT_SHOW_ELLIPSIS
    }, "Lorem ipsum dolor sit amet, consectetur.");

    TEST_ASSERT_EQUAL_STRING("Lorem ipsum dolor", spans.strings[0]);
    TEST_ASSERT_EQUAL_STRING("sit amet, conse", spans.strings[1]);
    TEST_ASSERT_EQUAL_STRING("...", spans.strings[2]);
    TEST_ASSERT_EQUAL_INT(3, label->span_count);
    TEST_ASSERT_EQUAL_INT(2, label->line_count);
  }
  end();
}

TEST(text_label, starts_with_empty_newline)
{
  begin();

  cig_label *label = cig_draw_label((cig_text_properties) { }, "\nSecond line");

  TEST_ASSERT_EQUAL(1, spans.render_count);
  TEST_ASSERT_NULL(label->spans[0].str);
  TEST_ASSERT_EQUAL_STRING("Second line", spans.strings[0]);
  TEST_ASSERT_EQUAL_INT(2, label->span_count);
  TEST_ASSERT_EQUAL_INT(2, label->line_count);
}

/* Raw text is for measuring and drawing smaller strings directly without caching anything internally */
TEST(text_label, raw_text)
{
  begin();

  cig_v bounds = cig_measure_raw_text(NULL, 0, "Olá mundo!");
  TEST_ASSERT_EQUAL_VEC2(cig_v_make(10, 1), bounds);

  /* We have already calculated the text size, so we can just pass it in */
  cig_draw_raw_text(cig_v_make(5, 5), bounds, NULL, 0, NULL, "Olá mundo!");
  TEST_ASSERT_EQUAL_RECT(cig_r_make(5, 5, 10, 1), spans.rects[0]);

  /* But size can also be calculated automatically */
  cig_draw_raw_text(cig_v_make(6, 6), CIG_RAW_TEXT_AUTOMATIC_SIZE, NULL, 0, NULL, "Olá mundo!");
  TEST_ASSERT_EQUAL_RECT(cig_r_make(6, 6, 10, 1), spans.rects[1]);
}

TEST(text_label, raw_text_formatted)
{
  begin();

  cig_v bounds = cig_measure_raw_text_formatted(NULL, 0, "Price of eggs: %.2f$", 130.45000);
  TEST_ASSERT_EQUAL_VEC2(cig_v_make(22, 1), bounds);

  cig_draw_raw_text_formatted(cig_v_make(5, 5), bounds, NULL, 0, NULL, "Price of eggs: %.2f$", 130.45000);
  TEST_ASSERT_EQUAL_STRING("Price of eggs: 130.45$", spans.strings[0]);
  TEST_ASSERT_EQUAL_RECT(cig_r_make(5, 5, 22, 1), spans.rects[0]);
}

TEST_GROUP_RUNNER(text_label)
{
  RUN_TEST_CASE(text_label, single);
  RUN_TEST_CASE(text_label, single_trailing_newlines);
  RUN_TEST_CASE(text_label, multiline);
  RUN_TEST_CASE(text_label, span_limit);
  RUN_TEST_CASE(text_label, horizontal_alignment_left);
  RUN_TEST_CASE(text_label, horizontal_alignment_center);
  RUN_TEST_CASE(text_label, horizontal_alignment_right);
  RUN_TEST_CASE(text_label, vertical_alignment_top);
  RUN_TEST_CASE(text_label, vertical_alignment_middle);
  RUN_TEST_CASE(text_label, vertical_alignment_bottom);
  RUN_TEST_CASE(text_label, forced_line_change);
  RUN_TEST_CASE(text_label, prepare_single_long_word);
  RUN_TEST_CASE(text_label, prepare_multiple_long_words);
  RUN_TEST_CASE(text_label, prepare_multiple_long_words_with_newlines);
  RUN_TEST_CASE(text_label, prepare_horizontal_wrap_disabled);
  RUN_TEST_CASE(text_label, overflow_enabled_left_aligned);
  RUN_TEST_CASE(text_label, overflow_enabled_center_aligned);
  RUN_TEST_CASE(text_label, overflow_enabled_right_aligned);
  RUN_TEST_CASE(text_label, single_line_overflow_truncate);
  RUN_TEST_CASE(text_label, single_line_overflow_ellipsis_ignores_newlines);
  RUN_TEST_CASE(text_label, multiline_overflow_truncate);
  RUN_TEST_CASE(text_label, multiline_overflow_ellipsis);
  RUN_TEST_CASE(text_label, starts_with_empty_newline);
  RUN_TEST_CASE(text_label, raw_text);
  RUN_TEST_CASE(text_label, raw_text_formatted);
}
