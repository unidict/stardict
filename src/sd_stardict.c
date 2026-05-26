//
//  sd_stardict.c
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#include "sd_stardict.h"
#include "sd_stardict_ifo.h"
#include "sd_dictfile_index.h"
#include "sd_dictfile.h"
#include "sd_stardict_syn.h"
#include "sd_stardict_res.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sd_port.h"

#include "sd_array.h"


struct sd_stardict {

    // StarDict private fields
    char *ifo_path;      // .ifo file path
    char *idx_path;      // .idx file path
    char *dict_path;     // .dict or .dict.dz file path
    char *syn_path;      // .syn file path (optional, can be NULL)

    // Components
    sd_stardict_ifo *ifo;              // .ifo file parser
    sd_dictfile_index *idx;            // .idx index file handler
    sd_dictfile *dict;                 // .dict data reader
    sd_stardict_syn *syn;              // .syn synonym file (optional)
    sd_stardict_res *res;        // .res resource storage (optional)
};

// ============================================================
// Helper functions
// ============================================================

/**
 * StarDict index entry parse function (for dict_index)
 * StarDict index format: word_str '\0' offset(32-bit, network byte order) size(32-bit, network byte order)
 */
static size_t stardict_parse_entry(const char *data, size_t data_size,
                                  sd_dictfile_index_entry *entry) {
    if (!data || !entry || data_size < 5) {
        return 0;  // At least need '\0' + 4-byte offset + 4-byte size
    }

    const char *p = data;
    const char *end = data + data_size;

    // Parse word (NUL-terminated)
    size_t word_len = strnlen(p, end - p);
    if (word_len == 0 || p + word_len >= end) {
        return 0;  // Invalid data
    }

    entry->word = strndup(p, word_len);
    p += word_len + 1;  // Skip word and '\0'

    // Check remaining space
    if (p + 8 > end) {
        free(entry->word);
        entry->word = NULL;
        return 0;
    }

    // Parse offset (4 bytes, network byte order)
    uint32_t offset;
    memcpy(&offset, p, 4);
    offset = ntohl(offset);  // Network to host byte order
    p += 4;

    // Parse size (4 bytes, network byte order)
    uint32_t size;
    memcpy(&size, p, 4);
    size = ntohl(size);  // Network to host byte order
    p += 4;

    entry->offset = offset;
    entry->size = size;

    // Return total bytes consumed by this entry
    return (p - data);
}

/**
 * Check if file exists
 */
static bool file_exists(const char *path) {
    SD_STAT_STRUCT st;
    return (sd_stat(path, &st) == 0 && SD_ISREG(st.st_mode));
}

/**
 * Replace file extension
 */
static char *replace_extension(const char *path, const char *new_ext) {
    const char *last_dot = strrchr(path, '.');
    if (!last_dot) {
        return NULL;
    }

    size_t base_len = last_dot - path;
    char *result = malloc(base_len + strlen(new_ext) + 1);
    if (!result) {
        return NULL;
    }

    memcpy(result, path, base_len);
    strcpy(result + base_len, new_ext);
    return result;
}

/**
 * Get directory path
 */
static char *get_directory(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return strdup(".");
    }

    size_t dir_len = last_slash - path;
    char *result = malloc(dir_len + 1);
    if (!result) {
        return NULL;
    }

    memcpy(result, path, dir_len);
    result[dir_len] = '\0';
    return result;
}

// ============================================================
// sd_stardict cleanup function (internal)
// ============================================================
static void sd_stardict_cleanup(sd_stardict *stardict) {
    if (!stardict) return;

    if (stardict->syn) {
        sd_stardict_syn_free(stardict->syn);
        stardict->syn = NULL;
    }

    if (stardict->res) {
        sd_stardict_res_store_free(stardict->res);
        stardict->res = NULL;
    }

    if (stardict->dict) {
        sd_dictfile_close(stardict->dict);
        stardict->dict = NULL;
    }

    if (stardict->idx) {
        sd_dictfile_index_close(stardict->idx);
        stardict->idx = NULL;
    }

    if (stardict->ifo) {
        sd_stardict_ifo_free(stardict->ifo);
        stardict->ifo = NULL;
    }

    if (stardict->ifo_path) {
        free(stardict->ifo_path);
        stardict->ifo_path = NULL;
    }

    if (stardict->idx_path) {
        free(stardict->idx_path);
        stardict->idx_path = NULL;
    }

    if (stardict->dict_path) {
        free(stardict->dict_path);
        stardict->dict_path = NULL;
    }

    if (stardict->syn_path) {
        free(stardict->syn_path);
        stardict->syn_path = NULL;
    }
}

