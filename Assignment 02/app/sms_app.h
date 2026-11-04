#ifndef SMS_APP_H
#define SMS_APP_H

#include <mqueue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMS_SAMPLE_CAPACITY 64
#define SMS_JITTER_CAPACITY 100000
#define SMS_DEVICE_DEFAULT "/dev/sms_sensor"
#define SMS_LOG_DEFAULT "/tmp/sms.log"
#define SMS_JITTER_DEFAULT "jitter_report.txt"

enum sms_alarm_type {
	SMS_TEMP_HIGH = 0,
	SMS_HUMID_HIGH = 1,
	SMS_TEMP_SPIKE = 2,
	SMS_ALARM_TYPE_COUNT = 3
};

enum sms_log_kind {
	SMS_LOG_SAMPLE = 1,
	SMS_LOG_ALERT = 2,
	SMS_LOG_OVERFLOW = 3,
	SMS_LOG_STOP = 4,
	SMS_LOG_THREAD = 5
};

struct sensor_data {
	uint64_t timestamp_ms;
	double temperature;
	double humidity;
};

struct circular_buffer {
	struct sensor_data *items;
	size_t capacity;
	size_t head;
	size_t tail;
	size_t size;
	uint64_t total_produced;
	uint64_t total_consumed;
	uint64_t overflow_count;
	bool closed;
	pthread_mutex_t mutex;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
};

struct sms_config {
	unsigned int interval_ms;
	double temperature_threshold;
	double humidity_threshold;
	unsigned int duration_seconds;
	bool verbose;
	bool realtime;
	const char *device_path;
	const char *log_path;
	const char *jitter_path;
};

struct sms_alarm_message {
	int type;
	uint64_t timestamp_ms;
	double value;
	double threshold;
};

struct sms_log_message {
	int kind;
	uint64_t timestamp_ms;
	double temperature;
	double humidity;
	int alarm_type;
	double value;
	double threshold;
	char thread_name[32];
};

struct sms_app {
	struct sms_config config;
	struct circular_buffer samples;
	mqd_t alarm_queue;
	mqd_t log_queue;
	char alarm_queue_name[64];
	char log_queue_name[64];
	atomic_bool stop_requested;
	atomic_bool worker_error;
	pthread_mutex_t stats_mutex;
	uint64_t alert_counts[SMS_ALARM_TYPE_COUNT];
	uint64_t processed_samples;
	int sensor_fd;
	int queues_initialized;
	int buffer_initialized;
	int stats_mutex_initialized;
	long long *jitter_us;
	size_t jitter_count;
	size_t jitter_capacity;
	pthread_mutex_t jitter_mutex;
	int jitter_mutex_initialized;
};

struct circular_buffer_stats {
	size_t current_size;
	uint64_t total_produced;
	uint64_t total_consumed;
	uint64_t overflow_count;
};

/* Initializes heap storage and synchronization primitives for the ring. */
int circular_buffer_init(struct circular_buffer *buffer, size_t capacity);
/* Adds a sample or returns EAGAIN when the bounded ring is full. */
int circular_buffer_push(struct circular_buffer *buffer,
			 const struct sensor_data *sample);
/* Waits for one sample; returns zero after drain or a negative pthread error. */
int circular_buffer_pop(struct circular_buffer *buffer,
			struct sensor_data *sample);
/* Wakes blocked readers and writers and rejects further writes. */
void circular_buffer_close(struct circular_buffer *buffer);
/* Copies ring occupancy and lifetime counters under the ring mutex. */
void circular_buffer_get_stats(struct circular_buffer *buffer,
			       struct circular_buffer_stats *stats);
/* Destroys synchronization primitives and releases ring storage. */
void circular_buffer_destroy(struct circular_buffer *buffer);

/* Reads samples on an absolute monotonic schedule and feeds the ring. */
void *sensor_thread_main(void *argument);
/* Computes rolling averages, emits alerts, and queues normal log messages. */
void *processor_thread_main(void *argument);
/* Receives alerts, prints them, counts types, and forwards them to logger. */
void *alert_thread_main(void *argument);
/* Writes queued messages, rotates the log, and flushes periodically. */
void *logger_thread_main(void *argument);

/* Signals every worker to stop and wakes condition-variable/message-queue waits. */
void sms_app_request_stop(struct sms_app *app);
/* Queues a formatted log event, reporting queue failures to stderr. */
int sms_app_log(struct sms_app *app, const struct sms_log_message *message);
/* Queues a worker-start event so the logger records each thread's activity. */
int sms_app_log_thread_start(struct sms_app *app, const char *thread_name);
/* Records one non-negative sensor-thread wake-up jitter sample. */
void sms_app_record_jitter(struct sms_app *app, long long jitter);

#endif
