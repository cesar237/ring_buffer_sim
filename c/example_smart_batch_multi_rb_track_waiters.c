/**
* Refactored example usage of the MPMC ring buffer with command line arguments
* Each producer-consumer pair has its own dedicated ring buffer
*/

#define _GNU_SOURCE

#include "rb_enhanced.h"
#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <math.h>

// Default values
#define DEFAULT_BUFFER_SIZE 100000
#define DEFAULT_NUM_PAIRS 16       // Number of producer-consumer pairs
#define DEFAULT_ITEMS_PER_PRODUCER 10000
#define DEFAULT_SERVICE_TIME 10
#define DEFAULT_ARRIVAL_RATE 100000
#define SAMPLING_SIZE 100
#define DEFAULT_BATCH_SIZE 4
#define DEFAULT_NUM_BUFFERS 16

typedef struct {
    uint64_t id;
    double value;
    uint64_t produce_time;
    uint64_t consume_time;
    uint64_t latency;
    int waiters;
    uint64_t spin_time;
    uint64_t access_time;
} item_t;

typedef struct {
    int id;
    int core;
    int total_produced;
    uint64_t total_spin_time;
    int items_per_producer;
    ring_buffer_t* buffer;  // Pointer to this producer's dedicated buffer
} producer_args_t;

typedef struct {
    int id;
    int core;
    int total_consumed;
    uint64_t total_spin_time;
    uint64_t total_service_time;
    uint64_t total_latency;
    uint64_t total_running_time;
    int batch_size;
    int service_time;
    int num_consumers;
    int total_items;
    ring_buffer_t* buffer;  // Pointer to this consumer's dedicated buffer
    uint64_t num_waiters;
    item_t *processed_items;
} consumer_args_t;

/**
* Pin the current thread to a specific CPU core.
* 
* @param core_id The core ID to pin the thread to (starting from 0)
* @return 0 on success, -1 on failure
*/
int pin_thread_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    if (core_id < 0 || core_id >= num_cores) {
        fprintf(stderr, "Error: Core ID %d is out of range (0-%d)\n", 
                core_id, num_cores - 1);
        return -1;
    }
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    pthread_t current_thread = pthread_self();
    
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        fprintf(stderr, "Error: Failed to set thread affinity\n");
        return -1;
    }
    
    return 0;
}

void* producer_thread(void* arg) {
    producer_args_t* producer_arg = (producer_args_t*)arg;
    
    // Pin the thread to a specific core if uncommented
    // if (pin_thread_to_core(producer_arg->core) != 0) {
    //     printf("Failed to pin producer thread %d to core %d\n", producer_arg->id, producer_arg->core);
    //     return NULL;
    // }
    
    for (int i = 0; i < producer_arg->items_per_producer; i++) {
        item_t item;
        item.id = i;
        item.produce_time = get_time_ns();
        item.value = (double)(producer_arg->id * 100) + (i * 0.5);
        
        producer_arg->total_produced++;

        // Try to produce until successful using this producer's dedicated buffer
        while (!ring_buffer_produce(producer_arg->buffer, &item)) {
            // Buffer is full, spin for a bit
            usleep(1000);
        }
    }
    
    // printf("Producer %d: Finished\n", producer_arg->id);
    return NULL;
}

