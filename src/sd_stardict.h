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
 * @return Dictionary object, NULL on failure
 */
sd_stardict *sd_stardict_open(const char *ifo_path);

/**
 * Open StarDict dictionary file (specify all file paths)
 * @param ifo_path .ifo file path
 * @param idx_path .idx file path
 * @param dict_path .dict or .dict.dz file path
 * @param syn_path .syn file path (can be NULL)
 * @return Dictionary object, NULL on failure
 */
sd_stardict *sd_stardict_open_from_paths(const char *ifo_path,
                                     const char *idx_path,
                                     const char *dict_path,
                                     const char *syn_path);

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
 * Look up all matching entries (a single key may have multiple definitions)
 * @param stardict Dictionary object
 * @param key Search keyword
 * @return Result list on success (needs sd_lookup_result_free), NULL on failure
 */
sd_lookup_result *stardict_lookup(sd_stardict *stardict, const char *key);

/**
 * Get word suggestions by prefix
 * @param stardict Dictionary object
 * @param prefix Prefix
 * @param max_results Max results (0 = unlimited)
 * @return Suggestion list on success (needs sd_suggestion_result_free), NULL on failure
 */
sd_suggestion_result *stardict_suggest(sd_stardict *stardict, const char *prefix, size_t max_results);

/**
 * Fast lookup word definition using index entry (avoid repeated index search)
 * @param stardict Dictionary object
 * @param entry Index entry (obtained from suggest)
 * @return Result on success (caller must free), NULL on failure
 */
char *stardict_lookup_by_index(sd_stardict *stardict, const sd_dictfile_index_entry *entry);

/**
 * Get internal index object for entry iteration
 * @param stardict Dictionary object
 * @return Index object, lifetime tied to dictionary object
 */
const sd_dictfile_index *stardict_get_index(sd_stardict *stardict);

#ifdef __cplusplus
}
#endif

#endif /* sd_stardict_h */
