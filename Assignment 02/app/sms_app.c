// Parses options, manages shared resources, and coordinates worker shutdown.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../driver/sms_sensor.h"

static volatile sig_atomic_t caught_sigint;

/*
 * Purpose: Records SIGINT without doing unsafe cleanup in signal context.
 * Parameters: signal number supplied by the kernel.
 * Returns: none.
 */
static void handle_sigint(int signal_number)
{
	(void)signal_number;
	caught_sigint = 1;
}

/*
 * Purpose: Displays supported command-line options.
 * Parameters: program name used in the usage line.
 * Returns: none.
 */
static void print_usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [-i ms] [-t celsius] [-h percent] [-d seconds] [-v] "
		"[-s fifo|other] [-D device] [-L logfile] [-J jitter-report]\n",
		program);
}

/*
 * Purpose: Parses a finite bounded double and rejects trailing input.
 * Parameters: text, inclusive minimum/maximum, and destination value.
 * Returns: 0 on success or -1 for invalid input.
 */
static int parse_double(const char *text, double minimum, double maximum,
			double *value)
{
	char *end;
	double parsed;

	errno = 0;
	parsed = strtod(text, &end);
	if (errno || end == text || *end != '\0' || !isfinite(parsed) ||
	    parsed < minimum || parsed > maximum)
		return -1;
	*value = parsed;
	return 0;
}

/*
 * Purpose: Parses a bounded unsigned integer without signs or trailing text.
 * Parameters: text, inclusive minimum/maximum, and destination value.
 * Returns: 0 on success or -1 for invalid input.
 */
static int parse_unsigned(const char *text, unsigned int minimum,
			  unsigned int maximum, unsigned int *value)
{
	char *end;
	unsigned long parsed;

	if (*text == '-' || *text == '+')
		return -1;
	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno || end == text || *end != '\0' || parsed < minimum ||
	    parsed > maximum)
		return -1;
	*value = (unsigned int)parsed;
	return 0;
}

/*
 * Purpose: Initializes defaults and validates the complete CLI.
 * Parameters: argc/argv command-line values and output configuration.
 * Returns: 0 on success or -1 for invalid input.
 */
static int parse_options(int argc, char **argv, struct sms_config *config)
{
	int option;

	config->interval_ms = 500;
	config->temperature_threshold = 40.0;
	config->humidity_threshold = 80.0;
	config->duration_seconds = 60;
	config->verbose = false;
	config->realtime = true;
	config->device_path = SMS_DEVICE_DEFAULT;
	config->log_path = SMS_LOG_DEFAULT;
	config->jitter_path = SMS_JITTER_DEFAULT;
	while ((option = getopt(argc, argv, "i:t:h:d:vs:D:L:J:?")) != -1) {
		switch (option) {
		case 'i':
			if (parse_unsigned(optarg, SMS_MIN_INTERVAL_MS,
					   SMS_MAX_INTERVAL_MS,
					   &config->interval_ms))
				return -1;
			break;
		case 't':
			if (parse_double(optarg, 0.0, 100.0,
					 &config->temperature_threshold))
				return -1;
			break;
		case 'h':
			if (parse_double(optarg, 0.0, 100.0,
					 &config->humidity_threshold))
				return -1;
			break;
		case 'd':
			if (parse_unsigned(optarg, 0, 86400,
					   &config->duration_seconds))
				return -1;
			break;
		case 'v':
			config->verbose = true;
			break;
		case 's':
			if (strcmp(optarg, "fifo") == 0)
				config->realtime = true;
			else if (strcmp(optarg, "other") == 0)
				config->realtime = false;
			else
				return -1;
			break;
		case 'D':
			config->device_path = optarg;
			break;
		case 'L':
			config->log_path = optarg;
			break;
		case 'J':
			config->jitter_path = optarg;
			break;
		case '?':
			return -1;
		default:
			return -1;
		}
	}
	if (optind != argc)
		return -1;
	return 0;
}

/*
 * Purpose: Creates unique POSIX queues with fixed-size message capacity.
 * Parameters: application context receiving queue descriptors and names.
 * Returns: 0 on success or -1 after reporting/cleaning up an error.
 */
