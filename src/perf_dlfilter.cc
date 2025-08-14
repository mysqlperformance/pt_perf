extern "C" {
  #include "perf_dlfilter.h"
  #include "string.h"
}

perf_dlfilter_fns perf_dlfilter_fns;

extern "C" {
  int filter_event(void *data, const struct perf_dlfilter_sample *sample,
                   void *ctx) {
    const struct perf_dlfilter_al *from_al;
    const struct perf_dlfilter_al *to_al;
  
    /* keep non branch events */
    if (!sample->ip || !sample->addr_correlates_sym) return 0;
  
    from_al = perf_dlfilter_fns.resolve_ip(ctx);
    to_al = perf_dlfilter_fns.resolve_addr(ctx);
  
    /* keep when either symbol is unknown */
    if (!from_al || !from_al->sym || !to_al || !to_al->sym) return 0;
  
    return !strcmp(from_al->sym, to_al->sym);
  }
}
