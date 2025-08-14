#include <stdio.h>
#include <cmath>
#include <cstring>
#include <assert.h>
#include <algorithm>

#include "graphs.h"
#include "stat_tools.h"

using namespace std;


const std::string FuncStat::unknown_latency_str = "(unknown latency) ";

static uint32_t global_print_width = 100;
void print_cross_line(char c) {
  printf("\n\033[33m");
  for (int i = 0; i < global_print_width; ++i) printf("%c", c);
  printf("\033[0m\n");
}
void print_title(const char *title) {
  printf("\033[32m%s\033[0m\n", title);
}

static void print_stars(uint64_t val, uint64_t val_max, int width)
{
  int num_stars, num_spaces, i;
  bool need_plus;

  if (val_max > 0)
    num_stars = min(val, val_max) * width / val_max;
  else
    num_stars = 0;
  num_spaces = width - num_stars;
  need_plus = val > val_max;

  for (i = 0; i < num_stars; i++)
    printf("*");
  for (i = 0; i < num_spaces; i++)
    printf(" ");
  if (need_plus)
    printf("+");
}

static void print_multi_line_text(uint32_t max_width,
    uint32_t off, const std::string &text) {
  int width = max_width - off;
  int len = text.size();
  int left = len;
  while (left > 0) {
    if (left != len) printf("%*s", off, "");
    printf("%s\n", text.substr(len - left, width).c_str());
    left -= width;
  }
}

uint32_t HistogramBucket::get_print_width(uint32_t bucket_num) {
  uint32_t print_width = 0;
  print_width += max_key_length;
  print_width += 23;
  print_width += (22 * (bucket_num - 1));
  print_width += 22;
  return print_width;
}

void HistogramBucket::print() {
  if (keys.size() == 0) {
    return;
  }

  max_key_length = (max_key_length + 1) / 2 * 2 + 2;
  printf("%*s%-*s : %-*s %-*s", max_key_length / 2,
          "", max_key_length / 2, "name",
         10, "avg", 10, "cnt");
  uint32_t max_print_width = max_key_length + 3 + 21;
  for (Bucket *bucket : extra_buckets) {
    max_print_width += (bucket->get_width() + 1);
    bucket->print_val_name();
  }
  max_print_width += 22;
  printf(" %-*s ", 20, "distribution (total)");
  printf("\n");
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    Bucket::Element &el = *it;
    const string &name = el.name;
    uint64_t avg = el.get_avg();
    /* 1. print name */
    el.val_str = "| " + el.val_str;
    int val_len = el.val_str.size();
    if (val_len > max_key_length) {
      // name is too long
      string part;
      part = el.val_str.substr(0, max_key_length);
      printf("%s\n", part.c_str());

      int width = max_key_length - 4;
      int i = val_len - max_key_length;
      for (; i > width; i -= width) {
        part = "    " + el.val_str.substr(val_len - i, width);
        printf("%s\n", part.c_str());
      }
      part =
        el.val_str.substr(val_len - i, width);
      printf("    %-*s : ", width, part.c_str());
    } else {
      printf("%-*s : ", max_key_length,
          el.val_str.substr(0, max_key_length).c_str());
    }
    if (!el.err_msg.empty()) {
      print_multi_line_text(max_print_width, max_key_length + 3, el.err_msg);
      continue;
    }
    /* 2. print avg and cnt */
    if (avg >= INTEGER_TEN_ZEROS) {
      string notation =
        std::to_string(avg*10/INTEGER_TEN_ZEROS) + "e9";
      string print_flag = "%-10s %-10lu";
      printf(print_flag.c_str(), notation.c_str(), el.count);
    } else {
      string print_flag = "%-10lu %-10lu";
      printf(print_flag.c_str(), avg, el.count);
    }
    /* 3. print each bucket */
    for (Bucket *bucket : extra_buckets) {
      if (bucket->slots.count(name)) {
        Bucket::Element &extra = bucket->slots[name];
        extra.print();
      } else {
        printf(" %-10d", 0);
      }
    }
    printf("|");
    /* 4. print distribution */
    print_stars(el.total, total_max, 20);
    printf("|\n");
  }
}

