#include <asm/io.h>
#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <asm/uaccess.h>

#define RIDL_PAGE_SIZE 4096
#define RIDL_PAGE_SIZE_BITS 12
#define RIDL_REINFORCEMENT_WRITES 40
#define RIDL_PROC_FILE_NAME "RIDL_interface"
#define RIDL_MSG_MAX_LEN 100
#define RIDL_CYCLE_LENGTH 400000000ull

MODULE_LICENSE("GPL");

uint8_t *wbuffer = NULL;
int msg_sent = 1;

static inline void flush(void *p) {
#ifdef __x86_64__
    asm volatile("clflush (%0)\n" : : "r"(p) : "rax");
#else
    asm volatile("clflush (%0)\n" : : "r"(p) : "eax");
#endif
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

static inline void flush_mem(void *mem) {
    int i = 0;
    for (; i < 256; i++) {
        flush(mem + (i<<RIDL_PAGE_SIZE_BITS));
    }
}

inline void victim(char *secret, uint64_t cycle_length) {
    uint64_t *destination = alloc_pages_exact(RIDL_PAGE_SIZE, GFP_KERNEL);
    int length = strlen(secret), i = 0, j;
    uint64_t t = ktime_get_boottime_ns();
    for (; i < length; i++) {
        while (ktime_get_boottime_ns() - t < cycle_length) {
            j = 1000;
            while (j--) store_and_flush(secret[i], destination);
        }
        t = ktime_get_boottime_ns();
    }
}

static ssize_t read_msg(struct file *file, char __user *ubuf,size_t count, loff_t *ppos)
{
	char* msg = "Proc File for the RIDL Kernel Module. Write to this file to start the Kernel-Victim.\n";
	msg_sent = !msg_sent;
	if(*ppos > 0 || count < strlen(msg) || msg_sent)
	    return 0;
	if(copy_to_user(ubuf, msg, strlen(msg)))
		return -EFAULT;
	return strlen(msg);
}

static ssize_t write_msg(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    char secret_buffer[RIDL_MSG_MAX_LEN];
    if(count > RIDL_MSG_MAX_LEN)
        return -2;
    if(copy_from_user(secret_buffer, ubuf, count))
        return -EFAULT;
    victim(secret_buffer, RIDL_CYCLE_LENGTH);
    *ppos += strlen(secret_buffer);
	return strlen(secret_buffer);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static struct file_operations file_ops = {
        .owner = THIS_MODULE,
        .read = read_msg,
        .write = write_msg,
};
#else
static struct proc_ops file_ops = {
        .proc_read = read_msg,
        .proc_write = write_msg,
};
#endif

static int __init

RIDL_init(void) {
    struct proc_dir_entry *entry;
    printk(KERN_INFO
    "[RIDL] Hello World!\n");
    entry = proc_create(RIDL_PROC_FILE_NAME, 777, NULL, &file_ops);
    printk(KERN_INFO
    "[RIDL] RIDL Kernel Module loaded!\n");
    return 0;
}

static void __exit

RIDL_exit(void) {
    remove_proc_entry(RIDL_PROC_FILE_NAME, NULL);
    if (wbuffer != NULL) {
        free_pages_exact(wbuffer, RIDL_PAGE_SIZE * 300);
    }
    printk(KERN_INFO
    "[RIDL] RIDL Kernel Module unloaded. Goodbye!\n");
}

module_init(RIDL_init);
module_exit(RIDL_exit);

