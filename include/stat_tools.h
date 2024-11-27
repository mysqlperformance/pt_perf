#ifndef _h_stat_tool_
#define _h_stat_tool_
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>

#include "sys_tools.h"
#include "pt_action.h"

#define INTEGER_TEN_ZEROS 10000000000UL

class HistogramBucket;
class Bucket {
public:
  Bucket() : val_name(""), width(10) {}
  Bucket(const std::string &name) : val_name(name), width(10) {}
  struct Element {
    std::string name;
    uint64_t count;
    uint64_t total;
    double scale;
    std::string val_str;
    std::string err_msg;
    int width;
    Element() : count(0), total(0), scale(0.0),
                val_str(""), err_msg(""), width(10) {}
    uint64_t get_avg() {
      if (!count) return 0;
      return (total / count);
    }
    bool operator<(Element &el) {
      return get_avg() > el.get_avg();
    }
    void print() {
      if (val_str != "")
        printf(" %-*s", width, val_str.substr(0, width).c_str());
      else if (scale)
        printf(" %-10.2f", get_avg() / scale);
      else {
        uint64_t avg = get_avg();
        if (avg >= INTEGER_TEN_ZEROS) {
          printf(" %-7uE10", avg / INTEGER_TEN_ZEROS);
        } else {
          printf(" %-10lu", avg);
        }
      }
    }
  };

  typedef std::unordered_map<std::string, Element> Slot;

  void add_val(const std::string &key, uint64_t val) {
    slots[key].name = key;
    slots[key].total += val;
    slots[key].count++;
  }
  void add_val(const std::string &key, const std::string &val) {
    slots[key].name = key;
    slots[key].val_str = val;
    if (val.size() > width) {
      width = val.size();
    }
  }
  void add_bucket(Bucket &b) {
    for (auto it = b.slots.begin(); it != b.slots.end(); ++it) {
      slots[it->first].count += it->second.count;
      slots[it->first].total += it->second.total;
      slots[it->first].name = it->first;
    }
  }
  void sub_bucket(Bucket &b) {
    for (auto it = b.slots.begin(); it != b.slots.end(); ++it) {
      if (slots.count(it->first) == 0)
        continue;
      slots[it->first].count -= it->second.count;
      slots[it->first].total -= it->second.total;
      slots[it->first].name = it->first;
    }
  }
  void set_scale(double scale) {
    for (auto it = slots.begin(); it != slots.end(); ++it) {
      it->second.scale = scale;
    }
  }
  void set_count(double cnt) {
    for (auto it = slots.begin(); it != slots.end(); ++it) {
      it->second.count = cnt;
    }
  }
  void set_count(Bucket &b) {
    for (auto it = b.slots.begin(); it != b.slots.end(); ++it) {
      if (slots.count(it->first) == 0)
        continue;
      slots[it->first].count = it->second.count;
    }
  }
  void set_width(int w) {
    width = w;
    for (auto it = slots.begin(); it != slots.end(); ++it) {
      it->second.width = w;
    }
  }
  uint32_t get_width() {
    return width;
  }
  void set_val_name(const std::string &name) {
    val_name = name;
  }
  void print_val_name() {
    printf(" %-*s", width, val_name.substr(0, width).c_str());
  }
  bool empty() { return slots.size() == 0; }
  void clear() { slots.clear(); }

  template<typename Func>
  void loop_for_element(Func f) {
    for (auto it = slots.begin(); it != slots.end(); ++it) {
      f(it->second);
    }
  }

  friend class HistogramBucket;
private:
  Slot slots;
  std::string val_name;
  uint32_t width;
};

class HistogramBucket {
public:
  HistogramBucket() : total_max(0), max_key_length(10) {}

  template <typename Function>
  void init_key(Bucket &b, Function f) {
    for (auto it = b.slots.begin(); it != b.slots.end(); ++it) {
      Bucket::Element &el = it->second;
      if (el.total > total_max) {
        total_max = el.total;
      }
      el.name = it->first;
      el.val_str = f(el.name, el);
      if (el.val_str.size() > max_key_length)
        max_key_length = el.val_str.size();
      keys.push_back(el);
    }
    if (max_key_length > 35) max_key_length = 35;
    std::sort(keys.begin(), keys.end());
  }
  void add_extra_bucket(Bucket *b) {
    extra_buckets.emplace_back(b);
  }
  uint32_t get_print_width(uint32_t extra_num);
  void print();
private:
  std::vector<Bucket::Element> keys;
  uint64_t total_max;
  size_t max_key_length;
  std::vector<Bucket *> extra_buckets;
};

