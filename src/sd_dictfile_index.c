//
//  sd_dictfile_index.c
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#include "sd_dictfile_index.h"
#include "sd_port.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <zlib.h>

// ============================================================
// Internal types
// ============================================================

typedef enum {
    DICTFILE_INDEX_WORDLIST,   // Small file mode (fully loaded into memory)
    DICTFILE_INDEX_OFFSET      // Large file mode (page-based paging)
} sd_dictfile_index_mode;

typedef struct {
    char *data;              // Raw page data (malloc, needs free)
    sd_dictfile_index_entry *entries;     // Cached entry array
    uint32_t index;          // Cached page index
    uint32_t count;          // Cached entry count
    bool valid;              // Whether cache is valid
} sd_dictfile_page_cache;

typedef struct {
    uint32_t offset;         // Page start offset in index file
    char *first_word;        // First word (malloc, needs free)
    char *last_word;         // Last word (malloc, needs free)
} sd_dictfile_index_sdoft_page;

struct sd_dictfile_index {
    sd_dictfile_index_mode mode;    // Index mode
    uint32_t wordcount;      // Word count
    uint32_t isize;          // Decompressed index file size (0 = auto-detect)

    // Format-specific parse function
    sd_dictfile_index_entry_parse_fn parse_entry;

    // WordList mode (small file, fully loaded into memory)
    sd_dictfile_index_entry *entries;     // Entry array

    // Offset mode (large file, mmap + paging)
    FILE *idx_file;          // File handle (for fread)

    // .sdoft cache
    sd_array(sd_dictfile_index_sdoft_page) pages;  // Page info array
    uint32_t end_offset;          // End offset of the last page
    bool pages_loaded;     // Whether sdoft is loaded

    // Page cache (for Offset mode)
    sd_dictfile_page_cache page_cache;

    // File path
    char *idx_path;
};

// ============================================================
// Constants
// ============================================================
#define SMALL_DICT_THRESHOLD (100 * 1024)  // Small dictionary threshold: 100 KB
#define ENTRIES_PER_PAGE 32                 // Entries per page

// .sdoft cache constants (libud custom format)
#define SDOFT_MAGIC "SDOFT\0"
#define SDOFT_VERSION 1
#define SDOFT_EXT ".sdoft"

// ============================================================
// Helper functions
// ============================================================

/**
 * Case-insensitive string comparison
 */
static int index_strcmp(const char *s1, const char *s2, bool prefix) {
    if (prefix) {
        size_t len1 = strlen(s1);
        int cmp = sd_strncasecmp(s1, s2, len1);
        if (cmp == 0) {
            return 0;  // s1 is a prefix of s2
        }
        return cmp;
    } else {
        // Exact match
        int a = sd_strcasecmp(s1, s2);
        if (a == 0) {
            return strcmp(s1, s2);
        }
        return a;
    }
}

/**
 * Check if word is within page range
 * @param word Word to search for
 * @param page Page info (contains first_word and last_word)
 * @param prefix true=prefix match, false=exact match
 * @return 0 if word is in page range, <0 if before page, >0 if after page
 */
static int index_pagecmp(const char *word, const sd_dictfile_index_sdoft_page *page, bool prefix) {
    if (!word || !page) {
        return 0;
    }

    // Compare first_word first
    int cmp_first = index_strcmp(word, page->first_word, prefix);

    // word < first_word
    if (cmp_first < 0) {
        return cmp_first;
    }

    // word >= first_word, check last_word
    int cmp_last = index_strcmp(word, page->last_word, prefix);

    // word > last_word
    if (cmp_last > 0) {
        return cmp_last;
    }

    // word is in [first_word, last_word] range
    return 0;
}

/**
 * Forward declarations
 */
static int load_page_to_cache(sd_dictfile_index *index, uint32_t page_idx);

/**
 * Binary search: find word in [low, high] range (find leftmost match)
 * @param index Index object
 * @param word Word to search for
 * @param low Start index
 * @param high End index
 * @param out_index Output: found index or insertion position, must not be NULL
 * @return 0 on success, -1 if not found, -2 on failure
 */
static int dict_index_bsearch_left(sd_dictfile_index *index, const char *word,
                                uint32_t low, uint32_t high,
                                sd_dictfile_index_entry *out_entry, uint32_t *out_index,
                                bool prefix) {
    if (!out_index) {
        return -2;
    }

    // Initialize entry_out to NULL (if pointer provided)
    if (out_entry) {
        out_entry->word = NULL;
        out_entry->offset = 0;
        out_entry->size = 0;
    }

    int32_t leftmost_match = -1;
    const sd_dictfile_index_entry *found_entry = NULL;

    // Use int64_t to avoid unsigned integer underflow
    int64_t ilow = (int64_t)low;
    int64_t ihigh = (int64_t)high;

    while (ilow <= ihigh) {
        int64_t imid = ilow + (ihigh - ilow) / 2;
        const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(index, (uint32_t)imid);
        if (!entry) {
            return -2;
        }

        int cmp = index_strcmp(word, entry->word, prefix);
        if (cmp == 0) {
            // Found, record and continue searching left
            leftmost_match = (int32_t)imid;
            found_entry = entry;
            ihigh = imid - 1;
        } else if (cmp < 0) {
            ihigh = imid - 1;
        } else {
            ilow = imid + 1;
        }
    }

    if (leftmost_match >= 0) {
        // Exact match found
        *out_index = (uint32_t)leftmost_match;
        // Output found entry
        if (out_entry && found_entry) {
            out_entry->word = found_entry->word;
            out_entry->offset = found_entry->offset;
            out_entry->size = found_entry->size;
        }
        return 0;
    } else {
        // Not found, return insertion position (low is the insertion point)
        *out_index = (uint32_t)ilow;
        return -1;
    }
}

