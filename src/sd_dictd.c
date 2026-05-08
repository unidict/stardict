//
//  sd_dictd.c
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#include "sd_dictd.h"
#include "sd_dictfile_index.h"
#include "sd_dictfile.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "sd_port.h"

// ============================================================
// dictd structure definition
// ============================================================

struct sd_dictd {
    char *index_path;          // .index file path
    char *dict_path;           // .dict or .dict.dz file path
    sd_dictfile_index *index;  // .index index file handler
    sd_dictfile *dict;         // .dict data reader
};

// ============================================================
// dictd index parsing functions
// ============================================================

/**
 * Base64 decode (dictd format)
 * dictd uses a MIME-like base64 encoding
 * Character set: A-Z, a-z, 0-9, +, /
 */
static uint32_t dictd_decode_base64(const char *str, size_t len) {
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    uint32_t result = 0;
    for (size_t i = 0; i < len; i++) {
        const char *p = strchr(digits, str[i]);
        if (!p) {
            return 0;  // Invalid character
        }
        int value = (int)(p - digits);
        result = result * 64 + value;
    }
    return result;
}

/**
 * Parse dictd index entry
 * Format: word\toffset_base64\tsize_base64\n
 * @return Bytes consumed by entry on success, 0 on failure
 */
static size_t dictd_parse_entry(const char *data, size_t data_size,
                                sd_dictfile_index_entry *entry) {
    const char *p = data;
    const char *end = data + data_size;

    // Find first tab (end of word)
    const char *tab1 = memchr(p, '\t', end - p);
    if (!tab1) {
        return 0;  // Format error
    }

    // Extract word
    size_t word_len = tab1 - p;
    entry->word = strndup(p, word_len);
    if (!entry->word) {
        return 0;
    }
    p = tab1 + 1;

    // Find second tab (end of offset)
    const char *tab2 = memchr(p, '\t', end - p);
    if (!tab2) {
        free(entry->word);
        entry->word = NULL;
        return 0;
    }

    // Parse offset (base64 encoded)
    size_t offset_len = tab2 - p;
    entry->offset = dictd_decode_base64(p, offset_len);
    p = tab2 + 1;

    // Find newline (end of size)
    const char *newline = memchr(p, '\n', end - p);
    if (!newline) {
        // If no newline, use remaining data
        newline = end;
    }

    // Parse size (base64 encoded)
    size_t size_len = newline - p;
    entry->size = dictd_decode_base64(p, size_len);

    // Return bytes consumed by this entry
    return (newline - data) + 1;
}

// ============================================================
// sd_dictd cleanup function (internal)
// ============================================================
static void sd_dictd_cleanup(sd_dictd *dictd) {
    if (!dictd) return;

    if (dictd->dict) {
        sd_dictfile_close(dictd->dict);
        dictd->dict = NULL;
    }

    if (dictd->index) {
        sd_dictfile_index_close(dictd->index);
        dictd->index = NULL;
    }

    if (dictd->index_path) {
        free(dictd->index_path);
        dictd->index_path = NULL;
    }

    if (dictd->dict_path) {
        free(dictd->dict_path);
        dictd->dict_path = NULL;
    }
}

// ============================================================
// Public API implementation
// ============================================================

/**
 * Look up all matching entries
 */
sd_lookup_result *sd_dictd_lookup(sd_dictd *dictd, const char *key) {
    if (!dictd || !key) {
        return NULL;
    }

    // Search entries in index
    sd_array(sd_dictfile_index_entry) entries = sd_dictfile_index_lookup(dictd->index, key,
                                                     false, 0);

    if (!entries || sd_array_size(entries) == 0) {
        return NULL;  // Not found
    }

    // Build result array
    size_t count = sd_array_size(entries);
    sd_lookup_result *results = malloc(sizeof(sd_lookup_result));
    if (!results) {
        sd_dictfile_index_free_entries(entries);
        return NULL;
    }

    results->count = count;
    results->definitions = calloc(count, sizeof(char *));
    if (!results->definitions) {
        free(results);
        sd_dictfile_index_free_entries(entries);
        return NULL;
    }

    // Read data for each match
    for (size_t i = 0; i < count; i++) {
        sd_dictfile_data_block *block = sd_dictfile_read(dictd->dict,
                                           entries[i].offset,
                                           entries[i].size);
        if (!block) {
            results->definitions[i] = NULL;
            continue;
        }

        // dictd data is usually plain text
        results->definitions[i] = strndup(block->data, block->size);
    }

    sd_dictfile_index_free_entries(entries);
    return results;
}

/**
 * Get word suggestions by prefix
 */
sd_suggestion_result *sd_dictd_suggest(sd_dictd *dictd, const char *prefix, size_t max_results) {
    if (!dictd || !prefix) {
        return NULL;
    }

    // Prefix match (returns cvector)
    sd_array(sd_dictfile_index_entry) entries = sd_dictfile_index_lookup(dictd->index, prefix,
                                                     true,
                                                     (uint32_t)max_results);

    if (!entries || sd_array_size(entries) == 0) {
        return NULL;  // Not found
    }

    size_t count = sd_array_size(entries);

    // Build suggestion list
    sd_suggestion_result *suggestions = malloc(sizeof(sd_suggestion_result));
    if (!suggestions) {
        sd_dictfile_index_free_entries(entries);
        return NULL;
    }

    suggestions->count = count;
    suggestions->entries = calloc(count, sizeof(sd_dictfile_index_entry *));
    if (!suggestions->entries) {
        free(suggestions);
        sd_dictfile_index_free_entries(entries);
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        // Create word_info (store offset/size for fast subsequent lookup)
        sd_dictfile_index_entry *info = calloc(1, sizeof(sd_dictfile_index_entry));
        if (info) {
            info->word = entries[i].word;  // Transfer ownership
            info->offset = entries[i].offset;
            info->size = entries[i].size;

            suggestions->entries[i] = info;

            // Clear original entry's word (prevent double free)
            entries[i].word = NULL;
        }
    }

    // Free entries structure (word transferred to word_info)
    sd_dictfile_index_free_entries(entries);

    return suggestions;
}

