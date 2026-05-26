//
//  test_dictd.c
//  stardict tests
//
//  Unit tests for sd_dictd (dictd dictionary)
//

#include "unity.h"
#include "sd_dictd.h"
#include "sd_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// Tests: constructor / destructor null safety
// ============================================================

void test_dictd_open_null(void) {
    sd_dictd *dict = NULL;
    sd_status st = sd_dictd_open(NULL, false, &dict);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(dict);
}

void test_dictd_open_nonexistent(void) {
    sd_dictd *dict = NULL;
    sd_status st = sd_dictd_open("/tmp/nonexistent_dictd_file_12345.index", false, &dict);
    TEST_ASSERT_TRUE(st != SD_OK);
    TEST_ASSERT_NULL(dict);
}

void test_dictd_close_null(void) {
    sd_dictd_close(NULL);  // Should not crash
}

// ============================================================
// Tests: query API null safety
// ============================================================

void test_dictd_lookup_null_dict(void) {
    sd_data_entry_array *result = NULL;
    sd_status st = sd_dictd_lookup(NULL, "hello", &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_lookup_null_key(void) {
    sd_data_entry_array *result = NULL;
    sd_status st = sd_dictd_lookup(NULL, NULL, &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_suggest_null_dict(void) {
    sd_index_entry_array *result = NULL;
    sd_status st = sd_dictd_suggest(NULL, "hel", 10, &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_suggest_null_prefix(void) {
    sd_index_entry_array *result = NULL;
    sd_status st = sd_dictd_suggest(NULL, NULL, 10, &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_fetch_null_dict(void) {
    sd_dictfile_index_entry entry;
    memset(&entry, 0, sizeof(entry));
    char *result = NULL;
    sd_status st = sd_dictd_fetch(NULL, &entry, &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_fetch_null_entry(void) {
    char *result = NULL;
    sd_status st = sd_dictd_fetch(NULL, NULL, &result);
    TEST_ASSERT_EQUAL(SD_ERR_INVALID_PARAM, st);
    TEST_ASSERT_NULL(result);
}

void test_dictd_get_index_null(void) {
    const sd_dictfile_index *idx = sd_dictd_get_index(NULL);
    TEST_ASSERT_NULL(idx);
}

// ============================================================
// Test Runner
// ============================================================

void run_dictd_tests(void) {
    UnityBegin("test_dictd.c");

    RUN_TEST(test_dictd_open_null);
    RUN_TEST(test_dictd_open_nonexistent);
    RUN_TEST(test_dictd_close_null);
    RUN_TEST(test_dictd_lookup_null_dict);
    RUN_TEST(test_dictd_lookup_null_key);
    RUN_TEST(test_dictd_suggest_null_dict);
    RUN_TEST(test_dictd_suggest_null_prefix);
    RUN_TEST(test_dictd_fetch_null_dict);
    RUN_TEST(test_dictd_fetch_null_entry);
    RUN_TEST(test_dictd_get_index_null);

    UnityEnd();
}