void Distribution::assign_slot(uint64_t val) {
  int i;
  for (i = 0;;++i) {
    uint64_t low = (1ULL << (i + 1)) >> 1;
    uint64_t high = (1ULL << (i + 1)) - 1;
    if (low == high) 
      low -= 1;
    if (val <= high && val >= low)
      break;
  }

  if (slots.size() < i + 1)
    slots.resize(i + 1, 0);
  ++slots[i];
  total += val;
  ++count;
}

void Distribution::merge_slots(Distribution &dist) {
  size_t slot_size = dist.slots.size();
  if (slots.size() < slot_size)
    slots.resize(slot_size, 0);
  for (size_t i=0; i < slot_size; ++i) {
    slots[i] += dist.slots[i];
  }
  total += dist.total;
  count += dist.count;
}

void Distribution::init_val_max() {
  val_max = 0;
  for (size_t i=0; i<slots.size(); ++i) {
    if (slots[i] > val_max) {
      val_max = slots[i];
    }
  }
}

uint32_t HistogramDist::get_print_width(uint32_t dist_num) {
  uint32_t print_width = 0;
  print_width += 27;
  print_width += (31 * dist_num);
  return print_width;
}

void HistogramDist::print() {
  if (slot_size == 0) return;
 
  /* resize all Distribution to max size*/
  for (auto &dist : dists) {
    dist.init_val_max();
    dist.slots.resize(slot_size, 0);
  }

  /* Histogram header */
  printf("%*s%-*s : ", 10, "", 14, m_unit.c_str());
  for (auto &dist : dists) {
    printf("%-*s %-*s", 10, dist.val_name.c_str(), 20, "distribution");
  }
  printf("\n");
  bool skip = true;
  for (uint32_t i = 0; i < slot_size; i++) {
    uint64_t low = (1ULL << (i + 1)) >> 1;
    uint64_t high = (1ULL << (i + 1)) - 1;
    if (low == high) {
      low -= 1;
    }
    bool all_zero = true;
    for (auto &dist : dists) {
      if (dist.slots[i] > 0) {
        all_zero = false;
      }
    }
    if (all_zero && skip)
      continue;
    skip = false;

    printf("%*lld -> %-*lld :", 10, low, 10, high);
    for (auto &dist : dists) {
      printf(" %-9d|", dist.slots[i]);
      print_stars(dist.slots[i], dist.val_max, 20);
      printf("|");
    }
    printf("\n");
  }
}

void FuncStat::init_print_width() {
  uint32_t num = 1;
  if (opt.offcpu) num += 1;
  if (opt.call_line) num += 1;
  if (opt.code_block) num += 1;
  HistogramBucket hist_b;
  hist_b.init_key(children.target, [&](const std::string &name,
        Bucket::Element &el) -> std::string {
    return funcname_get_name(name);
  });
	global_print_width = 100;
  global_print_width = std::max(HistogramDist::get_print_width(num), global_print_width);
  global_print_width = std::max(hist_b.get_print_width(num), global_print_width);
}

void FuncStat::add(Action &action_call, Action &action_return,
		uint64_t lat_s, LatencyChild &child) {
  uint64_t lat_t = action_return.ts - action_call.ts;
  std::string caller = action_return.to->name;
  if (opt.call_line) {
    if (opt.ip_filtering) {
      /* for ipfiltering, action_call is empty */
      funcname_add_addr(caller, action_return.to->addr);
    } else {
      funcname_add_addr(caller, action_call.from->addr);
    }
  }
  if (lat_t < opt.latency_interval.first ||
			lat_t > opt.latency_interval.second ||
      action_call.ts < opt.time_interval.first ||
			action_call.ts > opt.time_interval.second) {
    return;
  }

  if (opt.timeline && action_call.ts >= opt.time_start) {
    timeline_unit_lat += lat_t;
    if (++timeline_unit == opt.timeline_unit) {
      // caculate the average for one timeline_unit
      timeline.push_back({(action_call.ts - opt.time_start) / 1000.0,
                           timeline_unit_lat / timeline_unit / 1000.0}); // us
      timeline_unit_lat = timeline_unit = 0;
    }
  } else {
    add_latency(lat_t, lat_s, caller);
    if (!opt.code_block) {
      // add latency of target function self
      child.add_target(TARGET_SELF,
          lat_t > child.target_total ? lat_t - child.target_total : 0);
      child.add_sched(TARGET_SELF,
          lat_s > child.sched_total ? lat_s - child.sched_total : 0);
    }
    add_child_latency(child, caller);
  }
}

