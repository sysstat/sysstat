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
	int i, first = TRUE;
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
			if (first) {
				first = FALSE;

				indom = pmInDom_build(0, 0);
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
 * Define PCP metrics for queue and load statistics.
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

	indom = pmInDom_build(0, 1);
	pmiAddMetric("kernel.all.load",
		     PM_IN_NULL, PM_TYPE_FLOAT, indom, PM_SEM_INSTANT,
		     pmiUnits(0, 0, 0, 0, 0, 0));
	pmiAddInstance(indom, "1 min", 0);
	pmiAddInstance(indom, "5 min", 1);
	pmiAddInstance(indom, "15 min", 2);
#endif /* HAVE_PCP */
}

