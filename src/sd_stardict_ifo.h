//
//  sd_stardict_ifo.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_stardict_ifo_h
#define sd_stardict_ifo_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// StarDict .ifo file info structure
// ============================================================
typedef struct sd_stardict_ifo {
    // Basic metadata
    char *ifo_file_name;       // .ifo file path
    uint32_t wordcount;        // Word count
    uint32_t syn_wordcount;    // Synonym count (0 means no synonym file)
    char *bookname;            // Dictionary name
    char *author;              // Author
    char *email;               // Email
    char *website;             // Website
    char *date;                // Date
    char *description;         // Description
    uint32_t index_file_size;  // .idx file size
    uint32_t syn_file_size;    // .syn file size (0 means not available)
    char *sametypesequence;    // Data type sequence

    // Internal fields
    bool loaded;               // Whether loaded
} sd_stardict_ifo;

// ============================================================
// .ifo file parser API
// ============================================================

/**
 * Create .ifo parser and load file
 * @param ifo_filename .ifo file path
 * @return Parser object on success, NULL on failure
 */
sd_stardict_ifo *sd_stardict_ifo_load(const char *ifo_filename);

/**
 * Free .ifo parser
 * @param ifo Parser object
 */
void sd_stardict_ifo_free(sd_stardict_ifo *ifo);

/**
 * Check if synonym file exists
 * @param ifo Parser object
 * @return true if synonyms exist, false otherwise
 */
static inline bool sd_stardict_ifo_has_synonym(const sd_stardict_ifo *ifo) {
    return ifo && ifo->syn_wordcount > 0;
}

/**
 * Check if sametypesequence is defined
 * @param ifo Parser object
 * @return true if defined, false otherwise
 */
static inline bool sd_stardict_ifo_has_sametypesequence(const sd_stardict_ifo *ifo) {
    return ifo && ifo->sametypesequence != NULL && ifo->sametypesequence[0] != '\0';
}

/**
 * Check if searchable data is included (m, l, g, x, t, y types)
 * @param ifo Parser object
 * @return true if searchable, false otherwise
 */
bool sd_stardict_ifo_can_search_data(const sd_stardict_ifo *ifo);

#ifdef __cplusplus
}
#endif

#endif /* sd_stardict_ifo_h */