/**
 * Page-level binary search: find the leftmost page that may contain the word
 * @param index Index object
 * @param word Word to search for
 * @param out_index Output: target page number, must not be NULL
 * @return 0 on success, -1 on failure
 */
static int dict_index_page_bsearch_left(sd_dictfile_index *index, const char *word,
                                     uint32_t *out_index, bool prefix) {
    if (!out_index) {
        return -1;
    }

    // Use int64_t to avoid unsigned integer underflow
    int64_t page_from = 0;
    int64_t page_to = (int64_t)(sd_array_size(index->pages) - 1);  // // Index of last page
    int64_t target_page = 0; // Default to first page

    while (page_from <= page_to) {
        int64_t page_mid = page_from + (page_to - page_from) / 2;
        const sd_dictfile_index_sdoft_page *page = &index->pages[page_mid];

        int cmp = index_pagecmp(word, page, prefix);
        if (cmp == 0) {
            // word is in this page range, record and continue searching left
            target_page = page_mid;
            page_to = page_mid - 1;
        } else if (cmp < 0) {
            // word is before this page
            page_to = page_mid - 1;
        } else {
            // word is after this page
            page_from = page_mid + 1;
        }
    }

    // Check if target_page is out of range
    if (target_page >= (int64_t)sd_array_size(index->pages)) {
        target_page = (int64_t)(sd_array_size(index->pages) - 1); // // Out of range, return last page
    }
    if (target_page < 0) {
        target_page = 0;
    }

    *out_index = (uint32_t)target_page;
    return 0;
}

/**
 * Expand search: find all matches forward and backward from first_match
 * @param index Index object
 * @param word Word to search for
 * @param match_index Index of the first match
 * @param limit Max results (0 = unlimited)
 * @return cvector array with all matching entries
 */
static sd_array(sd_dictfile_index_entry) expand_matches(sd_dictfile_index *index, const char *word,
                                        int32_t match_index,
                                        const sd_dictfile_index_entry *match_entry,
                                        bool prefix,
                                        uint32_t limit, bool backtrack) {
    sd_array(sd_dictfile_index_entry) result = NULL;

    // Add first match entry (already in memory, copy directly)
    sd_dictfile_index_entry entry_copy;
    entry_copy.word = strdup(match_entry->word);
    if (!entry_copy.word) {
        return NULL; // Memory allocation failed
    }
    entry_copy.offset = match_entry->offset;
    entry_copy.size = match_entry->size;
    sd_array_push_back(result, entry_copy);

    if (backtrack) {
        // Search backward (starting from first_match - 1)
        for (int32_t i = match_index - 1; i >= 0; i--) {
            const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(index, i);
            if (!entry) break;

            int cmp = index_strcmp(word, entry->word, prefix);
            if (cmp != 0) break;

            // Copy entry
            entry_copy.word = strdup(entry->word);
            if (!entry_copy.word) {
                // Memory allocation failed, clean up allocated memory
                sd_dictfile_index_free_entries(result);
                return NULL;
            }
            entry_copy.offset = entry->offset;
            entry_copy.size = entry->size;

            // Insert at beginning
            sd_array_insert(result, 0, entry_copy);

            // If exceeds limit, remove from end
            if (limit > 0 && sd_array_size(result) > limit) {
                sd_array_pop_back(result);
            }
        }

        // If limit reached, return immediately (no need to search forward)
        if (limit > 0 && sd_array_size(result) >= limit) {
            return result;
        }
    }

    // Search forward (starting from first_match + 1)
    for (int32_t i = match_index + 1; i < (int32_t)index->wordcount; i++) {
        const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(index, i);
        if (!entry) break;

        int cmp = index_strcmp(word, entry->word, prefix);
        if (cmp != 0) break;

        // Copy entry
        entry_copy.word = strdup(entry->word);
        if (!entry_copy.word) {
            sd_dictfile_index_free_entries(result);
            return NULL;
        }
        entry_copy.offset = entry->offset;
        entry_copy.size = entry->size;

        // Add to end
        sd_array_push_back(result, entry_copy);

        if (limit > 0 && sd_array_size(result) >= limit) {
            break;
        }
    }

    return result;
}

/**
 * Get .sdoft cache file path
 */
static char *get_sdoft_cache_path(const char *idx_path) {
    if (!idx_path) {
        return NULL;
    }

    size_t len = strlen(idx_path);
    char *sdoft_path = malloc(len + sizeof(SDOFT_EXT));  // ".sdoft" + null
    if (!sdoft_path) {
        return NULL;
    }

    strcpy(sdoft_path, idx_path);
    strcat(sdoft_path, SDOFT_EXT);

    return sdoft_path;
}

/**
 * Read ISIZE field at end of gzip file (RFC 1952)
 * @param gz_path .gz file path
 * @return Decompressed size (Index SIZE), 0 on failure
 *
 * ISIZE = Input SIZE (RFC 1952) = Index SIZE (index file size)
 * Note: This is the original uncompressed data size, mod 2^32
 */
