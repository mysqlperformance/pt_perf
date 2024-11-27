#! /bin/bash

record=0
save_log=0
clear_log=0

get_key_value()
{
  echo "$1" | sed 's/^-[a-zA-Z_-]*=//'
}

parse_options()
{
  while test $# -gt 0
  do
    case "$1" in
    -r=*)
      record=`get_key_value "$1"`;;
    -r)
      shift
      record=`get_key_value "$1"`;;
    -s=*)
      save_log=`get_key_value "$1"`;;
    -s)
      shift
      save_log=`get_key_value "$1"`;;
    -c=*)
      clear_log=`get_key_value "$1"`;;
    -c)
      shift
      clear_log=`get_key_value "$1"`;;
    *)
      echo "Unknown option '$1'"
      exit 1;;
    esac
    shift
  done
}
parse_options "$@"

# binary-test
## record-test
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% binary-test: record-test %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
if [ x"$clear_log" = x"0" ]; then
  cd binary-test/record_test
  sudo ./test.sh
  cd ../..
fi


# script-test
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% binary-test: script-test %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
cd binary-test/script_test
if [ x"$clear_log" = x"0" ]; then
  echo "sudo ./test.sh -r $record"
  sudo ./test.sh -r $record
  if [ x"$save_log" = x"0" ]; then
    rm -rf *.log
  fi
else
  rm -rf *.log
fi
cd ../..


## script-test
echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% mysql-test %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
cd mysql-test
if [ x"$clear_log" = x"0" ]; then
  echo "sudo ./test.sh -r $record"
  sudo ./test.sh -r $record
  if [ x"$save_log" = x"0" ]; then
    rm -rf *.log
  fi
else
  rm -rf *.log
fi
cd ..
