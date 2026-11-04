// Bounded producer-consumer ring protected by a mutex and condition variables.
// Author: Nguyen Cao Phat (SE201044)
// Date: 2026-11-04

#include "sms_app.h"

#include <errno.h>
#include <stdlib.h>

/*
 * Purpose: Allocates the ring and initializes its synchronization primitives.
 * Parameters: buffer to initialize and desired nonzero capacity.
 * Returns: 0 on success or a positive pthread/allocation error.
 */
int circular_buffer_init(struct circular_buffer *buffer, size_t capacity)
{
	int error;

	buffer->items = calloc(capacity, sizeof(*buffer->items));
	if (!buffer->items)
		return ENOMEM;
	buffer->capacity = capacity;
	buffer->head = 0;
	buffer->tail = 0;
	buffer->size = 0;
	buffer->total_produced = 0;
	buffer->total_consumed = 0;
	buffer->overflow_count = 0;
	buffer->closed = false;
	error = pthread_mutex_init(&buffer->mutex, NULL);
	if (error)
		goto free_items;
	error = pthread_cond_init(&buffer->not_empty, NULL);
	if (error)
		goto destroy_mutex;
	error = pthread_cond_init(&buffer->not_full, NULL);
	if (error)
		goto destroy_not_empty;
	return 0;

destroy_not_empty:
	pthread_cond_destroy(&buffer->not_empty);
destroy_mutex:
	pthread_mutex_destroy(&buffer->mutex);
free_items:
	free(buffer->items);
	buffer->items = NULL;
	return error;
}

/*
 * Purpose: Inserts a sample or reports full/closed state without blocking.
 * Parameters: ring buffer and sample to copy into the ring.
 * Returns: 0 on success or a positive errno-style error.
 */
int circular_buffer_push(struct circular_buffer *buffer,
			 const struct sensor_data *sample)
{
	int error = pthread_mutex_lock(&buffer->mutex);

	if (error)
		return error;
	if (buffer->closed) {
		pthread_mutex_unlock(&buffer->mutex);
		return ECANCELED;
	}
	if (buffer->size == buffer->capacity) {
		buffer->overflow_count++;
		pthread_mutex_unlock(&buffer->mutex);
		return EAGAIN;
	}
	buffer->items[buffer->head] = *sample;
	buffer->head = (buffer->head + 1) % buffer->capacity;
	buffer->size++;
	buffer->total_produced++;
	pthread_cond_signal(&buffer->not_empty);
	pthread_mutex_unlock(&buffer->mutex);
	return 0;
}

/*
 * Purpose: Removes one item, waiting while empty unless the ring is closed.
 * Parameters: ring buffer and destination sample.
 * Returns: 1 for a sample, 0 after drain, or a negative pthread error.
 */
int circular_buffer_pop(struct circular_buffer *buffer,
			struct sensor_data *sample)
{
	int error = pthread_mutex_lock(&buffer->mutex);

	if (error)
		return -error;
	while (buffer->size == 0 && !buffer->closed) {
		error = pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
		if (error) {
			pthread_mutex_unlock(&buffer->mutex);
			return -error;
		}
	}
	if (buffer->size == 0) {
		pthread_mutex_unlock(&buffer->mutex);
		return 0;
	}
	*sample = buffer->items[buffer->tail];
	buffer->tail = (buffer->tail + 1) % buffer->capacity;
	buffer->size--;
	buffer->total_consumed++;
	pthread_cond_signal(&buffer->not_full);
	pthread_mutex_unlock(&buffer->mutex);
	return 1;
}

/*
 * Purpose: Closes the ring and wakes any blocked producer/consumer paths.
 * Parameters: initialized ring buffer.
 * Returns: none.
 */
void circular_buffer_close(struct circular_buffer *buffer)
{
	pthread_mutex_lock(&buffer->mutex);
	buffer->closed = true;
	pthread_cond_broadcast(&buffer->not_empty);
	pthread_cond_broadcast(&buffer->not_full);
	pthread_mutex_unlock(&buffer->mutex);
}

/*
 * Purpose: Copies a consistent occupancy/counter snapshot.
 * Parameters: initialized ring buffer and output statistics structure.
 * Returns: none.
 */
void circular_buffer_get_stats(struct circular_buffer *buffer,
			       struct circular_buffer_stats *stats)
{
	pthread_mutex_lock(&buffer->mutex);
	stats->current_size = buffer->size;
	stats->total_produced = buffer->total_produced;
	stats->total_consumed = buffer->total_consumed;
	stats->overflow_count = buffer->overflow_count;
	pthread_mutex_unlock(&buffer->mutex);
}

/*
 * Purpose: Destroys initialized synchronization objects and frees ring storage.
 * Parameters: initialized or safely partially initialized ring buffer.
 * Returns: none.
 */
void circular_buffer_destroy(struct circular_buffer *buffer)
{
	if (!buffer->items)
		return;
	pthread_cond_destroy(&buffer->not_full);
	pthread_cond_destroy(&buffer->not_empty);
	pthread_mutex_destroy(&buffer->mutex);
	free(buffer->items);
	buffer->items = NULL;
}
