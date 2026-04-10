//
//  sd_array.h
//  stardict
//
//  Created by kejinlu on 2026-04-08
//


#ifndef sd_array_h
#define sd_array_h

#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================
// Internal structure
// ============================================================

typedef struct {
    size_t size;      // Current element count
    size_t capacity;  // Allocated capacity
} sd_array_header;

// ============================================================
// Basic access macros
// ============================================================

#define sd_array(T) T *

#define sd_array_base(v) \
    (&(((sd_array_header *)(v))[-1]))

#define sd_array_size(v) \
    ((v) ? sd_array_base(v)->size : (size_t)0)

#define sd_array_capacity(v) \
    ((v) ? sd_array_base(v)->capacity : (size_t)0)

// ============================================================
// Internal growth (not recommended to call directly)
// ============================================================

static inline void sd_array_grow(void **v, size_t elem_size, size_t needed) {
    size_t cap = (*v) ? sd_array_capacity(*v) : 0;
    if (cap >= needed) return;

    size_t new_cap = cap ? cap * 2 : 4;
    if (new_cap < needed) new_cap = needed;

    size_t total = sizeof(sd_array_header) + new_cap * elem_size;
    sd_array_header *base = realloc((*v) ? sd_array_base(*v) : NULL, total);
    assert(base);

    if (!*v) {
        base->size = 0;
    }
    base->capacity = new_cap;
    *v = (void *)(base + 1);
}

// ============================================================
// Public operation macros
// ============================================================

/**
 * Reserve capacity
 */
#define sd_array_reserve(v, n) \
    sd_array_grow((void **)&(v), sizeof(*(v)), (n))

/**
 * Push back element
 */
#define sd_array_push_back(v, val) do {                         \
    sd_array_grow((void **)&(v), sizeof(*(v)), sd_array_size(v) + 1); \
    (v)[sd_array_size(v)] = (val);                             \
    sd_array_base(v)->size++;                                   \
} while (0)

/**
 * Insert element at pos
 */
#define sd_array_insert(v, pos, val) do {                      \
    sd_array_grow((void **)&(v), sizeof(*(v)), sd_array_size(v) + 1); \
    size_t _pos = (pos);                                       \
    if (_pos < sd_array_size(v)) {                              \
        memmove(&(v)[_pos + 1], &(v)[_pos],                   \
                (sd_array_size(v) - _pos) * sizeof(*(v)));     \
    }                                                          \
    (v)[_pos] = (val);                                         \
    sd_array_base(v)->size++;                                   \
} while (0)

/**
 * Pop last element
 */
#define sd_array_pop_back(v) do {       \
    if (sd_array_size(v) > 0)           \
        sd_array_base(v)->size--;        \
} while (0)

/**
 * Free array memory
 */
#define sd_array_free(v) do {           \
    if (v) {                            \
        free(sd_array_base(v));         \
        (v) = NULL;                     \
    }                                   \
} while (0)

#endif /* sd_array_h */
