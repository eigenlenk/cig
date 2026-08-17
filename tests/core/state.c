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

/* Test maximum number of stateful elements */
TEST(core_state, pool_limit) {
  int i;
  begin();
  
  for (i = 0; i < CIG_STATES_MAX + 1; ++i) {
    cig_push_frame(RECT_AUTO);

    /**
     * Linking data to element lifetimes requires making the element
     * stateful, so if the pool is empty, it won't work.
     */
    if (i < CIG_STATES_MAX) {
      TEST_ASSERT_NOT_NULL(cig_mem_alloc(NULL, 32));
    } else { /* Max states reached, no persistent state */
      TEST_ASSERT_NULL(cig_mem_alloc(NULL, 32));
    }
    
    cig_pop_frame();
  }
  
  end();
}

TEST(core_state, stale)
{
  int i;
  
  /**
   * Tick 1
   * Consume all internal states by allocating some bytes
   * and associating their lifetime with the element.
   */

  begin();
  for (i = 0; i < CIG_STATES_MAX; ++i) {
    cig_push_frame(RECT_AUTO);
    TEST_ASSERT_NOT_NULL(cig_mem_alloc(NULL, 32));
    TEST_ASSERT_NOT_NULL(cig_current()->_state);
    cig_pop_frame();
  }
  end();

  /**
   * Tick 2
   * Iterate all elements. This keeps the element IDs active and their
   * assocaited states alive even though we don't touch the state in
   * this iteration. The pool of available states is empty, so allocating
   * within a new element should fail.
   */

  begin();
  for (i = 0; i < CIG_STATES_MAX; ++i) {
    cig_push_frame(RECT_AUTO);
    TEST_ASSERT_NOT_NULL(cig_current()->_state);
    cig_pop_frame();
  }

  cig_push_frame(RECT_AUTO);
  TEST_ASSERT_NULL(cig_mem_alloc(NULL, 32));
  cig_pop_frame();

  end();

  /**
   * Tick 3
   * No elements are processed, meaning previously active
   * states are marked as stale and will not be associated
   * automatically on the next iteration.
   */

  free_count = 0;

  begin();
  end();

  /* All internal states were freed and associated memory was released */
  TEST_ASSERT_EQUAL_INT(CIG_STATES_MAX, free_count);

  /**
   * Tick 4
   * None of the elements has previous state assigned automatically,
   * but it can be enabled again manually because the pool of states
   * is full again.
   */

  begin();
  for (i = 0; i < CIG_STATES_MAX; ++i) {
    cig_push_frame(RECT_AUTO);
    TEST_ASSERT_NULL(cig_current()->_state);
    TEST_ASSERT_NOT_NULL(cig_mem_alloc(NULL, 32));
    cig_pop_frame();
  }
  end();
}

/* Allocate and free memory through allocator configured for CIG */
TEST(core_state, memory_allocate)
{
  begin();

  void *a = cig_mem_alloc(NULL, 16);
  void *b = cig_mem_alloc(NULL, 32);
  void *c = cig_mem_alloc(NULL, 64);

  /* Linked list */
  cig_mem *aa = cig_current()->_state->mem;
  cig_mem *ba = cig_current()->_state->mem->next;
  cig_mem *ca = cig_current()->_state->mem->next->next;

  /* `NULL` <-- `a` --> `b` */
  TEST_ASSERT_EQUAL_PTR(a, aa->bytes);
  TEST_ASSERT_NULL(aa->prev);
  TEST_ASSERT_EQUAL_PTR(ba, aa->next);
  TEST_ASSERT_EQUAL_UINT32(16, aa->size);

  /* `a` <-- `b` --> `c` */
  TEST_ASSERT_EQUAL_PTR(b, ba->bytes);
  TEST_ASSERT_EQUAL_PTR(aa, ba->prev);
  TEST_ASSERT_EQUAL_PTR(ca, ba->next);
  TEST_ASSERT_EQUAL_UINT32(32, ba->size);

  /* `b` <-- `c` --> `NULL` */
  TEST_ASSERT_EQUAL_PTR(c, ca->bytes);
  TEST_ASSERT_EQUAL_PTR(ba, ca->prev);
  TEST_ASSERT_NULL(ca->next);
  TEST_ASSERT_EQUAL_UINT32(64, ca->size);

  /* Free `b` --- `a` is now connected with `c` */
  cig_mem_free(b);
  TEST_ASSERT_EQUAL_PTR(aa, ca->prev);
  TEST_ASSERT_EQUAL_PTR(ca, aa->next);

  /* Free `a` --- `c` is now the only remaining allocation, reference stored in state */
  cig_mem_free(a);
  TEST_ASSERT_NULL(ca->prev);
  TEST_ASSERT_EQUAL_PTR(ca, cig_current()->_state->mem);

  cig_mem_free(c);
  TEST_ASSERT_NULL(cig_current()->_state->mem);

  TEST_ASSERT_EQUAL_INT(3, alloc_count);
  TEST_ASSERT_EQUAL_INT(3, free_count);

  end();
}

/* Resizing to 0 bytes = free */
TEST(core_state, memory_implicit_free)
{
  begin();

  void *ptr = cig_mem_alloc(NULL, 32);
  cig_mem_alloc(ptr, 0);

  TEST_ASSERT_EQUAL_INT(1, alloc_count);
  TEST_ASSERT_EQUAL_INT(1, free_count);

  end();
}

