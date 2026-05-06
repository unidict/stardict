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
    size_t count;        // Definition count
    char **definitions;  // Definition string array
} sd_lookup_result;

void sd_suggestion_result_free(sd_suggestion_result *result);
void sd_lookup_result_free(sd_lookup_result *result);

#endif /* sd_types_h */