void FuncStat::print_latency(FuncStat::Latency &latency) {
	/* print latency distribution of target and schedule */
  HistogramDist hist("ns");

  latency.target.set_val_name("cnt");
  hist.add_dist(latency.target);
  if (opt.offcpu) {
    latency.sched.set_val_name("sched");
    hist.add_dist(latency.sched);
  }
  hist.print();

	/* print summary text of latency */
  uint64_t target_avg = latency.target.get_avg();
  uint64_t target_cnt = latency.target.get_count();
  uint64_t sched_total = latency.sched.get_total();
  uint64_t sched_cnt = latency.sched.get_count();

  uint32_t width1 =
    floor(log10(sched_cnt + target_cnt + 1)) + 1;
  uint32_t width2 =
    floor(log10(target_avg + 1)) + 1;

  printf("trace count: %*lu, average latency: %*lu ns\n",
          width1, target_cnt + latency.unknown_count, width2, target_avg);
  if (opt.offcpu) {
    uint64_t sched_avg =
      target_cnt > 0 ? sched_total / target_cnt : 0;
    int cpu_pct =
         100 * ((target_avg - sched_avg) * target_cnt) / opt.trace_time;
    printf("sched count: %*lu,   sched latency: %*lu ns, cpu percent: %d \%\n",
          width1, sched_cnt, width2, sched_avg, cpu_pct);
  }
}

void FuncStat::print_child(FuncStat::LatencyChild &child) {
  HistogramBucket hist;

  Bucket oncpu("cpu_pct(%)");
  Bucket call_line("call_line");
  hist.init_key(child.target, [&](const std::string &name,
        Bucket::Element &el) -> std::string {
    if (name.find(FuncStat::unknown_latency_str) != std::string::npos) {
      char err_msg[256];
      snprintf(err_msg, 256, "%-10s %-10lu", "unknown", el.count);
      srcline_map->get(el.name, el.val_str);
      el.err_msg = std::string(err_msg) + " " + el.val_str +
                      "  incomplete call/return, trace it directly";
      return funcname_get_name(
          name.substr(FuncStat::unknown_latency_str.size(), name.size()));
    }
    return funcname_get_name(name);
  });
  if (opt.call_line) {
    // add srcline to all buckets
    call_line.add_bucket(child.target);
    int width = 10;
    call_line.loop_for_element([&](Bucket::Element &el){
      if (el.name.find(CODE_BLOCK_PREFIX) != string::npos) {
        // 'code block' latency needs the begin and end address
        string srcline_from, srcline_to;
        srcline_map->get(el.name + "_from", srcline_from);
        srcline_map->get(el.name + "_to", srcline_to);
        el.val_str = srcline_from + "-" + srcline_to;
      } else if (el.name.find(GATHER_CALL_LINE) != string::npos) {
        el.val_str = "[gathered]";
      } else if (el.name == TARGET_SELF) {
        srcline_map->get(opt.target, el.val_str);
      } else {
        srcline_map->get(el.name, el.val_str);
      }
      if (el.val_str.size() > width)
        width = el.val_str.size();
    });
    call_line.set_width(width + 1);
    hist.add_extra_bucket(&call_line);
  }
  if (opt.offcpu) {
    child.sched.set_val_name("sched_time");
    child.sched.set_count(child.target);
    hist.add_extra_bucket(&child.sched);

    oncpu.add_bucket(child.target);
    oncpu.sub_bucket(child.sched);
    oncpu.set_count(1);
    oncpu.set_scale(opt.trace_time / 100);
    hist.add_extra_bucket(&oncpu);
  }

  hist.print();
}

