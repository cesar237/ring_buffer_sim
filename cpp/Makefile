# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread

# Binary name
TARGET = ring_buffer_app

# Source files
SRC = ring_buffer.cpp

# Object files
OBJ = $(SRC:.cpp=.o)

# Default target
all: $(TARGET)

# Link the object files to create the binary
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJ) $(TARGET)

# Run the application with default parameters
run: $(TARGET)
	./$(TARGET)

# Run with custom parameters (example with microsecond service time)
run-custom: $(TARGET)
	./$(TARGET) --buffer-size 200 --producers 8 --consumers 4 --service-time 5000 --run-time 20

# Benchmark with very low service time to test high throughput
run-benchmark: $(TARGET)
	./$(TARGET) --buffer-size 1000 --producers 16 --consumers 16 --service-time 100 --run-time 30

.PHONY: all clean run run-custom run-benchmark