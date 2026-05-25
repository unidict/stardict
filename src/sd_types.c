//
//  sd_types.c
//  stardict
//
//  Created by kejinlu on 2026-04-08
//
#include "sd_types.h"
#include "sd_dictfile_index.h"
#include <stdlib.h>
#include <string.h>

void sd_suggestion_result_free(sd_suggestion_result *result) {
    if (!result) return;

    if (result->entries) {
        for (size_t i = 0; i < result->count; i++) {
            if (result->entries[i]) {
                free(result->entries[i]->word);
                free(result->entries[i]);  // sd_dictfile_index_entry is malloc allocated
            }
        }
        free(result->entries);
    }
    free(result);
}

void sd_lookup_result_free(sd_lookup_result *result) {
    if (!result) return;

    if (result->entries) {
        for (size_t i = 0; i < result->count; i++) {
            free(result->entries[i].word);
            free(result->entries[i].definition);
        }
        free(result->entries);
    }
    free(result);
}
