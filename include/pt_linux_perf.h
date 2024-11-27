#ifndef _h_pt_linux_perf_
#define _h_pt_linux_perf_

#include <string>
#include "sys_tools.h"

#define SCRIPT_FILE_PREFIX "script_out"

struct PerfOption {
  std::string binary;
  std::string cpu;
  std::string tid;
  long pid;
  float trace_time;
  bool per_thread_mode;

  std::string perf_tool;
  std::string record_filter;
  std::string intel_pt_config;

  bool parallel_script;
  std::string script_filter;
  std::string fields;
  std::string itrace;
  size_t worker_num;
  bool compact_format;

  int history;
  bool verbose;
};

void perf_record(PerfOption &opt);
void perf_script(PerfOption &opt);

void clear_record_files();
void clear_script_files();

struct FlameGraphOption {
  std::string type;
  std::string scripts_home;
  std::string pt_flame_home;

  bool parallel_script;
  size_t worker_num;

  bool verbose;
};

void print_flame_graph(FlameGraphOption &opt);

#endif
