/*
 * pcp_stats.c: Functions used to read and write PCP archives.
 * (C) 2019-2026 by Sebastien GODARD (sysstat <at> orange.fr)
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#include "version.h"
#include "sa.h"
#include "pcp_stats.h"
#include "pcp_def_metrics.h"
#include "pcp_local.h"

#ifdef HAVE_PCP
#include <sys/wait.h>
#endif

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef HAVE_PCP
extern struct record_header record_hdr[];
extern struct activity *act[];
extern uint64_t flags;
#ifdef HAVE_PMI_APPEND
static pid_t compress_child;
#endif
static const char * const bat_status[] = {
	"Unknown", "Charging", "Discharging", "NotCharging", "Full"
};

/*
 ***************************************************************************
 * Extract minimum 32-bit unsigned integer PCP metric value from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
unsigned long pcp_read_u32(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_U32);
	return atom.ul;
}

/*
 ***************************************************************************
 * Extract minimum 64-bit unsigned integer PCP metric value from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
unsigned long long pcp_read_u64(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_U64);
	return atom.ull;
}

/*
 ***************************************************************************
 * Extract 32-bit floating point PCP metric values from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
float pcp_read_float(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_FLOAT);
	return atom.f;
}

/*
 ***************************************************************************
 * Extract 64-bit floating point PCP metric values from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
double pcp_read_double(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_DOUBLE);
	return atom.d;
}

/*
 ***************************************************************************
 * Extract character array PCP metric values from a valueset.
 * Caller must free the returned character array (string).
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
char *pcp_read_str(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_STRING);
	return atom.cp;
}

/*
 ***************************************************************************
 * Update activity instance (nr) count and re-allocate space if needed.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_reallocate_buffers(pmValueSet *values, struct activity *a, int curr)
{
	if ((a->nr[curr] = values->numval) > a->nr_allocated) {
		if (a->nr_ini < 0)
			a->nr_ini = a->nr2 = values->numval;
		reallocate_buffers(a, a->nr[curr], flags);
	}
}

/*
 * Find the handle-array slot for a named instance (disk, NIC, sensor, etc.)
 * by scanning the activity's item_list.  Returns 0 if not found.
 */
static size_t pcp_slot_for_item(struct sa_item *item_list, const char *name)
{
	struct sa_item *list;
	size_t slot = 0;

	for (list = item_list; list != NULL; list = list->next, slot++)
		if (!strcmp(list->item_name, name))
			return slot;
	return 0;
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write CPU statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_cpu_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	int i, handle;
	size_t slot;
	unsigned long long deltot_jiffies = 1;
	char cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	struct stats_cpu *scc, *scp;
	/*
	 * @nr[curr] cannot normally be greater than @nr_ini.
	 * Yet we have created PCP metrics only for @nr_ini CPU.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/*
	 * Compute CPU "all" as sum of all individual CPU (on SMP machines)
	 * and look for offline CPU.
	 */
	if (a->nr_ini > 1) {
		deltot_jiffies = get_global_cpu_statistics(a, !curr, curr,
							   flags, offline_cpu_bitmap);
	}

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!IS_CPU_SELECTED(a->bitmap->b_array, i) ||
		    IS_CPU_OFFLINE(offline_cpu_bitmap, i))
			/* Don't display CPU */
			continue;

		scc = (struct stats_cpu *) ((char *) a->buf[curr]  + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			if (a->nr_ini == 1) {
				/*
				 * This is a UP machine. In this case
				 * interval has still not been calculated.
				 */
				deltot_jiffies = get_per_cpu_interval(scc, scp);
			}
			if (!deltot_jiffies) {
				/* CPU "all" cannot be tickless */
				deltot_jiffies = 1;
			}
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);

			/*
			 * Recalculate interval for current proc.
			 * If result is 0 then current CPU is a tickless one.
			 */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/* Current CPU is tickless */
				slot = pcp_find_slot(m, i - 1);
				atom.ull = 0;
				handle = ACT_HANDLE(m, CPU_PERCPU_USER, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_NICE, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_SYS, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_WAITTOTAL, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_STEAL, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_IRQHARD, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_IRQSOFT, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_CPU_INTR, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_GUEST, slot);
				pmiPutAtomValueHandle(handle, &atom);
				handle = ACT_HANDLE(m, CPU_PERCPU_GUESTNICE, slot);
				pmiPutAtomValueHandle(handle, &atom);
				atom.ull = 100;
				handle = ACT_HANDLE(m, CPU_PERCPU_IDLE, slot);
				pmiPutAtomValueHandle(handle, &atom);

				continue;
			}
		}
#define JIFFIES_TO_MSEC(j)	((j) * 1000ULL / hz)

		slot = i ? pcp_find_slot(m, i - 1) : 0;

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_user - scc->cpu_guest);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_USER : CPU_ALLCPU_USER, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_nice - scc->cpu_guest_nice);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_NICE : CPU_ALLCPU_NICE, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_sys);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_SYS : CPU_ALLCPU_SYS, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_iowait);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_WAITTOTAL : CPU_ALLCPU_WAITTOTAL, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_steal);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_STEAL : CPU_ALLCPU_STEAL, slot);
		pmiPutAtomValueHandle(handle, &atom);

		if (i) {
			/* kernel.percpu.cpu.intr — total interrupt ms per CPU; no all-CPU equivalent exists */
			atom.ull = JIFFIES_TO_MSEC(scc->cpu_hardirq + scc->cpu_softirq);
			handle = ACT_HANDLE(m, CPU_PERCPU_CPU_INTR, slot);
			pmiPutAtomValueHandle(handle, &atom);
		}

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_hardirq);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_IRQHARD : CPU_ALLCPU_IRQHARD, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_softirq);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_IRQSOFT : CPU_ALLCPU_IRQSOFT, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_guest);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_GUEST : CPU_ALLCPU_GUEST, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_guest_nice);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_GUESTNICE : CPU_ALLCPU_GUESTNICE, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = JIFFIES_TO_MSEC(scc->cpu_idle);
		handle = ACT_HANDLE(m, i ? CPU_PERCPU_IDLE : CPU_ALLCPU_IDLE, slot);
		pmiPutAtomValueHandle(handle, &atom);

		/*
		 * kernel.percpu.interrupts is written by pcp_print_irq_stats with
		 * the correct IRQ::cpuN instance naming; do not write it here.
		 */
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read CPU statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_cpu_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j, n;
	struct stats_cpu *scc;

	switch (values->pmid) {

	/* All-CPU scalar metrics: write into slot 0 */
	case PMID_CPU_ALLCPU_USER:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_user = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_USER);
		break;
	case PMID_CPU_ALLCPU_NICE:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_nice = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_NICE);
		break;
	case PMID_CPU_ALLCPU_SYS:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_sys = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_SYS);
		break;
	case PMID_CPU_ALLCPU_IDLE:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_idle = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_IDLE);
		break;
	case PMID_CPU_ALLCPU_WAITTOTAL:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_iowait = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_WAITTOTAL);
		break;
	case PMID_CPU_ALLCPU_IRQTOTAL:
		/* Reconstructed from IRQHARD + IRQSOFT; skip */
		break;
	case PMID_CPU_ALLCPU_IRQHARD:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_hardirq = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_IRQHARD);
		break;
	case PMID_CPU_ALLCPU_IRQSOFT:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_softirq = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_IRQSOFT);
		break;
	case PMID_CPU_ALLCPU_STEAL:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_steal = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_STEAL);
		break;
	case PMID_CPU_ALLCPU_GUEST:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_guest = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_GUEST);
		/* Restore original cpu_user (write path stored user - guest) */
		scc->cpu_user += scc->cpu_guest;
		break;
	case PMID_CPU_ALLCPU_GUESTNICE:
		scc = (struct stats_cpu *) a->buf[curr];
		scc->cpu_guest_nice = pcp_read_u64(values, 0, cpu_metric_descs, CPU_ALLCPU_GUESTNICE);
		/* Restore original cpu_nice (write path stored nice - guest_nice) */
		scc->cpu_nice += scc->cpu_guest_nice;
		break;

	/* Per-CPU metrics: slot 0 = "all", slots 1..N = cpu0..cpuN-1 */
	case PMID_CPU_PERCPU_USER:
		n = values->numval + 1;
		if (n > a->nr_allocated)
			reallocate_buffers(a, n, flags);
		a->nr[curr] = n;
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_user = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_USER);
		}
		break;
	case PMID_CPU_PERCPU_NICE:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_nice = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_NICE);
		}
		break;
	case PMID_CPU_PERCPU_SYS:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_sys = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_SYS);
		}
		break;
	case PMID_CPU_PERCPU_IDLE:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_idle = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_IDLE);
		}
		break;
	case PMID_CPU_PERCPU_WAITTOTAL:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_iowait = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_WAITTOTAL);
		}
		break;
	case PMID_CPU_PERCPU_CPU_INTR:
		/* Reconstructed from IRQHARD + IRQSOFT; skip */
		break;
	case PMID_CPU_PERCPU_IRQHARD:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_hardirq = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_IRQHARD);
		}
		break;
	case PMID_CPU_PERCPU_IRQSOFT:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_softirq = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_IRQSOFT);
		}
		break;
	case PMID_CPU_PERCPU_STEAL:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_steal = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_STEAL);
		}
		break;
	case PMID_CPU_PERCPU_GUEST:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_guest = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_GUEST);
			scc->cpu_user += scc->cpu_guest;
		}
		break;
	case PMID_CPU_PERCPU_GUESTNICE:
		for (j = 0; j < values->numval; j++) {
			scc = (struct stats_cpu *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			scc->cpu_guest_nice = pcp_read_u64(values, j, cpu_metric_descs, CPU_PERCPU_GUESTNICE);
			scc->cpu_nice += scc->cpu_guest_nice;
		}
		break;
	case PMID_CPU_PERCPU_INTERRUPTS:
		/* Per-interrupt-per-CPU data lives in A_IRQ; nothing to store here */
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write softnet statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_softnet_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int handle, i;
	struct stats_softnet *ssnc;
	char cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini.
	 * Yet we have created PCP metrics only for @nr_ini CPU.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Compute statistics for CPU "all" */
	get_global_soft_statistics(a, !curr, curr, flags, offline_cpu_bitmap);

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* No */
			continue;

                ssnc = (struct stats_softnet *) ((char *) a->buf[curr]  + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			continue;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
		}

		slot = pcp_find_slot(m, i - 1);

		atom.ull = (unsigned long long)ssnc->processed;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_PROCESSED, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = (unsigned long long)ssnc->dropped;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_DROPPED, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = (unsigned long long)ssnc->time_squeeze;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_TIMESQUEEZE, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = (unsigned long long)ssnc->received_rps;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_RECEIVEDRPS, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = (unsigned long long)ssnc->flow_limit;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_FLOWLIMIT, slot);
		pmiPutAtomValueHandle(handle, &atom);

		atom.ull = (unsigned long long)ssnc->backlog_len;
		handle = ACT_HANDLE(m, SOFTNET_PERCPU_BACKLOGLENGTH, slot);
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read softnet statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_softnet_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j, n;
	struct stats_softnet *ssnc;

	/*
	 * The write path skips the "all" slot and only writes PERCPU metrics.
	 * Buffer layout: slot 0 = "all" (computed later), slots 1..N = cpu0..cpuN-1.
	 */
	switch (values->pmid) {

	/* ALLCPU metrics come from PCP live data, not sysstat archives; skip */
	case PMID_SOFTNET_ALLCPU_PROCESSED:
	case PMID_SOFTNET_ALLCPU_DROPPED:
	case PMID_SOFTNET_ALLCPU_TIMESQUEEZE:
	case PMID_SOFTNET_ALLCPU_RECEIVEDRPS:
	case PMID_SOFTNET_ALLCPU_FLOWLIMIT:
	case PMID_SOFTNET_ALLCPU_BACKLOGLENGTH:
		break;

	case PMID_SOFTNET_PERCPU_PROCESSED:
		n = values->numval + 1;
		if (n > a->nr_allocated)
			reallocate_buffers(a, n, flags);
		a->nr[curr] = n;
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->processed = pcp_read_u32(values, j, softnet_metric_descs,
						       SOFTNET_PERCPU_PROCESSED);
		}
		break;
	case PMID_SOFTNET_PERCPU_DROPPED:
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->dropped = pcp_read_u32(values, j, softnet_metric_descs,
						     SOFTNET_PERCPU_DROPPED);
		}
		break;
	case PMID_SOFTNET_PERCPU_TIMESQUEEZE:
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->time_squeeze = pcp_read_u32(values, j, softnet_metric_descs,
							  SOFTNET_PERCPU_TIMESQUEEZE);
		}
		break;
	case PMID_SOFTNET_PERCPU_RECEIVEDRPS:
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->received_rps = pcp_read_u32(values, j, softnet_metric_descs,
							  SOFTNET_PERCPU_RECEIVEDRPS);
		}
		break;
	case PMID_SOFTNET_PERCPU_FLOWLIMIT:
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->flow_limit = pcp_read_u32(values, j, softnet_metric_descs,
							SOFTNET_PERCPU_FLOWLIMIT);
		}
		break;
	case PMID_SOFTNET_PERCPU_BACKLOGLENGTH:
		for (j = 0; j < values->numval; j++) {
			ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + (j + 1) * a->msize);
			ssnc->backlog_len = pcp_read_u32(values, j, softnet_metric_descs,
							 SOFTNET_PERCPU_BACKLOGLENGTH);
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write task creation and context switch statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pcsw_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr];

	atom.ull = (unsigned long long)spc->context_switch;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PCSW_CONTEXT_SWITCH, 0), &atom);

	atom.ull = (unsigned long long)spc->processes;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PCSW_FORK_SYSCALLS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read task creation and context switch statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_pcsw_stats(pmValueSet *values, struct activity *a, int curr)
{
	pcp_reallocate_buffers(values, a, curr);

	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr];

	switch (values->pmid) {
	case PMID_PCSW_CONTEXT_SWITCH:
		spc->context_switch = pcp_read_u64(values, 0,
						pcsw_metric_descs,
						PCSW_CONTEXT_SWITCH);
		break;

	case PMID_PCSW_FORK_SYSCALLS:
		spc->processes = pcp_read_u64(values, 0,
						pcsw_metric_descs,
						PCSW_FORK_SYSCALLS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write interrupts statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_irq_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, c, handle;
	char name[64];
	struct stats_irq *stc_cpu_irq, *stc_cpuall_irq;
	unsigned char masked_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Identify offline and unselected CPU, and keep persistent statistics values */
	get_global_int_statistics(a, !curr, curr, flags, masked_cpu_bitmap);

	for (i = 0; i < a->nr2; i++) {

		stc_cpuall_irq = (struct stats_irq *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of interrupts has been entered on the command line */
			if (!search_list_item(a->item_list, stc_cpuall_irq->irq_name))
				/* Interrupt not found in list */
				continue;
		}

		for (c = 0; (c < a->nr[curr]) && (c < a->bitmap->b_size + 1); c++) {

			stc_cpu_irq = (struct stats_irq *) ((char *) a->buf[curr] + c * a->msize * a->nr2
										  + i * a->msize);

			/* Should current CPU (including CPU "all") be processed? */
			if (IS_CPU_SET(masked_cpu_bitmap, c))
				/* No */
				continue;

			atom.ull = stc_cpu_irq->irq_nr;

			if (!c) {
				/* This is CPU "all" */
				if (!i) {
					/* This is interrupt "sum" */
					handle = ACT_HANDLE(m, IRQ_ALLIRQ_TOTAL, 0);
					pmiPutAtomValueHandle(handle, &atom);
				}
				else {
					slot = pcp_slot_for_item(a->item_list,
								 stc_cpuall_irq->irq_name);
					handle = ACT_HANDLE(m, IRQ_PERIRQ_TOTAL, slot);
					pmiPutAtomValueHandle(handle, &atom);
				}
			}
			else {
				/* This is a particular CPU — dynamic "irq::cpuN" instance.
				 * No pre-allocated handle exists for these instances, so
				 * pmiPutValue is used here.
				 */
				char numstr[32];

				pmsprintf(name, sizeof(name), "%s::cpu%d",
					 stc_cpuall_irq->irq_name, c - 1);
				pmsprintf(numstr, sizeof(numstr), "%u",
					  stc_cpu_irq->irq_nr);
				pmiPutValue("kernel.percpu.interrupts", name, numstr);
			}
		}
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read interrupts statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_irq_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_irq *stc;

	switch (values->pmid) {
	case PMID_IRQ_ALLIRQ_TOTAL:
		/* Total interrupt count for all CPUs — stored in slot [0] */
		pcp_reallocate_buffers(values, a, curr);
		stc = (struct stats_irq *) a->buf[curr];
		stc->irq_nr = pcp_read_u32(values, 0, irq_metric_descs,
					   IRQ_ALLIRQ_TOTAL);
		strncpy(stc->irq_name, "sum", MAX_SA_IRQ_LEN - 1);
		stc->irq_name[MAX_SA_IRQ_LEN - 1] = '\0';
		break;

	case PMID_IRQ_PERIRQ_TOTAL:
		/* Per-interrupt-line totals: complex 2D layout, not yet implemented */
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write swapping statistics to PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_swap_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr];

	atom.ul = (unsigned long)ssc->pswpin;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SWAP_PAGESIN, 0), &atom);

	atom.ull = (unsigned long long)ssc->pswpout;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SWAP_PAGESOUT, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read swapping statistics from PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_swap_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_swap
		*ssc =  (struct stats_swap *) a->buf[curr];

	switch (values->pmid) {
	case PMID_SWAP_PAGESIN:
		ssc->pswpin = pcp_read_u32(values, 0, swap_metric_descs,
						SWAP_PAGESIN);
		break;

	case PMID_SWAP_PAGESOUT:
		ssc->pswpout = pcp_read_u32(values, 0, swap_metric_descs,
						SWAP_PAGESOUT);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write paging statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_paging_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr];

	atom.ull = (unsigned long long)spc->pgpgin;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGPGIN, 0), &atom);

	atom.ull = (unsigned long long)spc->pgpgout;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGPGOUT, 0), &atom);

	atom.ull = (unsigned long long)spc->pgfault;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGFAULT, 0), &atom);

	atom.ull = (unsigned long long)spc->pgmajfault;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGMAJFAULT, 0), &atom);

	atom.ull = (unsigned long long)spc->pgfree;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGFREE, 0), &atom);

	atom.ull = (unsigned long long)spc->pgscan_kswapd;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGSCANKSWAPD, 0), &atom);

	atom.ull = (unsigned long long)spc->pgscan_direct;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGSCANDIRECT, 0), &atom);

	atom.ull = (unsigned long long)spc->pgsteal;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGSTEAL, 0), &atom);

	atom.ull = (unsigned long long)spc->pgpromote;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGPROMOTE, 0), &atom);

	atom.ull = (unsigned long long)spc->pgdemote;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PAGING_PGDEMOTE, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read paging statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_paging_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr];

	switch (values->pmid) {
	case PMID_PAGING_PGPGIN:
		spc->pgpgin = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGPGIN);
			break;

	case PMID_PAGING_PGPGOUT:
		spc->pgpgout = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGPGOUT);
		break;

	case PMID_PAGING_PGFAULT:
		spc->pgfault = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGFAULT);
		break;

	case PMID_PAGING_PGMAJFAULT:
		spc->pgmajfault = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGMAJFAULT);
		break;

	case PMID_PAGING_PGFREE:
		spc->pgfree = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGFREE);
		break;

	case PMID_PAGING_PGSCANDIRECT:
		spc->pgscan_direct = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSCANDIRECT);
		break;

	case PMID_PAGING_PGSCANKSWAPD:
		spc->pgscan_kswapd = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSCANKSWAPD);
		break;

	case PMID_PAGING_PGSTEAL:
		spc->pgsteal = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSTEAL);
		break;

	case PMID_PAGING_PGPROMOTE:
		spc->pgpromote = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGPROMOTE);
		break;

	case PMID_PAGING_PGDEMOTE:
		spc->pgdemote = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGDEMOTE);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write I/O and transfer rate statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_io_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr];

	atom.ull = (unsigned long long)sic->dk_drive;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_TOTAL, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_rio;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_READ, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_wio;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_WRITE, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_dio;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_DISCARD, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_rblk;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_READBYTES, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_wblk;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_WRITEBYTES, 0), &atom);

	atom.ull = (unsigned long long)sic->dk_drive_dblk;
	pmiPutAtomValueHandle(ACT_HANDLE(m, IO_ALLDEV_DISCARDBYTES, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 * **************************************************************************
 * Display RAM memory utilization in PCP format.
 *
 * IN:
 * @smc		Structure with statistics.
 * @dispall	TRUE if all memory fields should be displayed.
 ***************************************************************************
 */
void pcp_print_ram_memory_stats(struct stats_memory *smc, int dispall)
{
	struct act_metrics *m = &mem_metrics;
	pmAtomValue atom;

	atom.ul = (unsigned long)(smc->tlmkb >> 10);
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_PHYS_MB, 0), &atom);

	atom.ull = (unsigned long long)smc->tlmkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_PHYS_KB, 0), &atom);

	atom.ull = (unsigned long long)smc->frmkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_FREE, 0), &atom);

	atom.ull = (unsigned long long)smc->availablekb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_AVAIL, 0), &atom);

	atom.ull = (unsigned long long)smc->tlmkb - smc->availablekb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_USED, 0), &atom);

	atom.ull = (unsigned long long)smc->bufkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_BUFFER, 0), &atom);

	atom.ull = (unsigned long long)smc->camkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_CACHED, 0), &atom);

	atom.ull = (unsigned long long)smc->comkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_COMMITAS, 0), &atom);

	atom.ull = (unsigned long long)smc->activekb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_ACTIVE, 0), &atom);

	atom.ull = (unsigned long long)smc->inactkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_INACTIVE, 0), &atom);

	atom.ull = (unsigned long long)smc->dirtykb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_DIRTY, 0), &atom);

	atom.ull = (unsigned long long)smc->shmemkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SHARED, 0), &atom);

	if (dispall) {
		atom.ull = (unsigned long long)smc->anonpgkb;
		pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_ANON, 0), &atom);

		atom.ull = (unsigned long long)smc->slabkb;
		pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SLAB, 0), &atom);

		atom.ull = (unsigned long long)smc->kstackkb;
		pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_KSTACK, 0), &atom);

		atom.ull = (unsigned long long)smc->pgtblkb;
		pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_PGTABLE, 0), &atom);

		atom.ull = (unsigned long long)smc->vmusedkb;
		pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_VMALLOC, 0), &atom);
	}
}
#endif	/* HAVE_PCP */

