CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2
LDFLAGS = -lrt -pthread -lm

# Define object files
OBJS = simulation.o example.o
OBJS_BATCH = simulation.o example_batch.o
OBJS_MULTI_RB = simulation.o example_multi_rb.o
OBJS_TRACK = simulation.o example_track_waiters.o

# Default target
all: simulation simulation_batch simulation_multi_rb simulation_track_waiters

# Linking the executable
simulation: $(OBJS)
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)
simulation_batch: $(OBJS_BATCH)
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)
simulation_multi_rb: $(OBJS_MULTI_RB)
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)
simulation_track_waiters: $(OBJS_TRACK)	
	$(CC) -g $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling source files
simulation.o: simulation.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example.o: example.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example_batch.o: example_batch.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example_multi_rb.o: example_multi_rb.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

example_track_waiters.o: example_track_waiters.c simulation.h
	$(CC) -g $(CFLAGS) -c $<

# Clean target
clean:
	rm -f $(OBJS) $(OBJS_BATCH) $(OBJS_MULTI_RB) $(OBJS_TRACK) $(TARGET) 

# Run the example program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run