static uint32_t read_gzip_isize(const char *gz_path) {
    FILE *fp = fopen(gz_path, "rb");
    if (!fp) {
        return 0;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    // Check if file has at least gzip header (10 bytes) + tail (8 bytes)
    if (file_size < 18) {
        fclose(fp);
        return 0;  // File too small or corrupted
    }

    // Read ISIZE (last 4 bytes of file, little-endian)
    fseek(fp, -4, SEEK_END);
    uint32_t isize;
    if (fread(&isize, 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }

    fclose(fp);

    // ISIZE is little-endian
    // Already little-endian on most systems, but needs conversion on big-endian systems
    // Assume file is stored in little-endian
    return isize;
}

// ============================================================
// sdoft file operations
// ============================================================

/**
 * Save page info to .sdoft cache file (libud custom format)
 */
static int save_sdoft_cache(sd_dictfile_index *index, const char *sdoft_path) {
    if (!index || !sdoft_path || !index->pages) {
        return -1;
    }

    FILE *fp = fopen(sdoft_path, "wb");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot create .sdoft cache file: %s\n", sdoft_path);
        return -1;
    }

    // 1. Write header (16 bytes)
    // Magic: "SDOFT\0" (6 bytes)
    if (fwrite(SDOFT_MAGIC, 1, 6, fp) != 6) {
        fprintf(stderr, "Warning: Failed to write .sdoft magic\n");
        fclose(fp);
        return -1;
    }

    // Reserved: 2 bytes (alignment)
    uint16_t reserved = 0;
    if (fwrite(&reserved, 1, sizeof(uint16_t), fp) != sizeof(uint16_t)) {
        fprintf(stderr, "Warning: Failed to write .sdoft reserved\n");
        fclose(fp);
        return -1;
    }

    // Version: 4 bytes
    uint32_t version = SDOFT_VERSION;
    if (fwrite(&version, 1, sizeof(uint32_t), fp) != sizeof(uint32_t)) {
        fprintf(stderr, "Warning: Failed to write .sdoft version\n");
        fclose(fp);
        return -1;
    }

    // wordcount: 4 bytes
    uint32_t wordcount = index->wordcount;
    if (fwrite(&wordcount, 1, sizeof(uint32_t), fp) != sizeof(uint32_t)) {
        fprintf(stderr, "Warning: Failed to write .sdoft wordcount\n");
        fclose(fp);
        return -1;
    }

    // 2. Write page info array
    for (size_t i = 0; i < sd_array_size(index->pages); i++) {
        sd_dictfile_index_sdoft_page *page = &index->pages[i];

        // Write offset
        if (fwrite(&page->offset, sizeof(uint32_t), 1, fp) != 1) {
            fprintf(stderr, "Warning: Failed to write .sdoft page offset\n");
            fclose(fp);
            return -1;
        }

        // Write first word (NUL-terminated)
        if (page->first_word) {
            size_t len = strlen(page->first_word) + 1;
            if (fwrite(page->first_word, 1, len, fp) != len) {
                fprintf(stderr, "Warning: Failed to write .sdoft first_word\n");
                fclose(fp);
                return -1;
            }
        } else {
            // Write empty string as terminator
            char null_term = '\0';
            if (fwrite(&null_term, 1, 1, fp) != 1) {
                fprintf(stderr, "Warning: Failed to write .sdoft null terminator\n");
                fclose(fp);
                return -1;
            }
        }

        // Write last word (NUL-terminated)
        if (page->last_word) {
            size_t len = strlen(page->last_word) + 1;
            if (fwrite(page->last_word, 1, len, fp) != len) {
                fprintf(stderr, "Warning: Failed to write .sdoft last_word\n");
                fclose(fp);
                return -1;
            }
        } else {
            // Write empty string as terminator
            char null_term = '\0';
            if (fwrite(&null_term, 1, 1, fp) != 1) {
                fprintf(stderr, "Warning: Failed to write .sdoft null terminator\n");
                fclose(fp);
                return -1;
            }
        }
    }

    // 3. Write end_offset
    if (fwrite(&index->end_offset, sizeof(uint32_t), 1, fp) != 1) {
        fprintf(stderr, "Warning: Failed to write .sdoft end_offset\n");
        fclose(fp);
        return -1;
    }

    // 4. Write metadata string (optional)
    const char *metadata_template = "url=%s\n";
    int metadata_len = snprintf(NULL, 0, metadata_template, index->idx_path);
    if (metadata_len < 0) {
        fprintf(stderr, "Warning: Failed to calculate metadata length\n");
        fclose(fp);
        return -1;
    }

    char *metadata = malloc((size_t)metadata_len + 1);
    if (!metadata) {
        fprintf(stderr, "Warning: Failed to allocate metadata buffer\n");
        fclose(fp);
        return -1;
    }

    snprintf(metadata, (size_t)metadata_len + 1, metadata_template, index->idx_path);
    if (fwrite(metadata, 1, (size_t)metadata_len + 1, fp) != (size_t)metadata_len + 1) {
        fprintf(stderr, "Warning: Failed to write .sdoft metadata\n");
        free(metadata);
        fclose(fp);
        return -1;
    }

    free(metadata);
    fclose(fp);
    return 0;
}

/**
 * Load page info from .sdoft cache file (libud custom format)
 */
static int load_sdoft_cache(sd_dictfile_index *index, const char *sdoft_path) {
    if (!index || !sdoft_path) {
        return -1;
    }

    // Open file
    int fd = sd_open(sdoft_path, SD_O_RDONLY);
    if (fd < 0) {
        return -1;  // File not found, not an error
    }

    // Get file size
    SD_STAT_STRUCT st;
    if (fstat(fd, &st) < 0) {
        sd_close(fd);
        return -1;
    }

    // Read entire file into memory
    size_t file_size = (size_t)st.st_size;
    char *data = malloc(file_size);
    if (!data) {
        sd_close(fd);
        return -1;
    }

    FILE *fp = fdopen(fd, "rb");
    if (!fp) {
        free(data);
        sd_close(fd);
        return -1;
    }

    if (fread(data, 1, file_size, fp) != file_size) {
        free(data);
        fclose(fp);
        return -1;
    }

    const char *p = data;
    const char *end = data + file_size;

    // 1. Read and validate header (16 bytes)
    if (end - p < 16) {
        fprintf(stderr, "Warning: .sdoft file too small (no header)\n");
        free(data);
        fclose(fp);
        return -1;
    }

    if (memcmp(p, SDOFT_MAGIC, 6) != 0) {
        fprintf(stderr, "Warning: Invalid .sdoft magic (not a libud sdoft file)\n");
        free(data);
        fclose(fp);
        return -1;
    }
    p += 6;  // Skip magic

    p += 2;  // Skip reserved

    uint32_t version;
    memcpy(&version, p, sizeof(uint32_t));
    p += 4;
    if (version != SDOFT_VERSION) {
        fprintf(stderr, "Warning: Unsupported .sdoft version %u (expected %u)\n",
                version, SDOFT_VERSION);
        free(data);
        fclose(fp);
        return -1;
    }

    uint32_t wordcount;
    memcpy(&wordcount, p, sizeof(uint32_t));
    p += 4;

    // Verify wordcount
    if (wordcount == 0) {
        fprintf(stderr, "Warning: .sdoft file has wordcount=0, treating as invalid\n");
        free(data);
        fclose(fp);
        return -1;
    }

    if (index->wordcount == 0) {
        index->wordcount = wordcount;
    } else if (index->wordcount != wordcount) {
        fprintf(stderr, "Warning: .sdoft file wordcount mismatch (expected %u, got %u)\n",
                index->wordcount, wordcount);
        free(data);
        fclose(fp);
        return -1;
    }

    // 2. Calculate page count and pre-allocate
    uint32_t page_count = (wordcount - 1) / ENTRIES_PER_PAGE + 1;
    sd_array_reserve(index->pages, page_count);

    // 3. Read Page Info Array
    for (uint32_t i = 0; i < page_count; i++) {
        sd_dictfile_index_sdoft_page page;

        // Check if enough data to read offset
        if (end - p < (int)sizeof(uint32_t)) {
            fprintf(stderr, "Warning: Failed to read .sdoft page offset\n");
            goto error;
        }

        // Read offset
        memcpy(&page.offset, p, sizeof(uint32_t));
        p += 4;

        // Read first word (NUL-terminated)
        size_t len = strlen(p);
        if (p + len >= end) {
            fprintf(stderr, "Warning: Failed to read .sdoft first_word\n");
            goto error;
        }
        if (len > 0) {
            page.first_word = malloc(len + 1);
            if (!page.first_word) {
                fprintf(stderr, "Error: Memory allocation failed for first_word\n");
                goto error;
            }
            memcpy(page.first_word, p, len + 1);  // +1 copy NUL
        } else {
            page.first_word = NULL;
        }
        p += len + 1;  // Skip string and NUL

        // Read last word (NUL-terminated)
        len = strlen(p);
        if (p + len >= end) {
            fprintf(stderr, "Warning: Failed to read .sdoft last_word\n");
            goto error;
        }
        if (len > 0) {
            page.last_word = malloc(len + 1);
            if (!page.last_word) {
                fprintf(stderr, "Error: Memory allocation failed for last_word\n");
                goto error;
            }
            memcpy(page.last_word, p, len + 1);  // +1 copy NUL
        } else {
            page.last_word = NULL;
        }
        p += len + 1;  // Skip string and NUL

        // Add to cvector
        sd_array_push_back(index->pages, page);
    }

    // 4. Read end_offset
    if (end - p < (int)sizeof(uint32_t)) {
        fprintf(stderr, "Warning: Failed to read .sdoft end_offset\n");
        goto error;
    }
    memcpy(&index->end_offset, p, sizeof(uint32_t));

    free(data);
    fclose(fp);
    index->pages_loaded = true;
    return 0;

error:
    // Clean up allocated memory
    for (size_t i = 0; i < sd_array_size(index->pages); i++) {
        if (index->pages[i].first_word) free(index->pages[i].first_word);
        if (index->pages[i].last_word) free(index->pages[i].last_word);
    }
    sd_array_free(index->pages);
    index->pages = NULL;
    free(data);
    fclose(fp);
    return -1;
}

/**
 * Calculate page info (traverse index file, collect first and last words)
 * @param index Index object
 * @param idx_path Index file path
 */
static int calculate_pages(sd_dictfile_index *index, const char *idx_path) {
    if (!index || !idx_path) {
        return -1;
    }

    FILE *fp = fopen(idx_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open .idx file: %s\n", idx_path);
        return -1;
    }

    SD_STAT_STRUCT st;
    if (sd_stat(idx_path, &st) < 0) {
        fprintf(stderr, "Error: Cannot stat .idx file: %s\n", idx_path);
        fclose(fp);
        return -1;
    }

    if (index->isize > 0 && (uint32_t)st.st_size != index->isize) {
        fprintf(stderr, "Warning: .idx file size mismatch (expected %u, got %lld)\n",
                index->isize, (long long)st.st_size);
    }

    size_t data_size = (size_t)st.st_size;
    char *data = malloc(data_size);
    if (!data) {
        fprintf(stderr, "Error: Memory allocation failed for .idx file\n");
        fclose(fp);
        return -1;
    }

    if (fread(data, 1, data_size, fp) != data_size) {
        fprintf(stderr, "Error: Failed to read .idx file\n");
        free(data);
        fclose(fp);
        return -1;
    }

    // Pre-allocate capacity
    uint32_t expected_pages = index->wordcount > 0
        ? (index->wordcount - 1) / ENTRIES_PER_PAGE + 1
        : 16;
    sd_array_reserve(index->pages, expected_pages);

    uint32_t entry_count = 0;
    uint32_t page_start_offset = 0;
    char *first_word = NULL;
    char *last_word = NULL;

    const char *p = data;
    const char *end = data + data_size;

    while (p < end) {
        uint32_t current_offset = (uint32_t)(p - data);
        uint32_t index_in_page = entry_count % ENTRIES_PER_PAGE;

        // New page starts
        if (index_in_page == 0) {
            page_start_offset = current_offset;
        }

        sd_dictfile_index_entry entry;
        size_t entry_size = index->parse_entry(p, end - p, &entry);
        if (entry_size == 0) break;

        // Record first word
        if (index_in_page == 0) {
            free(first_word);
            first_word = entry.word;  // // Take ownership directly
            entry.word = NULL;
        }

        // Update last word (always keep the latest)
        free(last_word);
        if (entry.word) {
            last_word = entry.word;
            entry.word = NULL;
        } else {
            last_word = strdup(first_word);  // First word is also last word
        }

        p += entry_size;
        entry_count++;

        // Page full, save page info
        if (entry_count % ENTRIES_PER_PAGE == 0) {
            sd_dictfile_index_sdoft_page page = {
                .offset = page_start_offset,
                .first_word = first_word,
                .last_word = last_word
            };
            sd_array_push_back(index->pages, page);
            first_word = NULL;
            last_word = NULL;
        }
    }

    // Save last incomplete page
    if (first_word) {
        sd_dictfile_index_sdoft_page page = {
            .offset = page_start_offset,
            .first_word = first_word,
            .last_word = last_word
        };
        sd_array_push_back(index->pages, page);
    }

    index->end_offset = (uint32_t)data_size;
    if (index->wordcount == 0) {
        index->wordcount = entry_count;
    }
    index->pages_loaded = true;

    free(data);
    fclose(fp);
    return 0;
}
/**
 * // Load page into cache
 */
static int load_page_to_cache(sd_dictfile_index *index, uint32_t page_idx) {
    if (!index || page_idx >= sd_array_size(index->pages)) {
        return -1;
    }

    // If cache already has this page, return directly
    if (index->page_cache.valid && index->page_cache.index == page_idx) {
        return 0;
    }

    // Get current page offset and next page offset
    uint32_t page_offset = index->pages[page_idx].offset;
    uint32_t next_page_offset;

    // Check if this is the last page
    if (page_idx + 1 >= sd_array_size(index->pages)) {
        next_page_offset = index->end_offset;
    } else {
        next_page_offset = index->pages[page_idx + 1].offset;
    }

    uint32_t page_size = next_page_offset - page_offset;

    // Free old cache
    if (index->page_cache.data) {
        free(index->page_cache.data);
    }
    if (index->page_cache.entries) {
        for (uint32_t i = 0; i < index->page_cache.count; i++) {
            if (index->page_cache.entries[i].word) {
                free(index->page_cache.entries[i].word);
            }
        }
        free(index->page_cache.entries);
    }

    // Read page data
    index->page_cache.data = malloc(page_size);
    if (!index->page_cache.data) {
        fprintf(stderr, "Error: Memory allocation failed for page data\n");
        return -1;
    }

    fseek(index->idx_file, page_offset, SEEK_SET);
    size_t read_size = fread(index->page_cache.data, 1, page_size, index->idx_file);
    if (read_size != page_size) {
        fprintf(stderr, "Error: Failed to read page data\n");
        free(index->page_cache.data);
        index->page_cache.data = NULL;
        return -1;
    }

    // Parse entries in single pass (max ENTRIES_PER_PAGE per page)
    const char *p = index->page_cache.data;
    const char *end = p + page_size;

    index->page_cache.entries = calloc(ENTRIES_PER_PAGE, sizeof(sd_dictfile_index_entry));
    if (!index->page_cache.entries) {
        fprintf(stderr, "Error: Memory allocation failed for page entries\n");
        free(index->page_cache.data);
        index->page_cache.data = NULL;
        return -1;
    }

    uint32_t idx = 0;
    while (p < end && idx < ENTRIES_PER_PAGE) {
        size_t entry_size = index->parse_entry(p, end - p, &index->page_cache.entries[idx]);
        if (entry_size == 0) break;

        idx++;
        p += entry_size;
    }

    index->page_cache.index = page_idx;
    index->page_cache.count = idx;
    index->page_cache.valid = true;

    return 0;
}

// ============================================================
// WordList mode implementation (small files)
// ============================================================

static int load_wordlist_mode(sd_dictfile_index *index, const char *idx_path,
                               sd_dictfile_index_entry_parse_fn parse_fn) {
    // Check if compressed file
    bool is_gzipped = strstr(idx_path, ".gz") != NULL;

    char *buffer = NULL;
    long buffer_size = 0;

    if (is_gzipped) {
        // ===== Decompress .gz file using zlib =====
        gzFile gzfp = gzopen(idx_path, "rb");
        if (!gzfp) {
            fprintf(stderr, "Error: Cannot open .gz file: %s\n", idx_path);
            return -1;
        }

        // ===== Three-level priority strategy for buffer size =====
        size_t buffer_capacity = 0;

        // Priority 1: Use externally provided isize
        if (index->isize > 0) {
            buffer_capacity = index->isize;
        }
        // Priority 2: Read ISIZE field at end of .gz file (RFC 1952)
        else {
            uint32_t gzip_isize = read_gzip_isize(idx_path);
            if (gzip_isize > 0) {
                buffer_capacity = gzip_isize;
            }
            // Priority 3: Dynamic growth (starting from 1MB)
            else {
                buffer_capacity = 1024 * 1024;
            }
        }

        buffer = malloc(buffer_capacity);
        if (!buffer) {
            fprintf(stderr, "Error: Memory allocation failed for decompressed data\n");
            gzclose(gzfp);
            return -1;
        }

        // Read and decompress in loop
        int bytes_read;
        size_t total_read = 0;
        while ((bytes_read = gzread(gzfp, buffer + total_read,
                                   (unsigned int)(buffer_capacity - total_read))) > 0) {
            total_read += bytes_read;

            // Buffer insufficient, grow
            if (total_read >= buffer_capacity - 1024) {
                buffer_capacity *= 2;
                char *new_buffer = realloc(buffer, buffer_capacity);
                if (!new_buffer) {
                    fprintf(stderr, "Error: Memory reallocation failed\n");
                    free(buffer);
                    gzclose(gzfp);
                    return -1;
                }
                buffer = new_buffer;
            }
        }

        gzclose(gzfp);

        if (bytes_read < 0) {
            fprintf(stderr, "Error: Failed to decompress .gz file\n");
            free(buffer);
            return -1;
        }

        buffer_size = total_read;

    } else {
        // ===== Read regular file =====
        FILE *fp = fopen(idx_path, "rb");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open index file: %s\n", idx_path);
            return -1;
        }

        // Get file size
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (file_size <= 0) {
            fprintf(stderr, "Error: Invalid file size: %s\n", idx_path);
            fclose(fp);
            return -1;
        }

        buffer = malloc(file_size);
        if (!buffer) {
            fprintf(stderr, "Error: Memory allocation failed for index data\n");
            fclose(fp);
            return -1;
        }

        size_t read_size = fread(buffer, 1, file_size, fp);
        fclose(fp);

        if (read_size != (size_t)file_size) {
            fprintf(stderr, "Error: Failed to read index file\n");
            free(buffer);
            return -1;
        }

        buffer_size = file_size;
    }

    // If wordcount not provided, count and parse entries (dynamic growth)
    uint32_t wordcount = index->wordcount;

    if (wordcount > 0) {
        // ===== Case 1: wordcount known (most common) =====
        // Allocate array directly, fill in one pass
        index->entries = calloc(wordcount, sizeof(sd_dictfile_index_entry));
        if (!index->entries) {
            fprintf(stderr, "Error: Memory allocation failed for entries\n");
            free(buffer);
            return -1;
        }

        // Traverse and parse all entries
        const char *p = buffer;
        const char *end = buffer + buffer_size;
        uint32_t idx = 0;

        while (p < end && idx < wordcount) {
            size_t entry_size = parse_fn(p, end - p, &index->entries[idx]);
            if (entry_size == 0) break;

            idx++;
            p += entry_size;
        }
    } else {
        // ===== Case 2: wordcount unknown (rare) =====
        // Use dynamic growth, complete in one pass

        size_t capacity = 1024;  // Initial capacity: 1024 entries
        sd_dictfile_index_entry *entries = malloc(capacity * sizeof(sd_dictfile_index_entry));
        if (!entries) {
            fprintf(stderr, "Error: Memory allocation failed for entries\n");
            free(buffer);
            return -1;
        }

        uint32_t count = 0;
        const char *p = buffer;
        const char *end = buffer + buffer_size;

        while (p < end) {
            // Check if growth needed
            if (count >= capacity) {
                capacity *= 2;  // // Geometric growth
                sd_dictfile_index_entry *new_entries = realloc(entries, capacity * sizeof(sd_dictfile_index_entry));
                if (!new_entries) {
                    fprintf(stderr, "Error: Memory reallocation failed\n");
                    // Free already parsed words
                    for (uint32_t i = 0; i < count; i++) {
                        if (entries[i].word) free(entries[i].word);
                    }
                    free(entries);
                    free(buffer);
                    return -1;
                }
                entries = new_entries;
            }

            // Parse entry
            size_t entry_size = parse_fn(p, end - p, &entries[count]);
            if (entry_size == 0) break;

            count++;
            p += entry_size;
        }

        // Optional: resize to exact size (eliminate unused space)
        if (capacity > count) {
            sd_dictfile_index_entry *new_entries = realloc(entries, count * sizeof(sd_dictfile_index_entry));
            if (new_entries) {
                entries = new_entries;
                capacity = count;
            }
            // If realloc fails, original is fine (just has unused space)
        }

        index->entries = entries;
        index->wordcount = count;
    }

    free(buffer);
    return 0;
}

