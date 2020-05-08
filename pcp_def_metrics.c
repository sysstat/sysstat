/*
 * pcp_def_metrics.c: Funtions used by sadf to define PCP metrics
 * (C) 2019-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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
#endif

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
				     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.nice",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.sys",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.idle",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.iowait",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.steal",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.hardirq",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.softirq",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.guest",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));

				pmiAddMetric("kernel.all.cpu.guest_nice",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));
			}

			else if (a->id == A_PWR_CPU) {
				/* Create metric for A_PWR_CPU */
				pmiAddMetric("kernel.all.cpu.freqMHz",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, 0, 0, 0, 0, 0));
			}

			else if (a->id == A_NET_SOFT) {
				/* Create metrics for a_NET_SOFT */
				pmiAddMetric("network.all.soft.processed",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

				pmiAddMetric("network.all.soft.dropped",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

				pmiAddMetric("network.all.soft.time_squeeze",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

				pmiAddMetric("network.all.soft.received_rps",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

				pmiAddMetric("network.all.soft.flow_limit",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
			}
		}
		else {
			/* This is not CPU "all" */
			if (indom == PM_INDOM_NULL) {
				/* Create domain */
				indom = pmInDom_build(0, PM_INDOM_CPU);
				create = TRUE;
			}
			if (create) {
				/* Create instance for current CPU */
				sprintf(buf, "cpu%d", i - 1);
				pmiAddInstance(indom, buf, i - 1);
			}

			if (first) {
				if (a->id == A_CPU) {
					/* Create metrics for A_CPU */
					pmiAddMetric("kernel.percpu.cpu.user",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.nice",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.sys",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.idle",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.iowait",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.steal",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.hardirq",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.softirq",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.guest",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));

					pmiAddMetric("kernel.percpu.cpu.guest_nice",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));
				}

				else if (a->id == A_PWR_CPU) {
					/* Create metric for A_PWR_CPU */
					pmiAddMetric("kernel.percpu.cpu.freqMHz",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, 0, 0, 0, 0, 0));
				}

				else if (a->id == A_NET_SOFT) {
					/* Create metrics for a_NET_SOFT */
					pmiAddMetric("network.percpu.soft.processed",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

					pmiAddMetric("network.percpu.soft.dropped",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

					pmiAddMetric("network.percpu.soft.time_squeeze",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

					pmiAddMetric("network.percpu.soft.received_rps",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

					pmiAddMetric("network.percpu.soft.flow_limit",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("kernel.all.proc",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	int i, first = TRUE;
	char buf[64];
	pmInDom indom;

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			if (!i) {
				/* Interrupt "sum" */
				pmiAddMetric("kernel.all.intr",
					     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
					     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
			}
			else {
				if (first) {
					indom = pmInDom_build(0, PM_INDOM_INT);

					pmiAddMetric("kernel.all.int.count",
						     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
						     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
					first = FALSE;
				}
				sprintf(buf, "int%d", i - 1);
				pmiAddInstance(indom, buf, i - 1);
			}
		}
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
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("swap.pagesout",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgpgout",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgfault",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgmajfault",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgfree",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgscank",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgscand",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("mem.vmstat.pgsteal",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("disk.all.read",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("disk.all.write",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("disk.all.discard",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("disk.all.read_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.all.write_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.all.discard_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));
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

		pmiAddMetric("mem.util.free",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.available",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.used",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.used_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.buffers",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.cached",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.commit",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.commit_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.active",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.inactive",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.dirty",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		if (DISPLAY_MEM_ALL(a->opt_flags)) {

			pmiAddMetric("mem.util.anonpages",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.slab",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.stack",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.pageTables",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

			pmiAddMetric("mem.util.vmused",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		pmiAddMetric("mem.util.swapFree",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.swapUsed",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.swapUsed_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.swapCached",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

		pmiAddMetric("mem.util.swapCached_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
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
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("vfs.files.count",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("vfs.inodes.count",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("kernel.all.pty",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
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

	pmiAddMetric("proc.runq.runnable",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("proc.nprocs",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("proc.blocked",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	indom = pmInDom_build(0, PM_INDOM_QUEUE);
	pmiAddMetric("kernel.all.load",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
	pmiAddInstance(indom, "1 min", 0);
	pmiAddInstance(indom, "5 min", 1);
	pmiAddInstance(indom, "15 min", 2);
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
		indom = pmInDom_build(0, PM_INDOM_DISK);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("disk.device.tps",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("disk.device.read_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.device.write_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.device.discard_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.device.areq_sz",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("disk.device.aqu_sz",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("disk.device.await",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("disk.device.util",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
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
		indom = pmInDom_build(0, PM_INDOM_NET_DEV);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	if (a->id == A_NET_DEV) {
		/* Create metrics for A_NET_DEV */
		pmiAddMetric("network.interface.in.packets",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.packets",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.bytes",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

		pmiAddMetric("network.interface.out.bytes",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

		pmiAddMetric("network.interface.in.compressed",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.compressed",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.multicast",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.util",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
	else {
		/* Create metrics for A_NET_EDEV */
		pmiAddMetric("network.interface.in.errors",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.errors",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.collisions",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.drops",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.drops",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.carrier",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.frame",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.in.fifo",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

		pmiAddMetric("network.interface.out.fifo",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	indom = pmInDom_build(0, PM_INDOM_SERIAL);

	/* Create metrics */
	pmiAddMetric("serial.in.interrupts",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("serial.out.interrupts",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("serial.frame",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("serial.parity",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("serial.breaks",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("serial.overrun",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

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
	pmiAddMetric("network.fs.client.call",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.client.retrans",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.client.read",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.client.write",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.client.access",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.client.getattr",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.fs.server.call",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.badcall",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.packets",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.udp",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.tcp",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.hits",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.misses",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.read",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.write",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.access",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fs.server.gettattr",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.socket.sock_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket.tcp_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket.udp_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket.raw_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket.frag_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket.tcp_tw",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
	pmiAddMetric("network.snmp.ip.ipInReceives",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipForwDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipInDelivers",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipOutRequests",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipReasmReqds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipReasmOKs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipFragOKs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipFragCreates",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.ip.ipInHdrErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipInAddrErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipInUnknownProtos",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipInDiscards",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipOutDiscards",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipOutNoRoutes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipReasmFails",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip.ipFragFails",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.icmp.icmpInMsgs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutMsgs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInEchos",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInEchoReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutEchos",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutEchoReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInTimestamps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInTimestampReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutTimestamps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutTimestampReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInAddrMasks",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInAddrMaskReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutAddrMasks",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutAddrMaskReps",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.icmp.icmpInErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInDestUnreachs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutDestUnreachs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInTimeExcds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutTimeExcds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInParmProbs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutParmProbs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInSrcQuenchs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutSrcQuenchs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpInRedirects",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp.icmpOutRedirects",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.tcp.tcpActiveOpens",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpPassiveOpens",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpInSegs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpOutSegs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.tcp.tcpAttemptFails",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpEstabResets",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpRetransSegs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpInErrs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.tcp.tcpOutRsts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.udp.udpInDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp.udpOutDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp.udpNoPorts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp.udpInErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.socket6.tcp6_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket6.udp6_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket6.raw6_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("network.socket6.frag6_inuse",
		     PM_IN_NULL, PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInReceives",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutForwDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInDelivers",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutRequests",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsReasmReqds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsReasmOKs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInMcastPkts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutMcastPkts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutFragOKs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutFragCreates",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInHdrErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInAddrErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInUnknownProtos",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInTooBigErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInDiscards",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutDiscards",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInNoRoutes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutNoRoutes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsReasmFails",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsOutFragFails",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.ip6.ipv6IfStatsInTruncatedPkts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInMsgs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutMsgs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInEchos",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInEchoReplies",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutEchoReplies",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInGroupMembQueries",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInGroupMembResponses",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutGroupMembResponses",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInGroupMembReductions",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutGroupMembReductions",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInRouterSolicits",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutRouterSolicits",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInRouterAdvertisements",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInNeighborSolicits",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutNeighborSolicits",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInNeighborAdvertisements",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutNeighborAdvertisements",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInDestUnreachs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutDestUnreachs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInTimeExcds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutTimeExcds",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInParmProblems",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutParmProblems",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInRedirects",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutRedirects",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpInPktTooBigs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.icmp6.ipv6IfIcmpOutPktTooBigs",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("network.snmp.udp6.udpInDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp6.udpOutDatagrams",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp6.udpNoPorts",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.snmp.udp6.udpInErrors",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
	pmiAddMetric("mem.huge.free",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("mem.huge.used",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("mem.huge.used_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("mem.huge.reserved",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));

	pmiAddMetric("mem.huge.surplus",
		     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_KBYTE, 0, 0));
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
		indom = pmInDom_build(0, PM_INDOM_FAN);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "fan%d", inst + 1);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.fan.rpm",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.fan.drpm",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.fan.device",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
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
		indom = pmInDom_build(0, PM_INDOM_TEMP);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "temp%d", inst + 1);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.temp.degC",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.temp.temp_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.temp.device",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
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
		indom = pmInDom_build(0, PM_INDOM_IN);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "in%d", inst);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.in.inV",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.in.in_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.in.device",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
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
		indom = pmInDom_build(0, PM_INDOM_USB);

		for (inst = 0; inst < a->item_list_sz; inst++) {
			sprintf(buf, "usb%d", inst);
			pmiAddInstance(indom, buf, inst);
		}
	}

	pmiAddMetric("power.usb.bus",
		     PM_IN_NULL, PM_TYPE_U32, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.vendorId",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.productId",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.maxpower",
		     PM_IN_NULL, PM_TYPE_U32, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.manufacturer",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("power.usb.productName",
		     PM_IN_NULL, PM_TYPE_STRING, indom, PM_SEM_DISCRETE,
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
		indom = pmInDom_build(0, PM_INDOM_FILESYSTEM);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("fs.util.fsfree",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_MBYTE, 0, 0));

	pmiAddMetric("fs.util.fsused",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(1, 0, 0, PM_SPACE_MBYTE, 0, 0));

	pmiAddMetric("fs.util.fsused_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("fs.util.ufsused_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));

	pmiAddMetric("fs.util.ifree",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("fs.util.iused",
		     PM_IN_NULL, PM_TYPE_U64, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));

	pmiAddMetric("fs.util.iused_pct",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
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
		indom = pmInDom_build(0, PM_INDOM_FCHOST);

		/* Create instances */
		while (list != NULL) {
			pmiAddInstance(indom, list->item_name, inst++);
			list = list->next;
		}
	}

	pmiAddMetric("network.fchost.in.frame",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fchost.out.frame",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fchost.in.word",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));

	pmiAddMetric("network.fchost.out.word",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
		indom = pmInDom_build(0, PM_INDOM_PSI);

		pmiAddInstance(indom, "10 sec", 0);
		pmiAddInstance(indom, "60 sec", 1);
		pmiAddInstance(indom, "300 sec", 2);
	}

	if (a->id == A_PSI_CPU) {
		/* Create metrics for A_PSI_CPU */
		pmiAddMetric("psi.cpu.some.trends",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.cpu.some.avg",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
	else if (a->id == A_PSI_IO) {
		/* Create metrics for A_PSI_IO */
		pmiAddMetric("psi.io.some.trends",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.io.some.avg",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.io.full.trends",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.io.full.avg",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
	else {
		/* Create metrics for A_PSI_MEM */
		pmiAddMetric("psi.mem.some.trends",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.mem.some.avg",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.mem.full.trends",
			     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("psi.mem.full.avg",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));
	}
#endif /* HAVE_PCP */
}
