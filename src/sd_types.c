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

void sd_index_entry_array_free(sd_index_entry_array *result) {
    if (!result) return;

    if (result->items) {
        for (size_t i = 0; i < result->count; i++) {
            if (result->items[i]) {
                free(result->items[i]->word);
                free(result->items[i]);  // sd_dictfile_index_entry is malloc allocated
            }
        }
        free(result->items);
    }
    free(result);
}

void sd_data_entry_array_free(sd_data_entry_array *result) {
    if (!result) return;

    if (result->items) {
        for (size_t i = 0; i < result->count; i++) {
            free(result->items[i].word);
            free(result->items[i].definition);
        }
        free(result->items);
    }
    free(result);
}
