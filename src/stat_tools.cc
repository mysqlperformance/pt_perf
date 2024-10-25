#include <stdio.h>
#include <cmath>
#include <cstring>
#include <assert.h>
#include <algorithm>

#include "stat_tools.h"

using namespace std;

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
    string print_flag = (avg >= INTEGER_TEN_ZEROS) ?
                      "%-10lu %-7uE10" : "%-10lu %-10lu";
    printf(print_flag.c_str(), avg, el.count);
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