class HistogramDist;
class Distribution {
public:
  Distribution() : total(0), count(0), val_max(0), val_name("") {}
  void assign_slot(uint64_t val);
  void merge_slots(Distribution &dist);
  void init_val_max();
  void set_val_name(const std::string &name) {
    val_name = name;
  }
  uint64_t get_total() { return total; }
  uint64_t get_avg() {
    if (!count) return 0;
    return total / count;
  }
  uint32_t get_count() { return count; }

  friend class HistogramDist;
private:
  uint64_t total;
  uint32_t count;
  uint32_t val_max;
  std::string val_name;
  std::vector<uint32_t> slots;
};

class HistogramDist {
public:
  HistogramDist(const std::string &unit) : slot_size(0), m_unit(unit) {}
  void add_dist(Distribution &dist) {
    if (dist.slots.size() > slot_size) {
      slot_size = dist.slots.size();
    }
    dists.emplace_back(dist);
  }
  static uint32_t get_print_width(uint32_t dist_num);
  void print();
private:
  std::string m_unit;
  uint32_t slot_size;
  std::vector<Distribution> dists;
};

using namespace pt;
#define TARGET_SELF "*self"
#define CODE_BLOCK_PREFIX  "*code block: "

class FuncStat {
public:
  static const std::string unknown_latency_str;
	struct Option {
    std::string target;
    bool offcpu;
    bool call_line;
    bool code_block;
    std::pair<uint64_t, uint64_t> latency_interval;
    std::pair<uint64_t, uint64_t> time_interval;
    uint64_t trace_time;
    bool timeline;
    uint64_t time_start;
    uint32_t timeline_unit;
    bool ip_filtering;
	};
  struct Latency {
    Distribution target;
    Distribution sched;
    void add_target(uint64_t lat) {target.assign_slot(lat);}
    void add_sched(uint64_t lat) {sched.assign_slot(lat);}
    void merge(Latency &lat) {
      target.merge_slots(lat.target);
      sched.merge_slots(lat.sched);
    }
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
  FuncStat(Option o, SrclineMap *s) : opt(o), srcline_map(s),
      sched_count(0), timeline_unit_lat(0), timeline_unit(0) {}
  FuncStat() : srcline_map(nullptr),
      sched_count(0), timeline_unit_lat(0), timeline_unit(0) {}
  /* option */
  Option opt;
  /* srcline map */
  SrclineMap *srcline_map;

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
  void add_child_latency(LatencyChild &child,
      const std::string &caller, bool gather = true) {
    if (gather)
      children.add_child(child);
    callers[caller].add_child(child);
  }
  
  void add(Action &action_call, Action &action_return, uint64_t lat_s, LatencyChild &child);

  void merge(FuncStat &stat) {
    latency.merge(stat.latency);
    children.merge(stat.children);
    for (auto &it : stat.callers) {
      LatencyCaller &caller = it.second;
      callers[it.first].merge(caller);
    }
    sched_count += stat.sched_count;
  }

  void init_print_width();
  void generate_srcline();
  void print_latency(FuncStat::Latency &latency);
  void print_child(FuncStat::LatencyChild &child);
  void print();
  void print_timeline();
};

struct FuncGlobalStatus {
  /* the missing trace time because of the lost data  */
  std::atomic<uint64_t> miss;
  /* the real trace time based on minimum and
   * maximum timestamp of actions */
  std::pair<uint64_t, uint64_t> real;
  std::atomic<uint64_t> ancestor_begin;
  std::atomic<uint64_t> ancestor_end;

  void update_real(ActionSet &as) {
    real.first = std::min(real.first, as.min_timestamp());
    real.second = std::max(real.second, as.max_timestamp());
  }

  double real_trace_time() {
    // caculate the real trace time base on all actions timestamp
    if (real.second > real.first) {
      double real_trace_time =
            (double)(real.second - real.first) / NSECS_PER_SECS;
      return real_trace_time;
    }
    return 0;
  }

  void print(size_t thread_num, const std::string &ancestor);
  FuncGlobalStatus() : miss(0), real({UINT64_MAX, 0}),
    ancestor_begin(0), ancestor_end(0) {}
};


void print_cross_line(char c);
void print_title(const char *title);
#endif
