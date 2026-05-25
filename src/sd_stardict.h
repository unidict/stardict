//
//  sd_stardict.h
//  stardict
//
//  Created by kejinlu on 2026-04-09
//


#ifndef sd_stardict_h
#define sd_stardict_h

#include "sd_stardict_ifo.h"
#include "sd_dictfile_index.h"
#include "sd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// StarDict dictionary object
// ============================================================
typedef struct sd_stardict sd_stardict;

// ============================================================
// Constructor/Destructor
// ============================================================

/**
 * Open StarDict dictionary file (auto-discover .idx, .dict, etc.)
 * @param ifo_path .ifo file path
 * @param out_dict Output dictionary object
 * @return SD_OK on success, SD_ERR_INVALID_PARAM / SD_ERR_IO / SD_ERR_MEMORY / SD_ERR_FORMAT on failure
 */
sd_status sd_stardict_open(const char *ifo_path, sd_stardict **out_dict);

/**
 * Open StarDict dictionary file (specify all file paths)
 * @param ifo_path .ifo file path
 * @param idx_path .idx file path
 * @param dict_path .dict or .dict.dz file path
 * @param syn_path .syn file path (can be NULL)
 * @param out_dict Output dictionary object
 * @return SD_OK on success, SD_ERR_INVALID_PARAM / SD_ERR_IO / SD_ERR_MEMORY / SD_ERR_FORMAT on failure
 */
sd_status sd_stardict_open_from_paths(const char *ifo_path,
                                      const char *idx_path,
                                      const char *dict_path,
                                      const char *syn_path,
                                      sd_stardict **out_dict);

/**
 * Close StarDict dictionary
 * @param stardict StarDict dictionary object
 */
void sd_stardict_close(sd_stardict *stardict);

// ============================================================
// StarDict query API
// ============================================================

/**
 * Get dictionary info (returns internal ifo pointer, lifetime tied to dictionary object)
 * @param stardict Dictionary object
 * @return Dictionary info, NULL on failure
 */
const sd_stardict_ifo *stardict_get_info(sd_stardict *stardict);

/**
 * Look up index entries by exact key
 * @param stardict Dictionary object
 * @param key Search keyword
 * @param out_index_entries Output index entry array (needs sd_index_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY on error
 */
sd_status stardict_entry_lookup(sd_stardict *stardict, const char *key, sd_index_entry_array **out_index_entries);

/**
 * Look up all matching entries (a single key may have multiple definitions)
 * @param stardict Dictionary object
 * @param key Search keyword
 * @param out_data_entries Output data entry array (needs sd_data_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY / SD_ERR_IO on error
 */
sd_status stardict_lookup(sd_stardict *stardict, const char *key, sd_data_entry_array **out_data_entries);

/**
 * Get word suggestions by prefix
 * @param stardict Dictionary object
 * @param prefix Prefix
 * @param max_results Max results (0 = unlimited)
 * @param out_index_entries Output index entry array (needs sd_index_entry_array_free)
 * @return SD_OK on success, SD_NOT_FOUND if no match, SD_ERR_INVALID_PARAM / SD_ERR_MEMORY on error
 */
sd_status stardict_suggest(sd_stardict *stardict, const char *prefix, size_t max_results, sd_index_entry_array **out_index_entries);

/**
 * Fetch word definition using index entry (avoid repeated index search)
 * @param stardict Dictionary object
 * @param entry Index entry (obtained from suggest or entry_lookup)
 * @param out_definition Output definition string (caller must free)
 * @return SD_OK on success, SD_NOT_FOUND if not found, SD_ERR_INVALID_PARAM / SD_ERR_IO on error
 */
sd_status stardict_fetch(sd_stardict *stardict, const sd_dictfile_index_entry *entry, char **out_definition);

/**
 * Get internal index object for entry iteration
 * @param stardict Dictionary object
 * @return Index object, lifetime tied to dictionary object
 */
const sd_dictfile_index *stardict_get_index(sd_stardict *stardict);

typedef struct {
    const char *ifo_path;  /**< Valid while sd_stardict is alive */
    const char *idx_path;  /**< Valid while sd_stardict is alive */
    const char *dict_path; /**< Valid while sd_stardict is alive */
    const char *syn_path;  /**< Valid while sd_stardict is alive, may be NULL */
} sd_stardict_paths;

sd_stardict_paths sd_stardict_get_paths(sd_stardict *stardict);

#ifdef __cplusplus
}
#endif

#endif /* sd_stardict_h */