/*
 * **************************************************************************
 * Display swap memory utilization in PCP format.
 *
 * IN:
 * @smc		Structure with statistics.
 ***************************************************************************
 */
void pcp_print_swap_memory_stats(struct stats_memory *smc)
{
#ifdef HAVE_PCP
	struct act_metrics *m = &mem_metrics;
	pmAtomValue atom;

	atom.ull = (unsigned long long)smc->frskb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SWAPFREE, 0), &atom);

	atom.ull = (unsigned long long)smc->tlskb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SWAPTOTAL, 0), &atom);

	atom.ull = (unsigned long long)smc->caskb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SWAPCACHED, 0), &atom);

	atom.ull = (unsigned long long)smc->shmemkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_SHMEM, 0), &atom);

	atom.ull = (unsigned long long)smc->frmkb;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_UTIL_FREEMEM, 0), &atom);

	/* swap.free / swap.length are in bytes (frskb/tlskb are kB) */
	atom.ull = (unsigned long long)smc->frskb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SWAP_FREE, 0), &atom);

	atom.ull = (unsigned long long)smc->tlskb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SWAP_LENGTH, 0), &atom);
#endif	/* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read I/O and transfer rate statistics from PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_io_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr];

	switch (values->pmid) {
	case PMID_IO_ALLDEV_TOTAL:
		sic->dk_drive = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_TOTAL);
		break;

	case PMID_IO_ALLDEV_READ:
		sic->dk_drive_rio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_READ);
		break;

	case PMID_IO_ALLDEV_WRITE:
		sic->dk_drive_wio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_WRITE);
		break;

	case PMID_IO_ALLDEV_DISCARD:
		sic->dk_drive_dio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_DISCARD);
		break;

	case PMID_IO_ALLDEV_READBYTES:
		sic->dk_drive_rblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_READBYTES);
		break;

	case PMID_IO_ALLDEV_WRITEBYTES:
		sic->dk_drive_wblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_WRITEBYTES);
		break;

	case PMID_IO_ALLDEV_DISCARDBYTES:
		sic->dk_drive_dblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_DISCARDBYTES);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write memory statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_memory_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	pcp_print_ram_memory_stats(smc, TRUE);
	pcp_print_swap_memory_stats(smc);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read memory statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_memory_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	switch (values->pmid) {
	case PMID_MEM_PHYS_KB:
		smc->tlmkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_PHYS_KB);
		break;

	case PMID_MEM_UTIL_FREE:
		smc->frmkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_FREE);
		break;

	case PMID_MEM_UTIL_SHARED:
		smc->shmemkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SHARED);
		break;

	case PMID_MEM_UTIL_AVAIL:
		smc->availablekb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_AVAIL);
		break;

	case PMID_MEM_UTIL_BUFFER:
		smc->bufkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_BUFFER);
		break;

	case PMID_MEM_UTIL_CACHED:
		smc->camkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_CACHED);
		break;

	case PMID_MEM_UTIL_COMMITAS:
		smc->comkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_COMMITAS);
		break;

	case PMID_MEM_UTIL_ACTIVE:
		smc->activekb = pcp_read_u64(values, 0, mem_metric_descs,
					MEM_UTIL_ACTIVE);
		break;

	case PMID_MEM_UTIL_INACTIVE:
		smc->inactkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_INACTIVE);
		break;

	case PMID_MEM_UTIL_DIRTY:
		smc->dirtykb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_DIRTY);
		break;

	case PMID_MEM_UTIL_ANON:
		smc->anonpgkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_ANON);
		break;

	case PMID_MEM_UTIL_SLAB:
		smc->slabkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SLAB);
		break;

	case PMID_MEM_UTIL_KSTACK:
		smc->kstackkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_KSTACK);
		break;

	case PMID_MEM_UTIL_PGTABLE:
		smc->pgtblkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_PGTABLE);
		break;

	case PMID_MEM_UTIL_VMALLOC:
		smc->vmusedkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_VMALLOC);
		break;

	case PMID_MEM_UTIL_SWAPFREE:
		smc->frskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPFREE);
		break;

	case PMID_MEM_UTIL_SWAPTOTAL:
		smc->tlskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPTOTAL);
		break;

	case PMID_MEM_UTIL_SWAPCACHED:
		smc->caskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPCACHED);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write kernel tables statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_ktables_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	atom.ul = (unsigned long)skc->dentry_stat;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KTABLE_DENTRYS, 0), &atom);

	atom.ul = (unsigned long)skc->file_used;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KTABLE_FILES, 0), &atom);

	atom.ul = (unsigned long)skc->inode_used;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KTABLE_INODES, 0), &atom);

	atom.ul = (unsigned long)skc->pty_nr;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KTABLE_PTYS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read kernel tables statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_ktable_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	switch (values->pmid) {
	case PMID_KTABLE_DENTRYS:
		skc->dentry_stat = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_DENTRYS);
		break;

	case PMID_KTABLE_FILES:
		skc->file_used = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_FILES);
		break;

	case PMID_KTABLE_INODES:
		skc->inode_used = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_INODES);
		break;

	case PMID_KTABLE_PTYS:
		skc->pty_nr = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_PTYS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write queue and load statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_queue_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	atom.ul = (unsigned long)sqc->nr_running;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_RUNNABLE, 0), &atom);

	atom.ul = (unsigned long)sqc->nr_threads;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_PROCESSES, 0), &atom);

	atom.ull = (unsigned long long)(unsigned long) sqc->procs_blocked;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_BLOCKED, 0), &atom);

	atom.f = (float)sqc->load_avg_1 / 100.0;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_LOADAVG, pcp_find_slot(m, 1)), &atom);

	atom.f = (float)sqc->load_avg_5 / 100.0;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_LOADAVG, pcp_find_slot(m, 5)), &atom);

	atom.f = (float)sqc->load_avg_15 / 100.0;
	pmiPutAtomValueHandle(ACT_HANDLE(m, KQUEUE_LOADAVG, pcp_find_slot(m, 15)), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read queue and load statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_kqueue_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	switch (values->pmid) {

	case PMID_KQUEUE_RUNNABLE:
		sqc->nr_running = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_RUNNABLE);
		break;

	case PMID_KQUEUE_PROCESSES:
		sqc->nr_threads = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_PROCESSES);
		break;

	case PMID_KQUEUE_BLOCKED:
		sqc->procs_blocked = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_BLOCKED);
		break;

	case PMID_KQUEUE_LOADAVG:
		for (i = 0; i < values->numval; i++) {
			switch (values->vlist[i].inst) {

			case 1: /* 1 minute average */
				sqc->load_avg_1 = (unsigned int)(100.0 *
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG));
				break;
			case 5: /* 5 minute average */
				sqc->load_avg_5 = (unsigned int)(100.0 *
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG));
				break;
			case 15: /* 15 minute average */
				sqc->load_avg_15 = (unsigned int)(100.0 *
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG));
				break;
			}
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write disks statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_disk_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *cm;
	struct sa_item *ilist;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_disk *sdc;
	char *dev_name;
	char dm_name[MAX_NAME_LEN];
	const char *eff_name;

	for (i = 0; i < a->nr[curr]; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		/* Get device name */
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn, sdc->part_nr,
					   DISPLAY_PRETTY(flags), DISPLAY_PERSIST_NAME_S(flags),
					   USE_STABLE_ID(flags), NULL);

		/* Route to the correct metrics group and item_list */
		if (is_dm_device(dev_name)) {
			dm_persistent_name(dev_name, sdc->major, sdc->minor,
					   dm_name, sizeof(dm_name));
			eff_name = dm_name;
			cm    = &dm_metrics;
			ilist = dm_item_list;
		}
		else if (is_md_device(dev_name)) {
			eff_name = dev_name;
			cm    = &md_metrics;
			ilist = md_item_list;
		}
		else if (is_zram_device(dev_name)) {
			eff_name = dev_name;
			cm    = &zram_metrics;
			ilist = zram_item_list;
		}
		else if (is_part_device(dev_name)) {
			eff_name = dev_name;
			cm    = &part_metrics;
			ilist = part_item_list;
		}
		else {
			/* Primary disk.dev class */
			eff_name = dev_name;
			cm    = a->metrics;
			ilist = a->item_list;
		}

		if (ilist != NULL) {
			/* item_list was populated — check the device is in it */
			if (!search_list_item(ilist, (char *)eff_name))
				continue;
		}
		slot = pcp_slot_for_item(ilist, eff_name);

		if (cm == a->metrics) {
			/* Primary class: use DISK_PERDEV_* indices */
			handle = ACT_HANDLE(cm, DISK_PERDEV_READ, slot);
			atom.ull = sdc->rd_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_WRITE, slot);
			atom.ull = sdc->wr_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_TOTAL, slot);
			atom.ull = (unsigned long long) sdc->nr_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_TOTALBYTES, slot);
			atom.ull = (unsigned long long) (sdc->rd_sect + sdc->wr_sect) / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_READBYTES, slot);
			atom.ull = (unsigned long long) sdc->rd_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_WRITEBYTES, slot);
			atom.ull = (unsigned long long) sdc->wr_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_DISCARDBYTES, slot);
			atom.ull = (unsigned long long) sdc->dc_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_TOTALACTIVE, slot);
			atom.ul = (unsigned long) sdc->rd_ticks + sdc->wr_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_READACTIVE, slot);
			atom.ul = (unsigned long) sdc->rd_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_WRITEACTIVE, slot);
			atom.ul = (unsigned long) sdc->wr_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_DISCARDACTIVE, slot);
			atom.ul = (unsigned long)sdc->dc_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_AVACTIVE, slot);
			atom.ul = (unsigned long)sdc->tot_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_AVQUEUE, slot);
			atom.ul = (unsigned long)sdc->rq_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_BLKREAD, slot);
			atom.ull = (unsigned long long)sdc->rd_sect;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_BLKWRITE, slot);
			atom.ull = (unsigned long long)sdc->wr_sect;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DISK_PERDEV_DISCARD, slot);
			atom.ull = sdc->dc_ios;
			pmiPutAtomValueHandle(handle, &atom);
		}
		else {
			/* Secondary class: use DCLASS_* indices */
			handle = ACT_HANDLE(cm, DCLASS_READ, slot);
			atom.ull = sdc->rd_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_WRITE, slot);
			atom.ull = sdc->wr_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_TOTAL, slot);
			atom.ull = (unsigned long long) sdc->nr_ios;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_TOTALBYTES, slot);
			atom.ull = (unsigned long long) (sdc->rd_sect + sdc->wr_sect) / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_READBYTES, slot);
			atom.ull = (unsigned long long) sdc->rd_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_WRITEBYTES, slot);
			atom.ull = (unsigned long long) sdc->wr_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_DISCBYTES, slot);
			atom.ull = (unsigned long long) sdc->dc_sect / 2;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_TOTALACTIVE, slot);
			atom.ul = (unsigned long) sdc->rd_ticks + sdc->wr_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_RD_ACTIVE, slot);
			atom.ul = (unsigned long) sdc->rd_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_WR_ACTIVE, slot);
			atom.ul = (unsigned long) sdc->wr_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_DISCARDACTIVE, slot);
			atom.ul = (unsigned long)sdc->dc_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_AVACTIVE, slot);
			atom.ul = (unsigned long)sdc->tot_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_AVEQ, slot);
			atom.ul = (unsigned long)sdc->rq_ticks;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_BLKREAD, slot);
			atom.ull = (unsigned long long)sdc->rd_sect;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_BLKWRITE, slot);
			atom.ull = (unsigned long long)sdc->wr_sect;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(cm, DCLASS_DISCARD, slot);
			atom.ull = sdc->dc_ios;
			pmiPutAtomValueHandle(handle, &atom);
		}
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 * Look up major:minor for a block device name via /sys/class/block.
 * Returns 0 on success, -1 if the device is not found on this machine.
 */
