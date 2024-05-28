#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <thread>
#include <getopt.h>
#include <cmath>
#include <sstream>

#include "graphs.h"
#include "stat_tools.h"
#include "sys_tools.h"
#include "func_latency.h"

using namespace std;

static ParallelWorkerPool worker_pool;
static string trace_error_str = " instruction trace error";
/* the missing trace time because of the lost data  */
static atomic<uint64_t> miss_trace_time{0};
/* the real trace time based on minimum and maximum timestamp of actions */
static std::pair<uint64_t, uint64_t> real_trace_time = {UINT64_MAX, 0};
static atomic<uint64_t> ancestor_begin_count{0};
static atomic<uint64_t> ancestor_end_count{0};
/* use to decode source file and line number */
static SrclineMap srcline_map;
Param param;

#define CODE_BLOCK_PREFIX  "*code block: "
#define TARGET_SELF "*self"

Param::Param() {
  perf_tool = get_current_dir() + "/perf";
  perf_dlfilter = get_current_dir() + "/perf_dlfilter.so";
  binary = "";
  target = "";
  trace_time = 0.01;
  pid = -1;
  tid = "";
  cpu = "";
  verbose = false;
  parallel_script = false;
  per_thread_mode = false;
  worker_num = 10;
  ip_filtering = false;
  func_idx = "#0"; // global symbol
  offcpu = false;
  srcline = true;
  call_line = true;

  ancestor = "";
  code_block = false;

  timeline = false;
  timeline_unit = 1;
  sched_funcname = "__schedule";
  offcpu_filter = "filter " + sched_funcname +" ,";
  latency_interval = {0, UINT64_MAX};
  time_interval = {0, UINT64_MAX};
  time_start = UINT64_MAX;
  history = 0;

  flamegraph = "";
  scripts_home = get_current_dir() + "/scripts";
  pt_flame_home = "/usr/share/pt_flame";
}

struct option opts[] = {
  {"binary", 1, NULL, 'b'},
  {"func", 1, NULL, 'f'},
  {"duration", 1, NULL, 'd'},
  {"pid", 1, NULL, 'p'},
  {"tid", 1, NULL, 'T'},
  {"cpu", 1, NULL, 'C'},
  {"perf", 1, NULL, 'P'},
  {"worker_num", 1, NULL, 'w'},
  {"func_idx", 1, NULL, 'I'},
  {"timeline", 0, NULL, 'l'},
  {"flamegraph", 1, NULL, 'F'},
  {"pt_flame", 1, NULL, '4'},
  {"latency_interval", 1, NULL, '0'},
  {"li", 1, NULL, '0'},
  {"time_interval", 1, NULL, '1'},
  {"ti", 1, NULL, '1'},
  {"timeline_unit", 1, NULL, '3'},
  {"tu", 1, NULL, '3'},
  {"history", 1, NULL, '2'},
  {"ancestor", 1, NULL, 'a'},
  {"code_block", 0, NULL, 'c'},
  {"offcpu", 0, NULL, 'o'},
  {"srcline", 0, NULL, '5'},
  {"per_thread", 0, NULL, 't'},
  {"ip_filter", 0, NULL, 'i'},
  {"parallel_script", 0, NULL, 's'},
  {"verbose", 0, NULL, 'v'},
  {"help", 0, NULL, 'h'},
  {NULL, 0, NULL, 0}
};
const char *opt_str = "hvsitolcb:f:d:p:T:C:P:a:w:I:F:";

static void usage() {
  printf(
    "Intel processor tool - func_latency\n"
    "usage ./func_latency [-b bin/mysqld] [-f func] [-p pid] [-d trace_time] [-P perf_tool] [-s] [-i]\n"
    "Linux version 4.2+ is required for Intel PT\n"
    "Linux version 5.10+ is required for IP filtering when tracing\n"
    "\t-b / --binary          --- binary file path, empty for kernel func\n"
    "\t-f / --func            --- target's function name\n"
    "\t-d / --duration        --- trace time (seconds), 0.01 seconds by default\n"
    "\t-p / --pid             --- existing process ID\n"
    "\t-T / --tid             --- existing thread ID (comma separated list), example like tid1,tid2\n"
    "\t-C / --cpu             --- cpu list to trace, example like 0-47\n"
    "\t-w / --worker_num      --- parallel worker num, 10 by default\n"
    "\t-s / --parallel_script --- if use parallel script\n"
    "\t-t / --per_thread      --- use per_thread mode to trace data, better in multi-cores\n"
    "\t-o / --offcpu          --- trace offcpu time at the same time, which requires root privilege\n"
    "\t-i / --ip_filter       --- use ip_filter when tracing function\n"
    "\t-I / --func_idx        --- for ip_filter, choose function index if there exists multiple one, '#0' by default\n"
    "\t-P / --perf            --- perf tool path, 'perf' by default\n"
    "\t-a / --ancestor        --- only analyze target function with 'ancestor' function in its call chain\n"
    "\t-c / --code_block      --- show the code block latency of target function\n"
    "\t     --srcline         --- instead of the call line, show the define line of functions\n"
    "\t     --history         --- for history trace, 1: generate perf.data, 2: use perf.data \n"
    "\t--li/--latency_interval--- show the trace between the latency interval (ns), format: \"min,max\" \n"
    "\t-v / --verbose         --- verbose, be more verbose (show debug message, etc)\n"
    "\t-h / --help            --- show this help\n"
    "\n"
    "Timeline mode:\n"
    "\t-l / --timeline        --- show the target's func's latency by timeline for each thread\n"
    "\t--ti/--time_interval   --- show the trace between the time interval (ns), format:\"start,min,max\"\n"
    "\t--tu/--timeline_unit   --- the unit size in the timeline grapth, we caculate the average\n" 
    "\t                           latency in the unit, 1 by default\n"
    "\n"
    "Flamegraph mode:\n"
    "\t-F / --flamegraph      --- show the flamegraph, \"latency, cpu\"\n"
    "\t     --pt_flame        --- the installed path of pt_flame, latency-based flamegraph required\n"
    "\n"
    "Example: ./func_latency -b \"bin/mysqld\" -f \"do_command\" -d 1 -p 60467 -s -t -i\n"
    "         sudo ./func_latency -b \"bin/mysqld\" -f \"do_command\" -d 1 -p 60467 -s -t -i -o\n"
  );
}

