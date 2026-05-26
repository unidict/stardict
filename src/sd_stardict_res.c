//
//  sd_stardict_res.c
//  stardict
//
//  Created by kejinlu on 2026-04-08
//


#include "sd_stardict_res.h"
#include "sd_types.h"
#include "sd_port.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>  // for gzopen, gzread, gzclose

#ifdef _WIN32
#include <windows.h>
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

// ============================================================
// Internal constants
// ============================================================
#define STAR_DICT_RES_MAGIC "StarDict's storage ifo file"
#define STAR_DICT_RES_VERSION "3.0.0"

// ============================================================
// Helper functions
// ============================================================

/**
 * Build file path
 */
static char *build_path(const char *dir, const char *file) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    size_t len = dir_len + 1 + file_len + 1;

    // Check if separator is needed
    int need_separator = 1;
    if (dir_len > 0 && (dir[dir_len - 1] == '/' || dir[dir_len - 1] == '\\')) {
        need_separator = 0;
        len--;
    }

    char *path = (char *)malloc(len);
    if (!path) {
        return NULL;
    }

    snprintf(path, len, "%s%s%s", dir,
             need_separator ? PATH_SEPARATOR : "", file);
    return path;
}

/**
 * Check if file exists
 */
static bool file_exists(const char *path) {
    SD_STAT_STRUCT st;
    return (sd_stat(path, &st) == 0);
}

/**
 * Check if is directory
 */
static bool is_directory(const char *path) {
    SD_STAT_STRUCT st;
    return (sd_stat(path, &st) == 0 && SD_ISDIR(st.st_mode));
}

/**
 * Binary search file index (by filename)
 */
static int binary_search_entries(const sd_stardict_res_entry *entries,
                                   uint32_t count,
                                   const char *filename) {
    int low = 0;
    int high = count - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(filename, entries[mid].filename);

        if (cmp == 0) {
            return mid;  // Found
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    return -1;  // Not found
}

/**
 * Read key-value from ifo file
 */
static char *ifo_get_value(const char *ifo_content, const char *key) {
    const char *p = ifo_content;
    size_t key_len = strlen(key);

    while (p && *p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }

        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;  // Skip "key="

            // Find end of line
            const char *end = p;
            while (*end && *end != '\r' && *end != '\n') {
                end++;
            }

            // Copy value
            size_t len = end - p;
            char *value = (char *)malloc(len + 1);
            if (value) {
                memcpy(value, p, len);
                value[len] = '\0';

                // Trim trailing whitespace
                while (len > 0 && (value[len - 1] == ' ' ||
                                  value[len - 1] == '\t')) {
                    value[--len] = '\0';
                }
            }
            return value;
        }

        // Skip to next line
        while (*p && *p != '\r' && *p != '\n') {
            p++;
        }
        while (*p == '\r' || *p == '\n') {
            p++;
        }
    }

    return NULL;
}

// ============================================================
// Resource database implementation
// ============================================================

/**
 * Load res.rifo file
 * @param rifo_path res.rifo file path
 * @param store Resource storage object
 * @return 0 on success, -1 on failure
 */
static int load_rifo_file(const char *rifo_path, sd_stardict_res *store) {
    // Read file content
    FILE *fp = fopen(rifo_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open res.rifo file: %s (%s)\n",
                rifo_path, strerror(errno));
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "Error: Invalid res.rifo file size\n");
        fclose(fp);
        return -1;
    }

    // Read file content
    char *content = (char *)malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Error: Memory allocation failed for res.rifo\n");
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(content, 1, file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read res.rifo file\n");
        free(content);
        return -1;
    }
    content[file_size] = '\0';

    // Check magic
    if (!strstr(content, STAR_DICT_RES_MAGIC)) {
        fprintf(stderr, "Error: Invalid res.rifo file format (missing magic)\n");
        free(content);
        return -1;
    }

    // Parse filecount
    char *value = ifo_get_value(content, "filecount");
    if (!value) {
        fprintf(stderr, "Error: res.rifo missing filecount field\n");
        free(content);
        return -1;
    }

    store->filecount = (uint32_t)atoi(value);
    free(value);

    if (store->filecount == 0) {
        fprintf(stderr, "Error: res.rifo filecount is zero\n");
        free(content);
        return -1;
    }

    // Parse ridxfilesize (used for decompression buffer sizing)
    char *ridx_size_str = ifo_get_value(content, "ridxfilesize");
    if (ridx_size_str) {
        store->ridx_filesize = (uint32_t)atoi(ridx_size_str);
        free(ridx_size_str);
    }

    free(content);
    return 0;
}