static int pcp_lookup_disk_major_minor(const char *name,
				       unsigned int *major, unsigned int *minor)
{
	char path[PATH_MAX];
	FILE *fp;

	pmsprintf(path, sizeof(path), "/sys/class/block/%s/dev", name);
	if ((fp = fopen(path, "r")) == NULL)
		return -1;
	if (fscanf(fp, "%u:%u", major, minor) != 2) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

/*
 ***************************************************************************
 * Read disks statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_disk_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i, j;
	struct stats_disk *sdc;
	pmInDom indom = disk_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		sdc = (struct stats_disk *) ((char *) a->buf[curr] + j * a->msize);

		/* Populate major:minor from the instance name on first metric */
		if (values->pmid == PMID_DISK_PERDEV_TOTAL) {
			if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
				if (pcp_lookup_disk_major_minor(name,
						&sdc->major, &sdc->minor) < 0) {
					/* Archive from another machine: use slot index */
					sdc->major = 0;
					sdc->minor = (unsigned int) j;
				}
				free(name);
			}
		}

		switch (values->pmid) {
		case PMID_DISK_PERDEV_READ:
			/* read IOs not stored in native sa format; skip */
			break;
		case PMID_DISK_PERDEV_WRITE:
			/* write IOs not stored in native sa format; skip */
			break;
		case PMID_DISK_PERDEV_TOTAL:
			sdc->nr_ios = pcp_read_u64(values, j, disk_metric_descs,
						   DISK_PERDEV_TOTAL);
			break;
		case PMID_DISK_PERDEV_TOTALBYTES:
			/* stored as KB; rd_sect + wr_sect = 2 * KB value */
			i = (int) pcp_read_u64(values, j, disk_metric_descs,
					       DISK_PERDEV_TOTALBYTES);
			/* split evenly — individual fields updated by READBYTES/WRITEBYTES */
			sdc->rd_sect = (unsigned long) i;
			sdc->wr_sect = 0;
			break;
		case PMID_DISK_PERDEV_READBYTES:
			sdc->rd_sect = (unsigned long)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_READBYTES) * 2;
			break;
		case PMID_DISK_PERDEV_WRITEBYTES:
			sdc->wr_sect = (unsigned long)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_WRITEBYTES) * 2;
			break;
		case PMID_DISK_PERDEV_DISCARDBYTES:
			sdc->dc_sect = (unsigned long)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_DISCARDBYTES) * 2;
			break;
		case PMID_DISK_PERDEV_READACTIVE:
			sdc->rd_ticks = (unsigned int)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_READACTIVE);
			break;
		case PMID_DISK_PERDEV_WRITEACTIVE:
			sdc->wr_ticks = (unsigned int)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_WRITEACTIVE);
			break;
		case PMID_DISK_PERDEV_TOTALACTIVE:
			/* Reconstructed from rd_ticks + wr_ticks; skip */
			break;
		case PMID_DISK_PERDEV_DISCARDACTIVE:
			sdc->dc_ticks = (unsigned int)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_DISCARDACTIVE);
			break;
		case PMID_DISK_PERDEV_AVACTIVE:
			sdc->tot_ticks = (unsigned int)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_AVACTIVE);
			break;
		case PMID_DISK_PERDEV_AVQUEUE:
			sdc->rq_ticks = (unsigned int)
				pcp_read_u64(values, j, disk_metric_descs,
					     DISK_PERDEV_AVQUEUE);
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write network interfaces statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_dev_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_net_dev *sndc;

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}
		slot = pcp_slot_for_item(a->item_list, sndc->interface);

		/*
		 * No need to look for the previous sample values: PCP displays the raw
		 * counter value, not its variation over the interval.
		 * The whole list of network interfaces present in file has been created
		 * (this is goal of the FO_ITEM_LIST option set for pcp_fmt report format -
		 * see format.c). So no need to wonder if an instance needs to be created
		 * for current interface.
		 */

		handle = ACT_HANDLE(m, NET_PERINTF_INPACKETS, slot);
		atom.ull = (unsigned long long)sndc->rx_packets;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_OUTPACKETS, slot);
		atom.ull = (unsigned long long)sndc->tx_packets;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_INBYTES, slot);
		atom.ull = (unsigned long long)sndc->rx_bytes;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_OUTBYTES, slot);
		atom.ull = (unsigned long long)sndc->tx_bytes;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_INCOMPRESS, slot);
		atom.ull = (unsigned long long)sndc->rx_compressed;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_OUTCOMPRESS, slot);
		atom.ull = (unsigned long long)sndc->tx_compressed;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_PERINTF_INMULTICAST, slot);
		atom.ull = (unsigned long long)sndc->multicast;
		pmiPutAtomValueHandle(handle, &atom);

		if (sndc->speed) {
			handle = ACT_HANDLE(m, NET_PERINTF_SPEED, slot);
			atom.f = (float)sndc->speed / 8.0;
			pmiPutAtomValueHandle(handle, &atom);

			handle = ACT_HANDLE(m, NET_PERINTF_BAUDRATE, slot);
			atom.ull = (unsigned long long)sndc->speed * 125000ULL;
			pmiPutAtomValueHandle(handle, &atom);
		}
		if (sndc->duplex) {
			handle = ACT_HANDLE(m, NET_PERINTF_DUPLEX, slot);
			atom.ul = (unsigned int)sndc->duplex;
			pmiPutAtomValueHandle(handle, &atom);
		}
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read network interfaces statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_netdev_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_net_dev *sndc;
	pmInDom indom = netdev_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + j * a->msize);

		/* Store the interface name — needed by the display code */
		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			strncpy(sndc->interface, name, MAX_IFACE_LEN - 1);
			sndc->interface[MAX_IFACE_LEN - 1] = '\0';
			free(name);
		}

		switch (values->pmid) {
		case PMID_NET_PERINTF_INPACKETS:
			sndc->rx_packets = pcp_read_u64(values, j, netdev_metric_descs,
							NET_PERINTF_INPACKETS);
			break;
		case PMID_NET_PERINTF_OUTPACKETS:
			sndc->tx_packets = pcp_read_u64(values, j, netdev_metric_descs,
							NET_PERINTF_OUTPACKETS);
			break;
		case PMID_NET_PERINTF_INBYTES:
			sndc->rx_bytes = pcp_read_u64(values, j, netdev_metric_descs,
						      NET_PERINTF_INBYTES);
			break;
		case PMID_NET_PERINTF_OUTBYTES:
			sndc->tx_bytes = pcp_read_u64(values, j, netdev_metric_descs,
						      NET_PERINTF_OUTBYTES);
			break;
		case PMID_NET_PERINTF_INCOMPRESS:
			sndc->rx_compressed = pcp_read_u64(values, j, netdev_metric_descs,
							    NET_PERINTF_INCOMPRESS);
			break;
		case PMID_NET_PERINTF_OUTCOMPRESS:
			sndc->tx_compressed = pcp_read_u64(values, j, netdev_metric_descs,
							    NET_PERINTF_OUTCOMPRESS);
			break;
		case PMID_NET_PERINTF_INMULTICAST:
			sndc->multicast = pcp_read_u64(values, j, netdev_metric_descs,
						       NET_PERINTF_INMULTICAST);
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write network interfaces errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_edev_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_net_edev *snedc;

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}
		slot = pcp_slot_for_item(a->item_list, snedc->interface);

		handle = ACT_HANDLE(m, NET_EPERINTF_INERRORS, slot);
		atom.ull = (unsigned long long)snedc->rx_errors;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_OUTERRORS, slot);
		atom.ull = (unsigned long long)snedc->tx_errors;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_COLLISIONS, slot);
		atom.ull = (unsigned long long)snedc->collisions;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_INDROPS, slot);
		atom.ull = (unsigned long long)snedc->rx_dropped;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_OUTDROPS, slot);
		atom.ull = (unsigned long long)snedc->tx_dropped;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_OUTCARRIER, slot);
		atom.ull = (unsigned long long)snedc->tx_carrier_errors;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_INFRAME, slot);
		atom.ull = (unsigned long long)snedc->rx_frame_errors;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_INFIFO, slot);
		atom.ull = (unsigned long long)snedc->rx_fifo_errors;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, NET_EPERINTF_OUTFIFO, slot);
		atom.ull = (unsigned long long)snedc->tx_fifo_errors;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read network interfaces errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_enetdev_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_net_edev *snedc;
	pmInDom indom = netedev_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + j * a->msize);

		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			strncpy(snedc->interface, name, MAX_IFACE_LEN - 1);
			snedc->interface[MAX_IFACE_LEN - 1] = '\0';
			free(name);
		}

		switch (values->pmid) {
		case PMID_NET_EPERINTF_INERRORS:
			snedc->rx_errors = pcp_read_u64(values, j, netedev_metric_descs,
							NET_EPERINTF_INERRORS);
			break;
		case PMID_NET_EPERINTF_OUTERRORS:
			snedc->tx_errors = pcp_read_u64(values, j, netedev_metric_descs,
							NET_EPERINTF_OUTERRORS);
			break;
		case PMID_NET_EPERINTF_COLLISIONS:
			snedc->collisions = pcp_read_u64(values, j, netedev_metric_descs,
							 NET_EPERINTF_COLLISIONS);
			break;
		case PMID_NET_EPERINTF_INDROPS:
			snedc->rx_dropped = pcp_read_u64(values, j, netedev_metric_descs,
							 NET_EPERINTF_INDROPS);
			break;
		case PMID_NET_EPERINTF_OUTDROPS:
			snedc->tx_dropped = pcp_read_u64(values, j, netedev_metric_descs,
							 NET_EPERINTF_OUTDROPS);
			break;
		case PMID_NET_EPERINTF_OUTCARRIER:
			snedc->tx_carrier_errors = pcp_read_u64(values, j, netedev_metric_descs,
								NET_EPERINTF_OUTCARRIER);
			break;
		case PMID_NET_EPERINTF_INFRAME:
			snedc->rx_frame_errors = pcp_read_u64(values, j, netedev_metric_descs,
							      NET_EPERINTF_INFRAME);
			break;
		case PMID_NET_EPERINTF_INFIFO:
			snedc->rx_fifo_errors = pcp_read_u64(values, j, netedev_metric_descs,
							     NET_EPERINTF_INFIFO);
			break;
		case PMID_NET_EPERINTF_OUTFIFO:
			snedc->tx_fifo_errors = pcp_read_u64(values, j, netedev_metric_descs,
							     NET_EPERINTF_OUTFIFO);
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write serial lines statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_serial_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	char serialno[64];
	struct stats_serial *ssc;

	for (i = 0; i < a->nr[curr]; i++) {

		ssc = (struct stats_serial *) ((char *) a->buf[curr] + i * a->msize);

		pmsprintf(serialno, sizeof(serialno), "serial%u", ssc->line);
		slot = pcp_slot_for_item(a->item_list, serialno);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_RX, slot);
		atom.ul = (unsigned long)ssc->rx;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_TX, slot);
		atom.ul = (unsigned long)ssc->tx;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_FRAME, slot);
		atom.ul = (unsigned long)ssc->frame;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_PARITY, slot);
		atom.ul = (unsigned long)ssc->parity;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_BRK, slot);
		atom.ul = (unsigned long)ssc->brk;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, SERIAL_PERTTY_OVERRUN, slot);
		atom.ul = (unsigned long)ssc->overrun;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read serial lines statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_serial_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_serial *ssc;
	pmInDom indom = serial_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		ssc = (struct stats_serial *) ((char *) a->buf[curr] + j * a->msize);

		/* Extract line number from the instance name (e.g. "serial0" -> 0) */
		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			sscanf(name, "serial%u", &ssc->line);
			free(name);
		}

		switch (values->pmid) {
		case PMID_SERIAL_PERTTY_RX:
			ssc->rx = pcp_read_u32(values, j, serial_metric_descs,
					       SERIAL_PERTTY_RX);
			break;
		case PMID_SERIAL_PERTTY_TX:
			ssc->tx = pcp_read_u32(values, j, serial_metric_descs,
					       SERIAL_PERTTY_TX);
			break;
		case PMID_SERIAL_PERTTY_FRAME:
			ssc->frame = pcp_read_u32(values, j, serial_metric_descs,
						  SERIAL_PERTTY_FRAME);
			break;
		case PMID_SERIAL_PERTTY_PARITY:
			ssc->parity = pcp_read_u32(values, j, serial_metric_descs,
						   SERIAL_PERTTY_PARITY);
			break;
		case PMID_SERIAL_PERTTY_BRK:
			ssc->brk = pcp_read_u32(values, j, serial_metric_descs,
						SERIAL_PERTTY_BRK);
			break;
		case PMID_SERIAL_PERTTY_OVERRUN:
			ssc->overrun = pcp_read_u32(values, j, serial_metric_descs,
						    SERIAL_PERTTY_OVERRUN);
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write NFS client statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfs_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr];

	atom.ul = (unsigned long)snnc->nfs_rpccnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_RPCCCNT, 0), &atom);

	atom.ul = (unsigned long)snnc->nfs_rpcretrans;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_RPCRETRANS, 0), &atom);

	atom.ul = (unsigned long)snnc->nfs_readcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_REQUESTS, pcp_slot_for_item(a->item_list, "read")), &atom);

	atom.ul = (unsigned long)snnc->nfs_writecnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_REQUESTS, pcp_slot_for_item(a->item_list, "write")), &atom);

	atom.ul = (unsigned long)snnc->nfs_accesscnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_REQUESTS, pcp_slot_for_item(a->item_list, "access")), &atom);

	atom.ul = (unsigned long)snnc->nfs_getattcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSCLIENT_REQUESTS, pcp_slot_for_item(a->item_list, "getattr")), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read NFS client statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_nfs_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NFSCLIENT_RPCCCNT:
		snnc->nfs_rpccnt = pcp_read_u32(values, 0,
						nfsclient_metric_descs,
						NFSCLIENT_RPCCCNT);
		break;

	case PMID_NFSCLIENT_RPCRETRANS:
		snnc->nfs_rpcretrans = pcp_read_u32(values, 0,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
		break;

	case PMID_NFSCLIENT_REQUESTS:
		for (i = 0; i < values->numval; i++) {
			switch (values->vlist[i].inst) {

			case NFS_REQUEST_READ:
				snnc->nfs_readcnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_WRITE:
				snnc->nfs_writecnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_ACCESS:
				snnc->nfs_accesscnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_GETATTR:
				snnc->nfs_getattcnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;
			}
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write NFS server statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfsd_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr];

	atom.ull = (unsigned long long)snndc->nfsd_rpccnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_RPCCNT, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_rpcbad;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_RPCBADCLNT, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_netcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_NETCNT, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_netudpcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_NETUDPCNT, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_nettcpcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_NETTCPCNT, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_rchits;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_RCHITS, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_rcmisses;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_RCMISSES, 0), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_readcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_REQUESTS, pcp_slot_for_item(a->item_list, "read")), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_writecnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_REQUESTS, pcp_slot_for_item(a->item_list, "write")), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_accesscnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_REQUESTS, pcp_slot_for_item(a->item_list, "access")), &atom);

	atom.ull = (unsigned long long)snndc->nfsd_getattcnt;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NFSSERVER_REQUESTS, pcp_slot_for_item(a->item_list, "getattr")), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read NFS server statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_nfsd_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NFSSERVER_RPCCNT:
		snndc->nfsd_rpccnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RPCCNT);
		break;

	case PMID_NFSSERVER_RPCBADCLNT:
		snndc->nfsd_rpcbad = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RPCBADCLNT);
		break;

	case PMID_NFSSERVER_NETCNT:
		snndc->nfsd_netcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETCNT);
		break;

	case PMID_NFSSERVER_NETUDPCNT:
		snndc->nfsd_netudpcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETUDPCNT);
		break;

	case PMID_NFSSERVER_NETTCPCNT:
		snndc->nfsd_nettcpcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETTCPCNT);
		break;

	case PMID_NFSSERVER_RCHITS:
		snndc->nfsd_rchits = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RCHITS);
		break;

	case PMID_NFSSERVER_RCMISSES:
		snndc->nfsd_rcmisses = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RCMISSES);
		break;

	case PMID_NFSSERVER_REQUESTS:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case NFS_REQUEST_READ:
				snndc->nfsd_readcnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_WRITE:
				snndc->nfsd_writecnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_ACCESS:
				snndc->nfsd_accesscnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_GETATTR:
				snndc->nfsd_getattcnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;
			}
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	atom.ul = (unsigned long)snsc->sock_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_TOTAL, 0), &atom);

	atom.ul = (unsigned long)snsc->tcp_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_TCPINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->udp_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_UDPINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->raw_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_RAWINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->frag_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_FRAGINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->tcp_tw;
	pmiPutAtomValueHandle(ACT_HANDLE(m, SOCKET_TCPTW, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read network sockets statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_sock_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	switch (values->pmid) {

	case PMID_SOCKET_TOTAL:
		snsc->sock_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TOTAL);
		break;

	case PMID_SOCKET_TCPINUSE:
		snsc->tcp_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TCPINUSE);
		break;

	case PMID_SOCKET_UDPINUSE:
		snsc->udp_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_UDPINUSE);
		break;

	case PMID_SOCKET_RAWINUSE:
		snsc->raw_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_RAWINUSE);
		break;

	case PMID_SOCKET_FRAGINUSE:
		snsc->frag_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_FRAGINUSE);
		break;

	case PMID_SOCKET_TCPTW:
		snsc->tcp_tw = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TCPTW);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write IP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr];

	atom.ull = (unsigned long long)snic->InReceives;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_INRECEIVES, 0), &atom);

	atom.ull = (unsigned long long)snic->ForwDatagrams;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_FORWDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snic->InDelivers;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_INDELIVERS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutRequests;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_OUTREQUESTS, 0), &atom);

	atom.ull = (unsigned long long)snic->ReasmReqds;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_REASMREQDS, 0), &atom);

	atom.ull = (unsigned long long)snic->ReasmOKs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_REASMOKS, 0), &atom);

	atom.ull = (unsigned long long)snic->FragOKs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_FRAGOKS, 0), &atom);

	atom.ull = (unsigned long long)snic->FragCreates;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP_FRAGCREATES, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read IP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_ip_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_IP_INRECEIVES:
		snic->InReceives = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_INRECEIVES);
		break;

	case PMID_NET_IP_FORWDATAGRAMS:
		snic->ForwDatagrams = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FORWDATAGRAMS);
		break;

	case PMID_NET_IP_INDELIVERS:
		snic->InDelivers = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_INDELIVERS);
		break;

	case PMID_NET_IP_OUTREQUESTS:
		snic->OutRequests = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_OUTREQUESTS);
		break;

	case PMID_NET_IP_REASMREQDS:
		snic->ReasmReqds = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_REASMREQDS);
		break;

	case PMID_NET_IP_REASMOKS:
		snic->ReasmOKs = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_REASMOKS);
		break;

	case PMID_NET_IP_FRAGOKS:
		snic->FragOKs = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FRAGOKS);
		break;

	case PMID_NET_IP_FRAGCREATES:
		snic->FragCreates = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FRAGCREATES);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write IP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr];

	atom.ull = (unsigned long long)sneic->InHdrErrors;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_INHDRERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InAddrErrors;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_INADDRERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InUnknownProtos;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_INUNKNOWNPROTOS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InDiscards;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_INDISCARDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutDiscards;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_OUTDISCARDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutNoRoutes;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_OUTNOROUTES, 0), &atom);

	atom.ull = (unsigned long long)sneic->ReasmFails;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_REASMFAILS, 0), &atom);

	atom.ull = (unsigned long long)sneic->FragFails;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP_FRAGFAILS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read IP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eip_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EIP_INHDRERRORS:
		sneic->InHdrErrors = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INHDRERRORS);
		break;

	case PMID_NET_EIP_INADDRERRORS:
		sneic->InAddrErrors = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INADDRERRORS);
		break;

	case PMID_NET_EIP_INUNKNOWNPROTOS:
		sneic->InUnknownProtos = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INUNKNOWNPROTOS);
		break;

	case PMID_NET_EIP_INDISCARDS:
		sneic->InDiscards = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INDISCARDS);
		break;

	case PMID_NET_EIP_OUTDISCARDS:
		sneic->OutDiscards = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_OUTDISCARDS);
		break;

	case PMID_NET_EIP_OUTNOROUTES:
		sneic->OutNoRoutes = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_OUTNOROUTES);
		break;

	case PMID_NET_EIP_REASMFAILS:
		sneic->ReasmFails = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_REASMFAILS);
		break;

	case PMID_NET_EIP_FRAGFAILS:
		sneic->FragFails = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_FRAGFAILS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write ICMP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr];

	atom.ull = (unsigned long long)snic->InMsgs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INMSGS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutMsgs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTMSGS, 0), &atom);

	atom.ull = (unsigned long long)snic->InEchos;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INECHOS, 0), &atom);

	atom.ull = (unsigned long long)snic->InEchoReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INECHOREPS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutEchos;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTECHOS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutEchoReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTECHOREPS, 0), &atom);

	atom.ull = (unsigned long long)snic->InTimestamps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INTIMESTAMPS, 0), &atom);

	atom.ull = (unsigned long long)snic->InTimestampReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INTIMESTAMPREPS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutTimestamps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTTIMESTAMPS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutTimestampReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTTIMESTAMPREPS, 0), &atom);

	atom.ull = (unsigned long long)snic->InAddrMasks;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INADDRMASKS, 0), &atom);

	atom.ull = (unsigned long long)snic->InAddrMaskReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_INADDRMASKREPS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutAddrMasks;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTADDRMASKS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutAddrMaskReps;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP_OUTADDRMASKREPS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read ICMP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_icmp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ICMP_INMSGS:
		snic->InMsgs = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INMSGS);
		break;

	case PMID_NET_ICMP_OUTMSGS:
		snic->OutMsgs = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTMSGS);
		break;

	case PMID_NET_ICMP_INECHOS:
		snic->InEchos = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INECHOS);
		break;

	case PMID_NET_ICMP_INECHOREPS:
		snic->InEchoReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INECHOREPS);
		break;

	case PMID_NET_ICMP_OUTECHOS:
		snic->OutEchos = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTECHOS);
		break;

	case PMID_NET_ICMP_OUTECHOREPS:
		snic->OutEchoReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTECHOREPS);
		break;

	case PMID_NET_ICMP_INTIMESTAMPS:
		snic->InTimestamps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INTIMESTAMPS);
		break;

	case PMID_NET_ICMP_INTIMESTAMPREPS:
		snic->InTimestampReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INTIMESTAMPREPS);
		break;

	case PMID_NET_ICMP_OUTTIMESTAMPS:
		snic->OutTimestamps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTTIMESTAMPS);
		break;

	case PMID_NET_ICMP_OUTTIMESTAMPREPS:
		snic->OutTimestampReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTTIMESTAMPREPS);
		break;

	case PMID_NET_ICMP_INADDRMASKS:
		snic->InAddrMasks = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INADDRMASKS);
		break;

	case PMID_NET_ICMP_INADDRMASKREPS:
		snic->InAddrMaskReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INADDRMASKREPS);
		break;

	case PMID_NET_ICMP_OUTADDRMASKS:
		snic->OutAddrMasks = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTADDRMASKS);
		break;

	case PMID_NET_ICMP_OUTADDRMASKREPS:
		snic->OutAddrMaskReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTADDRMASKREPS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write ICMP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr];

	atom.ull = (unsigned long long)sneic->InErrors;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutErrors;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InDestUnreachs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INDESTUNREACHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutDestUnreachs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTDESTUNREACHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InTimeExcds;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INTIMEEXCDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutTimeExcds;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTTIMEEXCDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InParmProbs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INPARMPROBS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutParmProbs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTPARMPROBS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InSrcQuenchs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INSRCQUENCHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutSrcQuenchs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTSRCQUENCHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InRedirects;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_INREDIRECTS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutRedirects;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP_OUTREDIRECTS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read ICMP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eicmp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EICMP_INERRORS:
		sneic->InErrors = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INERRORS);
		break;

	case PMID_NET_EICMP_OUTERRORS:
		sneic->OutErrors = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTERRORS);
		break;

	case PMID_NET_EICMP_INDESTUNREACHS:
		sneic->InDestUnreachs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INDESTUNREACHS);
		break;

	case PMID_NET_EICMP_OUTDESTUNREACHS:
		sneic->OutDestUnreachs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTDESTUNREACHS);
		break;

	case PMID_NET_EICMP_INTIMEEXCDS:
		sneic->InTimeExcds = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INTIMEEXCDS);
		break;

	case PMID_NET_EICMP_OUTTIMEEXCDS:
		sneic->OutTimeExcds = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTTIMEEXCDS);
		break;

	case PMID_NET_EICMP_INPARMPROBS:
		sneic->InParmProbs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INPARMPROBS);
		break;

	case PMID_NET_EICMP_OUTPARMPROBS:
		 sneic->OutParmProbs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTPARMPROBS);
		break;

	case PMID_NET_EICMP_INSRCQUENCHS:
		sneic->InSrcQuenchs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INSRCQUENCHS);
		break;

	case PMID_NET_EICMP_OUTSRCQUENCHS:
		sneic->OutSrcQuenchs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTSRCQUENCHS);
		break;

	case PMID_NET_EICMP_INREDIRECTS:
		sneic->InRedirects = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INREDIRECTS);
		break;

	case PMID_NET_EICMP_OUTREDIRECTS:
		sneic->OutRedirects = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTREDIRECTS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write TCP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_tcp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr];

	atom.ull = (unsigned long long)sntc->ActiveOpens;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_TCP_ACTIVEOPENS, 0), &atom);

	atom.ull = (unsigned long long)sntc->PassiveOpens;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_TCP_PASSIVEOPENS, 0), &atom);

	atom.ull = (unsigned long long)sntc->InSegs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_TCP_INSEGS, 0), &atom);

	atom.ull = (unsigned long long)sntc->OutSegs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_TCP_OUTSEGS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read TCP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_tcp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_TCP_ACTIVEOPENS:
		sntc->ActiveOpens = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_ACTIVEOPENS);
		break;

	case PMID_NET_TCP_PASSIVEOPENS:
		sntc->PassiveOpens = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_PASSIVEOPENS);
		break;

	case PMID_NET_TCP_INSEGS:
		sntc->InSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_INSEGS);
		break;

	case PMID_NET_TCP_OUTSEGS:
		sntc->OutSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_OUTSEGS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write TCP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_etcp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr];

	atom.ull = (unsigned long long)snetc->AttemptFails;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ETCP_ATTEMPTFAILS, 0), &atom);

	atom.ull = (unsigned long long)snetc->EstabResets;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ETCP_ESTABRESETS, 0), &atom);

	atom.ull = (unsigned long long)snetc->RetransSegs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ETCP_RETRANSSEGS, 0), &atom);

	atom.ull = (unsigned long long)snetc->InErrs;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ETCP_INERRS, 0), &atom);

	atom.ull = (unsigned long long)snetc->OutRsts;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ETCP_OUTRSTS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read TCP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_etcp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ETCP_ATTEMPTFAILS:
		snetc->AttemptFails = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_ATTEMPTFAILS);
		break;

	case PMID_NET_ETCP_ESTABRESETS:
		snetc->EstabResets = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_ESTABRESETS);
		break;

	case PMID_NET_ETCP_RETRANSSEGS:
		snetc->RetransSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_RETRANSSEGS);
		break;

	case PMID_NET_ETCP_INERRS:
		snetc->InErrs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_INERRS);
		break;

	case PMID_NET_ETCP_OUTRSTS:
		snetc->OutRsts = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_OUTRSTS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write UDP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr];

	atom.ull = (unsigned long long)snuc->InDatagrams;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP_INDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snuc->OutDatagrams;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP_OUTDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snuc->NoPorts;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP_NOPORTS, 0), &atom);

	atom.ull = (unsigned long long)snuc->InErrors;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP_INERRORS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read UDP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_udp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_UDP_INDATAGRAMS:
		snuc->InDatagrams = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_INDATAGRAMS);
		break;

	case PMID_NET_UDP_OUTDATAGRAMS:
		snuc->OutDatagrams = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_OUTDATAGRAMS);
		break;

	case PMID_NET_UDP_NOPORTS:
		snuc->NoPorts = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_NOPORTS);
		break;

	case PMID_NET_UDP_INERRORS:
		snuc->InErrors = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_INERRORS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write IPv6 network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	atom.ul = (unsigned long)snsc->tcp6_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_SOCK6_TCPINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->udp6_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_SOCK6_UDPINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->raw6_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_SOCK6_RAWINUSE, 0), &atom);

	atom.ul = (unsigned long)snsc->frag6_inuse;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_SOCK6_FRAGINUSE, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read IPv6 network sockets statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_sock6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_SOCK6_TCPINUSE:
		snsc->tcp6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_TCPINUSE);
		break;

	case PMID_NET_SOCK6_UDPINUSE:
		snsc->udp6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_UDPINUSE);
		break;

	case PMID_NET_SOCK6_RAWINUSE:
		snsc->raw6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_RAWINUSE);
		break;

	case PMID_NET_SOCK6_FRAGINUSE:
		snsc->frag6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_FRAGINUSE);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write IPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr];

	atom.ull = (unsigned long long)snic->InReceives6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_INRECEIVES, 0), &atom);

	atom.ull = (unsigned long long)snic->OutForwDatagrams6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_OUTFORWDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snic->InDelivers6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_INDELIVERS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutRequests6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_OUTREQUESTS, 0), &atom);

	atom.ull = (unsigned long long)snic->ReasmReqds6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_REASMREQDS, 0), &atom);

	atom.ull = (unsigned long long)snic->ReasmOKs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_REASMOKS, 0), &atom);

	atom.ull = (unsigned long long)snic->InMcastPkts6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_INMCASTPKTS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutMcastPkts6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_OUTMCASTPKTS, 0), &atom);

	atom.ull = (unsigned long long)snic->FragOKs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_FRAGOKS, 0), &atom);

	atom.ull = (unsigned long long)snic->FragCreates6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_IP6_FRAGCREATES, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read IPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_ip6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_IP6_INRECEIVES:
		snic->InReceives6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INRECEIVES);
		break;

	case PMID_NET_IP6_OUTFORWDATAGRAMS:
		snic->OutForwDatagrams6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTFORWDATAGRAMS);
		break;

	case PMID_NET_IP6_INDELIVERS:
		snic->InDelivers6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INDELIVERS);
		break;

	case PMID_NET_IP6_OUTREQUESTS:
		snic->OutRequests6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTREQUESTS);
		break;

	case PMID_NET_IP6_REASMREQDS:
		snic->ReasmReqds6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_REASMREQDS);
		break;

	case PMID_NET_IP6_REASMOKS:
		snic->ReasmOKs6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_REASMOKS);
		break;

	case PMID_NET_IP6_INMCASTPKTS:
		snic->InMcastPkts6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INMCASTPKTS);
		break;

	case PMID_NET_IP6_OUTMCASTPKTS:
		snic->OutMcastPkts6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTMCASTPKTS);
		break;

	case PMID_NET_IP6_FRAGOKS:
		snic->FragOKs6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_FRAGOKS);
		break;

	case PMID_NET_IP6_FRAGCREATES:
		snic->FragCreates6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_FRAGCREATES);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write IPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr];

	atom.ull = (unsigned long long)sneic->InHdrErrors6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INHDRERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InAddrErrors6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INADDRERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InUnknownProtos6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INUNKNOWNPROTOS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InTooBigErrors6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INTOOBIGERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InDiscards6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INDISCARDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutDiscards6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_OUTDISCARDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InNoRoutes6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INNOROUTES, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutNoRoutes6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_OUTNOROUTES, 0), &atom);

	atom.ull = (unsigned long long)sneic->ReasmFails6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_REASMFAILS, 0), &atom);

	atom.ull = (unsigned long long)sneic->FragFails6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_FRAGFAILS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InTruncatedPkts6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EIP6_INTRUNCATEDPKTS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read IPv6 network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eip6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EIP6_INHDRERRORS:
		sneic->InHdrErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INHDRERRORS);
		break;

	case PMID_NET_EIP6_INADDRERRORS:
		sneic->InAddrErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INADDRERRORS);
		break;

	case PMID_NET_EIP6_INUNKNOWNPROTOS:
		sneic->InUnknownProtos6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INUNKNOWNPROTOS);
		break;

	case PMID_NET_EIP6_INTOOBIGERRORS:
		sneic->InTooBigErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INTOOBIGERRORS);
		break;

	case PMID_NET_EIP6_INDISCARDS:
		sneic->InDiscards6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INDISCARDS);
		break;

	case PMID_NET_EIP6_OUTDISCARDS:
		sneic->OutDiscards6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_OUTDISCARDS);
		break;

	case PMID_NET_EIP6_INNOROUTES:
		sneic->InNoRoutes6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INNOROUTES);
		break;

	case PMID_NET_EIP6_OUTNOROUTES:
		sneic->OutNoRoutes6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_OUTNOROUTES);
		break;

	case PMID_NET_EIP6_REASMFAILS:
		sneic->ReasmFails6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_REASMFAILS);
		break;

	case PMID_NET_EIP6_FRAGFAILS:
		sneic->FragFails6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_FRAGFAILS);
		break;

	case PMID_NET_EIP6_INTRUNCATEDPKTS:
		sneic->InTruncatedPkts6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INTRUNCATEDPKTS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write ICMPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr];

	atom.ull = (unsigned long long)snic->InMsgs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INMSGS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutMsgs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTMSGS, 0), &atom);

	atom.ull = (unsigned long long)snic->InEchos6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INECHOS, 0), &atom);

	atom.ull = (unsigned long long)snic->InEchoReplies6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INECHOREPLIES, 0), &atom);

	atom.ull = (unsigned long long)snic->OutEchoReplies6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTECHOREPLIES, 0), &atom);

	atom.ull = (unsigned long long)snic->InGroupMembQueries6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INGROUPMEMBQUERIES, 0), &atom);

	atom.ull = (unsigned long long)snic->InGroupMembResponses6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INGROUPMEMBRESPONSES, 0), &atom);

	atom.ull = (unsigned long long)snic->OutGroupMembResponses6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTGROUPMEMBRESPONSES, 0), &atom);

	atom.ull = (unsigned long long)snic->InGroupMembReductions6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INGROUPMEMBREDUCTIONS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutGroupMembReductions6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTGROUPMEMBREDUCTIONS, 0), &atom);

	atom.ull = (unsigned long long)snic->InRouterSolicits6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INROUTERSOLICITS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutRouterSolicits6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTROUTERSOLICITS, 0), &atom);

	atom.ull = (unsigned long long)snic->InRouterAdvertisements6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INROUTERADVERTISEMENTS, 0), &atom);

	atom.ull = (unsigned long long)snic->InNeighborSolicits6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INNEIGHBORSOLICITS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutNeighborSolicits6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTNEIGHBORSOLICITS, 0), &atom);

	atom.ull = (unsigned long long)snic->InNeighborAdvertisements6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_INNEIGHBORADVERTISEMENTS, 0), &atom);

	atom.ull = (unsigned long long)snic->OutNeighborAdvertisements6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_ICMP6_OUTNEIGHBORADVERTISEMENTS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read ICMPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_icmp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ICMP6_INMSGS:
		snic->InMsgs6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INMSGS);
		break;

	case PMID_NET_ICMP6_OUTMSGS:
		snic->OutMsgs6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTMSGS);
		break;

	case PMID_NET_ICMP6_INECHOS:
		snic->InEchos6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INECHOS);
		break;

	case PMID_NET_ICMP6_INECHOREPLIES:
		snic->InEchoReplies6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INECHOREPLIES);
		break;

	case PMID_NET_ICMP6_OUTECHOREPLIES:
		snic->OutEchoReplies6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTECHOREPLIES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBQUERIES:
		snic->InGroupMembQueries6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBQUERIES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBRESPONSES:
		snic->InGroupMembResponses6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBRESPONSES);
		break;

	case PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES:
		snic->OutGroupMembResponses6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTGROUPMEMBRESPONSES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS:
		snic->InGroupMembReductions6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBREDUCTIONS);
		break;

	case PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS:
		snic->OutGroupMembReductions6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTGROUPMEMBREDUCTIONS);
		break;

	case PMID_NET_ICMP6_INROUTERSOLICITS:
		snic->InRouterSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INROUTERSOLICITS);
		break;

	case PMID_NET_ICMP6_OUTROUTERSOLICITS:
		snic->OutRouterSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTROUTERSOLICITS);
		break;

	case PMID_NET_ICMP6_INROUTERADVERTISEMENTS:
		snic->InRouterAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INROUTERADVERTISEMENTS);
		break;

	case PMID_NET_ICMP6_INNEIGHBORSOLICITS:
		snic->InNeighborSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INNEIGHBORSOLICITS);
		break;

	case PMID_NET_ICMP6_OUTNEIGHBORSOLICITS:
		snic->OutNeighborSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTNEIGHBORSOLICITS);
		break;

	case PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS:
		snic->InNeighborAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INNEIGHBORADVERTISEMENTS);
		break;

	case PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS:
		snic->OutNeighborAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTNEIGHBORADVERTISEMENTS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write ICMPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr];

	atom.ull = (unsigned long long)sneic->InErrors6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INERRORS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InDestUnreachs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INDESTUNREACHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutDestUnreachs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_OUTDESTUNREACHS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InTimeExcds6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INTIMEEXCDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutTimeExcds6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_OUTTIMEEXCDS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InParmProblems6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INPARMPROBLEMS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutParmProblems6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_OUTPARMPROBLEMS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InRedirects6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INREDIRECTS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutRedirects6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_OUTREDIRECTS, 0), &atom);

	atom.ull = (unsigned long long)sneic->InPktTooBigs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_INPKTTOOBIGS, 0), &atom);

	atom.ull = (unsigned long long)sneic->OutPktTooBigs6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_EICMP6_OUTPKTTOOBIGS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read ICMP6 network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eicmp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EICMP6_INERRORS:
		sneic->InErrors6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INERRORS);
		break;

	case PMID_NET_EICMP6_INDESTUNREACHS:
		sneic->InDestUnreachs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INDESTUNREACHS);
		break;

	case PMID_NET_EICMP6_OUTDESTUNREACHS:
		sneic->OutDestUnreachs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTDESTUNREACHS);
		break;

	case PMID_NET_EICMP6_INTIMEEXCDS:
		sneic->InTimeExcds6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INTIMEEXCDS);
		break;

	case PMID_NET_EICMP6_OUTTIMEEXCDS:
		sneic->OutTimeExcds6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTTIMEEXCDS);
		break;

	case PMID_NET_EICMP6_INPARMPROBLEMS:
		sneic->InParmProblems6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INPARMPROBLEMS);
		break;

	case PMID_NET_EICMP6_OUTPARMPROBLEMS:
		sneic->OutParmProblems6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTPARMPROBLEMS);
		break;

	case PMID_NET_EICMP6_INREDIRECTS:
		sneic->InRedirects6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INREDIRECTS);
		break;

	case PMID_NET_EICMP6_OUTREDIRECTS:
		sneic->OutRedirects6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTREDIRECTS);
		break;

	case PMID_NET_EICMP6_INPKTTOOBIGS:
		sneic->InPktTooBigs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INPKTTOOBIGS);
		break;

	case PMID_NET_EICMP6_OUTPKTTOOBIGS:
		sneic->OutPktTooBigs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTPKTTOOBIGS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write UDPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr];

	atom.ull = (unsigned long long)snuc->InDatagrams6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP6_INDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snuc->OutDatagrams6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP6_OUTDATAGRAMS, 0), &atom);

	atom.ull = (unsigned long long)snuc->NoPorts6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP6_NOPORTS, 0), &atom);

	atom.ull = (unsigned long long)snuc->InErrors6;
	pmiPutAtomValueHandle(ACT_HANDLE(m, NET_UDP6_INERRORS, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read UDPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_udp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_UDP6_INDATAGRAMS:
		snuc->InDatagrams6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_INDATAGRAMS);
		break;

	case PMID_NET_UDP6_OUTDATAGRAMS:
		snuc->OutDatagrams6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_OUTDATAGRAMS);
		break;

	case PMID_NET_UDP6_NOPORTS:
		snuc->NoPorts6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_NOPORTS);
		break;

	case PMID_NET_UDP6_INERRORS:
		snuc->InErrors6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_INERRORS);
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write CPU frequency statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_cpufreq_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	int i;
	struct stats_pwr_cpufreq *spc;
	char cpuno[64];

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!IS_CPU_SELECTED(a->bitmap->b_array, i))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			continue;
		}
		else {
			pmsprintf(cpuno, sizeof(cpuno), "cpu%d", i - 1);
		}

		atom.f = (float)((double) spc->cpufreq) / 100;
		pmiPutAtomValueHandle(ACT_HANDLE(m, POWER_PERCPU_CLOCK, pcp_find_slot(m, i - 1)), &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read CPU frequency statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_pwr_cpufreq_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j, n;
	struct stats_pwr_cpufreq *spc;

	if (values->pmid != PMID_POWER_PERCPU_CLOCK)
		return;

	n = values->numval + 1;	/* slot 0 = "all" (unused), slots 1..N = per-CPU */
	if (n > a->nr_allocated)
		reallocate_buffers(a, n, flags);
	a->nr[curr] = n;

	for (j = 0; j < values->numval; j++) {
		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + (j + 1) * a->msize);
		spc->cpufreq = (unsigned long)
			pcp_read_u64(values, j, power_cpu_metric_descs,
				     POWER_PERCPU_CLOCK);
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write fan statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_fan_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_pwr_fan *spc;
	char instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "fan%d", i + 1);
		slot = pcp_slot_for_item(a->item_list, instance);

		handle = ACT_HANDLE(m, POWER_FAN_RPM, slot);
		atom.ull = (unsigned long long)spc->rpm;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_FAN_DRPM, slot);
		atom.ull = (unsigned long long)(spc->rpm - spc->rpm_min);
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_FAN_DEVICE, slot);
		atom.cp = spc->device;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read fan statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_fan_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_pwr_fan *spc;
	char *str;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + j * a->msize);
		switch (values->pmid) {
		case PMID_POWER_FAN_RPM:
			spc->rpm = pcp_read_double(values, j, power_fan_metric_descs,
						  POWER_FAN_RPM);
			break;
		case PMID_POWER_FAN_DRPM:
			/* drpm = rpm - rpm_min; reconstruct rpm_min after rpm is read */
			spc->rpm_min = spc->rpm -
				pcp_read_double(values, j, power_fan_metric_descs,
						POWER_FAN_DRPM);
			break;
		case PMID_POWER_FAN_DEVICE:
			str = pcp_read_str(values, j, power_fan_metric_descs,
					   POWER_FAN_DEVICE);
			if (str) {
				strncpy(spc->device, str, MAX_SENSORS_DEV_LEN - 1);
				spc->device[MAX_SENSORS_DEV_LEN - 1] = '\0';
				free(str);
			}
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write temperature statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_temp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_pwr_temp *spc;
	char instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "temp%d", i + 1);
		slot = pcp_slot_for_item(a->item_list, instance);

		handle = ACT_HANDLE(m, POWER_TEMP_CELSIUS, slot);
		atom.f = (float)spc->temp;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_TEMP_PERCENT, slot);
		atom.f = (float)(spc->temp_max - spc->temp_min) ?
			 (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			 0.0;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_TEMP_DEVICE, slot);
		atom.cp = spc->device;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read temperature statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_temp_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_pwr_temp *spc;
	char *str;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + j * a->msize);
		switch (values->pmid) {
		case PMID_POWER_TEMP_CELSIUS:
			spc->temp = pcp_read_double(values, j, power_temp_metric_descs,
						   POWER_TEMP_CELSIUS);
			break;
		case PMID_POWER_TEMP_PERCENT:
			/* Derived from temp/min/max; min/max not recoverable — skip */
			break;
		case PMID_POWER_TEMP_DEVICE:
			str = pcp_read_str(values, j, power_temp_metric_descs,
					   POWER_TEMP_DEVICE);
			if (str) {
				strncpy(spc->device, str, MAX_SENSORS_DEV_LEN - 1);
				spc->device[MAX_SENSORS_DEV_LEN - 1] = '\0';
				free(str);
			}
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write voltage inputs statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_in_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_pwr_in *spc;
	char instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "in%d", i);
		slot = pcp_slot_for_item(a->item_list, instance);

		handle = ACT_HANDLE(m, POWER_IN_VOLTAGE, slot);
		atom.f = (float)spc->in;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_IN_PERCENT, slot);
		atom.f = (float)(spc->in_max - spc->in_min) ?
			 (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			 0.0;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_IN_DEVICE, slot);
		atom.cp = spc->device;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read voltage input statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_in_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_pwr_in *spc;
	char *str;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + j * a->msize);
		switch (values->pmid) {
		case PMID_POWER_IN_VOLTAGE:
			spc->in = pcp_read_double(values, j, power_in_metric_descs,
						 POWER_IN_VOLTAGE);
			break;
		case PMID_POWER_IN_PERCENT:
			/* Derived from in/min/max; not recoverable — skip */
			break;
		case PMID_POWER_IN_DEVICE:
			str = pcp_read_str(values, j, power_in_metric_descs,
					   POWER_IN_DEVICE);
			if (str) {
				strncpy(spc->device, str, MAX_SENSORS_DEV_LEN - 1);
				spc->device[MAX_SENSORS_DEV_LEN - 1] = '\0';
				free(str);
			}
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 * **************************************************************************
 * Write batteries statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_bat_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_pwr_bat *spbc;
	char bat_name[16];

	for (i = 0; i < a->nr[curr]; i++) {

		spbc = (struct stats_pwr_bat *) ((char *) a->buf[curr] + i * a->msize);

		pmsprintf(bat_name, sizeof(bat_name), "BAT%d", (int) spbc->bat_id);
		slot = pcp_slot_for_item(a->item_list, bat_name);

		handle = ACT_HANDLE(m, POWER_BAT_CAPACITY, slot);
		atom.ul = (unsigned long)(unsigned int) spbc->capacity;
		pmiPutAtomValueHandle(handle, &atom);

		/* Battery status code should not be greater than or equal to BAT_STS_NR */
		if (spbc->status >= BAT_STS_NR) {
			spbc->status = 0;
		}

		handle = ACT_HANDLE(m, POWER_BAT_STATUS, slot);
		atom.cp = (char *)bat_status[(unsigned int) spbc->status];
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read batteries statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_bat_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i, j;
	struct stats_pwr_bat *spbc;
	pmInDom indom = power_bat_metric_descs[0].indom;
	char *name;
	char *str;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		spbc = (struct stats_pwr_bat *) ((char *) a->buf[curr] + j * a->msize);

		/* Extract battery ID from instance name (e.g. "BAT0" -> 0) */
		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			sscanf(name, "BAT%hhd", &spbc->bat_id);
			free(name);
		}

		switch (values->pmid) {
		case PMID_POWER_BAT_CAPACITY:
			spbc->capacity = (char) pcp_read_u32(values, j,
							     power_bat_metric_descs,
							     POWER_BAT_CAPACITY);
			break;
		case PMID_POWER_BAT_STATUS:
			str = pcp_read_str(values, j, power_bat_metric_descs,
					   POWER_BAT_STATUS);
			spbc->status = 0;
			for (i = 0; str && i < BAT_STS_NR; i++) {
				if (!strcmp(str, bat_status[i]))
					spbc->status = i;
			}
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write huge pages statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_huge_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	atom.ull = (unsigned long long)smc->frhkb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_HUGE_FREEBYTES, 0), &atom);

	atom.ull = (unsigned long long)smc->tlhkb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_HUGE_TOTALBYTES, 0), &atom);

	atom.ull = (unsigned long long)smc->rsvdhkb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_HUGE_RSVDBYTES, 0), &atom);

	atom.ull = (unsigned long long)smc->surphkb * 1024;
	pmiPutAtomValueHandle(ACT_HANDLE(m, MEM_HUGE_SURPBYTES, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read huge pages statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_huge_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	switch (values->pmid) {

	case PMID_MEM_HUGE_TOTALBYTES:
		smc->tlhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_TOTALBYTES);
		smc->tlhkb /= 1024;
		break;

	case PMID_MEM_HUGE_FREEBYTES:
		smc->frhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_FREEBYTES);
		smc->frhkb /= 1024;
		break;

	case PMID_MEM_HUGE_RSVDBYTES:
		smc->rsvdhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_RSVDBYTES);
		smc->rsvdhkb /= 1024;
		break;

	case PMID_MEM_HUGE_SURPBYTES:
		smc->surphkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_SURPBYTES);
		smc->surphkb /= 1024;
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write USB devices in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_usb_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_pwr_usb *suc;
	char name[64];
	char vendorid[16], productid[16];

	for (i = 0; i < a->nr[curr]; i++) {

		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);
		pmsprintf(name, sizeof(name), "usb%d", i);
		slot = pcp_slot_for_item(a->item_list, name);

		handle = ACT_HANDLE(m, POWER_USB_BUS, slot);
		atom.ul = (unsigned long)suc->bus_nr;
		pmiPutAtomValueHandle(handle, &atom);

		pmsprintf(vendorid, sizeof(vendorid), "%x", suc->vendor_id);
		handle = ACT_HANDLE(m, POWER_USB_VENDORID, slot);
		atom.cp = vendorid;
		pmiPutAtomValueHandle(handle, &atom);

		pmsprintf(productid, sizeof(productid), "%x", suc->product_id);
		handle = ACT_HANDLE(m, POWER_USB_PRODUCTID, slot);
		atom.cp = productid;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_USB_MAXPOWER, slot);
		atom.ul = (unsigned long)suc->bmaxpower << 1;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_USB_MANUFACTURER, slot);
		atom.cp = suc->manufacturer;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, POWER_USB_PRODUCTNAME, slot);
		atom.cp = suc->product;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read USB devices from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_usb_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_pwr_usb *spu;
	char *str;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		spu = (struct stats_pwr_usb *) ((char *) a->buf[curr] + j * a->msize);
		switch (values->pmid) {
		case PMID_POWER_USB_BUS:
			spu->bus_nr = pcp_read_u32(values, j, power_usb_metric_descs,
						   POWER_USB_BUS);
			break;
		case PMID_POWER_USB_VENDORID:
			spu->vendor_id = pcp_read_u32(values, j, power_usb_metric_descs,
						      POWER_USB_VENDORID);
			break;
		case PMID_POWER_USB_PRODUCTID:
			spu->product_id = pcp_read_u32(values, j, power_usb_metric_descs,
						       POWER_USB_PRODUCTID);
			break;
		case PMID_POWER_USB_MAXPOWER:
			spu->bmaxpower = pcp_read_u32(values, j, power_usb_metric_descs,
						      POWER_USB_MAXPOWER);
			break;
		case PMID_POWER_USB_MANUFACTURER:
			str = pcp_read_str(values, j, power_usb_metric_descs,
					   POWER_USB_MANUFACTURER);
			if (str) {
				strncpy(spu->manufacturer, str, MAX_MANUF_LEN - 1);
				spu->manufacturer[MAX_MANUF_LEN - 1] = '\0';
				free(str);
			}
			break;
		case PMID_POWER_USB_PRODUCTNAME:
			str = pcp_read_str(values, j, power_usb_metric_descs,
					   POWER_USB_PRODUCTNAME);
			if (str) {
				strncpy(spu->product, str, MAX_PROD_LEN - 1);
				spu->product[MAX_PROD_LEN - 1] = '\0';
				free(str);
			}
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write filesystem statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_filesystem_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_filesystem *sfc;
	char *dev_name;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		/* Get name to display (persistent or standard fs name, or mount point) */
		dev_name = get_fs_name_to_display(a, flags, sfc);

		/* Match --fs= against device name and/or mountpoint */
		if (!match_sa_filesystem_item(a->item_list, sfc, dev_name))
			continue;
		slot = pcp_slot_for_item(a->item_list, dev_name);

		handle = ACT_HANDLE(m, FILESYS_CAPACITY, slot);
		atom.ull = (unsigned long long)sfc->f_blocks / 1024;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_FREE, slot);
		atom.ull = (unsigned long long)sfc->f_bfree / 1024;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_USED, slot);
		atom.ull = (unsigned long long)(sfc->f_blocks - sfc->f_bfree) / 1024;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_FULL, slot);
		atom.d = (double)sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				       : 0.0;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_MAXFILES, slot);
		atom.ull = (unsigned long long)sfc->f_files;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_FREEFILES, slot);
		atom.ull = (unsigned long long)sfc->f_ffree;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_USEDFILES, slot);
		atom.ull = (unsigned long long)sfc->f_files - sfc->f_ffree;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FILESYS_AVAIL, slot);
		atom.ull = (unsigned long long)sfc->f_bavail / 1024;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read filesystem statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_filesystem_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_filesystem *sfc;
	pmInDom indom = filesys_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + j * a->msize);

		/* Store the filesystem name from the PCP instance */
		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			strncpy(sfc->fs_name, name, MAX_FS_LEN - 1);
			sfc->fs_name[MAX_FS_LEN - 1] = '\0';
			free(name);
		}

		switch (values->pmid) {
		case PMID_FILESYS_CAPACITY:
			/* Written as f_blocks / 1024; restore in 512-byte blocks */
			sfc->f_blocks = pcp_read_u64(values, j, filesys_metric_descs,
						     FILESYS_CAPACITY) * 1024;
			break;
		case PMID_FILESYS_FREE:
			sfc->f_bfree = pcp_read_u64(values, j, filesys_metric_descs,
						    FILESYS_FREE) * 1024;
			break;
		case PMID_FILESYS_USED:
			/* Derived (f_blocks - f_bfree); skip to avoid overwriting */
			break;
		case PMID_FILESYS_FULL:
			/* Percentage derived from blocks; skip */
			break;
		case PMID_FILESYS_MAXFILES:
			sfc->f_files = pcp_read_u64(values, j, filesys_metric_descs,
						    FILESYS_MAXFILES);
			break;
		case PMID_FILESYS_FREEFILES:
			sfc->f_ffree = pcp_read_u64(values, j, filesys_metric_descs,
						    FILESYS_FREEFILES);
			break;
		case PMID_FILESYS_USEDFILES:
			/* Derived (f_files - f_ffree); skip */
			break;
		case PMID_FILESYS_AVAIL:
			sfc->f_bavail = pcp_read_u64(values, j, filesys_metric_descs,
						     FILESYS_AVAIL) * 1024;
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write Fibre Channel HBA statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_fchost_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	size_t slot;
	int i, handle;
	struct stats_fchost *sfcc;

	for (i = 0; i < a->nr[curr]; i++) {

		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);
		slot = pcp_slot_for_item(a->item_list, sfcc->fchost_name);

		handle = ACT_HANDLE(m, FCHOST_INFRAMES, slot);
		atom.ull = (unsigned long long)sfcc->f_rxframes;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FCHOST_OUTFRAMES, slot);
		atom.ull = (unsigned long long)sfcc->f_txframes;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FCHOST_INBYTES, slot);
		atom.ull = (unsigned long long)sfcc->f_rxwords * 4;
		pmiPutAtomValueHandle(handle, &atom);

		handle = ACT_HANDLE(m, FCHOST_OUTBYTES, slot);
		atom.ull = (unsigned long long)sfcc->f_txwords * 4;
		pmiPutAtomValueHandle(handle, &atom);
	}
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read Fibre Channel HBA statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_fchost_stats(pmValueSet *values, struct activity *a, int curr)
{
	int j;
	struct stats_fchost *sfcc;
	pmInDom indom = fchost_metric_descs[0].indom;
	char *name;

	pcp_reallocate_buffers(values, a, curr);

	for (j = 0; j < values->numval; j++) {
		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + j * a->msize);

		if (pmNameInDom(indom, values->vlist[j].inst, &name) >= 0) {
			strncpy(sfcc->fchost_name, name, MAX_FCH_LEN - 1);
			sfcc->fchost_name[MAX_FCH_LEN - 1] = '\0';
			free(name);
		}

		switch (values->pmid) {
		case PMID_FCHOST_INFRAMES:
			sfcc->f_rxframes = (unsigned long)
				pcp_read_u64(values, j, fchost_metric_descs,
					     FCHOST_INFRAMES);
			break;
		case PMID_FCHOST_OUTFRAMES:
			sfcc->f_txframes = (unsigned long)
				pcp_read_u64(values, j, fchost_metric_descs,
					     FCHOST_OUTFRAMES);
			break;
		case PMID_FCHOST_INBYTES:
			/* Written as f_rxwords * 4; restore words */
			sfcc->f_rxwords = (unsigned long)
				pcp_read_u64(values, j, fchost_metric_descs,
					     FCHOST_INBYTES) / 4;
			break;
		case PMID_FCHOST_OUTBYTES:
			sfcc->f_txwords = (unsigned long)
				pcp_read_u64(values, j, fchost_metric_descs,
					     FCHOST_OUTBYTES) / 4;
			break;
		}
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write pressure-stall CPU statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psicpu_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr];

	atom.f = (float)psic->some_acpu_10 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_CPU_SOMEAVG, pcp_find_slot(m, 10)), &atom);

	atom.f = (float)psic->some_acpu_60 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_CPU_SOMEAVG, pcp_find_slot(m, 60)), &atom);

	atom.f = (float)psic->some_acpu_300 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_CPU_SOMEAVG, pcp_find_slot(m, 300)), &atom);

	atom.ull = (unsigned long long)psic->some_cpu_total;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_CPU_SOMETOTAL, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read pressure-stall CPU statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psicpu_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_CPU_SOMETOTAL:
		psic->some_cpu_total = pcp_read_u64(values, 0,
						psi_cpu_metric_descs,
						PSI_CPU_SOMETOTAL);
		break;

	case PMID_PSI_CPU_SOMEAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psic->some_acpu_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psic->some_acpu_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psic->some_acpu_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;
			}
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write pressure-stall I/O statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psiio_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr];

	atom.f = (float)psic->some_aio_10 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_SOMEAVG, pcp_find_slot(m, 10)), &atom);

	atom.f = (float)psic->some_aio_60 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_SOMEAVG, pcp_find_slot(m, 60)), &atom);

	atom.f = (float)psic->some_aio_300 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_SOMEAVG, pcp_find_slot(m, 300)), &atom);

	atom.ull = (unsigned long long)psic->some_io_total;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_SOMETOTAL, 0), &atom);

	atom.f = (float)psic->full_aio_10 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_FULLAVG, pcp_find_slot(m, 10)), &atom);

	atom.f = (float)psic->full_aio_60 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_FULLAVG, pcp_find_slot(m, 60)), &atom);

	atom.f = (float)psic->full_aio_300 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_FULLAVG, pcp_find_slot(m, 300)), &atom);

	atom.ull = (unsigned long long)psic->full_io_total;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_IO_FULLTOTAL, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read pressure-stall I/O statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psiio_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_io
		*psiio = (struct stats_psi_io *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_IO_SOMETOTAL:
		psiio->some_io_total = pcp_read_u64(values, 0,
						psi_io_metric_descs,
						PSI_IO_SOMETOTAL);
		break;

	case PMID_PSI_IO_SOMEAVG:
		for (i = 0; i < values->numval; i++) {

		       	switch (values->vlist[i].inst) {
			case 10:	/* 10 second */
				psiio->some_aio_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psiio->some_aio_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psiio->some_aio_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;
			}
		}
		break;

	case PMID_PSI_IO_FULLTOTAL:
		psiio->full_io_total = pcp_read_u64(values, 0,
						psi_io_metric_descs,
						PSI_IO_FULLTOTAL);
		break;

	case PMID_PSI_IO_FULLAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psiio->full_aio_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;

			case 60:	/* 1 minute */
				psiio->full_aio_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;

			case 300:	/* 5 minute */
				psiio->full_aio_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;
			}
		}
		break;
	}
}
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Write pressure-stall memory statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psimem_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	struct act_metrics *m = a->metrics;
	pmAtomValue atom;
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr];

	atom.f = (float)psic->some_amem_10 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_SOMEAVG, pcp_find_slot(m, 10)), &atom);

	atom.f = (float)psic->some_amem_60 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_SOMEAVG, pcp_find_slot(m, 60)), &atom);

	atom.f = (float)psic->some_amem_300 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_SOMEAVG, pcp_find_slot(m, 300)), &atom);

	atom.ull = (unsigned long long)psic->some_mem_total;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_SOMETOTAL, 0), &atom);

	atom.f = (float)psic->full_amem_10 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_FULLAVG, pcp_find_slot(m, 10)), &atom);

	atom.f = (float)psic->full_amem_60 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_FULLAVG, pcp_find_slot(m, 60)), &atom);

	atom.f = (float)psic->full_amem_300 / 100;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_FULLAVG, pcp_find_slot(m, 300)), &atom);

	atom.ull = (unsigned long long)psic->full_mem_total;
	pmiPutAtomValueHandle(ACT_HANDLE(m, PSI_MEM_FULLTOTAL, 0), &atom);
