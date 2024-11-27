#! /bin/bash

git clone https://github.com/RongBiaoXie/pt_perf_test_data.git

cp ./pt_perf_test_data/binary-test/script_test/perf_data/* binary-test/script_test/perf_data/

cp ./pt_perf_test_data/mysql-test/mysqld.bz2 mysql-test/
cp ./pt_perf_test_data/mysql-test/test1.tar.gz mysql-test/

cd mysql-test
bunzip2 mysqld.bz2
tar -zxf test1.tar.gz
