//
//  test_stardict.c
//  stardict tests
//
//  Unit tests for sd_stardict (StarDict dictionary)
//

#include "unity.h"
#include "sd_stardict.h"
#include "sd_dictfile_index.h"
#include "sd_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// Test data path helper (same pattern as bgl test_reader.c)
// Derive path from __FILE__ so tests work on any platform
// ============================================================

static char s_test_ifo_path[1024] = {0};

static const char *get_test_ifo_path(void) {
    if (s_test_ifo_path[0] == '\0') {
        const char *file = __FILE__;
        const char *last_slash = NULL;
        const char *p;
        for (p = file; *p; p++) {
            if (*p == '/' || *p == '\\') last_slash = p;
        }
        if (last_slash) {
            int dir_len = (int)(last_slash - file);
            snprintf(s_test_ifo_path, sizeof(s_test_ifo_path),
                     "%.*s/data/stardict-xiandaiyinghan-2.4.2/xiandaiyinghan.ifo",
                     dir_len, file);
        }
    }
    return s_test_ifo_path;
}

#define TEST_IFO_PATH get_test_ifo_path()

// ============================================================
// Tests: constructor / destructor null safety
// ============================================================

void test_stardict_open_null(void) {
    sd_stardict *dict = sd_stardict_open(NULL);
    TEST_ASSERT_NULL(dict);
}

void test_stardict_open_nonexistent(void) {
    sd_stardict *dict = sd_stardict_open("/tmp/nonexistent_stardict_file_12345.ifo");
    TEST_ASSERT_NULL(dict);
}

void test_stardict_open_from_paths_null_ifo(void) {
    sd_stardict *dict = sd_stardict_open_from_paths(NULL, "/tmp/a.idx", "/tmp/a.dict", NULL);
    TEST_ASSERT_NULL(dict);
}

void test_stardict_open_from_paths_null_idx(void) {
    sd_stardict *dict = sd_stardict_open_from_paths("/tmp/a.ifo", NULL, "/tmp/a.dict", NULL);
    TEST_ASSERT_NULL(dict);
}

void test_stardict_open_from_paths_null_dict(void) {
    sd_stardict *dict = sd_stardict_open_from_paths("/tmp/a.ifo", "/tmp/a.idx", NULL, NULL);
    TEST_ASSERT_NULL(dict);
}

void test_stardict_close_null(void) {
    sd_stardict_close(NULL);  // Should not crash
}

// ============================================================
// Tests: query API null safety
// ============================================================

void test_stardict_get_info_null(void) {
    const sd_stardict_ifo *info = stardict_get_info(NULL);
    TEST_ASSERT_NULL(info);
}

void test_stardict_lookup_null_dict(void) {
    sd_data_entry_array *result = stardict_lookup(NULL, "hello");
    TEST_ASSERT_NULL(result);
}

void test_stardict_lookup_null_key(void) {
    sd_data_entry_array *result = stardict_lookup(NULL, NULL);
    TEST_ASSERT_NULL(result);
}

void test_stardict_suggest_null_dict(void) {
    sd_index_entry_array *result = stardict_suggest(NULL, "hel", 10);
    TEST_ASSERT_NULL(result);
}

void test_stardict_suggest_null_prefix(void) {
    sd_index_entry_array *result = stardict_suggest(NULL, NULL, 10);
    TEST_ASSERT_NULL(result);
}

void test_stardict_lookup_by_index_null_dict(void) {
    sd_dictfile_index_entry entry;
    memset(&entry, 0, sizeof(entry));
    char *result = stardict_lookup_by_index(NULL, &entry);
    TEST_ASSERT_NULL(result);
}

void test_stardict_lookup_by_index_null_entry(void) {
    char *result = stardict_lookup_by_index(NULL, NULL);
    TEST_ASSERT_NULL(result);
}

void test_stardict_get_index_null(void) {
    const sd_dictfile_index *idx = stardict_get_index(NULL);
    TEST_ASSERT_NULL(idx);
}

// ============================================================
// Tests: result free null safety
// ============================================================

void test_lookup_result_free_null(void) {
    sd_data_entry_array_free(NULL);  // Should not crash
}

void test_suggestion_result_free_null(void) {
    sd_index_entry_array_free(NULL);  // Should not crash
}

// ============================================================
// Tests: open valid dictionary
// ============================================================

void test_stardict_open_valid(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);
    sd_stardict_close(dict);
}

// ============================================================
// Tests: dictionary info
// ============================================================

void test_stardict_get_info_bookname(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_stardict_ifo *ifo = stardict_get_info(dict);
    TEST_ASSERT_NOT_NULL(ifo);
    TEST_ASSERT_NOT_NULL(ifo->bookname);
    TEST_ASSERT_EQUAL_STRING("现代英汉词典", ifo->bookname);

    sd_stardict_close(dict);
}

void test_stardict_get_info_wordcount(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_stardict_ifo *ifo = stardict_get_info(dict);
    TEST_ASSERT_NOT_NULL(ifo);
    TEST_ASSERT_EQUAL_UINT32(40305, ifo->wordcount);

    sd_stardict_close(dict);
}

void test_stardict_get_info_author(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_stardict_ifo *ifo = stardict_get_info(dict);
    TEST_ASSERT_NOT_NULL(ifo);
    TEST_ASSERT_NOT_NULL(ifo->author);
    TEST_ASSERT_EQUAL_STRING("xiaobenwei", ifo->author);

    sd_stardict_close(dict);
}

void test_stardict_get_info_sametypesequence(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_stardict_ifo *ifo = stardict_get_info(dict);
    TEST_ASSERT_NOT_NULL(ifo);
    TEST_ASSERT_NOT_NULL(ifo->sametypesequence);
    TEST_ASSERT_EQUAL_STRING("h", ifo->sametypesequence);

    sd_stardict_close(dict);
}

