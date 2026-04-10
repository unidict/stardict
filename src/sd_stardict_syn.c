//
//  sd_stardict_syn.c
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#include "sd_stardict_syn.h"
#include "sd_types.h"
#include "sd_array.h"
#include "sd_port.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

// ============================================================
// Helper functions
// ============================================================

/**
 * Case-insensitive string comparison (consistent with idx)
 * @param prefix true=prefix match, false=exact match
 */
static int stardict_strcmp_syn(const char *s1, const char *s2, bool prefix) {
    if (prefix) {
        // Prefix match mode: check if s1 is a prefix of s2
        size_t len1 = strlen(s1);
        int cmp = sd_strncasecmp(s1, s2, len1);
        if (cmp == 0) {
            return 0;  // s1 is a prefix of s2
        }
        return cmp;
    } else {
        // Exact match mode: case-insensitive first, then case-sensitive
        int a = sd_strcasecmp(s1, s2);
        if (a == 0) {
            return strcmp(s1, s2);
        }
        return a;
    }
}

// ============================================================
// Public API implementation
// ============================================================

sd_stardict_syn *sd_stardict_syn_load(const char *syn_path, uint32_t word_count) {
    if (!syn_path || word_count == 0) {
        return NULL;
    }

    // Create syn object
    sd_stardict_syn *syn = calloc(1, sizeof(sd_stardict_syn));
    if (!syn) {
        fprintf(stderr, "Error: Memory allocation failed for sd_stardict_syn\n");
        return NULL;
    }

    syn->syn_path = strdup(syn_path);
    syn->word_count = word_count;
    syn->loaded = false;

    // Open file
    FILE *fp = fopen(syn_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open .syn file: %s\n", syn_path);
        free(syn->syn_path);
        free(syn);
        return NULL;
    }

    // Get file size
    SD_STAT_STRUCT st;
    if (sd_stat(syn_path, &st) < 0) {
        fprintf(stderr, "Error: Cannot stat .syn file: %s\n", syn_path);
        fclose(fp);
        free(syn->syn_path);
        free(syn);
        return NULL;
    }

    syn->file_size = st.st_size;

    // Read entire file into memory
    char *data = malloc(st.st_size);
    if (!data) {
        fprintf(stderr, "Error: Memory allocation failed for .syn file\n");
        fclose(fp);
        free(syn->syn_path);
        free(syn);
        return NULL;
    }

    if (fread(data, 1, st.st_size, fp) != (size_t)st.st_size) {
        fprintf(stderr, "Error: Failed to read .syn file\n");
        free(data);
        fclose(fp);
        free(syn->syn_path);
        free(syn);
        return NULL;
    }

    fclose(fp);
    syn->data = data;

    // Build index array (for fast lookup)
    syn->words = calloc(word_count, sizeof(char *));
    syn->indices = calloc(word_count, sizeof(uint32_t));

    if (!syn->words || !syn->indices) {
        fprintf(stderr, "Error: Memory allocation failed for syn indices\n");
        if (syn->words) free(syn->words);
        if (syn->indices) free(syn->indices);
        free(data);
        free(syn->syn_path);
        free(syn);
        return NULL;
    }

    // Parse file content
    char *p = data;
    char *end = data + syn->file_size;
    uint32_t count = 0;

    while (p < end && count < word_count) {
        // Save word pointer
        syn->words[count] = p;

        // Skip word string
        size_t word_len = strlen(p);
        p += word_len + 1;  // Skip word and '\0'

        // Read index (4 bytes, network byte order)
        if (p + 4 > end) {
            fprintf(stderr, "Error: Invalid .syn file format (truncated index)\n");
            break;
        }

        uint32_t net_index;
        memcpy(&net_index, p, 4);
        syn->indices[count] = ntohl(net_index);  // Convert to host byte order
        p += 4;

        count++;
    }

    if (count != word_count) {
        fprintf(stderr, "Warning: .syn file word count mismatch (expected %u, got %u)\n",
                word_count, count);
        syn->word_count = count;
    }

    syn->loaded = true;
    printf("Loaded %u synonym entries from .syn file\n", syn->word_count);

    return syn;
}

void sd_stardict_syn_free(sd_stardict_syn *syn) {
    if (!syn) {
        return;
    }

    // Free index array
    if (syn->words) {
        free(syn->words);
        syn->words = NULL;
    }

    if (syn->indices) {
        free(syn->indices);
        syn->indices = NULL;
    }

    // Free data buffer
    if (syn->data) {
        free(syn->data);
        syn->data = NULL;
    }

    // Free path
    if (syn->syn_path) {
        free(syn->syn_path);
        syn->syn_path = NULL;
    }

    free(syn);
}

/**
 * Expand search: find all matches forward and backward from first_match
 * @param syn syn object
 * @param word Word to search for
 * @param first_match Index of first match
 * @param prefix true=prefix match, false=exact match
 * @param limit Max results (0 = unlimited)
 * @return cvector array with all matching indices
 */
static sd_array(uint32_t) expand_syn_matches(const sd_stardict_syn *syn, const char *word,
                                            int32_t first_match, bool prefix,
                                            uint32_t limit) {
    sd_array(uint32_t) result = NULL;

    // Find leftmost match by scanning backward
    int32_t left = first_match;
    for (int32_t i = first_match - 1; i >= 0; i--) {
        const char *key = syn->words[i];
        if (!key) break;
        if (stardict_strcmp_syn(word, key, prefix) != 0) break;
        left = i;
    }

    // Collect all matches forward from leftmost
    for (int32_t i = left; i < (int32_t)syn->word_count; i++) {
        const char *key = syn->words[i];
        if (!key) break;
        if (stardict_strcmp_syn(word, key, prefix) != 0) break;

        sd_array_push_back(result, syn->indices[i]);

        if (limit > 0 && sd_array_size(result) >= limit) {
            break;
        }
    }

    return result;
}

/**
 * Look up synonym indices (new API)
 * @param syn syn object
 * @param word Synonym to search for
 * @param prefix true=prefix match, false=exact match
 * @param limit Max results (0 = unlimited)
 * @return cvector array with all matching indices (caller must free with sd_array_free)
 * @note Returns NULL on failure, empty vector (size=0) if not found
 */
sd_array(uint32_t) sd_stardict_syn_lookup(const sd_stardict_syn *syn, const char *word,
                                         bool prefix, uint32_t limit) {
    if (!syn || !syn->loaded || !word) {
        return NULL;
    }

    sd_array(uint32_t) result = NULL;

    if (syn->word_count == 0) {
        return result;  // Empty vector
    }

    // Binary search: find leftmost match
    int32_t low = 0;
    int32_t high = syn->word_count - 1;
    int32_t leftmost_match = -1;

    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        const char *key = syn->words[mid];
        if (!key) {
            return NULL;  // Error
        }

        int cmp = stardict_strcmp_syn(word, key, prefix);
        if (cmp == 0) {
            // Found, record and continue searching left
            leftmost_match = mid;
            high = mid - 1;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    if (leftmost_match < 0) {
        return result;  // Not found, return empty vector
    }

    // Expand search for all matches
    result = expand_syn_matches(syn, word, leftmost_match, prefix, limit);

    return result;
}
