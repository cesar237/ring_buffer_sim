/**
 * simulation.h
 * Provides functions for simulating CPU activity with microsecond precision
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

/**
 * Different work simulation modes
 */
typedef enum {
    BUSY_WAIT_ACCURATE,   // Most accurate, highest CPU usage
    BUSY_WAIT_EFFICIENT,  // Less accurate, still high CPU usage
    YIELD_WAIT,           // Yields to other threads, medium accuracy
    HYBRID_WAIT           // Combination of busy wait and yield
} simulation_mode_t;

/**
 * Configuration for the simulation
 */
typedef struct {
    simulation_mode_t mode;    // Simulation mode
    uint32_t check_frequency;  // How often to check time (iterations)
    bool verbose;              // Print diagnostic information
} simulation_config_t;

/**
 * Statistics from a simulation run
 */
typedef struct {
    uint64_t iterations;       // Number of iterations performed
    uint64_t time_checks;      // Number of time checks performed
    uint64_t requested_us;     // Requested duration in microseconds
    uint64_t actual_us;        // Actual duration in microseconds
    double accuracy_percent;   // Accuracy as a percentage
} simulation_stats_t;

/**
 * Function to get time in nanoseconds
 * @return current time in nanoseconds
 */
uint64_t get_time_ns(void);

/**
 * Initialize the simulation library
 * @return true if initialization succeeded, false otherwise
 */
bool simulation_init(void);

/**
 * Clean up the simulation library
 */
void simulation_cleanup(void);

/**
 * Get the default simulation configuration
 * @return Default configuration
 */
simulation_config_t simulation_get_default_config(void);

/**
 * Set the global simulation configuration
 * @param config The configuration to set
 */
void simulation_set_config(simulation_config_t config);

/**
 * Get the current simulation configuration
 * @return Current configuration
 */
simulation_config_t simulation_get_config(void);

/**
 * Busy wait for the specified number of microseconds
 * 
 * @param microseconds Duration to wait in microseconds
 * @return Statistics about the simulation run
 */
simulation_stats_t simulation_busy_wait_us(uint64_t microseconds);

/**
 * Busy wait for the specified number of milliseconds
 * 
 * @param milliseconds Duration to wait in milliseconds
 * @return Statistics about the simulation run
 */
simulation_stats_t simulation_busy_wait_ms(uint64_t milliseconds);

/**
 * Busy wait for the specified number of seconds
 * 
 * @param seconds Duration to wait in seconds
 * @return Statistics about the simulation run
 */
simulation_stats_t simulation_busy_wait_s(double seconds);


/**
 * Print statistics from a simulation run
 * 
 * @param stats Statistics to print
 */
void simulation_print_stats(simulation_stats_t stats);

#endif /* SIMULATION_H */