// ============================================================
// ridx parsing (internal helper)
// ============================================================

/**
 * Parse ridx data
 * @param data Data pointer
 * @param file_size Data size
 * @param store Resource storage object
 * @return 0 on success, -1 on failure
 */
static int parse_ridx_data(const char *data, size_t file_size,
                           sd_stardict_res *store) {
    // Allocate entry array
    store->entries = (sd_stardict_res_entry *)calloc(store->filecount,
                                                      sizeof(sd_stardict_res_entry));
    if (!store->entries) {
        fprintf(stderr, "Error: Memory allocation failed for entries\n");
        return -1;
    }

    const char *p = data;
    const char *end = data + file_size;
    uint32_t count = 0;
    const char *prev_filename = NULL;

    while (p < end && count < store->filecount) {
        // Read filename (NUL-terminated)
        size_t filename_len = strlen(p);
        if (filename_len == 0) {
            fprintf(stderr, "Warning: Empty filename in res.ridx\n");
            p++;
            continue;
        }

        store->entries[count].filename = strdup(p);
        p += filename_len + 1;

        // Check sorting
        if (prev_filename) {
            int cmp = strcmp(prev_filename, store->entries[count].filename);
            if (cmp > 0) {
                fprintf(stderr, "Warning: res.ridx not sorted: '%s' > '%s'\n",
                        prev_filename, store->entries[count].filename);
            } else if (cmp == 0) {
                fprintf(stderr, "Warning: Duplicate filename: '%s'\n",
                        store->entries[count].filename);
            }
        }

        // Check data integrity
        if (p + 2 * sizeof(uint32_t) > end) {
            fprintf(stderr, "Error: Truncated res.ridx file\n");
            break;
        }

        // Read offset (network byte order)
        uint32_t net_offset;
        memcpy(&net_offset, p, sizeof(uint32_t));
        store->entries[count].offset = ntohl(net_offset);
        p += sizeof(uint32_t);

        // Read size (network byte order)
        uint32_t net_size;
        memcpy(&net_size, p, sizeof(uint32_t));
        store->entries[count].size = ntohl(net_size);
        p += sizeof(uint32_t);

        if (store->entries[count].size == 0) {
            fprintf(stderr, "Warning: Zero size entry: '%s'\n",
                    store->entries[count].filename);
        }

        prev_filename = store->entries[count].filename;
        count++;
    }

    if (count != store->filecount) {
        fprintf(stderr, "Warning: res.ridx count mismatch (expected %u, got %u)\n",
                store->filecount, count);
        store->filecount = count;
    }

    return 0;
}

/**
 * Load and parse from .ridx.gz
 */
static int load_ridx_gz(const char *dirname, sd_stardict_res *store) {
    char *ridx_gz_path = build_path(dirname, "res.ridx.gz");
    if (!ridx_gz_path) {
        return -1;
    }

    if (!file_exists(ridx_gz_path)) {
        free(ridx_gz_path);
        return -1;
    }

    printf("Found res.ridx.gz, decompressing...\n");

    gzFile gzf = gzopen(ridx_gz_path, "rb");
    if (!gzf) {
        fprintf(stderr, "Error: Cannot open res.ridx.gz: %s\n", ridx_gz_path);
        free(ridx_gz_path);
        return -1;
    }
    free(ridx_gz_path);

    // Determine buffer size from rifo metadata if available
    size_t buffer_size = store->ridx_filesize > 0 ? store->ridx_filesize : 0;
    size_t buffer_capacity = buffer_size > 0 ? buffer_size : 65536;
    size_t total_read = 0;

    char *buffer = (char *)malloc(buffer_capacity);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        gzclose(gzf);
        return -1;
    }

    // Streaming decompression loop
    for (;;) {
        int chunk = gzread(gzf, buffer + total_read,
                           (unsigned)(buffer_capacity - total_read));
        if (chunk < 0) {
            fprintf(stderr, "Error: Failed to decompress res.ridx.gz\n");
            free(buffer);
            gzclose(gzf);
            return -1;
        }
        if (chunk == 0) break;

        total_read += chunk;

        // Grow buffer if needed (only when ridx_filesize was unknown)
        if (total_read == buffer_capacity) {
            size_t new_cap = buffer_capacity * 2;
            char *new_buf = (char *)realloc(buffer, new_cap);
            if (!new_buf) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                free(buffer);
                gzclose(gzf);
                return -1;
            }
            buffer = new_buf;
            buffer_capacity = new_cap;
        }
    }

    gzclose(gzf);

    if (total_read == 0) {
        fprintf(stderr, "Error: res.ridx.gz is empty\n");
        free(buffer);
        return -1;
    }

    printf("Decompressed: %zu bytes\n", total_read);

    // Parse
    int result = parse_ridx_data(buffer, total_read, store);
    free(buffer);  // Free decompression buffer immediately

    if (result == 0) {
        printf("Loaded %u resource entries from res.ridx.gz\n", store->filecount);
    }

    return result;
}

