//
//  sd_dictzip.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_dictzip_h
#define sd_dictzip_h

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// dictzip file handle (opaque pointer)
// ============================================================

typedef struct sd_dictzip sd_dictzip;

// ============================================================
// API functions
// ============================================================

/**
 * Open dictzip file
 * @param filename File path
 * @return dictzip handle on success, NULL on failure
 */
sd_dictzip *sd_dictzip_open(const char *filename);

/**
 * Close dictzip file
 * @param dz dictzip handle
 */
void sd_dictzip_close(sd_dictzip *dz);

/**
 * Read and decompress data
 * @param dz dictzip handle
 * @param offset Data offset (relative to decompressed data)
 * @param size Data size
 * @param out_size Output actual bytes read
 * @return Decompressed data pointer on success (caller must free), NULL on failure
 */
unsigned char *sd_dictzip_read(sd_dictzip *dz, uint32_t offset, uint32_t size,
                            uint32_t *out_size);

/**
 * Get total chunk count
 * @param dz dictzip handle
 * @return Chunk count, -1 on failure
 */
int sd_dictzip_get_chunk_count(sd_dictzip *dz);

/**
 * Get chunk size (uncompressed size per chunk)
 * @param dz dictzip handle
 * @return Chunk size, 0 on failure
 */
int sd_dictzip_get_chunk_size(sd_dictzip *dz);

/**
 * Get uncompressed file size
 * @param dz dictzip handle
 * @return Uncompressed file size
 */
uint32_t sd_dictzip_get_uncompressed_size(sd_dictzip *dz);

/**
 * Set cache size
 * @param dz dictzip handle
 * @param capacity Cache capacity (in chunk count)
 * @return 0 on success, -1 on failure
 */
int sd_dictzip_set_cache_size(sd_dictzip *dz, int capacity);

/**
 * Clear cache
 * @param dz dictzip handle
 */
void sd_dictzip_clear_cache(sd_dictzip *dz);

#ifdef __cplusplus
}
#endif

#endif /* sd_dictzip_h */
