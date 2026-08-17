#include "unity.h"
#include "fixture.h"
#include "cigcore.h"
#include "cigcorem.h"
#include "allocator.h"
#include "asserts.h"

TEST_GROUP(core_macros);

static cig_context ctx = { 0 };

TEST_SETUP(core_macros) {
  cig_init_context(&ctx);
  cig_begin_layout(&ctx, NULL, cig_r_make(0, 0, 640, 480), 0.1f);
}

TEST_TEAR_DOWN(core_macros) {
  cig_end_layout();
}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(core_macros, main_macro_and_retaining) {
  register int i;
  cig_frame *out_of_bounds, *main_frame; 

  CIG(_) {}
  /* CIG_LAST() gives us a reference to the last successfully closed element */
  TEST_ASSERT_NOT_NULL(CIG_LAST());

  /*
   * AVERT YOUR EYES - HACK AHEAD
   *
   * While `cig_push_frame` returns a pointer to the newly added frame (or NULL), we
   * can't read it when using the CIG macro because it's essentially a for-loop with
   * no return value. So we need to use another macro to assign the reference through
   * a global.
   */
  CIG_ASSIGN(out_of_bounds, CIG(RECT(9999, 9999, 100, 100)) { })
  TEST_ASSERT_NULL(out_of_bounds);
  TEST_ASSERT_NULL(CIG_LAST());

  /*
   * This creates a 500x400 frame with 4 unit inset and centers it in the root frame.
   * CIG_RETAIN is similar to CIG_ASSIGN but also retains the frame so we can safely
   * reference it even after it's closed.
   */
  CIG_RETAIN(main_frame, CIG(RECT_CENTERED(500, 400), CIG_INSETS(cig_i_uniform(4))) {
    /*  Regular control flow works in here */
    for (i = 0; i < 2; ++i) { }

    CIG(_) { /* _ stands for RECT_AUTO */
      TEST_ASSERT_EQUAL_RECT(cig_r_make(0, 0, 492, 392), cig_rect());
    }
  });

  TEST_ASSERT_NOT_NULL(main_frame);
  TEST_ASSERT_EQUAL_PTR(main_frame, CIG_LAST());
  TEST_ASSERT_EQUAL_RECT(cig_r_make(70, 40, 500, 400), main_frame->rect);
  TEST_ASSERT_BITS_HIGH(RETAINED, main_frame->_flags);
}

TEST(core_macros, vstack) {
  register int i;
  CIG_RETAIN(CIG_VSTACK(
    RECT_AUTO,
    CIG_PARAMS({
      CIG_HEIGHT(200),
      CIG_SPACING(10),
      CIG_LIMIT_VERTICAL(2)
    })
  ) {
    for (i = 0; i < 2; ++i) {
      CIG(_) {
        TEST_ASSERT_EQUAL_RECT(cig_r_make(0, i*(200+10), 640, 200), cig_rect());
      }
    }

    TEST_ASSERT_BITS_HIGH(RETAINED, cig_current()->_flags);
  });
}

TEST(core_macros, hstack) {
  register int i;
  CIG_RETAIN(CIG_HSTACK(
    RECT_AUTO,
    CIG_PARAMS({
      CIG_WIDTH(200),
      CIG_SPACING(10),
      CIG_LIMIT_HORIZONTAL(2)
    })
  ) {
    for (i = 0; i < 2; ++i) {
      CIG(_) {
        TEST_ASSERT_EQUAL_RECT(cig_r_make(i*(200+10), 0, 200, 480), cig_rect());
      }
    }

    TEST_ASSERT_BITS_HIGH(RETAINED, cig_current()->_flags);
  });
}

TEST(core_macros, grid) {
  register int i;
  cig_frame *grid;

  CIG_RETAIN(grid, CIG_GRID(
    RECT_AUTO,
    CIG_PARAMS({
      CIG_COLUMNS(5),
      CIG_ROWS(5)
    })
  ) {
    for (i = 0; i < 25; ++i) {
      int row = (i / 5);
      int column = i - (row * 5);
      CIG(_) {
        TEST_ASSERT_EQUAL_RECT(cig_r_make(128*column, 96*row, 128, 96), cig_rect());
      }
    }
  });

  TEST_ASSERT_NOT_NULL(grid);
  TEST_ASSERT_BITS_HIGH(RETAINED, grid->_flags);
}

