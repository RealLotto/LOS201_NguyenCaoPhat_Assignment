#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>

#define ASGN1_IOC_MAGIC 0xA1
#define ASGN1_VERSION_SIZE 64

struct asgn1_stats {
	int open_count;
	int write_count;
	int read_count;
	int buffer_len;
	int last_write_size;
};

#define ASGN1_RESET_BUFFER _IO(ASGN1_IOC_MAGIC, 0)
#define ASGN1_GET_STATS _IOR(ASGN1_IOC_MAGIC, 1, struct asgn1_stats)
#define ASGN1_SET_MODE _IOW(ASGN1_IOC_MAGIC, 2, int)
#define ASGN1_GET_VERSION _IOR(ASGN1_IOC_MAGIC, 3, char[ASGN1_VERSION_SIZE])

#endif