// ============================================================
// Offset mode implementation (large files + sdoft)
// ============================================================

static int load_offset_mode(sd_dictfile_index *index, const char *idx_path,
                             uint32_t wordcount) {
    // Save wordcount
    index->wordcount = wordcount;

    // Try loading .sdoft cache
    char *sdoft_path = get_sdoft_cache_path(idx_path);
    bool need_calc_offsets = true;

    if (sdoft_path) {
        if (load_sdoft_cache(index, sdoft_path) == 0) {
            // .sdoft loaded successfully
            need_calc_offsets = false;
        }
        free(sdoft_path);
    }

    // If page offsets need to be calculated
    if (need_calc_offsets) {
        // Calculate page info (internally mmaps file)
        int ret = calculate_pages(index, idx_path);
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to calculate page info\n");
            return -1;
        }

        // Save to .sdoft file
        sdoft_path = get_sdoft_cache_path(idx_path);
        if (sdoft_path) {
            save_sdoft_cache(index, sdoft_path);
            free(sdoft_path);
        }
    }

    // Open FILE* for subsequent fread
    index->idx_file = fopen(idx_path, "rb");
    if (!index->idx_file) {
        fprintf(stderr, "Error: Cannot open .idx file for reading: %s\n",
                idx_path);
        return -1;
    }

    // Allocate page cache
    index->page_cache.entries =
        malloc(sizeof(sd_dictfile_index_entry) * ENTRIES_PER_PAGE);
    if (!index->page_cache.entries) {
        fprintf(stderr, "Warning: Failed to allocate page cache\n");
    } else {
        index->page_cache.valid = false;
    }

    return 0;
}

