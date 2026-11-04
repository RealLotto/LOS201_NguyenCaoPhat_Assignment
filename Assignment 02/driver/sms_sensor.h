#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

#define SMS_IOC_MAGIC 0xB7
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, unsigned int)
#define SMS_DEFAULT_INTERVAL_MS 100
#define SMS_MIN_INTERVAL_MS 1
#define SMS_MAX_INTERVAL_MS 60000

#endif
