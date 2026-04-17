//
//  sd_dictfile.c
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#include "sd_dictfile.h"
#include "sd_port.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================
// Helper functions
// ============================================================

/**
 * Read 32-bit integer (little-endian)
 */
static inline uint32_t get_uint32(const char *addr) {
    uint32_t result;
    memcpy(&result, addr, sizeof(uint32_t));
    return result;
}

// ============================================================
// Public API implementation
// ============================================================

sd_dictfile *sd_dictfile_open(const char *filepath, const char *sametypesequence) {
    if (!filepath) {
        fprintf(stderr, "Error: Invalid parameters for dictfile_load\n");
        return NULL;
    }

    // Create data reader object
    sd_dictfile *dict = calloc(1, sizeof(sd_dictfile));
    if (!dict) {
        fprintf(stderr, "Error: Memory allocation failed for dictfile_t\n");
        return NULL;
    }

    dict->filepath = strdup(filepath);
    dict->sametypesequence = sametypesequence ? strdup(sametypesequence) : NULL;

    // Check if compressed file
    size_t path_len = strlen(filepath);
    if (path_len > 3 && strcmp(filepath + path_len - 3, ".dz") == 0) {
        dict->is_compressed = true;

        // Open dictzip compressed file
        dict->dz = dictzip_open(filepath);
        if (!dict->dz) {
            fprintf(stderr, "Error: Failed to open .dict.dz file\n");
            sd_dictfile_close(dict);
            return NULL;
        }

        return dict;
    } else {
        // Regular file
        dict->is_compressed = false;

        // Open file
        dict->fp = fopen(filepath, "rb");
        if (!dict->fp) {
            fprintf(stderr, "Error: Cannot open .dict file: %s\n", filepath);
            sd_dictfile_close(dict);
            return NULL;
        }

        return dict;
    }
}

void sd_dictfile_close(sd_dictfile *df) {
    if (!df) {
        return;
    }

    // Close regular file
    if (df->fp) {
        fclose(df->fp);
        df->fp = NULL;
    }

    // Free last read block
    free(df->last_block.data);

    // Free compressed file resources
    if (df->dz) {
        dictzip_close(df->dz);
        df->dz = NULL;
    }

    if (df->filepath) {
        free(df->filepath);
    }

    if (df->sametypesequence) {
        free(df->sametypesequence);
    }

    free(df);
}

sd_dictfile_data_block *sd_dictfile_read(sd_dictfile *df,
                           uint32_t offset, uint32_t size) {
    if (!df) {
        return NULL;
    }

    // Free previous read
    free(df->last_block.data);
    df->last_block.data = NULL;
    df->last_block.size = 0;

    if (df->is_compressed) {
        // === Compressed file read ===
        // Note: out_size may be less than size if offset+size exceeds
        // the uncompressed file size (dictzip clamps internally)
        uint32_t out_size;
        unsigned char *data = dictzip_read(df->dz, offset, size, &out_size);
        if (!data) {
            fprintf(stderr, "Error: Failed to read decompressed data\n");
            return NULL;
        }
        df->last_block.data = (char *)data;
        df->last_block.size = out_size;
    } else {
        // === Regular file read ===
        if (fseek(df->fp, offset, SEEK_SET) != 0) {
            fprintf(stderr, "Error: Failed to seek to offset %u\n", offset);
            return NULL;
        }

        char *data = malloc(size);
        if (!data) {
            fprintf(stderr, "Error: Memory allocation failed for data block\n");
            return NULL;
        }

        size_t read_size = fread(data, 1, size, df->fp);
        if (read_size != size) {
            fprintf(stderr, "Error: Failed to read data block (read %zu, expected %u)\n",
                    read_size, size);
            free(data);
            return NULL;
        }
        df->last_block.data = data;
        df->last_block.size = size;
    }

    return &df->last_block;
}

int sd_dictfile_data_to_string(sd_dictfile *df,
                        const char *raw_data, uint32_t raw_size,
                        char **result) {
    if (!df || !raw_data || !result) {
        return -1;
    }

    *result = NULL;

    const char *sametypesequence = df->sametypesequence;

    // If no sametypesequence, return raw data directly
    if (!sametypesequence || sametypesequence[0] == '\0') {
        // Parse mixed type data
        const char *p = raw_data;
        const char *end = raw_data + raw_size;

        // Find first 'm' or 't' type (plain text or HTML)
        while (p < end) {
            char type = *p++;

            if (type == 'm' || type == 't' || type == 'y' || type == 'l' ||
                type == 'g' || type == 'x' || type == 'k') {
                // NUL-terminated string
                const char *nul = memchr(p, '\0', end - p);
                if (!nul) {
                    // No NUL found, treat remaining as string
                    *result = strndup(p, end - p);
                    return 0;
                }
                *result = strndup(p, nul - p);
                return 0;
            } else if (type == 'h' || type == 'W' || type == 'P') {
                // 32-bit length + data
                if (p + 4 > end) {
                    break;
                }
                uint32_t sec_size = get_uint32(p);
                p += 4;
                if (p + sec_size > end) {
                    sec_size = (uint32_t)(end - p);
                }
                *result = strndup(p, sec_size);
                return 0;
            } else {
                // Skip unknown type
                if (isupper(type)) {
                    if (p + 4 > end) {
                        break;
                    }
                    uint32_t sec_size = get_uint32(p) + 4;
                    if (p + sec_size > end) {
                        break;
                    }
                    p += sec_size;
                } else {
                    const char *nul = memchr(p, '\0', end - p);
                    if (!nul) {
                        break;
                    }
                    p = nul + 1;
                }
            }
        }

        // No text type found, return entire data
        *result = strndup(raw_data, raw_size);
        return 0;
    }

    // Has sametypesequence, parse it
    size_t seq_len = strlen(sametypesequence);
    const char *p = raw_data;
    const char *end = raw_data + raw_size;

    // Find first text type
    for (size_t i = 0; i < seq_len; i++) {
        char type = sametypesequence[i];

        if (type == 'm' || type == 't' || type == 'l' || type == 'g' ||
            type == 'x' || type == 'y' || type == 'k') {
            // Plain text type
            if (i < seq_len - 1) {
                // Middle item, NUL-terminated
                const char *nul = memchr(p, '\0', end - p);
                if (!nul) {
                    break;
                }
                *result = strndup(p, nul - p);
            } else {
                // Last item, may not be NUL-terminated
                size_t remaining = (size_t)(end - p);
                *result = strndup(p, remaining);
            }
            return 0;
        } else if (type == 'h' || type == 'W' || type == 'P') {
            // Length-prefixed type
            if (p + 4 > end) {
                break;
            }
            uint32_t sec_size = get_uint32(p);
            p += 4;
            if (p + sec_size > end) {
                sec_size = (uint32_t)(end - p);
            }
            *result = strndup(p, sec_size);
            return 0;
        } else {
            // Skip this item
            if (i < seq_len - 1) {
                if (isupper(type)) {
                    if (p + 4 > end) {
                        break;
                    }
                    uint32_t sec_size = get_uint32(p) + 4;
                    if (p + sec_size > end) {
                        break;
                    }
                    p += sec_size;
                } else {
                    const char *nul = memchr(p, '\0', end - p);
                    if (!nul) {
                        break;
                    }
                    p = nul + 1;
                }
            } else {
                // Last item, skip to end
                p = end;
            }
        }
    }

    // No suitable type found, return entire data
    *result = strndup(raw_data, raw_size);
    return 0;
}
