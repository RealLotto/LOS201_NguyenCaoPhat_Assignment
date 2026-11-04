// Handles queued alarms, updates per-type counts, and forwards alerts to the logger.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *const alarm_names[SMS_ALARM_TYPE_COUNT] = {
	"TEMP_HIGH", "HUMID_HIGH", "TEMP_SPIKE"
};

/*
 * Purpose: Receives alarms, prints/counts them, and forwards them to the logger.
 * Parameters: application context passed as the pthread argument.
 * Returns: NULL after the ordered stop marker or a worker error.
 */
void *alert_thread_main(void *argument)
{
	struct sms_app *app = argument;

	if (sms_app_log_thread_start(app, "alert") != 0)
		return NULL;
	for (;;) {
		struct sms_alarm_message alarm;
		struct timespec deadline;
		ssize_t bytes;

		if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
			perror("clock_gettime alert");
			atomic_store(&app->worker_error, true);
			break;
		}
		deadline.tv_sec++;
		bytes = mq_timedreceive(app->alarm_queue, (char *)&alarm,
					sizeof(alarm), NULL, &deadline);
		if (bytes < 0) {
			if (errno == EINTR || errno == ETIMEDOUT) {
				continue;
			}
			perror("mq_timedreceive alarm");
			atomic_store(&app->worker_error, true);
			sms_app_request_stop(app);
			break;
		}
		if ((size_t)bytes != sizeof(alarm)) {
			fprintf(stderr, "invalid alarm message size: %zd\n", bytes);
			continue;
		}
		if (alarm.type < 0)
			break;
		if (alarm.type >= SMS_ALARM_TYPE_COUNT) {
			fprintf(stderr, "invalid alarm type: %d\n", alarm.type);
			continue;
		}
		fprintf(stderr, "[ALERT][%llu] TYPE: %s VALUE: %.1f THRESHOLD: %.1f\n",
			(unsigned long long)alarm.timestamp_ms, alarm_names[alarm.type],
			alarm.value, alarm.threshold);
		pthread_mutex_lock(&app->stats_mutex);
		app->alert_counts[alarm.type]++;
		pthread_mutex_unlock(&app->stats_mutex);
		{
			struct sms_log_message message = {
				.kind = SMS_LOG_ALERT,
				.timestamp_ms = alarm.timestamp_ms,
				.alarm_type = alarm.type,
				.value = alarm.value,
				.threshold = alarm.threshold
			};

			if (sms_app_log(app, &message) != 0)
				fprintf(stderr, "alert log could not be queued\n");
		}
	}
	return NULL;
}