// ============================================================
// Public API implementation
// ============================================================

sd_dictfile_index *sd_dictfile_index_open(const char *idx_path,
                         sd_dictfile_index_entry_parse_fn parse_fn,
                         uint32_t wordcount,
                         uint32_t isize) {
    if (!idx_path || !parse_fn) {
        fprintf(stderr, "Error: Invalid parameters for dictfile_index_open\n");
        return NULL;
    }

    // Allocate index object
    sd_dictfile_index *index = calloc(1, sizeof(sd_dictfile_index));
    if (!index) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    index->parse_entry = parse_fn;
    index->wordcount = wordcount;  // Save wordcount (if 0, will be counted in load_wordlist_mode)
    index->isize = isize;          // Save isize (if 0, will be auto-detected in load_wordlist_mode)
    index->idx_path = strdup(idx_path);
    if (!index->idx_path) {
        fprintf(stderr, "Error: Memory allocation failed for path\n");
        free(index);
        return NULL;
    }

    // Get file size to determine which mode to use
    SD_STAT_STRUCT st;
    if (sd_stat(idx_path, &st) != 0) {
        fprintf(stderr, "Error: Cannot stat index file: %s\n", idx_path);
        free(index->idx_path);
        free(index);
        return NULL;
    }

    size_t file_size = st.st_size;

    bool is_gzipped = strstr(idx_path, ".gz") != NULL;
    if (is_gzipped || file_size < SMALL_DICT_THRESHOLD) {
        // Small file: WordList mode
        index->mode = DICTFILE_INDEX_WORDLIST;
        if (load_wordlist_mode(index, idx_path, parse_fn) != 0) {
            free(index->idx_path);
            free(index);
            return NULL;
        }
    } else {
        // Large file: Offset mode
        index->mode = DICTFILE_INDEX_OFFSET;
        if (load_offset_mode(index, idx_path, wordcount) != 0) {
            free(index->idx_path);
            free(index);
            return NULL;
        }
    }

    return index;
}

