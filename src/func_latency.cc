#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <assert.h>

#include "stat_tools.h"

using namespace std;

string perf_tool = "perf";
string binary_file_path = "";
string func_name = "";
string caller_func_name = "";
float trace_time = 0.01;
long pid = -1;

static void usage() {
  printf(
    "Intel processor tool - func_latency\n"
    "usage ./func_latency [-b bin/mysqld] [-f func] [-p parent_func] [-d 0.01]\n"
    "Linux version 5.10+ is required\n"
    "\t-h                   --- show this help\n"
    "\t-b                   --- binary file path\n"
    "\t-f                   --- target's func name\n"
    "\t-c                   --- caller's func name, [not implement now]\n"
    "\t-d                   --- trace time (seconds)\n"
    "\t-p                   --- process's pid\n"
    "\t-x                   --- perf tool path, 'perf' by default\n"
  );
}

static bool check_system() {
  // first check if intel pt is supported
  FILE *file = fopen("/sys/devices/intel_pt", "r");
  if (file == nullptr) {
    printf("intel pt is not support by your cpu architecture\n");
    return -1;
  }
  fclose(file);

  file = fopen("/sys/devices/intel_pt/caps/ip_filtering", "r");
  if (file == nullptr) {
    printf("ip_filtering is not support by your cpu architecture\n");
    return -1;
  }
  return 0;
}

void dump_options() {
  printf("binary file path: %s\n", binary_file_path.c_str());
  printf("func_name: %s\n", func_name.c_str());
  printf("caller_func_name: %s\n", caller_func_name.c_str());
  printf("trace_time: %f\n", trace_time);
  printf("pid: %d\n", pid);
}

struct Symbol {
  string name;
  uint offset;
};

struct Action {
  enum ActionType {
    TR_START,
    TR_END,
    TR_END_CALL,
    TR_END_RETURN
  };
  static Symbol get_symbol(const string &str) {
    if (str.find("[unknown]") != string::npos) {
      return {"[unknown]", 0};
    }
    size_t start = str.find_first_of('+');
    size_t end = str.find_first_of('(');
    uint offset = stol(str.substr(start+1, end-start), nullptr, 16);
    
    end = start;
    while (str[--start] != ' ');
    return {str.substr(start + 1, end - start - 1), offset};
  }

  Symbol from;
  Symbol to;
  uint64_t ts; // timestamp for nanosecond
  enum ActionType type;
  long tid;
  long cpu;
  long id;
};

class Thread {
public:
  std::vector<Action> actions;
};

class FuncSet {
public:
  unordered_map<string, uint64_t> funcs;
};

unordered_map<long, Thread> threads_map;

void parse_trace() {
  Histogram global_hist;
  Histogram global_child_hist;

  unordered_map<string, Histogram> caller_child_hist;
  unordered_map<string, Histogram> caller_hists;

  ifstream ifs("cr.trace");
  if (ifs.is_open()) {
    string line;
    long id = 0;
    while(getline(ifs, line)) {
      Action action;
      FuncSet childs;

      action.id = ++id;
      /* action thread */
      size_t start = 0;
      size_t end = line.find_first_of('[');
      long tid = stol(line.substr(start, end - start));
      action.tid = tid;

      /* action cpu */
      start = end + 1;
      end = line.find_first_of(']');
      action.cpu = stol(line.substr(start, end - start));

      /* action timestamp */
      start = end + 1;
      end = line.find_first_of('.');
      long sec = stol(line.substr(start, end - start));
      
      start = end + 1;
      end = line.find_first_of(':');
      long nsec = stol(line.substr(start, end - start));
      action.ts = sec*1000UL*1000UL*1000UL + nsec;
      
      /* action function name */
      size_t split = line.find_first_of("=>");
      string part1 = line.substr(0, split);
      string part2 = line.substr(split, line.size() - split);
      action.from = Action::get_symbol(part1);
      action.to = Action::get_symbol(part2);

      /* action type */
      auto &actions = threads_map[tid].actions;
      if (line.find("tr strt") != string::npos) {
        action.type = Action::TR_START;
        if (action.to.offset == 0) {
          /* this is the start point for target function, 
           * clear stack to avoid there exists incompleted traces */
          actions.clear();
          childs.funcs.clear();
          actions.push_back(action);
        } else if (!actions.empty() && actions.back().type == Action::TR_END_CALL) {
          // this is return to target function from sub function
          string &child_name = actions.back().to.name;
          childs.funcs[child_name] += (action.ts - actions.back().ts);
          actions.pop_back();
        } else if (!actions.empty() && actions.back().type == Action::TR_END) {
          actions.pop_back();
        } else {
          printf("impletement trace error is detected in line %d of cr.trace\n", action.id);
        }
      } else if (line.find("tr end  call") != string::npos) {
        // this is call to sub function
        action.type = Action::TR_END_CALL;
        actions.push_back(action);
      } else if (line.find("tr end  return") != string::npos) {
        action.type = Action::TR_END_RETURN;
        if (actions.size() == 1) {
          // the execution chain is not lost
          uint64_t during = action.ts - actions[0].ts;
          global_hist.add_val(during);
          caller_hists[action.to.name].add_val(during);
        }
        // one complete target function is traced
        actions.clear();
      } else if (line.find("tr end") != string::npos) {
        action.type = Action::TR_END;
        actions.push_back(action);
      } else {
        printf("error action type for line: %s\n", line.c_str());
      }
    }

    /* print summary */
    global_hist.set_name("[" + func_name + "]");
    global_hist.init_slot();
    global_hist.print_log2("ns");


    global_child_hist.set_name("Latency of [" + func_name + "]'s child function");


    for (auto it = caller_hists.begin(); it != caller_hists.end(); it++) {
      auto &hist = it->second;
      hist.set_name("[" + func_name + "] called from [" + it->first + "]");
      hist.init_slot();
      hist.print_log2("ns");
    }
  } 
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    usage();
    exit(-1);
  }
  
  if (check_system()) exit(-1); 
  
  char c;
  while (-1 != (c = getopt(argc, argv, "h:b:f:c:d:p:x:"))) {
    switch (c) {
      case 'b':
        printf("%s\n", optarg);
        binary_file_path = string(optarg);
        break;
      case 'f':
        func_name = string(optarg);
        break;
      case 'd':
        trace_time = atof(optarg);
        break;
      case 'p':
        pid = atol(optarg);
        break;
      case 'x':
        perf_tool = string(optarg);
        break;
      case 'c':
      case 'h':
      default:
        usage();
        exit(0);
    }
  }
  dump_options();

  /* trace and decode */
  char filter[1024];
  snprintf(filter, 1024, "--filter 'filter %s @ %s'", 
           func_name.c_str(), binary_file_path.c_str());

  // perf record
  char perf_cmd[1024];
  snprintf(perf_cmd, 1024, "%s record -e intel_pt//u %s -p %d -m 128M -- sleep %f", perf_tool.c_str(), filter, pid, trace_time);
  printf("%s\n", perf_cmd);
  system(perf_cmd);

  // perf script
  snprintf(perf_cmd, 1024, "%s script --itrace=cr --ns -F-event,-period,+addr,-comm,+flags > cr.trace", perf_tool.c_str());
  printf("%s\n", perf_cmd);
  system(perf_cmd);

  /* parse and print summary */
  parse_trace();

  /* clear resource */
  //remove("cr.trace");
  remove("perf.data");
}
