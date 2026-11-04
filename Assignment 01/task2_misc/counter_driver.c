// Misc device with an atomic read-and-increment counter and proc statistics.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/version.h>

#define COUNTER_DEVICE_NAME "counter"
#define COUNTER_PROC_NAME "counter_info"

static atomic_t counter_value = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);
static atomic_t total_increments = ATOMIC_INIT(0);
static atomic_t total_resets = ATOMIC_INIT(0);
static DEFINE_MUTEX(counter_value_mutex);

struct counter_read_state {
	char text[32];
	size_t length;
	struct mutex mutex;
};

/*
 * Purpose: Initializes per-open state used to hold a stable read snapshot.
 * Parameters: inode and file are supplied by VFS.
 * Returns: 0 on success or a negative errno.
 */
static int counter_open(struct inode *inode, struct file *file)
{
	struct counter_read_state *state;

	(void)inode;
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;
	mutex_init(&state->mutex);
	file->private_data = state;
	return 0;
}

/*
 * Purpose: Releases the per-open snapshot allocated by counter_open().
 * Parameters: inode and file are supplied by VFS.
 * Returns: 0 on success.
 */
static int counter_release(struct inode *inode, struct file *file)
{
	struct counter_read_state *state = file->private_data;

	(void)inode;
	mutex_destroy(&state->mutex);
	kfree(state);
	return 0;
}

/*
 * Purpose: Copies a stable snapshot and counts its first successful read.
 * Parameters: file, user_buffer, count, and offset are supplied by VFS.
 * Returns: bytes copied, 0 at EOF, or a negative errno.
 */
static ssize_t counter_read(struct file *file, char __user *user_buffer,
			    size_t count, loff_t *offset)
{
	struct counter_read_state *state = file->private_data;
	ssize_t result;
	bool starts_record;

	if (mutex_lock_interruptible(&state->mutex))
		return -ERESTARTSYS;
	starts_record = *offset == 0;
	if (starts_record) {
		mutex_lock(&counter_value_mutex);
		state->length = scnprintf(state->text, sizeof(state->text), "%d\n",
					  atomic_read(&counter_value));
	}
	result = simple_read_from_buffer(user_buffer, count, offset, state->text,
					 state->length);
	if (result > 0) {
		if (starts_record) {
			atomic_inc(&counter_value);
			atomic_inc(&total_reads);
			atomic_inc(&total_increments);
		}
	}
	if (starts_record)
		mutex_unlock(&counter_value_mutex);
	mutex_unlock(&state->mutex);
	return result;
}

/*
 * Purpose: Parses a non-negative decimal integer and replaces the counter.
 * Parameters: file, user_buffer, count, and offset are supplied by VFS.
 * Returns: bytes accepted or a negative errno.
 */
static ssize_t counter_write(struct file *file, const char __user *user_buffer,
			     size_t count, loff_t *offset)
{
	char input[32];
	char *number;
	size_t copy_length = min(count, sizeof(input) - 1);
	int value;
	int error;

	(void)file;
	(void)offset;
	if (count == 0 || count >= sizeof(input))
		return -EINVAL;
	if (copy_from_user(input, user_buffer, copy_length))
		return -EFAULT;
	input[copy_length] = '\0';
	number = strim(input);
	error = kstrtoint(number, 10, &value);
	if (error || value < 0)
		return -EINVAL;
	mutex_lock(&counter_value_mutex);
	atomic_set(&counter_value, value);
	mutex_unlock(&counter_value_mutex);
	atomic_inc(&total_writes);
	if (value == 0)
		atomic_inc(&total_resets);
	return count;
}

static const struct file_operations counter_file_operations = {
	.owner = THIS_MODULE,
	.open = counter_open,
	.release = counter_release,
	.read = counter_read,
	.write = counter_write,
	.llseek = default_llseek,
};

/*
 * Purpose: Emits the value and cumulative successful read/write counts.
 * Parameters: file is the seq_file output; unused is the private callback data.
 * Returns: 0 on success.
 */
static int counter_info_show(struct seq_file *file, void *unused)
{
	(void)unused;
	seq_printf(file, "counter_value: %d\n", atomic_read(&counter_value));
	seq_printf(file, "total_reads: %d\n", atomic_read(&total_reads));
	seq_printf(file, "total_writes: %d\n", atomic_read(&total_writes));
	seq_printf(file, "total_increments: %d\n",
		   atomic_read(&total_increments));
	seq_printf(file, "total_resets: %d\n", atomic_read(&total_resets));
	return 0;
}

/*
 * Purpose: Opens the proc statistics view using seq_file helpers.
 * Parameters: inode and file are supplied by procfs.
 * Returns: 0 on success or a negative errno.
 */
static int counter_info_open(struct inode *inode, struct file *file)
{
	(void)inode;
	return single_open(file, counter_info_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops counter_proc_operations = {
	.proc_open = counter_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations counter_proc_operations = {
	.owner = THIS_MODULE,
	.open = counter_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static struct miscdevice counter_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = COUNTER_DEVICE_NAME,
	.fops = &counter_file_operations,
	.mode = 0666,
};

/*
 * Purpose: Registers the misc device and proc entry, rolling back on failure.
 * Parameters: none.
 * Returns: 0 on success or a negative registration errno.
 */
static int __init counter_init(void)
{
	int error;

	atomic_set(&counter_value, 0);
	atomic_set(&total_reads, 0);
	atomic_set(&total_writes, 0);
	atomic_set(&total_increments, 0);
	atomic_set(&total_resets, 0);
	error = misc_register(&counter_misc_device);
	if (error)
		return error;
	if (!proc_create(COUNTER_PROC_NAME, 0444, NULL, &counter_proc_operations)) {
		misc_deregister(&counter_misc_device);
		return -ENOMEM;
	}
	pr_info(COUNTER_DEVICE_NAME ": registered /dev/%s and /proc/%s\n",
		COUNTER_DEVICE_NAME, COUNTER_PROC_NAME);
	return 0;
}

/*
 * Purpose: Removes procfs and misc-device registrations at module unload.
 * Parameters: none.
 * Returns: none.
 */
static void __exit counter_exit(void)
{
	remove_proc_entry(COUNTER_PROC_NAME, NULL);
	misc_deregister(&counter_misc_device);
	pr_info(COUNTER_DEVICE_NAME ": cleanup complete\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Cao Phat (SE201044)");
MODULE_DESCRIPTION("Assignment 01 misc counter with procfs statistics");
