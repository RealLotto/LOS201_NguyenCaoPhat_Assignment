// SPDX-License-Identifier: GPL-2.0
/*
 * LAB-02 character device with procfs and sysfs statistics.
 *
 * A single mutex protects the shared data buffer and driver state.  The
 * copy_{to,from}_user helpers are required because user pointers are not
 * kernel pointers and may fault while the current process's address space is
 * being accessed.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "lab2"
#define CLASS_NAME "lab2_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 240

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NguyenCaoPhat (SE201044)");
MODULE_DESCRIPTION("LAB-02 character device with procfs and sysfs");
MODULE_VERSION("1.0");

static char device_buffer[BUFFER_SIZE];
static size_t buffer_len;
static unsigned int open_count;
/* Các trạng thái mới dùng chung; mọi lần đọc/ghi đều được bảo vệ bởi lab2_mutex. */
static unsigned int reverse_read;
static unsigned int overwrite_count;
static struct class *lab2_class;
static struct device *lab2_device;
static struct proc_dir_entry *lab2_proc_entry;
static DEFINE_MUTEX(lab2_mutex);

/*
 * Count successful opens.  The shared counter is updated under the mutex.
 */
static int lab2_open(struct inode *inode, struct file *file)
{
	mutex_lock(&lab2_mutex);
	open_count++;
	mutex_unlock(&lab2_mutex);

	return 0;
}

/* open_count is a lifetime total, so closing does not decrement it. */
static int lab2_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Return the current message and advance this file descriptor's read offset.
 * copy_to_user validates and handles the user virtual address; memcpy would
 * dereference it as a kernel address and may fault in kernel mode.
 */
static ssize_t lab2_read(struct file *file, char __user *user_buffer,
			 size_t length, loff_t *offset)
{
	char *reversed = NULL;
	size_t remaining;
	size_t bytes_to_copy;
	size_t i;

	if (*offset < 0)
		return -EINVAL;
	if (!length)
		return 0;

	mutex_lock(&lab2_mutex);
	if (*offset >= buffer_len) {
		mutex_unlock(&lab2_mutex);
		return 0;
	}

	remaining = buffer_len - (size_t)*offset;
	bytes_to_copy = min(length, remaining);

	if (reverse_read) {
		/*
		 * Khi bật chế độ đảo, tạo dữ liệu tạm theo thứ tự từ cuối về
		 * đầu. Chỉ đảo phần tương ứng với offset hiện tại, vì vậy các
		 * lần read liên tiếp vẫn đọc đúng thứ tự. device_buffer không
		 * bị thay đổi.
		 */
		reversed = kmalloc(bytes_to_copy, GFP_KERNEL);
		if (!reversed) {
			mutex_unlock(&lab2_mutex);
			return -ENOMEM;
		}
		for (i = 0; i < bytes_to_copy; i++)
			reversed[i] = device_buffer[buffer_len - 1 -
						    (size_t)*offset - i];
	}

	if (copy_to_user(user_buffer,
			 reverse_read ? reversed : device_buffer + *offset,
			 bytes_to_copy)) {
		kfree(reversed);
		mutex_unlock(&lab2_mutex);
		pr_err("lab2_driver: copy_to_user failed\n");
		return -EFAULT;
	}

	*offset += bytes_to_copy;
	kfree(reversed);
	mutex_unlock(&lab2_mutex);

	return bytes_to_copy;
}

/*
 * Replace the shared message, truncating oversized writes to leave room for a
 * terminating NUL.  copy_from_user performs the checked user-to-kernel copy.
 */
static ssize_t lab2_write(struct file *file, const char __user *user_buffer,
			  size_t length, loff_t *offset)
{
	size_t bytes_to_copy = min(length, (size_t)BUFFER_SIZE - 1);

	if (!length)
		return 0;
	if (length > bytes_to_copy)
		pr_warn("lab2_driver: write truncated from %zu to %zu bytes\n",
			length, bytes_to_copy);

	mutex_lock(&lab2_mutex);
	if (copy_from_user(device_buffer, user_buffer, bytes_to_copy)) {
		mutex_unlock(&lab2_mutex);
		pr_err("lab2_driver: copy_from_user failed\n");
		return -EFAULT;
	}

	/*
	 * Mỗi write thay thế toàn bộ nội dung hiện tại. Chỉ tăng bộ đếm nếu
	 * buffer đã có dữ liệu trước khi ghi; cả kiểm tra lẫn cập nhật đều
	 * nằm trong mutex để các write đồng thời không làm sai kết quả.
	 */
	if (buffer_len > 0)
		overwrite_count++;
	buffer_len = bytes_to_copy;
	device_buffer[buffer_len] = '\0';
	mutex_unlock(&lab2_mutex);

	return bytes_to_copy;
}

