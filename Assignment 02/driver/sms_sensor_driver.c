// Misc device that generates temperature/humidity readings and exposes proc statistics.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "sms_sensor.h"

#define SMS_DEVICE_NAME "sms_sensor"
#define SMS_PROC_NAME "sms_stats"

static DEFINE_MUTEX(sensor_mutex);
static unsigned int minimum_interval_ms = SMS_DEFAULT_INTERVAL_MS;
static u64 read_count;
static u64 start_time_ms;
static u64 last_read_ms;
static unsigned int last_temperature_tenths = 250;
static unsigned int last_humidity_tenths = 500;

/*
 * Purpose: Produces a CSV record and enforces the configured minimum interval.
 * Parameters: VFS file, userspace buffer, byte count, and file offset.
 * Returns: bytes copied, 0 for EOF/zero-size reads, or a negative errno.
 */
static ssize_t sms_sensor_read(struct file *file, char __user *user_buffer,
			       size_t count, loff_t *offset)
{
	char record[64];
	u64 now_ms;
	unsigned int temperature;
	unsigned int humidity;
	int length;
	ssize_t result;

	(void)file;
	if (count == 0)
		return 0;
	if (*offset != 0)
		return 0;
	if (mutex_lock_interruptible(&sensor_mutex))
		return -ERESTARTSYS;
	now_ms = ktime_to_ms(ktime_get());
	if (last_read_ms && now_ms - last_read_ms < minimum_interval_ms) {
		mutex_unlock(&sensor_mutex);
		return -EAGAIN;
	}

	temperature = 150 + get_random_u32() % 301;
	humidity = 300 + get_random_u32() % 601;
	last_temperature_tenths = temperature;
	last_humidity_tenths = humidity;
	last_read_ms = now_ms;
	length = scnprintf(record, sizeof(record), "%llu,%u.%u,%u.%u\n",
			   now_ms, temperature / 10, temperature % 10,
			   humidity / 10, humidity % 10);
	result = simple_read_from_buffer(user_buffer, count, offset, record, length);
	if (result > 0) {
		read_count++;
		*offset = 0;
	}
	mutex_unlock(&sensor_mutex);
	return result;
}

/*
 * Purpose: Sets a validated minimum interval via SMS_SET_INTERVAL.
 * Parameters: VFS file, ioctl request, and userspace argument address.
 * Returns: 0 on success or a negative errno.
 */
static long sms_sensor_ioctl(struct file *file, unsigned int command,
			     unsigned long argument)
{
	unsigned int interval_ms;

	(void)file;
	if (command != SMS_SET_INTERVAL)
		return -ENOTTY;
	if (get_user(interval_ms, (unsigned int __user *)argument))
		return -EFAULT;
	if (interval_ms < SMS_MIN_INTERVAL_MS ||
	    interval_ms > SMS_MAX_INTERVAL_MS)
		return -EINVAL;
	mutex_lock(&sensor_mutex);
	minimum_interval_ms = interval_ms;
	mutex_unlock(&sensor_mutex);
	pr_info(SMS_DEVICE_NAME ": interval set to %u ms\n", interval_ms);
	return 0;
}

static const struct file_operations sms_sensor_file_operations = {
	.owner = THIS_MODULE,
	.read = sms_sensor_read,
	.unlocked_ioctl = sms_sensor_ioctl,
	/* Seeking is unsupported; keep the current file position unchanged. */
	.llseek = noop_llseek,
};

static struct miscdevice sms_sensor_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SMS_DEVICE_NAME,
	.fops = &sms_sensor_file_operations,
	.mode = 0666,
};

/*
 * Purpose: Reports counters, latest values, and elapsed module uptime.
 * Parameters: seq_file output and unused callback data.
 * Returns: 0 on success.
 */
static int sms_stats_show(struct seq_file *file, void *unused)
{
	u64 uptime_seconds;

	(void)unused;
	mutex_lock(&sensor_mutex);
	/* Use the kernel's 64-bit divide helper on 32-bit ARM. */
	uptime_seconds = div_u64(ktime_to_ms(ktime_get()) - start_time_ms, 1000);
	seq_printf(file, "read_count: %llu\n", read_count);
	seq_printf(file, "last_temperature: %u.%u C\n",
		   last_temperature_tenths / 10, last_temperature_tenths % 10);
	seq_printf(file, "last_humidity: %u.%u %%\n",
		   last_humidity_tenths / 10, last_humidity_tenths % 10);
	seq_printf(file, "uptime_seconds: %llu\n", uptime_seconds);
	mutex_unlock(&sensor_mutex);
	return 0;
}

/*
 * Purpose: Opens proc statistics using the seq_file interface.
 * Parameters: proc inode and file supplied by the VFS.
 * Returns: 0 on success or a negative errno.
 */
static int sms_stats_open(struct inode *inode, struct file *file)
{
	(void)inode;
	return single_open(file, sms_stats_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops sms_stats_proc_operations = {
	.proc_open = sms_stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations sms_stats_proc_operations = {
	.owner = THIS_MODULE,
	.open = sms_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*
 * Purpose: Registers the sensor device and proc entry with rollback on failure.
 * Parameters: none.
 * Returns: 0 on success or a negative registration errno.
 */
static int __init sms_sensor_init(void)
{
	int error;

	start_time_ms = ktime_to_ms(ktime_get());
	read_count = 0;
	last_read_ms = 0;
	error = misc_register(&sms_sensor_misc_device);
	if (error)
		return error;
	if (!proc_create(SMS_PROC_NAME, 0444, NULL, &sms_stats_proc_operations)) {
		misc_deregister(&sms_sensor_misc_device);
		return -ENOMEM;
	}
	pr_info(SMS_DEVICE_NAME ": registered /dev/%s and /proc/%s\n",
		SMS_DEVICE_NAME, SMS_PROC_NAME);
	return 0;
}

/*
 * Purpose: Removes both exported interfaces when the module is unloaded.
 * Parameters: none.
 * Returns: none.
 */
static void __exit sms_sensor_exit(void)
{
	remove_proc_entry(SMS_PROC_NAME, NULL);
	misc_deregister(&sms_sensor_misc_device);
	pr_info(SMS_DEVICE_NAME ": cleanup complete\n");
}

module_init(sms_sensor_init);
module_exit(sms_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Cao Phat (SE201044)");
MODULE_DESCRIPTION("Embedded Linux simulated temperature and humidity sensor");
