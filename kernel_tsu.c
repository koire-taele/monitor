#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#define procfs_name "tsu"
static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t* offset)
{
    char s[64];
    int len;
    ssize_t return_value;
    len = snprintf(s, sizeof(s), "test data");
    if (*offset >= len) {return_value = 0;}
    else
    {
        if (copy_to_user(buffer, s + *offset, len - *offset)) {return_value -EFAULT;}
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

