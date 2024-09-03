/*
  Copyright (c) 2024 Milos Tatarevic

  This file is part of the FastGridPathCounter repository.
*/

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

#if CYCLES == 0
#undef HAMILTONIAN
#define HAMILTONIAN 0
#endif

// Grid
#define N_LO (((uint64_t)N) / 2)
#define N_HI ((((uint64_t)N) + 1) / 2)

#define SHIFT_N_LO (2 * N_LO)
#define CUTS_MASK_LO ((1ULL << (2 * N_LO)) - 1)

// State
#define I_SHIFT 1
#define VALUE_SHIFT 2
#define VALUE_MASK 3ULL
#define PAIR_MASK 0xfULL

// Groups and buckets
#define GROUP_BUCKET(i) ((i) <= N_LO ? 0 : (i) - N_LO)
#define GROUP_CNT (1ULL << N_HI)
#define GROUP_CNT_2 (GROUP_CNT * GROUP_CNT)
#define GROUP_BUCKET_CNT (N_HI)
#define STATES_LO_BUCKET_CNT (N_LO + 2)

// Replace lookup
#define REPLACE_LOOKUP_VALUES 8
#define REPLACE_LOOKUP_SIZE (1 << (REPLACE_LOOKUP_VALUES * 2))
#define REPLACE_LOOKUP_MASK (REPLACE_LOOKUP_SIZE - 1)
#define RIGHT_REPLACE_MASK 0xaaaaaaaaaaaaaaaaULL
#define LEFT_REPLACE_MASK 0x5555555555555555ULL

// Pair
#define PAIR(a, b) (((a) << VALUE_SHIFT) | (b))

// Cell states
enum { BLANK, LEFT, RIGHT, BLOCK };

// Counter type
typedef TYPE counter_t;

// Grid context
typedef struct {
    // Counters and lookup tables
    counter_t *main, *blocked;
    uint64_t *lookup[2];

    // Hi and lo states
    uint32_t *states_hi;
    uint32_t *states_lo[STATES_LO_BUCKET_CNT];
    uint32_t states_lo_cnt[STATES_LO_BUCKET_CNT];

    // Groups
    uint32_t *groups;
    uint32_t group_cnt[GROUP_BUCKET_CNT][GROUP_CNT];

    // Buckets for lo states
    uint32_t *state_lo_buckets[N_LO][STATES_LO_BUCKET_CNT];
    uint32_t state_lo_buckets_cnt[N_LO][STATES_LO_BUCKET_CNT];
    uint32_t *state_lo_buckets_size[N_LO][STATES_LO_BUCKET_CNT];

    // Replace lookup tables
    int8_t *replace_left_lookup, *replace_right_lookup;
    uint8_t *hi_cnt_lookup;
} grid_context_t;

#endif