/**
* Consumer thread function that processes items
*/
void* consumer_thread(void* arg) {
    consumer_args_t* consumer_arg = (consumer_args_t*)arg;
    int iterations = 0;
    int nr_waiters = 0;
    int access_time = 0;
    printf("Consumer %d: num_consumers=%d\n", consumer_arg->id, consumer_arg->num_consumers);
    int items_to_process = consumer_arg->total_items * 2 / consumer_arg->num_consumers;
    
    // Pin the thread to a specific core if uncommented
    // if (pin_thread_to_core(consumer_arg->core) != 0) {
    //     printf("Failed to pin consumer thread %d to core %d\n", consumer_arg->id, consumer_arg->core);
    //     return NULL;
    // }

    // Define batch size - can be adjusted based on performance testing
    const int BATCH_SIZE = consumer_arg->batch_size;

    uint64_t start = get_time_ns();

    // Process items until we reach the total expected from our paired producer
    while (iterations < items_to_process) {
        // Create items array for batch processing
        item_t items[BATCH_SIZE];
        void* item_ptrs[BATCH_SIZE];

        // Initialize pointers to items
        for (int i = 0; i < BATCH_SIZE; i++) {
            item_ptrs[i] = &items[i];
        }

        // Try to consume an item from this consumer's dedicated buffer
        uint64_t consume_start = get_time_ns();
        size_t consumed = ring_buffer_consume_batch(consumer_arg->buffer, item_ptrs, BATCH_SIZE);
        uint64_t consume_end = get_time_ns();

        // Update waiters and access time every SAMPLING_SIZE iterations
        if (iterations % SAMPLING_SIZE == 0) {
            nr_waiters = ring_buffer_consumers_waiting(consumer_arg->buffer);
            access_time = ring_buffer_access_time(consumer_arg->buffer);
        }

        consumer_arg->total_spin_time += consume_end - consume_start;

        if (consumed > 0) {
            for (size_t i = 0; i < consumed; i++) {  
                // Simulate some work for each item
                simulation_stats_t stats = simulation_busy_wait_us(consumer_arg->service_time);
                consumer_arg->total_service_time += stats.actual_us;

                // Update item's consume time and statistics
                item_t* item = (item_t*)item_ptrs[i];
                item->produce_time = consume_start;
                item->access_time = (uint64_t)access_time;
                item->waiters = nr_waiters;
                item->spin_time = consume_end - consume_start;
                item->consume_time = get_time_ns();
                item->latency = item->consume_time - item->produce_time;
                consumer_arg->total_latency += item->consume_time - item->produce_time;
                consumer_arg->total_consumed++;
                
                // Add item to processed items array
                consumer_arg->processed_items[iterations] = *item;
                iterations++;
            }
        } else {
            // No items consumed, queue is empty, wait a bit and try again
            break;
        }
    }
    uint64_t end = get_time_ns();

    consumer_arg->total_running_time = end - start;
    
    // printf("Consumer %d: Finished after processing %d items\n", 
    //        consumer_arg->id, consumer_arg->total_consumed);
    return NULL;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -b BUFFER_SIZE     Size of each ring buffer (default: %d)\n", DEFAULT_BUFFER_SIZE);
    printf("  -x NUM_BUFFERS     Number of ring buffer (default: %d)\n", DEFAULT_NUM_BUFFERS);
    printf("  -c NUM_PAIRS       Number of producer-consumer pairs (default: %d)\n", DEFAULT_NUM_PAIRS);
    printf("  -i ITEMS           Items per producer (default: %d)\n", DEFAULT_ITEMS_PER_PRODUCER);
    printf("  -s SERVICE_TIME    Service time in microseconds (default: %d)\n", DEFAULT_SERVICE_TIME);
    printf("  -a BATCH_SIZE      Number of items to process in a batch (default: %d)\n", DEFAULT_BATCH_SIZE);
    printf("  -h                 Display this help message\n");
}

void print_statistics(consumer_args_t* consumer_args, int num_consumers, int items_per_producer, int service_time) {
    double total_packet_consumed = 0;
    double total_throughput = 0.0;
    double total_spin_time = 0.0;
    double total_latency = 0.0;
    double total_service_time = 0.0;
    double total_running_time = 0.0;
    uint64_t total_items = num_consumers * items_per_producer;
    
    // print statistics for each consumer
    for (int i = 0; i < num_consumers; i++) {
        double spin_time_us = (double)consumer_args[i].total_spin_time / 1000.0;
        double service_time_us = (double)consumer_args[i].total_service_time / 1.0;
        
        // Only add valid values to totals
        total_packet_consumed += consumer_args[i].total_consumed;
        if (isfinite(spin_time_us)) {
            total_spin_time += spin_time_us;
        }
        total_latency += (double)consumer_args[i].total_latency / 1000.0;
        
        if (isfinite(service_time_us)) {
            total_service_time += service_time_us;
        }
        total_running_time += (double)consumer_args[i].total_running_time / 1000.0;
    }

    // Print statistics for each consumed item
    printf("\n");
    printf("Consumed items statistics:\n"); 
    printf("ID,Latency,Waiters,SpinTime,AccessTime\n");
    for (int i = 0; i < num_consumers; i++) {
        for (int j = 0; j < consumer_args[i].total_consumed; j++) {
            item_t item = consumer_args[i].processed_items[j];
            // printf("%ld,%.2f,%d,%.2f,%ld\n", item.id, item.latency/1000.0, item.waiters, item.spin_time/1000.0, item.access_time);
        }
    }

    double spin_lock_overhead = 100.0 * total_spin_time / (total_spin_time + total_service_time);
    double service_time_overhead = 100.0 * total_service_time / (total_spin_time + total_service_time);
    total_throughput = (double)total_packet_consumed / (double)total_running_time * 1000000.0;

    // Print total throughput total spin time and total service time
    printf("\n");
    printf("--------------------------------\n");
    printf("End of simulation:\n");
    printf("NUM_PAIRS=%d\n", num_consumers);
    printf("SERVICE_TIME=%d\n", service_time);
    printf("ITEMS_PER_PRODUCER=%d\n", items_per_producer);
    printf("SERVICE_TIME=%d\n", service_time);
    printf("BATCH_SIZE=%d\n", consumer_args->batch_size);
    printf("--------------------------------\n");

    printf("Total_throughput %.2f items/s\n", total_throughput);
    printf("Total_consumed %d items\n", (int)total_packet_consumed);
    printf("Total_running_time %.2f us\n", total_running_time);
    printf("Total_spin_time %.2f us\n", total_spin_time);
    printf("Total_service_time %.2f us\n", total_service_time);
    printf("Average_latency %.2f us\n", total_latency/total_items);
    printf("Spin_lock_overhead %.2f%%\n", spin_lock_overhead);
    printf("Service_time_overhead %.2f%%\n", service_time_overhead);
}

