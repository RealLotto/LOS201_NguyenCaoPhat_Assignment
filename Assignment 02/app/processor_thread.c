// Processes sensor samples, maintains rolling averages, and queues threshold alerts.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Purpose: Sends an alert with a bounded timeout.
 * Parameters: application context and alert message.
 * Returns: 0 on success or -1 on queue/clock failure.
 */
static int send_alarm(struct sms_app *app, const struct sms_alarm_message *alarm)
{
	struct timespec deadline;

	if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
		perror("clock_gettime alarm queue");
		return -1;
	}
	deadline.tv_sec++;
	while (mq_timedsend(app->alarm_queue, (const char *)alarm, sizeof(*alarm),
			    0, &deadline) < 0) {
		if (errno == EINTR)
			continue;
		perror("mq_timedsend alarm");
		return -1;
	}
	return 0;
}

/*
 * Purpose: Computes rolling averages and queues samples and threshold alerts.
 * Parameters: application context passed as the pthread argument.
 * Returns: NULL after draining the closed ring or a worker error.
 */
void *processor_thread_main(void *argument)
{
	struct sms_app *app = argument;
	double temperatures[10] = { 0 };
	double humidities[10] = { 0 };
	size_t window_size = 0;
	size_t next = 0;
	double previous_temperature = 0.0;
	int have_previous = 0;
	struct sensor_data sample;
	int result;

	if (sms_app_log_thread_start(app, "processor") != 0)
		return NULL;
	while ((result = circular_buffer_pop(&app->samples, &sample)) > 0) {
		double temperature_sum = 0.0;
		double humidity_sum = 0.0;
		size_t index;
		struct sms_log_message log_message = {
			.kind = SMS_LOG_SAMPLE,
			.timestamp_ms = sample.timestamp_ms,
			.temperature = sample.temperature,
			.humidity = sample.humidity
		};
		struct sms_alarm_message alarms[3];
		size_t alarm_count = 0;

		temperatures[next] = sample.temperature;
		humidities[next] = sample.humidity;
		next = (next + 1) % 10;
		if (window_size < 10)
			window_size++;
		for (index = 0; index < window_size; index++) {
			temperature_sum += temperatures[index];
			humidity_sum += humidities[index];
		}

		if (sample.temperature > app->config.temperature_threshold) {
			alarms[alarm_count++] = (struct sms_alarm_message) {
				.type = SMS_TEMP_HIGH,
				.timestamp_ms = sample.timestamp_ms,
				.value = sample.temperature,
				.threshold = app->config.temperature_threshold
			};
		}
		if (sample.humidity > app->config.humidity_threshold) {
			alarms[alarm_count++] = (struct sms_alarm_message) {
				.type = SMS_HUMID_HIGH,
				.timestamp_ms = sample.timestamp_ms,
				.value = sample.humidity,
				.threshold = app->config.humidity_threshold
			};
		}
		if (have_previous &&
		    fabs(sample.temperature - previous_temperature) > 5.0) {
			alarms[alarm_count++] = (struct sms_alarm_message) {
				.type = SMS_TEMP_SPIKE,
				.timestamp_ms = sample.timestamp_ms,
				.value = fabs(sample.temperature - previous_temperature),
				.threshold = 5.0
			};
		}
		previous_temperature = sample.temperature;
		have_previous = 1;
		for (index = 0; index < alarm_count; index++) {
			if (send_alarm(app, &alarms[index]) != 0) {
				fprintf(stderr, "alert message could not be queued\n");
				atomic_store(&app->worker_error, true);
				sms_app_request_stop(app);
			}
		}
		if (sms_app_log(app, &log_message) != 0)
			fprintf(stderr, "sample log could not be queued\n");
		pthread_mutex_lock(&app->stats_mutex);
		app->processed_samples++;
		pthread_mutex_unlock(&app->stats_mutex);
		if (app->config.verbose)
			printf("ROLLING average_temperature=%.2f average_humidity=%.2f "
			       "window=%zu\n",
			       temperature_sum / window_size,
			       humidity_sum / window_size, window_size);
	}
	if (result < 0)
		fprintf(stderr, "circular_buffer_pop: %s\n", strerror(-result));
	if (result < 0) {
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
	}
	return NULL;
}