static int create_queues(struct sms_app *app)
{
	struct mq_attr alarm_attributes = { 0 };
	struct mq_attr log_attributes = { 0 };

	snprintf(app->alarm_queue_name, sizeof(app->alarm_queue_name),
		 "/sms_alarm_%ld", (long)getpid());
	snprintf(app->log_queue_name, sizeof(app->log_queue_name),
		 "/sms_log_%ld", (long)getpid());
	alarm_attributes.mq_maxmsg = 10;
	alarm_attributes.mq_msgsize = sizeof(struct sms_alarm_message);
	log_attributes.mq_maxmsg = 10;
	log_attributes.mq_msgsize = sizeof(struct sms_log_message);
	app->alarm_queue = mq_open(app->alarm_queue_name,
				   O_CREAT | O_EXCL | O_RDWR, 0600,
				   &alarm_attributes);
	if (app->alarm_queue == (mqd_t)-1) {
		perror("mq_open alarm queue");
		return -1;
	}
	app->log_queue = mq_open(app->log_queue_name,
				 O_CREAT | O_EXCL | O_RDWR, 0600,
				 &log_attributes);
	if (app->log_queue == (mqd_t)-1) {
		perror("mq_open log queue");
		mq_close(app->alarm_queue);
		mq_unlink(app->alarm_queue_name);
		return -1;
	}
	app->queues_initialized = 1;
	return 0;
}

/*
 * Purpose: Sends one log record with a bounded wait and surfaces failures.
 * Parameters: application context and immutable log message.
 * Returns: 0 on success or -1 on clock/queue failure.
 */
int sms_app_log(struct sms_app *app, const struct sms_log_message *message)
{
	struct timespec deadline;

	if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
		perror("clock_gettime log queue");
		return -1;
	}
	deadline.tv_sec++;
	while (mq_timedsend(app->log_queue, (const char *)message,
			    sizeof(*message), 0, &deadline) < 0) {
		if (errno == EINTR)
			continue;
		perror("mq_timedsend log");
		atomic_store(&app->worker_error, true);
		sms_app_request_stop(app);
		return -1;
	}
	return 0;
}

/*
 * Purpose: Queues a startup event identifying one worker thread.
 * Parameters: application context and stable worker name.
 * Returns: result from sms_app_log().
 */
int sms_app_log_thread_start(struct sms_app *app, const char *thread_name)
{
	struct sms_log_message message = { .kind = SMS_LOG_THREAD };

	snprintf(message.thread_name, sizeof(message.thread_name), "%s", thread_name);
	return sms_app_log(app, &message);
}

/*
 * Purpose: Requests shutdown once and closes the ring to wake the consumer.
 * Parameters: initialized application context.
 * Returns: none.
 */
void sms_app_request_stop(struct sms_app *app)
{
	if (atomic_exchange(&app->stop_requested, true))
		return;
	if (app->buffer_initialized)
		circular_buffer_close(&app->samples);
}

/*
 * Purpose: Sends a stop marker after upstream producers have joined.
 * Parameters: queue, message bytes/size, and diagnostic queue description.
 * Returns: 0 on success or -1 on timeout/clock failure.
 */
static int send_stop_marker(mqd_t queue, const void *message, size_t size,
			    const char *description)
{
	struct timespec deadline;

	if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
		perror("clock_gettime stop marker");
		return -1;
	}
	deadline.tv_sec += 2;
	if (mq_timedsend(queue, message, size, 0, &deadline) != 0) {
		fprintf(stderr, "%s stop marker: %s\n", description, strerror(errno));
		return -1;
	}
	return 0;
}

/*
 * Purpose: Appends one absolute jitter observation within the fixed cap.
 * Parameters: application context and signed jitter in microseconds.
 * Returns: none.
 */
void sms_app_record_jitter(struct sms_app *app, long long jitter)
{
	pthread_mutex_lock(&app->jitter_mutex);
	if (app->jitter_count < app->jitter_capacity)
		app->jitter_us[app->jitter_count++] = jitter < 0 ? -jitter : jitter;
	pthread_mutex_unlock(&app->jitter_mutex);
}

/*
 * Purpose: Orders two jitter values for qsort().
 * Parameters: pointers to two long long values.
 * Returns: negative, zero, or positive according to numeric ordering.
 */
static int compare_jitter(const void *left, const void *right)
{
	long long a = *(const long long *)left;
	long long b = *(const long long *)right;

	return (a > b) - (a < b);
}

/*
 * Purpose: Writes jitter summary statistics and histogram to a file.
 * Parameters: application context containing measurements and report path.
 * Returns: 0 on success or -1 on missing data/allocation/file error.
 */
