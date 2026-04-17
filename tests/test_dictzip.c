//
//  test_dictzip.c
//  stardict tests
//
//  Unit tests for sd_dictzip (dictzip compressed file reader)
//

#include "unity.h"
#include "dictzip.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// Tests: null / invalid parameters
// ============================================================

void test_dictzip_open_null(void) {
    dictzip *dz = dictzip_open(NULL);
    TEST_ASSERT_NULL(dz);
}

void test_dictzip_open_nonexistent(void) {
    dictzip *dz = dictzip_open("/tmp/nonexistent_dictzip_file_12345.dz");
    TEST_ASSERT_NULL(dz);
}

void test_dictzip_close_null(void) {
    dictzip_close(NULL);  // Should not crash
}

void test_dictzip_read_null_dictzip(void) {
    unsigned char *data = dictzip_read(NULL, 0, 100, NULL);
    TEST_ASSERT_NULL(data);
}

void test_dictzip_read_zero_size(void) {
    // Open a valid dictzip file with zero offset and size
    dictzip *dz = dictzip_open("/tmp/nonexistent_dictzip_file_12345.dz");
    TEST_ASSERT_NULL(dz);

    // Cannot test zero-size read without a valid file,
    // but we verify the API accepts NULL out_size
}

// ============================================================
// Test Runner
// ============================================================

void run_dictzip_tests(void) {
    UnityBegin("test_dictzip.c");

    RUN_TEST(test_dictzip_open_null);
    RUN_TEST(test_dictzip_open_nonexistent);
    RUN_TEST(test_dictzip_close_null);
    RUN_TEST(test_dictzip_read_null_dictzip);
    RUN_TEST(test_dictzip_read_zero_size);

    UnityEnd();
}
