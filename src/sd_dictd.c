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
 * Look up index entries by exact key
 */
sd_status sd_dictd_entry_lookup(sd_dictd *dictd, const char *key, sd_index_entry_array **out_index_entries) {
    if (!dictd || !key || !out_index_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!dictd->index) {
        return SD_ERR_STATE;
    }
    *out_index_entries = NULL;

    sd_array(sd_dictfile_index_entry) entries = sd_dictfile_index_lookup(dictd->index, key,
                                                     false, 0);

    if (!entries || sd_array_size(entries) == 0) {
        return SD_NOT_FOUND;
    }

    size_t count = sd_array_size(entries);
    sd_index_entry_array *results = malloc(sizeof(sd_index_entry_array));
    if (!results) {
        sd_dictfile_index_free_entries(entries);
        return SD_ERR_MEMORY;
    }

    results->count = count;
    results->items = calloc(count, sizeof(sd_dictfile_index_entry *));
    if (!results->items) {
        free(results);
        sd_dictfile_index_free_entries(entries);
        return SD_ERR_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        sd_dictfile_index_entry *info = calloc(1, sizeof(sd_dictfile_index_entry));
        if (info) {
            info->word = entries[i].word;
            entries[i].word = NULL;
            info->offset = entries[i].offset;
            info->size = entries[i].size;
            results->items[i] = info;
        }
    }

    sd_dictfile_index_free_entries(entries);
    *out_index_entries = results;
    return SD_OK;
}

/**
 * Look up all matching entries
 */
sd_status sd_dictd_lookup(sd_dictd *dictd, const char *key, sd_data_entry_array **out_data_entries) {
    if (!dictd || !key || !out_data_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!dictd->index) {
        return SD_ERR_STATE;
    }
    *out_data_entries = NULL;

    sd_index_entry_array *index_results = NULL;
    sd_status st = sd_dictd_entry_lookup(dictd, key, &index_results);
    if (st != SD_OK) {
        return st;
    }

    sd_data_entry_array *results = malloc(sizeof(sd_data_entry_array));
    if (!results) {
        sd_index_entry_array_free(index_results);
        return SD_ERR_MEMORY;
    }

    results->count = index_results->count;
    results->items = calloc(index_results->count, sizeof(sd_data_entry));
    if (!results->items) {
        free(results);
        sd_index_entry_array_free(index_results);
        return SD_ERR_MEMORY;
    }

    for (size_t i = 0; i < index_results->count; i++) {
        sd_dictfile_index_entry *entry = index_results->items[i];
        results->items[i].word = entry->word;
        entry->word = NULL;

        sd_dictfile_data_block *block = sd_dictfile_read(dictd->dict,
                                           entry->offset,
                                           entry->size);
        if (block) {
            results->items[i].definition = strndup(block->data, block->size);
        }
    }

    sd_index_entry_array_free(index_results);
    *out_data_entries = results;
    return SD_OK;
}

/**
 * Get word suggestions by prefix
 */
sd_status sd_dictd_suggest(sd_dictd *dictd, const char *prefix, size_t limit, sd_index_entry_array **out_index_entries) {
    if (!dictd || !prefix || !out_index_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!dictd->index) {
        return SD_ERR_STATE;
    }
    *out_index_entries = NULL;

    sd_array(sd_dictfile_index_entry) entries = sd_dictfile_index_lookup(dictd->index, prefix,
                                                     true,
                                                     (uint32_t)limit);

    if (!entries || sd_array_size(entries) == 0) {
        return SD_NOT_FOUND;
    }

    size_t count = sd_array_size(entries);

    sd_index_entry_array *suggestions = malloc(sizeof(sd_index_entry_array));
    if (!suggestions) {
        sd_dictfile_index_free_entries(entries);
        return SD_ERR_MEMORY;
    }

    suggestions->count = count;
    suggestions->items = calloc(count, sizeof(sd_dictfile_index_entry *));
    if (!suggestions->items) {
        free(suggestions);
        sd_dictfile_index_free_entries(entries);
        return SD_ERR_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        sd_dictfile_index_entry *info = calloc(1, sizeof(sd_dictfile_index_entry));
        if (info) {
            info->word = entries[i].word;  // Transfer ownership
            info->offset = entries[i].offset;
            info->size = entries[i].size;

            suggestions->items[i] = info;

            entries[i].word = NULL;  // Prevent double free
        }
    }

    sd_dictfile_index_free_entries(entries);

    *out_index_entries = suggestions;
    return SD_OK;
}