void FuncStat::add_addr_from_funcname(const std::string &name) {
  if (name.find(GATHER_CALL_LINE) != string::npos) {
    return;
  }
  uint64_t addr = funcname_get_addr(name);
  if (addr > 0) {
    srcline_map->put(name, addr);
  }
}

void FuncStat::generate_srcline() {
  // put all callers
  for (auto it = callers.begin(); it != callers.end(); ++it) {
    LatencyCaller &caller = it->second;
    add_addr_from_funcname(it->first);
    caller.children.target.loop_for_element([&](Bucket::Element &el){
        add_addr_from_funcname(el.name);
    });
  }
  // put all children
  children.target.loop_for_element([&](Bucket::Element &el){
      add_addr_from_funcname(el.name);
  });
  // generate srcline
  srcline_map->process_address();
}

void FuncStat::print() {
  char title[1024];

	init_print_width();
  if (opt.call_line) {
    generate_srcline();
  }
  bool has_complete_target =
        (latency.target.get_total() > 0);
  /* print gathered target function's latency */
  if (has_complete_target) {
    print_cross_line('=');
    snprintf(title, 1024, "Histogram - Latency of [%s]:",
             opt.target.c_str());
    print_title(title);
    print_latency(latency);

    if (opt.offcpu) {
      printf("sched total: %lu, sched each time: %lu ns\n", sched_count,
             sched_count > 0 ? (latency.sched.get_total() / sched_count) : 0);
    }
    print_cross_line('-');

    /* print child function's latency  */
    snprintf(title, 1024,
             "Histogram - Child functions's Latency of [%s]:",
             opt.target.c_str());
    print_title(title);
		print_child(children);
  }

  for (auto &it : callers) {
    string caller_name = it.first;
    auto &caller = it.second;
    if (opt.call_line) {
      string srcline;
      srcline_map->get(caller_name, srcline);
      caller_name = funcname_get_name(caller_name) + "(" + srcline + ")";
    }
		/* no need to show unknown latency if has complete target */
    if (has_complete_target && it.first == "unknown")
      continue;

    /* target function's latency from current caller */
    print_cross_line('=');
    if (it.first != "unknown" && caller.latency.target.get_count()) {
      snprintf(title, 1024, "Histogram - Latency of [%s]\n"
             								"           called from [%s]:",
             opt.target.c_str(), caller_name.c_str());
      print_title(title);
      print_latency(caller.latency);
      print_cross_line('-');
    }

    /* print child function's latency from caller */
    snprintf(title, 1024,
             "Histogram - Child functions's Latency of [%s]\n"
             "                             called from [%s]:",
             opt.target.c_str(), caller_name.c_str());
    print_title(title);
		print_child(caller.children);
  }
  print_cross_line('=');
}

void FuncStat::print_timeline() {
  graphs::options gopt;
  gopt.type = graphs::type_braille;
  gopt.mark = graphs::mark_dot;
  gopt.style = graphs::style_light;
  gopt.check = false;
  gopt.color = graphs::color_default;
  gopt.xlabel = "x(us)";
  gopt.ylabel = "y(us)";
  //gopt.yexponent = true;
  printf("start_timestamp: %lld\n", opt.time_start);
  if (timeline.size() > 0)
    graphs::plot(100, 160, 0, 0, 0, 0, timeline, gopt);
}

void FuncGlobalStatus::print(size_t thread_num, const std::string &ancestor) {
  printf("[ real trace time: %0.2f seconds ]\n", real_trace_time());

  if (thread_num)
    miss.store(miss.load() / thread_num);
  printf("[ miss trace time: %0.2f seconds ]\n", (double)miss.load() / NSECS_PER_SECS);

  if (ancestor != "") {
    char title[1024];
    snprintf(title, 1024,
        "[ ancestor: %s, call: %llu, return: %llu ]", ancestor.c_str(),
        ancestor_begin.load(), ancestor_end.load());
    print_title(title);
  }
}
