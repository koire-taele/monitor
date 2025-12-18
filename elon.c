#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#define procfs_name "tsu"
static struct proc_dir_entry *our_proc_file = NULL;
static time64_t launch_seconds;
static const time64_t MARS_FLIGHT_SECONDS = 258 * 24 * 3600; // 258 дней

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t* offset)
{
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    launch_seconds = ts.tv_sec;
    char s[256];
    int len;
    ssize_t return_value;
    struct tm arrival_tm_local;
    struct tm launch_tm_local;
    time64_t arrival_time_kt;
    arrival_time_kt = launch_seconds + MARS_FLIGHT_SECONDS;
    time64_to_tm(launch_seconds, 0, &launch_tm_local);
    time64_to_tm(arrival_time_kt, 0, &arrival_tm_local);
    
    len = snprintf(s, sizeof(s),
                   "Launch date: %d-%d-%ld\nFlight time to Mars: %lld days\nArrival date on Mars: %d-%d-%ld\n",
                   launch_tm_local.tm_mday,
                   launch_tm_local.tm_mon + 1,
                   launch_tm_local.tm_year + 1900,
                   MARS_FLIGHT_SECONDS / (24 * 3600),
                   arrival_tm_local.tm_mday,
                   arrival_tm_local.tm_mon + 1,
                   arrival_tm_local.tm_year + 1900);

    if (*offset >= len) {return_value = 0;}
    else
    {
        if (copy_to_user(buffer, s + *offset, len - *offset)) {return_value = -EFAULT;}
        else
        {
            return_value = len - *offset;
            *offset += return_value;
            pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
        }
    }
    return return_value;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {.proc_read = procfile_read,};
#else
static const struct file_operations proc_file_fops = {.read = procfile_read,};
#endif

static int __init procfs1_init(void)
{
    our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
    if (!our_proc_file)
    {
        pr_err("Failed to create /proc/%s\n", procfs_name);
        return -ENOMEM;
    }
    pr_info("/proc/%s created at local time\n", procfs_name);
    return 0;
}

static void __exit procfs1_exit(void)
{
    proc_remove(our_proc_file);
    pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs1_init);
module_exit(procfs1_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mars arrival time calculator - 258 days constant");

MODULE_AUTHOR("G. Alemasov");
