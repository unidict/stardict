//
//  sd_dictfile.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_dictfile_h
#define sd_dictfile_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "dictzip.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Data block structure
// ============================================================
typedef struct {
    char *data;       // Data content (needs free)
    uint32_t size;    // Data size
} sd_dictfile_data_block;

// ============================================================
// Data file reader
// ============================================================
typedef struct {
    // File info
    char *filepath;          // .dict or .dict.dz file path
    bool is_compressed;       // Whether it is a .dict.dz compressed file
    FILE *fp;          // Regular file handle
    dictzip *dz;            // .dict.dz compressed file handler (for compressed files only)

    // Last read block (reused across calls)
    sd_dictfile_data_block last_block;

    // sametypesequence (StarDict supported, optional)
    char *sametypesequence;

} sd_dictfile;

// ============================================================
// Data read API
// ============================================================

/**
 * Open data file
 * @param filepath .dict or .dict.dz file path
 * @param sametypesequence sametypesequence string (optional, pass NULL to disable)
 * @return Data reader object on success, NULL on failure
 */
sd_dictfile *sd_dictfile_open(const char *filepath, const char *sametypesequence);

/**
 * Close data file
 * @param df dictfile object
 */
void sd_dictfile_close(sd_dictfile *df);

/**
 * Read data block
 * @param df dictfile object
 * @param offset Data offset
 * @param size Data size
 * @return Data block pointer on success, NULL on failure
 * @note The returned pointer points to internal cache, caller must not free it.
 *       The pointer is only valid until the next call to sd_dictfile_read.
 */
sd_dictfile_data_block *sd_dictfile_read(sd_dictfile *df,
                           uint32_t offset, uint32_t size);

/**
 * Convert data to string (parse sametypesequence)
 * @param df Data reader object
 * @param raw_data Raw data
 * @param raw_size Raw data size
 * @param result Output string (caller must free)
 * @return 0 on success, error code on failure
 */
int sd_dictfile_data_to_string(sd_dictfile *df,
                        const char *raw_data, uint32_t raw_size,
                        char **result);

#ifdef __cplusplus
}
#endif

#endif /* sd_dictfile_h */
