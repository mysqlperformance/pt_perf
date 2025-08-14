#ifndef _h_stat_tool_
#define _h_stat_tool_
#include <stdint.h>
#include <string>
#include <vector>

#define HIST_RESERVE_SLOT 64
class Histogram {
public:
  Histogram();
  void add_val(uint64_t val) { arr.emplace_back(val); }
  void set_name(const std::string &hist_name) {
    name = hist_name;
  }
  void init_slot();
  void print_log2(const char *val_type);
  void print(std::vector<std::string> &names, std::vector<uint32_t> &val);

private:
  std::vector<uint64_t> arr;

  std::string name;
  uint64_t total;
  uint32_t count;
  std::vector<uint32_t> slots;
  uint32_t slot_size;
};

#endif
