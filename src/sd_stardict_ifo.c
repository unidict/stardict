//
//  sd_stardict_ifo.c
//  stardict
//
//  Created by kejinlu on 2026-04-08
//


#include "sd_stardict_ifo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================
// Constants
// ============================================================
#define STARDICT_MAGIC_DATA "StarDict's dict ifo file"
#define STARDICT_MAGIC_TREE "StarDict's treedict ifo file"
#define UTF8_BOM "\xEF\xBB\xBF"

// ============================================================
// Helper functions
// ============================================================

/**
 * Skip whitespace
 */
static const char *skip_whitespace(const char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/**
 * Find next non-whitespace character
 */
static const char *find_non_whitespace(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/**
 * Find equals sign
 */
static const char *find_equals(const char *p, const char *end) {
    while (p < end && *p != '=') {
        p++;
    }
    return p;
}

/**
 * Find end of line
 */
static const char *find_line_end(const char *p, const char *end) {
    while (p < end && *p != '\r' && *p != '\n') {
        p++;
    }
    return p;
}

/**
 * Duplicate string (trimmed)
 */
static char *dup_string_trim(const char *start, const char *end) {
    // Skip leading whitespace
    while (start < end && isspace((unsigned char)*start)) {
        start++;
    }

    // Skip trailing whitespace
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    if (start >= end) {
        return NULL;
    }

    size_t len = end - start;
    char *str = malloc(len + 1);
    if (str) {
        memcpy(str, start, len);
        str[len] = '\0';
    }
    return str;
}

// ============================================================
// .ifo file parser implementation
// ============================================================

sd_stardict_ifo *sd_stardict_ifo_load(const char *ifo_filename) {
    if (!ifo_filename) {
        fprintf(stderr, "Error: ifo_filename is NULL\n");
        return NULL;
    }

    // Read file content
    FILE *fp = fopen(ifo_filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open .ifo file: %s\n", ifo_filename);
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "Error: Invalid .ifo file size: %s\n", ifo_filename);
        fclose(fp);
        return NULL;
    }

    // Read entire file
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed for .ifo file\n");
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read .ifo file completely\n");
        free(buffer);
        return NULL;
    }
    buffer[file_size] = '\0';

    // Create ifo object
    sd_stardict_ifo *ifo = calloc(1, sizeof(sd_stardict_ifo));
    if (!ifo) {
        fprintf(stderr, "Error: Memory allocation failed for sd_stardict_ifo\n");
        free(buffer);
        return NULL;
    }

    // Save filename
    ifo->ifo_file_name = strdup(ifo_filename);
    ifo->loaded = false;

    // Parse content
    const char *p = buffer;
    const char *end = buffer + file_size;

    // Skip UTF-8 BOM
    if (file_size >= 3 && memcmp(p, UTF8_BOM, 3) == 0) {
        p += 3;
    }

    // Check magic number
    bool is_treedict = false;
    if (strncmp(p, STARDICT_MAGIC_DATA, strlen(STARDICT_MAGIC_DATA)) == 0) {
        p += strlen(STARDICT_MAGIC_DATA);
    } else if (strncmp(p, STARDICT_MAGIC_TREE, strlen(STARDICT_MAGIC_TREE)) == 0) {
        is_treedict = true;
        p += strlen(STARDICT_MAGIC_TREE);
    } else {
        fprintf(stderr, "Error: Invalid .ifo file format (missing magic header): %s\n", ifo_filename);
        free(ifo->ifo_file_name);
        free(ifo);
        free(buffer);
        return NULL;
    }

    // Parse key-value pairs
    while (p < end) {
        // Find key start
        const char *key_start = find_non_whitespace(p, end);
        if (key_start >= end) {
            break;
        }

        // Find equals sign
        const char *eq = find_equals(key_start, end);
        if (eq >= end) {
            // No equals sign, possibly empty line or comment
            p = key_start;
            break;
        }

        // Find value start
        const char *val_start = find_non_whitespace(eq + 1, end);
        const char *val_end = find_line_end(val_start, end);
        const char *line_end = val_end;

        // Skip line ending characters
        while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
            line_end++;
        }

        // Extract key and value
        char *key = dup_string_trim(key_start, eq);
        char *value = NULL;
        if (val_start < val_end) {
            value = dup_string_trim(val_start, val_end);
        }

        // Process key-value pair
        if (key) {
            if (strcmp(key, "wordcount") == 0 && value) {
                ifo->wordcount = (uint32_t)atol(value);
            } else if (strcmp(key, "synwordcount") == 0 && value) {
                ifo->syn_wordcount = (uint32_t)atol(value);
            } else if (strcmp(key, "bookname") == 0 && value) {
                ifo->bookname = value;
                value = NULL;  // Transfer ownership
            } else if (strcmp(key, "author") == 0 && value) {
                ifo->author = value;
                value = NULL;
            } else if (strcmp(key, "email") == 0 && value) {
                ifo->email = value;
                value = NULL;
            } else if (strcmp(key, "website") == 0 && value) {
                ifo->website = value;
                value = NULL;
            } else if (strcmp(key, "date") == 0 && value) {
                ifo->date = value;
                value = NULL;
            } else if (strcmp(key, "description") == 0 && value) {
                ifo->description = value;
                value = NULL;
            } else if (strcmp(key, "sametypesequence") == 0 && value) {
                ifo->sametypesequence = value;
                value = NULL;
            } else if (strcmp(key, "idxfilesize") == 0 && value) {
                ifo->index_file_size = (uint32_t)atol(value);
            } else if (strcmp(key, "tdxfilesize") == 0 && value) {
                ifo->index_file_size = (uint32_t)atol(value);
            } else if (strcmp(key, "synfilesize") == 0 && value) {
                ifo->syn_file_size = (uint32_t)atol(value);
            }

            free(key);
        }
        if (value) {
            free(value);
        }

        p = line_end;
    }

    free(buffer);

    // Validate required fields
    if (ifo->wordcount == 0) {
        fprintf(stderr, "Error: Missing 'wordcount' in .ifo file: %s\n", ifo_filename);
        sd_stardict_ifo_free(ifo);
        return NULL;
    }

    if (ifo->index_file_size == 0) {
        fprintf(stderr, "Error: Missing 'idxfilesize' or 'tdxfilesize' in .ifo file: %s\n", ifo_filename);
        sd_stardict_ifo_free(ifo);
        return NULL;
    }

    if (!ifo->bookname) {
        fprintf(stderr, "Error: Missing 'bookname' in .ifo file: %s\n", ifo_filename);
        sd_stardict_ifo_free(ifo);
        return NULL;
    }

    ifo->loaded = true;
    return ifo;
}

void sd_stardict_ifo_free(sd_stardict_ifo *ifo) {
    if (!ifo) {
        return;
    }

    if (ifo->ifo_file_name) {
        free(ifo->ifo_file_name);
    }
    if (ifo->bookname) {
        free(ifo->bookname);
    }
    if (ifo->author) {
        free(ifo->author);
    }
    if (ifo->email) {
        free(ifo->email);
    }
    if (ifo->website) {
        free(ifo->website);
    }
    if (ifo->date) {
        free(ifo->date);
    }
    if (ifo->description) {
        free(ifo->description);
    }
    if (ifo->sametypesequence) {
        free(ifo->sametypesequence);
    }

    free(ifo);
}

bool sd_stardict_ifo_can_search_data(const sd_stardict_ifo *ifo) {
    if (!ifo) {
        return false;
    }

    // If sametypesequence is not defined, may contain mixed types, can search
    if (!ifo->sametypesequence || ifo->sametypesequence[0] == '\0') {
        return true;
    }

    // Check if searchable types are included: m, l, g, x, t, y, k
    const char *types = "mlgxtyk";
    return (strpbrk(ifo->sametypesequence, types) != NULL);
}
