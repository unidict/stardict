//
//  sd_stardict_res.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_stardict_res_h
#define sd_stardict_res_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "dictzip.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Resource storage type
// ============================================================
typedef enum {
    UD_STARDICT_RES_TYPE_UNKNOWN = 0,
    UD_STARDICT_RES_TYPE_FILE,     // Files in res/ directory
    UD_STARDICT_RES_TYPE_DATABASE  // res.rifo/ridx/rdic database
} sd_stardict_res_type;

// ============================================================
// Resource index entry (database mode)
// ============================================================
typedef struct {
    char *filename;    // Filename (caller must free)
    uint32_t offset;   // Offset in res.rdic
    uint32_t size;     // Data size
} sd_stardict_res_entry;

// ============================================================
// Resource storage object
// ============================================================
typedef struct sd_stardict_res_store {
    sd_stardict_res_type type;    // Storage type

    // Dictionary directory path
    char *dirname;

    // Resource database mode (res.rifo/ridx/rdic)
    uint32_t filecount;           // File count
    sd_stardict_res_entry *entries;  // Sorted index entry array

    // Database file handle
    FILE *rdic_file;              // res.rdic file handle (regular file)
    dictzip *dz;                // res.rdic.dz compressed file handler

    // Internal state
    bool loaded;
} sd_stardict_res;

// ============================================================
// Resource storage API
// ============================================================

/**
 * Load resource storage
 * @param dirname Dictionary directory path (containing res/ or res.rifo)
 * @return Resource storage object on success, NULL on failure
 *
 * Loading strategy:
 * 1. Try loading res.rifo first (database mode)
 * 2. If failed, try res/ directory (file mode)
 * 3. Return NULL if both fail
 */
sd_stardict_res *sd_stardict_res_store_load(const char *dirname);

/**
 * Free resource storage object
 * @param store Resource storage object
 */
void sd_stardict_res_store_free(sd_stardict_res *store);

/**
 * Check if file exists
 * @param store Resource storage object
 * @param filename Filename (use '/' as directory separator)
 * @return true if exists, false otherwise
 */
bool sd_stardict_res_store_have_file(const sd_stardict_res *store,
                                      const char *filename);

/**
 * Get storage type
 * @param store Resource storage object
 * @return Storage type
 */
static inline sd_stardict_res_type sd_stardict_res_store_get_type(
    const sd_stardict_res *store) {
    return store ? store->type : UD_STARDICT_RES_TYPE_UNKNOWN;
}

/**
 * Get file count (database mode only)
 * @param store Resource storage object
 * @return File count, 0 on failure
 */
static inline uint32_t sd_stardict_res_store_get_count(
    const sd_stardict_res *store) {
    return store ? store->filecount : 0;
}

/**
 * Read resource file data (database mode only)
 * @param store Resource storage object
 * @param filename Filename
 * @param data_out Output data pointer (caller must free)
 * @param size_out Output data size
 * @return 0 on success, -1 on failure
 *
 * Note:
 * - This function only works in database mode
 * - For file mode, use dirname + "res/" to construct file path
 */
int sd_stardict_res_store_read_file(const sd_stardict_res *store,
                                      const char *filename,
                                      char **data_out,
                                      uint32_t *size_out);

#ifdef __cplusplus
}
#endif

#endif /* sd_stardict_res_h */
