/**
 * example.c
 * Example usage of the CPU activity simulation library
 */

#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Initialize the simulation library
    if (!simulation_init()) {
        fprintf(stderr, "Failed to initialize simulation library\n");
        return 1;
    }
    
    printf("CPU Activity Simulation Examples\n");
    printf("================================\n\n");
    
    // Example 0: Basic busy wait for 10 microseconds
    printf("Example 0: Basic busy wait for 10 microseconds\n");
    simulation_stats_t stats0 = simulation_busy_wait_us(10);
    simulation_print_stats(stats0);
    printf("\n");
    
    // Example 1: Basic busy wait for 500 microseconds
    printf("Example 1: Basic busy wait for 500 microseconds\n");
    simulation_stats_t stats1 = simulation_busy_wait_us(500);
    simulation_print_stats(stats1);
    printf("\n");
    
    // Example 2: Busy wait for 10 milliseconds
    printf("Example 2: Busy wait for 10 milliseconds\n");
    simulation_stats_t stats2 = simulation_busy_wait_ms(10);
    simulation_print_stats(stats2);
    printf("\n");
    
    // Example 3: Try different modes
    printf("Example 3: Trying different simulation modes for 5ms\n");
    
    // Default mode (BUSY_WAIT_ACCURATE)
    printf("Default mode (BUSY_WAIT_ACCURATE):\n");
    simulation_stats_t stats3_default = simulation_busy_wait_ms(5);
    simulation_print_stats(stats3_default);
    printf("\n");
    
    // Efficient mode
    simulation_config_t config = simulation_get_config();
    config.mode = BUSY_WAIT_EFFICIENT;
    simulation_set_config(config);
    
    printf("BUSY_WAIT_EFFICIENT mode:\n");
    simulation_stats_t stats3_efficient = simulation_busy_wait_ms(5);
    simulation_print_stats(stats3_efficient);
    printf("\n");
    
    // Yield mode
    config.mode = YIELD_WAIT;
    simulation_set_config(config);
    
    printf("YIELD_WAIT mode:\n");
    simulation_stats_t stats3_yield = simulation_busy_wait_ms(5);
    simulation_print_stats(stats3_yield);
    printf("\n");
    
    // Hybrid mode
    config.mode = HYBRID_WAIT;
    simulation_set_config(config);
    
    printf("HYBRID_WAIT mode:\n");
    simulation_stats_t stats3_hybrid = simulation_busy_wait_ms(5);
    simulation_print_stats(stats3_hybrid);
    printf("\n");
    
    // Example 5: Microsecond precision test
    printf("Example 5: Microsecond precision test\n");
    
    // Reset to accurate mode
    config.mode = BUSY_WAIT_ACCURATE;
    config.check_frequency = 100; // More frequent checks for better precision
    simulation_set_config(config);
    
    // Test different microsecond values
    uint64_t test_us[] = {1, 10, 50, 100, 500, 1000};
    size_t num_tests = sizeof(test_us) / sizeof(test_us[0]);
    
    for (size_t i = 0; i < num_tests; i++) {
        printf("Waiting for %lu microseconds:\n", test_us[i]);
        simulation_stats_t stats = simulation_busy_wait_us(test_us[i]);
        simulation_print_stats(stats);
        printf("\n");
    }
    
    // Clean up
    simulation_cleanup();
    
    return 0;
}