#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* Define the item structure */
typedef struct item_t {
    double arrival_time;   // Arrival time following Poisson process
    int id;                // Unique identifier for the item
    // Add any other item properties you need
} item_t;

/**
 * Generate a random number following an exponential distribution
 * with rate parameter lambda.
 * 
 * The time between arrivals in a Poisson process follows an 
 * exponential distribution.
 * 
 * @param lambda The rate parameter of the Poisson process
 * @return A random value from the exponential distribution
 */
double random_exponential(double lambda) {
    double u = (double)rand() / RAND_MAX;  // Uniform random number between 0 and 1
    
    // Convert uniform to exponential using inverse transform sampling
    // Avoid taking log of 0
    while (u == 0) {
        u = (double)rand() / RAND_MAX;
    }
    
    return -log(u) / lambda;
}

/**
 * Create a new item with an arrival time that follows a Poisson process.
 * 
 * @param lambda The rate parameter (average number of arrivals per unit time)
 * @param current_time The current simulation time
 * @param next_id The ID to assign to the new item
 * @return A new item structure with arrival time following Poisson process
 */
item_t create_poisson_arrival_item(double lambda, double current_time, int next_id) {
    item_t new_item;
    
    // Generate the inter-arrival time (time since last arrival)
    double inter_arrival_time = random_exponential(lambda);
    
    // Set the arrival time as current time plus inter-arrival time
    new_item.arrival_time = current_time + inter_arrival_time;
    new_item.id = next_id;
    
    return new_item;
}

/**
 * Generate a sequence of n items with arrival times following a Poisson process.
 * 
 * @param lambda The rate parameter (average number of arrivals per unit time)
 * @param n The number of items to generate
 * @return An array of items with Poisson arrival times
 */
item_t* generate_poisson_arrivals(double lambda, int n) {
    item_t* items = (item_t*)malloc(n * sizeof(item_t));
    double current_time = 0.0;
    
    if (items == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    
    // Seed the random number generator
    srand(time(NULL));
    
    // Generate n items with Poisson arrival times
    for (int i = 0; i < n; i++) {
        items[i] = create_poisson_arrival_item(lambda, current_time, i + 1);
        current_time = items[i].arrival_time;  // Update current time for next item
    }
    
    return items;
}

/* Example usage */
void example_usage() {
    double lambda = 5.0;  // Average 5 arrivals per unit time
    int n = 10;           // Generate 10 items
    
    item_t* items = generate_poisson_arrivals(lambda, n);
    
    if (items != NULL) {
        printf("Generated %d items with Poisson arrival times (lambda = %.2f):\n", n, lambda);
        for (int i = 0; i < n; i++) {
            printf("Item %d: arrival time = %.6f\n", items[i].id, items[i].arrival_time);
        }
        
        // Calculate and print inter-arrival times
        printf("\nInter-arrival times:\n");
        double prev_time = 0.0;
        for (int i = 0; i < n; i++) {
            printf("Item %d: inter-arrival time = %.6f\n", 
                  items[i].id, 
                  i == 0 ? items[i].arrival_time : items[i].arrival_time - items[i-1].arrival_time);
        }
        
        free(items);  // Don't forget to free the allocated memory
    }
}