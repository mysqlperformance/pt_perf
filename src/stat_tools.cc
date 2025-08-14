#include <stdio.h>
#include <cmath>
#include <cstring>
#include <assert.h>

#include "stat_tools.h"

using namespace std;

Histogram::Histogram() :
  name(""),
  total(0),
  count(0) {}

void Histogram::init_slot() {
  for (auto val : arr) {
    int i = 0;
    while ((1 << i) <= val) i++;

    if (slots.size() < i)
      slots.resize(i);
    
    ++slots[i - 1];
    total += val;
  }
  count = arr.size();
}

static void print_stars(unsigned int val, unsigned int val_max, int width)
{
  int num_stars, num_spaces, i;
  bool need_plus;

  num_stars = min(val, val_max) * width / val_max;
  num_spaces = width - num_stars;
  need_plus = val > val_max;

  for (i = 0; i < num_stars; i++)
    printf("*");
  for (i = 0; i < num_spaces; i++)
    printf(" ");
  if (need_plus)
    printf("+");
}

void Histogram::print_log2(const char *val_type)
{
  int vals_size = slots.size();
  int stars_max = 40, idx_max = -1;
  unsigned int val, val_max = 0;
  unsigned long long low, high;
  int stars, width, i;

  for (i = 0; i < vals_size; i++) {
    val = slots[i];
    if (val > 0)
      idx_max = i;
    if (val > val_max)
      val_max = val;
  }

  if (idx_max < 0)
    return;

  for (i = 0; i < 80; ++i) printf("=");
  printf("\n");
  printf("Histogram - %s :\n", name.c_str());
  printf("%*s%-*s : count    distribution\n", idx_max <= 32 ? 5 : 15, "",
    idx_max <= 32 ? 19 : 29, val_type);

  if (idx_max <= 32)
    stars = stars_max;
  else
    stars = stars_max / 2;

  for (i = 0; i <= idx_max; i++) {
    low = (1ULL << (i + 1)) >> 1;
    high = (1ULL << (i + 1)) - 1;
    if (low == high)
      low -= 1;
    val = slots[i];
    width = idx_max <= 32 ? 10 : 20;
    printf("%*lld -> %-*lld : %-8d |", width, low, width, high, val);
    print_stars(val, val_max, stars);
    printf("|\n");
  }
  printf("trace count: %d, average latency: %d %s\n", count, total / count, val_type);
}

void Histogram::print(vector<string> &names, vector<uint32_t> &val)
{
  for (int i = 0; i < 80; ++i) printf("=");
  printf("\n");
  //printf("%*s%-*s : count    distribution\n", 15, "", 29, val_type);

}
