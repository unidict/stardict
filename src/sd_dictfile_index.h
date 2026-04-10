//
//  sd_dictfile_index.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_dictfile_index_h
#define sd_dictfile_index_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "sd_types.h"
#include "sd_array.h"


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Index entry structure (generic)
// ============================================================
typedef struct sd_dictfile_index_entry {
    char *word;             // Word string (malloc allocated)
    uint32_t offset;        // Offset in .dict/.dict.dz file
    uint32_t size;          // Data size
} sd_dictfile_index_entry;

// ============================================================
// Entry parser interface (format-specific)
// ============================================================
/**
 * Entry parse function type
 * @param data Index data pointer
 * @param data_size Available data size
 * @param entry Output: parsed entry
 * @return Bytes consumed by entry on success (for skipping to next), 0 on failure
 */
typedef size_t (*sd_dictfile_index_entry_parse_fn)(const char *data, size_t data_size, sd_dictfile_index_entry *entry);

// ============================================================
// Opaque index object
// ============================================================
typedef struct sd_dictfile_index sd_dictfile_index;

// ============================================================
// Public API
// ============================================================

/**
 * Load index file
 * @param idx_path Index file path (supports .idx.gz)
 * @param parse_fn Format-specific parse function
 * @param wordcount Word count (pass 0 for auto-detect if unknown)
 * @param isize Decompressed index file size (pass 0 for auto-detect if unknown)
 * @return Index object on success, NULL on failure
 *
 * Loading strategy:
 * - File < 100KB: WordList mode (fully loaded into memory)
 * - File >= 100KB: Offset mode (page-based paging)
 */
sd_dictfile_index *sd_dictfile_index_open(const char *idx_path,
                            sd_dictfile_index_entry_parse_fn parse_fn,
                            uint32_t wordcount,
                            uint32_t isize);

/**
 * Free index file
 * @param index Index object
 */
void sd_dictfile_index_close(sd_dictfile_index *index);

/**
 * Get entry count
 * @param index Index object
 * @return Entry count, 0 on failure
 */
uint32_t sd_dictfile_index_get_count(const sd_dictfile_index *index);

/**
 * Search (binary search)
 * @param index Index object
 * @param word Word to search for
 * @param prefix true=prefix match, false=exact match
 * @param limit Max results (0 = unlimited)
 * @return cvector array with all matching entries (caller must call sd_dictfile_index_free_entries)
 * @note Returns NULL on failure, empty vector (size=0) if not found
 */
sd_array(sd_dictfile_index_entry) sd_dictfile_index_lookup(sd_dictfile_index *index, const char *word,
                                      bool prefix, uint32_t limit);

/**
 * Free cvector returned by sd_dictfile_index_lookup
 * @param entries cvector array
 * @note Automatically frees all entry words, then frees the vector itself
 */
void sd_dictfile_index_free_entries(sd_array(sd_dictfile_index_entry) entries);

/**
 * Get entry (by index number)
 * @param index Index object
 * @param idx Index number (0 to wordcount-1)
 * @return Entry pointer on success, NULL on failure
 * @note Pointer is owned by index, valid until index is closed or next get_entry call in Offset mode
 */
const sd_dictfile_index_entry *sd_dictfile_index_get_entry(sd_dictfile_index *index, uint32_t idx);

/**
 * Get word (by index number)
 * @param index Index object
 * @param idx Index number
 * @return Word string on success, NULL on failure
 */
const char *sd_dictfile_index_get_word(sd_dictfile_index *index, uint32_t idx);

/**
 * Get data offset and size (by index number)
 * @param index Index object
 * @param idx Index number
 * @param offset Output data offset
 * @param size Output data size
 * @return 0 on success, -1 on failure
 */
int sd_dictfile_index_get_data_info(sd_dictfile_index *index, uint32_t idx,
                              uint32_t *offset, uint32_t *size);

/**
 * Insert entry into sorted cvector in order
 * @param entries cvector array (must already be sorted by word+offset)
 * @param entry Entry to insert
 * @note Sort order: compare word first, then offset if equal.
 *       If both word and offset are equal, insert after (stable).
 */
void sd_dictfile_index_entries_sorted_insert(sd_array(sd_dictfile_index_entry) entries,
                                       const sd_dictfile_index_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* sd_dictfile_index_h */
