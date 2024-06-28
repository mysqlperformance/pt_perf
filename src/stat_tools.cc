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

  max_key_length = (max_key_length + 1) / 2 * 2;
  printf("%*s%-*s : %-*s %-*s", max_key_length / 2,
          "", max_key_length / 2, "name",
         10, "avg", 10, "cnt");
  for (Bucket *bucket : extra_buckets) {
    bucket->print_val_name();
  }
  printf(" %-*s ", 20, "distribution (total)");
  printf("\n");
  for (auto it = keys.begin(); it != keys.end(); ++it) {
    Bucket::Element &el = *it;
    const string &name = el.name;
    uint64_t avg = el.get_avg();
    string print_flag;
    if (avg >= INTEGER_TEN_ZEROS) {
      print_flag = "%-*s : %-10lu %-7uE10";
    } else {
      print_flag = "%-*s : %-10lu %-10lu";
    }
    if (el.val_str.size() > max_key_length) {
      // name is too long
      string part;
      el.val_str = "[[" + el.val_str + "]]";
      int i, val_len = el.val_str.size();
      for (i = val_len; i > max_key_length; i -= max_key_length) {
        part = el.val_str.substr(val_len - i, max_key_length);
        printf("%s\n", part.c_str());
      }
      part =
        el.val_str.substr(val_len - i, max_key_length);
      printf(print_flag.c_str(), max_key_length,
              part.c_str(), avg, el.count);
    } else {
      printf(print_flag.c_str(), max_key_length,
              el.val_str.substr(0, max_key_length).c_str(),
              avg, el.count);
    }
    for (Bucket *bucket : extra_buckets) {
      if (bucket->slots.count(name)) {
        Bucket::Element &extra = bucket->slots[name];
        extra.print();
      } else {
        printf(" %-10d", 0);
      }
    }
    printf("|");
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
