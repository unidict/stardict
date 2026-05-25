//
//  sd_types.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_types_h
#define sd_types_h
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// Forward declaration (full definition in sd_dictfile_index.h)
typedef struct sd_dictfile_index_entry sd_dictfile_index_entry;

// ============================================================
// Generic dictionary result types
// ============================================================

typedef struct {
    size_t count;                                // Suggestion count
    struct sd_dictfile_index_entry **entries; // Suggestion array
} sd_suggestion_result;

typedef struct {
    char *word;        // Original headword from index entry
    char *definition;  // Definition text
} sd_lookup_entry;

typedef struct {
    size_t count;
    sd_lookup_entry *entries;  // Result array (word + definition pairs)
} sd_lookup_result;

void sd_suggestion_result_free(sd_suggestion_result *result);
void sd_lookup_result_free(sd_lookup_result *result);

#endif /* sd_types_h */
