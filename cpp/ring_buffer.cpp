#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <random>
#include <queue>
#include <iomanip>
#include <algorithm> // For std::sort

// Configuration constants (can be modified as command line parameters)
struct Config {
    size_t BUFFER_SIZE = 100;
    size_t NUM_PRODUCERS = 4;
    size_t NUM_CONSUMERS = 4;
    int SERVICE_TIME_US = 10000; // Default service time in microseconds (10ms)
    int RUN_TIME_SECONDS = 10; // How long to run the simulation
};

// Request structure
struct Request {
    int id;
    int service_time_us; // Changed to microseconds
    std::chrono::high_resolution_clock::time_point produced_time;
    std::chrono::high_resolution_clock::time_point consumed_time; // When the request was consumed
    std::chrono::high_resolution_clock::time_point service_end_time; // When service was completed
    // Timing details
    long long produce_function_time_ns; // Time spent inside produce function
    long long consume_function_time_ns; // Time spent inside consume function
};

// Statistics structure
struct Statistics {
    std::atomic<size_t> total_produced{0};
    std::atomic<size_t> total_consumed{0};
    std::atomic<long long> produce_time_ns{0};
    std::atomic<long long> consume_time_ns{0};
    std::atomic<long long> service_time_ns{0};
    std::atomic<long long> response_time_ns{0}; // Time from production to consumption
    std::atomic<long long> total_time_ns{0};    // Total time from production to service completion
    
    // For calculating percentiles and distributions
    mutable std::mutex results_mutex;
    std::vector<long long> response_times; // From production to consumption
    std::vector<long long> service_times;  // Time spent in service
    std::vector<long long> total_times;    // End-to-end time
    std::vector<long long> produce_function_times; // Time inside produce function
    std::vector<long long> consume_function_times; // Time inside consume function
};

// Spin lock implementation
class SpinLock {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Spin until we acquire the lock
        }
    }
    
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

// Ring Buffer implementation
class RingBuffer {
private:
    std::vector<Request> buffer;
    size_t head = 0;  // Producer writes here
    size_t tail = 0;  // Consumer reads from here
    size_t count = 0; // Number of items in buffer
    size_t capacity;
    
    SpinLock producer_lock;
    SpinLock consumer_lock;
    
    Statistics& stats;
    
public:
    RingBuffer(size_t size, Statistics& stats) : capacity(size), stats(stats) {
        buffer.resize(size);
    }
    
    bool produce(Request& request) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        producer_lock.lock();
        bool success = false;
        
        if (count < capacity) {
            buffer[head] = request;
            head = (head + 1) % capacity;
            count++;
            success = true;
        }
        
        producer_lock.unlock();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        
        request.produce_function_time_ns = duration;
        stats.produce_time_ns.fetch_add(duration, std::memory_order_relaxed);
        
        if (success) {
            stats.total_produced.fetch_add(1, std::memory_order_relaxed);
            
            // Store produce function time
            std::lock_guard<std::mutex> lock(stats.results_mutex);
            stats.produce_function_times.push_back(duration);
        }
        
        return success;
    }
    
    bool consume(Request& request) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        consumer_lock.lock();
        bool success = false;
        
        if (count > 0) {
            request = buffer[tail];
            tail = (tail + 1) % capacity;
            count--;
            success = true;
        }
        
        consumer_lock.unlock();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        
        request.consume_function_time_ns = duration;
        stats.consume_time_ns.fetch_add(duration, std::memory_order_relaxed);
        
        if (success) {
            request.consumed_time = end_time;
            
            // Calculate response time (from production to consumption)
            auto response_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - request.produced_time).count();
            stats.response_time_ns.fetch_add(response_time, std::memory_order_relaxed);
            
            // Store consume function time
            std::lock_guard<std::mutex> lock(stats.results_mutex);
            stats.consume_function_times.push_back(duration);
            stats.response_times.push_back(response_time);
        }
        
        return success;
    }
    
    // Record service completion
    void record_service_completion(const Request& request) {
        auto service_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            request.service_end_time - request.consumed_time).count();
            
        auto total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            request.service_end_time - request.produced_time).count();
            
        stats.service_time_ns.fetch_add(service_time, std::memory_order_relaxed);
        stats.total_time_ns.fetch_add(total_time, std::memory_order_relaxed);
        stats.total_consumed.fetch_add(1, std::memory_order_relaxed);
        
        // Store detailed timing data
        std::lock_guard<std::mutex> lock(stats.results_mutex);
        stats.service_times.push_back(service_time);
        stats.total_times.push_back(total_time);
    }
    
    size_t size() const {
        return count;
    }
};