#endif /* HAVE_PCP */
}

#ifdef HAVE_PCP
/*
 ***************************************************************************
 * Read pressure-stall memory statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psimem_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_mem
		*psim = (struct stats_psi_mem *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_MEM_SOMETOTAL:
		psim->some_mem_total = pcp_read_u64(values, 0,
						psi_mem_metric_descs,
						PSI_MEM_SOMETOTAL);
		break;

	case PMID_PSI_MEM_SOMEAVG:
		for (i = 0; i < values->numval; i++) {

		       	switch (values->vlist[i].inst) {
			case 10:	/* 10 second */
				psim->some_amem_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psim->some_amem_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psim->some_amem_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;
			}
		}
		break;

	case PMID_PSI_MEM_FULLTOTAL:
		psim->full_mem_total = pcp_read_u64(values, 0,
						psi_mem_metric_descs,
						PSI_MEM_FULLTOTAL);
		break;

	case PMID_PSI_MEM_FULLAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psim->full_amem_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;

			case 60:	/* 1 minute */
				psim->full_amem_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;

			case 300:	/* 5 minute */
				psim->full_amem_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Read PCP metric valuesets into corresponding file header fields.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @file_hdr	File header structure housing global values.
 ***************************************************************************
 */
void pcp_read_file_header_stats(pmValueSet *values, struct file_header *file_hdr)
{
	char		*s;

	/*
	 * Metrics that augment the information from the PCP archive label.
	 * The label provides sa_ust_time (start time) and sa_tzname ($TZ).
	 */

	switch (values->pmid) {

		case PMID_FILE_HEADER_CPU_COUNT:
			file_hdr->sa_cpu_nr = pcp_read_u32(values, 0,
						file_header_metrics.descs,
						FILE_HEADER_CPU_COUNT);
			break;

		case PMID_FILE_HEADER_KERNEL_HERTZ:
			file_hdr->sa_hz = pcp_read_u32(values, 0,
						file_header_metrics.descs,
						FILE_HEADER_KERNEL_HERTZ);
			break;

		case PMID_FILE_HEADER_UNAME_SYSNAME:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_SYSNAME);
			pmsprintf(file_hdr->sa_sysname, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_RELEASE:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_RELEASE);
			pmsprintf(file_hdr->sa_release, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_NODENAME:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_NODENAME);
			pmsprintf(file_hdr->sa_nodename, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_MACHINE:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_MACHINE);
			pmsprintf(file_hdr->sa_machine, UTSNAME_LEN, "%s", s);
			free(s);
			break;
	}
}