void sd_dictfile_index_close(sd_dictfile_index *index) {
    if (!index) {
        return;
    }

    // WordList mode cleanup
    if (index->mode == DICTFILE_INDEX_WORDLIST) {
        if (index->entries) {
            for (uint32_t i = 0; i < index->wordcount; i++) {
                if (index->entries[i].word) {
                    free(index->entries[i].word);
                }
            }
            free(index->entries);
        }
    }

    // Offset mode cleanup
    if (index->mode == DICTFILE_INDEX_OFFSET) {
        // Clear page cache
        if (index->page_cache.data) {
            free(index->page_cache.data);
        }
        if (index->page_cache.entries) {
            for (uint32_t i = 0; i < index->page_cache.count; i++) {
                if (index->page_cache.entries[i].word) {
                    free(index->page_cache.entries[i].word);
                }
            }
            free(index->page_cache.entries);
        }

        // Free .sdoft page info array
        if (index->pages) {
            for (size_t i = 0; i < sd_array_size(index->pages); i++) {
                if (index->pages[i].first_word) {
                    free(index->pages[i].first_word);
                }
                if (index->pages[i].last_word) {
                    free(index->pages[i].last_word);
                }
            }
            sd_array_free(index->pages);
            index->pages = NULL;
        }

        if (index->idx_file) {
            fclose(index->idx_file);
        }
    }

    if (index->idx_path) {
        free(index->idx_path);
    }

    free(index);
}

