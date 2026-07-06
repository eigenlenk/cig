#include "unity.h"
#include "fixture.h"
#include "cigcore.h"
#include "cigcorem.h"
#include "asserts.h"
#include "allocator.h"
#include <string.h>

TEST_GROUP(core_state);

static cig_context ctx = { 0 };

TEST_SETUP(core_state) {
  cig_init_context(&ctx);

  set_up_test_allocator(&ctx);
}

TEST_TEAR_DOWN(core_state) {}

static void begin() {
  cig_begin_layout(&ctx, NULL, cig_r_make(0, 0, 640, 480), 0.1f);
}

static void end() {
  cig_end_layout();
}

/*  ┌────────────┐
    │ TEST CASES │
    └────────────┘ */

TEST(core_state, visibility) {
  register int i;
  cig_id persistent_id = 0;

  for (i = 0; i < 5; ++i) {
    begin();

    if (i == 2) {
      end();
      continue;
    }

    cig_retain(cig_push_frame(RECT_AUTO));

    if (!persistent_id) { persistent_id = cig_current()->id; }
    else { TEST_ASSERT_EQUAL_UINT32(persistent_id, cig_current()->id); }

    if (i == 0 || i == 3) {
      TEST_ASSERT_EQUAL(CIG_FRAME_APPEARED, cig_visibility());
    }
    else if (i == 1 || i == 4) {
      TEST_ASSERT_EQUAL(CIG_FRAME_VISIBLE, cig_visibility());
    }

    cig_pop_frame();
    end();
  }
}

/*
 * Test maximum number of stateful elements. Trying to allocate memory while
 * exceeding the maximum yields NULL.
 */
TEST(core_state, pool_limit) {
  register int i;
  begin();
  
  for (i = 0; i < CIG_STATES_MAX + 1; ++i) {
    cig_push_frame(RECT_AUTO);
    
    if (i < CIG_STATES_MAX) {
      TEST_ASSERT_NOT_NULL(cig_memory_allocate(sizeof(int)));
    } else {
      TEST_ASSERT_NULL(cig_memory_allocate(sizeof(int)));
    }
    
    cig_pop_frame();
  }
  
  end();
}

TEST(core_state, stale)
{
  int i;
  
  begin();
  /* (Tick 1) Mark all states as used */
  for (i = 0; i < CIG_STATES_MAX; ++i) {
    cig_push_frame(RECT_AUTO);
    TEST_ASSERT_NOT_NULL(cig_memory_allocate(sizeof(int)));
    cig_pop_frame();
  }
  end();

  TEST_ASSERT_EQUAL_INT(CIG_STATES_MAX, alloc_count);

  /*
   * (Tick 2) State is considered stale if it hasn't been used during the last tick,
   * so at this point we don't expect there to be any available states. Memory allocated
   * for these frames will be deallocated.
   */
  begin();
  cig_set_next_id(12345);
  cig_push_frame(RECT_AUTO);
  TEST_ASSERT_NULL(cig_memory_allocate(sizeof(int)));
  cig_pop_frame();
  end();

  /* (Tick 3) Now there should be available states again */
  begin();

  /* All stale states were in fact freed... */
  TEST_ASSERT_EQUAL_INT(CIG_STATES_MAX, free_count);

  cig_set_next_id(12345);
  cig_push_frame(RECT_AUTO);
  /* ...and there are available states for allocation again */
  TEST_ASSERT_NOT_NULL(cig_memory_allocate(sizeof(int)));
  cig_pop_frame();
  end();
}

TEST(core_state, memory_allocation) {
  int i;

  for (i = 0; i < 2; ++i) {
    begin();

    if (i == 0) {
      TEST_ASSERT_NULL(cig_memory_allocation(NULL));
    } else {
      TEST_ASSERT_NOT_NULL(cig_memory_allocation(NULL));
    }

    /* Allocate or return the previously allocated memory */
    cig_v *vec2 = cig_memory_allocate(sizeof(cig_v));

    TEST_ASSERT_NOT_NULL(vec2);

    if (i == 0) { /* Store data on first frame */
      *vec2 = cig_v_make(13, 17);
    } else { /* Read data on the second */
      TEST_ASSERT_EQUAL_VEC2(cig_v_make(13, 17), *vec2);
    }

    end();
  }

  TEST_ASSERT_EQUAL_INT(1, alloc_count);
}

TEST(core_state, memory_free)
{
  begin();

  cig_memory_allocate(1);
  cig_memory_free();

  TEST_ASSERT_EQUAL_INT(1, alloc_count);
  TEST_ASSERT_EQUAL_INT(1, free_count);

  end();
}

TEST(core_state, memory_realloc)
{
  begin();

  cig_memory_allocate(1);
  cig_memory_allocate(2);

  TEST_ASSERT_EQUAL_INT(1, alloc_count);
  TEST_ASSERT_EQUAL_INT(1, realloc_count);

  end();
}

TEST(core_state, tracked_bytes)
{
  begin();

  TEST_ASSERT_EQUAL(0, cig_tracked_bytes());

  cig_memory_allocate(1); /* Alloc */

  TEST_ASSERT_EQUAL(1, cig_tracked_bytes());

  cig_memory_allocate(2); /* Realloc */

  TEST_ASSERT_EQUAL(2, cig_tracked_bytes());

  cig_memory_free();

  TEST_ASSERT_EQUAL(0, cig_tracked_bytes());

  end();
}

TEST_GROUP_RUNNER(core_state) {
  RUN_TEST_CASE(core_state, visibility);
  RUN_TEST_CASE(core_state, pool_limit);
  RUN_TEST_CASE(core_state, stale);
  RUN_TEST_CASE(core_state, memory_allocation);
  RUN_TEST_CASE(core_state, memory_free);
  RUN_TEST_CASE(core_state, memory_realloc);
  RUN_TEST_CASE(core_state, tracked_bytes);
}
