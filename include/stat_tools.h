#ifndef _h_stat_tool_
#define _h_stat_tool_
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>

class HistogramBucket;
class Bucket {
public:
  Bucket() : val_name("") {}
  Bucket(const std::string &name) : val_name(name) {}
  struct Element {
    std::string name;
    uint64_t count;
    uint64_t total;
    double scale;
    Element() : count(0), total(0), scale(0.0) {}
    uint64_t get_avg() {
      if (!count) return 0;
      return (total / count);
    }
    bool operator<(Element &el) {
      return get_avg() > el.get_avg();
    }
    void print() {
      if (scale)
        printf(" %-10.2f", get_avg() / scale);
      else
        printf(" %-10d", get_avg());
    }
  };

  typedef std::unordered_map<std::string, Element> Slot;

  void add_val(const std::string &key, uint64_t val) {
    slots[key].name = key;
    slots[key].total += val;
    slots[key].count++;
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
  void set_val_name(const std::string &name) {
    val_name = name;
  }
  void print_val_name() {
    printf(" %-*s", 10, val_name.substr(0, 10).c_str());
  }
  bool empty() { return slots.size() == 0; }
  void clear() { slots.clear(); }

  friend class HistogramBucket;
private:
  Slot slots;
  std::string val_name;
};

class HistogramBucket {
public:
  HistogramBucket() : total_max(0) {}
  void init_key(Bucket &b);
  void add_extra_bucket(Bucket *b) {
    extra_buckets.emplace_back(b);
  }
  static uint32_t get_print_width(uint32_t extra_num);
  void print();
private:
  std::vector<Bucket::Element> keys;
  uint64_t total_max;
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

#endif