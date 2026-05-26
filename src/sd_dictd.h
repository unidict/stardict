//
//  sd_dictd.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_dictd_h
#define sd_dictd_h

#include "sd_types.h"
#include "sd_dictfile_index.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// dictd dictionary object
// ============================================================

typedef struct sd_dictd sd_dictd;

// ============================================================
// Constructor/Destructor
// ============================================================

/**
 * Open dictd dictionary file (auto-discover .dict file)
 * @param index_path .index file path
 * @param data_only true to load only .dict (skip .index)
 * @param out_dict Output dictionary object
 * @return SD_OK on success, SD_ERR_INVALID_PARAM / SD_ERR_IO / SD_ERR_MEMORY / SD_ERR_FORMAT on failure
 */
sd_status sd_dictd_open(const char *index_path, bool data_only, sd_dictd **out_dict);

/**
 * Close dictd dictionary
 * @param dictd dictd dictionary object
 */
void sd_dictd_close(sd_dictd *dictd);

// ============================================================
// dictd query API
// ============================================================

/**
 * Look up index entries by exact key
 * @param dictd dictionary object
 * @param key Search keyword
 * @param out_index_entries Output index entry array (needs sd_index_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY on error
 */
sd_status sd_dictd_entry_lookup(sd_dictd *dictd, const char *key, sd_index_entry_array **out_index_entries);

/**
 * Look up all matching entries (a single key may have multiple definitions)
 * @param dictd dictionary object
 * @param key Search keyword
 * @param out_data_entries Output data entry array (needs sd_data_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY / SD_ERR_IO on error
 */
sd_status sd_dictd_lookup(sd_dictd *dictd, const char *key, sd_data_entry_array **out_data_entries);

/**
 * Get word suggestions by prefix
 * @param dictd dictionary object
 * @param prefix Prefix
 * @param limit Max results (0 = unlimited)
 * @param out_index_entries Output index entry array (needs sd_index_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY on error
 */
sd_status sd_dictd_suggest(sd_dictd *dictd, const char *prefix, size_t limit, sd_index_entry_array **out_index_entries);

/**
 * Fetch word definition using index entry (avoid repeated index search)
 * @param dictd dictionary object
 * @param entry Index entry (obtained from suggest or entry_lookup)
 * @param out_definition Output definition string (caller must free)
 * @return SD_OK on success, SD_NOT_FOUND if not found, SD_ERR_INVALID_PARAM / SD_ERR_IO on error
 */
sd_status sd_dictd_fetch(sd_dictd *dictd, const sd_dictfile_index_entry *entry, char **out_definition);

/**
 * Get internal index object for entry iteration
 * @param dictd dictionary object
 * @return Index object, lifetime tied to dictionary object
 */
const sd_dictfile_index *sd_dictd_get_index(sd_dictd *dictd);

typedef struct {
    const char *index_path; /**< Valid while sd_dictd is alive */
    const char *dict_path;  /**< Valid while sd_dictd is alive */
} sd_dictd_paths;

sd_dictd_paths sd_dictd_get_paths(sd_dictd *dictd);

#ifdef __cplusplus
}
#endif

#endif /* sd_dictd_h */