static void clear_record_files() {
  system("rm -f perf.data");
}

static void clear_script_files() {
  system("rm -f script_out*");
}
static void dump_options() {
  printf("binary file path: %s\n", param.binary.c_str());
  printf("target: %s\n", param.target.c_str());
  printf("trace duration: %f\n", param.trace_time);
  printf("pid: %d\n", param.pid);
  printf("tid: %s\n", param.tid.c_str());
  printf("time_interval: %llu - %llu\n",
      param.time_interval.first, param.time_interval.second);
  printf("latency_interval: %llu - %llu\n",
      param.latency_interval.first, param.latency_interval.second);
}

bool SrclineMap::get(const std::string &function, std::string &srcline) {
  srcline = "-";
  shared_lock<shared_mutex> rlock(srcline_mutex);
  auto it = srcline_map.find(function);
  if (it != srcline_map.end()) {
    srcline = it->second;
    return true;
  }
  return false;
}

void SrclineMap::process_address() {
  unique_lock<shared_mutex> wlock(srcline_mutex);
  vector<string> func_vec;
  vector<uint64_t> addr_vec;
  vector<string> filename_vec;
  vector<uint> line_nr_vec;
  for (auto it = addr_map.begin(); it != addr_map.end(); ++it) {
    func_vec.push_back(it->first);
    addr_vec.push_back(it->second);
  }
  addr2line(param.binary, addr_vec, filename_vec, line_nr_vec);
  if (filename_vec.size() != addr_vec.size()) {
    printf("ERROR: addr2line failed !!!");
    return;
  }
  for (size_t i = 0; i < func_vec.size(); ++i) {
    string &name = func_vec[i];
    srcline_map[name] = filename_vec[i] + ":" + to_string(line_nr_vec[i]);
  }
}

uint64_t SrclineMap::get(const std::string &function) {
  shared_lock<shared_mutex> rlock(srcline_mutex);
  auto it = addr_map.find(function);
  if (it != addr_map.end()) {
    return it->second;
  }
  return 0;
}

void SrclineMap::put(const std::string &function, uint64_t addr) {
  unique_lock<shared_mutex> wlock(srcline_mutex);
  auto it = addr_map.find(function);
  if (it != addr_map.end()) {
    return;
  } else {
    addr_map[function] = addr;
  }
}

Symbol Action::get_symbol(const string &str, Symbol *caller) {
  if (str.find("[unknown]") != string::npos) {
    return {"[unknown]", 0, 0};
  }
  
  std::string name = "";
  uint64_t addr;
  uint offset;
  size_t start, end;

  // address
  end = str.find_first_of(' ');
  addr = stoull(str.substr(0, end), nullptr, 16);

  // name
  start = end + 1;
  end = str.find_first_of('+', start);
  name = str.substr(start, end - start);

  // offset
  start = str.find_first_of('+');
  end = str.find_first_of(' ', start);
  if (end == string::npos) end = str.size() - 1;
  offset = stol(str.substr(start + 1, end-start+1), nullptr, 16);
  
  if (param.srcline && static_cast<int64_t>(addr) > 0) {
    uint64_t func_addr = addr-offset;
    if (name != param.target && name != param.ancestor) {
      if (param.call_line) {
        if (caller) {
          // show the source line of the call address
          func_addr = caller->addr;
        } else {
          // 'from' symbol has no call address
          return {name, addr, offset};
        }
      }
      stringstream ss;
      ss << "|" << std::hex << func_addr << std::dec;
      name += ss.str();
    }
    // only for user symbol
    if(!srcline_map.get(name)) {
      srcline_map.put(name, func_addr);
    }
  }
  return {name, addr, offset};
}

static void report_error_action(const string &type, uint32_t id, uint32_t lnum) {
  if (param.verbose)
    printf("impletement %s error is detected in action of "
           "file %d, line %d\n", type.c_str(), id, lnum);
}