// ============================================================
// Public API implementation
// ============================================================

/**
 * Get dictionary info
 */
const sd_stardict_ifo *stardict_get_info(sd_stardict *stardict) {
    if (!stardict) {
        return NULL;
    }
    return stardict->ifo;
}

/**
 * Look up index entries by exact key (returns index entries without data)
 */
sd_status stardict_entry_lookup(sd_stardict *stardict, const char *key, sd_index_entry_array **out_index_entries) {
    if (!stardict || !key || !out_index_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!stardict->idx) {
        return SD_ERR_STATE;
    }
    *out_index_entries = NULL;

    // Collect all entries (synonyms + direct matches)
    sd_array(sd_dictfile_index_entry) all_entries = NULL;

    // 1. First check direct matches (main index)
    sd_array(sd_dictfile_index_entry) direct_entries = sd_dictfile_index_lookup(stardict->idx, key, false, 0);

    if (direct_entries && sd_array_size(direct_entries) > 0) {
        size_t direct_count = sd_array_size(direct_entries);
        for (size_t i = 0; i < direct_count; i++) {
            sd_array_push_back(all_entries, direct_entries[i]);
        }
        sd_array_free(direct_entries);
    }

    // 2. Then check synonyms (supplement), use sorted insert
    if (stardict->syn && sd_stardict_syn_is_loaded(stardict->syn)) {
        sd_array(uint32_t) syn_indices = sd_stardict_syn_lookup(stardict->syn, key,
                                                                 false, 0);

        if (syn_indices && sd_array_size(syn_indices) > 0) {
            size_t syn_count = sd_array_size(syn_indices);
            for (size_t i = 0; i < syn_count; i++) {
                const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(stardict->idx, syn_indices[i]);
                if (entry) {
                    sd_dictfile_index_entry entry_copy;
                    entry_copy.word = strdup(entry->word);
                    if (entry_copy.word) {
                        entry_copy.offset = entry->offset;
                        entry_copy.size = entry->size;
                        sd_dictfile_index_entries_sorted_insert(all_entries, &entry_copy);
                    }
                }
            }
            sd_array_free(syn_indices);
        }
    }

    if (!all_entries || sd_array_size(all_entries) == 0) {
        return SD_NOT_FOUND;
    }

    // Build index entry array
    size_t count = sd_array_size(all_entries);
    sd_index_entry_array *results = malloc(sizeof(sd_index_entry_array));
    if (!results) {
        sd_dictfile_index_free_entries(all_entries);
        return SD_ERR_MEMORY;
    }

    results->count = count;
    results->items = calloc(count, sizeof(sd_dictfile_index_entry *));
    if (!results->items) {
        free(results);
        sd_dictfile_index_free_entries(all_entries);
        return SD_ERR_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        sd_dictfile_index_entry *info = calloc(1, sizeof(sd_dictfile_index_entry));
        if (info) {
            info->word = all_entries[i].word;
            all_entries[i].word = NULL;
            info->offset = all_entries[i].offset;
            info->size = all_entries[i].size;
            results->items[i] = info;
        }
    }

    sd_dictfile_index_free_entries(all_entries);
    *out_index_entries = results;
    return SD_OK;
}

/**
 * Look up all matching entries
 */
