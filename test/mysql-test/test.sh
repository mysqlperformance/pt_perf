#! /bin/bash

record=0
gen_perf_case=0
mysql_pid=-1
dir=`pwd`

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
    -r=*)
      record=`get_key_value "$1"`;;
    -r)
      shift
      record=`get_key_value "$1"`;;
    -p=*)
      mysql_pid=`get_key_value "$1"`;;
    -p)
      shift
      mysql_pid=`get_key_value "$1"`;;
    *)
      echo "Unknown option '$1'"
      exit 1;;
    esac
    shift
  done
}
parse_options "$@"

func_latency=$dir/../../func_latency

funcs="
do_command
dispatch_command
mysqld_stmt_execute
mysql_execute_command

open_tables_for_query
Sql_cmd_dml::execute
row_search_mvcc
rec_get_offsets_func
row_sel_store_mysql_rec
Optimize_table_order::choose_table_order

handler::ha_update_row
ha_innobase::update_row
row_upd_step
row_upd_clust_step
row_upd_clust_rec
btr_cur_optimistic_update
btr_cur_update_in_place

log_writer
os_file_write_page
"

funcs2="
do_command
btr_cur_optimistic_update
log_writer
"

# case 1: simple case with multiple worker test
gen_case() {
  worker_num=$1

  mkdir -p test1/$worker_num
  cd test1/$worker_num
  for func in ${funcs[@]};
  do
    echo "%%%%%%%%%%%%% generate text case [$func]"
    mkdir -p text/$func
    cd text/$func
    # check perf.data twice, otherwise the decode maybe different for kernel symbols
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="text" -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="text" -v > /dev/null
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="text" --history=2 -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="text" --history=2 -v > /dev/null
    mv perf.data ../../
    cd ../../
    echo "%%%%%%%%%%%%% generate compact case [$func]"
    mkdir -p compact/$func
    mv perf.data compact/$func/
    cd compact/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="compact" --history=2 -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num -p $mysql_pid --script_format="compact" --history=2 -v > /dev/null
    rm -rf perf.data
    cd ../../
  done
  cd ../../
}

test_text_case() {
  worker_num=$1
  cd test1/$worker_num
  for func in ${funcs[@]};
  do
    echo "%%%%%%%%%%%%% test text case [$func]"
    mkdir -p text/$func
    cd text/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num --script_format="text" --history=3
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num --script_format="text" --history=3
    cd ../../
  done
  cd ../../
}

test_compact_case() {
  worker_num=$1
  cd test1/$worker_num
  for func in ${funcs[@]};
  do
    echo "%%%%%%%%%%%%% test compact case [$func]"
    mkdir -p compact/$func
    cd compact/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num --script_format="compact" --history=3
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -o -w $worker_num --script_format="compact" --history=3
    cd ../../
  done
  cd ../../
}

# case2: code_block and has many trace errors (32 threads)
gen_case2() {
  mkdir -p test2/
  cd test2
  for func in ${funcs2[@]};
  do
    echo "%%%%%%%%%%%%% generate text case [$func]"
    mkdir -p text/$func
    cd text/$func
    # check perf.data twice, otherwise the decode maybe different for kernel symbols
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="text" -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="text" -v > /dev/null
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="text" --history=2 -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="text" --history=2 -v > /dev/null
    mv perf.data ../../
    cd ../../
    echo "%%%%%%%%%%%%% generate compact case [$func]"
    mkdir -p compact/$func
    mv perf.data compact/$func/
    cd compact/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="compact" --history=2 -v > /dev/null
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -p $mysql_pid -c 1 --script_format="compact" --history=2 -v > /dev/null
    rm -rf perf.data
    cd ../../
  done
  cd ../
}

test_text_case2() {
  cd test2
  for func in ${funcs2[@]};
  do
    echo "%%%%%%%%%%%%% test text case [$func]"
    mkdir -p text/$func
    cd text/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -c 1 --script_format="text" --history=3
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -c 1 --script_format="text" --history=3
    cd ../../
  done
  cd ../
}

test_compact_case2() {
  cd test2
  for func in ${funcs2[@]};
  do
    echo "%%%%%%%%%%%%% test compact case [$func]"
    mkdir -p compact/$func
    cd compact/$func
    echo $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -c 1 --script_format="compact" --history=3
         $func_latency -b "$dir/mysqld" -f "$func" -d 0.1 -s -t -o -c 1 --script_format="compact" --history=3
    cd ../../
  done
  cd ../
}

show_diff() {
  echo "show_diff $1 $2"
  cat -n $1 | grep -v "$func_latency -b" | grep -v "%%%%%%" > file1.txt
  cat -n $2 | grep -v "$func_latency -b" | grep -v "%%%%%%" > file2.txt
  diff file1.txt file2.txt
  rm -rf file1.txt
  rm -rf file2.txt
}

if [ x"$gen_perf_case" = x"1" ]; then
  # test 1
  rm -rf test1
  gen_case 5
  gen_case 10
  gen_case 32
  # test 2
  rm -rf test2
  gen_case2
else
  # test 1
  echo "%%%%%%%%%%%%%% test 1"
  output="."
  if [ x"$record" = x"1" ]; then
    output="res/test1"
    mkdir -p $output
  fi
  test_text_case 5  | grep -v "has consumed" > $output/5.log
  test_text_case 10 | grep -v "has consumed" > $output/10.log
  test_text_case 32 | grep -v "has consumed" > $output/32.log
  if [ x"$record" = x"0" ]; then
    test_compact_case 5  | grep -v "has consumed" > c5.log
    test_compact_case 10 | grep -v "has consumed" > c10.log
    test_compact_case 32 | grep -v "has consumed" > c32.log
    show_diff 5.log   res/test1/5.log
    show_diff 5.log   c5.log
    show_diff 10.log  res/test1/10.log
    show_diff 10.log   c10.log
    show_diff 32.log  res/test1/32.log
    show_diff 32.log   c32.log
  fi
  # test 2
  echo "%%%%%%%%%%%%%% test 2"
  output="."
  if [ x"$record" = x"1" ]; then
    output="res/test2"
    mkdir -p $output
  fi
  test_text_case2 | grep -v "has consumed" > $output/10.log
  if [ x"$record" = x"0" ]; then
    test_compact_case2 | grep -v "has consumed" > c10.log
    show_diff 10.log res/test2/10.log
    show_diff 10.log c10.log
  fi
fi