// ============================================================
// Tests: index
// ============================================================

void test_stardict_get_index_count(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_dictfile_index *idx = stardict_get_index(dict);
    TEST_ASSERT_NOT_NULL(idx);
    TEST_ASSERT_EQUAL_UINT32(40305, sd_dictfile_index_get_count(idx));

    sd_stardict_close(dict);
}

void test_stardict_get_index_first_entry(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    const sd_dictfile_index *idx = stardict_get_index(dict);
    TEST_ASSERT_NOT_NULL(idx);

    /* get_entry modifies page cache internally, so const must be cast away */
    const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(
        (sd_dictfile_index *)idx, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_STRING("'d", entry->word);

    sd_stardict_close(dict);
}

// ============================================================
// Tests: lookup
// ============================================================

void test_stardict_lookup_hello(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    sd_data_entry_array *result = stardict_lookup(dict, "hello");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_size_t(1, result->count);
    TEST_ASSERT_NOT_NULL(result->items[0].word);
    TEST_ASSERT_EQUAL_STRING("hello", result->items[0].word);
    TEST_ASSERT_NOT_NULL(result->items[0].definition);
    TEST_ASSERT_NOT_NULL(strstr(result->items[0].definition, "Hello"));

    sd_data_entry_array_free(result);
    sd_stardict_close(dict);
}

void test_stardict_lookup_not_found(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    sd_data_entry_array *result = stardict_lookup(dict, "xyznonexistent12345");
    TEST_ASSERT_NULL(result);

    sd_stardict_close(dict);
}

void test_stardict_lookup_a(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    sd_data_entry_array *result = stardict_lookup(dict, "a");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result->count >= 1);
    TEST_ASSERT_NOT_NULL(result->items[0].definition);

    sd_data_entry_array_free(result);
    sd_stardict_close(dict);
}

// ============================================================
// Tests: suggest
// ============================================================

void test_stardict_suggest_h_prefix(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    sd_index_entry_array *result = stardict_suggest(dict, "h", 5);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_size_t(5, result->count);
    TEST_ASSERT_EQUAL_STRING("H", result->items[0]->word);
    TEST_ASSERT_EQUAL_STRING("h", result->items[1]->word);
    TEST_ASSERT_EQUAL_STRING("H-bomb", result->items[2]->word);
    TEST_ASSERT_EQUAL_STRING("H.C.F.", result->items[3]->word);
    TEST_ASSERT_EQUAL_STRING("h.p.", result->items[4]->word);

    sd_index_entry_array_free(result);
    sd_stardict_close(dict);
}

void test_stardict_suggest_not_found(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    sd_index_entry_array *result = stardict_suggest(dict, "zzzzzzz", 10);
    TEST_ASSERT_NULL(result);

    sd_stardict_close(dict);
}

// ============================================================
// Tests: lookup by index
// ============================================================

void test_stardict_lookup_by_index(void) {
    sd_stardict *dict = sd_stardict_open(TEST_IFO_PATH);
    TEST_ASSERT_NOT_NULL(dict);

    // First get an entry via suggest
    sd_index_entry_array *sg = stardict_suggest(dict, "hello", 1);
    TEST_ASSERT_NOT_NULL(sg);
    TEST_ASSERT_EQUAL_size_t(1, sg->count);

    // Then use lookup_by_index for fast definition retrieval
    char *def = stardict_lookup_by_index(dict, sg->items[0]);
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_NOT_NULL(strstr(def, "Hello"));
    free(def);

    sd_index_entry_array_free(sg);
    sd_stardict_close(dict);
}

// ============================================================
// Test Runner
// ============================================================

void run_stardict_tests(void) {
    UnityBegin("test_stardict.c");

    // Null safety
    RUN_TEST(test_stardict_open_null);
    RUN_TEST(test_stardict_open_nonexistent);
    RUN_TEST(test_stardict_open_from_paths_null_ifo);
    RUN_TEST(test_stardict_open_from_paths_null_idx);
    RUN_TEST(test_stardict_open_from_paths_null_dict);
    RUN_TEST(test_stardict_close_null);
    RUN_TEST(test_stardict_get_info_null);
    RUN_TEST(test_stardict_lookup_null_dict);
    RUN_TEST(test_stardict_lookup_null_key);
    RUN_TEST(test_stardict_suggest_null_dict);
    RUN_TEST(test_stardict_suggest_null_prefix);
    RUN_TEST(test_stardict_lookup_by_index_null_dict);
    RUN_TEST(test_stardict_lookup_by_index_null_entry);
    RUN_TEST(test_stardict_get_index_null);
    RUN_TEST(test_lookup_result_free_null);
    RUN_TEST(test_suggestion_result_free_null);

    // Valid dictionary tests
    RUN_TEST(test_stardict_open_valid);
    RUN_TEST(test_stardict_get_info_bookname);
    RUN_TEST(test_stardict_get_info_wordcount);
    RUN_TEST(test_stardict_get_info_author);
    RUN_TEST(test_stardict_get_info_sametypesequence);
    RUN_TEST(test_stardict_get_index_count);
    RUN_TEST(test_stardict_get_index_first_entry);
    RUN_TEST(test_stardict_lookup_hello);
    RUN_TEST(test_stardict_lookup_not_found);
    RUN_TEST(test_stardict_lookup_a);
    RUN_TEST(test_stardict_suggest_h_prefix);
    RUN_TEST(test_stardict_suggest_not_found);
    RUN_TEST(test_stardict_lookup_by_index);

    UnityEnd();
}