/*
 * Tính vị trí mới theo quy ước của lseek: SEEK_SET tính từ đầu dữ liệu,
 * SEEK_CUR từ vị trí hiện tại và SEEK_END từ cuối dữ liệu. Kiểm tra tràn
 * số và giới hạn [0, buffer_len] trước khi cập nhật f_pos, nên nếu đầu vào
 * không hợp lệ thì vị trí cũ được giữ nguyên.
 */
static loff_t lab2_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t base;
	loff_t new_position;
	loff_t result;

	mutex_lock(&lab2_mutex);
	switch (whence) {
	case SEEK_SET:
		base = 0;
		break;
	case SEEK_CUR:
		base = file->f_pos;
		break;
	case SEEK_END:
		base = buffer_len;
		break;
	default:
		result = -EINVAL;
		goto out;
	}

	if (check_add_overflow(base, offset, &new_position) ||
	    new_position < 0 || new_position > buffer_len) {
		result = -EINVAL;
		goto out;
	}

	file->f_pos = new_position;
	result = new_position;
out:
	mutex_unlock(&lab2_mutex);
	return result;
}

static const struct file_operations lab2_fops = {
	.owner = THIS_MODULE,
	.open = lab2_open,
	.release = lab2_release,
	.read = lab2_read,
	.write = lab2_write,
	.llseek = lab2_llseek,
};

/*
 * seq_file safely streams proc output, including when a read spans multiple
 * buffers.  Snapshot shared state under the mutex, then format without
 * holding the lock.  single_open supplies the seq_file instance to show().
 */
static int lab2_proc_show(struct seq_file *seq, void *unused)
{
	char *last_data;
	size_t current_len;
	unsigned int current_open_count;
	unsigned int current_overwrite_count;

	last_data = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!last_data)
		return -ENOMEM;

	mutex_lock(&lab2_mutex);
	current_len = buffer_len;
	current_open_count = open_count;
	current_overwrite_count = overwrite_count;
	memcpy(last_data, device_buffer, current_len + 1);
	mutex_unlock(&lab2_mutex);

	seq_puts(seq, "LAB-02 Driver Statistics\n");
	seq_puts(seq, "========================\n");
	seq_printf(seq, "Driver name   : %s\n", DEVICE_NAME);
	seq_printf(seq, "Major number  : %d\n", DRIVER_MAJOR);
	seq_printf(seq, "Buffer size   : %d bytes\n", BUFFER_SIZE);
	seq_printf(seq, "Data length   : %zu bytes\n", current_len);
	seq_printf(seq, "Open count    : %u\n", current_open_count);
	/* In cùng giá trị đã chụp dưới mutex để tránh đọc bộ đếm không đồng bộ. */
	seq_printf(seq, "Overwrites    : %u\n", current_overwrite_count);
	if (current_len)
		seq_printf(seq, "Last data     : [%s]\n", last_data);

	kfree(last_data);
	return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_ops = {
	.proc_open = lab2_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*
 * sysfs callbacks report one small value per attribute.  Snapshot the
 * protected state under the mutex before formatting the text result.
 */
static ssize_t buffer_len_show(struct device *device,
			       struct device_attribute *attribute, char *buf)
{
	size_t current_len;

	mutex_lock(&lab2_mutex);
	current_len = buffer_len;
	mutex_unlock(&lab2_mutex);

	return sysfs_emit(buf, "%zu\n", current_len);
}

static ssize_t open_count_show(struct device *device,
			       struct device_attribute *attribute, char *buf)
{
	unsigned int current_open_count;

	mutex_lock(&lab2_mutex);
	current_open_count = open_count;
	mutex_unlock(&lab2_mutex);

	return sysfs_emit(buf, "%u\n", current_open_count);
}

static ssize_t reverse_read_show(struct device *device,
				 struct device_attribute *attribute, char *buf)
{
	unsigned int current_reverse_read;

	mutex_lock(&lab2_mutex);
	current_reverse_read = reverse_read;
	mutex_unlock(&lab2_mutex);

	return sysfs_emit(buf, "%u\n", current_reverse_read);
}

/*
 * Ghi giá trị chế độ từ sysfs: kstrtouint chuyển chuỗi thành số nguyên,
 * sau đó chỉ chấp nhận 0 hoặc 1. Chỉ cập nhật biến dùng chung sau khi
 * kiểm tra hợp lệ và giữ mutex để lab2_read thấy trạng thái nhất quán.
 */
static ssize_t reverse_read_store(struct device *device,
				  struct device_attribute *attribute,
				  const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return ret;
	if (value > 1)
		return -EINVAL;

	mutex_lock(&lab2_mutex);
	reverse_read = value;
	mutex_unlock(&lab2_mutex);

	return count;
}

/* Trả về bản chụp bộ đếm dưới mutex để sysfs đọc an toàn khi có write. */
static ssize_t overwrite_count_show(struct device *device,
				    struct device_attribute *attribute,
				    char *buf)
{
	unsigned int current_overwrite_count;

	mutex_lock(&lab2_mutex);
	current_overwrite_count = overwrite_count;
	mutex_unlock(&lab2_mutex);

	return sysfs_emit(buf, "%u\n", current_overwrite_count);
}

static ssize_t last_data_show(struct device *device,
			      struct device_attribute *attribute, char *buf)
{
	char *last_data;
	size_t current_len;
	ssize_t result;

	last_data = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!last_data)
		return -ENOMEM;

	mutex_lock(&lab2_mutex);
	current_len = buffer_len;
	memcpy(last_data, device_buffer, current_len + 1);
	mutex_unlock(&lab2_mutex);

	if (!current_len)
		result = sysfs_emit(buf, "(empty)\n");
	else
		result = sysfs_emit(buf, "%s\n", last_data);
	kfree(last_data);
	return result;
}

