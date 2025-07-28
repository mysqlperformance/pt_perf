#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <thread>
#include <getopt.h>
#include <cmath>
#include <sstream>
#include <signal.h>

#include "stat_tools.h"
#include "sys_tools.h"
#include "pt_linux_perf.h"
#include "func_latency.h"

using namespace std;

Param param;
FuncGlobalStatus gstat;
/* use to decode source file and line number */
static SrclineMap srcline_map;
static ParallelWorkerPool worker_pool;

Param::Param() {
  perf_tool = get_executor_dir() + "/perf";
  perf_dlfilter = get_executor_dir() + "/perf_dlfilter.so";
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
  call_line = true;
  pt_config = "cyc=1";
  compact_format = true;

  ancestor = "";
  ancestor_latency = {0, UINT64_MAX};
  code_block = false;

  timeline = false;
  timeline_unit = 1;
  offcpu_filter = "filter " + sys_sched_funcname +" ,";
  latency_interval = {0, UINT64_MAX};
  time_interval = {0, UINT64_MAX};
  time_start = UINT64_MAX;
  history = 0;

  flamegraph = "";
  scripts_home = get_executor_dir() + "/scripts";
  pt_flame_home = "/usr/share/pt_flame";
  result_dir = "";
  unfold_gathered_line = false;

  sub_command = "";
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
  {"pt_config", 1, NULL, '5'},
  {"script_format", 1, NULL, '6'},
  {"result_dir", 1, NULL, 'D'},
  {"unfold_gathered_line", 0, NULL, 'U'},
  {"code_block", 0, NULL, 'c'},
  {"offcpu", 0, NULL, 'o'},
  {"per_thread", 0, NULL, 't'},
  {"ip_filter", 0, NULL, 'i'},
  {"parallel_script", 0, NULL, 's'},
  {"verbose", 0, NULL, 'v'},
  {"help", 0, NULL, 'h'},
  {NULL, 0, NULL, 0}
};
const char *opt_str = "hvsitolcUb:f:d:p:T:C:P:a:w:I:F:D:";

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
    "\t-a / --ancestor        --- only analyze target function with 'ancestor' function in its call chain,\n"
    "\t                           eg, 'test#100,200', we shows the result of target function\n"
    "\t                           where its ancestor latency is between 100ns and 200 ns.\n"
    "\t-c / --code_block      --- show the code block latency of target function\n"
    "\t     --history         --- for history trace, 1: generate perf.data, 2: use perf.data \n"
    "\t-D / --result_dir      --- the result directory to save and use perf.data and temporary files\n"
    "\t-U / --unfold_gathered_line\n"
    "\t                       --- unfold the call-line which gathered for simplicity, like interrupts that\n"
    "\t                           may be called from multiple locations\n"
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

static void sig_handler(int sig) {
  abort_cmd_killable(sig);
  if (param.history < 2)
    clear_record_files();
  clear_script_files();
  exit(1);
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
  printf("sub_command: %s\n", param.sub_command.c_str());
}

