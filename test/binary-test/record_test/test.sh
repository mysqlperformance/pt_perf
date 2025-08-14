#! /bin/bash

func_latency=../../../func_latency

g++ binary.cc -g -o binary --std=c++11 -lpthread -O0

./binary &

sleep 1

pid=$(< pid.file)

# test 1: pid mode, no-per-thread, offcpu, no-ip-filtering
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 1 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -o --history=1 -p $pid
     $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -o --history=1 -p $pid

# test 2: pid mode, per-thread, no-offcpu, no-ip-filtering
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 2 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t --history=1 -p $pid
     $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t --history=1 -p $pid

# test 3: pid mode, per-thread, offcpu, no-ip-filtering
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 3 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t -o --history=1 -p $pid
     $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t -o --history=1 -p $pid

# test 4: pid mode, per-thread, ip-filtering
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 4 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t -o -i --history=1 -p $pid
     $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -t -o -i --history=1 -p $pid

# test 5: pid mode, ip-filtering
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 5 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -o -i --history=1 -p $pid
     $func_latency -b "./binary" -f "thread_func" -d 0.1 -s -o -i --history=1 -p $pid

# test 6: pid mode, ip-filtering, ancestor
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 6 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "std::to_string" -d 0.1 -s -i -a "do_cpu_work" --history=1 -I "#1" -p $pid
     $func_latency -b "./binary" -f "std::to_string" -d 0.1 -s -i -a "do_cpu_work" --history=1 -I "#1" -p $pid

## test 7: trace by CPU
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 7 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
echo $func_latency -b "./binary" -f "thread_func" -d 1 -C 0-1 -t --history=1 
     $func_latency -b "./binary" -f "thread_func" -d 1 -C 0-1 -t --history=1 
echo $func_latency -b "./binary" -f "thread_func" -o -d 1 -C 0-1 -t --history=1 
     $func_latency -b "./binary" -f "thread_func" -o -d 1 -C 0-1 -t --history=1 

## test 8: trace by tid
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% test 8 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
tid_begin=`ps -T -p $pid | grep "thread_0" | awk '{print $2}'`
tid_end=$(($tid_begin+2))
echo $func_latency -b "./binary" -f "thread_func" -d 1 -T $tid_begin-$tid_end -t --history=1 
     $func_latency -b "./binary" -f "thread_func" -d 1 -T $tid_begin-$tid_end -t --history=1 

kill -SIGINT $pid

rm -rf perf.data*
rm -rf binary