/**
 * Fetch word definition using index entry
 */
sd_status sd_dictd_fetch(sd_dictd *dictd, const sd_dictfile_index_entry *entry, char **out_definition) {
    if (!dictd || !entry || !out_definition) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_definition = NULL;

    sd_dictfile_data_block *block = sd_dictfile_read(dictd->dict,
                                       entry->offset,
                                       entry->size);
    if (!block) {
        return SD_NOT_FOUND;
    }

    char *result = strndup(block->data, block->size);
    if (!result) {
        return SD_ERR_MEMORY;
    }

    *out_definition = result;
    return SD_OK;
}

const sd_dictfile_index *sd_dictd_get_index(sd_dictd *dictd) {
    if (!dictd) {
        return NULL;
    }
    return dictd->index;
}

sd_dictd_paths sd_dictd_get_paths(sd_dictd *dictd) {
    sd_dictd_paths paths = {0};
    if (!dictd) return paths;
    paths.index_path = dictd->index_path;
    paths.dict_path = dictd->dict_path;
    return paths;
}

// ============================================================
// Constructor (public API)
// ============================================================

/**
 * Open dictd dictionary file (specify all file paths)
 */
static sd_status sd_dictd_open_from_paths(const char *index_path,
                                   const char *dict_path,
                                   bool load_index,
                                   sd_dictd **out_dict) {
    if (!dict_path || !out_dict) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_dict = NULL;

    // Verify files exist
    SD_STAT_STRUCT st;
    if (index_path && load_index) {
        if (sd_stat(index_path, &st) != 0 || !SD_ISREG(st.st_mode)) {
            return SD_ERR_IO;
        }
    }

    if (sd_stat(dict_path, &st) != 0 || !SD_ISREG(st.st_mode)) {
        return SD_ERR_IO;
    }

    // Allocate sd_dictd object
    sd_dictd *dictd = calloc(1, sizeof(sd_dictd));
    if (!dictd) {
        return SD_ERR_MEMORY;
    }

    // Copy paths
    if (index_path) dictd->index_path = strdup(index_path);
    dictd->dict_path = strdup(dict_path);

    if (!dictd->dict_path) {
        sd_dictd_cleanup(dictd);
        free(dictd);
        return SD_ERR_MEMORY;
    }
    if (index_path && !dictd->index_path) {
        sd_dictd_cleanup(dictd);
        free(dictd);
        return SD_ERR_MEMORY;
    }

    // Load .index index file (skip in data_only mode)
    if (index_path && load_index) {
        dictd->index = sd_dictfile_index_open(index_path, dictd_parse_entry, 0, 0);
        if (!dictd->index) {
            sd_dictd_cleanup(dictd);
            free(dictd);
            return SD_ERR_FORMAT;
        }
    }

    // Open .dict data file
    dictd->dict = sd_dictfile_open(dict_path, NULL);
    if (!dictd->dict) {
        sd_dictd_cleanup(dictd);
        free(dictd);
        return SD_ERR_IO;
    }

    *out_dict = dictd;
    return SD_OK;
}

/**
 * Open dictd dictionary file (auto-discover related files)
 */
sd_status sd_dictd_open(const char *index_path, bool data_only, sd_dictd **out_dict) {
    if (!index_path || !out_dict) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_dict = NULL;

    // Construct .dict file path from base name (before last '.')
    const char *dot = strrchr(index_path, '.');
    size_t base_len = dot ? (size_t)(dot - index_path) : strlen(index_path);

    char *dict_path = malloc(base_len + 6);     // base + ".dict" + null
    char *dict_dz_path = malloc(base_len + 9);  // base + ".dict.dz" + null

    if (!dict_path || !dict_dz_path) {
        free(dict_path);
        free(dict_dz_path);
        return SD_ERR_MEMORY;
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
        free(dict_path);
        free(dict_dz_path);
        return SD_ERR_IO;
    }

    // Call full-path version
    bool load_index = !data_only;
    sd_status status = sd_dictd_open_from_paths(index_path, actual_dict_path, load_index, out_dict);

    free(dict_path);
    free(dict_dz_path);

    return status;
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