int main(int argc, char* argv[]) {
    // Default parameters
    int buffer_size = DEFAULT_BUFFER_SIZE;
    int num_pairs = DEFAULT_NUM_PAIRS;
    int items_per_producer = DEFAULT_ITEMS_PER_PRODUCER;
    int service_time = DEFAULT_SERVICE_TIME;
    int num_buffers = DEFAULT_NUM_BUFFERS;
    int batch_size = DEFAULT_BATCH_SIZE;
    
    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "b:a:c:i:s:h")) != -1) {
        switch (opt) {
            case 'b':
                buffer_size = atoi(optarg);
                if (buffer_size <= 0) {
                    fprintf(stderr, "Buffer size must be positive\n");
                    return 1;
                }
                break;
            case 'a':
                batch_size = atoi(optarg);
                if (batch_size <= 0) {
                    fprintf(stderr, "Batch size must be positive\n");
                    return 1;
                }
                break;
            case 'x':
                num_buffers = atoi(optarg);
                if (num_buffers <= 0) {
                    fprintf(stderr, "Number of buffers must be positive\n");
                    return 1;
                }
                break;
            case 'c':
                num_pairs = atoi(optarg);
                if (num_pairs <= 0) {
                    fprintf(stderr, "Number of pairs must be positive\n");
                    return 1;
                }
                break;
            case 'i':
                items_per_producer = atoi(optarg);
                if (items_per_producer <= 0) {
                    fprintf(stderr, "Items per producer must be positive\n");
                    return 1;
                }
                break;
            case 's':
                service_time = atoi(optarg);
                if (service_time <= 0) {
                    fprintf(stderr, "Service time must be positive\n");
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage(argv[0]);
                return 1;
        }
    }

    // Initialize random seed
    srand(time(NULL));
    items_per_producer = items_per_producer / num_pairs;

    // Create array of ring buffers - one for each producer-consumer pair
    ring_buffer_t* buffers = (ring_buffer_t*)malloc(num_buffers * sizeof(ring_buffer_t));
    if (!buffers) {
        fprintf(stderr, "Failed to allocate memory for ring buffers\n");
        return 1;
    }
    
    // Initialize each ring buffer
    for (int i = 0; i < num_buffers; i++) {
        if (!ring_buffer_init(&buffers[i], (int)buffer_size/num_buffers, sizeof(item_t))) {
            fprintf(stderr, "Failed to initialize ring buffer %d\n", i);
            
            // Clean up already initialized buffers
            for (int j = 0; j < i; j++) {
                ring_buffer_destroy(&buffers[j]);
            }
            free(buffers);
            return 1;
        }
    }

    // Display configuration
    printf("Configuration:\n");
    printf("  Buffer Size: %d\n", buffer_size);
    printf("  Number of Buffers: %d\n", num_buffers);
    printf("  Per-Buffer Buffer Size: %ld\n", buffers[0].size);
    printf("  Number of Producer-Consumer Pairs: %d\n", num_pairs);
    printf("  Items per Producer: %d\n", items_per_producer);
    printf("  Service Time: %d us\n", service_time);
    printf("\n");
    
    pthread_t* producers = (pthread_t*)malloc(num_pairs * sizeof(pthread_t));
    pthread_t* consumers = (pthread_t*)malloc(num_pairs * sizeof(pthread_t));
    producer_args_t* producer_args = (producer_args_t*)malloc(num_pairs * sizeof(producer_args_t));
    consumer_args_t* consumer_args = (consumer_args_t*)malloc(num_pairs * sizeof(consumer_args_t));
    
    if (!producers || !consumers || !producer_args || !consumer_args) {
        fprintf(stderr, "Failed to allocate memory for threads\n");
        
        // Clean up buffers
        for (int i = 0; i < num_buffers; i++) {
            ring_buffer_destroy(&buffers[i]);
        }
        free(buffers);
        
        // Free any successfully allocated arrays
        free(producers);
        free(consumers);
        free(producer_args);
        free(consumer_args);
        return 1;
    }

    // Create producer threads
    for (int i = 0; i < num_pairs; i++) {
        producer_args[i].id = i + 1;
        producer_args[i].core = i % sysconf(_SC_NPROCESSORS_ONLN); // Distribute across available cores
        producer_args[i].total_produced = 0;
        producer_args[i].total_spin_time = 0;
        producer_args[i].items_per_producer = items_per_producer;
        producer_args[i].buffer = &buffers[i % num_buffers];  // Assign this producer's dedicated buffer
        
        if (pthread_create(&producers[i], NULL, producer_thread, &producer_args[i]) != 0) {
            fprintf(stderr, "Failed to create producer thread %d\n", i + 1);
            
            // Clean up buffers
            for (int j = 0; j < num_buffers; j++) {
                ring_buffer_destroy(&buffers[j]);
            }
            free(buffers);
            free(producers);
            free(consumers);
            free(producer_args);
            free(consumer_args);
            return 1;
        }
    }
    
    // Create consumer threads - each paired with a specific producer
    for (int i = 0; i < num_pairs; i++) {
        consumer_args[i].processed_items = (item_t*)malloc(items_per_producer * sizeof(item_t));
        if (!consumer_args[i].processed_items) {
            fprintf(stderr, "Failed to allocate memory for processed items\n");
            // Free previously created consumers processed items
            for (int j = 0; j < i; j++) {
                free(consumer_args[j].processed_items);
            }
            ring_buffer_destroy(&buffers[i]);
            free(buffers);
            free(producers);
            free(consumers);
            free(producer_args);
            free(consumer_args);
            return 1;
        }

        consumer_args[i].id = i + 1;
        consumer_args[i].core = (i + num_pairs) % sysconf(_SC_NPROCESSORS_ONLN); // Distribute across available cores
        consumer_args[i].total_consumed = 0;
        consumer_args[i].total_spin_time = 0;
        consumer_args[i].total_latency = 0;
        consumer_args[i].total_service_time = 0;
        consumer_args[i].service_time = service_time;
        consumer_args[i].total_items = items_per_producer;  // Each consumer handles exactly one producer's items
        consumer_args[i].total_running_time = 0;
        consumer_args[i].batch_size = batch_size;
        consumer_args[i].num_consumers = num_pairs;
        consumer_args[i].buffer = &buffers[i % num_buffers];  // Assign this consumer's dedicated buffer (same as paired producer)

        if (pthread_create(&consumers[i], NULL, consumer_thread, &consumer_args[i]) != 0) {
            fprintf(stderr, "Failed to create consumer thread %d\n", i + 1);
            // Free previously created consumers processed items
            for (int j = 0; j <= i; j++) {
                free(consumer_args[j].processed_items);
            }
            // Clean up buffers
            for (int j = 0; j < num_buffers; j++) {
                ring_buffer_destroy(&buffers[j]);
            }
            free(buffers);
            free(producers);
            free(consumers);
            free(producer_args);
            free(consumer_args);
            return 1;
        }
    }
    
    // Wait for producer threads to finish
    for (int i = 0; i < num_pairs; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Wait for consumer threads to finish
    for (int i = 0; i < num_pairs; i++) {
        pthread_join(consumers[i], NULL);
    }

    // Print total statistics
    print_statistics(consumer_args, num_pairs, items_per_producer, service_time);

    // Print individual pair statistics if desired
    // for (int i = 0; i < num_pairs; i++) {
    //     printf("\nPair %d Statistics:\n", i + 1);
    //     printf("  Produced: %d items\n", producer_args[i].total_produced);
    //     printf("  Consumed: %d items\n", consumer_args[i].total_consumed);
    //     double latency_us = (double)consumer_args[i].total_latency / consumer_args[i].total_consumed / 1000.0;
    //     printf("  Average Latency: %.2f us\n", latency_us);
    // }

    // Clean up
    for (int i = 0; i < num_buffers; i++) {
        ring_buffer_destroy(&buffers[i]);
    }
    // Free memory of processed items
    for (int i = 0; i < num_pairs; i++) {
        free(consumer_args[i].processed_items);
    }
    free(buffers);
    free(producers);
    free(consumers);
    free(producer_args);
    free(consumer_args);

    return 0;
}