/*
 ***************************************************************************
 * Read PCP metric valuesets into corresponding record header fields.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_record_header_stats(pmValueSet *values, int curr)
{
	/*
	 * Metrics that augment the information from the PCP archive label.
	 * The label provides sa_ust_time (start time) and sa_tzname ($TZ).
	 */
	switch (values->pmid) {

		case PMID_RECORD_HEADER_KERNEL_UPTIME:
			record_hdr[curr].uptime_cs = (unsigned long long)
					(100.0 * pcp_read_double(values, 0,
						record_header_metrics.descs,
						RECORD_HEADER_KERNEL_UPTIME));
			break;
	}
}

/*
 ***************************************************************************
 * Insert PCP metric valuesets into corresponding activity buffers,
 * and/or file header structure.
 *
 * As efficiently as possible; uses a single switch statement across
 * the globally unique pmID field - an unsigned integer - allows the
 * compiler to optimise lookup for us without ancillary structures.
 * The use of a single case statement has the additional benefit of
 * ensuring we never (accidentally) get duplicate metric identifiers
 * in the internal tables.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @header	File header structure housing global values.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_stats(pmValueSet *values, struct file_header *header, int curr)
{
	int p;

	if (values->numval <= 0)
		return;

	switch (values->pmid) {

		case PMID_FILE_HEADER_CPU_COUNT:
		case PMID_FILE_HEADER_KERNEL_HERTZ:
		case PMID_FILE_HEADER_UNAME_RELEASE:
		case PMID_FILE_HEADER_UNAME_SYSNAME:
		case PMID_FILE_HEADER_UNAME_NODENAME:
		case PMID_FILE_HEADER_UNAME_MACHINE:
			pcp_read_file_header_stats(values, header);
			break;

		case PMID_RECORD_HEADER_KERNEL_UPTIME:
			pcp_read_record_header_stats(values, curr);
			break;

		case PMID_CPU_ALLCPU_USER:
		case PMID_CPU_ALLCPU_SYS:
		case PMID_CPU_ALLCPU_NICE:
		case PMID_CPU_ALLCPU_IDLE:
		case PMID_CPU_ALLCPU_WAITTOTAL:
		case PMID_CPU_ALLCPU_IRQTOTAL:
		case PMID_CPU_ALLCPU_IRQSOFT:
		case PMID_CPU_ALLCPU_IRQHARD:
		case PMID_CPU_ALLCPU_STEAL:
		case PMID_CPU_ALLCPU_GUEST:
		case PMID_CPU_ALLCPU_GUESTNICE:
		case PMID_CPU_PERCPU_USER:
		case PMID_CPU_PERCPU_NICE:
		case PMID_CPU_PERCPU_SYS:
		case PMID_CPU_PERCPU_IDLE:
		case PMID_CPU_PERCPU_WAITTOTAL:
		case PMID_CPU_PERCPU_CPU_INTR:
		case PMID_CPU_PERCPU_IRQSOFT:
		case PMID_CPU_PERCPU_IRQHARD:
		case PMID_CPU_PERCPU_STEAL:
		case PMID_CPU_PERCPU_GUEST:
		case PMID_CPU_PERCPU_GUESTNICE:
		case PMID_CPU_PERCPU_INTERRUPTS:
			p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);
			pcp_read_cpu_stats(values, act[p], curr);
			break;

		case PMID_POWER_PERCPU_CLOCK:
			p = get_activity_position(act, A_PWR_CPU, EXIT_IF_NOT_FOUND);
			pcp_read_pwr_cpufreq_stats(values, act[p], curr);
			break;

		case PMID_SOFTNET_ALLCPU_PROCESSED:
		case PMID_SOFTNET_ALLCPU_DROPPED:
		case PMID_SOFTNET_ALLCPU_TIMESQUEEZE:
		case PMID_SOFTNET_ALLCPU_RECEIVEDRPS:
		case PMID_SOFTNET_ALLCPU_FLOWLIMIT:
		case PMID_SOFTNET_ALLCPU_BACKLOGLENGTH:
		case PMID_SOFTNET_PERCPU_PROCESSED:
		case PMID_SOFTNET_PERCPU_DROPPED:
		case PMID_SOFTNET_PERCPU_TIMESQUEEZE:
		case PMID_SOFTNET_PERCPU_RECEIVEDRPS:
		case PMID_SOFTNET_PERCPU_FLOWLIMIT:
		case PMID_SOFTNET_PERCPU_BACKLOGLENGTH:
			p = get_activity_position(act, A_NET_SOFT, EXIT_IF_NOT_FOUND);
			pcp_read_softnet_stats(values, act[p], curr);
			break;

		case PMID_PCSW_CONTEXT_SWITCH:
		case PMID_PCSW_FORK_SYSCALLS:
			p = get_activity_position(act, A_PCSW, EXIT_IF_NOT_FOUND);
			pcp_read_pcsw_stats(values, act[p], curr);
			break;

		case PMID_IRQ_ALLIRQ_TOTAL:
		case PMID_IRQ_PERIRQ_TOTAL:
			p = get_activity_position(act, A_IRQ, EXIT_IF_NOT_FOUND);
			pcp_read_irq_stats(values, act[p], curr);
			break;

		case PMID_SWAP_PAGESIN:
		case PMID_SWAP_PAGESOUT:
			p = get_activity_position(act, A_SWAP, EXIT_IF_NOT_FOUND);
			pcp_read_swap_stats(values, act[p], curr);
			break;

		case PMID_PAGING_PGPGIN:
		case PMID_PAGING_PGPGOUT:
		case PMID_PAGING_PGFAULT:
		case PMID_PAGING_PGMAJFAULT:
		case PMID_PAGING_PGFREE:
		case PMID_PAGING_PGSCANDIRECT:
		case PMID_PAGING_PGSCANKSWAPD:
		case PMID_PAGING_PGSTEAL:
		case PMID_PAGING_PGDEMOTE:
		case PMID_PAGING_PGPROMOTE:
			p = get_activity_position(act, A_PAGE, EXIT_IF_NOT_FOUND);
			pcp_read_paging_stats(values, act[p], curr);
			break;

		case PMID_IO_ALLDEV_TOTAL:
		case PMID_IO_ALLDEV_READ:
		case PMID_IO_ALLDEV_WRITE:
		case PMID_IO_ALLDEV_DISCARD:
		case PMID_IO_ALLDEV_READBYTES:
		case PMID_IO_ALLDEV_WRITEBYTES:
		case PMID_IO_ALLDEV_DISCARDBYTES:
			p = get_activity_position(act, A_IO, EXIT_IF_NOT_FOUND);
			pcp_read_io_stats(values, act[p], curr);
			break;

		case PMID_MEM_PHYS_MB:
		case PMID_MEM_PHYS_KB:
		case PMID_MEM_UTIL_FREE:
		case PMID_MEM_UTIL_SHARED:
		case PMID_MEM_UTIL_AVAIL:
		case PMID_MEM_UTIL_USED:
		case PMID_MEM_UTIL_BUFFER:
		case PMID_MEM_UTIL_CACHED:
		case PMID_MEM_UTIL_COMMITAS:
		case PMID_MEM_UTIL_ACTIVE:
		case PMID_MEM_UTIL_INACTIVE:
		case PMID_MEM_UTIL_DIRTY:
		case PMID_MEM_UTIL_ANON:
		case PMID_MEM_UTIL_SLAB:
		case PMID_MEM_UTIL_KSTACK:
		case PMID_MEM_UTIL_PGTABLE:
		case PMID_MEM_UTIL_VMALLOC:
		case PMID_MEM_UTIL_SWAPFREE:
		case PMID_MEM_UTIL_SWAPTOTAL:
		case PMID_MEM_UTIL_SWAPCACHED:
			p = get_activity_position(act, A_MEMORY, EXIT_IF_NOT_FOUND);
			pcp_read_memory_stats(values, act[p], curr);
			break;

		case PMID_KTABLE_DENTRYS:
		case PMID_KTABLE_FILES:
		case PMID_KTABLE_INODES:
		case PMID_KTABLE_PTYS:
			p = get_activity_position(act, A_KTABLES, EXIT_IF_NOT_FOUND);
			pcp_read_ktable_stats(values, act[p], curr);
			break;

		case PMID_KQUEUE_RUNNABLE:
		case PMID_KQUEUE_PROCESSES:
		case PMID_KQUEUE_BLOCKED:
		case PMID_KQUEUE_LOADAVG:
			p = get_activity_position(act, A_QUEUE, EXIT_IF_NOT_FOUND);
			pcp_read_kqueue_stats(values, act[p], curr);
			break;

		case PMID_DISK_PERDEV_READ:
		case PMID_DISK_PERDEV_WRITE:
		case PMID_DISK_PERDEV_TOTAL:
		case PMID_DISK_PERDEV_TOTALBYTES:
		case PMID_DISK_PERDEV_READBYTES:
		case PMID_DISK_PERDEV_WRITEBYTES:
		case PMID_DISK_PERDEV_DISCARDBYTES:
		case PMID_DISK_PERDEV_READACTIVE:
		case PMID_DISK_PERDEV_WRITEACTIVE:
		case PMID_DISK_PERDEV_TOTALACTIVE:
		case PMID_DISK_PERDEV_DISCARDACTIVE:
		case PMID_DISK_PERDEV_AVACTIVE:
		case PMID_DISK_PERDEV_AVQUEUE:
			p = get_activity_position(act, A_DISK, EXIT_IF_NOT_FOUND);
			pcp_read_disk_stats(values, act[p], curr);
			break;

		case PMID_NET_PERINTF_INPACKETS:
		case PMID_NET_PERINTF_OUTPACKETS:
		case PMID_NET_PERINTF_INBYTES:
		case PMID_NET_PERINTF_OUTBYTES:
		case PMID_NET_PERINTF_INCOMPRESS:
		case PMID_NET_PERINTF_OUTCOMPRESS:
		case PMID_NET_PERINTF_INMULTICAST:
			p = get_activity_position(act, A_NET_DEV, EXIT_IF_NOT_FOUND);
			pcp_read_netdev_stats(values, act[p], curr);
			break;

		case PMID_NET_EPERINTF_INERRORS:
		case PMID_NET_EPERINTF_OUTERRORS:
		case PMID_NET_EPERINTF_COLLISIONS:
		case PMID_NET_EPERINTF_INDROPS:
		case PMID_NET_EPERINTF_OUTDROPS:
		case PMID_NET_EPERINTF_OUTCARRIER:
		case PMID_NET_EPERINTF_INFRAME:
		case PMID_NET_EPERINTF_INFIFO:
		case PMID_NET_EPERINTF_OUTFIFO:
			p = get_activity_position(act, A_NET_EDEV, EXIT_IF_NOT_FOUND);
			pcp_read_enetdev_stats(values, act[p], curr);
			break;

		case PMID_SERIAL_PERTTY_RX:
		case PMID_SERIAL_PERTTY_TX:
		case PMID_SERIAL_PERTTY_FRAME:
		case PMID_SERIAL_PERTTY_PARITY:
		case PMID_SERIAL_PERTTY_BRK:
		case PMID_SERIAL_PERTTY_OVERRUN:
			p = get_activity_position(act, A_SERIAL, EXIT_IF_NOT_FOUND);
			pcp_read_serial_stats(values, act[p], curr);
			break;

		case PMID_SOCKET_TOTAL:
		case PMID_SOCKET_TCPINUSE:
		case PMID_SOCKET_UDPINUSE:
		case PMID_SOCKET_RAWINUSE:
		case PMID_SOCKET_FRAGINUSE:
		case PMID_SOCKET_TCPTW:
			p = get_activity_position(act, A_NET_SOCK, EXIT_IF_NOT_FOUND);
			pcp_read_net_sock_stats(values, act[p], curr);
			break;

		case PMID_NET_IP_INRECEIVES:
		case PMID_NET_IP_FORWDATAGRAMS:
		case PMID_NET_IP_INDELIVERS:
		case PMID_NET_IP_OUTREQUESTS:
		case PMID_NET_IP_REASMREQDS:
		case PMID_NET_IP_REASMOKS:
		case PMID_NET_IP_FRAGOKS:
		case PMID_NET_IP_FRAGCREATES:
			p = get_activity_position(act, A_NET_IP, EXIT_IF_NOT_FOUND);
			pcp_read_net_ip_stats(values, act[p], curr);
			break;

		case PMID_NET_EIP_INHDRERRORS:
		case PMID_NET_EIP_INADDRERRORS:
		case PMID_NET_EIP_INUNKNOWNPROTOS:
		case PMID_NET_EIP_INDISCARDS:
		case PMID_NET_EIP_OUTDISCARDS:
		case PMID_NET_EIP_OUTNOROUTES:
		case PMID_NET_EIP_REASMFAILS:
		case PMID_NET_EIP_FRAGFAILS:
			p = get_activity_position(act, A_NET_EIP, EXIT_IF_NOT_FOUND);
			pcp_read_net_eip_stats(values, act[p], curr);
			break;

		case PMID_NFSCLIENT_RPCCCNT:
		case PMID_NFSCLIENT_RPCRETRANS:
		case PMID_NFSCLIENT_REQUESTS:
			p = get_activity_position(act, A_NET_NFS, EXIT_IF_NOT_FOUND);
			pcp_read_net_nfs_stats(values, act[p], curr);
			break;

		case PMID_NFSSERVER_RPCCNT:
		case PMID_NFSSERVER_RPCBADCLNT:
		case PMID_NFSSERVER_NETCNT:
		case PMID_NFSSERVER_NETUDPCNT:
		case PMID_NFSSERVER_NETTCPCNT:
		case PMID_NFSSERVER_RCHITS:
		case PMID_NFSSERVER_RCMISSES:
		case PMID_NFSSERVER_REQUESTS:
			p = get_activity_position(act, A_NET_NFSD, EXIT_IF_NOT_FOUND);
			pcp_read_net_nfsd_stats(values, act[p], curr);
			break;

		case PMID_NET_ICMP_INMSGS:
		case PMID_NET_ICMP_OUTMSGS:
		case PMID_NET_ICMP_INECHOS:
		case PMID_NET_ICMP_INECHOREPS:
		case PMID_NET_ICMP_OUTECHOS:
		case PMID_NET_ICMP_OUTECHOREPS:
		case PMID_NET_ICMP_INTIMESTAMPS:
		case PMID_NET_ICMP_INTIMESTAMPREPS:
		case PMID_NET_ICMP_OUTTIMESTAMPS:
		case PMID_NET_ICMP_OUTTIMESTAMPREPS:
		case PMID_NET_ICMP_INADDRMASKS:
		case PMID_NET_ICMP_INADDRMASKREPS:
		case PMID_NET_ICMP_OUTADDRMASKS:
		case PMID_NET_ICMP_OUTADDRMASKREPS:
			p = get_activity_position(act, A_NET_ICMP, EXIT_IF_NOT_FOUND);
			pcp_read_net_icmp_stats(values, act[p], curr);
			break;

		case PMID_NET_EICMP_INERRORS:
		case PMID_NET_EICMP_OUTERRORS:
		case PMID_NET_EICMP_INDESTUNREACHS:
		case PMID_NET_EICMP_OUTDESTUNREACHS:
		case PMID_NET_EICMP_INTIMEEXCDS:
		case PMID_NET_EICMP_OUTTIMEEXCDS:
		case PMID_NET_EICMP_INPARMPROBS:
		case PMID_NET_EICMP_OUTPARMPROBS:
		case PMID_NET_EICMP_INSRCQUENCHS:
		case PMID_NET_EICMP_OUTSRCQUENCHS:
		case PMID_NET_EICMP_INREDIRECTS:
		case PMID_NET_EICMP_OUTREDIRECTS:
			p = get_activity_position(act, A_NET_EICMP, EXIT_IF_NOT_FOUND);
			pcp_read_net_eicmp_stats(values, act[p], curr);
			break;

		case PMID_NET_TCP_ACTIVEOPENS:
		case PMID_NET_TCP_PASSIVEOPENS:
		case PMID_NET_TCP_INSEGS:
		case PMID_NET_TCP_OUTSEGS:
			p = get_activity_position(act, A_NET_TCP, EXIT_IF_NOT_FOUND);
			pcp_read_net_tcp_stats(values, act[p], curr);
			break;

		case PMID_NET_ETCP_ATTEMPTFAILS:
		case PMID_NET_ETCP_ESTABRESETS:
		case PMID_NET_ETCP_RETRANSSEGS:
		case PMID_NET_ETCP_INERRS:
		case PMID_NET_ETCP_OUTRSTS:
			p = get_activity_position(act, A_NET_ETCP, EXIT_IF_NOT_FOUND);
			pcp_read_net_etcp_stats(values, act[p], curr);
			break;

		case PMID_NET_UDP_INDATAGRAMS:
		case PMID_NET_UDP_OUTDATAGRAMS:
		case PMID_NET_UDP_NOPORTS:
		case PMID_NET_UDP_INERRORS:
			p = get_activity_position(act, A_NET_UDP, EXIT_IF_NOT_FOUND);
			pcp_read_net_udp_stats(values, act[p], curr);
			break;

		case PMID_NET_SOCK6_TCPINUSE:
		case PMID_NET_SOCK6_UDPINUSE:
		case PMID_NET_SOCK6_RAWINUSE:
		case PMID_NET_SOCK6_FRAGINUSE:
			p = get_activity_position(act, A_NET_SOCK6, EXIT_IF_NOT_FOUND);
			pcp_read_net_sock6_stats(values, act[p], curr);
			break;

		case PMID_NET_IP6_INRECEIVES:
		case PMID_NET_IP6_OUTFORWDATAGRAMS:
		case PMID_NET_IP6_INDELIVERS:
		case PMID_NET_IP6_OUTREQUESTS:
		case PMID_NET_IP6_REASMREQDS:
		case PMID_NET_IP6_REASMOKS:
		case PMID_NET_IP6_INMCASTPKTS:
		case PMID_NET_IP6_OUTMCASTPKTS:
		case PMID_NET_IP6_FRAGOKS:
		case PMID_NET_IP6_FRAGCREATES:
			p = get_activity_position(act, A_NET_IP6, EXIT_IF_NOT_FOUND);
			pcp_read_net_ip6_stats(values, act[p], curr);
			break;

		case PMID_NET_EIP6_INHDRERRORS:
		case PMID_NET_EIP6_INADDRERRORS:
		case PMID_NET_EIP6_INUNKNOWNPROTOS:
		case PMID_NET_EIP6_INTOOBIGERRORS:
		case PMID_NET_EIP6_INDISCARDS:
		case PMID_NET_EIP6_OUTDISCARDS:
		case PMID_NET_EIP6_INNOROUTES:
		case PMID_NET_EIP6_OUTNOROUTES:
		case PMID_NET_EIP6_REASMFAILS:
		case PMID_NET_EIP6_FRAGFAILS:
		case PMID_NET_EIP6_INTRUNCATEDPKTS:
			p = get_activity_position(act, A_NET_EIP6, EXIT_IF_NOT_FOUND);
			pcp_read_net_eip6_stats(values, act[p], curr);
			break;

		case PMID_NET_ICMP6_INMSGS:
		case PMID_NET_ICMP6_OUTMSGS:
		case PMID_NET_ICMP6_INECHOS:
		case PMID_NET_ICMP6_INECHOREPLIES:
		case PMID_NET_ICMP6_OUTECHOREPLIES:
		case PMID_NET_ICMP6_INGROUPMEMBQUERIES:
		case PMID_NET_ICMP6_INGROUPMEMBRESPONSES:
		case PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES:
		case PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS:
		case PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS:
		case PMID_NET_ICMP6_INROUTERSOLICITS:
		case PMID_NET_ICMP6_OUTROUTERSOLICITS:
		case PMID_NET_ICMP6_INROUTERADVERTISEMENTS:
		case PMID_NET_ICMP6_INNEIGHBORSOLICITS:
		case PMID_NET_ICMP6_OUTNEIGHBORSOLICITS:
		case PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS:
		case PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS:
			p = get_activity_position(act, A_NET_ICMP6, EXIT_IF_NOT_FOUND);
			pcp_read_net_icmp6_stats(values, act[p], curr);
			break;

		case PMID_NET_EICMP6_INERRORS:
		case PMID_NET_EICMP6_INDESTUNREACHS:
		case PMID_NET_EICMP6_OUTDESTUNREACHS:
		case PMID_NET_EICMP6_INTIMEEXCDS:
		case PMID_NET_EICMP6_OUTTIMEEXCDS:
		case PMID_NET_EICMP6_INPARMPROBLEMS:
		case PMID_NET_EICMP6_OUTPARMPROBLEMS:
		case PMID_NET_EICMP6_INREDIRECTS:
		case PMID_NET_EICMP6_OUTREDIRECTS:
		case PMID_NET_EICMP6_INPKTTOOBIGS:
		case PMID_NET_EICMP6_OUTPKTTOOBIGS:
			p = get_activity_position(act, A_NET_EICMP6, EXIT_IF_NOT_FOUND);
			pcp_read_net_eicmp6_stats(values, act[p], curr);
			break;

		case PMID_NET_UDP6_INDATAGRAMS:
		case PMID_NET_UDP6_OUTDATAGRAMS:
		case PMID_NET_UDP6_NOPORTS:
		case PMID_NET_UDP6_INERRORS:
			p = get_activity_position(act, A_NET_UDP6, EXIT_IF_NOT_FOUND);
			pcp_read_net_udp6_stats(values, act[p], curr);
			break;

		case PMID_MEM_HUGE_TOTALBYTES:
		case PMID_MEM_HUGE_FREEBYTES:
		case PMID_MEM_HUGE_RSVDBYTES:
		case PMID_MEM_HUGE_SURPBYTES:
			p = get_activity_position(act, A_HUGE, EXIT_IF_NOT_FOUND);
			pcp_read_huge_stats(values, act[p], curr);
			break;

		case PMID_POWER_FAN_RPM:
		case PMID_POWER_FAN_DRPM:
		case PMID_POWER_FAN_DEVICE:
			p = get_activity_position(act, A_PWR_FAN, EXIT_IF_NOT_FOUND);
			pcp_read_power_fan_stats(values, act[p], curr);
			break;

		case PMID_POWER_TEMP_CELSIUS:
		case PMID_POWER_TEMP_PERCENT:
		case PMID_POWER_TEMP_DEVICE:
			p = get_activity_position(act, A_PWR_TEMP, EXIT_IF_NOT_FOUND);
			pcp_read_power_temp_stats(values, act[p], curr);
			break;

		case PMID_POWER_IN_VOLTAGE:
		case PMID_POWER_IN_PERCENT:
		case PMID_POWER_IN_DEVICE:
			p = get_activity_position(act, A_PWR_IN, EXIT_IF_NOT_FOUND);
			pcp_read_power_in_stats(values, act[p], curr);
			break;

		case PMID_POWER_BAT_CAPACITY:
		case PMID_POWER_BAT_STATUS:
			p = get_activity_position(act, A_PWR_BAT, EXIT_IF_NOT_FOUND);
			pcp_read_power_bat_stats(values, act[p], curr);
			break;

		case PMID_POWER_USB_BUS:
		case PMID_POWER_USB_VENDORID:
		case PMID_POWER_USB_PRODUCTID:
		case PMID_POWER_USB_MAXPOWER:
		case PMID_POWER_USB_MANUFACTURER:
		case PMID_POWER_USB_PRODUCTNAME:
			p = get_activity_position(act, A_PWR_USB, EXIT_IF_NOT_FOUND);
			pcp_read_power_usb_stats(values, act[p], curr);
			break;

		case PMID_FILESYS_CAPACITY:
		case PMID_FILESYS_FREE:
		case PMID_FILESYS_USED:
		case PMID_FILESYS_FULL:
		case PMID_FILESYS_MAXFILES:
		case PMID_FILESYS_FREEFILES:
		case PMID_FILESYS_USEDFILES:
		case PMID_FILESYS_AVAIL:
			p = get_activity_position(act, A_FS, EXIT_IF_NOT_FOUND);
			pcp_read_filesystem_stats(values, act[p], curr);
			break;

		case PMID_FCHOST_INFRAMES:
		case PMID_FCHOST_OUTFRAMES:
		case PMID_FCHOST_INBYTES:
		case PMID_FCHOST_OUTBYTES:
			p = get_activity_position(act, A_NET_FC, EXIT_IF_NOT_FOUND);
			pcp_read_fchost_stats(values, act[p], curr);
			break;

		case PMID_PSI_CPU_SOMETOTAL:
		case PMID_PSI_CPU_SOMEAVG:
			p = get_activity_position(act, A_PSI_CPU, EXIT_IF_NOT_FOUND);
			pcp_read_psicpu_stats(values, act[p], curr);
			break;

		case PMID_PSI_IO_SOMETOTAL:
		case PMID_PSI_IO_SOMEAVG:
		case PMID_PSI_IO_FULLTOTAL:
		case PMID_PSI_IO_FULLAVG:
			p = get_activity_position(act, A_PSI_IO, EXIT_IF_NOT_FOUND);
			pcp_read_psiio_stats(values, act[p], curr);
			break;

		case PMID_PSI_MEM_SOMETOTAL:
		case PMID_PSI_MEM_SOMEAVG:
		case PMID_PSI_MEM_FULLTOTAL:
		case PMID_PSI_MEM_FULLAVG:
			p = get_activity_position(act, A_PSI_MEM, EXIT_IF_NOT_FOUND);
			pcp_read_psimem_stats(values, act[p], curr);
			break;
	}
}

/*
 ***************************************************************************
 * Build a comma-separated string of the short names of all currently
 * collected activities, e.g. "CPU,PCSW,IO,MEMORY,NET_DEV".
 * The "A_" prefix common to all activity names is stripped.
 *
 * IN:
 * @buf		Output buffer.
 * @len		Length of @buf.
 ***************************************************************************
 */
