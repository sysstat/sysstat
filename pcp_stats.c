/*
 * pcp_stats.c: Funtions used by sadf to create PCP archive files.
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

#include "sa.h"
#include "pcp_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#endif

/*
 ***************************************************************************
 * Display task creation and context switch statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pcsw_stats(struct activity *a, int curr, unsigned long long itv,
				     struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->context_switch, spc->context_switch, itv));
	pmiPutValue("kernel.all.pswitch", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->processes, spc->processes, itv));
	pmiPutValue("kernel.all.proc", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display queue and load statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_queue_stats(struct activity *a, int curr, unsigned long long itv,
				      struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sqc->nr_running);
	pmiPutValue("proc.runq.runnable", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sqc->nr_threads);
	pmiPutValue("proc.nprocs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sqc->procs_blocked);
	pmiPutValue("proc.blocked", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_1 / 100);
	pmiPutValue("kernel.all.load", "1 min", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_5 / 100);
	pmiPutValue("kernel.all.load", "5 min", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_15 / 100);
	pmiPutValue("kernel.all.load", "15 min", buf);
#endif	/* HAVE_PCP */
}