// Producer thread function
void producer_thread(int id, RingBuffer& buffer, std::atomic<bool>& running, int service_time_us) {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Allow some variance in service time
    std::uniform_int_distribution<> service_time_dist(
        std::max(1, service_time_us - service_time_us / 4), 
        service_time_us + service_time_us / 4
    );
    
    int request_id = 0;
    
    while (running.load(std::memory_order_relaxed)) {
        Request req;
        req.id = id * 10000 + request_id++;
        req.service_time_us = service_time_dist(gen);
        req.produced_time = std::chrono::high_resolution_clock::now();
        
        while (!buffer.produce(req) && running.load(std::memory_order_relaxed)) {
            // If buffer is full, retry after a small delay
            std::this_thread::yield();
        }
        
        // Small delay to prevent overwhelming the system
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Reduced to allow higher throughput
    }
}

// Consumer thread function
void consumer_thread(RingBuffer& buffer, std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        Request req;
        if (buffer.consume(req)) {
            // Simulate processing by sleeping for the service time in microseconds
            std::this_thread::sleep_for(std::chrono::microseconds(req.service_time_us));
            
            // Record service completion
            req.service_end_time = std::chrono::high_resolution_clock::now();
            buffer.record_service_completion(req);
        } else {
            // If buffer is empty, wait a bit
            std::this_thread::yield();
        }
    }
}

// Helper function to calculate percentiles
long long calculate_percentile(const std::vector<long long>& data, double percentile) {
    if (data.empty()) return 0;
    
    std::vector<long long> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());
    
    size_t index = static_cast<size_t>(percentile * sorted_data.size() / 100);
    return sorted_data[index];
}

// Helper function to format time in appropriate units
std::string format_time(long long nanoseconds) {
    if (nanoseconds < 1000) {
        return std::to_string(nanoseconds) + " ns";
    } else if (nanoseconds < 1000000) {
        return std::to_string(nanoseconds / 1000.0) + " μs";
    } else if (nanoseconds < 1000000000) {
        return std::to_string(nanoseconds / 1000000.0) + " ms";
    } else {
        return std::to_string(nanoseconds / 1000000000.0) + " s";
    }
}

// Function to print detailed timing statistics for a vector of measurements
void print_timing_stats(const std::string& name, const std::vector<long long>& times) {
    if (times.empty()) {
        std::cout << name << ": No data" << std::endl;
        return;
    }
    
    long long sum = 0;
    for (long long t : times) {
        sum += t;
    }
    
    long long min = *std::min_element(times.begin(), times.end());
    long long max = *std::max_element(times.begin(), times.end());
    double mean = static_cast<double>(sum) / times.size();
    
    long long p50 = calculate_percentile(times, 50);
    long long p95 = calculate_percentile(times, 95);
    long long p99 = calculate_percentile(times, 99);
    
    std::cout << name << ":" << std::endl;
    std::cout << "  Count: " << times.size() << std::endl;
    std::cout << "  Total: " << format_time(sum) << std::endl;
    std::cout << "  Min: " << format_time(min) << std::endl;
    std::cout << "  Max: " << format_time(max) << std::endl;
    std::cout << "  Mean: " << format_time(static_cast<long long>(mean)) << std::endl;
    std::cout << "  P50: " << format_time(p50) << std::endl;
    std::cout << "  P95: " << format_time(p95) << std::endl;
    std::cout << "  P99: " << format_time(p99) << std::endl;
}