TEST(core_macros, pin_basic)
{
  cig_pin pin_top = PIN(TOP);
  TEST_ASSERT_EQUAL(TOP, pin_top.attribute);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, pin_top.value);
  TEST_ASSERT_EQUAL_PTR(NULL, pin_top.relation);
  TEST_ASSERT_EQUAL_INT(0, pin_top.relation_attribute);

  cig_pin pin_inset_bottom = PIN(BOTTOM_INSET_OF(cig_current()), OFFSET_BY(4));
  TEST_ASSERT_EQUAL(BOTTOM | INSET_ATTRIBUTE, pin_inset_bottom.relation_attribute);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, pin_inset_bottom.value);
  TEST_ASSERT_EQUAL_PTR(cig_current(), pin_inset_bottom.relation);
  TEST_ASSERT_EQUAL_INT(0, pin_inset_bottom.attribute);
}

TEST(core_macros, pinning) 
{
  cig_frame *root = cig_current();

  /* CASE 1 ::: Pin right edge to right edge of 'root' offset by 5px (moves left) */
  cig_pin left = PIN(RIGHT, OFFSET_BY(5), RIGHT_OF(root));
  TEST_ASSERT_EQUAL(RIGHT, left.attribute);
  TEST_ASSERT_EQUAL_INT(5, left.value);
  TEST_ASSERT_EQUAL_PTR(root, left.relation);
  TEST_ASSERT_EQUAL(RIGHT, left.relation_attribute);

  /* CASE 2 ::: Pin width to width of 'root' but make it 50% */
  cig_pin width = PIN(WIDTH_OF(root), OFFSET_BY(CIG_REL(0.5)));
  TEST_ASSERT_EQUAL(UNSPECIFIED, width.attribute); /* Will default to WIDTH when building the rectangle */
  TEST_ASSERT_EQUAL_INT(CIG_REL(0.5), width.value);
  TEST_ASSERT_EQUAL_PTR(root, width.relation);
  TEST_ASSERT_EQUAL(WIDTH, width.relation_attribute);

  /* CASE 3 ::: Y/TOP will be defaulted to 0 */
  cig_r rect = BUILD_RECT(left, width, PIN(HEIGHT, 50));

  TEST_ASSERT_EQUAL_RECT(cig_r_make(315, 0, 320, 50), rect);

  /* CASE 4 ::: Aspect ratio */
  cig_r aspect_ratio_rect_0 = BUILD_RECT(
    PIN(CENTER_X_OF(root)),
    PIN(CENTER_Y_OF(root)),
    PIN(WIDTH, 400),
    PIN(ASPECT, 4/3.0)
  );

  TEST_ASSERT_EQUAL_RECT(cig_r_make(120, 90, 400, 300), aspect_ratio_rect_0);

  /* CASE 5 ::: Aspect ratio of another element */
  cig_r aspect_ratio_rect_1 = BUILD_RECT(
    PIN(CENTER_X_OF(root)),
    PIN(CENTER_Y_OF(root)),
    PIN(WIDTH, 200),
    PIN(ASPECT_OF(root))
  );

  TEST_ASSERT_EQUAL_RECT(cig_r_make(220, 165, 200, 150), aspect_ratio_rect_1);

  /* CASE 6 ::: Pinning to inset edges */
  root->insets = cig_i_uniform(10);
  cig_r inset_rect = BUILD_RECT(
    PIN(TOP_INSET_OF(root)),
    PIN(RIGHT_INSET_OF(root)),
    PIN(WIDTH, 200),
    PIN(ASPECT_OF(root))
  );

  TEST_ASSERT_EQUAL_RECT(cig_r_make(420, 0, 200, 150), inset_rect);
}

TEST(core_macros, mem_init)
{
  set_up_test_allocator(&ctx);

  int i, init_count = 0;

  for (i = 0; i < 2; ++i) {
    int *a = CIG_MEM_INIT(sizeof(int)) {
      *a = 100;
      init_count ++;
    }

    if (i == 0) {
      TEST_ASSERT_NULL(cig__macro_ctx.last_read);
      TEST_ASSERT_NOT_NULL(cig__macro_ctx.last_allocation);
    } else {
      TEST_ASSERT_NOT_NULL(cig__macro_ctx.last_read);
    }

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_INT(100, *a);
  }

  TEST_ASSERT_EQUAL_INT(1, init_count);
  TEST_ASSERT_EQUAL_INT(1, alloc_count);
}

TEST_GROUP_RUNNER(core_macros)
{
  RUN_TEST_CASE(core_macros, main_macro_and_retaining);
  RUN_TEST_CASE(core_macros, vstack);
  RUN_TEST_CASE(core_macros, hstack);
  RUN_TEST_CASE(core_macros, grid);
  RUN_TEST_CASE(core_macros, pin_basic);
  RUN_TEST_CASE(core_macros, pinning);
  RUN_TEST_CASE(core_macros, mem_init);
  // TODO: check positional and rect macros
}
