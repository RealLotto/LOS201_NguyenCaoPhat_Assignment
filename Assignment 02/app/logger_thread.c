// Writes queued events to the log, rotates oversized files, and flushes periodically.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define SMS_LOG_ROTATE_BYTES (1024U * 1024U)

static const char *const logger_alarm_names[SMS_ALARM_TYPE_COUNT] = {
	"TEMP_HIGH", "HUMID_HIGH", "TEMP_SPIKE"
};

/*
 * Purpose: Formats current UTC wall time for a persisted record.
 * Parameters: destination buffer, buffer size, and output milliseconds field.
 * Returns: 0 on success or -1 on clock/conversion failure.
 */
static int format_wall_timestamp(char *buffer, size_t buffer_size,
				 long *milliseconds)
{
	struct timespec now;
	struct tm utc;

	if (clock_gettime(CLOCK_REALTIME, &now) != 0)
		return -1;
	if (!gmtime_r(&now.tv_sec, &utc))
		return -1;
	if (strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S", &utc) == 0)
		return -1;
	*milliseconds = now.tv_nsec / 1000000L;
	return 0;
}

/*
 * Purpose: Flushes logger output and reports stream errors.
 * Parameters: open output stream.
 * Returns: 0 on success or -1 on stream failure.
 */
static int flush_log(FILE *log_file)
{
	if (fflush(log_file) != 0) {
		perror("fflush sms log");
		return -1;
	}
	return 0;
}

/*
 * Purpose: Rotates an oversized log and opens a new active file.
 * Parameters: pointer to logger stream and active log path.
 * Returns: 0 when unchanged/rotated or -1 on filesystem failure.
 */
static int rotate_log_if_needed(FILE **log_file, const char *path)
{
	struct stat status;
	char rotated_path[1024];

	if (fflush(*log_file) != 0) {
		perror("flush sms log before size check");
		return -1;
	}
	if (stat(path, &status) != 0) {
		if (errno == ENOENT)
			return 0;
		perror("stat sms log");
		return -1;
	}
	/* st_size is signed; compare it with the threshold in the same type. */
	if (status.st_size <= (off_t)SMS_LOG_ROTATE_BYTES)
		return 0;
	if (snprintf(rotated_path, sizeof(rotated_path), "%s.1", path) >=
	    (int)sizeof(rotated_path)) {
		fprintf(stderr, "log path too long for rotation\n");
		return -1;
	}
	if (fclose(*log_file) != 0) {
		perror("close sms log before rotate");
		*log_file = NULL;
		return -1;
	}
	*log_file = NULL;
	if (rename(path, rotated_path) != 0) {
		perror("rename sms log");
		return -1;
	}
	*log_file = fopen(path, "a");
	if (!*log_file) {
		perror("reopen sms log after rotate");
		return -1;
	}
	return 0;
}

/*
 * Purpose: Serializes one typed message to the log stream.
 * Parameters: open stream and message received from a worker queue.
 * Returns: 0 on success or -1 on formatting/write failure.
 */
static int write_log_message(FILE *log_file, const struct sms_log_message *message)
{
	char wall_timestamp[32];
	long milliseconds;

	if (format_wall_timestamp(wall_timestamp, sizeof(wall_timestamp),
				  &milliseconds) != 0) {
		perror("format log timestamp");
		return -1;
	}
	switch (message->kind) {
	case SMS_LOG_SAMPLE:
		return fprintf(log_file,
			       "timestamp=%s.%03ldZ monotonic_ms=%llu SAMPLE "
			       "temperature=%.1f humidity=%.1f\n",
			       wall_timestamp, milliseconds,
			       (unsigned long long)message->timestamp_ms,
			       message->temperature, message->humidity) < 0 ? -1 : 0;
	case SMS_LOG_ALERT:
		return fprintf(log_file,
			       "timestamp=%s.%03ldZ monotonic_ms=%llu ALERT "
			       "type=%s value=%.1f threshold=%.1f\n",
			       wall_timestamp, milliseconds,
			       (unsigned long long)message->timestamp_ms,
			       message->alarm_type >= 0 &&
				       message->alarm_type < SMS_ALARM_TYPE_COUNT
				   ? logger_alarm_names[message->alarm_type] : "UNKNOWN",
			       message->value, message->threshold) < 0 ? -1 : 0;
	case SMS_LOG_OVERFLOW:
		return fprintf(log_file,
			       "timestamp=%s.%03ldZ monotonic_ms=%llu "
			       "WARNING shared_buffer_full\n",
			       wall_timestamp, milliseconds,
			       (unsigned long long)message->timestamp_ms) < 0 ? -1 : 0;
	case SMS_LOG_THREAD:
		return fprintf(log_file, "timestamp=%s.%03ldZ THREAD %s started\n",
			       wall_timestamp, milliseconds,
			       message->thread_name) < 0 ? -1 : 0;
	default:
		fprintf(stderr, "unknown log message kind: %d\n", message->kind);
		return -1;
	}
}

/*
 * Purpose: Receives queued records, rotates, and flushes on a five-second cadence.
 * Parameters: application context passed as the pthread argument.
 * Returns: NULL after draining the stop marker or a worker error.
 */
void *logger_thread_main(void *argument)
{
	struct sms_app *app = argument;
	FILE *log_file = fopen(app->config.log_path, "a");
	int result = 0;

	if (!log_file) {
		perror("open sms log");
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return NULL;
	}
	{
		char wall_timestamp[32];
		long milliseconds;

		if (format_wall_timestamp(wall_timestamp, sizeof(wall_timestamp),
					  &milliseconds) != 0 ||
		    fprintf(log_file, "timestamp=%s.%03ldZ THREAD logger started\n",
			    wall_timestamp, milliseconds) < 0) {
			perror("write logger startup event");
			atomic_store(&app->worker_error, true);
			sms_app_request_stop(app);
			fclose(log_file);
			return NULL;
		}
	}
	for (;;) {
		struct sms_log_message message;
		struct timespec deadline;
		ssize_t bytes;

		if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
			perror("clock_gettime logger");
			result = -1;
			break;
		}
		deadline.tv_sec += 5;
		bytes = mq_timedreceive(app->log_queue, (char *)&message,
					sizeof(message), NULL, &deadline);
		if (bytes < 0) {
			if (errno == EINTR)
				continue;
			if (errno == ETIMEDOUT) {
				if (flush_log(log_file) != 0)
					result = -1;
				continue;
			}
			perror("mq_timedreceive log");
			result = -1;
			break;
		}
		if ((size_t)bytes != sizeof(message)) {
			fprintf(stderr, "invalid log message size: %zd\n", bytes);
			result = -1;
			continue;
		}
		if (message.kind == SMS_LOG_STOP)
			break;
		if (rotate_log_if_needed(&log_file, app->config.log_path) != 0) {
			result = -1;
			break;
		}
		if (write_log_message(log_file, &message) != 0) {
			perror("write sms log");
			result = -1;
			break;
		}
	}
	if (log_file) {
		if (flush_log(log_file) != 0)
			result = -1;
		if (fclose(log_file) != 0) {
			perror("close sms log");
			result = -1;
		}
	}
	if (result != 0) {
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
	}
	return NULL;
}