static void
build_sadc_activities_string(char *buf, size_t len,
			     const struct pcp_local_config *local_cfg)
{
	int	p, first = TRUE;

	buf[0] = '\0';
	for (p = 0; p < NR_ACT; p++) {
		const char *name;

		if (!IS_COLLECTED(act[p]->options))
			continue;

		name = act[p]->name;
		if (!strncmp(name, "A_", 2))
			name += 2;

		if (!first)
			pmstrncat(buf, len, ",");
		pmstrncat(buf, len, name);
		first = FALSE;
	}

	pcp_local_append_group_names(local_cfg, buf, len);
}

/*
 ***************************************************************************
 * Register pmimport.sadc.* provenance metrics and populate write handles.
 * Safe to call multiple times — pmiAddMetric is idempotent for the same
 * metric, and pcp_alloc_handle overwrites the stored handle with the
 * current session value (which is identical across calls in one session).
 ***************************************************************************
 */
static const char *sadc_metric_oneline[] = {
	[SADC_VERSION]    = "sysstat version that created this archive",
	[SADC_ACTIVITIES] = "sysstat activities collected (comma-separated)",
	[SADC_INTERVAL]   = "nominal collection interval in seconds",
	[SADC_COMMENT]    = "operator comment inserted at this timestamp",
	[SADC_RESTARTS]   = "system restart event (value=1 at restart timestamps)",
	[SADC_ARCHIVE]    = "archive base path",
	[SADC_HOSTNAME]   = "hostname of the monitored system",
	[SADC_TIMEZONE]   = "timezone of the monitored system",
	[SADC_ZONEINFO]   = "Olson timezone name of the monitored system",
};

