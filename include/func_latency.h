#ifndef _h_func_latency_
#define _h_func_latency_
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

#include "stat_tools.h"
#include "worker.h"

#define NSECS_PER_SECS 1000000000UL
#define SCRIPT_FILE_PREFIX "script_out"

struct Param {
  std::string perf_tool;
  std::string binary;
  std::string target;
  float trace_time;
  long pid;
  long tid;
  std::string cpu;
  bool verbose;
  bool parallel_script;
  bool per_thread_mode;
  size_t worker_num;
  bool ip_filtering;
  std::string func_idx;
  bool offcpu;
  bool srcline;

  bool timeline;
  uint32_t timeline_unit;
  std::pair<uint64_t, uint64_t> latency_interval;
  std::pair<uint64_t, uint64_t> time_interval;
  uint64_t time_start;
  int history;
  std::string sched_funcname;
  std::string offcpu_filter;

  std::string flamegraph;
  std::string scripts_home;
  std::string pt_flame_home;
  Param();
};
extern Param param;

struct Symbol {
  std::string name;
  uint64_t addr;
  uint offset;
};

struct Action {
  enum ActionType : uint8_t {
    TR_START = 0,
    TR_END_CALL,
    TR_END_RETURN,
    TR_END_HW_INT,
    TR_END,
    CALL,
    RETURN,
    SYSCALL
  };
  static Symbol get_symbol(const std::string &str);

  Symbol from;
  Symbol to;
  uint64_t ts; // timestamp for nanosecond
  enum ActionType type;
  long tid;
  long cpu;
  long id;
  long lnum;
  bool from_target;
  bool to_target;
  bool sched_begin; // begin of schedule
  bool sched_end; // end of schedule
  bool is_error;
};

class Stat {
public:
  struct Latency {
    Distribution target;
    Distribution sched;
    void add_target(uint64_t lat) {target.assign_slot(lat);}
    void add_sched(uint64_t lat) {sched.assign_slot(lat);}
    void merge(Latency &lat) {
      target.merge_slots(lat.target);
      sched.merge_slots(lat.sched);
    }
    void print_summary();
  };
  struct LatencyChild {
    Bucket target;
    Bucket sched;
    void clear() {
      target.clear();
      sched.clear();
    }
    bool empty() { return target.empty() && sched.empty();}
    void add_target(const std::string &name, uint64_t lat) {
      target.add_val(name, lat);
    }
    void add_sched(const std::string &name, uint64_t lat) {
      if (lat == 0) return;
      sched.add_val(name, lat);
    }
    void add_child(LatencyChild &child) {
      target.add_bucket(child.target);
      sched.add_bucket(child.sched);
    }
    void merge(LatencyChild &child) { add_child(child);}
    void print_summary();
  };
  struct LatencyCaller {
    Latency latency;
    LatencyChild children;
    void add_target(uint64_t lat) {latency.add_target(lat);}
    void add_sched(uint64_t lat) {latency.add_sched(lat);}
    void add_child(LatencyChild &c) { children.add_child(c); }
    void merge(LatencyCaller &caller) {
      latency.merge(caller.latency);
      children.merge(caller.children);
    }
  };
  Stat() : sched_count(0), timeline_unit_lat(0), timeline_unit(0) {}
  /* Latency */
  Latency latency;
  /* Child Latency */
  LatencyChild children;
  /* Latency divided by Caller */
  std::unordered_map<std::string, LatencyCaller> callers;
  /* schedule count */
  uint64_t sched_count;

  /* Latency timeline */
  std::vector<std::vector<long double>> timeline;
  uint64_t timeline_unit_lat;
  uint32_t timeline_unit;

  void add_latency(uint64_t lat_t, uint64_t lat_s, const std::string &caller) {
    latency.add_target(lat_t);
    callers[caller].add_target(lat_t);
    if (lat_s) {
      latency.add_sched(lat_s);
      callers[caller].add_sched(lat_s);
    }
  }
  void add_child_latency(LatencyChild &child, const std::string &caller) {
    children.add_child(child);
    callers[caller].add_child(child);
  }
  
  void add(Action &action_call, Action &action_return, uint64_t lat_s, LatencyChild &child) { 
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
      add_child_latency(child, caller);
    }
  }

  void merge(Stat &stat) {
    latency.merge(stat.latency);
    children.merge(stat.children);
    for (auto &it : stat.callers) {
      LatencyCaller &caller = it.second;
      callers[it.first].merge(caller);
    }
    sched_count += stat.sched_count;
  }
  void print_summary();
  void print_timeline();
};

/* parse action from file */
class ParseJob : public ParallelJob {
public:
  struct ActionSet {
    long tid;
    uint32_t target;
    std::vector<Action> actions;
    void add_action(Action &action) {
      if (action.is_error && !actions.empty() && actions.back().is_error) {
        // no need to add the same trace error
        return;
      }
      assert(actions.empty() || action.ts >= actions.back().ts);
      actions.emplace_back(action);
    }
    Action &operator[] (uint32_t i) { return actions[i]; }
    size_t size() { return actions.size(); }
    ActionSet() : target(0) {}
  };

  ParseJob(const std::string &name, uint32_t f, uint32_t t, uint32_t i) 
    : filename(name), from(f), to(t),
      start_time(UINT64_MAX), id(i) {}

  void exec() override {
    decode_to_actions();
  }

  void add_action(Action &a) {
    ActionSet &as = parsed_actions[a.tid];
    as.tid = a.tid;
    as.add_action(a);
    if (a.from_target || a.to_target)
      ++as.target;
  }

  void decode_to_actions();
  size_t get_actions_num(long tid) {
    return parsed_actions[tid].size();
  }
  Action &get_action(long tid, uint32_t idx) {
    return parsed_actions[tid][idx];
  }
  bool has_parsed(long tid) {
    return parsed_actions.count(tid) > 0;
  }
  uint64_t get_start_time() { return start_time; }

  template <typename Func>
  void loop_parsed_actions(Func f) {
    for (auto it = parsed_actions.begin();
            it != parsed_actions.end(); ++it) {
      f(it->second);
    }
  }
  friend class ThreadJob;

private:
  std::string filename;
  uint32_t from;
  uint32_t to;
  uint32_t total;
  uint64_t start_time;
  uint32_t id;

  // store decoded acitions grouped by thread
  std::unordered_map<long, ActionSet> parsed_actions;
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

  Stat &get_stat() { return stat; }

private:
  std::vector<ParseJob *> *parse_jobs_ptr;
  std::vector<Action> actions;
  Stat stat;
  long tid;
};

class SrclineMap {
public:
  bool get(const std::string &function, std::string &srcline);
  uint64_t get(const std::string &function);
  void put(const std::string &function, uint64_t addr);
  void process_address();
private:
  std::unordered_map<std::string, uint64_t> addr_map;
  std::unordered_map<std::string, std::string> srcline_map;
  std::shared_mutex srcline_mutex;
};

#endif
