// Character-device driver with ioctl controls, synchronized buffer I/O, and usage statistics.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "asgn1_ioctl.h"

#define ASGN1_DEVICE_NAME "asgn1"
/* Major 241 is already registered by the target kernel; 242 is available. */
#define ASGN1_MAJOR 242
#define ASGN1_BUFFER_SIZE 1024
#define ASGN1_KERNEL_PREFIX "[KERNEL]"
#define ASGN1_VERSION "asgn1_driver v1.0 - SE201044"

static char device_buffer[ASGN1_BUFFER_SIZE];
static size_t device_buffer_len;
static int operating_mode = 1;
static int open_count;
static int write_count;
static int read_count;
static int last_write_size;
static DEFINE_MUTEX(device_mutex);

/*
 * Purpose: Opens the device and records the cumulative number of opens.
 * Parameters: inode and file are supplied by VFS and unused.
 * Returns: 0 on success.
 */
static int asgn1_open(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;
	mutex_lock(&device_mutex);
	open_count++;
	mutex_unlock(&device_mutex);
	pr_info(ASGN1_DEVICE_NAME ": opened\n");
	return 0;
}

/*
 * Purpose: Closes a file instance; this driver holds no per-open allocation.
 * Parameters: inode and file are supplied by VFS and unused.
 * Returns: 0 on success.
 */
static int asgn1_release(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;
	pr_info(ASGN1_DEVICE_NAME ": released\n");
	return 0;
}

/*
 * Purpose: Copies remaining buffer bytes to userspace and advances the offset.
 * Parameters: file, user_buffer, count, and offset are provided by VFS.
 * Returns: bytes copied, 0 at EOF, or a negative errno.
 */
static ssize_t asgn1_read(struct file *file, char __user *user_buffer,
			  size_t count, loff_t *offset)
{
	size_t remaining;
	size_t bytes_to_copy;

	if (count == 0)
		return 0;
	if (mutex_lock_interruptible(&device_mutex))
		return -ERESTARTSYS;
	if (*offset < 0 || (size_t)*offset >= device_buffer_len) {
		mutex_unlock(&device_mutex);
		return 0;
	}

	remaining = device_buffer_len - (size_t)*offset;
	bytes_to_copy = min(count, remaining);
	if (copy_to_user(user_buffer, device_buffer + *offset, bytes_to_copy)) {
		mutex_unlock(&device_mutex);
		return -EFAULT;
	}
	*offset += bytes_to_copy;
	read_count++;
	mutex_unlock(&device_mutex);
	return bytes_to_copy;
}

/*
 * Purpose: Replaces the buffer, optionally prefixing writes in kernel mode.
 * Parameters: file, user_buffer, count, and offset are provided by VFS.
 * Returns: payload bytes accepted or a negative errno.
 */
static ssize_t asgn1_write(struct file *file, const char __user *user_buffer,
			   size_t count, loff_t *offset)
{
	char input[ASGN1_BUFFER_SIZE];
	size_t prefix_len = 0;
	size_t payload_len;

	(void)file;
	(void)offset;
	if (count > sizeof(input))
		return -ENOSPC;
	if (copy_from_user(input, user_buffer, count))
		return -EFAULT;

	if (mutex_lock_interruptible(&device_mutex))
		return -ERESTARTSYS;
	if (operating_mode == 0)
		prefix_len = sizeof(ASGN1_KERNEL_PREFIX) - 1;
	if (count > sizeof(device_buffer) - prefix_len) {
		mutex_unlock(&device_mutex);
		return -ENOSPC;
	}

	if (prefix_len)
		memcpy(device_buffer, ASGN1_KERNEL_PREFIX, prefix_len);
	payload_len = count;
	memcpy(device_buffer + prefix_len, input, payload_len);
	device_buffer_len = prefix_len + payload_len;
	last_write_size = count;
	write_count++;
	mutex_unlock(&device_mutex);
	return count;
}

