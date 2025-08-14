#! /bin/bash

# Some problems:
# when using pid mode and parallel script, there may exists some special cases:
# 1. some cyc packets are in last pt buffer of previous worker, so few branchs in
#    the first pt buffer of current worker may has lower precision within 20ns.
# 2. the first psb of current worker has jump instruction with relative ip, which
#    depends on the last pt buffer of previous worker.


# normal case: without offcpu, which not depends on the symbols of kernel

binary_path=/u01/pt_perf_test
record=0
gen_perf_case=0
gen_normal=0
gen_offcpu=0
extra_opt="--script_format='compact'"

get_key_value()
{
  echo "$1" | sed 's/^-[a-zA-Z_-]*=//'
}

parse_options()
{
  while test $# -gt 0
  do
    case "$1" in
    -g=*)
      gen_perf_case=`get_key_value "$1"`;;
    -g)
      shift
      gen_perf_case=`get_key_value "$1"`;;
    -n=*)
      gen_normal=`get_key_value "$1"`;;
    -n)
      shift
      gen_normal=`get_key_value "$1"`;;
    -o=*)
      gen_offcpu=`get_key_value "$1"`;;
    -o)
      shift
      gen_offcpu=`get_key_value "$1"`;;
    -r=*)
      record=`get_key_value "$1"`;;
    -r)
      shift
      record=`get_key_value "$1"`;;
    *)
      echo "Unknown option '$1'"
      exit 1;;
    esac
    shift
  done
}
parse_options "$@"

func_latency=../../../func_latency

gen_no_per_thread_case_normal() {
  rm -rf perf_data/no_per_thread.n.*
  pid=$1
  # test 1: pid mode, no-per-thread, no-ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate non-per-thread test 1"
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1       -p $pid --history=1
  mv perf.data perf_data/no_per_thread.n.1

  # test 2: pid mode, no-per-thread ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate no-per-thread test 2"
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1    -i -p $pid --history=1
  mv perf.data perf_data/no_per_thread.n.2
}

test_no_per_thread_case_normal() {
  worker_num_opt=$1
  parallel_script_opt=$2
  tid_opt=$3
  # test 1: no-ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% normal non-per-thread test 1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/no_per_thread.n.1 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2 
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2 
  echo $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2
       $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --history=2

  # latency_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --li=4194304,8388607 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --li=4194304,8388607 --history=2

  # time_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --ti=27231953741678534,34977000,57208000 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --ti=27231953741678534,34977000,57208000 --history=2

  # timeline
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -l --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -l --history=2

  # ancestor
  echo $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -a "do_cpu_work" --history=2
       $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -a "do_cpu_work" --history=2

  # flamegraph
  echo $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="cpu"  --history=2 > /dev/null 2>&1
       $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="cpu"  --history=2 > /dev/null 2>&1
  echo $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="latency"  --history=2 > /dev/null 2>&1
       $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="latency"  --history=2 > /dev/null 2>&1

  # test 2: ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% normal non-per-thread test 2 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/no_per_thread.n.2 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i --history=2
  #timeline
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -l --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -l --history=2
}

gen_per_thread_case_normal() {
  rm -rf perf_data/per_thread.n.*
  pid=$1
  
  # test 1: pid mode, per-thread, no-ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate per-thread test 1"
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1 -t    -p $pid --history=1
  mv perf.data perf_data/per_thread.n.1

  # test 2: pid mode, per-thread, ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate per-thread test 2"
  echo $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1 -t -i -p $pid --history=1
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1 -t -i -p $pid --history=1
  mv perf.data perf_data/per_thread.n.2
  $func_latency -b "$binary_path/binary" -f "do_cpu_work"     -d 0.1 -t -i -p $pid --history=1
  mv perf.data perf_data/per_thread.n.3
  $func_latency -b "$binary_path/binary" -f "do_io"           -d 0.1 -t -i -p $pid --history=1
  mv perf.data perf_data/per_thread.n.4


  # test 3: pid mode, per-thread ip-filtering, ancestor
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate per-thread test 3"
  $func_latency -b "$binary_path/binary" -f "std::to_string"  -d 0.1 -t  -i -p $pid --history=1 -a "do_cpu_work" -I "#1"
  mv perf.data perf_data/per_thread.n.5
}