/**
 * Reallocating changes the pointer, so `previous` and `previous` links
 * need to be updated.
 */
TEST(core_state, memory_realloc)
{
  begin();

  void *ptr1 = cig_mem_alloc(NULL, 16);
  void *ptr2 = cig_mem_alloc(NULL, 64);

  cig_mem_alloc(ptr1, 32);
  cig_mem_alloc(ptr2, 128);

  cig_mem *ptr2_header = cig_current()->_state->mem->next;

  TEST_ASSERT_NULL(cig_current()->_state->mem->prev);
  TEST_ASSERT_EQUAL_PTR(cig_current()->_state->mem->next, ptr2_header);
  TEST_ASSERT_EQUAL_INT(0xDEADBEEF, cig_current()->_state->mem->id);
  TEST_ASSERT_EQUAL_INT(32, cig_current()->_state->mem->size);

  TEST_ASSERT_EQUAL_PTR(cig_current()->_state->mem, ptr2_header->prev);
  TEST_ASSERT_NULL(ptr2_header->next);
  TEST_ASSERT_EQUAL_INT(0xDEADBEEF, ptr2_header->id);
  TEST_ASSERT_EQUAL_INT(128, ptr2_header->size);

  TEST_ASSERT_EQUAL_INT(2, alloc_count);
  TEST_ASSERT_EQUAL_INT(2, realloc_count);

  end();
}

/**
 * Checks if calling `cig_mem_free` with foreign data (not allocated through `cig_mem_alloc`)
 * is safe. It checks for an internal header and some magic value.
 */
TEST(core_state, memory_free_external_data_safe)
{
  char x = 'x';

  begin();
  /* Safe to call, nothing is freed though */
  cig_mem_free(&x);
  end();
}

TEST(core_state, memory_reads)
{
  begin();

  /* Nothing to read at first */
  TEST_ASSERT_NULL(cig_mem_read(NULL));

  /* Allocate some data */
  void *one = cig_mem_alloc(NULL, 16);
  void *two = cig_mem_alloc(NULL, 32);

  void *first = cig_mem_read(NULL);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_EQUAL_PTR(one, first);

  void *second = cig_mem_read(first);
  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_EQUAL_PTR(two, second);

  void *third = cig_mem_read(second);
  TEST_ASSERT_NULL(third);

  end();
}

TEST(core_state, store_value)
{
  int i;
  char x = 'x';

  for (i = 0; i < 2; ++i) {
    begin();

    if (i == 0) {
      TEST_ASSERT_NULL(cig_value());
      cig_set_value(&x, NULL);
    } else {
      TEST_ASSERT_NOT_NULL(cig_value());
    }

    char y = *(char*)cig_value();

    TEST_ASSERT_EQUAL_CHAR('x', y);

    end();
  }
}

static void *free_fn_arg;
static int free_fn_call_count;

static void
free_value_free(void *ptr)
{
  free_fn_arg = ptr;
  free_fn_call_count ++;
}

TEST(core_state, free_value)
{
  free_fn_arg = NULL;
  free_fn_call_count = 0;
  char x = 'x';

  begin();
  cig_set_value(&x, free_value_free);
  end();

  TEST_ASSERT_NULL(free_fn_arg);

  /**
   * Element we associated data with is not present this iteration.
   * At the end of the layout pass, data will be 'freed'.
   */
  begin();
  end();

  TEST_ASSERT_EQUAL_PTR(&x, free_fn_arg);
  TEST_ASSERT_EQUAL_INT(1, free_fn_call_count);
}

TEST(core_state, free_value_manual)
{
  free_fn_arg = NULL;
  free_fn_call_count = 0;
  char x = 'x';

  begin();
  cig_set_value(&x, free_value_free);
  /* Passing NULL frees any existing value */
  cig_set_value(NULL, NULL);
  cig_set_value(NULL, NULL);
  end();

  TEST_ASSERT_EQUAL_PTR(&x, free_fn_arg);
  TEST_ASSERT_EQUAL_INT(1, free_fn_call_count);
}

TEST(core_state, tracked_bytes)
{
  const size_t header_size = sizeof(cig_mem);

  begin();

  TEST_ASSERT_EQUAL(0, cig_tracked_bytes());

  void *ptr = cig_mem_alloc(NULL, 16); /* Alloc */

  TEST_ASSERT_EQUAL(header_size + 16, cig_tracked_bytes());

  ptr = cig_mem_alloc(ptr, 32); /* Realloc */

  TEST_ASSERT_EQUAL(header_size + 32, cig_tracked_bytes());

  cig_mem_free(ptr);

  TEST_ASSERT_EQUAL(0, cig_tracked_bytes());

  end();
}

TEST_GROUP_RUNNER(core_state) {
  RUN_TEST_CASE(core_state, visibility);
  RUN_TEST_CASE(core_state, pool_limit);
  RUN_TEST_CASE(core_state, stale);
  RUN_TEST_CASE(core_state, memory_allocate);
  RUN_TEST_CASE(core_state, memory_implicit_free);
  RUN_TEST_CASE(core_state, memory_realloc);
  RUN_TEST_CASE(core_state, memory_free_external_data_safe);
  RUN_TEST_CASE(core_state, memory_reads);
  RUN_TEST_CASE(core_state, store_value);
  RUN_TEST_CASE(core_state, free_value);
  RUN_TEST_CASE(core_state, free_value_manual);
  RUN_TEST_CASE(core_state, tracked_bytes);
}
