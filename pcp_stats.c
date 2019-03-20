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
 * Display CPU statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_cpu_stats(struct activity *a, int curr, unsigned long long itv,
				    struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	unsigned long long deltot_jiffies = 1;
	char buf[64], cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	char *str;
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
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* Don't display CPU */
			continue;

		scc = (struct stats_cpu *) ((char *) a->buf[curr]  + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			str = NULL;

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
			str = cpuno;

			/*
			 * Recalculate interval for current proc.
			 * If result is 0 then current CPU is a tickless one.
			 */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/* Current CPU is tickless */
				pmiPutValue("kernel.percpu.cpu.user", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.nice", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.sys", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.iowait", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.steal", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.hardirq", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.softirq", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.guest", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.guest_nice", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.idle", cpuno, "100");

				continue;
			}
		}

		snprintf(buf, sizeof(buf), "%f",
			 (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
			 0.0 :
			 ll_sp_value(scp->cpu_user - scp->cpu_guest,
				     scc->cpu_user - scc->cpu_guest, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.user" : "kernel.all.cpu.user", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
			 0.0 :
			 ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
				     scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.nice" : "kernel.all.cpu.nice", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_sys, scc->cpu_sys, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.sys" : "kernel.all.cpu.sys", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.iowait" : "kernel.all.cpu.iowait", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.steal" : "kernel.all.cpu.steal", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.hardirq" : "kernel.all.cpu.hardirq", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.softirq" : "kernel.all.cpu.softirq", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_guest, scc->cpu_guest, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.guest" : "kernel.all.cpu.guest", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.guest_nice" : "kernel.all.cpu.guest_nice", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 scc->cpu_idle < scp->cpu_idle ?
			 0.0 :
			 ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies));
		pmiPutValue(i ? "kernel.percpu.cpu.idle" : "kernel.all.cpu.idle", str, buf);
	}
#endif	/* HAVE_PCP */
}

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
 * Display interrupts statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_irq_stats(struct activity *a, int curr, unsigned long long itv,
				    struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_irq
		*sic = (struct stats_irq *) a->buf[curr],
		*sip = (struct stats_irq *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(sip->irq_nr, sic->irq_nr, itv));
	pmiPutValue("kernel.all.intr", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display swapping statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_swap_stats(struct activity *a, int curr, unsigned long long itv,
				     struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(ssp->pswpin, ssc->pswpin, itv));
	pmiPutValue("swap.pagesin", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(ssp->pswpout, ssc->pswpout, itv));
	pmiPutValue("swap.pagesout", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_io_stats(struct activity *a, int curr, unsigned long long itv,
				   struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive < sip->dk_drive ? 0.0
					       : S_VALUE(sip->dk_drive, sic->dk_drive, itv));
	pmiPutValue("disk.all.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_rio < sip->dk_drive_rio ? 0.0
						       : S_VALUE(sip->dk_drive_rio, sic->dk_drive_rio, itv));
	pmiPutValue("disk.all.read", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_wio < sip->dk_drive_wio ? 0.0
						       : S_VALUE(sip->dk_drive_wio, sic->dk_drive_wio, itv));
	pmiPutValue("disk.all.write", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_dio < sip->dk_drive_dio ? 0.0
						       : S_VALUE(sip->dk_drive_dio, sic->dk_drive_dio, itv));
	pmiPutValue("disk.all.discard", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_rblk < sip->dk_drive_rblk ? 0.0
							 : S_VALUE(sip->dk_drive_rblk, sic->dk_drive_rblk, itv) / 2);
	pmiPutValue("disk.all.read_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_wblk < sip->dk_drive_wblk ? 0.0
							 : S_VALUE(sip->dk_drive_wblk, sic->dk_drive_wblk, itv) / 2);
	pmiPutValue("disk.all.write_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 sic->dk_drive_dblk < sip->dk_drive_dblk ? 0.0
							 : S_VALUE(sip->dk_drive_dblk, sic->dk_drive_dblk, itv) / 2);
	pmiPutValue("disk.all.discard_bytes", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display memory statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_memory_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];
	unsigned long long nousedmem;

	if (DISPLAY_MEMORY(a->opt_flags)) {

		nousedmem = smc->frmkb + smc->bufkb + smc->camkb + smc->slabkb;
		if (nousedmem > smc->tlmkb) {
			nousedmem = smc->tlmkb;
		}

		snprintf(buf, sizeof(buf), "%llu", smc->frmkb);
		pmiPutValue("mem.util.free", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->availablekb);
		pmiPutValue("mem.util.available", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlmkb - nousedmem);
		pmiPutValue("mem.util.used", NULL, buf);

		snprintf(buf, sizeof(buf), "%f",
			 smc->tlmkb ? SP_VALUE(nousedmem, smc->tlmkb, smc->tlmkb) : 0.0);
		pmiPutValue("mem.util.used_pct", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->bufkb);
		pmiPutValue("mem.util.buffers", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->camkb);
		pmiPutValue("mem.util.cached", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->comkb);
		pmiPutValue("mem.util.commit", NULL, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (smc->tlmkb + smc->tlskb) ? SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb)
						   : 0.0);
		pmiPutValue("mem.util.commit_pct", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->activekb);
		pmiPutValue("mem.util.active", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->inactkb);
		pmiPutValue("mem.util.inactive", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->dirtykb);
		pmiPutValue("mem.util.dirty", NULL, buf);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {

			snprintf(buf, sizeof(buf), "%llu", smc->anonpgkb);
			pmiPutValue("mem.util.anonpages", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->slabkb);
			pmiPutValue("mem.util.slab", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->kstackkb);
			pmiPutValue("mem.util.stack", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->pgtblkb);
			pmiPutValue("mem.util.pageTables", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->vmusedkb);
			pmiPutValue("mem.util.vmused", NULL, buf);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		snprintf(buf, sizeof(buf), "%llu", smc->frskb);
		pmiPutValue("mem.util.swapFree", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlskb - smc->frskb);
		pmiPutValue("mem.util.swapUsed", NULL, buf);

		snprintf(buf, sizeof(buf), "%f",
			 smc->tlskb ? SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb) : 0.0);
		pmiPutValue("mem.util.swapUsed_pct", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->caskb);
		pmiPutValue("mem.util.swapCached", NULL, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (smc->tlskb - smc->frskb) ? SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb)
						   : 0.0);
		pmiPutValue("mem.util.swapCached_pct", NULL, buf);
	}
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
