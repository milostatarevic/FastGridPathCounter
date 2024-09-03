/*
  Copyright (c) 2024 Milos Tatarevic

  This file is part of the FastGridPathCounter repository.
*/

#include <stdio.h>

#include "inline.h"

const uint64_t replace_pairs[16] = {9, 4, 8, 0, 1, 0, 0, 0, 2};

uint64_t replace_left(const int8_t *left_lookup, uint64_t state, int col) {
    uint64_t state_segment = state >> ((col + 2) << I_SHIFT);
    int s = left_lookup[state_segment & REPLACE_LOOKUP_MASK];

    if (s >= 0) {
        return set_state_value(state, col + 2 + s, LEFT);
    }

    state_segment >>= REPLACE_LOOKUP_VALUES << I_SHIFT;
    state_segment = reverse_values(state_segment);

    s = -s;
    int i = col + 1 + REPLACE_LOOKUP_VALUES;
    while (state_segment && s) {
        s += 2 - (state_segment & VALUE_MASK);
        state_segment >>= 2;
        i++;
    }
    return s ? state : set_state_value(state, i, LEFT);
}

uint64_t replace_right(const int8_t *right_lookup, uint64_t state, int col) {
    uint64_t mask = (1ULL << (col << I_SHIFT)) - 1;
    int shift_right = col > REPLACE_LOOKUP_VALUES ? col - REPLACE_LOOKUP_VALUES : 0;
    int shift_left = REPLACE_LOOKUP_VALUES > col ? REPLACE_LOOKUP_VALUES - col : 0;

    uint64_t state_segment = (state & mask) >> (shift_right << I_SHIFT);
    state_segment <<= shift_left << I_SHIFT;

    int s = right_lookup[state_segment];
    if (s >= 0) {
        return set_state_value(state, col - 1 - s, RIGHT);
    }

    s = -s;
    state_segment = reverse_values(state);

    int i = col - 1 - REPLACE_LOOKUP_VALUES;
    while (i >= 0 && s) {
        s += state_value(state_segment, i) - 2;
        i--;
    }

    return set_state_value(state, i + 1, RIGHT);
}

uint64_t replace_right_hi(uint64_t state, int col) {
    uint64_t state_segment = reverse_values(state);

    int s = 1, i = col - 1;
    while (i >= N_LO && s) {
        s += state_value(state_segment, i) - 2;
        i--;
    }

    return s ? state : set_state_value(state, i + 1, RIGHT);
}

