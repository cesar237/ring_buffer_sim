/**
* ring_buffer.h
* A lock-based ring buffer implementation for multiple producers and consumers
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

/**
* Using pthread spin lock for thread synchronization
*/
typedef pthread_spinlock_t spinlock_t;

static inline void spinlock_init(spinlock_t *lock) {
    pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void spinlock_lock(spinlock_t *lock) {
    pthread_spin_lock(lock);
}

static inline int try_spinlock_lock(spinlock_t *lock) {
    return pthread_spin_trylock(lock);
}


static inline void spinlock_unlock(spinlock_t *lock) {
    pthread_spin_unlock(lock);
}

static inline void spinlock_destroy(spinlock_t *lock) {
    pthread_spin_destroy(lock);
}

/**
* Ring buffer structure
*/
typedef struct {
    void **buffer;           // Pointer to buffer storage
    size_t size;             // Size of the buffer (must be power of 2)
    size_t mask;             // Bit mask for fast modulo operations
    size_t element_size;     // Size of each element in bytes
    int degree;

    int batch_size;          // Number of elements to process in a batch
    bool batched;            // true if batched, false otherwise

    volatile size_t *producers;    // Producer index
    volatile size_t *consumers;    // Consumer index

    spinlock_t **produce_locks; // Lock for producers
    spinlock_t **consume_locks; // Lock for consumers

    int produce_contention;
    int consume_contention;

    // Add spinlocks stats 
    uint64_t access_time;
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
bool ring_buffer_init_batch(ring_buffer_t *rb, size_t capacity, size_t element_size, int batch_size, int degree) {
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

    rb->produce_locks = (spinlock_t **)malloc(degree * sizeof(spinlock_t *));
    if (!rb->produce_locks) {
        return false;
    }

    rb->consume_locks = (spinlock_t **)malloc(degree * sizeof(spinlock_t *));
    if (!rb->consume_locks) {
        return false;
    }

    rb->size = capacity;
    rb->mask = capacity - 1;
    rb->element_size = element_size;
    rb->degree = degree;
    rb->producers = (size_t *)malloc(degree * sizeof(size_t));
    rb->consumers = (size_t *)malloc(degree * sizeof(size_t));

    rb->batch_size = batch_size;
    rb->access_time = 0;


    
    for (int i=0; i < rb->degree; i++) {
        spinlock_init(rb->produce_locks[i]);
        spinlock_init(rb->consume_locks[i]);
    }
    
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
    // printf("Init ring buffer\n");
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
* Enqueue an item to the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param item Pointer to the item to enqueue
* @param batch_size Number of elements to process in a batch
* @return Number of items enqueued
*/
int ring_buffer_produce_batch(ring_buffer_t *rb, const void *item, int batch_size) {
    int total_produced = 0;

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
    
    spinlock_lock(&rb->consume_lock);
    
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
    
    spinlock_unlock(&rb->consume_lock);
    
    return items_consumed;
}

/**
* Dequeue a single item from the ring buffer
* 
* @param rb Pointer to the ring buffer structure
* @param item Pointer to store the dequeued item
* @return true if successful, false otherwise
*/
bool ring_buffer_consume(ring_buffer_t *rb, void *item) {
    // For a single item, we can just call the original implementation directly
    bool result = false;
    
    spinlock_lock(&rb->consume_lock);

    if (!ring_buffer_is_empty(rb)) {
        // Get the item
        size_t index = rb->tail & rb->mask;
        if (rb->buffer[index]) {
            memcpy(item, rb->buffer[index], rb->element_size);
            free(rb->buffer[index]);
            rb->buffer[index] = NULL;
            
            // Memory barrier to ensure the item is read before updating tail
            __sync_synchronize();
            
            // Update tail
            rb->tail = (rb->tail + 1) & rb->mask;
            result = true;
        }
    }
    
    spinlock_unlock(&rb->consume_lock);
    
    return result;
}

#endif /* RING_BUFFER_H */