void ThreadJob::do_analyze() {
  vector<Action> stack;
  Stat::LatencyChild child;

  // for schedule latency
  uint64_t sched_in_target = 0;
  uint64_t sched_in_child = 0;
  Action *sched_begin = nullptr;
  stat.sched_count = 0;

 // to obtain miss trace time
  Action *target_begin = nullptr;
  bool prev_target_error = false;

  // for ancestor filter
  bool check_ancestor = (param.ancestor != "");
  Action *ancestor_begin = nullptr;

  Action *cursor = nullptr;

  auto clear_context = [&]() {
    stack.clear();
    child.clear();
    sched_in_child = sched_in_target = 0;
    sched_begin = nullptr;
    ancestor_begin = nullptr;
  };

  auto add_code_block = [&](Action *a1, Action *a2) {
    if (a2->from.offset == a1->to.offset) return;
    std::string child_name = CODE_BLOCK_PREFIX + to_string(a1->to.offset) + "-"
                   + to_string(a2->from.offset);
    uint64_t lat_t = a2->ts - a1->ts;
    uint64_t lat_s = sched_in_child;
    child.add_target(child_name, lat_t);
    child.add_sched(child_name, lat_s);
    // set to obtain srcline of code block
    if(!srcline_map.get(child_name + "_from"))
      srcline_map.put(child_name + "_from", a1->to.addr);
    if(!srcline_map.get(child_name + "_to"))
      srcline_map.put(child_name + "_to", a2->from.addr);
    assert(lat_t >= lat_s);
    sched_in_child = 0;
    cursor = a2;
  };

  for (size_t i = 0; i < actions.size(); ++i) {
    Action &action = actions[i];

    if (action.is_error) {
      /* encounter trace error, discard current execution chain */
      clear_context();
      prev_target_error = true;
      continue;
    }

    if (check_ancestor) {
      if (action.ancestor_begin) {
        // mark that ancestor has began
        clear_context();
        ancestor_begin = &action;
        ancestor_begin_count.fetch_add(1);
        continue;
      } else if (action.ancestor_end) {
        ancestor_begin = nullptr;
        ancestor_end_count.fetch_add(1);
        continue;
      } else if (!ancestor_begin) {
        // we only add target function within the ancestor
        continue;
      }
    }

    if (action.to.offset == 0 && action.to_target) {
        /* this action is the start point for target function, 
         * clear stack to avoid there exists incompleted traces */
       stack.clear();
       stack.push_back(action);
       if (!child.empty()) {
          /* there are not return action in last execution chain,
           * we just add the child latency information */
          stat.add_child_latency(child, "unknown");
          child.clear();
       }
       sched_in_target = sched_in_child = 0;
       if (sched_begin) {
         /* ERROR: the schedule is not finished when the target is called */
         report_error_action("schedule begin", sched_begin->id, sched_begin->lnum);
         sched_begin = nullptr;
       }
       if (prev_target_error) {
         /* calculate the trace missing time */
         if (target_begin)
           miss_trace_time.fetch_add(action.ts - target_begin->ts);
         prev_target_error = false;
       }
       target_begin = &action;
       cursor = &action;
       continue;
    }

    if (action.sched_begin) {
      sched_begin = &action;
      continue;
    }

    if (action.sched_end) {
      if (sched_begin) {
        uint32_t sched_time = action.ts - sched_begin->ts;
        sched_in_target += sched_time;
        sched_in_child += sched_time;
        ++stat.sched_count;
        sched_begin = nullptr;
      } else {
        /* ERROR: the schedule has not started */
        report_error_action("schedule end", action.id, action.lnum);
      }
      continue;
    }

    if ((action.type == Action::JMP || action.type == Action::JCC)) {
      if (action.from_target && action.to_target) {
        // internal jump, just add code block latency
        assert(param.code_block);
        add_code_block(cursor, &action);
        continue;
      } else if (action.from_target) {
        // change jmp instruction to call/return
        if (!action.to.offset) action.type = Action::CALL;
        else action.type = Action::RETURN;
      } else {
        assert(action.to_target);
        action.type = Action::RETURN;
      }
    }

    // process one execution chain
    switch (action.type) {
      case Action::TR_START:
        if (!stack.empty() &&
          (stack.back().type == Action::TR_END_HW_INT ||
          stack.back().type == Action::TR_END_CALL)) {
          // this is return to target function from sub function
          string &child_name = stack.back().to.name;
          child.add_target(child_name, (action.ts - stack.back().ts));
          child.add_sched(child_name, sched_in_child);
          stack.pop_back();
        } else if (!stack.empty() && stack.back().type == Action::TR_END) {
          stack.pop_back();
        } else {
          // wrong chain
          stack.clear();
          report_error_action("trace", action.id, action.lnum);
        }
        sched_in_child = 0;
        break;
      case Action::HW_INT:
      case Action::TR_END_HW_INT:
      case Action::CALL:
      case Action::TR_END_CALL:
        // this is call to sub function
        if (param.code_block) add_code_block(cursor, &action);
        stack.push_back(action);
        if (sched_begin) {
          /* ERROR: the schedule is not finished when the child function is called */
          report_error_action("schedule begin in child", sched_begin->id, sched_begin->lnum);
          sched_begin = nullptr;
        }
        sched_in_child = 0;
        break;
      case Action::TR_END:
        stack.push_back(action);
        break;
      case Action::IRET:
      case Action::RETURN:
      case Action::TR_END_RETURN:
        if (stack.size() == 1 && action.from_target && stack[0].to_target) {
          // the execution chain is done
          stat.add(stack[0], action, sched_in_target, child);
          child.clear();
          sched_in_target = 0;
          stack.clear();
        } else if (stack.size() == 2 && action.type != Action::TR_END_RETURN) {
          // return to target function from sub function
          string child_name = stack.back().to.name;
          if (!action.from.equal(&stack.back().to)) {
            // target may calls inline function, but returns from another functions
            if (param.call_line) {
              // 'from' function has not call address, we assign one
              uint64_t call_addr = stack.back().from.addr;
              stringstream ss;
              ss << action.from.name << "|" << std::hex << call_addr << std::dec;
              child_name = ss.str();
              // only for user symbol
              if(!srcline_map.get(child_name)) srcline_map.put(child_name, call_addr);
            } else {
              child_name = action.from.name;
            }
          }
          child.add_target(child_name, (action.ts - stack.back().ts));
          child.add_sched(child_name, sched_in_child);
          sched_in_child = 0;
          stack.pop_back();
        } else {
          // wrong chain, discard it
          stack.clear();
          sched_in_target = sched_in_child = 0;
        }
        break;
      default:
        abort();
    }
    cursor = &action;
  }
}

