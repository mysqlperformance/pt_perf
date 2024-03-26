/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PMU_EVENTS_H
#define PMU_EVENTS_H

struct perf_pmu;

enum aggr_mode_class {
	PerChip = 1,
	PerCore
};

/*
 * Describe each PMU event. Each CPU has a table of PMU events.
 */
struct pmu_event {
	const char *name;
	const char *compat;
	const char *event;
	const char *desc;
	const char *topic;
	const char *long_desc;
	const char *pmu;
	const char *unit;
	const char *perpkg;
	const char *aggr_mode;
	const char *deprecated;
};

struct pmu_metric {
	const char *metric_name;
	const char *metric_group;
	const char *metric_expr;
	const char *unit;
	const char *compat;
	const char *aggr_mode;
	const char *metric_constraint;
	const char *desc;
	const char *long_desc;
};

struct pmu_events_table;
struct pmu_metrics_table;

typedef int (*pmu_event_iter_fn)(const struct pmu_event *pe,
				 const struct pmu_events_table *table,
				 void *data);

typedef int (*pmu_metric_iter_fn)(const struct pmu_metric *pm,
				  const struct pmu_metrics_table *table,
				  void *data);

int pmu_events_table_for_each_event(const struct pmu_events_table *table, pmu_event_iter_fn fn,
				    void *data);
int pmu_metrics_table_for_each_metric(const struct pmu_metrics_table *table, pmu_metric_iter_fn fn,
				     void *data);

const struct pmu_events_table *perf_pmu__find_events_table(struct perf_pmu *pmu);
const struct pmu_metrics_table *perf_pmu__find_metrics_table(struct perf_pmu *pmu);
const struct pmu_events_table *find_core_events_table(const char *arch, const char *cpuid);
const struct pmu_metrics_table *find_core_metrics_table(const char *arch, const char *cpuid);
int pmu_for_each_core_event(pmu_event_iter_fn fn, void *data);
int pmu_for_each_core_metric(pmu_metric_iter_fn fn, void *data);

const struct pmu_events_table *find_sys_events_table(const char *name);
const struct pmu_metrics_table *find_sys_metrics_table(const char *name);
int pmu_for_each_sys_event(pmu_event_iter_fn fn, void *data);
int pmu_for_each_sys_metric(pmu_metric_iter_fn fn, void *data);

#endif
