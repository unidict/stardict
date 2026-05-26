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
// Status codes
// ============================================================

typedef enum {
    SD_OK                = 0,
    SD_NOT_FOUND         = 1,
    SD_DONE              = 2,

    SD_ERR_INVALID_PARAM = -1,
    SD_ERR_IO            = -2,
    SD_ERR_MEMORY        = -3,
    SD_ERR_FORMAT        = -4,
    SD_ERR_INTERNAL      = -5,
    SD_ERR_STATE         = -6,
} sd_status;

const char *sd_strerror(sd_status status);

// ============================================================
// Result types
// ============================================================

typedef struct {
    size_t count;                                // Entry count
    struct sd_dictfile_index_entry **items;      // Index entry array
} sd_index_entry_array;

typedef struct {
    char *word;        // Original headword from index entry
    char *definition;  // Definition text
} sd_data_entry;

typedef struct {
    size_t count;
    sd_data_entry *items;  // Data entry array
} sd_data_entry_array;

void sd_index_entry_array_free(sd_index_entry_array *result);
void sd_data_entry_array_free(sd_data_entry_array *result);

#endif /* sd_types_h */