void ThreadJob::extract_actions() {
  uint32_t total_actions = 0;
  vector<ParseJob *> &parse_jobs = *parse_jobs_ptr;
  uint32_t parse_job_num = parse_jobs.size();
  for (ParseJob *parse_job : parse_jobs) {
    total_actions += parse_job->parsed_actions_num(tid);
    total_actions += parse_job->error_actions_num(tid);
  }
  actions.reserve(total_actions);

  // extract thread actions (parsed + error action) from mutiple parse job by merge sort
  vector<uint32_t> pointers(parse_job_num * 2, 0);
  while (true) {
    uint32_t min_idx = -1;
    uint64_t min_ts = UINT64_MAX;
    Action *min_action = nullptr;
    for (size_t i = 0; i < pointers.size(); ++i) {
      Action *action = nullptr;
      ParseJob *parse_job = parse_jobs[i / 2];
      if (i & 1) {
        // error action
        uint32_t idx = pointers[i];
        if (idx < parse_job->error_actions_num(tid))
          action = parse_job->get_error_action(tid, idx);
      } else {
        // normal parsed action
        uint32_t idx = pointers[i];
        if (idx < parse_job->parsed_actions_num(tid))
          action = parse_job->get_parsed_action(tid, idx);
      }
      if (action && action->ts < min_ts) {
        min_ts = action->ts;
        min_idx = i;
        min_action = action;
      }
    }
    if (min_action == nullptr) {
      break;
    }
    actions.push_back(*min_action);
    pointers[min_idx]++;
  }
  assert(actions.size() == total_actions);
}

static std::vector<std::string> action_type_strs = {
  "tr strt", "tr end  call", "tr end  return", "tr end  hw int",
  "tr end", "call", "return", "syscall", "jmp", "jcc", "hw int",
  "iret"
};

void ParseJob::decode_to_actions() {
  fstream ifs(filename, std::ios::in | std::ios::out);
  if (ifs.is_open()) {
    string line;
    size_t lnum = 0;
    while(getline(ifs, line)) {
      Action action;
      action.id = id;
      action.lnum = ++lnum;
      action.is_error = false;
      action.from_target = action.to_target = false;
      if (action.lnum < from || action.lnum >= to) {
        continue;
      }
      if (!line.compare(0, trace_error_str.size(),
              trace_error_str, 0, trace_error_str.size())) {
        if (line.find("Lost trace data") == string::npos) {
          continue;
        }
        // action implies the trace data lost
        action.is_error = true;
        size_t start = line.find("tid") + 3;
        size_t end = line.find("ip");
        action.tid = stol(line.substr(start, end - start));

        start = line.find("time") + 4;
        end = line.find_first_of('.', start);
        long sec = stol(line.substr(start, end - start));
        start = end + 1;
        end = line.find("cpu");
        long nsec = stol(line.substr(start, end - start));
        action.ts = sec * NSECS_PER_SECS + nsec;

        add_error_action(action);
        //if (param.verbose)
        //  printf("Thread %d lost data at timestamp %lld\n", action.tid, action.ts);
        continue;
      }

      /* action thread */
      size_t start = 0;
      size_t end = line.find_first_of('[', start);
      long tid = stol(line.substr(start, end - start));
      action.tid = tid;

      /* action cpu */
      start = end + 1;
      end = line.find_first_of(']', start);
      action.cpu = stol(line.substr(start, end - start));

      /* action timestamp */
      start = end + 1;
      end = line.find_first_of('.', start);
      long sec = stol(line.substr(start, end - start));
      
      start = end + 1;
      end = line.find_first_of(':', start);
      long nsec = stol(line.substr(start, end - start));
      action.ts = sec*NSECS_PER_SECS + nsec;

      /* action type */
      start = line.find_first_not_of(' ', end + 1);
      end = string::npos;
      for (size_t i=0; i<action_type_strs.size(); ++i) {
        string &type_str = action_type_strs[i];
        if (line.substr(start, type_str.size()) == type_str) {
          end = start + type_str.size();
          action.type = (Action::ActionType)(Action::TR_START + i);
          break;
        }
      }

      if (end == string::npos) {
        printf("error action type for line: %s\n", line.c_str());
        abort();
      }

      /* action symbol */
      start = line.find_first_not_of(' ', end);
      end = line.find("=>", start);
      action.from = Action::get_symbol(line.substr(start, end - start), nullptr);
      action.from_target = (action.from.name == param.target) ? true : false;
      
      start = line.find_first_not_of(' ', end + 2);
      end = line.size();
      action.to = Action::get_symbol(line.substr(start, end - start), &action.from);
      action.to_target = (action.to.name == param.target) ? true : false;

      /* action for offcpu */
      action.sched_begin = action.sched_end = false;
      if (param.offcpu) {
        if (action.to.name == param.sched_funcname &&
            action.to.offset == 0 && (action.type == Action::CALL
            || action.type == Action::TR_START)) {
          action.sched_begin = true;
        } else if (action.from.name == param.sched_funcname &&
                   (action.type == Action::RETURN ||
                    action.type == Action::TR_END_RETURN)) {
          action.sched_end = true;
        }
      }

      /* action for ancestor function */
      action.ancestor_begin = action.ancestor_end = false;
      if (param.ancestor != "") {
        if (action.to.name == param.ancestor &&
            action.to.offset == 0 && (action.type == Action::CALL
            || action.type == Action::TR_START)) {
          action.ancestor_begin = true;
        } else if (action.from.name == param.ancestor &&
                   (action.type == Action::RETURN ||
                    action.type == Action::TR_END_RETURN)) {
          action.ancestor_end = true;
        }
      }

      if (!action.from_target && !action.to_target &&
          !action.sched_begin && !action.sched_end &&
          !action.ancestor_begin && !action.ancestor_end) {
        /* current action does not contain target, ancestor,
         * and sched functions, discard it */
        continue;
      }

      if (!param.code_block && action.from_target && action.to_target) {
        /* inner-function jump, skip */
        continue;
      }

      /* add to action set */
      add_action(action);
    }
  }
}

