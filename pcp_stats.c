/*
 * pcp_stats.c: Funtions used by sadf to create PCP archive files.
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
	char buf[64], intno[64];
	int i;
	struct stats_irq *sic, *sip;

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		sic = (struct stats_irq *) ((char *) a->buf[curr]  + i * a->msize);
		sip = (struct stats_irq *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			if (!i) {
				/* This is interrupt "sum" */
				snprintf(buf, sizeof(buf), "%f",
					 S_VALUE(sip->irq_nr, sic->irq_nr, itv));
				pmiPutValue("kernel.all.intr", NULL, buf);
			}
			else {
				sprintf(intno, "int%d", i - 1);
				snprintf(buf, sizeof(buf), "%f",
					 S_VALUE(sip->irq_nr, sic->irq_nr, itv));
				pmiPutValue("kernel.all.int.count", intno, buf);
			}
		}
	}
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
 * Display paging statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_paging_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgpgin, spc->pgpgin, itv));
	pmiPutValue("mem.vmstat.pgpgin", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgpgout, spc->pgpgout, itv));
	pmiPutValue("mem.vmstat.pgpgout", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgfault, spc->pgfault, itv));
	pmiPutValue("mem.vmstat.pgfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgmajfault, spc->pgmajfault, itv));
	pmiPutValue("mem.vmstat.pgmajfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgfree, spc->pgfree, itv));
	pmiPutValue("mem.vmstat.pgfree", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv));
	pmiPutValue("mem.vmstat.pgscank", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv));
	pmiPutValue("mem.vmstat.pgscand", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		 S_VALUE(spp->pgsteal, spc->pgsteal, itv));
	pmiPutValue("mem.vmstat.pgsteal", NULL, buf);
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
 * Display kernel tables statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_ktables_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", skc->dentry_stat);
	pmiPutValue("vfs.dentry.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", skc->file_used);
	pmiPutValue("vfs.files.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", skc->inode_used);
	pmiPutValue("vfs.inodes.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", skc->pty_nr);
	pmiPutValue("kernel.all.pty", NULL, buf);
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