// Function to print statistics
void print_statistics(const Statistics& stats, std::chrono::seconds duration) {
    double seconds = duration.count();
    size_t total_produced = stats.total_produced.load(std::memory_order_relaxed);
    size_t total_consumed = stats.total_consumed.load(std::memory_order_relaxed);
    
    double producer_throughput = total_produced / seconds;
    double consumer_throughput = total_consumed / seconds;
    
    std::cout << "=== Performance Summary ===" << std::endl;
    std::cout << "Total runtime: " << seconds << " seconds" << std::endl;
    std::cout << "Total produced: " << total_produced << " requests" << std::endl;
    std::cout << "Total consumed: " << total_consumed << " requests" << std::endl;
    std::cout << "Producer throughput: " << std::fixed << std::setprecision(2) 
              << producer_throughput << " req/sec" << std::endl;
    std::cout << "Consumer throughput: " << std::fixed << std::setprecision(2) 
              << consumer_throughput << " req/sec" << std::endl;
    std::cout << std::endl;
    
    // Make copies of vectors to prevent race conditions
    std::vector<long long> produce_function_times;
    std::vector<long long> consume_function_times;
    std::vector<long long> response_times;
    std::vector<long long> service_times;
    std::vector<long long> total_times;
    
    {
        std::lock_guard<std::mutex> lock(stats.results_mutex);
        produce_function_times = stats.produce_function_times;
        consume_function_times = stats.consume_function_times;
        response_times = stats.response_times;
        service_times = stats.service_times;
        total_times = stats.total_times;
    }
    
    std::cout << "=== Detailed Timing Statistics ===" << std::endl;
    
    // Print produce function timing statistics
    print_timing_stats("Time spent in produce function", produce_function_times);
    std::cout << std::endl;
    
    // Print consume function timing statistics
    print_timing_stats("Time spent in consume function", consume_function_times);
    std::cout << std::endl;
    
    // Print response time statistics (from production to consumption)
    print_timing_stats("Response time (production to consumption)", response_times);
    std::cout << std::endl;
    
    // Print service time statistics (processing time)
    print_timing_stats("Service time (request processing)", service_times);
    std::cout << std::endl;
    
    // Print total time statistics (end-to-end)
    print_timing_stats("Total time (end-to-end)", total_times);
    
    std::cout << "============================" << std::endl;
}

int main(int argc, char* argv[]) {
    Config config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            std::cerr << "Missing value for argument " << argv[i] << std::endl;
            return 1;
        }
        
        std::string arg = argv[i];
        if (arg == "--buffer-size") {
            config.BUFFER_SIZE = std::stoul(argv[i+1]);
        } else if (arg == "--producers") {
            config.NUM_PRODUCERS = std::stoul(argv[i+1]);
        } else if (arg == "--consumers") {
            config.NUM_CONSUMERS = std::stoul(argv[i+1]);
        } else if (arg == "--service-time") {
            config.SERVICE_TIME_US = std::stoi(argv[i+1]);
        } else if (arg == "--run-time") {
            config.RUN_TIME_SECONDS = std::stoi(argv[i+1]);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    
    std::cout << "Starting with configuration:" << std::endl;
    std::cout << "  Buffer size: " << config.BUFFER_SIZE << std::endl;
    std::cout << "  Producers: " << config.NUM_PRODUCERS << std::endl;
    std::cout << "  Consumers: " << config.NUM_CONSUMERS << std::endl;
    std::cout << "  Service time: " << config.SERVICE_TIME_US << " μs" << std::endl;
    std::cout << "  Run time: " << config.RUN_TIME_SECONDS << " seconds" << std::endl;
    
    // Initialize statistics
    Statistics stats;
    
    // Create ring buffer
    RingBuffer buffer(config.BUFFER_SIZE, stats);
    
    // Control flag for threads
    std::atomic<bool> running{true};
    
    // Create and start producer threads
    std::vector<std::thread> producers;
    for (size_t i = 0; i < config.NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer_thread, i, std::ref(buffer), std::ref(running), 
                              config.SERVICE_TIME_US);
    }
    
    // Create and start consumer threads
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < config.NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer_thread, std::ref(buffer), std::ref(running));
    }
    
    // Run for specified time
    auto start_time = std::chrono::steady_clock::now();
    
    // Monitor and report progress
    for (int i = 0; i < config.RUN_TIME_SECONDS; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        size_t produced = stats.total_produced.load(std::memory_order_relaxed);
        size_t consumed = stats.total_consumed.load(std::memory_order_relaxed);
        
        std::cout << "\rRunning... " << (i+1) << "/" << config.RUN_TIME_SECONDS 
                  << " seconds, produced: " << produced << ", consumed: " << consumed 
                  << ", buffer: " << buffer.size() << "   " << std::flush;
    }
    std::cout << std::endl;
    
    // Signal threads to stop
    running.store(false, std::memory_order_relaxed);
    
    // Wait for all threads to finish
    for (auto& t : producers) {
        t.join();
    }
    
    for (auto& t : consumers) {
        t.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Print final statistics
    print_statistics(stats, duration);
    
    return 0;
}