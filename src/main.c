/*
  Copyright (c) 2024 Milos Tatarevic

  This file is part of the FastGridPathCounter repository.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "count.h"
#include "init.h"
#include "inline.h"

typedef struct {
    const grid_context_t *context;
    counter_t mod;
    int col;
    uint32_t *groups;
    uint32_t groups_cnt;
    int next_task_index;
    pthread_mutex_t task_mutex;
} thread_pool_data_t;

void pthread_assert(int ret) {
    if (ret != 0) {
        fprintf(stderr, "pthread error\n");
        exit(EXIT_FAILURE);
    }
}

void *process_group_tasks(void *arg) {
    thread_pool_data_t *data = (thread_pool_data_t *)arg;

    while (1) {
        pthread_assert(pthread_mutex_lock(&data->task_mutex));
        int task_index = data->next_task_index++;
        pthread_assert(pthread_mutex_unlock(&data->task_mutex));

        if (task_index >= data->groups_cnt) {
            break;
        }
        run_group_task(data->context, data->mod, data->col, data->groups[task_index]);
    }
    pthread_exit(NULL);
}

void init_thread_data(thread_pool_data_t *data, const grid_context_t *context, counter_t mod, int col,
                      uint32_t *groups, uint32_t groups_cnt) {
    data->context = context;
    data->mod = mod;
    data->col = col;
    data->groups = groups;
    data->groups_cnt = groups_cnt;
    data->next_task_index = 0;

    pthread_assert(pthread_mutex_init(&data->task_mutex, NULL));
}

void run_threads(pthread_t *threads, thread_pool_data_t *data) {
    for (int t = 0; t < N_THREADS; t++) {
        pthread_assert(pthread_create(&threads[t], NULL, process_group_tasks, (void *)data));
    }
}

void join_threads(pthread_t *threads) {
    for (int t = 0; t < N_THREADS; t++) {
        pthread_assert(pthread_join(threads[t], NULL));
    }
}

void process_cell(const grid_context_t *context, counter_t mod, int i) {
    uint32_t groups[GROUP_CNT];
    uint32_t group_bucket = GROUP_BUCKET(i);
    uint32_t groups_cnt = 0;

    for (int32_t g = GROUP_CNT - 1; g >= 0; g--) {
        if (context->group_cnt[group_bucket][g] > 0) {
            groups[groups_cnt++] = g;
        }
    }

    thread_pool_data_t thread_data;
    init_thread_data(&thread_data, context, mod, i, groups, groups_cnt);

    pthread_t threads[N_THREADS];
    run_threads(threads, &thread_data);
    join_threads(threads);

    pthread_assert(pthread_mutex_destroy(&thread_data.task_mutex));
}

void run(const grid_context_t *context, counter_t mod) {
    uint64_t state = set_state_value(0, 0, CYCLES ? BLANK : RIGHT);
    *counters_main_ptr(context, state) = 1;

    uint64_t count = 0;
    for (int row = 0; row < N; row++) {
        for (int col = N - 2; col >= 0; col--) {
            printf("counting = %d/%d (%d) \r", row + 1, N, N - col);
            fflush(stdout);

            if (CYCLES && (!HAMILTONIAN || (row == N - 1 && col == 0))) {
                state = set_state_pair(0, col, PAIR(RIGHT, LEFT));
                count += *counters_main_ptr(context, state);
                count %= mod;
            }
            process_cell(context, mod, col);
        }
    }

    if (!CYCLES) {
        state = set_state_value(0, N - 1, RIGHT);
        count = *counters_main_ptr(context, state) % mod;
    }

    printf("\nsolution = %llu mod %llu\n\n", count, (uint64_t)mod);
}

counter_t parse_mod(int argc, const char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <mod>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *endptr;
    uint64_t mod = strtoull(argv[1], &endptr, 10);

    int bits = sizeof(counter_t) * 8;
    int bound_bits = bits < 64 ? bits : bits - 1;

    uint64_t max_mod = (1ULL << bound_bits) - 1;
    if (mod == 0 || mod > max_mod) {
        fprintf(stderr, "mod is out of range (0, %llu]\n", max_mod);
        exit(EXIT_FAILURE);
    }

    return (counter_t)mod;
}

void print_config(counter_t mod) {
    printf("N        = %d\n", N);
    printf("bits     = %d\n", (int)(sizeof(counter_t) * 8));
    printf("cycles   = %s %s\n", CYCLES ? "yes" : "no", HAMILTONIAN ? "(hamiltonian)" : "");
    printf("threads  = %d\n", N_THREADS);
    printf("mod      = %llu\n", (uint64_t)mod);
}

int main(int argc, const char *argv[]) {
    counter_t mod = parse_mod(argc, argv);
    print_config(mod);

    const grid_context_t *context = init();
    run(context, mod);

    return 0;
}