/**
 * Load and parse from .ridx
 */
static int load_ridx_normal(const char *ridx_path, sd_stardict_res *store) {
    FILE *fp = fopen(ridx_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open res.ridx: %s\n", ridx_path);
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "Error: Invalid res.ridx size\n");
        fclose(fp);
        return -1;
    }

    // Read entire file
    char *buffer = (char *)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, size, fp);
    fclose(fp);

    if (read_size != (size_t)size) {
        fprintf(stderr, "Error: Failed to read res.ridx\n");
        free(buffer);
        return -1;
    }

    // Parse
    int result = parse_ridx_data(buffer, read_size, store);
    free(buffer);  // Free read buffer immediately

    if (result == 0) {
        printf("Loaded %u resource entries from res.ridx\n", store->filecount);
    }

    return result;
}

/**
 * Load resource database (res.rifo/ridx/rdic)
 */
static int load_resource_database(sd_stardict_res *store,
                                   const char *dirname) {
    // Build file paths
    char *rifo_path = build_path(dirname, "res.rifo");
    char *ridx_path = build_path(dirname, "res.ridx");

    // Check if rifo file exists
    if (!file_exists(rifo_path)) {
        free(rifo_path);
        free(ridx_path);
        return -1;  // Database does not exist
    }

    // Load rifo
    if (load_rifo_file(rifo_path, store) != 0) {
        free(rifo_path);
        free(ridx_path);
        return -1;
    }
    free(rifo_path);  // Free immediately after use

    // Load ridx (auto-select compressed or regular file)
    int ridx_result = load_ridx_gz(dirname, store);
    if (ridx_result != 0) {
        // Try regular .ridx
        ridx_result = load_ridx_normal(ridx_path, store);
    }
    free(ridx_path);  // Free immediately after use

    if (ridx_result != 0) {
        return -1;
    }

    // Open rdic file (supports .rdic.dz)
    char *rdic_dz_path = build_path(dirname, "res.rdic.dz");
    if (file_exists(rdic_dz_path)) {
        // Use .rdic.dz
        store->dz = dictzip_open(rdic_dz_path);
        if (!store->dz) {
            fprintf(stderr, "Error: Cannot open res.rdic.dz: %s\n", rdic_dz_path);
            free(rdic_dz_path);
            return -1;
        }
        printf("Loaded res.rdic.dz (compressed, random access supported)\n");
        free(rdic_dz_path);
    } else {
        free(rdic_dz_path);

        // Open regular rdic file
        char *rdic_path = build_path(dirname, "res.rdic");
        store->rdic_file = fopen(rdic_path, "rb");
        if (!store->rdic_file) {
            fprintf(stderr, "Error: Cannot open res.rdic: %s\n", rdic_path);
            free(rdic_path);
            return -1;
        }
        printf("Loaded res.rdic (normal)\n");
        free(rdic_path);
    }

    store->type = UD_STARDICT_RES_TYPE_DATABASE;
    store->loaded = true;

    return 0;
}

/**
 * Load resource directory (res/)
 */
static int load_resource_files(sd_stardict_res *store,
                                const char *dirname) {
    // Build res/ path
    char *res_dir_path = build_path(dirname, "res");

    if (!is_directory(res_dir_path)) {
        free(res_dir_path);
        return -1;  // Directory does not exist
    }

    free(res_dir_path);

    // Save directory path
    store->dirname = strdup(dirname);
    if (!store->dirname) {
        fprintf(stderr, "Error: Memory allocation failed for dirname\n");
        return -1;
    }

    store->type = UD_STARDICT_RES_TYPE_FILE;
    store->loaded = true;

    printf("Loaded resource directory: %s/res\n", dirname);
    return 0;
}

