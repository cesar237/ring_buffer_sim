/**
* ring_buffer.h
* A lock-based ring buffer implementation for multiple producers and consumers
* with tracking of waiting threads
*/

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "simulation.h"
#include <stdio.h>
#include <stdatomic.h>

/**
* Using pthread spin lock for thread synchronization
*/
typedef struct {
    pthread_spinlock_t lock;
    atomic_int waiting;     // Number of threads waiting for this lock
} enhanced_spinlock_t;

static inline void spinlock_init(enhanced_spinlock_t *lock) {
    pthread_spin_init(&lock->lock, PTHREAD_PROCESS_PRIVATE);
    atomic_init(&lock->waiting, 0);
}

static inline void spinlock_lock(enhanced_spinlock_t *lock) {
    // Increment waiting count before attempting to acquire the lock
    atomic_fetch_add(&lock->waiting, 1);
    
    // Try to acquire the lock
    pthread_spin_lock(&lock->lock);
    
    // Decrement waiting count after acquiring the lock
    atomic_fetch_sub(&lock->waiting, 1);
}

static inline void spinlock_unlock(enhanced_spinlock_t *lock) {
    pthread_spin_unlock(&lock->lock);
}

static inline int spinlock_get_waiting(enhanced_spinlock_t *lock) {
    return atomic_load(&lock->waiting);
}

static inline void spinlock_destroy(enhanced_spinlock_t *lock) {
    pthread_spin_destroy(&lock->lock);
}

/**
* Ring buffer structure
*/
typedef struct {
    void **buffer;             // Pointer to buffer storage
    size_t size;               // Size of the buffer (must be power of 2)
    size_t mask;               // Bit mask for fast modulo operations
    size_t element_size;       // Size of each element in bytes

    int batch_size;            // Number of elements to process in a batch
    bool batched;              // true if batched, false otherwise
    
    volatile size_t head;      // Producer index
    volatile size_t tail;      // Consumer index
    
    enhanced_spinlock_t produce_lock;  // Lock for producers
    enhanced_spinlock_t consume_lock;  // Lock for consumers

    // Stats
    atomic_int access_time;    // Time spent accessing the buffer in consume
    atomic_int nr_access_time; // Number of accesses in consume
    uint64_t producer_waits;   // Total number of producer waits
    uint64_t consumer_waits;   // Total number of consumer waits
} ring_buffer_t;

/**
* Initialize a ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param capacity Number of elements in the buffer (must be power of 2)
* @param element_size Size of each element in bytes
* @param batch_size Number of elements to process in a batch
* @param batched true if batched, false otherwise
* @return true if initialization succeeded, false otherwise
*/
bool ring_buffer_init_batch(ring_buffer_t *rb, size_t capacity, size_t element_size, int batch_size) {
    // Ensure capacity is a power of 2
    if ((capacity & (capacity - 1)) != 0) {
        // Find the next power of 2
        size_t power = 1;
        while (power < capacity) {
            power *= 2;
        }
        capacity = power;
    }
    
    rb->buffer = (void**)malloc(capacity * sizeof(void*));
    if (!rb->buffer) {
        return false;
    }
    
    rb->size = capacity;
    rb->mask = capacity - 1;
    rb->element_size = element_size;
    rb->head = 0;
    rb->tail = 0;
    rb->batch_size = batch_size;
    rb->access_time = 0;
    rb->batched = batch_size > 1;
    rb->producer_waits = 0;
    rb->consumer_waits = 0;
    
    spinlock_init(&rb->produce_lock);
    spinlock_init(&rb->consume_lock);
    atomic_init(&rb->access_time, 0);
    atomic_init(&rb->nr_access_time, 0);

    return true;
}

/**
* Initialize a ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param capacity Number of elements in the buffer (must be power of 2)
* @param element_size Size of each element in bytes
* @return true if initialization succeeded, false otherwise
*/
bool ring_buffer_init(ring_buffer_t *rb, size_t capacity, size_t element_size) {
    return ring_buffer_init_batch(rb, capacity, element_size, 1);
}

/**
* Clean up the ring buffer
* 
* @param rb Pointer to the ring buffer structure
*/
void ring_buffer_destroy(ring_buffer_t *rb) {
    if (rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
    }
    
    // Clean up the spin locks
    spinlock_destroy(&rb->produce_lock);
    spinlock_destroy(&rb->consume_lock);
}

/**
* Check if the ring buffer is empty
* 
* @param rb Pointer to the ring buffer structure
* @return true if empty, false otherwise
*/
bool ring_buffer_is_empty(const ring_buffer_t *rb) {
    return rb->head == rb->tail;
}

/**
* Check if the ring buffer is full
* 
* @param rb Pointer to the ring buffer structure
* @return true if full, false otherwise
*/
bool ring_buffer_is_full(const ring_buffer_t *rb) {
    return ((rb->head + 1) & rb->mask) == rb->tail;
}

/**
* Get the number of elements in the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @return Number of elements
*/
size_t ring_buffer_count(const ring_buffer_t *rb) {
    return (rb->head - rb->tail) & rb->mask;
}

/**
* Get the number of producers waiting
* 
* @param rb Pointer to the ring buffer structure
* @return Number of producers waiting
*/
int ring_buffer_producers_waiting(ring_buffer_t *rb) {
    return spinlock_get_waiting(&rb->produce_lock);
}

/**
* Get the number of consumers waiting
* 
* @param rb Pointer to the ring buffer structure
* @return Number of consumers waiting
*/
int ring_buffer_consumers_waiting(ring_buffer_t *rb) {
    return spinlock_get_waiting(&rb->consume_lock);
}

