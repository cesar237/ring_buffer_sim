CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2
LDFLAGS = -lrt -pthread

# Define object files
OBJS = simulation.o example.o

# Define the executable
TARGET = simulation_example

# Default target
all: $(TARGET)

# Linking the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling source files
simulation.o: simulation.c simulation.h
	$(CC) $(CFLAGS) -c $<

example.o: example.c simulation.h
	$(CC) $(CFLAGS) -c $<

# Clean target
clean:
	rm -f $(OBJS) $(TARGET)

# Run the example program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run