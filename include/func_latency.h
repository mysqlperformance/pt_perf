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
  bool srcline;
  bool call_line;

  std::string ancestor;
  std::pair<uint64_t, uint64_t> ancestor_latency;

  bool code_block;

  bool timeline;
  uint32_t timeline_unit;
  std::pair<uint64_t, uint64_t> latency_interval;
  std::pair<uint64_t, uint64_t> time_interval;
  uint64_t time_start;
  int history;
  std::string sched_funcname; // for linux 510 higher
  std::string sched_funcname_419; // for linux 419
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
  uint32_t offset;
  bool equal(struct Symbol *sym) {
    uint64_t func_addr1 = addr - offset;
    uint64_t func_addr2 = sym->addr - sym->offset;
    return (func_addr1 == func_addr2);
  }
};

struct Action {
  enum ActionType : uint8_t {
    TR_START = 0,
    TR_END_CALL,
    TR_END_RETURN,
    TR_END_HW_INT,
    TR_END_SYSCALL,
    TR_END,
    CALL,
    RETURN,
    SYSCALL,
    JMP,
    JCC,
    HW_INT,
    IRET
  };
  static Symbol get_symbol(const std::string &str, Symbol *caller);
  void init_for_sched() {
    if ((to.name == param.sched_funcname || to.name == param.sched_funcname_419)
        && to.offset == 0 && (type == Action::CALL || type == Action::TR_START)) {
      sched_begin = true;
    } else if ((from.name == param.sched_funcname || from.name == param.sched_funcname_419) &&
               (type == Action::RETURN || type == Action::TR_END_RETURN)) {
      sched_end = true;
    }
  }

  void init_for_ancestor() {
    if (to.name == param.ancestor && to.offset == 0 &&
        (type == Action::CALL || type == Action::TR_START)) {
      ancestor_begin = true;
    } else if (from.name == param.ancestor && (type == Action::RETURN ||
                type == Action::TR_END_RETURN)) {
      ancestor_end = true;
    }
  }

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
  bool sched_begin;  // begin of schedule
  bool sched_end;    // end of schedule
  bool ancestor_begin; // begin of ancestor func
  bool ancestor_end;   // end of ancestor func
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
    uint64_t target_total;
    uint64_t sched_total;
    LatencyChild() : target_total(0), sched_total(0) {}
    void clear() {
      target_total = sched_total = 0;
      target.clear();
      sched.clear();
    }
    bool empty() { return target.empty() && sched.empty();}
    void add_target(const std::string &name, uint64_t lat) {
      target.add_val(name, lat);
      target_total += lat;
    }
    void add_sched(const std::string &name, uint64_t lat) {
      if (lat == 0) return;
      sched.add_val(name, lat);
      sched_total += lat;
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
  
  void add(Action &action_call, Action &action_return, uint64_t lat_s, LatencyChild &child);

  void merge(Stat &stat) {
    latency.merge(stat.latency);
    children.merge(stat.children);
    for (auto &it : stat.callers) {
      LatencyCaller &caller = it.second;
      callers[it.first].merge(caller);
    }
    sched_count += stat.sched_count;
  }
  void generate_srcline();
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
      assert(actions.empty() || action.ts >= actions.back().ts || action.is_error);
      actions.emplace_back(action);
    }
    Action &operator[] (uint32_t i) { return actions[i]; }
    size_t size() { return actions.size(); }
    static inline bool cmp_action(const Action &a1, const Action &a2) {
      return a1.ts < a2.ts;
    }
    void sort() { std::sort(actions.begin(), actions.end(), cmp_action); }
    uint64_t min_timestamp() { return actions.empty() ? UINT64_MAX : actions[0].ts; }
    uint64_t max_timestamp() { return actions.empty() ? 0 : actions.back().ts; }
    ActionSet() : target(0) {}
  };

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
    // now only error actions may be out-of-order
    for (auto it = error_actions.begin(); it != error_actions.end(); ++it) {
      ActionSet &as = it->second;
      as.sort();
    }
  }
  size_t parsed_actions_num(long tid) {
    if (parsed_actions.count(tid) > 0)
      return parsed_actions[tid].size();
    return 0;
  }
  Action *get_parsed_action(long tid, uint32_t idx) {
    if (parsed_actions.count(tid) > 0)
      return &parsed_actions[tid][idx];
    return nullptr;
  }
  size_t error_actions_num(long tid) {
    if (error_actions.count(tid) > 0)
      return error_actions[tid].size();
    return 0;
  }
  Action *get_error_action(long tid, uint32_t idx) {
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
  RwSpinLock m_lock;
};

inline void funcname_add_addr(std::string &name, uint64_t addr) {
  name += ("|" + std::to_string(addr));
}
inline uint64_t funcname_get_addr(const std::string &name) {
  size_t sep = name.find_first_of('|');
  if (sep == std::string::npos)
    return 0;
  return stol(name.substr(sep + 1, name.size() - sep + 1));
}
inline std::string funcname_get_name(const std::string &name) {
  size_t sep = name.find_first_of('|');
  if (sep == std::string::npos)
    return name;
  return name.substr(0, sep);
}

#endif