void process_group(const grid_context_t *context, counter_t mod, int col, uint32_t group) {
    uint64_t mask = (1ULL << ((col + 1) << I_SHIFT)) - 1;
    uint32_t states_lo_col = col < N_LO ? col : N_LO - 1;

    const uint32_t *g_ptr = group_ptr(context, col, group);
    uint32_t g_cnt = context->group_cnt[GROUP_BUCKET(col)][group];

    for (uint32_t g = 0; g < g_cnt; g++) {
        uint32_t state_hi_index = g_ptr[g];
        uint64_t state_hi = context->states_hi[state_hi_index];
        uint64_t state_hi_shifted = state_hi << SHIFT_N_LO;

        uint32_t hi_cnt = context->hi_cnt_lookup[state_hi_index];
        const uint32_t *states_lo_ptr = context->state_lo_buckets[states_lo_col][hi_cnt];
        uint32_t states_lo_cnt = context->state_lo_buckets_cnt[states_lo_col][hi_cnt];

        const uint32_t *state_lo_buckets_size_ptr =
            context->state_lo_buckets_size[states_lo_col][hi_cnt];
        counter_t *counters_ptr = counters_main_ptr(context, state_hi_shifted | *states_lo_ptr);
        counter_t *counters_ptr_start = counters_ptr;

        uint32_t bucket_size;
        for (uint32_t i = 0; i < states_lo_cnt; i++, counters_ptr += bucket_size) {
            bucket_size = state_lo_buckets_size_ptr[i];

            if (!HAMILTONIAN && *counters_ptr == 0) {
                continue;
            }

            uint64_t state = state_hi_shifted | states_lo_ptr[i];
            uint64_t pair = state_pair(state, col);

            if (HAMILTONIAN && bucket_size == 1 && *counters_ptr == 0 &&
                (pair >> VALUE_SHIFT) != BLANK) {
                continue;
            }

            uint64_t new_state = set_state_pair(state, col, replace_pairs[pair]);

            if ((pair >> VALUE_SHIFT) == BLANK) {
                uint64_t shifted_state = shift_state(state, mask);
                counter_t *blocked_ptr = counters_blocked_ptr(context, shifted_state);

                if (pair == PAIR(BLANK, BLANK)) {
                    add_mod_twice(bucket_size, counters_ptr, counters_main_ptr(context, new_state),
                                  blocked_ptr, mod);
                } else {
                    add_mod_set_src(bucket_size, counters_ptr, blocked_ptr, mod);
                }

            } else if ((pair & VALUE_MASK) == BLANK) {
                add_mod(bucket_size, counters_main_ptr(context, new_state), counters_ptr, mod);

            } else if ((pair >> VALUE_SHIFT) == LEFT) {
                if (pair == PAIR(LEFT, LEFT)) {
                    new_state = replace_left(context->replace_left_lookup, new_state, col);
                }

                uint64_t shifted_state = shift_state(new_state, mask);
                counter_t *blocked_ptr = counters_blocked_ptr(context, shifted_state);

                add_mod(bucket_size, blocked_ptr, counters_ptr, mod);

            } else if (pair == PAIR(RIGHT, RIGHT)) {
                if (col > N_LO) {
                    uint64_t new_state_replaced = replace_right_hi(new_state, col);

                    if (new_state_replaced != new_state) {
                        uint64_t shifted_state = shift_state(new_state_replaced, mask);
                        counter_t *blocked_ptr = counters_blocked_ptr(context, shifted_state);

                        add_mod(bucket_size, blocked_ptr, counters_ptr, mod);
                        continue;
                    }
                }

                const uint32_t *lo_ptr =
                    &context->states_lo[hi_cnt][counters_ptr - counters_ptr_start];
                for (uint32_t j = 0; j < bucket_size; j++) {
                    if (counters_ptr[j]) {
                        state = state_hi_shifted | lo_ptr[j];
                        pair = state_pair(state, col);

                        new_state = set_state_pair(state, col, replace_pairs[pair]);
                        new_state = replace_right(context->replace_right_lookup, new_state, col);

                        uint64_t shifted_state = shift_state(new_state, mask);
                        counter_t *blocked_ptr = counters_blocked_ptr(context, shifted_state);
                        _add_mod(blocked_ptr, counters_ptr + j, mod);
                    }
                }
            }
        }
    }
}