/**
 * Fast lookup definition using index entry
 */
char *sd_dictd_lookup_by_index(sd_dictd *dictd, const sd_dictfile_index_entry *entry) {
    if (!dictd || !entry) {
        return NULL;
    }

    // Read data directly using offset and size from entry
    sd_dictfile_data_block *block = sd_dictfile_read(dictd->dict,
                                       entry->offset,
                                       entry->size);
    if (!block) {
        return NULL;
    }

    // Return data directly
    char *result = strndup(block->data, block->size);

    if (!result) {
        return NULL;
    }

    return result;
}

const sd_dictfile_index *sd_dictd_get_index(sd_dictd *dictd) {
    if (!dictd) {
        return NULL;
    }
    return dictd->index;
}

const sd_dictd_paths *sd_dictd_get_paths(sd_dictd *dictd) {
    if (!dictd) return NULL;
    static sd_dictd_paths paths;
    paths.index_path = dictd->index_path;
    paths.dict_path = dictd->dict_path;
    return &paths;
}

// ============================================================
// Constructor (public API)
// ============================================================

/**
 * Open dictd dictionary file (specify all file paths)
 */
sd_dictd *sd_dictd_open_from_paths(const char *index_path,
                                  const char *dict_path) {
    if (!index_path || !dict_path) {
        fprintf(stderr, "Error: Invalid parameters for sd_dictd_open_from_paths\n");
        return NULL;
    }

    // Verify files exist
    SD_STAT_STRUCT st;
    if (sd_stat(index_path, &st) != 0 || !SD_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: .index file not found: %s\n", index_path);
        return NULL;
    }

    if (sd_stat(dict_path, &st) != 0 || !SD_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: .dict file not found: %s\n", dict_path);
        return NULL;
    }

    // Allocate sd_dictd object
    sd_dictd *dictd = calloc(1, sizeof(sd_dictd));
    if (!dictd) {
        fprintf(stderr, "Error: Memory allocation failed for sd_dictd\n");
        return NULL;
    }

    // Copy paths
    dictd->index_path = strdup(index_path);
    dictd->dict_path = strdup(dict_path);

    if (!dictd->index_path || !dictd->dict_path) {
        fprintf(stderr, "Error: Memory allocation failed for file paths\n");
        sd_dictd_cleanup(dictd);
        free(dictd);
        return NULL;
    }

    // Load .index index file
    dictd->index = sd_dictfile_index_open(index_path, dictd_parse_entry, 0, 0);
    if (!dictd->index) {
        fprintf(stderr, "Error: Failed to load .index file\n");
        sd_dictd_cleanup(dictd);
        free(dictd);
        return NULL;
    }

    // Open .dict data file
    dictd->dict = sd_dictfile_open(dict_path, NULL);
    if (!dictd->dict) {
        fprintf(stderr, "Error: Failed to open .dict file\n");
        sd_dictd_cleanup(dictd);
        free(dictd);
        return NULL;
    }

    return dictd;
}

/**
 * Open dictd dictionary file (auto-discover related files)
 */
sd_dictd *sd_dictd_open(const char *index_path) {
    if (!index_path) {
        fprintf(stderr, "Error: index_path is NULL\n");
        return NULL;
    }

    // Construct .dict file path from base name (before last '.')
    const char *dot = strrchr(index_path, '.');
    size_t base_len = dot ? (size_t)(dot - index_path) : strlen(index_path);

    char *dict_path = malloc(base_len + 6);     // base + ".dict" + null
    char *dict_dz_path = malloc(base_len + 9);  // base + ".dict.dz" + null

    if (!dict_path || !dict_dz_path) {
        fprintf(stderr, "Error: Memory allocation failed for file paths\n");
        free(dict_path);
        free(dict_dz_path);
        return NULL;
    }

    memcpy(dict_path, index_path, base_len);
    strcpy(dict_path + base_len, ".dict");

    memcpy(dict_dz_path, index_path, base_len);
    strcpy(dict_dz_path + base_len, ".dict.dz");

    // Check for .dict or .dict.dz file
    const char *actual_dict_path = NULL;
    SD_STAT_STRUCT st;
    if (sd_stat(dict_path, &st) == 0 && SD_ISREG(st.st_mode)) {
        actual_dict_path = dict_path;
    } else if (sd_stat(dict_dz_path, &st) == 0 && SD_ISREG(st.st_mode)) {
        actual_dict_path = dict_dz_path;
    } else {
        fprintf(stderr, "Error: Neither .dict nor .dict.dz file found\n");
        free(dict_path);
        free(dict_dz_path);
        return NULL;
    }

    // Call full-path version
    sd_dictd *result = sd_dictd_open_from_paths(index_path, actual_dict_path);

    free(dict_path);
    free(dict_dz_path);

    return result;
}

// ============================================================
// Destructor (public API)
// ============================================================

void sd_dictd_close(sd_dictd *dictd) {
    if (dictd) {
        sd_dictd_cleanup(dictd);
        free(dictd);
    }
}
