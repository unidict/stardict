//
//  sd_stardict_syn.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_stardict_syn_h
#define sd_stardict_syn_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sd_types.h"

#include "sd_array.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// .syn file handler
// ============================================================
typedef struct sd_stardict_syn {
    char *syn_path;          // .syn file path
    uint32_t word_count;     // Synonym count
    uint32_t file_size;      // File size
    bool loaded;             // Whether loaded

    // Data buffer
    char *data;              // File data buffer (pointed to by words array)

    // Index data (for fast lookup)
    char **words;            // Word pointer array (pointing to data)
    uint32_t *indices;       // Corresponding index array (network byte order converted)

} sd_stardict_syn;

// ============================================================
// .syn file API
// ============================================================

/**
 * Load .syn file
 * @param syn_path .syn file path
 * @param word_count Synonym count (from synwordcount field in .ifo file)
 * @return syn object on success, NULL on failure
 */
sd_stardict_syn *sd_stardict_syn_load(const char *syn_path, uint32_t word_count);

/**
 * Free .syn file object
 * @param syn syn object
 */
void sd_stardict_syn_free(sd_stardict_syn *syn);

/**
 * Look up synonym indices
 * @param syn syn object
 * @param word Synonym to search for
 * @param prefix true=prefix match, false=exact match
 * @param limit Max results (0 = unlimited)
 * @return cvector array with all matching indices (caller must free with sd_array_free)
 * @note Returns NULL on failure, empty vector (size=0) if not found
 */
sd_array(uint32_t) sd_stardict_syn_lookup(const sd_stardict_syn *syn, const char *word,
                                         bool prefix, uint32_t limit);

/**
 * Get synonym count
 */
static inline uint32_t sd_stardict_syn_get_count(const sd_stardict_syn *syn) {
    return syn ? syn->word_count : 0;
}

/**
 * Check if loaded
 */
static inline bool sd_stardict_syn_is_loaded(const sd_stardict_syn *syn) {
    return syn && syn->loaded;
}

#ifdef __cplusplus
}
#endif

#endif /* sd_stardict_syn_h */
