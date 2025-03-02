CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2
LDFLAGS = -lrt -pthread

# Define object files
OBJS = simulation.o example.o
OBJS_BATCH = simulation.o example_batch.o

# Default target
all: simulation simulation_batch

# Linking the executable
simulation: $(OBJS)
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)
simulation_batch: $(OBJS_BATCH)
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling source files
simulation.o: simulation.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example.o: example.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example_batch.o: example_batch.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

# Clean target
clean:
	rm -f $(OBJS) $(TARGET)

# Run the example program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run