static int write_jitter_report(struct sms_app *app)
{
	FILE *report;
	long long *sorted;
	long long minimum = 0;
	long long maximum = 0;
	long double total = 0.0;
	size_t bins[4] = { 0, 0, 0, 0 };
	size_t index;
	size_t percentile_index;

	pthread_mutex_lock(&app->jitter_mutex);
	if (app->jitter_count == 0) {
		pthread_mutex_unlock(&app->jitter_mutex);
		fprintf(stderr, "No jitter samples were recorded.\n");
		return -1;
	}
	sorted = malloc(app->jitter_count * sizeof(*sorted));
	if (!sorted) {
		pthread_mutex_unlock(&app->jitter_mutex);
		perror("malloc jitter report");
		return -1;
	}
	memcpy(sorted, app->jitter_us, app->jitter_count * sizeof(*sorted));
	index = app->jitter_count;
	pthread_mutex_unlock(&app->jitter_mutex);

	qsort(sorted, index, sizeof(*sorted), compare_jitter);
	minimum = sorted[0];
	maximum = sorted[index - 1];
	for (size_t sample = 0; sample < index; sample++) {
		total += sorted[sample];
		if (sorted[sample] < 100)
			bins[0]++;
		else if (sorted[sample] < 500)
			bins[1]++;
		else if (sorted[sample] < 1000)
			bins[2]++;
		else
			bins[3]++;
	}
	percentile_index = (index * 99 + 99) / 100 - 1;
	report = fopen(app->config.jitter_path, "w");
	if (!report) {
		perror("open jitter report");
		free(sorted);
		return -1;
	}
	fprintf(report, "scheduler=%s\nsamples=%zu\nmin_us=%lld\navg_us=%.2Lf\n"
		"max_us=%lld\np99_us=%lld\n",
		app->config.realtime ? "SCHED_FIFO" : "SCHED_OTHER", index,
		minimum, total / index, maximum, sorted[percentile_index]);
	fprintf(report, "histogram_0_100_us=%zu\nhistogram_100_500_us=%zu\n"
		"histogram_500_1000_us=%zu\nhistogram_over_1000_us=%zu\n",
		bins[0], bins[1], bins[2], bins[3]);
	if (fclose(report) != 0) {
		perror("close jitter report");
		free(sorted);
		return -1;
	}
	printf("Jitter report: %s (%zu samples, min=%lldus avg=%.2Lf us "
	       "max=%lldus p99=%lldus)\n",
	       app->config.jitter_path, index, minimum, total / index, maximum,
	       sorted[percentile_index]);
	free(sorted);
	return 0;
}

/*
 * Purpose: Releases every initialized descriptor and owned resource.
 * Parameters: application context being destroyed.
 * Returns: none; cleanup errors are printed.
 */
static void destroy_app(struct sms_app *app)
{
	if (app->sensor_fd >= 0 && close(app->sensor_fd) != 0)
		perror("close sensor device");
	if (app->queues_initialized) {
		if (mq_close(app->alarm_queue) != 0)
			perror("mq_close alarm queue");
		if (mq_close(app->log_queue) != 0)
			perror("mq_close log queue");
		if (mq_unlink(app->alarm_queue_name) != 0 && errno != ENOENT)
			perror("mq_unlink alarm queue");
		if (mq_unlink(app->log_queue_name) != 0 && errno != ENOENT)
			perror("mq_unlink log queue");
	}
	if (app->buffer_initialized)
		circular_buffer_destroy(&app->samples);
	if (app->jitter_mutex_initialized)
		pthread_mutex_destroy(&app->jitter_mutex);
	if (app->stats_mutex_initialized)
		pthread_mutex_destroy(&app->stats_mutex);
	free(app->jitter_us);
}

/*
 * Purpose: Initializes the app, coordinates four workers, and shuts down cleanly.
 * Parameters: process argc/argv command-line values.
 * Returns: EXIT_SUCCESS on normal completion, otherwise EXIT_FAILURE.
 */
