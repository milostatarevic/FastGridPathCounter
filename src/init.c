/*
  Copyright (c) 2024 Milos Tatarevic

  This file is part of the FastGridPathCounter repository.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inline.h"

#define CUTS_LIMIT_LO (1ULL << (2 * N_LO))
#define CUTS_LIMIT_HI (1ULL << (2 * N_HI))

uint64_t g_memory_allocated = 0;

void *alloc(size_t size, int tmp) {
    void *ptr = malloc(size);

    if (ptr == NULL) {
        fprintf(stderr, "memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    memset(ptr, 0, size);
    if (!tmp) {
        g_memory_allocated += size;
    }
    return ptr;
}

int is_balanced_lo(uint64_t state) {
    if (is_blocked(state)) {
        return 0;
    }

    int s = CYCLES ? 1 : 2;
    state = reverse_values(state);

    for (int i = 0; i < N_LO && s; i++) {
        s += 2 - state_value(state, i);
    }
    return s;
}

int is_balanced_hi(uint64_t state) {
    if (is_blocked(state)) {
        return 0;
    }

    int s = 1;
    state = reverse_values(state);

    for (int i = N_HI - 1; i >= 0 && s; i--) {
        s += state_value(state, i) - 2;
    }
    return s;
}

int get_right_cnt(uint64_t state) {
    int c = 0;

    for (int i = 0; i < N_HI; i++) {
        int v = state_value(state, i);
        if (v == BLANK) {
            continue;
        }
        c += (v == RIGHT) ? 1 : -1;
    }
    return c;
}

uint32_t group_id(uint32_t r) {
    uint32_t id = 0;

    for (int i = 0; i < N_HI; i++) {
        if (state_value(r, i) != BLANK) {
            id |= 1 << i;
        }
    }
    return id;
}

void init_balanced_states(uint32_t *balanced_lo, uint32_t *balanced_hi, uint32_t *balanced_lo_cnt,
                          uint32_t *balanced_hi_cnt) {
    *balanced_lo_cnt = *balanced_hi_cnt = 0;

    for (uint32_t i = 0; i < CUTS_LIMIT_LO; i++) {
        if (is_balanced_lo(i)) {
            balanced_lo[(*balanced_lo_cnt)++] = i;
        }
    }
    for (uint32_t i = 0; i < CUTS_LIMIT_HI; i++) {
        if (is_balanced_hi(i)) {
            balanced_hi[(*balanced_hi_cnt)++] = i;
        }
    }
}

void init_hi_cnt_lookup(grid_context_t *context, const uint32_t *balanced_hi, uint32_t balanced_hi_cnt) {
    context->states_hi = (uint32_t *)alloc(balanced_hi_cnt * sizeof(uint32_t), 0);
    context->hi_cnt_lookup = (uint8_t *)alloc(balanced_hi_cnt, 0);

    for (uint32_t i = 0; i < balanced_hi_cnt; i++) {
        uint32_t state = balanced_hi[i];
        context->states_hi[i] = state;
        context->hi_cnt_lookup[i] = get_right_cnt(state);
    }
}

void init_states_lo(grid_context_t *context, const uint32_t *balanced_lo, uint32_t balanced_lo_cnt,
                    uint32_t *cl_cnt) {
    uint8_t *r_cnt_lookup = (uint8_t *)alloc(balanced_lo_cnt, 1);

    for (uint32_t i = 0; i < balanced_lo_cnt; i++) {
        r_cnt_lookup[i] = -get_right_cnt(balanced_lo[i]) + (CYCLES ? 0 : 1);
    }

    memset(context->states_lo_cnt, 0, sizeof(context->states_lo_cnt));
    for (uint32_t i = 0; i < balanced_lo_cnt; i++) {
        context->states_lo_cnt[r_cnt_lookup[i]]++;
    }

    for (uint32_t i = 0; i < STATES_LO_BUCKET_CNT; i++) {
        context->states_lo[i] = (uint32_t *)alloc(context->states_lo_cnt[i] * sizeof(uint32_t), 0);
    }

    for (uint32_t i = 0; i < balanced_lo_cnt; i++) {
        uint32_t r_cnt = r_cnt_lookup[i];
        context->states_lo[r_cnt][cl_cnt[r_cnt]++] = balanced_lo[i];
    }

    free(r_cnt_lookup);
}

void init_lookups_and_counts(grid_context_t *context, const uint32_t *balanced_hi,
                             uint32_t balanced_hi_cnt, const uint32_t *cl_cnt,
                             uint64_t *counters_cnt, uint64_t *blocked_cnt) {
    context->lookup[0] = (uint64_t *)alloc(CUTS_LIMIT_LO * sizeof(uint64_t), 0);
    context->lookup[1] = (uint64_t *)alloc(CUTS_LIMIT_HI * sizeof(uint64_t), 0);

    for (uint32_t c = 0; c < STATES_LO_BUCKET_CNT; c++) {
        uint64_t lo_cnt = 0;
        for (uint32_t j = 0; j < context->states_lo_cnt[c]; j++) {
            uint32_t state_lo = context->states_lo[c][j];
            if (context->lookup[0][state_lo] == 0) {
                context->lookup[0][state_lo] = lo_cnt++;
            }
        }
    }

    uint64_t hi_cnt = 0;
    for (uint32_t i = 0; i < balanced_hi_cnt; i++) {
        uint32_t state_hi = balanced_hi[i];
        uint32_t c = context->hi_cnt_lookup[i];

        context->lookup[1][state_hi] = hi_cnt;
        hi_cnt += cl_cnt[c];

        if (state_hi < CUTS_LIMIT_HI / 4) {
            *blocked_cnt = hi_cnt;
        }
    }
    *counters_cnt = hi_cnt;
}

void init_groups(grid_context_t *context, const uint32_t *balanced_hi, uint32_t balanced_hi_cnt) {
    context->groups = (uint32_t *)alloc(GROUP_CNT_2 * GROUP_BUCKET_CNT * sizeof(uint32_t), 0);

    for (uint32_t i = 0; i < GROUP_BUCKET_CNT; i++) {
        memset(context->group_cnt[i], 0, sizeof(context->group_cnt[i]));

        for (uint32_t j = 0; j < balanced_hi_cnt; j++) {
            uint32_t g = group_id(balanced_hi[j] & ~(PAIR_MASK << (i << I_SHIFT)));
            uint64_t pos = (uint64_t)i * GROUP_CNT_2 + g * GROUP_CNT + context->group_cnt[i][g];
            context->groups[pos] = j;
            context->group_cnt[i][g]++;
        }
    }
}

void init_buckets(grid_context_t *context) {
    for (uint32_t i = 0; i < N_LO; i++) {
        uint32_t i_shifted = i << I_SHIFT;

        for (uint32_t b = 0; b < STATES_LO_BUCKET_CNT; b++) {
            context->state_lo_buckets[i][b] =
                (uint32_t *)alloc(context->states_lo_cnt[b] * sizeof(uint32_t), 0);
            context->state_lo_buckets_size[i][b] =
                (uint32_t *)alloc(context->states_lo_cnt[b] * sizeof(uint32_t), 0);

            uint32_t j_start = 0, l_cnt = 0;
            for (uint32_t j = 0; j <= context->states_lo_cnt[b]; j++) {
                if (j == context->states_lo_cnt[b] ||
                    (context->states_lo[b][j_start] >> i_shifted) !=
                        (context->states_lo[b][j] >> i_shifted)) {
                    context->state_lo_buckets[i][b][l_cnt] = context->states_lo[b][j_start];
                    context->state_lo_buckets_size[i][b][l_cnt] = j - j_start;
                    j_start = j;
                    l_cnt++;
                }
            }
            context->state_lo_buckets_cnt[i][b] = l_cnt;
        }
    }
}

void allocate_counters(grid_context_t *context, uint64_t counters_cnt, uint64_t blocked_cnt) {
    uint64_t counters_size, blocked_size;

    counters_size = counters_cnt * sizeof(counter_t);
    context->main = (counter_t *)alloc(counters_size, 0);

    blocked_size = blocked_cnt * sizeof(counter_t);
    context->blocked = (counter_t *)alloc(blocked_size, 0);
}

int replace_left_value(uint32_t state) {
    state = reverse_values(state);
    int s = 1, i = 0;

    while (i < REPLACE_LOOKUP_VALUES && s) {
        s += 2 - state_value(state, i);
        i++;
    }
    return s ? -s : i - 1;
}

int replace_right_value(uint32_t state) {
    state = reverse_values(state);
    int s = 1, i = 0;

    while (i < REPLACE_LOOKUP_VALUES && s) {
        s += state_value(state, REPLACE_LOOKUP_VALUES - 1 - i) - 2;
        i++;
    }
    return s ? -s : i - 1;
}

void init_replace_lookups(grid_context_t *context) {
    context->replace_left_lookup = (int8_t *)alloc(REPLACE_LOOKUP_SIZE, 0);
    context->replace_right_lookup = (int8_t *)alloc(REPLACE_LOOKUP_SIZE, 0);

    for (int i = 0; i < REPLACE_LOOKUP_SIZE; i++) {
        context->replace_left_lookup[i] = replace_left_value(i);
        context->replace_right_lookup[i] = replace_right_value(i);
    }
}

grid_context_t *init() {
    grid_context_t *context = alloc(sizeof(grid_context_t), 1);

    // Allocate temporary lookups
    uint32_t *balanced_lo = (uint32_t *)alloc(CUTS_LIMIT_LO * sizeof(uint32_t), 1);
    uint32_t *balanced_hi = (uint32_t *)alloc(CUTS_LIMIT_HI * sizeof(uint32_t), 1);

    // Initialize balanced states
    uint32_t balanced_lo_cnt, balanced_hi_cnt;
    init_balanced_states(balanced_lo, balanced_hi, &balanced_lo_cnt, &balanced_hi_cnt);

    // Initialize hi_cnt_lookup
    init_hi_cnt_lookup(context, balanced_hi, balanced_hi_cnt);

    // Initialize states_lo
    uint32_t *cl_cnt = (uint32_t *)alloc(STATES_LO_BUCKET_CNT * sizeof(uint32_t), 1);
    init_states_lo(context, balanced_lo, balanced_lo_cnt, cl_cnt);

    // Initialize lookups and get the size of counters and blocked states
    uint64_t counters_cnt, blocked_cnt;
    init_lookups_and_counts(context, balanced_hi, balanced_hi_cnt, cl_cnt, &counters_cnt,
                            &blocked_cnt);

    // Initialize groups
    init_groups(context, balanced_hi, balanced_hi_cnt);

    // Initialize buckets
    init_buckets(context);

    // Initialize replace lookups
    init_replace_lookups(context);

    // Free temporary lookups
    free(balanced_lo);
    free(balanced_hi);
    free(cl_cnt);

    // Allocate counters
    allocate_counters(context, counters_cnt, blocked_cnt);

    printf("memory   = %lluMB\n\n", g_memory_allocated / (1 << 20));

    return context;
}