void
pcp_register_sadc_metrics(void)
{
	size_t i;

	for (i = 0; i < SADC_METRIC_COUNT; i++) {
		const pmDesc *d = &sadc_metric_descs[i];
		pmiAddMetric(sadc_metric_names[i],
			     d->pmid, d->type, d->indom, d->sem, d->units);
		pcp_alloc_handle(&sadc_metrics, i, 0, PM_IN_NULL, NULL);
		if (sadc_metric_oneline[i])
			pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE,
				   d->pmid, sadc_metric_oneline[i]);
	}
}

/*
 ***************************************************************************
 * Register pmimport metrics and write their values at archive open time.
 * Called once per archive (initial open and daily rotation).
 *
 * Metrics written:
 *   pmimport.version       - sysstat version string
 *   pmimport.args          - comma-separated list of collected activities
 *   pmimport.sadc.interval - collection interval in seconds (when > 0)
 *   pmimport.sadc.comment  - pre-registered for later use at comment time
 *   pmimport.archive       - archive base path
 *   pmimport.hostname      - hostname of the monitored system
 *   pmimport.timezone      - timezone name from the sa file header
 *   pmimport.zoneinfo      - Olson timezone name
 *
 * A context label {"sadc":true} is written so that tools can identify
 * a sadc-created archive via 'pminfo -l'.
 *
 * IN:
 * @path		Archive base path.
 * @hdr			File header supplying hostname and timezone.
 * @interval_secs	Collection interval in seconds, or <= 0 if this is
 *			not a normal collection invocation.
 ***************************************************************************
 */
void
pcp_write_import_metrics(const char *path, const struct file_header *hdr,
			 long interval_secs,
			 const struct pcp_local_config *local_cfg)
{
	char		abuf[1024];
	pmAtomValue	atom;
#ifdef HAVE_PMI_APPEND
	/* Private libpcp helper - returns the Olson timezone name */
	extern const char *__pmZoneinfo(void);
#endif

	/* Context label: presence of "sadc":true identifies the archive */
	pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "sadc", "true");

	/* Register all pmimport metrics and populate handles */
	pcp_register_sadc_metrics();

	atom.cp = VERSION;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_VERSION, 0), &atom);

	build_sadc_activities_string(abuf, sizeof(abuf), local_cfg);
	atom.cp = abuf;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_ACTIVITIES, 0), &atom);

	if (interval_secs > 0) {
		atom.ul = (unsigned long)interval_secs;
		pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_INTERVAL, 0), &atom);
	}
	/* pmimport.sadc.comment is pre-registered so it always appears in .meta */

	atom.cp = (char *)path;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_ARCHIVE, 0), &atom);

	atom.cp = (char *)hdr->sa_nodename;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_HOSTNAME, 0), &atom);

	atom.cp = (char *)hdr->sa_tzname;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_TIMEZONE, 0), &atom);

#ifdef HAVE_PMI_APPEND
	atom.cp = (char *)__pmZoneinfo();
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_ZONEINFO, 0), &atom);
#else
	/*
	 * Older libpcp does not export __pmZoneinfo(); fall back to the
	 * (usually more generic) timezone name recorded in the sa header.
	 */
	atom.cp = (char *)hdr->sa_tzname;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_ZONEINFO, 0), &atom);
#endif
}

/*
 ***************************************************************************
 * Register sadc with pmdapmimport via pmiSetImportProgram() so that
 * metrics for this instance are discoverable in /var/run/pmimport.
 *
 * IN:
 * @archive_path	Full path to the current PCP archive base.
 ***************************************************************************
 */
void
pcp_register_import_program(const char *archive_path,
			    const struct pcp_local_config *local_cfg)
{
#ifdef HAVE_PMI_APPEND
	char	abuf[512];

	build_sadc_activities_string(abuf, sizeof(abuf), local_cfg);
	pmiSetImportProgram("sadc", VERSION, abuf, archive_path);
#else
	(void)archive_path;
	(void)local_cfg;
#endif
}

/*
 ***************************************************************************
 * Write a PCP special record (restart or comment) and close the archive.
 * Mirrors write_special_record() for the native .sa format.
 *
 * Restart (empty @comment): pmiPutMark() signals the data gap; hinv.ncpu
 * and the sadc self-description metrics are refreshed so readers have
 * current values immediately after the discontinuity boundary.
 *
 * Comment (non-empty @comment): a plain timestamped annotation with no
 * mark record — a comment is not a data disruption.
 *
 * IN:
 * @comment	Admin text from sadc -C, or "" for a restart mark.
 * @cpu_nr	file_header.sa_cpu_nr (includes the "all" CPU — subtract
 *		one for hinv.ncpu; used only for restart records).
 * @timestamp	Unix epoch timestamp for the record.
 ***************************************************************************
 */
void
pcp_write_sadc_special_record(const char *comment, unsigned int cpu_nr,
			      unsigned long long timestamp, unsigned int nsec,
			      const struct pcp_local_config *local_cfg)
{
	if (comment[0]) {
		/* Comment: annotation only, no discontinuity mark */
		pmAtomValue atom;
		atom.cp = (char *)comment;
		pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_COMMENT, 0), &atom);
	}
	else {
		/* Restart: mark the gap then refresh boundary metadata */
		char        abuf[1024];
		pmAtomValue atom;
		int         h;

		pmiPutMark();

		/* Refresh hinv.ncpu via the file_header_metrics handle */
		h = ACT_HANDLE(&file_header_metrics, FILE_HEADER_CPU_COUNT, 0);
		atom.ul = cpu_nr > 1 ? cpu_nr - 1 : 1;
		pmiPutAtomValueHandle(h, &atom);

		atom.cp = VERSION;
		pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_VERSION, 0), &atom);

		build_sadc_activities_string(abuf, sizeof(abuf), local_cfg);
		atom.cp = abuf;
		pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_ACTIVITIES, 0), &atom);
	}

	pcp_pmi_write(timestamp, nsec);
}

/*
 ***************************************************************************
 * Read optional sadc self-description metrics from the current PCP archive
 * context.  These metrics are written by sadc -O pcp; they may not be
 * present in archives created by other tools (pmlogger, sadf -l etc.).
 *
 * OUT:
 * @version	If non-NULL and the metric exists: filled with the sysstat
 *		version string.  Caller must free().  Set to NULL on failure.
 * @interval	If non-NULL and the metric exists: filled with the nominal
 *		collection interval in seconds.  Set to 0 on failure.
 ***************************************************************************
 */
void
pcp_read_sadc_metrics(char **version, long *interval)
{
	pmID		pmids[2];
	pmDesc		descs[2];
	pcp_result_t	*result;
	const char	*names[2] = {"pmimport.version", "pmimport.sadc.interval"};
	int		n = 0, sts;

	if (version)  *version  = NULL;
	if (interval) *interval = 0;

	/*
	 * Look up metrics by name — their pmIDs are assigned dynamically
	 * at archive creation time so we cannot hardcode them.
	 */
	sts = pmLookupName(2, names, pmids);
	if (sts < 0)
		return;		/* Neither metric is in the archive */

	for (int i = 0; i < 2; i++) {
		if (pmids[i] == PM_ID_NULL)
			continue;
		if (pmLookupDesc(pmids[i], &descs[i]) < 0)
			pmids[i] = PM_ID_NULL;
		else
			n++;
	}
	if (n == 0)
		return;

	if (pcp_fetch(2, pmids, &result) < 0)
		return;

	for (int i = 0; i < result->numpmid; i++) {
		pmValueSet *vset = result->vset[i];
		pmAtomValue av;

		if (vset->numval < 1 || vset->pmid == PM_ID_NULL)
			continue;

		if (version && vset->pmid == pmids[0] &&
		    pmExtractValue(vset->valfmt, &vset->vlist[0],
				   PM_TYPE_STRING, &av, PM_TYPE_STRING) == 0) {
			*version = av.cp;	/* caller frees */
		}
		else if (interval && vset->pmid == pmids[1] &&
			 pmExtractValue(vset->valfmt, &vset->vlist[0],
					PM_TYPE_U32, &av, PM_TYPE_U32) == 0) {
			*interval = (long) av.ul;
		}
	}
	pcp_free_result(result);
}

