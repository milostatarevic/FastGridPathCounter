/*
  Copyright (c) 2024 Milos Tatarevic

  This file is part of the FastGridPathCounter repository.
*/

#include "defs.h"

inline uint64_t state_value(uint64_t state, int i) {
    return (state >> (i << I_SHIFT)) & VALUE_MASK;
}

inline uint64_t state_pair(uint64_t state, int i) {
    return (state >> (i << I_SHIFT)) & PAIR_MASK;
}

inline uint64_t set_state_value(uint64_t state, int i, uint64_t value) {
    i <<= I_SHIFT;
    return (state & ~(VALUE_MASK << i)) | (value << i);
}

inline uint64_t set_state_pair(uint64_t state, int i, uint64_t pair) {
    i <<= I_SHIFT;
    return (state & ~(PAIR_MASK << i)) | (pair << i);
}

inline uint64_t shift_state(uint64_t state, uint64_t mask) {
    return ((state >> VALUE_SHIFT) & ~mask) | (state & mask);
}

inline uint64_t reverse_values(uint64_t state) {
    return state | ((state & RIGHT_REPLACE_MASK) >> 1) | ((~state & LEFT_REPLACE_MASK) << 1);
}

inline int is_blocked(uint64_t state) {
    return (state & (state << 1) & RIGHT_REPLACE_MASK) != 0;
}

inline uint64_t counters_lookup_pos(const grid_context_t *context, uint64_t state) {
    return context->lookup[0][state & CUTS_MASK_LO] + context->lookup[1][state >> SHIFT_N_LO];
}

inline counter_t *counters_main_ptr(const grid_context_t *context, uint64_t state) {
    return context->main + counters_lookup_pos(context, state);
}

inline counter_t *counters_blocked_ptr(const grid_context_t *context, uint64_t state) {
    return context->blocked + counters_lookup_pos(context, state);
}

inline uint32_t *group_ptr(const grid_context_t *context, int col, uint32_t group) {
    uint64_t pos = (uint64_t)GROUP_BUCKET(col) * GROUP_CNT_2 + group * GROUP_CNT;
    return context->groups + pos;
}

inline uint32_t group_cnt(const grid_context_t *context, int col, uint32_t group) {
    return context->group_cnt[GROUP_BUCKET(col)][group];
}

inline void _add_mod(counter_t *dest, const counter_t *src, counter_t mod) {
    uint64_t v = (uint64_t)(*dest) + (uint64_t)(*src);
    if (v > mod) {
        v -= mod;
    }
    *dest = v;
}

inline void _add_mod_reset(counter_t *dest, counter_t *src, counter_t mod) {
    if (HAMILTONIAN) {
        *dest = *src;
    } else {
        _add_mod(dest, src, mod);
    }
    *src = 0;
}

inline void _add_mod_twice(counter_t *dest, counter_t *dest_new, counter_t *src, counter_t mod) {
    _add_mod(dest_new, dest, mod);
    _add_mod_reset(dest, src, mod);
}

inline void _add_mod_set_src(counter_t *dest, counter_t *src, counter_t mod) {
    counter_t c = *dest;
    if (HAMILTONIAN) {
        *dest = *src;
    } else {
        _add_mod(dest, src, mod);
    }
    *src = c;
}

inline void add_mod_twice(uint32_t len, counter_t *dest, counter_t *dest_new, counter_t *src,
                          counter_t mod) {
    for (uint32_t i = 0; i < len; i++, dest++, dest_new++, src++) {
        _add_mod_twice(dest, dest_new, src, mod);
    }
}

inline void add_mod_set_src(uint32_t len, counter_t *dest, counter_t *src, counter_t mod) {
    for (uint32_t i = 0; i < len; i++, dest++, src++) {
        _add_mod_set_src(dest, src, mod);
    }
}

inline void add_mod(uint32_t len, counter_t *dest, counter_t *src, counter_t mod) {
    for (uint32_t i = 0; i < len; i++, dest++, src++) {
        _add_mod(dest, src, mod);
    }
}