void process_group_for_col0(const grid_context_t *context, counter_t mod, uint32_t group) {
    const uint32_t *g_ptr = group_ptr(context, 0, group);
    uint32_t g_cnt = context->group_cnt[0][group];

    for (uint32_t g = 0; g < g_cnt; g++) {
        uint64_t state_hi = context->states_hi[g_ptr[g]];
        uint64_t state_hi_shifted = state_hi << SHIFT_N_LO;

        uint32_t hi_cnt = context->hi_cnt_lookup[g_ptr[g]];
        uint32_t states_lo_cnt = context->states_lo_cnt[hi_cnt];
        const uint32_t *states_lo_ptr = context->state_lo_buckets[0][hi_cnt];

        counter_t *counters_ptr = counters_main_ptr(context, state_hi_shifted | *states_lo_ptr);
        counter_t *blocked_ptr = NULL;

        for (uint32_t i = 0; i < states_lo_cnt; i++) {
            if ((states_lo_ptr[i] & VALUE_MASK) == BLANK) {
                uint64_t state = state_hi_shifted | states_lo_ptr[i];
                blocked_ptr = counters_blocked_ptr(context, state >> VALUE_SHIFT);
                break;
            }
        }

        for (uint32_t i = 0; i < states_lo_cnt; i++, counters_ptr++) {
            if (!HAMILTONIAN && *counters_ptr == 0) {
                if (blocked_ptr && (states_lo_ptr[i] & VALUE_MASK) == BLANK) {
                    _add_mod_reset(counters_ptr, blocked_ptr, mod);
                    blocked_ptr++;
                }
                continue;
            }

            uint64_t state = state_hi_shifted | states_lo_ptr[i];
            uint64_t pair = state & PAIR_MASK;

            if (HAMILTONIAN && *counters_ptr == 0 && (pair >> VALUE_SHIFT) != BLANK) {
                continue;
            }

            if ((pair >> VALUE_SHIFT) == BLANK) {
                if (pair == PAIR(BLANK, BLANK)) {
                    counter_t *counters_new_ptr = counters_ptr + (CYCLES ? 1 : 2);
                    _add_mod_twice(counters_ptr, counters_new_ptr, blocked_ptr, mod);
                    blocked_ptr++;

                } else {
                    _add_mod_set_src(counters_ptr, blocked_ptr, mod);

                    counters_ptr++;
                    i++;

                    _add_mod(counters_ptr - 1, counters_ptr, mod);
                    if (!HAMILTONIAN) {
                        _add_mod_reset(counters_ptr, blocked_ptr, mod);
                    }
                    blocked_ptr++;
                }

            } else if (!HAMILTONIAN && (pair & VALUE_MASK) == BLANK) {
                _add_mod(counters_ptr - 1, counters_ptr, mod);
                _add_mod_reset(counters_ptr, blocked_ptr, mod);
                blocked_ptr++;

            } else if ((pair >> VALUE_SHIFT) == LEFT) {
                uint64_t new_state = set_state_pair(state, 0, replace_pairs[pair]);
                if (pair == PAIR(LEFT, LEFT)) {
                    new_state = replace_left(context->replace_left_lookup, new_state, 0);
                }

                counter_t *blocked_new_ptr =
                    counters_blocked_ptr(context, new_state >> VALUE_SHIFT);
                _add_mod(blocked_new_ptr, counters_ptr, mod);

                if (!HAMILTONIAN) {
                    _add_mod_reset(counters_main_ptr(context, new_state), blocked_new_ptr, mod);
                }
            }
        }
    }
}

void merge_blocked(const grid_context_t *context, counter_t mod, uint32_t group) {
    const uint32_t *g_ptr = group_ptr(context, 0, group);
    uint32_t g_cnt = context->group_cnt[0][group];

    for (uint32_t g = 0; g < g_cnt; g++) {
        uint32_t state_hi_index = g_ptr[g];
        uint64_t state_hi = context->states_hi[state_hi_index];
        uint64_t state_hi_shifted = state_hi << SHIFT_N_LO;

        uint32_t hi_cnt = context->hi_cnt_lookup[state_hi_index];
        uint32_t states_lo_cnt = context->states_lo_cnt[hi_cnt];
        const uint32_t *states_lo_ptr = context->states_lo[hi_cnt];

        uint32_t i = 0;
        while (i < states_lo_cnt && (states_lo_ptr[i] & VALUE_MASK) != BLANK) {
            i++;
        }
        if (i == states_lo_cnt) {
            continue;
        }

        uint64_t state = state_hi_shifted | states_lo_ptr[i];
        counter_t *counters_ptr = counters_main_ptr(context, state);
        counter_t *blocked_ptr = counters_blocked_ptr(context, state >> VALUE_SHIFT);

        while (i < states_lo_cnt) {
            if ((states_lo_ptr[i] & VALUE_MASK) == BLANK) {
                _add_mod_reset(counters_ptr, blocked_ptr, mod);
                blocked_ptr++;
            }
            counters_ptr++;
            i++;
        }
    }
}

void run_group_task(const grid_context_t *context, counter_t mod, int col, uint32_t group) {
    if (col == 0) {
        process_group_for_col0(context, mod, group);
        if (HAMILTONIAN) {
            merge_blocked(context, mod, group);
        }
    } else {
        process_group(context, mod, col, group);
    }
}