/**
* Get the total number of producer waits that have occurred
* 
* @param rb Pointer to the ring buffer structure
* @return Total number of producer waits
*/
uint64_t ring_buffer_producer_wait_count(const ring_buffer_t *rb) {
    return rb->producer_waits;
}

/**
* Get the total number of consumer waits that have occurred
* 
* @param rb Pointer to the ring buffer structure
* @return Total number of consumer waits
*/
uint64_t ring_buffer_consumer_wait_count(const ring_buffer_t *rb) {
    return rb->consumer_waits;
}

/**
* Enqueue an item to the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param item Pointer to the item to enqueue
* @param batch_size Number of elements to process in a batch
* @return Number of items enqueued
*/
int ring_buffer_produce_batch(ring_buffer_t *rb, const void *item, int batch_size) {
    int total_produced = 0;

    // Update producer waits before acquiring the lock
    rb->producer_waits += spinlock_get_waiting(&rb->produce_lock);
    spinlock_lock(&rb->produce_lock);

    while (!ring_buffer_is_full(rb)) {
        // Store the item
        size_t index = rb->head & rb->mask;
        rb->buffer[index] = malloc(rb->element_size);
        if (rb->buffer[index]) {
            memcpy(rb->buffer[index], item, rb->element_size);
            
            // Memory barrier to ensure the item is written before updating head
            __sync_synchronize();
            
            // Update head
            rb->head = (rb->head + 1) & rb->mask;
            
            total_produced++;
            if (total_produced >= batch_size) {
                break;
            }
        }
    }

    spinlock_unlock(&rb->produce_lock);
    
    return total_produced;
}

/**
* Enqueue an item to the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param item Pointer to the item to enqueue
* @return true if successful, false otherwise
*/
bool ring_buffer_produce(ring_buffer_t *rb, const void *item) {
    return ring_buffer_produce_batch(rb, item, 1) == 1;
}

/**
* Dequeue multiple items from the ring buffer in a batch into an array of items
* 
* @param rb Pointer to the ring buffer structure
* @param items_array Array of pointers to store the dequeued items
* @param max_items Maximum number of items to dequeue
* @return Number of items successfully dequeued
*/
int ring_buffer_consume_batch(ring_buffer_t *rb, void **items_array, int max_items) {
    int items_consumed = 0;
    
    // Update consumer waits before acquiring the lock
    rb->consumer_waits += spinlock_get_waiting(&rb->consume_lock);
    spinlock_lock(&rb->consume_lock);
    
    uint64_t start = get_time_ns();
    // Process up to max_items or until the buffer is empty
    while (items_consumed < max_items && !ring_buffer_is_empty(rb)) {
        // Get the item from the current tail position
        size_t index = rb->tail & rb->mask;
        
        if (rb->buffer[index]) {
            // Copy the item to the destination array at the current position
            void *dest = items_array[items_consumed];
            memcpy(dest, rb->buffer[index], rb->element_size);
            
            // Free the memory allocated for this item
            free(rb->buffer[index]);
            rb->buffer[index] = NULL;
            
            // Update tail position
            rb->tail = (rb->tail + 1) & rb->mask;
            items_consumed++;
        } else {
            // This shouldn't happen in theory, but break if we find a NULL slot
            break;
        }
    }
    
    // Memory barrier to ensure all reads complete before unlocking
    if (items_consumed > 0) {
        __sync_synchronize();
    }
    
    uint64_t end = get_time_ns();
    spinlock_unlock(&rb->consume_lock);
    
    int nr_access_time = atomic_load(&rb->nr_access_time);
    int access_time = atomic_load(&rb->access_time);

    access_time = (access_time * nr_access_time + end - start) / (nr_access_time + 1);
    atomic_store(&rb->access_time, access_time);
    atomic_fetch_add(&rb->nr_access_time, 1);

    return items_consumed;
}

/**
* Dequeue a single item from the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param item Pointer to store the dequeued item
* @return true if successful, false otherwise
*/
void *ring_buffer_consume(ring_buffer_t *rb) {
    void *result = NULL;
    
    // Update consumer waits before acquiring the lock
    rb->consumer_waits += spinlock_get_waiting(&rb->consume_lock);
    spinlock_lock(&rb->consume_lock);
    uint64_t start = get_time_ns();

    if (!ring_buffer_is_empty(rb)) {
        // Get the item
        size_t index = rb->tail & rb->mask;
        if (rb->buffer[index]) {
            // memcpy(item, rb->buffer[index], rb->element_size);
            result = rb->buffer[index];
            free(rb->buffer[index]);
            rb->buffer[index] = NULL;

            // Memory barrier to ensure the item is read before updating tail
            __sync_synchronize();
            
            // Update tail
            rb->tail = (rb->tail + 1) & rb->mask;
        }
    }

    uint64_t end = get_time_ns();
    spinlock_unlock(&rb->consume_lock);
    
    int nr_access_time = atomic_load(&rb->nr_access_time);
    int access_time = atomic_load(&rb->access_time);

    access_time = (access_time * nr_access_time + end - start) / (nr_access_time + 1);
    atomic_store(&rb->access_time, access_time);
    atomic_fetch_add(&rb->nr_access_time, 1);
    
    return result;
}

/**
* Get the average access time for the ring buffer
*
* @param rb Pointer to the ring buffer structure
* @return Average access time in nanoseconds
*/
int ring_buffer_access_time(const ring_buffer_t *rb) {
    return atomic_load(&rb->access_time);
}

#endif /* RING_BUFFER_H */