/*
 * Purpose: Handles the four documented ioctls and rejects unknown requests.
 * Parameters: file, command, and argument are supplied by the ioctl syscall.
 * Returns: 0 on success or a negative errno.
 */
static long asgn1_ioctl(struct file *file, unsigned int command,
			unsigned long argument)
{
	struct asgn1_stats stats;
	int requested_mode;
	char version[ASGN1_VERSION_SIZE] = { 0 };
	void __user *user_argument = (void __user *)argument;

	(void)file;
	switch (command) {
	case ASGN1_RESET_BUFFER:
		if (mutex_lock_interruptible(&device_mutex))
			return -ERESTARTSYS;
		memset(device_buffer, 0, sizeof(device_buffer));
		device_buffer_len = 0;
		write_count = 0;
		read_count = 0;
		last_write_size = 0;
		mutex_unlock(&device_mutex);
		pr_info(ASGN1_DEVICE_NAME ": ioctl RESET_BUFFER\n");
		return 0;

	case ASGN1_GET_STATS:
		if (mutex_lock_interruptible(&device_mutex))
			return -ERESTARTSYS;
		stats.open_count = open_count;
		stats.write_count = write_count;
		stats.read_count = read_count;
		stats.buffer_len = device_buffer_len;
		stats.last_write_size = last_write_size;
		mutex_unlock(&device_mutex);
		pr_info(ASGN1_DEVICE_NAME ": ioctl GET_STATS\n");
		return copy_to_user(user_argument, &stats, sizeof(stats)) ? -EFAULT : 0;

	case ASGN1_SET_MODE:
		if (get_user(requested_mode, (int __user *)user_argument))
			return -EFAULT;
		if (requested_mode != 0 && requested_mode != 1)
			return -EINVAL;
		if (mutex_lock_interruptible(&device_mutex))
			return -ERESTARTSYS;
		operating_mode = requested_mode;
		mutex_unlock(&device_mutex);
		pr_info(ASGN1_DEVICE_NAME ": ioctl SET_MODE=%d\n", requested_mode);
		return 0;

	case ASGN1_GET_VERSION:
		strscpy(version, ASGN1_VERSION, sizeof(version));
		pr_info(ASGN1_DEVICE_NAME ": ioctl GET_VERSION\n");
		return copy_to_user(user_argument, version, sizeof(version)) ? -EFAULT : 0;

	default:
		pr_info(ASGN1_DEVICE_NAME ": ioctl unknown command=0x%x\n", command);
		return -ENOTTY;
	}
}

static const struct file_operations asgn1_file_operations = {
	.owner = THIS_MODULE,
	.open = asgn1_open,
	.release = asgn1_release,
	.read = asgn1_read,
	.write = asgn1_write,
	.unlocked_ioctl = asgn1_ioctl,
	/* Seeking is unsupported; keep the current file position unchanged. */
	.llseek = noop_llseek,
};

/*
 * Purpose: Registers the fixed-major character device.
 * Parameters: none.
 * Returns: 0 on success or a negative registration errno.
 */
static int __init asgn1_init(void)
{
	int error = register_chrdev(ASGN1_MAJOR, ASGN1_DEVICE_NAME,
				    &asgn1_file_operations);

	/* A fixed-major registration returns zero on success. */
	if (error < 0)
		return error;
	pr_info(ASGN1_DEVICE_NAME ": registered major=%d; create /dev/%s with mknod\n",
		ASGN1_MAJOR, ASGN1_DEVICE_NAME);
	return 0;
}

/*
 * Purpose: Unregisters the character device and releases its major number.
 * Parameters: none.
 * Returns: none.
 */
static void __exit asgn1_exit(void)
{
	unregister_chrdev(ASGN1_MAJOR, ASGN1_DEVICE_NAME);
	pr_info(ASGN1_DEVICE_NAME ": cleanup complete\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Cao Phat (SE201044)");
MODULE_DESCRIPTION("Assignment 01 ioctl character device driver");
