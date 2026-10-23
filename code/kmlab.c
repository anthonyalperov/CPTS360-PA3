#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "kmlab_given.h"
// Include headers as needed ...
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/init.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cougs"); // Change with your lastname
MODULE_DESCRIPTION("CPTS360 KM PA");

#define DEBUG 1

// store per process data (you may modify it as needed)
typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

// Global variables as needed ...
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_status;

static ssize_t kmlab_read (struct file *file, char __user *buffer, size_t count, loff_t *offset)
{
   const char message[] = "KMLAB status ready\n";
   size_t len = sizeof(message) - 1;

   return simple_read_from_buffer(buffer, count, offset, message, len);
}

static const struct proc_ops kmlab_proc_ops = { .proc_read = kmlab_read,};


// kmlab_init - Called when the module is loaded
int __init kmlab_init(void)
{
   #ifdef DEBUG
   pr_info("KMLAB MODULE LOADING\n");
   #endif
   // Insert your code here ...
   if (!proc_dir)
   {
      pr_err("Failed to create /proc/kmlab\n");
      return -ENOMEM;
   }
   
   proc_status = proc_create("status", 0444, proc_dir, NULL);

   if (!proc_status)
   {
      pr_err("Failed to create /proc/kmlab\n");
      proc_remove(proc_dir);
      return -ENOMEM;
   }
   
   pr_info("KMLAB MODULE LOADED\n");
   return 0;   
}

// kmlab_exit - Called when the module is unloaded
void __exit kmlab_exit(void)
{
   #ifdef DEBUG
   pr_info("KMLAB MODULE UNLOADING\n");
   #endif
   // Insert your code here ...
   
   

   pr_info("KMLAB MODULE UNLOADED\n");
}

// Register init and exit functions
module_init(kmlab_init);
module_exit(kmlab_exit);