int main(int argc, char **argv)
{
	struct sms_app app = { .sensor_fd = -1 };
	pthread_t sensor_thread;
	pthread_t processor_thread;
	pthread_t alert_thread;
	pthread_t logger_thread;
	int sensor_started = 0;
	int processor_started = 0;
	int alert_started = 0;
	int logger_started = 0;
	int option_result;
	int result = EXIT_FAILURE;
	struct sigaction action = { 0 };
	struct timespec wait_time = { .tv_sec = 0, .tv_nsec = 100000000L };
	time_t start_time;

	atomic_init(&app.stop_requested, false);
	atomic_init(&app.worker_error, false);
	option_result = parse_options(argc, argv, &app.config);
	if (option_result != 0) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	app.sensor_fd = open(app.config.device_path, O_RDONLY);
	if (app.sensor_fd < 0) {
		perror("open sensor device");
		return EXIT_FAILURE;
	}
	if (ioctl(app.sensor_fd, SMS_SET_INTERVAL, &app.config.interval_ms) < 0) {
		perror("SMS_SET_INTERVAL");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	if (circular_buffer_init(&app.samples, SMS_SAMPLE_CAPACITY) != 0) {
		fprintf(stderr, "Failed to initialize the circular buffer.\n");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	app.buffer_initialized = 1;
	if (pthread_mutex_init(&app.stats_mutex, NULL) != 0) {
		fprintf(stderr, "Failed to initialize stats mutex.\n");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	app.stats_mutex_initialized = 1;
	if (pthread_mutex_init(&app.jitter_mutex, NULL) != 0) {
		fprintf(stderr, "Failed to initialize jitter mutex.\n");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	app.jitter_mutex_initialized = 1;
	app.jitter_capacity = SMS_JITTER_CAPACITY;
	app.jitter_us = calloc(app.jitter_capacity, sizeof(*app.jitter_us));
	if (!app.jitter_us) {
		perror("calloc jitter samples");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	if (create_queues(&app) != 0) {
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
		fprintf(stderr, "mlockall unavailable: %s\n", strerror(errno));
	action.sa_handler = handle_sigint;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0) {
		perror("sigaction");
		destroy_app(&app);
		return EXIT_FAILURE;
	}
	if (pthread_create(&sensor_thread, NULL, sensor_thread_main, &app) != 0) {
		fprintf(stderr, "Failed to create sensor thread.\n");
		goto shutdown;
	}
	sensor_started = 1;
	if (pthread_create(&processor_thread, NULL, processor_thread_main, &app) != 0) {
		fprintf(stderr, "Failed to create processor thread.\n");
		goto shutdown;
	}
	processor_started = 1;
	if (pthread_create(&alert_thread, NULL, alert_thread_main, &app) != 0) {
		fprintf(stderr, "Failed to create alert thread.\n");
		goto shutdown;
	}
	alert_started = 1;
	if (pthread_create(&logger_thread, NULL, logger_thread_main, &app) != 0) {
		fprintf(stderr, "Failed to create logger thread.\n");
		goto shutdown;
	}
	logger_started = 1;

	printf("SMS started: interval=%ums mode=%s duration=%us device=%s\n",
	       app.config.interval_ms,
	       app.config.realtime ? "SCHED_FIFO" : "SCHED_OTHER",
	       app.config.duration_seconds, app.config.device_path);
	start_time = time(NULL);
	while (!caught_sigint && !atomic_load(&app.stop_requested) &&
	       (app.config.duration_seconds == 0 ||
		time(NULL) - start_time < (time_t)app.config.duration_seconds)) {
		nanosleep(&wait_time, NULL);
	}
	result = EXIT_SUCCESS;

shutdown:
	sms_app_request_stop(&app);
	if (sensor_started)
		pthread_join(sensor_thread, NULL);
	if (processor_started)
		pthread_join(processor_thread, NULL);
	if (app.queues_initialized) {
		struct sms_alarm_message alarm_stop = { .type = -1 };

		if (alert_started &&
		    send_stop_marker(app.alarm_queue, &alarm_stop,
				     sizeof(alarm_stop), "alert queue") != 0)
			result = EXIT_FAILURE;
	}
	if (alert_started)
		pthread_join(alert_thread, NULL);
	if (app.queues_initialized && logger_started) {
		struct sms_log_message log_stop = { .kind = SMS_LOG_STOP };

		if (send_stop_marker(app.log_queue, &log_stop, sizeof(log_stop),
				     "logger queue") != 0)
			result = EXIT_FAILURE;
	}
	if (logger_started)
		pthread_join(logger_thread, NULL);
	if (atomic_load(&app.worker_error))
		result = EXIT_FAILURE;
	if (write_jitter_report(&app) != 0)
		result = EXIT_FAILURE;
	if (app.buffer_initialized) {
		struct circular_buffer_stats stats;

		circular_buffer_get_stats(&app.samples, &stats);
		printf("Shutdown: produced=%llu consumed=%llu overflow=%llu\n",
		       (unsigned long long)stats.total_produced,
		       (unsigned long long)stats.total_consumed,
		       (unsigned long long)stats.overflow_count);
	}
	if (app.stats_mutex_initialized) {
		pthread_mutex_lock(&app.stats_mutex);
		printf("Processed=%llu alerts TEMP_HIGH=%llu HUMID_HIGH=%llu "
		       "TEMP_SPIKE=%llu\n",
		       (unsigned long long)app.processed_samples,
		       (unsigned long long)app.alert_counts[SMS_TEMP_HIGH],
		       (unsigned long long)app.alert_counts[SMS_HUMID_HIGH],
		       (unsigned long long)app.alert_counts[SMS_TEMP_SPIKE]);
		pthread_mutex_unlock(&app.stats_mutex);
	}
	destroy_app(&app);
	return result;
}
