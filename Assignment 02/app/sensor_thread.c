// Reads sensor records on a monotonic schedule and passes them to the shared buffer.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "../driver/sms_sensor.h"

/*
 * Purpose: Adds a millisecond period to a normalized timespec.
 * Parameters: timespec to update and interval in milliseconds.
 * Returns: none.
 */
static void add_milliseconds(struct timespec *time, unsigned int milliseconds)
{
	time->tv_sec += milliseconds / 1000;
	time->tv_nsec += (long)(milliseconds % 1000) * 1000000L;
	if (time->tv_nsec >= 1000000000L) {
		time->tv_sec++;
		time->tv_nsec -= 1000000000L;
	}
}

/*
 * Purpose: Converts actual-minus-expected timespec to microseconds.
 * Parameters: actual and expected monotonic timespecs.
 * Returns: signed microsecond difference.
 */
static long long timespec_difference_us(const struct timespec *actual,
					const struct timespec *expected)
{
	return (long long)(actual->tv_sec - expected->tv_sec) * 1000000LL +
	       (actual->tv_nsec - expected->tv_nsec) / 1000LL;
}

/*
 * Purpose: Sleeps toward a deadline in bounded chunks for responsive shutdown.
 * Parameters: application context and absolute monotonic deadline.
 * Returns: 0 at deadline, ECANCELED on shutdown, or a positive clock error.
 */
static int sleep_until_deadline(struct sms_app *app,
				const struct timespec *deadline)
{
	for (;;) {
		struct timespec now;
		struct timespec wake;
		int result;

		if (atomic_load(&app->stop_requested))
			return ECANCELED;
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
			return errno;
		if (timespec_difference_us(&now, deadline) >= 0)
			return 0;
		wake = now;
		add_milliseconds(&wake, 50);
		if (wake.tv_sec > deadline->tv_sec ||
		    (wake.tv_sec == deadline->tv_sec &&
		     wake.tv_nsec > deadline->tv_nsec))
			wake = *deadline;
		result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wake, NULL);
		if (result != 0 && result != EINTR)
			return result;
	}
}

/*
 * Purpose: Reads and parses one timestamp/temperature/humidity CSV record.
 * Parameters: application context and output sample.
 * Returns: 1 for a parsed record, 0 for EOF, or -1 with errno set.
 */
static int read_sensor_sample(struct sms_app *app, struct sensor_data *sample)
{
	char line[128];
	ssize_t bytes;
	unsigned long long timestamp;

	for (;;) {
		if (atomic_load(&app->stop_requested)) {
			errno = ECANCELED;
			return -1;
		}
		bytes = read(app->sensor_fd, line, sizeof(line) - 1);
		if (bytes >= 0)
			break;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN) {
			struct timespec retry = { .tv_sec = 0, .tv_nsec = 1000000L };

			if (atomic_load(&app->stop_requested)) {
				errno = ECANCELED;
				return -1;
			}
			nanosleep(&retry, NULL);
			continue;
		}
		return -1;
	}
	if (bytes == 0)
		return 0;
	line[bytes] = '\0';
	if (sscanf(line, "%llu,%lf,%lf", &timestamp, &sample->temperature,
		   &sample->humidity) != 3) {
		fprintf(stderr, "Malformed sensor record: %s\n", line);
		errno = EPROTO;
		return -1;
	}
	sample->timestamp_ms = (uint64_t)timestamp;
	return 1;
}

/*
 * Purpose: Schedules periodic sensor reads and sends samples into the ring.
 * Parameters: application context passed as the pthread argument.
 * Returns: NULL after shutdown or a worker error.
 */
void *sensor_thread_main(void *argument)
{
	struct sms_app *app = argument;
	struct sched_param scheduling = { 0 };
	struct timespec expected;
	int policy = app->config.realtime ? SCHED_FIFO : SCHED_OTHER;
	int priority = app->config.realtime ? 50 : 0;
	int error;

	if (sms_app_log_thread_start(app, "sensor") != 0)
		return NULL;
	if (app->config.realtime &&
	    (priority > sched_get_priority_max(SCHED_FIFO) ||
	     priority < sched_get_priority_min(SCHED_FIFO))) {
		fprintf(stderr, "SCHED_FIFO priority 50 is unsupported.\n");
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return NULL;
	}
	scheduling.sched_priority = priority;
	error = pthread_setschedparam(pthread_self(), policy, &scheduling);
	if (error != 0) {
		fprintf(stderr, "pthread_setschedparam(%s): %s\n",
			app->config.realtime ? "SCHED_FIFO" : "SCHED_OTHER",
			strerror(error));
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return NULL;
	}
	if (ioctl(app->sensor_fd, SMS_SET_INTERVAL, &app->config.interval_ms) < 0) {
		perror("sensor thread SMS_SET_INTERVAL");
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return NULL;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &expected) != 0) {
		perror("clock_gettime");
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return NULL;
	}
	add_milliseconds(&expected, app->config.interval_ms);

	while (!atomic_load(&app->stop_requested)) {
		struct timespec actual;
		struct sensor_data sample;
		struct sms_log_message overflow = { .kind = SMS_LOG_OVERFLOW };
		long long jitter;
		int read_result;
		int wait_result;

		wait_result = sleep_until_deadline(app, &expected);
		if (wait_result == ECANCELED)
			break;
		if (wait_result != 0 && wait_result != EINTR) {
			fprintf(stderr, "clock_nanosleep: %s\n", strerror(wait_result));
			atomic_store(&app->worker_error, true);
			sms_app_request_stop(app);
			break;
		}
		if (atomic_load(&app->stop_requested))
			break;
		if (clock_gettime(CLOCK_MONOTONIC, &actual) != 0) {
			perror("clock_gettime");
			atomic_store(&app->worker_error, true);
			sms_app_request_stop(app);
			break;
		}
		jitter = timespec_difference_us(&actual, &expected);
		sms_app_record_jitter(app, jitter);
		read_result = read_sensor_sample(app, &sample);
		if (read_result < 0) {
			fprintf(stderr, "sensor read: %s\n", strerror(errno));
		} else if (read_result == 0) {
			fprintf(stderr, "sensor device returned EOF; retrying next period\n");
		} else {
			int push_result = circular_buffer_push(&app->samples, &sample);

			if (push_result == EAGAIN) {
				fprintf(stderr, "shared buffer full; sample dropped\n");
				overflow.timestamp_ms = sample.timestamp_ms;
				sms_app_log(app, &overflow);
			} else if (push_result != 0 && push_result != ECANCELED) {
				fprintf(stderr, "shared buffer push: %s\n",
					strerror(push_result));
				atomic_store(&app->worker_error, true);
				sms_app_request_stop(app);
				break;
			} else if (push_result == 0 && app->config.verbose) {
				printf("SAMPLE %llu,%.1f,%.1f\n",
				       (unsigned long long)sample.timestamp_ms,
				       sample.temperature, sample.humidity);
			}
		}
		add_milliseconds(&expected, app->config.interval_ms);
		if (clock_gettime(CLOCK_MONOTONIC, &actual) == 0 &&
		    (actual.tv_sec > expected.tv_sec ||
		     (actual.tv_sec == expected.tv_sec &&
		      actual.tv_nsec > expected.tv_nsec)))
			expected = actual;
	}
	return NULL;
}
