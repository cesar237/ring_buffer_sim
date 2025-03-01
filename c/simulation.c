/**
 * simulation.c
 * Implementation of CPU activity simulation functions
 */

#include "simulation.h"
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>


// Global configuration
static simulation_config_t global_config;
static bool is_initialized = false;

// Get current time in nanoseconds
uint64_t get_time_ns(void) {
    struct timeval tv;
    
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
}

bool simulation_init(void) {
    if (is_initialized) {
        return true;
    }
    
    // Set up default configuration
    global_config.mode = BUSY_WAIT_ACCURATE;
    global_config.check_frequency = 1000;
    global_config.verbose = false;
    
    is_initialized = true;
    return true;
}

void simulation_cleanup(void) {
    is_initialized = false;
}

simulation_config_t simulation_get_default_config(void) {
    simulation_config_t config;
    config.mode = BUSY_WAIT_ACCURATE;
    config.check_frequency = 1000;
    config.verbose = false;
    return config;
}

void simulation_set_config(simulation_config_t config) {
    if (!is_initialized) {
        simulation_init();
    }
    
    global_config = config;
}

simulation_config_t simulation_get_config(void) {
    if (!is_initialized) {
        simulation_init();
    }
    
    return global_config;
}

static simulation_stats_t busy_wait_accurate(uint64_t microseconds) {
    simulation_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.requested_us = microseconds;
    
    // Convert microseconds to nanoseconds
    uint64_t wait_ns = microseconds * 1000ULL;
    
    // Get the start time
    uint64_t start_ns = get_time_ns();
    uint64_t end_ns = start_ns + wait_ns;
    
    // Busy wait loop
    volatile uint64_t counter = 0;
    uint64_t check_frequency = global_config.check_frequency;
    
    // Adjust check frequency based on wait duration
    if (microseconds < 1000) {
        check_frequency = check_frequency / 10;  // Check more often for short durations
        if (check_frequency < 10) check_frequency = 10;
    } else if (microseconds > 1000000) {
        check_frequency = check_frequency * 10;  // Check less often for long durations
    }
    
    while (1) {
        // Do some meaningless work
        counter++;
        
        // Check if we've waited long enough
        if (counter % check_frequency == 0) {
            uint64_t current_ns = get_time_ns();
            stats.time_checks++;
            
            if (current_ns >= end_ns) {
                break;
            }
        }
    }
    
    // Calculate statistics
    uint64_t end_time = get_time_ns();
    stats.iterations = counter;
    stats.actual_us = (end_time - start_ns) / 1000ULL;
    stats.accuracy_percent = 100.0 * (double)stats.actual_us / (double)stats.requested_us;
    
    return stats;
}

static simulation_stats_t busy_wait_efficient(uint64_t microseconds) {
    simulation_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.requested_us = microseconds;
    
    // Convert microseconds to nanoseconds
    uint64_t wait_ns = microseconds * 1000ULL;
    
    // Get the start time
    uint64_t start_ns = get_time_ns();
    uint64_t end_ns = start_ns + wait_ns;
    
    // For efficiency, we'll use a graduated approach:
    // - For the bulk of the time, check infrequently
    // - As we get closer to the target time, check more frequently
    volatile uint64_t counter = 0;
    
    while (1) {
        // Do some meaningless work
        counter++;
        
        // Determine how frequently to check time based on how close we are to the end
        uint64_t current_ns = get_time_ns();
        stats.time_checks++;
        
        if (current_ns >= end_ns) {
            break;
        }
        
        // Calculate remaining time
        uint64_t remaining_ns = end_ns - current_ns;
        
        // Adjust check frequency based on remaining time
        if (remaining_ns > 1000000) {  // More than 1ms remaining
            counter += 10000;  // Skip ahead, check less frequently
        } else if (remaining_ns > 100000) {  // More than 100us remaining
            counter += 1000;   // Check more frequently
        } else if (remaining_ns > 10000) {  // More than 10us remaining
            counter += 100;    // Check very frequently
        }
        // Otherwise check on every iteration for maximum precision
    }
    
    // Calculate statistics
    uint64_t end_time = get_time_ns();
    stats.iterations = counter;
    stats.actual_us = (end_time - start_ns) / 1000ULL;
    stats.accuracy_percent = 100.0 * (double)stats.actual_us / (double)stats.requested_us;
    
    return stats;
}


simulation_stats_t simulation_busy_wait_us(uint64_t microseconds) {
    if (!is_initialized) {
        simulation_init();
    }
    
    // Choose the appropriate wait implementation based on the configuration
    switch (global_config.mode) {
        case BUSY_WAIT_ACCURATE:
            return busy_wait_accurate(microseconds);
        
        case BUSY_WAIT_EFFICIENT:
            return busy_wait_efficient(microseconds);
         
        default:
            return busy_wait_accurate(microseconds);
    }
}

simulation_stats_t simulation_busy_wait_ms(uint64_t milliseconds) {
    return simulation_busy_wait_us(milliseconds * 1000ULL);
}

simulation_stats_t simulation_busy_wait_s(double seconds) {
    uint64_t microseconds = (uint64_t)(seconds * 1000000.0);
    return simulation_busy_wait_us(microseconds);
}


void simulation_print_stats(simulation_stats_t stats) {
    printf("Simulation Statistics:\n");
    printf("  Requested duration: %lu microseconds\n", stats.requested_us);
    printf("  Actual duration:    %lu microseconds\n", stats.actual_us);
    printf("  Accuracy:           %.2f%%\n", stats.accuracy_percent);
    printf("  Iterations:         %lu\n", stats.iterations);
    printf("  Time checks:        %lu\n", stats.time_checks);
}