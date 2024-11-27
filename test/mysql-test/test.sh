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

gen_case() {
  worker_num=$1

  mkdir -p test1/$worker_num
  cd test1/$worker_num
  for func in ${funcs[@]};
  do
    mkdir -p $func
    cd $func
    echo "generate case [$func]"
    $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -w $worker_num -p $mysql_pid -v > /dev/null
    rm -rf perf.data
    cd ..
  done
  cd ../../
}

test_case() {
  worker_num=$1
  cd test1/$worker_num
  for func in ${funcs[@]};
  do
    mkdir -p $func
    cd $func
    echo "test case [$func]"
    $func_latency -b "$dir/mysqld" -f "$func" -d 0.01 -s -t -w $worker_num --history=3
    cd ..
  done
  cd ../../
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
  rm -rf test1
  gen_case 5
  gen_case 10
  gen_case 32
else
  output="."
  if [ x"$record" = x"1" ]; then
    output="res/test1"
    mkdir -p $output
  fi
  test_case 5  | grep -v "has consumed" > $output/5.log
  test_case 10 | grep -v "has consumed" > $output/10.log
  test_case 32 | grep -v "has consumed" > $output/32.log
  if [ x"$record" = x"0" ]; then
    show_diff 5.log   res/test1/5.log
    show_diff 10.log  res/test1/10.log
    show_diff 32.log  res/test1/32.log
  fi
fi