uint32_t sd_dictfile_index_get_count(const sd_dictfile_index *index) {
    return index ? index->wordcount : 0;
}

sd_array(sd_dictfile_index_entry) sd_dictfile_index_lookup(sd_dictfile_index *index, const char *word,
                                   bool prefix, uint32_t limit) {
    if (!index || !word) {
        return NULL;
    }

    sd_array(sd_dictfile_index_entry) results = NULL;

    if (index->mode == DICTFILE_INDEX_WORDLIST) {
        // WordList mode: use simple binary search
        sd_dictfile_index_entry first_match_entry;
        uint32_t first_match_index;
        int ret = dict_index_bsearch_left(index, word, 0, index->wordcount - 1,
                                       &first_match_entry, &first_match_index,
                                       prefix);
        if (ret < 0) {
            return NULL; // Not found or failed
        }

        return expand_matches(index, word, first_match_index,
                             &first_match_entry, prefix, limit, false);
    } else if (index->mode == DICTFILE_INDEX_OFFSET) {
        // Offset mode: use two-level binary search
        if (!index->pages_loaded || !index->pages) {
            fprintf(stderr, "Error: Page info not loaded\n");
            return NULL;
        }

        // Level 1: page-level binary search (search in page first words)
        uint32_t target_page;
        int ret = dict_index_page_bsearch_left(index, word, &target_page, prefix);
        if (ret < 0) {
            return NULL;  // Not found or failed
        }

        // Level 2: binary search within target page
        uint32_t start_index = target_page * ENTRIES_PER_PAGE;
        uint32_t end_index = start_index + ENTRIES_PER_PAGE;
        if (end_index > index->wordcount) {
            end_index = index->wordcount;
        }

        sd_dictfile_index_entry first_entry;
        uint32_t first_match;
        ret = dict_index_bsearch_left(index, word, start_index, end_index - 1,
                                   &first_entry, &first_match, prefix);
        if (ret < 0) {
            return NULL; // Not found or failed
        }
        bool need_backtrack = (first_match == start_index);
        return expand_matches(index, word, first_match, &first_entry, prefix, limit,
                             need_backtrack);
    }

    return results;
}