static void print_cross_line(char c) {
  uint32_t max_width = 80, num = 1;
  if (param.offcpu) num += 1;
  if (param.srcline) num += 1;
  if (param.code_block) num += 1;
  max_width = std::max(HistogramDist::get_print_width(num), max_width);
  max_width = std::max(HistogramBucket::get_print_width(num), max_width);
  printf("\n\033[33m");
  for (int i = 0; i < max_width; ++i) printf("%c", c);
  printf("\033[0m\n");
}

static void print_title(const char *title) {
  printf("\033[32m%s\033[0m\n", title);
}

void Stat::add(Action &action_call, Action &action_return, uint64_t lat_s, LatencyChild &child) {
  uint64_t lat_t = action_return.ts - action_call.ts;
  std::string &caller = action_return.to.name;
  if (lat_t < param.latency_interval.first || lat_t > param.latency_interval.second ||
      action_call.ts < param.time_interval.first || action_call.ts > param.time_interval.second) {
    return;
  }
  if (param.timeline && action_call.ts >= param.time_start) {
    timeline_unit_lat += lat_t;
    if (++timeline_unit == param.timeline_unit) {
      // caculate the average for one timeline_unit
      timeline.push_back({(action_call.ts - param.time_start) / 1000.0,
                           timeline_unit_lat / timeline_unit / 1000.0}); // us
      timeline_unit_lat = timeline_unit = 0;
    }
  } else {
    add_latency(lat_t, lat_s, caller);
    if (!param.code_block) {
      // add latency of target function self
      child.add_target(TARGET_SELF, lat_t - child.target_total);
      child.add_sched(TARGET_SELF, lat_s - child.sched_total);
    }
    add_child_latency(child, caller);
  }
}

void Stat::Latency::print_summary() {
  HistogramDist hist("ns");
  target.set_val_name("cnt");
  hist.add_dist(target);
  if (param.offcpu) {
    sched.set_val_name("sched");
    hist.add_dist(sched);
  }
  hist.print();
}

void Stat::LatencyChild::print_summary() {
  HistogramBucket hist;

  Bucket oncpu("cpu_pct(%)");
  Bucket srcline(param.call_line ? "call_line" : "src_line");
  hist.init_key(target, [&](const std::string &name) -> std::string {
    if (param.srcline && !param.verbose) {
      // print func_name without address
      return name.substr(0, name.find_first_of('|'));
    }
    return name;
  });
  if (param.srcline) {
    // add srcline to all buckets
    srcline.add_bucket(target);
    int width = 10;
    srcline.loop_for_element([&](Bucket::Element &el){
      if (el.name.find(CODE_BLOCK_PREFIX) != string::npos) {
        // 'code block' latency needs the begin and end address
        string srcline_from, srcline_to;
        srcline_map.get(el.name + "_from", srcline_from);
        srcline_map.get(el.name + "_to", srcline_to);
        el.val_str = srcline_from + "-" + srcline_to;
      } else if (el.name == TARGET_SELF) {
        srcline_map.get(param.target, el.val_str);
      } else {
        srcline_map.get(el.name, el.val_str);
      }
      if (el.val_str.size() > width)
        width = el.val_str.size();
    });
    srcline.set_width(width + 1);
    hist.add_extra_bucket(&srcline);
  }
  if (param.offcpu) {
    uint64_t trace_time
      = param.trace_time * NSECS_PER_SECS - miss_trace_time.load();
    sched.set_val_name("sched_time");
    sched.set_count(target);
    hist.add_extra_bucket(&sched);

    oncpu.add_bucket(target);
    oncpu.sub_bucket(sched);
    oncpu.set_count(1);
    oncpu.set_scale(trace_time / 100);
    hist.add_extra_bucket(&oncpu);
  }

  hist.print();
}

static void print_latency(Stat::Latency &latency) {
  latency.print_summary();

  uint64_t target_avg = latency.target.get_avg();
  uint64_t target_cnt = latency.target.get_count();
  uint64_t sched_total = latency.sched.get_total();
  uint64_t sched_cnt = latency.sched.get_count();

  uint32_t width1 =
    floor(log10(sched_cnt + target_cnt + 1)) + 1;
  uint32_t width2 =
    floor(log10(target_avg + 1)) + 1;
  printf("trace count: %*lu, average latency: %*lu ns\n",
          width1, target_cnt, width2, target_avg);
  if (param.offcpu) {
    uint64_t trace_time = param.trace_time * NSECS_PER_SECS - miss_trace_time.load();
    uint64_t sched_avg =
      target_cnt > 0 ? sched_total / target_cnt : 0;
    int cpu_pct =
         100 * ((target_avg - sched_avg) * target_cnt) / trace_time;
    printf("sched count: %*lu,   sched latency: %*lu ns, cpu percent: %d \%\n",
          width1, sched_cnt, width2, sched_avg, cpu_pct);
  }
}

void Stat::print_summary() {
  char title[1024];
  string target_name = param.target;
  if (param.srcline) {
    string srcline;
    srcline_map.get(target_name, srcline);
    target_name += ("(" + srcline + ")");
  }
  /* print target function's latency */
  print_cross_line('=');
  snprintf(title, 1024,
           "Histogram - Latency of [%s]:",
           target_name.c_str());
  print_title(title);
  print_latency(latency);

  if (param.offcpu) {
    printf("sched total: %lu, sched each time: %lu ns\n", sched_count,
           sched_count > 0 ? (latency.sched.get_total() / sched_count) : 0);
  }

  /* print child function's latency  */
  print_cross_line('-');
  snprintf(title, 1024,
           "Histogram - Child functions's Latency of [%s]:",
           target_name.c_str());
  print_title(title);
  children.print_summary();

  for (auto &it : callers) {
    string caller_name = it.first;
    auto &caller = it.second;
    if (param.srcline) {
      string srcline;
      srcline_map.get(caller_name, srcline);
      if (!param.verbose)
        caller_name = caller_name.substr(0, caller_name.find_first_of('|'));
      caller_name += ("(" + srcline + ")");
    }
    /* target function's latency from current caller */
    print_cross_line('=');
    if (it.first != "unknown") {
      snprintf(title, 1024,
             "Histogram - Latency of [%s]\n"
             "           called from [%s]:",
             target_name.c_str(), caller_name.c_str());
      print_title(title);
      print_latency(caller.latency);
      print_cross_line('-');
    }

    /* print child function's latency from caller */
    snprintf(title, 1024,
             "Histogram - Child functions's Latency of [%s]\n"
             "                             called from [%s]:",
             target_name.c_str(), caller_name.c_str());
    print_title(title);
    caller.children.print_summary();
  }
  print_cross_line('=');
}