test_per_thread_case_normal() {
  worker_num_opt=$1
  parallel_script_opt=$2
  tid_opt=$3

  # test 1: pid mode, per-thread, no-ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% normal per-thread test 1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/per_thread.n.1 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
  echo $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2

  # latency_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --li=4194304,8388607 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --li=4194304,8388607 --history=2

  # time_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --ti=27231946654937838,31990000,56377000 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --ti=27231946654937838,31990000,56377000 --history=2

  # timeline
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -l --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -l --history=2

  echo $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="cpu"  --history=2 > /dev/null 2>&1
       $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="cpu"  --history=2 > /dev/null 2>&1
  echo $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="latency"  --history=2 > /dev/null 2>&1
       $func_latency -b "$binary_path/binary" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt --flamegraph="latency"  --history=2 > /dev/null 2>&1
  
  # test 2: pid mode, per-thread, ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% normal per-thread test 2 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/per_thread.n.2 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
  cp ./perf_data/per_thread.n.3 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
  cp ./perf_data/per_thread.n.4 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2

  # latency_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --li=4194304,8388607 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --li=4194304,8388607 --history=2
       
  # time_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --ti=27231949280479124,31990000,56377000 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --ti=27231949280479124,31990000,56377000 --history=2

  #timeline
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i -l --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i -l --history=2

  # test 3: ip-filtering, ancestor
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% normal per_thread test 3 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/per_thread.n.5 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work" --history=2
       $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work" --history=2

  # ancestor
  echo $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work#4194304,8388608" --history=2
       $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work#4194304,8388608" --history=2
  #timeline
  echo $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work#4194304,8388608" -l --history=2
       $func_latency -b "$binary_path/binary" -f "std::to_string" -d 0.1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -i -a "do_cpu_work#4194304,8388608" -l --history=2
}

gen_per_thread_case_offcpu() {
  rm -rf perf_data/perf.data.o*
  pid=$1
  # test 1: pid mode, per-thread, no-ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate offcpu per_thread test 1"
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1 -t    -p $pid -o --history=1
  mv perf.data perf_data/per_thread.o.1

  # test 2: pid mode, per-thread, ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% geneate offcpu per_thread test 2"
  $func_latency -b "$binary_path/binary" -f "thread_func"     -d 0.1 -t -i -p $pid -o --history=1
  mv perf.data perf_data/per_thread.o.2
  $func_latency -b "$binary_path/binary" -f "do_cpu_work"     -d 0.1 -t -i -p $pid -o --history=1
  mv perf.data perf_data/per_thread.o.3
  $func_latency -b "$binary_path/binary" -f "do_io"           -d 0.1 -t -i -p $pid -o --history=1
  mv perf.data perf_data/per_thread.o.4
}

test_per_thread_case_offcpu() {
  worker_num_opt=$1
  parallel_script_opt=$2
  tid_opt=$3

  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% offcpu per_thread test 1 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/per_thread.o.1 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
  echo $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --history=2

  # latency_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --li=4194304,8388607 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t --li=4194304,8388607 --history=2

  # test 3: pid mode, per-thread, ip-filtering
  echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% offcpu per_thread test 2 $parallel_script_opt $worker_num_opt $tid_opt $extra_opt %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
  cp ./perf_data/per_thread.o.2 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "thread_func"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
  cp ./perf_data/per_thread.o.3 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "do_cpu_work"    -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
  cp ./perf_data/per_thread.o.4 ./perf.data
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --history=2

  # latency_interval
  echo $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --li=4194304,8388607 --history=2
       $func_latency -b "$binary_path/binary" -f "do_io"          -d 0.1 -o $parallel_script_opt $worker_num_opt $tid_opt $extra_opt -t -i --li=4194304,8388607 --history=2
}