// ============================================================
// Public API implementation
// ============================================================

sd_stardict_res *sd_stardict_res_store_load(const char *dirname) {
    if (!dirname) {
        return NULL;
    }

    // Create resource storage object
    sd_stardict_res *store = (sd_stardict_res *)calloc(
        1, sizeof(sd_stardict_res));
    if (!store) {
        fprintf(stderr, "Error: Memory allocation failed for res_store\n");
        return NULL;
    }

    // Try database mode first
    if (load_resource_database(store, dirname) == 0) {
        printf("Loaded resource database\n");
        return store;
    }

    // Try file mode
    if (load_resource_files(store, dirname) == 0) {
        return store;
    }

    // Both modes failed
    fprintf(stderr, "Warning: No resource storage found in %s\n", dirname);
    sd_stardict_res_store_free(store);
    return NULL;
}

void sd_stardict_res_store_free(sd_stardict_res *store) {
    if (!store) {
        return;
    }

    // Free database mode resources
    if (store->entries) {
        for (uint32_t i = 0; i < store->filecount; i++) {
            if (store->entries[i].filename) {
                free(store->entries[i].filename);
            }
        }
        free(store->entries);
    }

    if (store->rdic_file) {
        fclose(store->rdic_file);
    }

    if (store->dz) {
        dictzip_close(store->dz);
    }

    // Free file mode resources
    if (store->dirname) free(store->dirname);

    free(store);
}

bool sd_stardict_res_store_have_file(const sd_stardict_res *store,
                                      const char *filename) {
    if (!store || !filename || !store->loaded) {
        return false;
    }

    if (store->type == UD_STARDICT_RES_TYPE_DATABASE) {
        // Database mode: binary search index
        return binary_search_entries(store->entries, store->filecount,
                                       filename) >= 0;
    } else if (store->type == UD_STARDICT_RES_TYPE_FILE) {
        // File mode: check if file exists (build res/ path dynamically)
        char *res_dir_path = build_path(store->dirname, "res");
        char *full_path = build_path(res_dir_path, filename);
        bool exists = file_exists(full_path);
        free(full_path);
        free(res_dir_path);
        return exists;
    }

    return false;
}

int sd_stardict_res_store_read_file(const sd_stardict_res *store,
                                      const char *filename,
                                      char **data_out,
                                      uint32_t *size_out) {
    if (!store || !filename || !data_out || !store->loaded) {
        return -1;
    }

    if (store->type != UD_STARDICT_RES_TYPE_DATABASE) {
        fprintf(stderr, "Error: read_file only works for database mode\n");
        return -1;
    }

    // Search file index
    int idx = binary_search_entries(store->entries, store->filecount, filename);
    if (idx < 0) {
        return -1;  // File not found
    }

    const sd_stardict_res_entry *entry = &store->entries[idx];

    // Read data (supports both compressed and regular files)
    if (store->dz) {
        // Read from .rdic.dz compressed file (supports random access)
        uint32_t out_size;
        unsigned char *data = dictzip_read(store->dz, entry->offset, entry->size, &out_size);
        if (!data) {
            fprintf(stderr, "Error: Failed to read from res.rdic.dz: %s\n", filename);
            return -1;
        }
        *data_out = (char *)data;  // unsigned char* → char*
    } else if (store->rdic_file) {
        // Read from regular .rdic file
        *data_out = (char *)malloc(entry->size);
        if (!*data_out) {
            fprintf(stderr, "Error: Memory allocation failed for resource data\n");
            return -1;
        }

        fseek((FILE *)store->rdic_file, entry->offset, SEEK_SET);
        size_t read_size = fread(*data_out, 1, entry->size, (FILE *)store->rdic_file);

        if (read_size != entry->size) {
            fprintf(stderr, "Error: Failed to read from res.rdic: %s\n", filename);
            free(*data_out);
            *data_out = NULL;
            return -1;
        }
    } else {
        fprintf(stderr, "Error: No valid rdic handler\n");
        return -1;
    }

    if (size_out) {
        *size_out = entry->size;
    }

    return 0;
}
