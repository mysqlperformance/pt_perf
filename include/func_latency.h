#ifndef _h_func_latency_
#define _h_func_latency_
#include <string>
#include <vector>
#include <unordered_map>

#include "stat_tools.h"

#define PT_HOME_PATH "/sys/devices/intel_pt"
#define PT_IP_FILTER_PATH "/sys/devices/intel_pt/caps/ip_filtering"
#define PT_FRAME_PATH "/usr/share/pt_flame"
#define NSECS_PER_SECS 1000000000UL

struct Param {
  std::string perf_tool;
  std::string binary;
  std::string target;
  float trace_time;
  long pid;
  long tid;
  bool verbose;
  bool parallel_script;
  bool per_thread_mode;
  size_t worker_num;
  bool ip_filtering;
  std::string func_idx;
  bool offcpu;
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
  Param();
};
extern Param param;

struct Symbol {
  std::string name;
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
};

class Parser;
/* Profile thread class */
class Thread {
public:
  Thread() : thr(nullptr), target_count(0) {}
  ~Thread() {
    if(thr) {
      delete thr;
      thr = nullptr;
    }
  }
  void add_action(Action &action) {
    if (!actions.empty() && actions.back().is_error) {
      // no need to add the same trace error
      return;
    }
    assert(actions.empty() || action.ts >= actions.back().ts);
    actions.emplace_back(action); 
  }
  long get_tid() { return tid; }
  void set_tid(long t) { tid = t; }
  void add_actions_from_parser(std::vector<Parser> &parsers);
  void perf_func();

  static void thread_func(Thread *t, std::vector<Parser> *parsers_ptr) {
    t->add_actions_from_parser(*parsers_ptr);
    t->perf_func();
  }
  void parse(std::vector<Parser> &parsers) {
    thr = new std::thread(thread_func, this, &parsers);
  }
  void join() {
    if(thr) thr->join();
  }

  Stat &get_stat() { return stat; }
  void inc_target() { ++target_count; }
  uint32_t target() { return target_count; }

  std::vector<Action> actions;
  
private:
  Stat stat;
  long tid;
  std::thread *thr;
  uint32_t target_count; // action count of target function
};
typedef std::unordered_map<long, Thread> Thread_map;

/* parse action from file */
class Parser {
public:
  Parser(const std::string &name, uint32_t f, uint32_t t, uint32_t i) 
    : filename(name), from(f), to(t),
      start_time(UINT64_MAX), thr(nullptr), id(i) {}
  ~Parser() {
    if(thr) {
      delete thr;
      thr = nullptr;
    }
  }
  static void thread_func(Parser *p) {
    p->decode_to_actions();
  }
  void parse() {
    thr = new std::thread(thread_func, this);
  }
  void join() {
    if(thr) thr->join();
  }
  void decode_to_actions();
  size_t get_actions_num(long tid) {
    return threads[tid].actions.size();
  }
  Action &get_action(long tid, uint32_t idx) {
    return threads[tid].actions[idx];
  }
  bool has_parsed(long tid) {
    return threads.count(tid) > 0;
  }
  uint64_t get_start_time() { return start_time; }
  Thread_map threads;

private:
  std::string filename;
  uint32_t from;
  uint32_t to;
  uint32_t total;
  uint64_t start_time;

  std::thread *thr;
  uint32_t id;
};
#endif
