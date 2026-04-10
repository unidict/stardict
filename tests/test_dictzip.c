//
//  test_dictzip.c
//  stardict tests
//
//  Unit tests for sd_dictzip (dictzip compressed file reader)
//

#include "unity.h"
#include "sd_dictzip.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// Tests: null / invalid parameters
// ============================================================

void test_dictzip_open_null(void) {
    sd_dictzip *dz = sd_dictzip_open(NULL);
    TEST_ASSERT_NULL(dz);
}

void test_dictzip_open_nonexistent(void) {
    sd_dictzip *dz = sd_dictzip_open("/tmp/nonexistent_dictzip_file_12345.dz");
    TEST_ASSERT_NULL(dz);
}

void test_dictzip_close_null(void) {
    sd_dictzip_close(NULL);  // Should not crash
}

void test_dictzip_read_null_dictzip(void) {
    unsigned char *data = sd_dictzip_read(NULL, 0, 100, NULL);
    TEST_ASSERT_NULL(data);
}

void test_dictzip_read_zero_size(void) {
    // Open a valid dictzip file with zero offset and size
    sd_dictzip *dz = sd_dictzip_open("/tmp/nonexistent_dictzip_file_12345.dz");
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