/*
 ***************************************************************************
 * Open a PCP archive and verify that selected activities have metrics
 * present.  Activities whose metrics are absent are deselected.
 * Shared between sar and sadf.
 *
 * IN:
 * @from_file	Name of PCP archive.
 * @act		Array of activities.
 * @flags	Flags for common options and system state.
 ***************************************************************************
 */
void check_pcpfile_actlist(const char *from_file, struct activity *act[], uint64_t flags)
{
	struct act_metrics *metrics;
	pmInDom instdomain;
	char **namelist;
	int *instlist;
	int missing[NR_ACT] = {0};
	int i, j, sts;

	for (i = 0; i < NR_ACT; i++) {

		if (!IS_SELECTED(act[i]->options))
			continue;

		metrics = act[i]->metrics;

		if ((sts = pmLookupName(metrics->count, metrics->names, metrics->pmids)) < 0) {
			fprintf(stderr, _("Cannot lookup %s metrics in PCP archive %s: %s\n"),
				act[i]->name, from_file, pmErrStr(sts));
		}

		if (sts != metrics->count) {
			act[i]->options &= ~AO_SELECTED;
			missing[i] = sts;
			continue;
		}

		act[i]->nr_ini = act[i]->nr2 = 1;
		instdomain = metrics->descs[0].indom;
		if (instdomain == PM_INDOM_NULL)
			continue;

		if ((sts = pmGetInDom(instdomain, &instlist, &namelist)) < 0)
			continue;
		else if (sts > 1)
			act[i]->nr_ini = sts;
		free(instlist);
		free(namelist);
	}

	if (!get_activity_nr(act, AO_SELECTED, COUNT_ACTIVITIES)) {
		for (i = 0; i < NR_ACT; i++) {
			if (!missing[i])
				continue;
			metrics = act[i]->metrics;
			fprintf(stderr,
				_("Missing %zu of %zu metrics from %s activity:\n"),
				missing[i] < 0 ? metrics->count : (size_t)missing[i],
				metrics->count, act[i]->name + 2);
			for (j = 0; j < metrics->count; j++) {
				if (metrics->pmids[j] != PM_ID_NULL)
					continue;
				fprintf(stderr, "\t[%d] %s\n", j, metrics->names[j]);
			}
		}
		print_collect_error();
	}
}

/*
 ***************************************************************************
 * Populate activity buffers from a PCP fetch result.
 * Shared between sar and sadf.
 *
 * IN:
 * @result	Fetch result spanning all selected activities.
 * @header	System activity file header (for context).
 * @curr	Buffer index for current sample.
 *
 * RETURNS:
 * R_RESTART if result carries a mark record, 0 otherwise.
 ***************************************************************************
 */
int read_stats_from_result(pcp_result_t *result, struct file_header *header, int curr)
{
	int i;

	if (result->numpmid == 0)
		return R_RESTART;

	for (i = 0; i < result->numpmid; i++) {
		pcp_read_stats(result->vset[i], header, curr);
	}

	record_hdr[curr].ust_time = result->timestamp.tv_sec;
	/*
	 * Encode the Unix timestamp (seconds) as centiseconds so that
	 * get_itv_value() computes the correct interval between samples:
	 *   itv = (T2 * 100) - (T1 * 100) = (T2 - T1) * 100 centiseconds.
	 * Only overwrite if kernel.all.uptime was not already set by pcp_read_stats.
	 */
	if (!record_hdr[curr].uptime_cs)
		record_hdr[curr].uptime_cs = (unsigned long long)result->timestamp.tv_sec * 100;

	return 0;
}

/*
 ***************************************************************************
 * Write help text into the open archive for every metric that is actively
 * being collected in this sadc session.  Mirrors the pcp_def_*_metrics()
 * dispatch in sadc.c: only activities with IS_COLLECTED set are included,
 * and activities that share a metrics table (A_CPU/A_PWR_CPU/A_NET_SOFT)
 * or have secondary tables (A_DISK, A_NET_DEV/A_NET_EDEV) are handled the
 * same way as their pcp_def counterparts.
 *
 * IN:
 * @act		Array of activities.
 * @nact	Number of entries in @act.
 ***************************************************************************
 */
void
pcp_write_activity_help(struct activity *act[], int nact)
{
	int p;

	for (p = 0; p < nact; p++) {
		if (!IS_COLLECTED(act[p]->options))
			continue;

		switch (act[p]->id) {
		case A_DISK:
			pcp_local_write_pmid_help(disk_metrics.pmids,  disk_metrics.count);
			pcp_local_write_pmid_help(dm_metrics.pmids,    dm_metrics.count);
			pcp_local_write_pmid_help(md_metrics.pmids,    md_metrics.count);
			pcp_local_write_pmid_help(part_metrics.pmids,  part_metrics.count);
			pcp_local_write_pmid_help(zram_metrics.pmids,  zram_metrics.count);
			break;
		case A_NET_DEV:
		case A_NET_EDEV:
			pcp_local_write_pmid_help(netdev_metrics.pmids,  netdev_metrics.count);
			pcp_local_write_pmid_help(netedev_metrics.pmids, netedev_metrics.count);
			break;
		default:
			if (act[p]->metrics && act[p]->metrics->pmids)
				pcp_local_write_pmid_help(act[p]->metrics->pmids,
							  act[p]->metrics->count);
			break;
		}
	}
}

/*
 ***************************************************************************
 * Register and stage the file-header metrics that sar/sadf read to print
 * the report header (CPU count, uname strings).  Written once at archive
 * open time using pmiPutAtomValueHandle after pmiAddMetric.
 *
 * IN:
 * @hdr		File header supplying CPU count and uname fields.
 ***************************************************************************
 */
void pcp_write_file_header_metrics(const struct file_header *hdr)
{
	pmAtomValue atom;
	size_t i;

	/* Register each metric and populate its handle via the descriptor table */
	for (i = 0; i < FILE_HEADER_METRIC_COUNT; i++) {
		const pmDesc *d = &file_header_metric_descs[i];
		const char   *n =  file_header_metric_names[i];
		pmiAddMetric(n, d->pmid, d->type, d->indom, d->sem, d->units);
		pcp_alloc_handle(&file_header_metrics, i, 0, PM_IN_NULL, NULL);
	}

	atom.ul = hdr->sa_cpu_nr > 1 ? hdr->sa_cpu_nr - 1 : 1;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_CPU_COUNT, 0), &atom);

	atom.ul = hdr->sa_hz ? hdr->sa_hz : 100UL;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_KERNEL_HERTZ, 0), &atom);

	atom.cp = (char *)hdr->sa_sysname;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_UNAME_SYSNAME, 0), &atom);

	atom.cp = (char *)hdr->sa_release;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_UNAME_RELEASE, 0), &atom);

	atom.cp = (char *)hdr->sa_machine;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_UNAME_MACHINE, 0), &atom);

	atom.cp = (char *)hdr->sa_nodename;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_UNAME_NODENAME, 0), &atom);

	atom.ul = (unsigned long) sysconf(_SC_PAGESIZE);
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_PAGESIZE, 0), &atom);

	{
		char uname_full[256];
		/* pmda.uname: "sysname nodename release machine" — matches pmcd format */
		pmsprintf(uname_full, sizeof(uname_full), "%s %s %s %s",
			  hdr->sa_sysname, hdr->sa_nodename,
			  hdr->sa_release, hdr->sa_machine);
		atom.cp = uname_full;
		pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_UNAME_FULL, 0), &atom);
	}
}

/*
 ***************************************************************************
 * Write inventory metrics that require live-system data unavailable from
 * the file header alone: disk count, interface count, and boot time.
 * Called from sadc after all activities have been set up.
 *
 * IN:
 * @nr_disk	Number of disk devices (A_DISK.nr_ini).
 * @nr_iface	Number of network interfaces (A_NET_DEV.nr_ini).
 * @ust_time	Timestamp of current sample (seconds since epoch).
 * @uptime_cs	System uptime in centiseconds at @ust_time.
 ***************************************************************************
 */
void pcp_write_inventory_metrics(__nr_t nr_disk, __nr_t nr_iface,
				 unsigned long long ust_time,
				 unsigned long long uptime_cs)
{
#ifdef HAVE_PMI_APPEND
	pmAtomValue atom;

	atom.ul = (unsigned long) nr_disk;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_NDISK, 0), &atom);

	atom.ul = (unsigned long) nr_iface;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_NINTERFACE, 0), &atom);

	/* boot time = current epoch - uptime_cs (centiseconds -> seconds) */
	atom.ll = (long long)(ust_time - uptime_cs / 100);
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_BOOTTIME, 0), &atom);
#endif
}

/*
 ***************************************************************************
 * Write one PCP sample timestamp for the sadf->PCP write path.
 *
 * IN:
 * @ust_time	Unix epoch timestamp (seconds).
 ***************************************************************************
 */
void pcp_write_sadf_sample(unsigned long long ust_time)
{
	int rc;

	if ((rc = pcp_pmi_write(ust_time, 0)) < 0) {
		/* Non-fatal: skip records with out-of-order timestamps (e.g. corrupt input) */
		if (rc == PM_ERR_LOGREC)
			fprintf(stderr, _("PCP: skipping out-of-order timestamp %llu: %s\n"),
				ust_time, pmiErrStr(rc));
		else {
			fprintf(stderr, _("PCP: pmiWrite: %s\n"), pmiErrStr(rc));
			exit(4);
		}
	}
}

#ifdef HAVE_PMI_APPEND
/*
 ***************************************************************************
 * Copy context-level labels from a PCP context into the active PMI archive.
 * This propagates host labels (site tags, environment identifiers, etc.)
 * set by the system administrator into sadc-produced archives so that
 * tools reading the archive see the same context metadata as live queries.
 *
 * IN:
 * @local_ctx	PM_CONTEXT_LOCAL (or any live context) to read labels from.
 *              Pass -1 to skip silently (local PMDA not available).
 ***************************************************************************
 */
void
pcp_copy_context_labels(int local_ctx)
{
	pmLabelSet	*sets = NULL;
	int		nsets, i, j, saved;

	if (local_ctx < 0)
		return;

	saved = pmWhichContext();
	pmUseContext(local_ctx);
	nsets = pmGetContextLabels(&sets);
	pmUseContext(saved);

	if (nsets <= 0)
		return;

	for (i = 0; i < nsets; i++) {
		for (j = 0; j < sets[i].nlabels; j++) {
			pmLabel	*lp = &sets[i].labels[j];
			const char *name  = sets[i].json + lp->name;
			const char *value = sets[i].json + lp->value;
			char nbuf[64], vbuf[256];

			/* copy to null-terminated buffers */
			snprintf(nbuf, sizeof(nbuf),  "%.*s", (int)lp->namelen, name);
			snprintf(vbuf, sizeof(vbuf), "%.*s", (int)lp->valuelen, value);

			/* Skip the "sadc":true label we write ourselves */
			if (!strcmp(nbuf, "sadc"))
				continue;

			pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, nbuf, vbuf);
		}
	}
	pmFreeLabelSets(sets, nsets);
}
#endif

/*
 ***************************************************************************
 * Open a PCP archive for sadf->PCP conversion and register file-header
 * metrics.  Mirrors the F_BEGIN block of print_pcp_header().
 *
 * IN:
 * @dfile	Destination archive base path.
 * @hdr		File header supplying timezone, hostname, and uname fields.
 ***************************************************************************
 */
void pcp_open_sadf_archive(const char *dfile, const struct file_header *hdr)
{
	/* Initialise the hz global so JIFFIES_TO_MSEC works in pcp_print_* */
	if (hdr->sa_hz)
		hz = hdr->sa_hz;

	pmiStart(dfile, FALSE);
	pmiSetTimezone(hdr->sa_tzname);
	pmiSetHostname(hdr->sa_nodename);
#ifdef HAVE_PMI_APPEND
	pmiSetZoneinfo(NULL);	/* auto-detect Olson name from /etc/localtime */
	pcp_copy_context_labels(pcp_local_get_ctx());
#endif

	pcp_write_file_header_metrics(hdr);
	pcp_register_sadc_metrics();
}

/*
 ***************************************************************************
 * Close the sadf->PCP archive, optionally writing a final sample first.
 *
 * IN:
 * @ust_time	Timestamp for the initial sample (0 = skip write).
 ***************************************************************************
 */
void pcp_close_sadf_archive(unsigned long long ust_time)
{
	if (ust_time)
		pcp_write_sadf_sample(ust_time);
	pmiEnd();
}

/*
 ***************************************************************************
 * Write a restart record to the sadf->PCP archive.
 *
 * IN:
 * @hdr		File header (for CPU count).
 * @ust_time	Record timestamp.
 ***************************************************************************
 */
void pcp_write_sadf_restart(const struct file_header *hdr,
			    unsigned long long ust_time)
{
	pmAtomValue atom;

	/*
	 * Ensure pmimport.sadc.* metrics are registered and handles populated.
	 * pcp_register_sadc_metrics() is idempotent — safe to call each time.
	 */
	pcp_register_sadc_metrics();

	/* Mark the discontinuity so tools don't compute rates across the gap */
	pmiPutMark();

	/* pmimport.sadc.restarts = 1 signals a restart event at this timestamp */
	atom.ul = 1;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_RESTARTS, 0), &atom);

	/*
	 * hinv.ncpu is updated via the file_header_metrics handle so the
	 * new CPU count is visible immediately after the discontinuity.
	 */
	atom.ul = hdr->sa_cpu_nr > 1 ? hdr->sa_cpu_nr - 1 : 1;
	pmiPutAtomValueHandle(ACT_HANDLE(&file_header_metrics, FILE_HEADER_CPU_COUNT, 0), &atom);

	pcp_write_sadf_sample(ust_time);
}

/*
 ***************************************************************************
 * Write a comment record to the sadf->PCP archive.
 *
 * IN:
 * @comment	Comment string.
 * @ust_time	Record timestamp.
 ***************************************************************************
 */
void pcp_write_sadf_comment(const char *comment, unsigned long long ust_time)
{
	pmAtomValue atom;

	pcp_register_sadc_metrics();

	atom.cp = (char *)comment;
	pmiPutAtomValueHandle(ACT_HANDLE(&sadc_metrics, SADC_COMMENT, 0), &atom);
	pcp_write_sadf_sample(ust_time);
}

/*
 ***************************************************************************
 * Register and write kernel.all.uptime per sample.
 * This metric is declared in record_header_metric_descs[] for the sadf
 * read path but was never written by sadc.  It is needed by PCP derived
 * metrics such as proc.psinfo.age and proc.hog.cpu.
 *
 * IN:
 * @uptime_cs	System uptime in centiseconds (from record_hdr.uptime_cs).
 ***************************************************************************
 */
void pcp_write_uptime(unsigned long long uptime_cs)
{
	static int handle = -1;
	pmAtomValue atom;

	if (handle < 0)
		handle = act_register_scalar_handle(&record_header_metrics,
						    RECORD_HEADER_KERNEL_UPTIME);
	if (handle >= 0) {
		/* kernel.all.uptime is a double in seconds; uptime_cs is centiseconds */
		atom.d = (double)uptime_cs / 100.0;
		pmiPutAtomValueHandle(handle, &atom);
	}
}

#ifdef HAVE_PMI_APPEND
/*
 ***************************************************************************
 * Open a PCP archive for sadc direct-write mode.  Returns the pmiStart
 * status so the caller can handle failure (fall back to native .sa only).
 *
 * IN:
 * @path	Archive base path.
 * @hdr		File header supplying hostname and timezone.
 *
 * RETURNS:
 * pmiStart() return value (negative on error).
 ***************************************************************************
 */
int pcp_open_sadc_archive(const char *path, const struct file_header *hdr)
{
	int sts;

	sts = pmiStart(path, PMI_APPEND);
	if (sts < 0)
		return sts;
	pmiSetHostname(hdr->sa_nodename);
	pmiSetTimezone(hdr->sa_tzname);
#ifdef HAVE_PMI_APPEND
	pmiSetZoneinfo(NULL);	/* auto-detect Olson name from /etc/localtime */
	pcp_copy_context_labels(pcp_local_get_ctx());
#endif
	return sts;
}

/*
 ***************************************************************************
 * Write one PCP sample timestamp for the sadc direct-write path.
 *
 * IN:
 * @ust_time	Unix epoch timestamp (seconds).
 * @nsec	Nanosecond part of the timestamp (captured via clock_gettime).
 * @flags	sadc flags (for error-mode handling).
 *
 * RETURNS:
 * 0 on success, negative PCP error code on failure.
 ***************************************************************************
 */
int pcp_write_sadc_sample(unsigned long long ust_time, unsigned int nsec,
			  uint64_t flags)
{
	int sts;

	sts = pcp_pmi_write(ust_time, nsec);
	if (sts < 0)
		fprintf(stderr, _("PCP write error: %s\n"), pmiErrStr(sts));
	return sts;
}

/*
 ***************************************************************************
 * Callback invoked by pmiSetVolumeSize() when a data volume is completed.
 * Compresses the closed volume with zstd in a child process.  The child
 * PID is saved so pcp_close_sadc_archive() can wait for it before the
 * process exits — otherwise systemd cgroup cleanup kills the child.
 ***************************************************************************
 */
static void
pcp_sadc_volume_rotate(const char *vol_path)
{
	pid_t pid;

	/* Reap any previous compression child before forking a new one */
	if (compress_child > 0) {
		waitpid(compress_child, NULL, 0);
		compress_child = 0;
	}

	pid = fork();

	if (pid == 0) {
		alarm(0);
		execlp("zstd", "zstd", "-q", "--rm", vol_path, (char *)NULL);
		_exit(1);
	}
	if (pid > 0)
		compress_child = pid;
}

/*
 ***************************************************************************
 * Configure automatic data volume rotation using pmiSetVolumeSize().
 * Called once after pcp_open_sadc_archive() when PCP_VOLUME_SIZE > 0.
 *
 * IN:
 * @volume_size	Maximum data volume size in bytes (0 = disabled).
 ***************************************************************************
 */
void pcp_sadc_set_volume_size(size_t volume_size)
{
	if (volume_size == 0)
		return;
	if (pmiSetVolumeSize(volume_size, pcp_sadc_volume_rotate) < 0)
		fprintf(stderr,
			_("pmiSetVolumeSize(%zu): %s — volume rotation disabled\n"),
			volume_size, pmiErrStr(-1));
}

/*
 ***************************************************************************
 * Close the sadc PCP archive.
 ***************************************************************************
 */
void pcp_close_sadc_archive(void)
{
	if (compress_child > 0) {
		waitpid(compress_child, NULL, 0);
		compress_child = 0;
	}
	pmiEnd();
}
#endif /* HAVE_PMI_APPEND */
#endif /* HAVE_PCP */
