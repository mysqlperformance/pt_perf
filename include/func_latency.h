#ifndef _h_func_latency_
#define _h_func_latency_
#include <string>
#include <vector>
#include <unordered_map>

#include "stat_tools.h"
#include "worker.h"
#include "pt_action.h"

using namespace pt;

struct Param {
  std::string perf_tool;
  std::string perf_dlfilter;
  std::string binary;
  std::string target;
  float trace_time;
  long pid;
  std::string tid;
  std::string cpu;
  bool verbose;
  bool parallel_script;
  bool per_thread_mode;
  size_t worker_num;
  bool ip_filtering;
  std::string func_idx;
  bool offcpu;
  bool call_line;
  std::string pt_config;
  bool compact_format;

  std::string ancestor;
  std::pair<uint64_t, uint64_t> ancestor_latency;

  bool code_block;

  bool timeline;
  uint32_t timeline_unit;
  std::pair<uint64_t, uint64_t> latency_interval;
  std::pair<uint64_t, uint64_t> time_interval;
  uint64_t time_start;
  int history;
  std::string offcpu_filter;

  std::string flamegraph;
  std::string scripts_home;
  std::string pt_flame_home;
  std::string result_dir;

  bool unfold_gathered_line;

  std::string sub_command;
  Param();
};
extern Param param;

/* parse action from file */
class ParseJob : public ParallelJob {
public:
  ParseJob(const std::string &name, uint32_t f, uint32_t t, uint32_t i) 
    : filename(name), from(f), to(t), id(i) {}

  void exec() override {
    decode_to_actions();
    sort_actions();
  }

  void add_action(Action &a) {
    ActionSet &as = parsed_actions[a.tid];
    as.tid = a.tid;
    as.add_action(a);
    if (a.from_target || a.to_target)
      ++as.target;
  }

  void add_error_action(Action &a) {
    ActionSet &as = error_actions[a.tid];
    as.tid = a.tid;
    as.add_action(a);
  }

  void decode_to_actions();
  void sort_actions() {
    // now only error actions may be out-of-order. if pt is configed
    // with "cyc == 0", it may be out-of-order too.
    for (auto it = error_actions.begin(); it != error_actions.end(); ++it) {
      ActionSet &as = it->second;
      as.sort();
    }
    if (pt::ActionSet::out_of_order) {
      for (auto it = parsed_actions.begin(); it != parsed_actions.end(); ++it) {
        ActionSet &as = it->second;
        as.sort();
      }
    }
  }
  size_t parsed_actions_num(int tid) {
    if (parsed_actions.count(tid) > 0)
      return parsed_actions[tid].size();
    return 0;
  }
  Action *get_parsed_action(int tid, uint32_t idx) {
    if (parsed_actions.count(tid) > 0)
      return &parsed_actions[tid][idx];
    return nullptr;
  }
  size_t error_actions_num(int tid) {
    if (error_actions.count(tid) > 0)
      return error_actions[tid].size();
    return 0;
  }
  Action *get_error_action(int tid, uint32_t idx) {
    if (error_actions.count(tid) > 0)
      return &error_actions[tid][idx];
    return nullptr;
  }
  template <typename Func>
  void loop_parsed_actions(Func f) {
    for (auto it = parsed_actions.begin();
            it != parsed_actions.end(); ++it) {
      f(it->second);
    }
  }
  template <typename Func>
  void loop_error_actions(Func f) {
    for (auto it = error_actions.begin();
            it != error_actions.end(); ++it) {
      f(it->second);
    }
  }
  friend class ThreadJob;

private:
  std::string filename;
  uint32_t from;
  uint32_t to;
  uint32_t total;
  uint32_t id;

  // store decoded acitions grouped by thread
  std::unordered_map<long, ActionSet> parsed_actions;
  // store trace error action grouped by thread
  std::unordered_map<long, ActionSet> error_actions;

  // symbol manager
  SymbolMgr sym_mgr;
};

/* class of traced thread */
class ThreadJob : public ParallelJob {
public:
  ThreadJob(long t, std::vector<ParseJob *> * ptr)
    : tid(t), parse_jobs_ptr(ptr) {}

  void exec() override {
    extract_actions();
    do_analyze();
  }

  long get_tid() { return tid; }
  void set_tid(long t) { tid = t; }
  void extract_actions();
  void do_analyze();

  void init_stat(FuncStat::Option &opt) {
    stat.opt = opt;
  }
  FuncStat &get_stat() { return stat; }

private:
  std::vector<ParseJob *> *parse_jobs_ptr;
  std::vector<Action> actions;

  FuncStat stat;
  long tid;
};

#endif
