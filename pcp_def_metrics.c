/*
 * pcp_def_metrics.c: Functions used by sadf to define PCP metrics
 * (C) 2019-2022 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include "common.h"
#include "sa.h"
#include "pcp_def_metrics.h"

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#ifdef HAVE_PCP_IMPL_H
#include <pcp/impl.h>
#endif
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * Define PCP metrics for per-CPU interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @cpu		CPU number (0 is cpu0, 1 is cpu1, etc.)
 ***************************************************************************
 */
void pcp_def_percpu_int_metrics(struct activity *a, int cpu)
{
#ifdef HAVE_PCP
	char buf[64];
	struct sa_item *list = a->item_list;
	static pmInDom indom = PM_INDOM_NULL;
	static int inst = 0;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 40);

		/* Create metric */
		pmiAddMetric("kernel.percpu.interrupts",
			     pmiID(60, 4, 1), PM_TYPE_U32, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
	}

	/* Create instance for each interrupt for the current CPU */
	while (list != NULL) {

		snprintf(buf, sizeof(buf), "%s::cpu%d", list->item_name, cpu);
		buf[sizeof(buf) - 1] = '\0';

		pmiAddInstance(indom, buf, inst++);
		list = list->next;
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for CPU related statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_cpu_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int i, first = TRUE, create = FALSE;
	char buf[64];
	static pmInDom indom = PM_INDOM_NULL;

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * Should current CPU (including CPU "all") be displayed?
		 * NB: Offline not tested (they may be turned off and on within
		 * the same file.
		 */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* CPU not selected */
			continue;

		if (!i) {
			if (a->id == A_CPU) {
				/* This is CPU "all" */
				pmiAddMetric("kernel.all.cpu.user",
					     pmiID(60, 0, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.nice",
					     pmiID(60, 0, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.sys",
					     pmiID(60, 0, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.idle",
					     pmiID(60, 0, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.iowait",
					     pmiID(60, 0, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.steal",
					     pmiID(60, 0, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.irq.hard",
					     pmiID(60, 0, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.irq.soft",
					     pmiID(60, 0, 53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.irq.total",
					     pmiID(60, 0, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.guest",
					     pmiID(60, 0, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

				pmiAddMetric("kernel.all.cpu.guest_nice",
					     pmiID(60, 0, 81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));
			}

			else if (a->id == A_NET_SOFT) {
				/* Create metrics for A_NET_SOFT */
				pmiAddMetric("network.softnet.processed",
					     pmiID(60, 57, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

				pmiAddMetric("network.softnet.dropped",
					     pmiID(60, 57, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

				pmiAddMetric("network.softnet.time_squeeze",
					     pmiID(60, 57, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

				pmiAddMetric("network.softnet.received_rps",
					     pmiID(60, 57, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

				pmiAddMetric("network.softnet.flow_limit",
					     pmiID(60, 57, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

				pmiAddMetric("network.softnet.backlog_length",
					     pmiID(60, 57, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
			}
		}
		else {
			/* This is not CPU "all" */
			if (indom == PM_INDOM_NULL) {
				/* Create domain */
				indom = pmInDom_build(60, 0);
				create = TRUE;
			}
			if (create) {
				/* Create instance for current CPU */
				sprintf(buf, "cpu%d", i - 1);
				pmiAddInstance(indom, buf, i - 1);
			}

			if (a->id == A_IRQ) {
				/* Create per-CPU interrupts metrics */
				pcp_def_percpu_int_metrics(a, i - 1);
			}

			else if (first) {
				if (a->id == A_CPU) {
					/* Create metrics for A_CPU */
					pmiAddMetric("kernel.percpu.cpu.user",
						     pmiID(60, 0, 0), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.nice",
						     pmiID(60, 0, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.sys",
						     pmiID(60, 0, 2), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.idle",
						     pmiID(60, 0, 3), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.iowait",
						     pmiID(60, 0, 30), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.steal",
						     pmiID(60, 0, 58), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.irq.hard",
						     pmiID(60, 0, 57), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.irq.soft",
						     pmiID(60, 0, 56), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.irq.total",
						     pmiID(60, 0, 35), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.guest",
						     pmiID(60, 0, 61), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

					pmiAddMetric("kernel.percpu.cpu.guest_nice",
						     pmiID(60, 0, 83), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));
				}

				else if (a->id == A_PWR_CPU) {
					/* Create metric for A_PWR_CPU */
					pmiAddMetric("hinv.cpu.clock",
						     pmiID(60, 18, 0), PM_TYPE_FLOAT, indom, PM_SEM_DISCRETE,
						     pmiUnits(0, -1, 0, 0, PM_TIME_USEC, 0));
				}

				else if (a->id == A_NET_SOFT) {
					/* Create metrics for a_NET_SOFT */
					pmiAddMetric("network.softnet.percpu.processed",
						     pmiID(60, 57, 6), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

					pmiAddMetric("network.softnet.percpu.dropped",
						     pmiID(60, 57, 7), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

					pmiAddMetric("network.softnet.percpu.time_squeeze",
						     pmiID(60, 57, 8), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

					pmiAddMetric("network.softnet.percpu.received_rps",
						     pmiID(60, 57, 10), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

					pmiAddMetric("network.softnet.percpu.flow_limit",
						     pmiID(60, 57, 11), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

					pmiAddMetric("network.softnet.percpu.backlog_length",
						     pmiID(60, 57, 13), PM_TYPE_U64, indom, PM_SEM_COUNTER,
						     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
				}

				first = FALSE;
			}
		}
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for task creation and context switch statistics.
 ***************************************************************************
 */
void pcp_def_pcsw_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("kernel.all.pswitch",
		     pmiID(60, 0, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("kernel.all.sysfork",
		     pmiID(60, 0, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_irq_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int first = TRUE, inst = 0;
	struct sa_item *list = a->item_list;
	pmInDom indom;

	if (!(a->bitmap->b_array[0] & 1))
		/* CPU "all" not selected: Nothing to do here */
		return;

	/* Create domain */
	indom = pmiInDom(60, 4);

	/* Create instances and metrics for each interrupts for CPU "all" */
	while (list != NULL) {

		if (!strcmp(list->item_name, K_LOWERSUM)) {
			/*
			 * Create metric for interrupt "sum" for CPU "all".
			 * Interrupt "sum" appears at most once in list.
			 * No need to create an instance for it: It has a specific metric name.
			 */
			pmiAddMetric("kernel.all.intr",
				     pmiID(60, 0, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
				     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
		}
		else {
			if (first) {
				/* Create metric for a common interrupt for CPU "all" if not already done */
				pmiAddMetric("kernel.all.interrupts.total",
					     pmiID(60, 4, 0), PM_TYPE_U64, indom, PM_SEM_COUNTER,
					     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
				first = FALSE;
			}
			/* Create instance */
			pmiAddInstance(indom, list->item_name, inst++);
		}
		list = list->next;
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for swapping statistics.
 ***************************************************************************
 */
void pcp_def_swap_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("swap.pagesin",
		     pmiID(60, 0, 8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("swap.pagesout",
		     pmiID(60, 0, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for paging statistics.
 ***************************************************************************
 */
void pcp_def_paging_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("mem.vmstat.pgpgin",
		     pmiID(60, 28, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgpgout",
		     pmiID(60, 28, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgfault",
		     pmiID(60, 28, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgmajfault",
		     pmiID(60, 28, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgfree",
		     pmiID(60, 28, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgscan_kswapd_total",
		     pmiID(60, 28, 177), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgscan_direct_total",
		     pmiID(60, 28, 176), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgsteal_total",
		     pmiID(60, 28, 178), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for I/O and transfer rate statistics.
 ***************************************************************************
 */
void pcp_def_io_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("disk.all.total",
		     pmiID(60, 0, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.all.read",
		     pmiID(60, 0, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.all.write",
		     pmiID(60, 0, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.all.discard",
		     pmiID(60, 0, 96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.all.read_bytes",
		     pmiID(60, 0, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.all.write_bytes",
		     pmiID(60, 0, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.all.discard_bytes",
		     pmiID(60, 0, 98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_memory_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	if (DISPLAY_MEMORY(a->opt_flags)) {

		pmiAddMetric("hinv.physmem",
			     pmiID(60, 1, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
			     pmiUnits(1, 0, 0, PM_SPACE_MBYTE, 0, 0));

		pmiAddMetric("mem.physmem",
			     pmiID(60, 1, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.free",
			     pmiID(60, 1, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.available",
			     pmiID(60, 1, 58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.used",
			     pmiID(60, 1, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.bufmem",
			     pmiID(60, 1, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.cached",
			     pmiID(60, 1, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.committed_AS",
			     pmiID(60, 1, 26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.active",
			     pmiID(60, 1, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.inactive",
			     pmiID(60, 1, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.dirty",
			     pmiID(60, 1, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		if (DISPLAY_MEM_ALL(a->opt_flags)) {

			pmiAddMetric("mem.util.anonpages",
				     pmiID(60, 1, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.slab",
				     pmiID(60, 1, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.kernelStack",
				     pmiID(60, 1, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.pageTables",
				     pmiID(60, 1, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.vmallocUsed",
				     pmiID(60, 1, 51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		pmiAddMetric("mem.util.swapFree",
			     pmiID(60, 1, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.swapTotal",
			     pmiID(60, 1, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.swapCached",
			     pmiID(60, 1, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for kernel tables statistics.
 ***************************************************************************
 */
void pcp_def_ktables_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("vfs.dentry.count",
		     pmiID(60, 27, 5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("vfs.files.count",
		     pmiID(60, 27, 0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("vfs.inodes.count",
		     pmiID(60, 27, 3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("kernel.all.nptys",
		     pmiID(60, 72, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for queue and load statistics.
 ***************************************************************************
 */
void pcp_def_queue_metrics(void)
{
#ifdef HAVE_PCP
	pmInDom indom;

	pmiAddMetric("kernel.all.runnable",
		     pmiID(60, 2, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("kernel.all.nprocs",
		     pmiID(60, 2, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("kernel.all.blocked",
		     pmiID(60, 0, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	indom = pmiInDom(60, 2);
	pmiAddMetric("kernel.all.load",
		     pmiID(60, 2, 0), PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
	pmiAddInstance(indom, "1 minute", 1);
	pmiAddInstance(indom, "5 minute", 5);
	pmiAddInstance(indom, "15 minute", 15);
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for disks statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_disk_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	struct sa_item *list = a->item_list;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 1);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("disk.dev.read",
		     pmiID(60, 0, 4), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.dev.write",
		     pmiID(60, 0, 5), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.dev.total",
		     pmiID(60, 0, 28), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("disk.dev.total_bytes",
		     pmiID(60, 0, 37), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.dev.read_bytes",
		     pmiID(60, 0, 38), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.dev.write_bytes",
		     pmiID(60, 0, 39), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.dev.discard_bytes",
		     pmiID(60, 0, 90), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.dev.read_rawactive",
		     pmiID(60, 0, 72), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

	pmiAddMetric("disk.dev.write_rawactive",
		     pmiID(60, 0, 73), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

	pmiAddMetric("disk.dev.total_rawactive",
		     pmiID(60, 0, 79), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

	pmiAddMetric("disk.dev.discard_rawactive",
		     pmiID(60, 0, 92), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

	pmiAddMetric("disk.dev.avactive",
		     pmiID(60, 0, 46), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));

	pmiAddMetric("disk.dev.aveq",
		     pmiID(60, 0, 47), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 1, 0, 0, PM_TIME_MSEC, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for network interfaces (errors) statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_dev_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	struct sa_item *list = a->item_list;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 3);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	if (a->id == A_NET_DEV) {
		/* Create metrics for A_NET_DEV */
		pmiAddMetric("network.interface.in.packets",
			     pmiID(60, 3, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.packets",
			     pmiID(60, 3, 9), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.bytes",
			     pmiID(60, 3, 0), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

		pmiAddMetric("network.interface.out.bytes",
			     pmiID(60, 3, 8), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

		pmiAddMetric("network.interface.in.compressed",
			     pmiID(60, 3, 6), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.compressed",
			     pmiID(60, 3, 15), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.mcasts",
			     pmiID(60, 3, 7), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
	}
	else {
		/* Create metrics for A_NET_EDEV */
		pmiAddMetric("network.interface.in.errors",
			     pmiID(60, 3, 2), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.errors",
			     pmiID(60, 3, 10), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.collisions",
			     pmiID(60, 3, 13), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.drops",
			     pmiID(60, 3, 3), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.drops",
			     pmiID(60, 3, 11), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.carrier",
			     pmiID(60, 3, 14), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.frame",
			     pmiID(60, 3, 5), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.fifo",
			     pmiID(60, 3, 4), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.fifo",
			     pmiID(60, 3, 12), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for serial lines statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_serial_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int i;
	pmInDom indom;
	char buf[64];

	/* Create domain */
	indom = pmInDom_build(60, 35);

	/* Create metrics */
	pmiAddMetric("tty.serial.rx",
		     pmiID(60, 74, 0), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("tty.serial.tx",
		     pmiID(60, 74, 1), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("tty.serial.frame",
		     pmiID(60, 74, 2), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("tty.serial.parity",
		     pmiID(60, 74, 3), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("tty.serial.brk",
		     pmiID(60, 74, 4), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("tty.serial.overrun",
		     pmiID(60, 74, 5), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	for (i = 0; i < a->nr_ini; i++) {
		/* Create instances */
		sprintf(buf, "serial%d", i);
		pmiAddInstance(indom, buf, i);
	}
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for NFS client statistics.
 ***************************************************************************
 */
void pcp_def_net_nfs_metrics(void)
{
#ifdef HAVE_PCP
	pmInDom indom;

	pmiAddMetric("rpc.client.rpccnt",
		     pmiID(60, 7, 20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.client.rpcretrans",
		     pmiID(60, 7, 21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	indom = pmiInDom(60, 7);
	pmiAddInstance(indom, "read", 6);
	pmiAddInstance(indom, "write", 8);
	pmiAddInstance(indom, "access", 18);
	pmiAddInstance(indom, "getattr", 4);

	pmiAddMetric("nfs.client.reqs",
		     pmiID(60, 7, 4), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for NFS server statistics.
 ***************************************************************************
 */
void pcp_def_net_nfsd_metrics(void)
{
#ifdef HAVE_PCP
	pmInDom indom;

	pmiAddMetric("rpc.server.rpccnt",
		     pmiID(60, 7, 30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.rpcbadclnt",
		     pmiID(60, 7, 34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.netcnt",
		     pmiID(60, 7, 44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.netudpcnt",
		     pmiID(60, 7, 45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.nettcpcnt",
		     pmiID(60, 7, 46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.rchits",
		     pmiID(60, 7, 35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("rpc.server.rcmisses",
		     pmiID(60, 7, 36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	indom = pmiInDom(60, 7);
	pmiAddInstance(indom, "read", 6);
	pmiAddInstance(indom, "write", 8);
	pmiAddInstance(indom, "access", 18);
	pmiAddInstance(indom, "getattr", 4);

	pmiAddMetric("nfs.server.reqs",
		     pmiID(60, 7, 12), PM_TYPE_U32, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for network sockets statistics.
 ***************************************************************************
 */
void pcp_def_net_sock_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.sockstat.total",
		     pmiID(60, 11, 9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.tcp.inuse",
		     pmiID(60, 11, 0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.udp.inuse",
		     pmiID(60, 11, 3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.raw.inuse",
		     pmiID(60, 11, 6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.frag.inuse",
		     pmiID(60, 11, 15), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.tcp.tw",
		     pmiID(60, 11, 11), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for IP network statistics.
 ***************************************************************************
 */
void pcp_def_net_ip_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.ip.inreceives",
		     pmiID(60, 14, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.forwdatagrams",
		     pmiID(60, 14, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.indelivers",
		     pmiID(60, 14, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.outrequests",
		     pmiID(60, 14, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.reasmreqds",
		     pmiID(60, 14, 13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.reasmoks",
		     pmiID(60, 14, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.fragoks",
		     pmiID(60, 14, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.fragcreates",
		     pmiID(60, 14, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for IP network errors statistics.
 ***************************************************************************
 */
void pcp_def_net_eip_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.ip.inhdrerrors",
		     pmiID(60, 14, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.inaddrerrors",
		     pmiID(60, 14, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.inunknownprotos",
		     pmiID(60, 14, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.indiscards",
		     pmiID(60, 14, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.outdiscards",
		     pmiID(60, 14, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.outnoroutes",
		     pmiID(60, 14, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.reasmfails",
		     pmiID(60, 14, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip.fragfails",
		     pmiID(60, 14, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.icmp.inmsgs",
		     pmiID(60, 14, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outmsgs",
		     pmiID(60, 14, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inechos",
		     pmiID(60, 14, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inechoreps",
		     pmiID(60, 14, 28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outechos",
		     pmiID(60, 14, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outechoreps",
		     pmiID(60, 14, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.intimestamps",
		     pmiID(60, 14, 29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.intimestampreps",
		     pmiID(60, 14, 30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outtimestamps",
		     pmiID(60, 14, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outtimestampreps",
		     pmiID(60, 14, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inaddrmasks",
		     pmiID(60, 14, 31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inaddrmaskreps",
		     pmiID(60, 14, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outaddrmasks",
		     pmiID(60, 14, 44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outaddrmaskreps",
		     pmiID(60, 14, 45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network errors statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.icmp.inerrors",
		     pmiID(60, 14, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outerrors",
		     pmiID(60, 14, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.indestunreachs",
		     pmiID(60, 14, 22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outdestunreachs",
		     pmiID(60, 14, 35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.intimeexcds",
		     pmiID(60, 14, 23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outtimeexcds",
		     pmiID(60, 14, 36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inparmprobs",
		     pmiID(60, 14, 24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outparmprobs",
		     pmiID(60, 14, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.insrcquenchs",
		     pmiID(60, 14, 25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outsrcquenchs",
		     pmiID(60, 14, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.inredirects",
		     pmiID(60, 14, 27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp.outredirects",
		     pmiID(60, 14, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for TCP network statistics.
 ***************************************************************************
 */
void pcp_def_net_tcp_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.tcp.activeopens",
		     pmiID(60, 14, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.passiveopens",
		     pmiID(60, 14, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.insegs",
		     pmiID(60, 14, 59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.outsegs",
		     pmiID(60, 14, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for TCP network errors statistics.
 ***************************************************************************
 */
void pcp_def_net_etcp_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.tcp.attemptfails",
		     pmiID(60, 14, 56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.estabresets",
		     pmiID(60, 14, 57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.retranssegs",
		     pmiID(60, 14, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.inerrs",
		     pmiID(60, 14, 62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.tcp.outrsts",
		     pmiID(60, 14, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for UDP network statistics.
 ***************************************************************************
 */
void pcp_def_net_udp_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.udp.indatagrams",
		     pmiID(60, 14, 70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp.outdatagrams",
		     pmiID(60, 14, 74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp.noports",
		     pmiID(60, 14, 71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp.inerrors",
		     pmiID(60, 14, 72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network sockets statistics.
 ***************************************************************************
 */
void pcp_def_net_sock6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.sockstat.tcp6.inuse",
		     pmiID(60, 73, 0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.udp6.inuse",
		     pmiID(60, 73, 1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.raw6.inuse",
		     pmiID(60, 73, 3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.sockstat.frag6.inuse",
		     pmiID(60, 73, 4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network statistics.
 ***************************************************************************
 */
void pcp_def_net_ip6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.ip6.inreceives",
		     pmiID(60, 58, 0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.outforwdatagrams",
		     pmiID(60, 58, 9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.indelivers",
		     pmiID(60, 58, 8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.outrequests",
		     pmiID(60, 58, 10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.reasmreqds",
		     pmiID(60, 58, 14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.reasmoks",
		     pmiID(60, 58, 15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.inmcastpkts",
		     pmiID(60, 58, 20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.outmcastpkts",
		     pmiID(60, 58, 21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.fragoks",
		     pmiID(60, 58, 17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.fragcreates",
		     pmiID(60, 58, 19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network errors statistics.
 ***************************************************************************
 */
void pcp_def_net_eip6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.ip6.inhdrerrors",
		     pmiID(60, 58, 1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.inaddrerrors",
		     pmiID(60, 58, 4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.inunknownprotos",
		     pmiID(60, 58, 5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.intoobigerrors",
		     pmiID(60, 58, 2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.indiscards",
		     pmiID(60, 58, 7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.outdiscards",
		     pmiID(60, 58, 11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.innoroutes",
		     pmiID(60, 58, 3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.outnoroutes",
		     pmiID(60, 58, 12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.reasmfails",
		     pmiID(60, 58, 16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.fragfails",
		     pmiID(60, 58, 18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.ip6.intruncatedpkts",
		     pmiID(60, 58, 6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.icmp6.inmsgs",
		     pmiID(60, 58, 32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outmsgs",
		     pmiID(60, 58, 34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inechos",
		     pmiID(60, 58, 41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inechoreplies",
		     pmiID(60, 58, 42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outechoreplies",
		     pmiID(60, 58, 57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.ingroupmembqueries",
		     pmiID(60, 58, 43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.ingroupmembresponses",
		     pmiID(60, 58, 44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outgroupmembresponses",
		     pmiID(60, 58, 59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.ingroupmembreductions",
		     pmiID(60, 58, 45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outgroupmembreductions",
		     pmiID(60, 58, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inroutersolicits",
		     pmiID(60, 58, 46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outroutersolicits",
		     pmiID(60, 58, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inrouteradvertisements",
		     pmiID(60, 58, 47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inneighborsolicits",
		     pmiID(60, 58, 48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outneighborsolicits",
		     pmiID(60, 58, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inneighboradvertisements",
		     pmiID(60, 58, 49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outneighboradvertisements",
		     pmiID(60, 58, 64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network errors statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.icmp6.inerrors",
		     pmiID(60, 58, 33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.indestunreachs",
		     pmiID(60, 58, 37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outdestunreachs",
		     pmiID(60, 58, 52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.intimeexcds",
		     pmiID(60, 58, 39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outtimeexcds",
		     pmiID(60, 58, 54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inparmproblems",
		     pmiID(60, 58, 40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outparmproblems",
		     pmiID(60, 58, 55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inredirects",
		     pmiID(60, 58, 50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outredirects",
		     pmiID(60, 58, 65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.inpkttoobigs",
		     pmiID(60, 58, 38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.icmp6.outpkttoobigs",
		     pmiID(60, 58, 53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for UDPv6 network statistics.
 ***************************************************************************
 */
void pcp_def_net_udp6_metrics(void)
{
#ifdef HAVE_PCP
	pmiAddMetric("network.udp6.indatagrams",
		     pmiID(60, 58, 67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp6.outdatagrams",
		     pmiID(60, 58, 70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp6.noports",
		     pmiID(60, 58, 68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.udp6.inerrors",
		     pmiID(60, 58, 69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for huge pages statistics.
 ***************************************************************************
 */
void pcp_def_huge_metrics()
{
#ifdef HAVE_PCP
	pmiAddMetric("mem.util.hugepagesTotalBytes",
		     pmiID(60, 1, 60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

	pmiAddMetric("mem.util.hugepagesFreeBytes",
		     pmiID(60, 1, 61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

	pmiAddMetric("mem.util.hugepagesRsvdBytes",
		     pmiID(60, 1, 62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

	pmiAddMetric("mem.util.hugepagesSurpBytes",
		     pmiID(60, 1, 63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_fan_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	char buf[16];

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(34, 0);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "fan%d", inst + 1);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.fan.rpm",
		     pmiID(34, 0, 0), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.fan.drpm",
		     pmiID(34, 0, 1), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.fan.device",
		     pmiID(34, 0, 2), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_temp_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	char buf[16];

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(34, 1);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "temp%d", inst + 1);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.temp.celsius",
		     pmiID(34, 1, 0), PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.temp.percent",
		     pmiID(34, 1, 1), PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.temp.device",
		     pmiID(34, 1, 2), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_in_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	char buf[16];

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(34, 2);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "in%d", inst);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.in.voltage",
		     pmiID(34, 2, 0), PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.in.percent",
		     pmiID(34, 2, 1), PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.in.device",
		     pmiID(34, 2, 2), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_usb_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	char buf[16];

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(34, 3);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "usb%d", inst);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.usb.bus",
		     pmiID(34, 3, 0), PM_TYPE_U32, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.vendorId",
		     pmiID(34, 3, 1), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.productId",
		     pmiID(34, 3, 2), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.maxpower",
		     pmiID(34, 3, 3), PM_TYPE_U32, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.manufacturer",
		     pmiID(34, 3, 3), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.productName",
		     pmiID(34, 3, 3), PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for filesystem statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_filesystem_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	struct sa_item *list = a->item_list;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 5);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("filesys.capacity",
		     pmiID(60, 5, 1), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("filesys.free",
		     pmiID(60, 5, 3), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("filesys.used",
		     pmiID(60, 5, 2), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("filesys.full",
		     pmiID(60, 5, 8), PM_TYPE_DOUBLE, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("filesys.maxfiles",
		     pmiID(60, 5, 4), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("filesys.freefiles",
		     pmiID(60, 5, 6), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("filesys.usedfiles",
		     pmiID(60, 5, 5), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("filesys.avail",
		     pmiID(60, 5, 10), PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for Fibre Channel HBA statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_fchost_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int inst = 0;
	static pmInDom indom = PM_INDOM_NULL;
	struct sa_item *list = a->item_list;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 39);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("fchost.in.frames",
		     pmiID(60, 91, 0), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("fchost.out.frames",
		     pmiID(60, 91, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("fchost.in.bytes",
		     pmiID(60, 91, 2), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));

	pmiAddMetric("fchost.out.bytes",
		     pmiID(60, 91, 3), PM_TYPE_U64, indom, PM_SEM_COUNTER,
		     pmiUnits(1, 0, 0, PM_SPACE_BYTE, 0, 0));
#endif /* HAVE_PCP */
}

/*
 ***************************************************************************
 * Define PCP metrics for pressure-stall statistics.
 *
 * IN:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psi_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	static pmInDom indom = PM_INDOM_NULL;

	if (indom == PM_INDOM_NULL) {
		/* Create domain */
		indom = pmInDom_build(60, 37);

		pmiAddInstance(indom, "10 second", 10);
		pmiAddInstance(indom, "1 minute", 60);
		pmiAddInstance(indom, "5 minute", 300);
	}

	if (a->id == A_PSI_CPU) {
		/* Create metrics for A_PSI_CPU */
		pmiAddMetric("kernel.all.pressure.cpu.some.total",
			     pmiID(60, 83, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 1, 0, 0, PM_TIME_USEC, 0));

		pmiAddMetric("kernel.all.pressure.cpu.some.avg",
			     pmiID(60, 83, 0), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
	else if (a->id == A_PSI_IO) {
		/* Create metrics for A_PSI_IO */
		pmiAddMetric("kernel.all.pressure.io.some.total",
			     pmiID(60, 85, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 1, 0, 0, PM_TIME_USEC, 0));

		pmiAddMetric("kernel.all.pressure.io.some.avg",
			     pmiID(60, 85, 0), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("kernel.all.pressure.io.full.total",
			     pmiID(60, 85, 3), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 1, 0, 0, PM_TIME_USEC, 0));

		pmiAddMetric("kernel.all.pressure.io.full.avg",
			     pmiID(60, 85, 2), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
	else {
		/* Create metrics for A_PSI_MEM */
		pmiAddMetric("kernel.all.pressure.memory.some.total",
			     pmiID(60, 84, 1), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 1, 0, 0, PM_TIME_USEC, 0));

		pmiAddMetric("kernel.all.pressure.memory.some.avg",
			     pmiID(60, 84, 0), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("kernel.all.pressure.memory.full.total",
			     pmiID(60, 84, 3), PM_TYPE_U64, indom, PM_SEM_COUNTER,
			     pmiUnits(0, 1, 0, 0, PM_TIME_USEC, 0));

		pmiAddMetric("kernel.all.pressure.memory.full.avg",
			     pmiID(60, 84, 2), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
#endif /* HAVE_PCP */
}
