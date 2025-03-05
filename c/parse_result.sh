#! /bin/bash

mkdir -p results

function log_items_csv() {
    file=$1
    suffix=$2

    cat $dir/$file | head -n -16 | tail -n +11 | awk -v suffix=$suffix '{print $0","suffix}'
}

function log_summary_csv() {
    file=$1
    suffix=$2

    total_consumed=$(cat $file | tail -n 15 | grep "Total_consumed" | awk '{print $2}')
    total_running_time=$(cat $file | tail -n 15 | grep "Total_running_time" | awk '{print $2}')
    total_spin_time=$(cat $file | tail -n 15 | grep "Total_spin_time" | awk '{print $2}')
    total_service_time=$(cat $file | tail -n 15 | grep "Total_service_time" | awk '{print $2}')

    echo $total_consumed,$total_running_time,$total_spin_time,$total_service_time,$suffix
}

# This script is used to parse the result of the test case.
# Parse base log

dir=logdir/base
resdir=results/base
mkdir -p $resdir

echo ID,Latency,Waiters,SpinTime,AccessTime,Variant,Consumers,ServiceTime,Run > $resdir/base_items.csv
echo TotalConsumed,TotalRunningTime,TotalSpinTime,TotalServiceTime,Variant,Consumers,ServiceTime,Run > $resdir/base_summary.csv

if [ -d $dir ]; then
    echo "Parse base logs"
    for file in `ls $dir`; do
        echo "Parse $file"
        prefix=$(echo $file | cut -d '.' -f 1)
        variant=$(echo $prefix | cut -d '-' -f 1)
        consumers=$(echo $prefix | cut -d '-' -f 2)
        run=$(echo $prefix | cut -d '-' -f 3)
        service_time=$(cat $dir/$file | grep SERVICE_TIME | cut -d '=' -f 2)
        suffix=$(echo $variant,$consumers,$service_time,$run)

        log_items_csv $file $suffix >> $resdir/base_items.csv
        log_summary_csv $dir/$file $suffix >> $resdir/base_summary.csv
    done
fi

# Parse Multi-Queue log
dir=logdir/multi
resdir=results/multi-queue
mkdir -p $resdir

echo ID,Latency,Waiters,SpinTime,AccessTime,Variant,Consumers,ServiceTime,Run > $resdir/multi_items.csv
echo TotalConsumed,TotalRunningTime,TotalSpinTime,TotalServiceTime,Variant,Consumers,ServiceTime,Run > $resdir/multi_summary.csv

if [ -d $dir ]; then
    echo "Parse Multi-Queue logs"
    for file in `ls $dir`; do
        echo "Parse $file"
        prefix=$(echo $file | cut -d '.' -f 1)
        variant=$(echo $prefix | cut -d '-' -f 1)
        consumers=$(echo $prefix | cut -d '-' -f 2)
        run=$(echo $prefix | cut -d '-' -f 3)
        service_time=$(cat $dir/$file | grep SERVICE_TIME | cut -d '=' -f 2)
        suffix=$(echo $variant,$consumers,$service_time,$run)

        log_items_csv $file $suffix >> $resdir/multi_items.csv
        log_summary_csv $dir/$file $suffix >> $resdir/multi_summary.csv
    done
fi


# Parse Batch-queue log
dir=logdir/batch
resdir=results/batch
mkdir -p $resdir

echo ID,Latency,Waiters,SpinTime,AccessTime,Variant,Consumers,ServiceTime,BatchSize,Run > $resdir/batch_items.csv
echo TotalConsumed,TotalRunningTime,TotalSpinTime,TotalServiceTime,Variant,Consumers,ServiceTime,BatchSize,Run > $resdir/batch_summary.csv

if [ -d $dir ]; then
    echo "Parse batch logs"
    for file in `ls $dir`; do
        echo "Parse $file"
        prefix=$(echo $file | cut -d '.' -f 1)
        variant=$(echo $prefix | cut -d '-' -f 1)
        consumers=$(echo $prefix | cut -d '-' -f 2)
        batch_size=$(echo $prefix | cut -d '-' -f 3)
        run=$(echo $prefix | cut -d '-' -f 4)
        service_time=$(cat $dir/$file | grep SERVICE_TIME | cut -d '=' -f 2)
        suffix=$(echo $variant,$consumers,$service_time,$batch_size,$run)

        log_items_csv $file $suffix >> $resdir/batch_items.csv
        log_summary_csv $dir/$file $suffix >> $resdir/batch_summary.csv
    done
fi