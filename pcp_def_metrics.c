/*
 * pcp_def_metrics.c: Funtions used by sadf to define PCP metrics
 * (C) 2019 by Sebastien GODARD (sysstat <at> orange.fr)
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
#endif

/*
 ***************************************************************************
 * Define PCP metrics for CPU statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_cpu_metrics(struct activity *a)
{
#ifdef HAVE_PCP
	int i;
	char buf[64];
	pmInDom indom;

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
		else {
			if (i == 1) {
				indom = pmInDom_build(0, PM_INDOM_CPU);

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
			sprintf(buf, "cpu%d", i - 1);
			pmiAddInstance(indom, buf, i - 1);
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
	if (a->bitmap->b_array[0] & 1) {
		/* Interrupt "sum" */
		pmiAddMetric("kernel.all.intr",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE));
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
		     pmiUnits(1, -1, 1, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.all.write_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 1, PM_SPACE_KBYTE, PM_TIME_SEC, 0));

	pmiAddMetric("disk.all.discard_bytes",
		     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
		     pmiUnits(1, -1, 1, PM_SPACE_KBYTE, PM_TIME_SEC, 0));
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
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.available",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.used",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.used_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.buffers",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.cached",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.commit",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.commit_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.active",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.inactive",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.dirty",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		if (DISPLAY_MEM_ALL(a->opt_flags)) {

			pmiAddMetric("mem.util.anonpages",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

			pmiAddMetric("mem.util.slab",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

			pmiAddMetric("mem.util.stack",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

			pmiAddMetric("mem.util.pageTables",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

			pmiAddMetric("mem.util.vmused",
				     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
				     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		pmiAddMetric("mem.util.swapFree",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.swapUsed",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

		pmiAddMetric("mem.util.swapUsed_pct",
			     PM_IN_NULL, PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(0, 0, 0, 0, 0, 0));

		pmiAddMetric("mem.util.swapCached",
			     PM_IN_NULL, PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
			     pmiUnits(1, 0, 0, 0, PM_SPACE_KBYTE, 0));

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