show_diff() {
  echo "show_diff $1 $2"
  cat -n $1 | grep -v "$func_latency -b" | grep -v "%%%%%%" > file1.txt
  cat -n $2 | grep -v "$func_latency -b" | grep -v "%%%%%%" > file2.txt
  diff file1.txt file2.txt
  rm -rf file1.txt
  rm -rf file2.txt
}

test_normal_case() 
{
  test_per_thread_case_normal ""       ""    ""         | grep -v "has consumed" | grep -v "parallel workers" > p1.log

  if [ x"$record" = x"1" ]; then
    cp p1.log ./res/per_thread/
  else
    show_diff p1.log ./res/per_thread/p1.log
  fi
  test_per_thread_case_normal ""       "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > p2.log
  show_diff p1.log p2.log
  test_per_thread_case_normal "-w 4"   "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > p3.log
  show_diff p1.log p3.log
  test_per_thread_case_normal "-w 32"  "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > p4.log
  show_diff p1.log p4.log
  test_per_thread_case_normal "-w 128" "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > p5.log
  show_diff p1.log p5.log

  test_per_thread_case_normal ""       ""   "-t 80565" | grep -v "has consumed" | grep -v "parallel workers" > p6.log
  test_per_thread_case_normal "-w 128" "-s" "-t 80565" | grep -v "has consumed" | grep -v "parallel workers" > p7.log
  show_diff p6.log p7.log

  echo "normal per_thread case is completed !!!!"

  output="."
  if [ x"$record" = x"1" ]; then
    output="./res/no_per_thread"
  fi
  test_no_per_thread_case_normal ""       ""    ""         | grep -v "has consumed" | grep -v "parallel workers" > $output/np1.log
  test_no_per_thread_case_normal ""       "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > $output/np2.log
  test_no_per_thread_case_normal "-w 4"   "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > $output/np3.log
  test_no_per_thread_case_normal "-w 32"  "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > $output/np4.log
  test_no_per_thread_case_normal "-w 128" "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > $output/np5.log

  if [ x"$record" = x"0" ]; then
    show_diff np1.log ./res/no_per_thread/np1.log
    show_diff np2.log ./res/no_per_thread/np2.log
    show_diff np3.log ./res/no_per_thread/np3.log
    show_diff np4.log ./res/no_per_thread/np4.log
    show_diff np5.log ./res/no_per_thread/np5.log
  fi
  echo "normal no_per_thread case is completed !!!!"
}

test_offcpu_case() {
  test_per_thread_case_offcpu ""       ""    ""         | grep -v "has consumed" | grep -v "parallel workers" > p8.log
  if [ x"$record" = x"1" ]; then
    cp p8.log ./res/per_thread/
  else
    show_diff p8.log ./res/per_thread/p8.log
  fi
  test_per_thread_case_offcpu ""       "-s"  ""         | grep -v "has consumed" | grep -v "parallel workers" > p9.log
  show_diff p8.log p9.log

  echo "offcpu test case is completed !!!!"
}

mkdir -p $binary_path

if [ x"$gen_perf_case" = x"0" ]; then
  cp ./binary $binary_path/
  
  test_normal_case

  test_offcpu_case

  rm -rf perf.data
  rm -rf flame.svg
else
  if [ x"$gen_normal" = x"1" ] &&  [ x"$gen_offcpu" = x"1" ]; then
    # re-compile
    cp ../record_test/binary.cc ./
    g++ binary.cc -g -o binary --std=c++11 -lpthread -O0
    rm -rf binary.cc
  fi
  cp ./binary $binary_path/

  $binary_path/binary &
  sleep 1
  pid=$(< pid.file)

  if [ x"$gen_normal" = x"1" ]; then
    gen_per_thread_case_normal $pid
    gen_no_per_thread_case_normal $pid
  fi
  if [ x"$gen_offcpu" = x"1" ]; then
    gen_per_thread_case_offcpu $pid
  fi

  kill -SIGINT $pid
fi
rm -rf $binary_path

