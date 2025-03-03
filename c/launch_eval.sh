#! /bin/bash

# This script is used to launch the different simulation scripts with set parameters

# Set the number of runs
n_runs=100
# Set the number of consumers
n_consumers="2 5 10 20 30 40 50 60"
# set the number of items
n_items=10000
# Set the  number of batches
n_batches="1 2 4 8 16 32"

mkdir -p logdir

mkdir -p logdir/base
# 1- Test base
for run in $(seq 1 $n_runs); do
    for n in $n_consumers; do
        echo "Run $run with $n consumers"
        ./simulation_track_waiters -c $n > logdir/base/base-$n-$run.log
    done
done

# 2- Test with batches
mkdir -p logdir/batch
for run in $(seq 1 $n_runs); do
    for n in $n_consumers; do
        echo "Run $run with $n consumers"
        for b in $n_batches; do
            echo "Run $run with $n consumers and $b batches"
            ./simulation_batch_track_waiters -c $n -a $b > logdir/batch/batch-$n-$b-$run.log
        done
    done
done

# 3- Test with multi ring buffer
mkdir -p logdir/multi
for run in $(seq 1 $n_runs); do
    for n in $n_consumers; do
        echo "Run $run with $n consumers"
        ./simulation_multi_rb_track_waiters -c $n > logdir/multi/multi-$n-$run.log
    done
done