/*
 * DEVICE_ATTR_RW tạo thuộc tính reverse_read có cả show/store để đọc và
 * đổi chế độ; DEVICE_ATTR_RO tạo overwrite_count chỉ đọc. Nhóm thuộc tính
 * được gắn vào device nên xuất hiện trong /sys/class/lab2_class/lab2.
 */
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RW(reverse_read);
static DEVICE_ATTR_RO(overwrite_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attributes[] = {
	&dev_attr_buffer_len.attr,
	&dev_attr_open_count.attr,
	&dev_attr_reverse_read.attr,
	&dev_attr_overwrite_count.attr,
	&dev_attr_last_data.attr,
	NULL,
};

static const struct attribute_group lab2_attribute_group = {
	.attrs = lab2_attributes,
};

/*
 * Register the fixed-major character device and its userspace reporting
 * interfaces.  Each failure unwinds every resource already acquired.
 */
static int __init lab2_init(void)
{
	int ret;

	pr_info("lab2_driver: initializing module\n");

	ret = register_chrdev(DRIVER_MAJOR, DEVICE_NAME, &lab2_fops);
	if (ret < 0) {
		pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
		return ret;
	}

	lab2_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lab2_class)) {
		ret = PTR_ERR(lab2_class);
		pr_err("lab2_driver: class_create failed: %d\n", ret);
		goto err_unregister_chrdev;
	}

	lab2_device = device_create(lab2_class, NULL,
				    MKDEV(DRIVER_MAJOR, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lab2_device)) {
		ret = PTR_ERR(lab2_device);
		pr_err("lab2_driver: device_create failed: %d\n", ret);
		goto err_destroy_class;
	}

	ret = sysfs_create_group(&lab2_device->kobj, &lab2_attribute_group);
	if (ret) {
		pr_err("lab2_driver: sysfs_create_group failed: %d\n", ret);
		goto err_destroy_device;
	}

	lab2_proc_entry = proc_create("lab2_info", 0444, NULL,
				      &lab2_proc_ops);
	if (!lab2_proc_entry) {
		ret = -ENOMEM;
		pr_err("lab2_driver: proc_create failed\n");
		goto err_remove_sysfs;
	}

	pr_info("lab2_driver: loaded, major=%d\n", DRIVER_MAJOR);
	pr_info("lab2_driver: /proc/lab2_info created\n");
	return 0;

err_remove_sysfs:
	sysfs_remove_group(&lab2_device->kobj, &lab2_attribute_group);
err_destroy_device:
	device_destroy(lab2_class, MKDEV(DRIVER_MAJOR, 0));
err_destroy_class:
	class_destroy(lab2_class);
err_unregister_chrdev:
	unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
	return ret;
}

/* Remove reporting interfaces before destroying their parent device/class. */
static void __exit lab2_exit(void)
{
	proc_remove(lab2_proc_entry);
	sysfs_remove_group(&lab2_device->kobj, &lab2_attribute_group);
	device_destroy(lab2_class, MKDEV(DRIVER_MAJOR, 0));
	class_destroy(lab2_class);
	unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
	pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
