#include <sstream>
#include <unistd.h>
#include "pt_linux_perf.h"

using namespace std;
void perf_record(PerfOption &opt) {
  
  if (opt.history >= 2) {
    return;
  }

  if (opt.trace_time <= 0) {
    printf("Error: trace_time must be greater than 0\n ");
    exit(0);
  }

  string binary = opt.binary;
  stringstream cmd;

  cmd << opt.perf_tool << " record -e " << opt.intel_pt_config << " -B " << opt.record_filter << " ";

  if (opt.cpu != "") {
    cmd << " -C " << opt.cpu;
    printf("[ trace cpu %s for %.2f seconds ]\n", opt.cpu.c_str(), opt.trace_time);
  } else if (opt.tid != "") {
    cmd << " -t " << opt.tid;
    printf("[ trace thread %s for %.2f seconds ]\n", opt.tid.c_str(), opt.trace_time);
  } else if (opt.pid != -1) {
    cmd << " -p " << opt.pid;
    printf("[ trace process %ld for %.2f seconds ]\n", opt.pid, opt.trace_time);
  } else {
    printf("ERROR: use --cpu/--pid/--tid to set trace method\n");
    exit(0);
  }

  if (opt.per_thread_mode) {
    cmd << " --per-thread --timestamp ";
  }

  cmd << " -m,32M --no-bpf-event -- sleep " << opt.trace_time;

  if (opt.verbose) printf("%s\n", cmd.str().c_str());

  /* execute */
  auto t1 = ut_time_now();
  system(cmd.str().c_str());
  auto t2 = ut_time_now();
  printf("[ perf record has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));

  if (opt.history == 1) {
    printf("[ trace done, you can copy perf.data and the binary file \n"
           "  (with the same absolute path) to another machine for analysis. ]\n");
    exit(0);
  }
}


void perf_script(PerfOption &opt) {
  clear_script_files();

  stringstream cmd;
  cmd << opt.perf_tool << " script --ns --itrace=" << opt.itrace;

  if (opt.compact_format) {
    cmd << " " << "--compact_format=1";
  }
  cmd << " " << opt.fields << " " << opt.script_filter;
  if (opt.parallel_script) {
    cmd << " --parallel=" << opt.worker_num
        << " " << (opt.verbose ? "" : "&> /dev/null");
  } else {
    cmd << " > script_out";
  }
  if (opt.verbose) printf("%s\n", cmd.str().c_str());
  auto t1 = ut_time_now();
  exec_cmd_killable(cmd.str());
  auto t2 = ut_time_now();
  printf("[ perf script has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
}

void clear_record_files() {
  system("rm -f perf.data");
}

void clear_script_files() {
  system("rm -f script_out*");
}

void print_flame_graph(FlameGraphOption &opt) {
  stringstream cmd;
  char filename[128];

  if (opt.type == "latency") {
    /* latency flamegraph */
    cmd << opt.pt_flame_home << "/bin/pt_flame -j " << opt.worker_num;
    if (opt.parallel_script) {
      for (size_t i = 0; i < opt.worker_num; ++i) {
        snprintf(filename, 128, SCRIPT_FILE_PREFIX "__%05d", i);
        cmd << " " << filename;
      }
    } else {
      cmd << " " SCRIPT_FILE_PREFIX;
    }
    cmd << " | " << opt.scripts_home << "/flamegraph.pl"
        << " --countname=\"ns\" > flame.svg";
  } else if (opt.type == "cpu") {
    /* cpu flamegraph */
    cmd << "cat ";
    if (opt.parallel_script) {
      for (size_t i = 0; i < opt.worker_num; ++i) {
        snprintf(filename, 128, SCRIPT_FILE_PREFIX "__%05d", i);
        if (access(filename, F_OK) != -1) {
          cmd << " " << filename;
        }
      }
    } else {
      cmd << " " SCRIPT_FILE_PREFIX;
    }
    cmd << " | " << opt.scripts_home << "/stackcollapse-perf.pl"
        << " | " << opt.scripts_home << "/flamegraph.pl > flame.svg";
  } else {
    printf("ERROR: wrong flamegraph mode, use latency or cpu\n");
    return;
  }

  if (opt.verbose) printf("%s\n", cmd.str().c_str());
  auto t1 = ut_time_now();
  system(cmd.str().c_str());
  auto t2 = ut_time_now();
  printf("[ print flame graph has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
  printf("[ Flamegraph has been saved to flame.svg ]\n"); 
}

