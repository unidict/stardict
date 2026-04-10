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
    sd_dictd *dict = sd_dictd_open(NULL);
    TEST_ASSERT_NULL(dict);
}

void test_dictd_open_nonexistent(void) {
    sd_dictd *dict = sd_dictd_open("/tmp/nonexistent_dictd_file_12345.index");
    TEST_ASSERT_NULL(dict);
}

void test_dictd_open_from_paths_null_index(void) {
    sd_dictd *dict = sd_dictd_open_from_paths(NULL, "/tmp/a.dict");
    TEST_ASSERT_NULL(dict);
}

void test_dictd_open_from_paths_null_dict(void) {
    sd_dictd *dict = sd_dictd_open_from_paths("/tmp/a.index", NULL);
    TEST_ASSERT_NULL(dict);
}

void test_dictd_close_null(void) {
    sd_dictd_close(NULL);  // Should not crash
}

// ============================================================
// Tests: query API null safety
// ============================================================

void test_dictd_lookup_null_dict(void) {
    sd_lookup_result *result = sd_dictd_lookup(NULL, "hello");
    TEST_ASSERT_NULL(result);
}

void test_dictd_lookup_null_key(void) {
    sd_lookup_result *result = sd_dictd_lookup(NULL, NULL);
    TEST_ASSERT_NULL(result);
}

void test_dictd_suggest_null_dict(void) {
    sd_suggestion_result *result = sd_dictd_suggest(NULL, "hel", 10);
    TEST_ASSERT_NULL(result);
}

void test_dictd_suggest_null_prefix(void) {
    sd_suggestion_result *result = sd_dictd_suggest(NULL, NULL, 10);
    TEST_ASSERT_NULL(result);
}

void test_dictd_lookup_by_index_null_dict(void) {
    sd_dictfile_index_entry entry;
    memset(&entry, 0, sizeof(entry));
    char *result = sd_dictd_lookup_by_index(NULL, &entry);
    TEST_ASSERT_NULL(result);
}

void test_dictd_lookup_by_index_null_entry(void) {
    char *result = sd_dictd_lookup_by_index(NULL, NULL);
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
    RUN_TEST(test_dictd_open_from_paths_null_index);
    RUN_TEST(test_dictd_open_from_paths_null_dict);
    RUN_TEST(test_dictd_close_null);
    RUN_TEST(test_dictd_lookup_null_dict);
    RUN_TEST(test_dictd_lookup_null_key);
    RUN_TEST(test_dictd_suggest_null_dict);
    RUN_TEST(test_dictd_suggest_null_prefix);
    RUN_TEST(test_dictd_lookup_by_index_null_dict);
    RUN_TEST(test_dictd_lookup_by_index_null_entry);
    RUN_TEST(test_dictd_get_index_null);

    UnityEnd();
}
