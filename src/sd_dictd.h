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
 * @return Dictionary object, NULL on failure
 */
sd_dictd *sd_dictd_open(const char *index_path);

/**
 * Open dictd dictionary file (specify all file paths)
 * @param index_path .index file path
 * @param dict_path .dict or .dict.dz file path
 * @return Dictionary object, NULL on failure
 */
sd_dictd *sd_dictd_open_from_paths(const char *index_path,
                                  const char *dict_path);

/**
 * Close dictd dictionary
 * @param dictd dictd dictionary object
 */
void sd_dictd_close(sd_dictd *dictd);

// ============================================================
// dictd query API
// ============================================================

/**
 * Look up all matching entries (a single key may have multiple definitions)
 * @param dictd dictionary object
 * @param key Search keyword
 * @return Result list on success (needs sd_lookup_result_free), NULL on failure or not found
 */
sd_lookup_result *sd_dictd_lookup(sd_dictd *dictd, const char *key);

/**
 * Get word suggestions by prefix
 * @param dictd dictionary object
 * @param prefix Prefix
 * @param max_results Max results (0 = unlimited)
 * @return Suggestion list on success (needs sd_suggestion_result_free), NULL on failure
 */
sd_suggestion_result *sd_dictd_suggest(sd_dictd *dictd, const char *prefix, size_t max_results);

/**
 * Fast lookup word definition using index entry (avoid repeated index search)
 * @param dictd dictionary object
 * @param entry Index entry (obtained from suggest)
 * @return Result on success (caller must free), NULL on failure
 */
char *sd_dictd_lookup_by_index(sd_dictd *dictd, const sd_dictfile_index_entry *entry);

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