sd_status stardict_lookup(sd_stardict *stardict, const char *key, sd_data_entry_array **out_data_entries) {
    if (!stardict || !key || !out_data_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!stardict->idx) {
        return SD_ERR_STATE;
    }
    *out_data_entries = NULL;

    sd_index_entry_array *index_results = NULL;
    sd_status st = stardict_entry_lookup(stardict, key, &index_results);
    if (st != SD_OK) {
        return st;
    }

    // Build data entry array
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

        sd_dictfile_data_block *block = sd_dictfile_read(stardict->dict,
                                                               entry->offset,
                                                               entry->size);
        if (block) {
            sd_dictfile_data_to_string(stardict->dict,
                                            block->data, block->size,
                                            &results->items[i].definition);
        }
    }

    sd_index_entry_array_free(index_results);
    *out_data_entries = results;
    return SD_OK;
}

/**
 * Get word suggestions by prefix
 */
sd_status stardict_suggest(sd_stardict *stardict, const char *prefix, size_t limit, sd_index_entry_array **out_index_entries) {
    if (!stardict || !prefix || !out_index_entries) {
        return SD_ERR_INVALID_PARAM;
    }
    if (!stardict->idx) {
        return SD_ERR_STATE;
    }
    *out_index_entries = NULL;

    // Prefix match
    sd_array(sd_dictfile_index_entry) entries = sd_dictfile_index_lookup(
        stardict->idx, prefix, true, limit);

    if (!entries || sd_array_size(entries) == 0) {
        if (entries) sd_dictfile_index_free_entries(entries);
        return SD_NOT_FOUND;
    }

    // Build suggestion list
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
sd_status stardict_fetch(sd_stardict *stardict, const sd_dictfile_index_entry *entry, char **out_definition) {
    if (!stardict || !entry || !out_definition) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_definition = NULL;

    sd_dictfile_data_block *block = sd_dictfile_read(stardict->dict,
                                                           entry->offset,
                                                           entry->size);
    if (!block) {
        return SD_NOT_FOUND;
    }

    char *result = NULL;
    if (sd_dictfile_data_to_string(stardict->dict,
                                   block->data, block->size, &result) == 0) {
        *out_definition = result;
        return SD_OK;
    }

    return SD_ERR_IO;
}

const sd_dictfile_index *stardict_get_index(sd_stardict *stardict) {
    if (!stardict) {
        return NULL;
    }
    return stardict->idx;
}

sd_stardict_paths sd_stardict_get_paths(sd_stardict *stardict) {
    sd_stardict_paths paths = {0};
    if (!stardict) return paths;
    paths.ifo_path = stardict->ifo_path;
    paths.idx_path = stardict->idx_path;
    paths.dict_path = stardict->dict_path;
    paths.syn_path = stardict->syn_path;
    return paths;
}

// ============================================================
// Constructor (public API)
// ============================================================

/**
 * Open StarDict dictionary file (specify all file paths)
 */
static sd_status sd_stardict_open_from_paths(const char *ifo_path,
                                      const char *idx_path,
                                      const char *dict_path,
                                      const char *syn_path,
                                      sd_stardict **out_dict) {
    if (!ifo_path || !dict_path || !out_dict) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_dict = NULL;

    // Verify files exist
    if (!file_exists(ifo_path)) {
        return SD_ERR_IO;
    }

    if (idx_path && !file_exists(idx_path)) {
        return SD_ERR_IO;
    }

    if (!file_exists(dict_path)) {
        return SD_ERR_IO;
    }

    if (syn_path && !file_exists(syn_path)) {
        syn_path = NULL;  // syn is optional, ignore if not found
    }

    // Allocate sd_stardict object
    sd_stardict *stardict = calloc(1, sizeof(sd_stardict));
    if (!stardict) {
        return SD_ERR_MEMORY;
    }

    // Copy paths
    stardict->ifo_path = strdup(ifo_path);
    if (idx_path) stardict->idx_path = strdup(idx_path);
    stardict->dict_path = strdup(dict_path);
    if (syn_path) {
        stardict->syn_path = strdup(syn_path);
    }

    if (!stardict->ifo_path || !stardict->dict_path) {
        sd_stardict_cleanup(stardict);
        free(stardict);
        return SD_ERR_MEMORY;
    }
    if (idx_path && !stardict->idx_path) {
        sd_stardict_cleanup(stardict);
        free(stardict);
        return SD_ERR_MEMORY;
    }

    // Load .ifo file
    stardict->ifo = sd_stardict_ifo_load(ifo_path);
    if (!stardict->ifo) {
        sd_stardict_cleanup(stardict);
        free(stardict);
        return SD_ERR_FORMAT;
    }

    // Load .idx index file (skip in data_only mode)
    if (idx_path) {
        stardict->idx = sd_dictfile_index_open(idx_path, stardict_parse_entry,
                                          stardict->ifo->wordcount,
                                          stardict->ifo->index_file_size);
        if (!stardict->idx) {
            sd_stardict_cleanup(stardict);
            free(stardict);
            return SD_ERR_FORMAT;
        }
    }

    // Open .dict data file
    stardict->dict = sd_dictfile_open(dict_path, stardict->ifo->sametypesequence);
    if (!stardict->dict) {
        sd_stardict_cleanup(stardict);
        free(stardict);
        return SD_ERR_IO;
    }

    // Load .syn synonym file (if exists)
    if (syn_path) {
        stardict->syn = sd_stardict_syn_load(syn_path,
                                             stardict->ifo->syn_wordcount);
        // syn is optional, failure is OK
    }

    // Try loading resource storage (res/ directory or res.rdic database)
    char *dirname = get_directory(ifo_path);
    if (dirname) {
        stardict->res = sd_stardict_res_store_load(dirname);
        // Resource storage is optional, failure is OK
        free(dirname);
    }

    *out_dict = stardict;
    return SD_OK;
}

/**
 * Open StarDict dictionary file (auto-discover related files)
 */
sd_status sd_stardict_open(const char *ifo_path, bool data_only, sd_stardict **out_dict) {
    if (!ifo_path || !out_dict) {
        return SD_ERR_INVALID_PARAM;
    }
    *out_dict = NULL;

    // Construct various file paths
    char *idx_gz_path = replace_extension(ifo_path, ".idx.gz");
    char *idx_path = replace_extension(ifo_path, ".idx");
    char *dict_path = replace_extension(ifo_path, ".dict");
    char *dict_dz_path = replace_extension(ifo_path, ".dict.dz");
    char *syn_path = replace_extension(ifo_path, ".syn");

    if (!idx_gz_path || !idx_path || !dict_path || !dict_dz_path || !syn_path) {
        free(idx_gz_path);
        free(idx_path);
        free(dict_path);
        free(dict_dz_path);
        free(syn_path);
        return SD_ERR_MEMORY;
    }

    // In data_only mode, skip .idx and .syn
    const char *actual_idx_path = NULL;
    if (!data_only) {
        // Prefer .idx.gz (if exists)
        if (file_exists(idx_gz_path)) {
            actual_idx_path = idx_gz_path;
        } else if (file_exists(idx_path)) {
            actual_idx_path = idx_path;
        } else {
            free(idx_gz_path);
            free(idx_path);
            free(dict_path);
            free(dict_dz_path);
            free(syn_path);
            return SD_ERR_IO;
        }
    }

    // Check for .dict or .dict.dz file
    const char *actual_dict_path = NULL;
    if (file_exists(dict_path)) {
        actual_dict_path = dict_path;
    } else if (file_exists(dict_dz_path)) {
        actual_dict_path = dict_dz_path;
    } else {
        free(idx_gz_path);
        free(idx_path);
        free(dict_path);
        free(dict_dz_path);
        free(syn_path);
        return SD_ERR_IO;
    }

    // Check if .syn file exists (skip in data_only mode)
    const char *actual_syn_path = NULL;
    if (!data_only) {
        actual_syn_path = file_exists(syn_path) ? syn_path : NULL;
    }

    // Call full-path version
    sd_status st = sd_stardict_open_from_paths(ifo_path, actual_idx_path,
                                               actual_dict_path,
                                               actual_syn_path, out_dict);

    free(idx_gz_path);
    free(idx_path);
    free(dict_path);
    free(dict_dz_path);
    free(syn_path);

    return st;
}

// ============================================================
// Destructor (public API)
// ============================================================

void sd_stardict_close(sd_stardict *stardict) {
    if (stardict) {
        sd_stardict_cleanup(stardict);
        free(stardict);
    }
}