void sd_dictfile_index_free_entries(sd_array(sd_dictfile_index_entry) entries) {
    if (!entries) {
        return;
    }

    // Free word field of each entry (if exists)
    for (size_t i = 0; i < sd_array_size(entries); i++) {
        if (entries[i].word) {
            free(entries[i].word);
            entries[i].word = NULL;
        }
    }

    // Free vector itself
    sd_array_free(entries);
}

const sd_dictfile_index_entry *sd_dictfile_index_get_entry(sd_dictfile_index *index, uint32_t idx) {
    if (!index || idx >= index->wordcount) {
        return NULL;
    }

    if (index->mode == DICTFILE_INDEX_WORDLIST) {
        return &index->entries[idx];
    } else if (index->mode == DICTFILE_INDEX_OFFSET) {
        // Offset mode: get from page cache
        if (!index->page_cache.entries || !index->idx_file) {
            return NULL;
        }

        // Calculate page index
        uint32_t page_idx = idx / ENTRIES_PER_PAGE;

        // Check if cache hit
        if (index->page_cache.valid && index->page_cache.index == page_idx) {
            // Cache hit
            uint32_t offset_in_page = idx % ENTRIES_PER_PAGE;
            if (offset_in_page < index->page_cache.count) {
                return &index->page_cache.entries[offset_in_page];
            }
            return NULL;
        }

        // Cache miss, load page
        if (load_page_to_cache(index, page_idx) != 0) {
            return NULL;
        }

        // Return requested entry
        uint32_t offset_in_page = idx % ENTRIES_PER_PAGE;
        if (offset_in_page < index->page_cache.count) {
            return &index->page_cache.entries[offset_in_page];
        }
        return NULL;
    }

    return NULL;
}

const char *sd_dictfile_index_get_word(sd_dictfile_index *index, uint32_t idx) {
    const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(index, idx);
    return entry ? entry->word : NULL;
}

int sd_dictfile_index_get_data_info(sd_dictfile_index *index, uint32_t idx,
                           uint32_t *offset, uint32_t *size) {
    const sd_dictfile_index_entry *entry = sd_dictfile_index_get_entry(index, idx);
    if (!entry) {
        return -1;
    }

    if (offset) *offset = entry->offset;
    if (size) *size = entry->size;
    return 0;
}

void sd_dictfile_index_entries_sorted_insert(sd_array(sd_dictfile_index_entry) entries,
                                    const sd_dictfile_index_entry *entry) {
    if (!entries || !entry) {
        return;
    }

    size_t size = sd_array_size(entries);
    if (size == 0) {
        // Empty array, add directly
        sd_array_push_back(entries, *entry);
        return;
    }

    // Binary search for insertion position (lower_bound)
    size_t low = 0, high = size;
    while (low < high) {
        size_t mid = low + (high - low) / 2;

        // Compare word first
        int cmp = index_strcmp(entry->word, entries[mid].word, false);

        if (cmp == 0) {
            // word equal, compare offset
            if (entry->offset < entries[mid].offset) {
                high = mid;
            } else if (entry->offset > entries[mid].offset) {
                low = mid + 1;
            } else {
                // Fully equal (both word and offset), insert after (stable)
                low = mid + 1;
                break;
            }
        } else if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    // Insert at low position
    sd_array_insert(entries, low, *entry);
}
