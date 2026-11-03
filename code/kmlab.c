
#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "kmlab_given.h"

// Required kernel headers
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cougs");
MODULE_DESCRIPTION("CPTS360 KM PA");

#define DEBUG 1

// Store per-process data
typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

// Global variables
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_status;

static LIST_HEAD(process_list);
static DEFINE_MUTEX(process_lock);

// Timer and workqueue declarations
static void kmlab_timer_callback(struct timer_list *timer);
static void kmlab_work_function(struct work_struct *work);

static DEFINE_TIMER(kmlab_timer, kmlab_timer_callback);
static DECLARE_WORK(kmlab_work, kmlab_work_function);

// Read callback: display registered processes
static ssize_t kmlab_read(
    struct file *file,
    char __user *buffer,
    size_t count,
    loff_t *offset)
{
    char *output;
    size_t length = 0;
    ssize_t result;
    proc_list *entry;

    // Allocate memory for output
    output = kmalloc(PAGE_SIZE, GFP_KERNEL);

    if (!output)
        return -ENOMEM;

    // Protect the linked list
    mutex_lock(&process_lock);

    list_for_each_entry(entry, &process_list, list)
    {
        if (PAGE_SIZE - length < 64)
            break;

        length += scnprintf(
            output + length,
            PAGE_SIZE - length,
            "%u: %lu\n",
            entry->pid,
            entry->cpu_time
        );
    }

    mutex_unlock(&process_lock);

    // Return output to userspace
    result = simple_read_from_buffer(
        buffer,
        count,
        offset,
        output,
        length
    );

    kfree(output);

    return result;
}

// Write callback: register a process PID
static ssize_t kmlab_write(
    struct file *file,
    const char __user *buffer,
    size_t count,
    loff_t *offset)
{
    char pid_buffer[32];
    unsigned int pid;
    proc_list *entry;
    proc_list *existing_entry;
    int result;

    // Reject invalid input
    if (count == 0 || count >= sizeof(pid_buffer))
        return -EINVAL;

    // Copy PID from userspace
    if (copy_from_user(pid_buffer, buffer, count))
        return -EFAULT;

    pid_buffer[count] = '\0';

    // Convert PID string to integer
    result = kstrtouint(
        strim(pid_buffer), 10, &pid);

    if (result || pid == 0)
        return -EINVAL;

    // Allocate memory for the process
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);

    if (!entry)
        return -ENOMEM;

    entry->pid = pid;
    entry->cpu_time = 0;
    INIT_LIST_HEAD(&entry->list);

    // Protect the linked list
    mutex_lock(&process_lock);

    // Prevent duplicate registrations
    list_for_each_entry(existing_entry, &process_list, list)
    {
        if (existing_entry->pid == pid)
        {
            mutex_unlock(&process_lock);
            kfree(entry);
            return -EEXIST;
        }
    }

    // Add process to linked list
    list_add_tail(&entry->list, &process_list);

    mutex_unlock(&process_lock);

    pr_info("KMLAB: Registered PID %u\n", pid);

    return count;
}

// Timer callback: runs every five seconds
static void kmlab_timer_callback(struct timer_list *timer)
{
    // Schedule workqueue execution
    schedule_work(&kmlab_work);

    // Restart the timer
    mod_timer(
        &kmlab_timer,
        jiffies + 5 * HZ
    );
}

// Workqueue function
static void kmlab_work_function(struct work_struct *work)
{
    proc_list *entry;
    proc_list *temp;
    unsigned long cpu_value;

    mutex_lock(&process_lock);

    list_for_each_entry_safe(entry, temp, &process_list, list)
    {
        if (get_cpu_use(entry->pid, &cpu_value) == 0)
        {
            entry->cpu_time = cpu_value;
            pr_info("KMLAB: Updated PID %u, CPU time %lu\n", entry->pid, entry->cpu_time);
        }
        else
        {
            // Process no longer exists: remove it
            pr_info("KMLAB: Removing terminated PID %u\n", entry->pid);

            list_del(&entry->list);
            kfree(entry);
        }
    }

    mutex_unlock(&process_lock);
}

// Register read and write callbacks
static const struct proc_ops kmlab_proc_ops = {
    .proc_read = kmlab_read,
    .proc_write = kmlab_write,
};

// Called when the module is loaded
static int __init kmlab_init(void)
{
#ifdef DEBUG
    pr_info("KMLAB MODULE LOADING\n");
#endif

    // Create /proc/kmlab
    proc_dir = proc_mkdir("kmlab", NULL);

    if (!proc_dir)
    {
        pr_err("Failed to create /proc/kmlab\n");
        return -ENOMEM;
    }

    // Create /proc/kmlab/status
    proc_status = proc_create(
        "status", 0666, proc_dir, &kmlab_proc_ops);

    if (!proc_status)
    {
        pr_err("Failed to create /proc/kmlab/status\n");
        proc_remove(proc_dir);
        return -ENOMEM;
    }

    // Start five-second timer
    mod_timer(
        &kmlab_timer,
        jiffies + 5 * HZ
    );

    pr_info("KMLAB MODULE LOADED\n");

    return 0;
}

// Called when the module is unloaded
static void __exit kmlab_exit(void)
{
    proc_list *entry;
    proc_list *temp;

#ifdef DEBUG
    pr_info("KMLAB MODULE UNLOADING\n");
#endif

    // Stop timer and workqueue
    timer_shutdown_sync(&kmlab_timer);
    cancel_work_sync(&kmlab_work);

    // Remove /proc entries
    proc_remove(proc_status);
    proc_remove(proc_dir);

    // Free registered process records
    mutex_lock(&process_lock);

    list_for_each_entry_safe(
        entry, temp, &process_list, list)
    {
        list_del(&entry->list);
        kfree(entry);
    }

    mutex_unlock(&process_lock);

    pr_info("KMLAB MODULE UNLOADED\n");
}

// Register initialization and cleanup functions
module_init(kmlab_init);
module_exit(kmlab_exit);