void ThreadJob::do_analyze() {
  vector<Action> stack;
  FuncStat::LatencyChild child;

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
  // if current execution chain is wrong
  bool wrong_chain = true;

  /* clear execution chain */
  auto clear_context = [&]() {
    stack.clear();
    child.clear();
    sched_in_child = sched_in_target = 0;
    sched_begin = nullptr;
    ancestor_begin = nullptr;
  };

  /* add one child function latency */
  auto add_one_child = [&](Action *a1, Action *a2,
      bool unknown = false, bool gather_call_line = false) {
    std::string child_name = a1->to->name;
    /* for "to" symbol, attach call address to
     * the tail of funcname */

    if (likely(param.call_line)) {
      if (unlikely(gather_call_line) && !param.unfold_gathered_line) {
        funcname_add_string_mark(child_name, GATHER_CALL_LINE);
      } else if (child_name != param.target
          && child_name != param.ancestor) {
        // show the source line of the call address
        funcname_add_addr(child_name, a1->from->addr);
      }
    }

    uint64_t lat_t = a2->ts - a1->ts;
    uint64_t lat_s = sched_in_child;
    if (unlikely(unknown)) {
      child_name = FuncStat::unknown_latency_str + child_name;
      lat_t = lat_s = 0;
    }
    child.add_target(child_name, lat_t);
    child.add_sched(child_name, lat_s);
    sched_in_child = 0;
  };

  /* add code block latency */
  auto add_code_block = [&](Action *a1, Action *a2) {
    if (a2->from->offset == a1->to->offset) return;
    std::string child_name = CODE_BLOCK_PREFIX + to_string(a1->to->offset) + "-"
                   + to_string(a2->from->offset);
    uint64_t lat_t = a2->ts - a1->ts;
    uint64_t lat_s = sched_in_child;
    child.add_target(child_name, lat_t);
    child.add_sched(child_name, lat_s);
    // set to obtain srcline of code block
    if(!srcline_map.get(child_name + "_from"))
      srcline_map.put(child_name + "_from", a1->to->addr);
    if(!srcline_map.get(child_name + "_to"))
      srcline_map.put(child_name + "_to", a2->from->addr);
    assert(lat_t >= lat_s);
    sched_in_child = 0;
    cursor = a2;
  };

  for (size_t i = 0; i < actions.size(); ++i) {
    Action &action = actions[i];

    if (action.is_error) {
      /* encounter trace error, discard current execution chain,
       * set caller of exist child latency as unknown */
      stat.add_unknown_latency(child, "unknown");
      clear_context();
      prev_target_error = true;
      continue;
    }

    if (action.to->offset == 0 && action.to_target) {
       /* new target function is called, add child function
        * of previous round */
       if (target_begin && stack.size() == 2) {
         // add stat of no-return child
         add_one_child(&stack.back(), &action, true);
       }
       if (!child.empty()) {
          /* there are not return action in last execution chain,
           * we just add the child latency information */
         if (!wrong_chain && target_begin) {
           std::string caller = target_begin->from->name;
           if (param.call_line && !param.ip_filtering) {
             funcname_add_addr(caller, target_begin->from->addr);
           }
           stat.add_unknown_latency(child, caller);
         } else {
           stat.add_unknown_latency(child, "unknown");
         }
         child.clear();
       }
       /* this action is the start point for target function,
        * clear context in previous round. */
       stack.clear();
       stack.push_back(action);
       sched_in_target = sched_in_child = 0;
       if (sched_begin) {
         /* ERROR: the schedule in previous round is not finished
          * when the new target is called */
         report_error_action("schedule begin", sched_begin, param.verbose);
         sched_begin = nullptr;
       }
       if (prev_target_error) {
         /* if error occurs in previous round,
          * calculate the trace missing time */
         if (target_begin)
           gstat.miss.fetch_add(action.ts - target_begin->ts);
         prev_target_error = false;
       }
       target_begin = &action;
       wrong_chain = false; // reset
       cursor = &action;
       continue;
    }

    if (check_ancestor) {
      /* filter actions by ancestor function */
      if (action.ancestor_begin) {
        // ancestor begin now
        // clear_context();
        ancestor_begin = &action;
        gstat.ancestor_begin.fetch_add(1);
        if (param.ancestor_latency.first != 0 ||
            param.ancestor_latency.second != UINT64_MAX) {
          // find the ancestor end in follow actions
          Action *ancestor_end = nullptr;
          for (size_t j = i; j < actions.size(); ++j) {
            if (actions[j].ancestor_end) {
              ancestor_end = &actions[j];
              break;
            }
          }
          if (ancestor_end) {
            uint64_t al = ancestor_end->ts - ancestor_begin->ts;
            if (al < param.ancestor_latency.first ||
                al > param.ancestor_latency.second) {
              // discard if not within ancestor latency
              ancestor_begin = nullptr;
            }
          } else {
            // incomplete ancestor calls
            ancestor_begin = nullptr;
          }
        }
        continue;
      } else if (action.ancestor_end) {
        /* ancestor is end */
        ancestor_begin = nullptr;
        gstat.ancestor_end.fetch_add(1);
        continue;
      } else if (!ancestor_begin) {
        /* we only add target function within the ancestor,
         * other targets is discard. */
        continue;
      }
    }

    if (action.sched_begin) {
      /* thread is schedule-out */
      sched_begin = &action;
      continue;
    }

    if (action.sched_end) {
      /* thread is schedule-in */
      if (sched_begin) {
        /* caculate the schedule time */
        uint32_t sched_time = action.ts - sched_begin->ts;
        sched_in_target += sched_time;
        sched_in_child += sched_time;
        ++stat.sched_count;
        sched_begin = nullptr;
      } else {
        /* ERROR: the schedule has not started, but sched_end occurs. */
        report_error_action("schedule end", &action, param.verbose);
      }
      continue;
    }

    if ((action.type == PT_ACTION_JMP || action.type == PT_ACTION_JCC)) {
      /* change jmp/jcc instruction to call/return */
      if (action.from_target && action.to_target) {
        // internal jump, just add code block latency
        assert(param.code_block);
        add_code_block(cursor, &action);
        continue;
      } else if (action.from_target) {
        // change jmp instruction to call/return
        if (!action.to->offset) action.type = PT_ACTION_CALL;
        else action.type = PT_ACTION_RETURN;
      } else {
        assert(action.to_target);
        action.type = PT_ACTION_RETURN;
      }
    }

    /* process one execution chain */
    switch (action.type) {
      case PT_ACTION_TR_START:
        /* for ipfiltering, usually means return from child. */
        if (!stack.empty() &&
          (stack.back().type == PT_ACTION_TR_END_HW_INT ||
          stack.back().type == PT_ACTION_TR_END_CALL)) {
          // this is return to target function from sub function
          add_one_child(&stack.back(), &action);
          stack.pop_back();
        } else if (!stack.empty() && stack.back().type == PT_ACTION_TR_END) {
          stack.pop_back();
        } else {
          // wrong chain
          stack.clear();
          wrong_chain = true;
          report_error_action("trace", &action, param.verbose);
        }
        sched_in_child = 0;
        break;
      case PT_ACTION_HW_INT:
      case PT_ACTION_TR_END_HW_INT:
      case PT_ACTION_CALL:
      case PT_ACTION_TR_END_SYSCALL:
      case PT_ACTION_TR_END_CALL:
        /* this is call to child function */
        if (param.code_block) add_code_block(cursor, &action);
        stack.push_back(action);
        if (sched_begin) {
          /* ERROR: the schedule is not finished when the child function is called */
          report_error_action("schedule begin in child", sched_begin, param.verbose);
          sched_begin = nullptr;
        }
        sched_in_child = 0;
        break;
      case PT_ACTION_TR_END:
        stack.push_back(action);
        break;
      case PT_ACTION_IRET:
      case PT_ACTION_RETURN:
      case PT_ACTION_TR_END_RETURN:
        /* return from target function */
        if (stack.size() == 1 && action.from_target && stack[0].to_target) {
          // the execution chain is done
          stat.add(stack[0], action, sched_in_target, child);
          child.clear();
          sched_in_target = 0;
          stack.clear();
        } else if (!stack.empty() && stack.back().from_target
            && action.type != PT_ACTION_TR_END_RETURN) {
          bool gather_call_line = (action.type == PT_ACTION_IRET);
          /* for non-ipfiltering, return to target function from child function */
          add_one_child(&stack.back(), &action, false, gather_call_line);
          stack.pop_back();
        } else {
          // wrong chain, discard it
          stack.clear();
          wrong_chain = true;
          sched_in_target = sched_in_child = 0;
        }
        break;
      default:
        printf("ERROR: unknown action at timestamp %llu\n", action.ts);
        break;
    }
    cursor = &action;
  }

  if (!child.empty()) {
     /* there are incomplete call chains, just add
      * the child latency for more information */
     stat.add_unknown_latency(child, "unknown");
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

void ParseJob::decode_to_actions() {
  auto init_action = [&](Action &action) -> void {
    if (action.pt_type != PT_ACTION_TYPE_BRANCH && 
        action.pt_type != PT_ACTION_TYPE_ERROR) {
      return;
    }
    if (action.is_error) {
      /* add to error action set */
      add_error_action(action);
      return;
    }
    action.from_target =
      (action.from->name == param.target) ? true : false;
    action.to_target =
      (action.to->name == param.target) ? true : false;

    /* action for offcpu */
    action.sched_begin = action.sched_end = false;
    if (param.offcpu) {
      action.init_for_sched();
    }

    /* action for ancestor function */
    action.ancestor_begin = action.ancestor_end = false;
    if (param.ancestor != "") {
      action.init_for_ancestor(param.ancestor);
    }

    if (!action.from_target && !action.to_target &&
        !action.sched_begin && !action.sched_end &&
        !action.ancestor_begin && !action.ancestor_end) {
      /* current action does not contain target, ancestor,
       * and sched functions, discard it */
      return;
    }
    if (!param.code_block && action.from_target && action.to_target) {
      /* inner-function jump, discard it */
      return;
    }
    /* add to action set */
    add_action(action);
    return;
  };

  if (param.compact_format) {
    read_actions_from_compact_file(filename,
        id, sym_mgr, init_action); 
  } else {
    read_actions_from_text_file(filename,
        id, from, to, sym_mgr, init_action); 
  }
}

static void print_stat(FuncStat::Option &opt,
    unordered_map<long, ThreadJob *> &thread_jobs) {
  auto t1 = ut_time_now();
  if (param.timeline) {
    vector<std::pair<long, ThreadJob *>> vec(thread_jobs.begin(), thread_jobs.end());
    std::sort(vec.begin(), vec.end());
    for (const auto &it: vec) {
      ThreadJob *thread_job = it.second;
      FuncStat &stat = thread_job->get_stat();
      char title[1024];
      snprintf(title, 1024, "\nThread %ld: ", thread_job->get_tid());
      print_title(title);
      stat.print_timeline();
    }
  } else {
    FuncStat stat(opt, &srcline_map);
    /* merge all threads' latency */
    for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it) {
      ThreadJob *thread = it->second;
      stat.merge(thread->get_stat());
    }
    stat.print();
  }
  auto t2 = ut_time_now();
  printf("[ print stat has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));
}

static void assign_parse_jobs(vector<ParseJob *> &parse_jobs) {
  if (param.parallel_script) {
    for (size_t i = 0; i < param.worker_num; ++i) {
      char filename[1024];
      sprintf(filename, SCRIPT_FILE_PREFIX "__%05d", i);
      parse_jobs.push_back(new ParseJob(string(filename), 0, UINT32_MAX, i));
    }
  } else {
    size_t total = get_file_linecount(SCRIPT_FILE_PREFIX);
    if (total > 100 * param.worker_num) {
      size_t step = total / param.worker_num + 1;
      for (size_t i = 0; i < param.worker_num; ++i) {
        size_t from = i * step;
        size_t to = std::min((i + 1) * step, total);
        parse_jobs.push_back(new ParseJob(SCRIPT_FILE_PREFIX, from, to, i));
      }
    } else {
      parse_jobs.push_back(new ParseJob(SCRIPT_FILE_PREFIX, 0, UINT32_MAX, 0));
    }
  }
}

static void assign_thread_jobs(vector<ParseJob *> &parse_jobs,
    unordered_map<long, ThreadJob*> &thread_jobs) {
  size_t total_actions = 0, error_actions = 0;
  for (ParseJob *parse_job : parse_jobs) {
    // create thread jobs
    parse_job->loop_parsed_actions([&](ActionSet &as) {
      long tid = as.tid;
      if (!thread_jobs.count(tid) && as.target > 0) {
         thread_jobs[tid] = new ThreadJob(tid, &parse_jobs);
      }
      gstat.update_real(as);
      total_actions += as.size();
    });
    parse_job->loop_error_actions([&](ActionSet &as) {
      gstat.update_real(as);
      error_actions += as.size();
    });
  }
  param.time_start = gstat.real.first;
  total_actions += error_actions;
  printf("[ parsed %d actions, trace errors: %d ]\n", total_actions, error_actions);
}

/*
 * Main function for analyzing performance of function
 * */
static void analyze_funcs() {
  FuncStat::Option stat_opt = {
     param.target,
     param.offcpu,
     param.call_line,
     param.code_block,
     param.latency_interval,
     param.time_interval,
     (uint64_t)(param.trace_time * NSECS_PER_SECS),
     param.timeline,
     param.time_start,
     param.timeline_unit,
     param.ip_filtering};

  /* 1. dispatch parse_jobs */
  vector<ParseJob *> parse_jobs;
  assign_parse_jobs(parse_jobs);
  
  // do parse jobs
  auto t1 = ut_time_now();
  for (size_t i = 0; i < parse_jobs.size(); ++i) {
    worker_pool.add_job(parse_jobs[i], i);
  }
  worker_pool.wait_all_idle();
  auto t2 = ut_time_now();

  printf("[ parse actions has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));

  /* 2. analyze function for each thread */
  unordered_map<long, ThreadJob*> thread_jobs;
  assign_thread_jobs(parse_jobs, thread_jobs);
  stat_opt.time_start = param.time_start;

  // do thread job
  size_t i = 0;
  t1 = ut_time_now();
  for (auto it = thread_jobs.begin(); it != thread_jobs.end(); ++it, ++i) {
    it->second->init_stat(stat_opt);
    worker_pool.add_job(it->second, i);
  }
  worker_pool.wait_all_idle();
  t2 = ut_time_now();

  printf("[ analyze functions has consumed %.2f seconds ]\n",
          ut_time_diff(t2, t1));

  if (gstat.real_trace_time() > param.trace_time) {
    param.trace_time = gstat.real_trace_time();
    stat_opt.trace_time = (uint64_t)(param.trace_time * NSECS_PER_SECS);
  }
  gstat.print(thread_jobs.size(), param.ancestor);
  stat_opt.trace_time -= gstat.miss.load();

  /* print summary */
  print_stat(stat_opt, thread_jobs);

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

static string get_record_filter() {
  if (!param.ip_filtering)
    return "";

  string record_filter = "";
  string binary = param.binary;
  string filter2 = "";
  if (param.offcpu)
    filter2 = param.offcpu_filter;
  if (param.ancestor != "") {
    if (param.offcpu) {
      printf("ERROR: under ip_filter, offcpu and ancestor filter "
             "can not be set at the same time.\n");
      exit(0);
    }
    filter2 = "filter " + param.ancestor + " #0 @ " + binary + ",";
  }
  if (binary == "") {
    // kernel function
    param.func_idx = "";
  } else {
    // user function
    binary = "@ " + binary;
  }
  record_filter = "--filter '" + filter2 + "filter " + param.target +
                  " " + param.func_idx + " " + binary + "'";
  return record_filter;
}

static string get_script_filter() {
  stringstream script_filter;
  // ip filter
  if (param.parallel_script) {
    if (!param.ip_filtering && param.flamegraph == "") {
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
  }

  // time filter
  if (param.time_interval.first != 0
      || param.time_interval.second != UINT64_MAX) {
    char time_filter[1024];
    snprintf(time_filter, 1024, " --time=%d.%09d,%d.%09d",
             param.time_interval.first / NSECS_PER_SECS,
             param.time_interval.first % NSECS_PER_SECS,
             param.time_interval.second / NSECS_PER_SECS,
             param.time_interval.second % NSECS_PER_SECS);
    script_filter << time_filter;
  }
  /* use dl filter to discard internal jump of target function, if not analyze code block latency */
  if (!param.code_block && access(param.perf_dlfilter.c_str(), F_OK) != -1) {
    script_filter << " --dlfilter=" << param.perf_dlfilter;
  }
  return script_filter.str();
}

static void check_parameter() {
  if (param.verbose) dump_options();

  if (system("addr2line --help &> /dev/null")) {
    printf("Warning: addr2line command not found, run without src_line/call_line.\n");
    param.call_line = false;
  }
  if (param.binary == "") {
    printf("Warning: binary path is empty, run without src_line/call_line.\n");
    param.call_line = false;
  }
  if (param.ancestor == param.target) {
    param.ancestor = "";
    param.latency_interval.first =
      std::max(param.latency_interval.first, param.ancestor_latency.first);
    param.latency_interval.second =
      std::min(param.latency_interval.second, param.ancestor_latency.second);
  }
  if (param.offcpu && getuid() != 0) {
    printf("Error: offcpu time needs root privilege, please use sudo command\n");
    exit(0);
  }
  if (param.flamegraph == "latency" && check_pt_flame(param.pt_flame_home)) {
    exit(0);
  }
  if (param.cpu != "" && param.offcpu) {
    // TODO: there exists never-stop looping
    // when tracing schedule function for cpu tracing
    printf("Warning: offcpu is not support for CPU tracing, turn it off\n");
    param.offcpu = false;
  }
  if (param.cpu != "" && param.ip_filtering) {
    printf("Warning: ip filtering is not support for CPU tracing, turn it off\n");
    param.ip_filtering = false;
  }
  if (param.flamegraph == "" && param.target == "") {
    printf("ERROR: target function name is required if is not in flamegraph mode\n");
    exit(0);
  }
  if (param.compact_format) {
    if (!param.parallel_script || param.flamegraph != "") {
      param.compact_format = false;
    }
  }

  if (param.pt_config.find("cyc=1") == std::string::npos) {
    pt::ActionSet::out_of_order = true;
  }
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    usage();
    exit(-1);
  }
  
  if (check_system()) exit(-1);

  signal(SIGINT, sig_handler);

  param.sub_command = parse_sub_command(argc, argv);;

  int c;
  while (-1 != (c = getopt_long(argc, argv, opt_str, opts, NULL))) {
    switch (c) {
      case 'b':
        param.binary = resolve_path(string(optarg));
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
        param.tid = parse_number_range_to_sequence(string(optarg));
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
        param.latency_interval = get_interval_from_string(str);
        break;}
      case '1': {
        string str = string(optarg);
        int sep1 = str.find_first_of(',');
        int sep2 = str.find_last_of(',');
        if (sep1 == string::npos || sep1 == sep2) {
          printf("ERROR: wrong time_interval format!\n");
          exit(1);
        }
        param.time_start = str2long(str.substr(0, sep1));
        param.time_interval.first =
                        param.time_start + str2long(str.substr(sep1 + 1, sep2));
        param.time_interval.second =
                        param.time_start + str2long(str.substr(sep2 + 1, str.size()));
        param.time_start = param.time_interval.first;
        break;}
      case '3':
        param.timeline_unit = atol(optarg);
        break;
      case '4':
        param.pt_flame_home = string(optarg);
        break;
      case '5':
        param.pt_config = string(optarg);
        break;
      case '6': {
        string script_format = string(optarg);
        if (script_format == "text") {
          param.compact_format = false;
        }
        break;}
      case 'D' : {
        string dir = string(optarg);
        if (!check_path_exist(dir) && create_directory(dir)) {
          printf("ERROR: Failed to create result directory!\n");
          exit(1);
        }
        param.result_dir = dir;
        break;}
      case 'c':
        param.code_block = true;
        break;
      case 'a': {
        string str = string(optarg);
        int sep = str.find_first_of('#');
        if (sep != string::npos) {
          param.ancestor = str.substr(0, sep);
          param.ancestor_latency =
            get_interval_from_string(str.substr(sep + 1, str.size() - sep));
        } else {
          param.ancestor = str;
        }
        break;}
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
      case 'U':
        param.unfold_gathered_line = true;
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
  
  check_parameter();

  PerfOption perf_option = {
    param.binary,
    param.cpu,
    param.tid,
    param.pid,
    param.trace_time,
    param.per_thread_mode,
    param.perf_tool,
    "",
    "",
    param.parallel_script,
    "",
    "-F-event,-period,+tid,+cpu,+time,+addr,-comm,+flags,-dso",
    "be",
    param.worker_num,
    param.compact_format,
    param.sub_command,
    param.history,
    param.verbose};

  if (param.result_dir != "")
    switch_work_dir(param.result_dir);

  // perf record
  perf_option.intel_pt_config = "intel_pt/" + param.pt_config +
                                "/" + (param.offcpu ? ' ' : 'u');
  perf_option.record_filter = get_record_filter();
  perf_record(perf_option);

  // create worker pool
  worker_pool.start(param.worker_num);

  // init srcline map
  srcline_map.init(param.binary);

  // perf script
  perf_option.script_filter = get_script_filter();
  if (param.flamegraph == "cpu") {
    perf_option.itrace = "i10usg127";
    perf_option.fields = "";
  } else if (param.flamegraph == "latency") {
    perf_option.itrace = "b";
  }
  if (param.history < 3) {
    perf_script(perf_option);
  }

  if (param.flamegraph != "") {
    // flamegraph mode
    FlameGraphOption flame_option = {
      param.flamegraph,
      param.scripts_home,
      param.pt_flame_home,
      param.parallel_script,
      param.worker_num,
      param.verbose};
    print_flame_graph(flame_option);
  } else {
    // function analysis mode
    analyze_funcs();
  }

  /* clear resource */
  if (!param.verbose && !param.history) clear_record_files();
  if (!param.verbose && param.history < 3) clear_script_files();
  exit(0);
}