/*
 ***************************************************************************
 * Display disks statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_disk_stats(struct activity *a, int curr, unsigned long long itv,
				     struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i, j;
	struct stats_disk *sdc,	*sdp, sdpzero;
	struct ext_disk_stats xds;
	char *dev_name;
	char buf[64];

	memset(&sdpzero, 0, STATS_DISK_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		j = check_disk_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sdp = &sdpzero;
		}
		else {
			sdp = (struct stats_disk *) ((char *) a->buf[!curr] + j * a->msize);
		}

		/* Get device name */
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn, sdc->part_nr,
					   DISPLAY_PRETTY(flags), DISPLAY_PERSIST_NAME_S(flags),
					   USE_STABLE_ID(flags), NULL);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, dev_name))
				/* Device not found */
				continue;
		}

		/* Compute extended statistics values */
		compute_ext_disk_stats(sdc, sdp, itv, &xds);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sdp->nr_ios, sdc->nr_ios, itv));
		pmiPutValue("disk.device.tps", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sdp->rd_sect, sdc->rd_sect, itv) / 2);
		pmiPutValue("disk.device.read_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sdp->wr_sect, sdc->wr_sect, itv) / 2);
		pmiPutValue("disk.device.write_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sdp->dc_sect, sdc->dc_sect, itv) / 2);
		pmiPutValue("disk.device.discard_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 xds.arqsz / 2);
		pmiPutValue("disk.device.areq_sz", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0);
		pmiPutValue("disk.device.aqu_sz", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 xds.await);
		pmiPutValue("disk.device.await", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 xds.util / 10.0);
		pmiPutValue("disk.device.util", dev_name, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display network interfaces statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_dev_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i, j;
	struct stats_net_dev *sndc, *sndp, sndzero;
	double rxkb, txkb, ifutil;
	char buf[64];

	memset(&sndzero, 0, STATS_NET_DEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		j = check_net_dev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sndp = &sndzero;
		}
		else {
			sndp = (struct stats_net_dev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		rxkb = S_VALUE(sndp->rx_bytes, sndc->rx_bytes, itv);
		txkb = S_VALUE(sndp->tx_bytes, sndc->tx_bytes, itv);
		ifutil = compute_ifutil(sndc, rxkb, txkb);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sndp->rx_packets, sndc->rx_packets, itv));
		pmiPutValue("network.interface.in.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sndp->tx_packets, sndc->tx_packets, itv));
		pmiPutValue("network.interface.out.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f", rxkb / 1024);
		pmiPutValue("network.interface.in.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f", txkb / 1024);
		pmiPutValue("network.interface.out.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sndp->rx_compressed, sndc->rx_compressed, itv));
		pmiPutValue("network.interface.in.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sndp->tx_compressed, sndc->tx_compressed, itv));
		pmiPutValue("network.interface.out.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sndp->multicast, sndc->multicast, itv));
		pmiPutValue("network.interface.in.multicast", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%f", ifutil);
		pmiPutValue("network.interface.util", sndc->interface, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display network interfaces errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_edev_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i, j;
	struct stats_net_edev *snedc, *snedp, snedzero;
	char buf[64];

	memset(&snedzero, 0, STATS_NET_EDEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		j = check_net_edev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			snedp = &snedzero;
		}
		else {
			snedp = (struct stats_net_edev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->rx_errors, snedc->rx_errors, itv));
		pmiPutValue("network.interface.in.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->tx_errors, snedc->tx_errors, itv));
		pmiPutValue("network.interface.out.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->collisions, snedc->collisions, itv));
		pmiPutValue("network.interface.out.collisions", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->rx_dropped, snedc->rx_dropped, itv));
		pmiPutValue("network.interface.in.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->tx_dropped, snedc->tx_dropped, itv));
		pmiPutValue("network.interface.out.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->tx_carrier_errors, snedc->tx_carrier_errors, itv));
		pmiPutValue("network.interface.out.carrier", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->rx_frame_errors, snedc->rx_frame_errors, itv));
		pmiPutValue("network.interface.in.frame", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->rx_fifo_errors, snedc->rx_fifo_errors, itv));
		pmiPutValue("network.interface.in.fifo", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(snedp->tx_fifo_errors, snedc->tx_fifo_errors, itv));
		pmiPutValue("network.interface.out.fifo", snedc->interface, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display serial lines statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_serial_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i, j, j0, found;
	char buf[64], serialno[64];
	struct stats_serial *ssc, *ssp;

	for (i = 0; i < a->nr[curr]; i++) {

		found = FALSE;

		if (a->nr[!curr] > 0) {
			ssc = (struct stats_serial *) ((char *) a->buf[curr] + i * a->msize);

			/* Look for corresponding serial line in previous iteration */
			j = i;

			if (j >= a->nr[!curr]) {
				j = a->nr[!curr] - 1;
			}

			j0 = j;

			do {
				ssp = (struct stats_serial *) ((char *) a->buf[!curr] + j * a->msize);
				if (ssc->line == ssp->line) {
					found = TRUE;
					break;
				}
				if (++j >= a->nr[!curr]) {
					j = 0;
				}
			}
			while (j != j0);
		}

		if (!found)
			continue;

		sprintf(serialno, "serial%d", ssc->line);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->rx, ssc->rx, itv));
		pmiPutValue("serial.in.interrupts", serialno, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->tx, ssc->tx, itv));
		pmiPutValue("serial.out.interrupts", serialno, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->frame, ssc->frame, itv));
		pmiPutValue("serial.frame", serialno, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->parity, ssc->parity, itv));
		pmiPutValue("serial.parity", serialno, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->brk, ssc->brk, itv));
		pmiPutValue("serial.breaks", serialno, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssp->overrun, ssc->overrun, itv));
		pmiPutValue("serial.overrun", serialno, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display NFS client statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfs_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_rpccnt, snnc->nfs_rpccnt, itv));
	pmiPutValue("network.fs.client.call", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_rpcretrans, snnc->nfs_rpcretrans, itv));
	pmiPutValue("network.fs.client.retrans", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_readcnt, snnc->nfs_readcnt, itv));
	pmiPutValue("network.fs.client.read", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_writecnt, snnc->nfs_writecnt, itv));
	pmiPutValue("network.fs.client.write", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_accesscnt, snnc->nfs_accesscnt, itv));
	pmiPutValue("network.fs.client.access", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snnp->nfs_getattcnt, snnc->nfs_getattcnt, itv));
	pmiPutValue("network.fs.client.getattr", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display NFS server statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfsd_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr],
		*snndp = (struct stats_net_nfsd *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_rpccnt, snndc->nfsd_rpccnt, itv));
	pmiPutValue("network.fs.server.call", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_rpcbad, snndc->nfsd_rpcbad, itv));
	pmiPutValue("network.fs.server.badcall", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_netcnt, snndc->nfsd_netcnt, itv));
	pmiPutValue("network.fs.server.packets", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_netudpcnt, snndc->nfsd_netudpcnt, itv));
	pmiPutValue("network.fs.server.udp", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_nettcpcnt, snndc->nfsd_nettcpcnt, itv));
	pmiPutValue("network.fs.server.tcp", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_rchits, snndc->nfsd_rchits, itv));
	pmiPutValue("network.fs.server.hits", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_rcmisses, snndc->nfsd_rcmisses, itv));
	pmiPutValue("network.fs.server.misses", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_readcnt, snndc->nfsd_readcnt, itv));
	pmiPutValue("network.fs.server.read", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_writecnt, snndc->nfsd_writecnt, itv));
	pmiPutValue("network.fs.server.write", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_accesscnt, snndc->nfsd_accesscnt, itv));
	pmiPutValue("network.fs.server.access", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snndp->nfsd_getattcnt, snndc->nfsd_getattcnt, itv));
	pmiPutValue("network.fs.server.getattr", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->sock_inuse);
	pmiPutValue("network.socket.sock_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_inuse);
	pmiPutValue("network.socket.tcp_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp_inuse);
	pmiPutValue("network.socket.udp_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw_inuse);
	pmiPutValue("network.socket.raw_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag_inuse);
	pmiPutValue("network.socket.frag_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_tw);
	pmiPutValue("network.socket.tcp_tw", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InReceives, snic->InReceives, itv));
	pmiPutValue("network.snmp.ip.ipInReceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->ForwDatagrams, snic->ForwDatagrams, itv));
	pmiPutValue("network.snmp.ip.ipForwDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InDelivers, snic->InDelivers, itv));
	pmiPutValue("network.snmp.ip.ipInDelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutRequests, snic->OutRequests, itv));
	pmiPutValue("network.snmp.ip.ipOutRequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->ReasmReqds, snic->ReasmReqds, itv));
	pmiPutValue("network.snmp.ip.ipReasmReqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->ReasmOKs, snic->ReasmOKs, itv));
	pmiPutValue("network.snmp.ip.ipReasmOKs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->FragOKs, snic->FragOKs, itv));
	pmiPutValue("network.snmp.ip.ipFragOKs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->FragCreates, snic->FragCreates, itv));
	pmiPutValue("network.snmp.ip.ipFragCreates", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip_stats(struct activity *a, int curr, unsigned long long itv,
				        struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InHdrErrors, sneic->InHdrErrors, itv));
	pmiPutValue("network.snmp.ip.ipInHdrErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InAddrErrors, sneic->InAddrErrors, itv));
	pmiPutValue("network.snmp.ip.ipInAddrErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InUnknownProtos, sneic->InUnknownProtos, itv));
	pmiPutValue("network.snmp.ip.ipInUnknownProtos", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InDiscards, sneic->InDiscards, itv));
	pmiPutValue("network.snmp.ip.ipInDiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutDiscards, sneic->OutDiscards, itv));
	pmiPutValue("network.snmp.ip.ipOutDiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutNoRoutes, sneic->OutNoRoutes, itv));
	pmiPutValue("network.snmp.ip.ipOutNoRoutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->ReasmFails, sneic->ReasmFails, itv));
	pmiPutValue("network.snmp.ip.ipReasmFails", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->FragFails, sneic->FragFails, itv));
	pmiPutValue("network.snmp.ip.ipFragFails", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InMsgs, snic->InMsgs, itv));
	pmiPutValue("network.snmp.icmp.icmpInMsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutMsgs, snic->OutMsgs, itv));
	pmiPutValue("network.snmp.icmp.icmpOutMsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InEchos, snic->InEchos, itv));
	pmiPutValue("network.snmp.icmp.icmpInEchos", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InEchoReps, snic->InEchoReps, itv));
	pmiPutValue("network.snmp.icmp.icmpInEchoReps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutEchos, snic->OutEchos, itv));
	pmiPutValue("network.snmp.icmp.icmpOutEchos", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutEchoReps, snic->OutEchoReps, itv));
	pmiPutValue("network.snmp.icmp.icmpOutEchoReps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InTimestamps, snic->InTimestamps, itv));
	pmiPutValue("network.snmp.icmp.icmpInTimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InTimestampReps, snic->InTimestampReps, itv));
	pmiPutValue("network.snmp.icmp.icmpInTimestampReps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutTimestamps, snic->OutTimestamps, itv));
	pmiPutValue("network.snmp.icmp.icmpOutTimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutTimestampReps, snic->OutTimestampReps, itv));
	pmiPutValue("network.snmp.icmp.icmpOutTimestampReps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InAddrMasks, snic->InAddrMasks, itv));
	pmiPutValue("network.snmp.icmp.icmpInAddrMasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InAddrMaskReps, snic->InAddrMaskReps, itv));
	pmiPutValue("network.snmp.icmp.icmpInAddrMaskReps", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutAddrMasks, snic->OutAddrMasks, itv));
	pmiPutValue("network.snmp.icmp.icmpOutAddrMasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutAddrMaskReps, snic->OutAddrMaskReps, itv));
	pmiPutValue("network.snmp.icmp.icmpOutAddrMaskReps", NULL, buf);
#endif	/* HAVE_PCP */
}


/*
 ***************************************************************************
 * Display ICMP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp_stats(struct activity *a, int curr, unsigned long long itv,
					  struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InErrors, sneic->InErrors, itv));
	pmiPutValue("network.snmp.icmp.icmpInErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutErrors, sneic->OutErrors, itv));
	pmiPutValue("network.snmp.icmp.icmpOutErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InDestUnreachs, sneic->InDestUnreachs, itv));
	pmiPutValue("network.snmp.icmp.icmpInDestUnreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutDestUnreachs, sneic->OutDestUnreachs, itv));
	pmiPutValue("network.snmp.icmp.icmpOutDestUnreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InTimeExcds, sneic->InTimeExcds, itv));
	pmiPutValue("network.snmp.icmp.icmpInTimeExcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutTimeExcds, sneic->OutTimeExcds, itv));
	pmiPutValue("network.snmp.icmp.icmpOutTimeExcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InParmProbs, sneic->InParmProbs, itv));
	pmiPutValue("network.snmp.icmp.icmpInParmProbs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutParmProbs, sneic->OutParmProbs, itv));
	pmiPutValue("network.snmp.icmp.icmpOutParmProbs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InSrcQuenchs, sneic->InSrcQuenchs, itv));
	pmiPutValue("network.snmp.icmp.icmpInSrcQuenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutSrcQuenchs, sneic->OutSrcQuenchs, itv));
	pmiPutValue("network.snmp.icmp.icmpOutSrcQuenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InRedirects, sneic->InRedirects, itv));
	pmiPutValue("network.snmp.icmp.icmpInRedirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutRedirects, sneic->OutRedirects, itv));
	pmiPutValue("network.snmp.icmp.icmpOutRedirects", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display TCP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_tcp_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sntp->ActiveOpens, sntc->ActiveOpens, itv));
	pmiPutValue("network.snmp.tcp.tcpActiveOpens", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sntp->PassiveOpens, sntc->PassiveOpens, itv));
	pmiPutValue("network.snmp.tcp.tcpPassiveOpens", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sntp->InSegs, sntc->InSegs, itv));
	pmiPutValue("network.snmp.tcp.tcpInSegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sntp->OutSegs, sntc->OutSegs, itv));
	pmiPutValue("network.snmp.tcp.tcpOutSegs", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display TCP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_etcp_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snetp->AttemptFails, snetc->AttemptFails, itv));
	pmiPutValue("network.snmp.tcp.tcpAttemptFails", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snetp->EstabResets, snetc->EstabResets, itv));
	pmiPutValue("network.snmp.tcp.tcpEstabResets", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snetp->RetransSegs, snetc->RetransSegs, itv));
	pmiPutValue("network.snmp.tcp.tcpRetransSegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snetp->InErrs, snetc->InErrs, itv));
	pmiPutValue("network.snmp.tcp.tcpInErrs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snetp->OutRsts, snetc->OutRsts, itv));
	pmiPutValue("network.snmp.tcp.tcpOutRsts", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display UDP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->InDatagrams, snuc->InDatagrams, itv));
	pmiPutValue("network.snmp.udp.udpInDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->OutDatagrams, snuc->OutDatagrams, itv));
	pmiPutValue("network.snmp.udp.udpOutDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->NoPorts, snuc->NoPorts, itv));
	pmiPutValue("network.snmp.udp.udpNoPorts", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->InErrors, snuc->InErrors, itv));
	pmiPutValue("network.snmp.udp.udpInErrors", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock6_stats(struct activity *a, int curr, unsigned long long itv,
					  struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->tcp6_inuse);
	pmiPutValue("network.socket6.tcp6_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp6_inuse);
	pmiPutValue("network.socket6.udp6_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw6_inuse);
	pmiPutValue("network.socket6.raw6_inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag6_inuse);
	pmiPutValue("network.socket6.frag6_inuse", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip6_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InReceives6, snic->InReceives6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInReceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutForwDatagrams6, snic->OutForwDatagrams6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutForwDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InDelivers6, snic->InDelivers6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInDelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutRequests6, snic->OutRequests6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutRequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->ReasmReqds6, snic->ReasmReqds6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsReasmReqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->ReasmOKs6, snic->ReasmOKs6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsReasmOKs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InMcastPkts6, snic->InMcastPkts6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInMcastPkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutMcastPkts6, snic->OutMcastPkts6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutMcastPkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->FragOKs6, snic->FragOKs6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutFragOKs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->FragCreates6, snic->FragCreates6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutFragCreates", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip6_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InHdrErrors6, sneic->InHdrErrors6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInHdrErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InAddrErrors6, sneic->InAddrErrors6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInAddrErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InUnknownProtos6, sneic->InUnknownProtos6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInUnknownProtos", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InTooBigErrors6, sneic->InTooBigErrors6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInTooBigErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InDiscards6, sneic->InDiscards6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInDiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutDiscards6, sneic->OutDiscards6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutDiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InNoRoutes6, sneic->InNoRoutes6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInNoRoutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutNoRoutes6, sneic->OutNoRoutes6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutNoRoutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->ReasmFails6, sneic->ReasmFails6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsReasmFails", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->FragFails6, sneic->FragFails6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsOutFragFails", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InTruncatedPkts6, sneic->InTruncatedPkts6, itv));
	pmiPutValue("network.snmp.ip6.ipv6IfStatsInTruncatedPkts", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp6_stats(struct activity *a, int curr, unsigned long long itv,
					  struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InMsgs6, snic->InMsgs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInMsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutMsgs6, snic->OutMsgs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutMsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InEchos6, snic->InEchos6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInEchos", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InEchoReplies6, snic->InEchoReplies6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInEchoReplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutEchoReplies6, snic->OutEchoReplies6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutEchoReplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InGroupMembQueries6, snic->InGroupMembQueries6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInGroupMembQueries", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InGroupMembResponses6, snic->InGroupMembResponses6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInGroupMembResponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutGroupMembResponses6, snic->OutGroupMembResponses6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutGroupMembResponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InGroupMembReductions6, snic->InGroupMembReductions6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInGroupMembReductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutGroupMembReductions6, snic->OutGroupMembReductions6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutGroupMembReductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InRouterSolicits6, snic->InRouterSolicits6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInRouterSolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutRouterSolicits6, snic->OutRouterSolicits6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutRouterSolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InRouterAdvertisements6, snic->InRouterAdvertisements6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInRouterAdvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InNeighborSolicits6, snic->InNeighborSolicits6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInNeighborSolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutNeighborSolicits6, snic->OutNeighborSolicits6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutNeighborSolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->InNeighborAdvertisements6, snic->InNeighborAdvertisements6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInNeighborAdvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snip->OutNeighborAdvertisements6, snic->OutNeighborAdvertisements6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutNeighborAdvertisements", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp6_stats(struct activity *a, int curr, unsigned long long itv,
					   struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InErrors6, sneic->InErrors6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInErrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InDestUnreachs6, sneic->InDestUnreachs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInDestUnreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutDestUnreachs6, sneic->OutDestUnreachs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutDestUnreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InTimeExcds6, sneic->InTimeExcds6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInTimeExcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutTimeExcds6, sneic->OutTimeExcds6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutTimeExcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InParmProblems6, sneic->InParmProblems6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInParmProblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutParmProblems6, sneic->OutParmProblems6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutParmProblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InRedirects6, sneic->InRedirects6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInRedirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutRedirects6, sneic->OutRedirects6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutRedirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->InPktTooBigs6, sneic->InPktTooBigs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpInPktTooBigs", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(sneip->OutPktTooBigs6, sneic->OutPktTooBigs6, itv));
	pmiPutValue("network.snmp.icmp6.ipv6IfIcmpOutPktTooBigs", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display UDPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp6_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->InDatagrams6, snuc->InDatagrams6, itv));
	pmiPutValue("network.snmp.udp6.udpInDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->OutDatagrams6, snuc->OutDatagrams6, itv));
	pmiPutValue("network.snmp.udp6.udpOutDatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->NoPorts6, snuc->NoPorts6, itv));
	pmiPutValue("network.snmp.udp6.udpNoPorts", NULL, buf);

	snprintf(buf, sizeof(buf), "%f",
		S_VALUE(snup->InErrors6, snuc->InErrors6, itv));
	pmiPutValue("network.snmp.udp6.udpInErrors", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_cpufreq_stats(struct activity *a, int curr, unsigned long long itv,
					    struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_cpufreq *spc;
	char buf[64], cpuno[64];
	char *str;

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			str = NULL;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
			str = cpuno;
		}

		snprintf(buf, sizeof(buf), "%f", ((double) spc->cpufreq) / 100);
		pmiPutValue(i ? "kernel.percpu.cpu.freqMHz" : "kernel.all.cpu.freqMHz", str, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display fan statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_fan_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_fan *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "fan%d", i + 1);

		snprintf(buf, sizeof(buf), "%llu",
			 (unsigned long long) spc->rpm);
		pmiPutValue("power.fan.rpm", instance, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 (unsigned long long) (spc->rpm - spc->rpm_min));
		pmiPutValue("power.fan.drpm", instance, buf);

		snprintf(buf, sizeof(buf), "%s",
			spc->device);
		pmiPutValue("power.fan.device", instance, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display temperature statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_temp_stats(struct activity *a, int curr, unsigned long long itv,
					 struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_temp *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "temp%d", i + 1);

		snprintf(buf, sizeof(buf), "%f",
			 spc->temp);
		pmiPutValue("power.temp.degC", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->temp_max - spc->temp_min) ?
			 (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			 0.0);
		pmiPutValue("power.temp.temp_pct", instance, buf);

		snprintf(buf, sizeof(buf), "%s",
			spc->device);
		pmiPutValue("power.temp.device", instance, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display voltage inputs statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_in_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_in *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "in%d", i);

		snprintf(buf, sizeof(buf), "%f",
			 spc->in);
		pmiPutValue("power.in.inV", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->in_max - spc->in_min) ?
			 (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			 0.0);
		pmiPutValue("power.in.in_pct", instance, buf);

		snprintf(buf, sizeof(buf), "%s",
			spc->device);
		pmiPutValue("power.in.device", instance, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display huge pages statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_huge_stats(struct activity *a, int curr, unsigned long long itv,
				     struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", smc->frhkb);
	pmiPutValue("mem.huge.free", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->tlhkb - smc->frhkb);
	pmiPutValue("mem.huge.used", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", smc->tlhkb ? SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) : 0.0);
	pmiPutValue("mem.huge.used_pct", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->rsvdhkb);
	pmiPutValue("mem.huge.reserved", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->surphkb);
	pmiPutValue("mem.huge.surplus", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display USB devices in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_usb_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_usb *suc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "usb%d", i);

		snprintf(buf, sizeof(buf), "%d", suc->bus_nr);
		pmiPutValue("power.usb.bus", instance, buf);

		snprintf(buf, sizeof(buf), "%x", suc->vendor_id);
		pmiPutValue("power.usb.vendorId", instance, buf);

		snprintf(buf, sizeof(buf), "%x", suc->product_id);
		pmiPutValue("power.usb.productId", instance, buf);

		snprintf(buf, sizeof(buf), "%u", suc->bmaxpower << 1);
		pmiPutValue("power.usb.maxpower", instance, buf);

		snprintf(buf, sizeof(buf), "%s", suc->manufacturer);
		pmiPutValue("power.usb.manufacturer", instance, buf);

		snprintf(buf, sizeof(buf), "%s", suc->product);
		pmiPutValue("power.usb.productName", instance, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display filesystem statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_filesystem_stats(struct activity *a, int curr, unsigned long long itv,
					   struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_filesystem *sfc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list,
					      DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%.0f",
			 (double) sfc->f_bfree / 1024 / 1024);
		pmiPutValue("fs.util.fsfree",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%.0f",
			 (double) (sfc->f_blocks - sfc->f_bfree) / 1024 / 1024);
		pmiPutValue("fs.util.fsused",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				       : 0.0);
		pmiPutValue("fs.util.fsused_pct",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 sfc->f_blocks ? SP_VALUE(sfc->f_bavail, sfc->f_blocks, sfc->f_blocks)
				       : 0.0);
		pmiPutValue("fs.util.ufsused_pct",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 sfc->f_ffree);
		pmiPutValue("fs.util.ifree",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 sfc->f_files - sfc->f_ffree);
		pmiPutValue("fs.util.iused",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 sfc->f_files ? SP_VALUE(sfc->f_ffree, sfc->f_files, sfc->f_files)
				      : 0.0);
		pmiPutValue("fs.util.iused_pct",
			    DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display softnet statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_softnet_stats(struct activity *a, int curr, unsigned long long itv,
					struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_softnet *ssnc, *ssnp;
	char buf[64], cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	char *str;

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
                ssnp = (struct stats_softnet *) ((char *) a->buf[!curr] + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			str = NULL;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
			str = cpuno;
		}

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssnp->processed, ssnc->processed, itv));
		pmiPutValue(i ? "network.percpu.soft.processed" : "network.all.soft.processed", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssnp->dropped, ssnc->dropped, itv));
		pmiPutValue(i ? "network.percpu.soft.dropped" : "network.all.soft.dropped", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssnp->time_squeeze, ssnc->time_squeeze, itv));
		pmiPutValue(i ? "network.percpu.soft.time_squeeze" : "network.all.soft.time_squeeze", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssnp->received_rps, ssnc->received_rps, itv));
		pmiPutValue(i ? "network.percpu.soft.received_rps" : "network.all.soft.received_rps", str, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(ssnp->flow_limit, ssnc->flow_limit, itv));
		pmiPutValue(i ? "network.percpu.soft.flow_limit" : "network.all.soft.flow_limit", str, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display Fibre Channel HBA statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_fchost_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	int i, j, j0, found;
	struct stats_fchost *sfcc, *sfcp, sfczero;
	char buf[64];

	memset(&sfczero, 0, sizeof(struct stats_fchost));

	for (i = 0; i < a->nr[curr]; i++) {

		found = FALSE;
		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		if (a->nr[!curr] > 0) {
			/* Look for corresponding structure in previous iteration */
			j = i;

			if (j >= a->nr[!curr]) {
				j = a->nr[!curr] - 1;
			}

			j0 = j;

			do {
				sfcp = (struct stats_fchost *) ((char *) a->buf[!curr] + j * a->msize);
				if (!strcmp(sfcc->fchost_name, sfcp->fchost_name)) {
					found = TRUE;
					break;
				}
				if (++j >= a->nr[!curr]) {
					j = 0;
				}
			}
			while (j != j0);
		}

		if (!found) {
			/* This is a newly registered host */
			sfcp = &sfczero;
		}

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sfcp->f_rxframes, sfcc->f_rxframes, itv));
		pmiPutValue("network.fchost.in.frame", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sfcp->f_txframes, sfcc->f_txframes, itv));
		pmiPutValue("network.fchost.out.frame", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sfcp->f_rxwords, sfcc->f_rxwords, itv));
		pmiPutValue("network.fchost.in.word", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 S_VALUE(sfcp->f_txwords, sfcc->f_txwords, itv));
		pmiPutValue("network.fchost.out.word", sfcc->fchost_name, buf);
	}
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_psicpu_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr],
		*psip = (struct stats_psi_cpu *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_10 / 100);
	pmiPutValue("psi.cpu.some.trends", "10 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_60 / 100);
	pmiPutValue("psi.cpu.some.trends", "60 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_300 / 100);
	pmiPutValue("psi.cpu.some.trends", "300 sec", buf);

	snprintf(buf, sizeof(buf), "%f",
		 ((double) psic->some_cpu_total - psip->some_cpu_total) / (100 * itv));
	pmiPutValue("psi.cpu.some.avg", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_psiio_stats(struct activity *a, int curr, unsigned long long itv,
				      struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr],
		*psip = (struct stats_psi_io *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_10 / 100);
	pmiPutValue("psi.io.some.trends", "10 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_60 / 100);
	pmiPutValue("psi.io.some.trends", "60 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_300 / 100);
	pmiPutValue("psi.io.some.trends", "300 sec", buf);

	snprintf(buf, sizeof(buf), "%f",
		 ((double) psic->some_io_total - psip->some_io_total) / (100 * itv));
	pmiPutValue("psi.io.some.avg", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_10 / 100);
	pmiPutValue("psi.io.full.trends", "10 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_60 / 100);
	pmiPutValue("psi.io.full.trends", "60 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_300 / 100);
	pmiPutValue("psi.io.full.trends", "300 sec", buf);

	snprintf(buf, sizeof(buf), "%f",
		 ((double) psic->full_io_total - psip->full_io_total) / (100 * itv));
	pmiPutValue("psi.io.full.avg", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @record_hdr	Record header for current sample.
 ***************************************************************************
 */
__print_funct_t pcp_print_psimem_stats(struct activity *a, int curr, unsigned long long itv,
				       struct record_header *record_hdr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr],
		*psip = (struct stats_psi_mem *) a->buf[!curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_10 / 100);
	pmiPutValue("psi.mem.some.trends", "10 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_60 / 100);
	pmiPutValue("psi.mem.some.trends", "60 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_300 / 100);
	pmiPutValue("psi.mem.some.trends", "300 sec", buf);

	snprintf(buf, sizeof(buf), "%f",
		 ((double) psic->some_mem_total - psip->some_mem_total) / (100 * itv));
	pmiPutValue("psi.mem.some.avg", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_10 / 100);
	pmiPutValue("psi.mem.full.trends", "10 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_60 / 100);
	pmiPutValue("psi.mem.full.trends", "60 sec", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_300 / 100);
	pmiPutValue("psi.mem.full.trends", "300 sec", buf);

	snprintf(buf, sizeof(buf), "%f",
		 ((double) psic->full_mem_total - psip->full_mem_total) / (100 * itv));
	pmiPutValue("psi.mem.full.avg", NULL, buf);
#endif	/* HAVE_PCP */
}