void Stat::print_timeline() {
  graphs::options gopt;
  gopt.type = graphs::type_braille;
  gopt.mark = graphs::mark_dot;
  gopt.style = graphs::style_light;
  gopt.check = false;
  gopt.color = graphs::color_default;
  gopt.xlabel = "x(us)";
  gopt.ylabel = "y(us)";
  //gopt.yexponent = true;
  printf("start_timestamp: %lld\n", param.time_start);
  if (timeline.size() > 0)
    graphs::plot(100, 160, 0, 0, 0, 0, timeline, gopt);
}

static void print_stat(unordered_map<long, ThreadJob *> &thread_jobs) {
  auto t1 = ut_time_now();
  if (param.timeline) {
    for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it) {
      ThreadJob *thread_job = it->second;
      Stat &stat = thread_job->get_stat();
      char title[1024];
      snprintf(title, 1024, "\nThread %ld: ", thread_job->get_tid());
      print_title(title);
      stat.print_timeline();
    }
  } else {
    Stat global_stat;
    /* merge all threads' latency */
    for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it) {
      ThreadJob *thread = it->second;
      global_stat.merge(thread->get_stat());
    }
    global_stat.print_summary();
  }
  auto t2 = ut_time_now();
  printf("[ print stat has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
}

/*
 * Main function for analyzing performance of function
 * */
static void analyze_funcs() {
  auto t1 = ut_time_now();
  size_t i;

  /* 1. dispatch parse_jobs */
  vector<ParseJob *> parse_jobs;
  if (param.parallel_script) {
    for (i = 0; i < param.worker_num; ++i) {
      char filename[1024];
      sprintf(filename, SCRIPT_FILE_PREFIX "__%05d", i);
      parse_jobs.push_back(new ParseJob(string(filename), 0, UINT32_MAX, i));
    }
  } else {
    size_t total = get_file_linecount(SCRIPT_FILE_PREFIX);
    if (total > 100 * param.worker_num) {
      size_t step = total / param.worker_num + 1;
      for (i = 0; i < param.worker_num; ++i) {
        size_t from = i * step;
        size_t to = std::min((i + 1) * step, total);
        parse_jobs.push_back(new ParseJob(SCRIPT_FILE_PREFIX, from, to, i));
      }
    } else {
      parse_jobs.push_back(new ParseJob(SCRIPT_FILE_PREFIX, 0, UINT32_MAX, 0));
    }
  }

  // do parse jobs
  for (i = 0; i < parse_jobs.size(); ++i) {
    worker_pool.add_job(parse_jobs[i], i);
  }
  worker_pool.wait_all_idle();

  auto t2 = ut_time_now();
  printf("[ parse actions has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
  t1 = ut_time_now();

  /* 2. analyze function for each thread */
  unordered_map<long, ThreadJob*> thread_jobs;
  size_t total_actions = 0, error_actions = 0;
  for (ParseJob *parse_job : parse_jobs) {
    // create thread jobs
    parse_job->loop_parsed_actions([&](ParseJob::ActionSet &as) {
      long tid = as.tid;
      if (!thread_jobs.count(tid) && as.target > 0) {
         thread_jobs[tid] = new ThreadJob(tid, &parse_jobs);
      }
      real_trace_time.first = std::min(real_trace_time.first, as.min_timestamp());
      real_trace_time.second = std::max(real_trace_time.second, as.max_timestamp());
      total_actions += as.size();
    });
    parse_job->loop_error_actions([&](ParseJob::ActionSet &as) {
      real_trace_time.first = std::min(real_trace_time.first, as.min_timestamp());
      real_trace_time.second = std::max(real_trace_time.second, as.max_timestamp());
      error_actions += as.size();
    });
  }
  param.time_start = real_trace_time.first;
  total_actions += error_actions;
  printf("[ parsed %d actions, trace errors: %d ]\n", total_actions, error_actions);

  // do thread job
  i = 0;
  for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it, ++i) {
    worker_pool.add_job(it->second, i);
  }
  worker_pool.wait_all_idle();

  t2 = ut_time_now();
  printf("[ analyze functions has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));

  // caculate the real trace time base on all actions timestamp
  if (real_trace_time.second > real_trace_time.first) {
    double trace_time =
          (double)(real_trace_time.second - real_trace_time.first) / NSECS_PER_SECS;
    printf("[ real trace time: %0.2f seconds ]\n", trace_time);
    if (trace_time > param.trace_time)
      param.trace_time = trace_time;
  }

  if (thread_jobs.size())
    miss_trace_time.store(miss_trace_time.load() / thread_jobs.size());
  printf("[ miss trace time: %0.2f seconds ]\n", (double) miss_trace_time.load() / NSECS_PER_SECS);

  if (param.ancestor != "") {
    char title[1024];
    snprintf(title, 1024,
        "[ ancestor: %s, call: %llu, return: %llu ]", param.ancestor.c_str(),
        ancestor_begin_count.load(), ancestor_end_count.load());
    print_title(title);
  }

  if (param.srcline) {
    srcline_map.process_address();
  }

  /* print summary */
  print_stat(thread_jobs);

  // free memory of all allocated job
  vector<MemoryFreeJob> memfree_jobs(parse_jobs.size() + thread_jobs.size());
  for (i = 0; i < parse_jobs.size(); ++i) {
    memfree_jobs[i].set_to_free(parse_jobs[i]);
    worker_pool.add_job(&memfree_jobs[i], i);
  }
  for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it, ++i) {
    memfree_jobs[i].set_to_free(it->second);
    worker_pool.add_job(&memfree_jobs[i], i);
  }
  assert(i == memfree_jobs.size());
  worker_pool.wait_all_idle();
}

