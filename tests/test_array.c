//
//  test_array.c
//  stardict tests
//
//  Unit tests for sd_array (dynamic array)
//

#include "unity.h"
#include "sd_array.h"
#include <string.h>

// ============================================================
// Tests: basic operations
// ============================================================

void test_array_push_back_int(void) {
    sd_array(int) arr = NULL;

    for (int i = 0; i < 10; i++) {
        sd_array_push_back(arr, i);
    }

    TEST_ASSERT_EQUAL_size_t(10, sd_array_size(arr));
    TEST_ASSERT_TRUE(sd_array_size(arr) <= sd_array_capacity(arr));

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(i, arr[i]);
    }

    sd_array_free(arr);
}

void test_array_push_back_struct(void) {
    typedef struct { int x; int y; } Point;
    sd_array(Point) arr = NULL;

    Point p1 = {1, 2};
    Point p2 = {3, 4};
    sd_array_push_back(arr, p1);
    sd_array_push_back(arr, p2);

    TEST_ASSERT_EQUAL_size_t(2, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(1, arr[0].x);
    TEST_ASSERT_EQUAL_INT(2, arr[0].y);
    TEST_ASSERT_EQUAL_INT(3, arr[1].x);
    TEST_ASSERT_EQUAL_INT(4, arr[1].y);

    sd_array_free(arr);
}

void test_array_size_null(void) {
    sd_array(int) arr = NULL;
    TEST_ASSERT_EQUAL_size_t(0, sd_array_size(arr));
    TEST_ASSERT_EQUAL_size_t(0, sd_array_capacity(arr));
}

// ============================================================
// Tests: insert
// ============================================================

void test_array_insert_beginning(void) {
    sd_array(int) arr = NULL;
    sd_array_push_back(arr, 2);
    sd_array_push_back(arr, 3);

    sd_array_insert(arr, 0, 1);

    TEST_ASSERT_EQUAL_size_t(3, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(1, arr[0]);
    TEST_ASSERT_EQUAL_INT(2, arr[1]);
    TEST_ASSERT_EQUAL_INT(3, arr[2]);

    sd_array_free(arr);
}

void test_array_insert_middle(void) {
    sd_array(int) arr = NULL;
    sd_array_push_back(arr, 1);
    sd_array_push_back(arr, 3);

    sd_array_insert(arr, 1, 2);

    TEST_ASSERT_EQUAL_size_t(3, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(1, arr[0]);
    TEST_ASSERT_EQUAL_INT(2, arr[1]);
    TEST_ASSERT_EQUAL_INT(3, arr[2]);

    sd_array_free(arr);
}

void test_array_insert_end(void) {
    sd_array(int) arr = NULL;
    sd_array_push_back(arr, 1);
    sd_array_push_back(arr, 2);

    sd_array_insert(arr, 2, 3);

    TEST_ASSERT_EQUAL_size_t(3, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(1, arr[0]);
    TEST_ASSERT_EQUAL_INT(2, arr[1]);
    TEST_ASSERT_EQUAL_INT(3, arr[2]);

    sd_array_free(arr);
}

// ============================================================
// Tests: pop_back
// ============================================================

void test_array_pop_back(void) {
    sd_array(int) arr = NULL;
    sd_array_push_back(arr, 1);
    sd_array_push_back(arr, 2);
    sd_array_push_back(arr, 3);

    sd_array_pop_back(arr);
    TEST_ASSERT_EQUAL_size_t(2, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(2, arr[1]);

    sd_array_pop_back(arr);
    TEST_ASSERT_EQUAL_size_t(1, sd_array_size(arr));

    sd_array_pop_back(arr);
    TEST_ASSERT_EQUAL_size_t(0, sd_array_size(arr));

    // Pop on empty should be safe
    sd_array_pop_back(arr);
    TEST_ASSERT_EQUAL_size_t(0, sd_array_size(arr));

    sd_array_free(arr);
}

// ============================================================
// Tests: reserve
// ============================================================

void test_array_reserve(void) {
    sd_array(int) arr = NULL;
    sd_array_reserve(arr, 100);
    TEST_ASSERT_TRUE(sd_array_capacity(arr) >= 100);
    TEST_ASSERT_EQUAL_size_t(0, sd_array_size(arr));

    // Push should not reallocate
    sd_array_push_back(arr, 42);
    TEST_ASSERT_EQUAL_INT(42, arr[0]);

    sd_array_free(arr);
}

// ============================================================
// Tests: growth
// ============================================================

void test_array_growth_doubling(void) {
    sd_array(int) arr = NULL;

    // Push 1000 elements, verify no crash and correct size
    for (int i = 0; i < 1000; i++) {
        sd_array_push_back(arr, i);
    }

    TEST_ASSERT_EQUAL_size_t(1000, sd_array_size(arr));
    TEST_ASSERT_EQUAL_INT(0, arr[0]);
    TEST_ASSERT_EQUAL_INT(999, arr[999]);

    sd_array_free(arr);
}

// ============================================================
// Tests: free null
// ============================================================

void test_array_free_null(void) {
    sd_array(int) arr = NULL;
    sd_array_free(arr);  // Should be safe, no crash
    TEST_ASSERT_NULL(arr);
}

// ============================================================
// Test Runner
// ============================================================

void run_array_tests(void) {
    UnityBegin("test_array.c");

    RUN_TEST(test_array_push_back_int);
    RUN_TEST(test_array_push_back_struct);
    RUN_TEST(test_array_size_null);
    RUN_TEST(test_array_insert_beginning);
    RUN_TEST(test_array_insert_middle);
    RUN_TEST(test_array_insert_end);
    RUN_TEST(test_array_pop_back);
    RUN_TEST(test_array_reserve);
    RUN_TEST(test_array_growth_doubling);
    RUN_TEST(test_array_free_null);

    UnityEnd();
}