static void perf_record() {
  char perf_cmd[1024];
  char record_filter[1024];
  string binary = param.binary;
  stringstream trace_param;
  snprintf(record_filter, 1024, "");

  /* record filter */
  if (param.ip_filtering) {
    string filter2 = "";
    if (param.offcpu)
      filter2 = param.offcpu_filter;
    if (param.ancestor != "") {
      if (param.offcpu) {
        printf("ERROR: under ip_filter, offcpu and ancestor filter "
               "can not be set at the same time.\n");
        exit(0);
      }
      filter2 = "filter " + param.ancestor +
                 " #0 @ " + binary + ",";
    }
    if (binary == "") {
      // kernel function
      param.func_idx = "";
    } else {
      // user function
      binary = "@ " + binary;
    }
    snprintf(record_filter, 1024, "--filter '%sfilter %s %s %s'",
             filter2.c_str(), param.target.c_str(),
             param.func_idx.c_str(), binary.c_str());
  }

  /* trace parameter */
  if (param.cpu != "") {
    trace_param << " -C " << param.cpu;
    printf("[ trace cpu %s for %.2f seconds ]\n", param.cpu.c_str(), param.trace_time);
    if (param.offcpu) {
      // TODO: there exists never-stop looping
      // when tracing schedule function for cpu tracing
      printf("Warning: offcpu is not support for CPU tracing, turn it off\n");
      param.offcpu = false;
    }
    if (param.ip_filtering) {
      printf("Warning: ip filtering is not support for CPU tracing, turn it off\n");
      param.ip_filtering = false;
    }
  } else if (param.tid != "") {
    trace_param << " -t " << param.tid;
    printf("[ trace thread %s for %.2f seconds ]\n", param.tid.c_str(), param.trace_time);
  } else if (param.pid != -1) {
    trace_param << " -p " << param.pid;
    printf("[ trace process %ld for %.2f seconds ]\n", param.pid, param.trace_time);
  } else {
    printf("ERROR: use --cpu/--pid/--tid to set trace method\n");
    exit(0);
  }

  if (param.per_thread_mode) {
    trace_param << " --per-thread --timestamp ";
  }

  /* perf record */
  snprintf(perf_cmd, 1024,
    "%s record -e intel_pt/cyc=1/%c -B %s %s -m,32M --no-bpf-event -- sleep %f",
    param.perf_tool.c_str(), param.offcpu ? ' ' : 'u',
    record_filter, trace_param.str().c_str(), param.trace_time);

  if (param.verbose) printf("%s\n", perf_cmd);
  /* execute */
  auto t1 = ut_time_now();
  system(perf_cmd);
  auto t2 = ut_time_now();
  printf("[ perf record has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
}

static void perf_script() {
  stringstream perf_cmd, script_filter;
  string itrace = "b";
  string field = "-F-event,-period,+tid,+cpu,+time,+addr,-comm,+flags,-dso";

  clear_script_files();

  /* script filter */
  if (param.parallel_script) {
    // ip filter
    if (!param.ip_filtering) {
      if (param.target != "") {
        script_filter << " --func_filter=\"" << param.target;
        if (param.ancestor != "")
          script_filter << "," << param.ancestor;
        script_filter << "\"";
      }
      if (param.binary != "") {
        script_filter << " --opt_dso_name=\"" << param.binary << "\"";
      }
    }
    // thread filter
    if (param.tid != "")
      script_filter << " --tid=" << param.tid;
    // time filter
    if (param.time_interval.first != 0
        || param.time_interval.second != UINT64_MAX) {
      char time_filter[1024];
      snprintf(time_filter, 1024, " --time=%d.%09d,%d.%09d",
               param.time_interval.first / NSECS_PER_SECS,
               param.time_interval.first % NSECS_PER_SECS,
               param.time_interval.second / NSECS_PER_SECS,
               param.time_interval.second % NSECS_PER_SECS);
      script_filter << " --time=" << time_filter;
    }
  }

  /* use dl filter to discard internal jump of target function, if not analyze code block latency */
  if (!param.code_block && access(param.perf_dlfilter.c_str(), F_OK) != -1) {
    script_filter << " --dlfilter=" << param.perf_dlfilter;
  }

  if (param.flamegraph == "cpu") {
    itrace = "i10usg127";
    field = "";
  } else if (param.flamegraph == "latency") {
    itrace = "b";
  } else {
    // for trace errors
    itrace += "e";
  }

  /* perf script */
  perf_cmd << param.perf_tool << " script --ns"
           << " --itrace=" << itrace
           << " " << script_filter.str()
           << " " << field;
  if (param.parallel_script) {
    perf_cmd << " --parallel=" << param.worker_num
             << " " << (param.verbose ? "" : "&> /dev/null");
  } else {
    perf_cmd << " > script_out";
  }
  if (param.verbose) printf("%s\n", perf_cmd.str().c_str());
  auto t1 = ut_time_now();
  system(perf_cmd.str().c_str());
  auto t2 = ut_time_now();
  printf("[ perf script has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
}

static void print_flame_graph() {
  stringstream cmd;
  char filename[128];

  if (param.flamegraph == "latency") {
    /* latency flamegraph */
    cmd << param.pt_flame_home << "/bin/pt_flame -j " << param.worker_num;
    if (param.parallel_script) {
      for (size_t i = 0; i < param.worker_num; ++i) {
        snprintf(filename, 128, SCRIPT_FILE_PREFIX "__%05d", i);
        cmd << " " << filename;
      }
    } else {
      cmd << " " SCRIPT_FILE_PREFIX;
    }
    cmd << " | " << param.scripts_home << "/flamegraph.pl"
        << " --countname=\"ns\" > flame.svg";
  } else if (param.flamegraph == "cpu") {
    /* cpu flamegraph */
    cmd << "cat ";
    if (param.parallel_script) {
      for (size_t i = 0; i < param.worker_num; ++i) {
        snprintf(filename, 128, SCRIPT_FILE_PREFIX "__%05d", i);
        if (access(filename, F_OK) != -1) {
          cmd << " " << filename;
        }
      }
    } else {
      cmd << " " SCRIPT_FILE_PREFIX;
    }
    cmd << " | " << param.scripts_home << "/stackcollapse-perf.pl"
        << " | " << param.scripts_home << "/flamegraph.pl > flame.svg";
  } else {
    printf("ERROR: wrong flamegraph mode, use latency or cpu\n");
    return;
  }

  if (param.verbose) printf("%s\n", cmd.str().c_str());
  auto t1 = ut_time_now();
  system(cmd.str().c_str());
  auto t2 = ut_time_now();
  printf("[ print flame graph has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
  printf("[ Flamegraph has been saved to flame.svg ]\n");
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    usage();
    exit(-1);
  }
  
  if (check_system()) exit(-1);
  
  int c;
  while (-1 != (c = getopt_long(argc, argv, opt_str, opts, NULL))) {
    switch (c) {
      case 'b':
        param.binary = string(optarg);
        break;
      case 'f':
        param.target = string(optarg);
        break;
      case 'd':
        param.trace_time = atof(optarg);
        break;
      case 'p':
        param.pid = atol(optarg);
        break;
      case 'T':
        param.tid = string(optarg);
        break;
      case 'C':
        param.cpu = string(optarg);
        break;
      case 'P':
        param.perf_tool = string(optarg);
        break;
      case 'w':
        param.worker_num = atol(optarg);
        break;
      case 'I':
        param.func_idx = string(optarg);
        break;
      case 'F':
        param.flamegraph = string(optarg);
        break;
      case '0': {
        string str = string(optarg);
        int sep = str.find_first_of(',');
        if (sep == string::npos) {
          printf("ERROR: wrong latency_interval format!\n");
          exit(0);
        }
        param.latency_interval.first = stol(str.substr(0, sep));
        param.latency_interval.second = stol(str.substr(sep + 1, str.size()));
        break;}
      case '1': {
        string str = string(optarg);
        int sep1 = str.find_first_of(',');
        int sep2 = str.find_last_of(',');
        if (sep1 == string::npos || sep1 == sep2) {
          printf("ERROR: wrong time_interval format!\n");
          exit(0);
        }
        param.time_start = stol(str.substr(0, sep1));
        param.time_interval.first =
                        param.time_start + stol(str.substr(sep1 + 1, sep2));
        param.time_interval.second =
                        param.time_start + stol(str.substr(sep2 + 1, str.size()));
        param.time_start = param.time_interval.first;
        break;}
      case '3':
        param.timeline_unit = atol(optarg);
        break;
      case '4':
        param.pt_flame_home = string(optarg);
        break;
      case '5':
        param.call_line = false;
        break;
      case 'c':
        param.code_block = true;
        break;
      case 'a':
        param.ancestor = string(optarg);
        break;
      case '2':
        param.history = atol(optarg);
        break;
      case 'l':
        param.timeline = true;
        break;
      case 'o':
        param.offcpu = true;
        break;
      case 't':
        param.per_thread_mode = true;
        break;
      case 'i':
        param.ip_filtering = true;
        break;
      case 's':
        param.parallel_script = true;
        break;
      case 'v':
        param.verbose = true;
        break;
      case 'h':
      default:
        usage();
        exit(0);
    }
  }
  if (param.verbose) dump_options();

  if (system("addr2line --help &> /dev/null")) {
    printf("Warning: addr2line command not found, run without srcline.\n");
    param.srcline = false;
  }

  if (param.binary == "") {
    printf("Warning: binary path is empty, run without srcline.\n");
    param.srcline = false;
  }

  if (param.trace_time <= 0) {
    printf("Error: trace_time must be greater than 0\n ");
    exit(0);
  }

  if (param.offcpu && getuid() != 0) {
    printf("Error: offcpu time needs root privilege, please use sudo command\n");
    exit(0);
  }

  if (param.flamegraph == "latency" && check_pt_flame(param.pt_flame_home)) {
    exit(0);
  }

  // perf record
  if (param.history < 2)
    perf_record();

  if (param.history == 1) {
    printf("[ trace done, you can copy perf.data and the binary file \n"
           "  (with the same absolute path) to another machine for analysis. ]\n");
    exit(0);
  }

  printf("[ start %d parallel workers ]\n", param.worker_num);
  // create worker pool
  worker_pool.start(param.worker_num);

  // perf script
  perf_script();

  if (param.flamegraph != "") {
    // flamegraph mode
    print_flame_graph();
  } else {
    // function analysis mode
    if (param.target == "") {
      printf("ERROR: target function name is required if is not in flamegraph mode\n");
      exit(0);
    }
    /* analyze funcs */
    analyze_funcs();
  }

  /* clear resource */
  if (!param.verbose && !param.history) clear_record_files();
  if (!param.verbose) clear_script_files();
  exit(0);
}
