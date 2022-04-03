/*
 * pcp_stats.c: Functions used by sadf to create PCP archive files.
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

#include "sa.h"
#include "pcp_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern uint64_t flags;

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
__print_funct_t pcp_print_cpu_stats(struct activity *a, int curr)
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

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_user - scc->cpu_guest);
		pmiPutValue(i ? "kernel.percpu.cpu.user" : "kernel.all.cpu.user", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_nice - scc->cpu_guest_nice);
		pmiPutValue(i ? "kernel.percpu.cpu.nice" : "kernel.all.cpu.nice", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_sys);
		pmiPutValue(i ? "kernel.percpu.cpu.sys" : "kernel.all.cpu.sys", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_iowait);
		pmiPutValue(i ? "kernel.percpu.cpu.iowait" : "kernel.all.cpu.iowait", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_steal);
		pmiPutValue(i ? "kernel.percpu.cpu.steal" : "kernel.all.cpu.steal", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_hardirq + scc->cpu_softirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.total" : "kernel.all.cpu.irq.total", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_hardirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.hard" : "kernel.all.cpu.irq.hard", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_softirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.soft" : "kernel.all.cpu.irq.soft", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_guest);
		pmiPutValue(i ? "kernel.percpu.cpu.guest" : "kernel.all.cpu.guest", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_guest_nice);
		pmiPutValue(i ? "kernel.percpu.cpu.guest_nice" : "kernel.all.cpu.guest_nice", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_idle);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_pcsw_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->context_switch);
	pmiPutValue("kernel.all.pswitch", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", spc->processes);
	pmiPutValue("kernel.all.sysfork", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display interrupts statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_irq_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i, c;
	char buf[64], name[64];
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
			if (masked_cpu_bitmap[c >> 3] & (1 << (c & 0x07)))
				/* No */
				continue;

			snprintf(buf, sizeof(buf), "%u", stc_cpu_irq->irq_nr);

			if (!c) {
				/* This is CPU "all" */
				if (!i) {
					/* This is interrupt "sum" */
					pmiPutValue("kernel.all.intr", NULL, buf);
				}
				else {
					pmiPutValue("kernel.all.interrupts.total",
						    stc_cpuall_irq->irq_name, buf);
				}
			}
			else {
				/* This is a particular CPU */
				snprintf(name, sizeof(name), "%s::cpu%d",
					 stc_cpuall_irq->irq_name, c - 1);
				name[sizeof(name) - 1] = '\0';

				pmiPutValue("kernel.percpu.interrupts", name, buf);
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
__print_funct_t pcp_print_swap_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", ssc->pswpin);
	pmiPutValue("swap.pagesin", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", ssc->pswpout);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_paging_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgpgin);
	pmiPutValue("mem.vmstat.pgpgin", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgpgout);
	pmiPutValue("mem.vmstat.pgpgout", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgfault);
	pmiPutValue("mem.vmstat.pgfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgmajfault);
	pmiPutValue("mem.vmstat.pgmajfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgfree);
	pmiPutValue("mem.vmstat.pgfree", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgscan_kswapd);
	pmiPutValue("mem.vmstat.pgscan_kswapd_total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgscan_direct);
	pmiPutValue("mem.vmstat.pgscan_direct_total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgsteal);
	pmiPutValue("mem.vmstat.pgsteal_total", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_io_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive);
	pmiPutValue("disk.all.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_rio);
	pmiPutValue("disk.all.read", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu",sic->dk_drive_wio);
	pmiPutValue("disk.all.write", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_dio);
	pmiPutValue("disk.all.discard", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_rblk);
	pmiPutValue("disk.all.read_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_wblk);
	pmiPutValue("disk.all.write_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_dblk);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_memory_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	if (DISPLAY_MEMORY(a->opt_flags)) {

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) (smc->tlmkb >> 10));
		pmiPutValue("hinv.physmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlmkb);
		pmiPutValue("mem.physmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->frmkb);
		pmiPutValue("mem.util.free", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->availablekb);
		pmiPutValue("mem.util.available", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlmkb - smc->frmkb);
		pmiPutValue("mem.util.used", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->bufkb);
		pmiPutValue("mem.util.bufmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->camkb);
		pmiPutValue("mem.util.cached", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->comkb);
		pmiPutValue("mem.util.committed_AS", NULL, buf);

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
			pmiPutValue("mem.util.kernelStack", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->pgtblkb);
			pmiPutValue("mem.util.pageTables", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->vmusedkb);
			pmiPutValue("mem.util.vmallocUsed", NULL, buf);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		snprintf(buf, sizeof(buf), "%llu", smc->frskb);
		pmiPutValue("mem.util.swapFree", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlskb);
		pmiPutValue("mem.util.swapTotal", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->caskb);
		pmiPutValue("mem.util.swapCached", NULL, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_ktables_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->dentry_stat);
	pmiPutValue("vfs.dentry.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->file_used);
	pmiPutValue("vfs.files.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->inode_used);
	pmiPutValue("vfs.inodes.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", skc->pty_nr);
	pmiPutValue("kernel.all.nptys", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display queue and load statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_queue_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->nr_running);
	pmiPutValue("kernel.all.runnable", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->nr_threads);
	pmiPutValue("kernel.all.nprocs", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->procs_blocked);
	pmiPutValue("kernel.all.blocked", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_1 / 100);
	pmiPutValue("kernel.all.load", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_5 / 100);
	pmiPutValue("kernel.all.load", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_15 / 100);
	pmiPutValue("kernel.all.load", "15 minute", buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display disks statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_disk_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_disk *sdc;
	char *dev_name;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

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

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->nr_ios);
		pmiPutValue("disk.dev.total", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) (sdc->rd_sect + sdc->wr_sect) / 2);
		pmiPutValue("disk.dev.total_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->rd_sect / 2);
		pmiPutValue("disk.dev.read_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->wr_sect / 2);
		pmiPutValue("disk.dev.write_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->dc_sect / 2);
		pmiPutValue("disk.dev.discard_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->rd_ticks + sdc->wr_ticks);
		pmiPutValue("disk.dev.total_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->rd_ticks);
		pmiPutValue("disk.dev.read_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->wr_ticks);
		pmiPutValue("disk.dev.write_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->dc_ticks);
		pmiPutValue("disk.dev.discard_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->tot_ticks);
		pmiPutValue("disk.dev.avactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->rq_ticks);
		pmiPutValue("disk.dev.aveq", dev_name, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_net_dev_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_net_dev *sndc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		/*
		 * No need to look for the previous sample values: PCP displays the raw
		 * counter value, not its variation over the interval.
		 * The whole list of network interfaces present in file has been created
		 * (this is goal of the FO_ITEM_LIST option set for pcp_fmt report format -
		 * see format.c). So no need to wonder if an instance needs to be created
		 * for current interface.
		 */

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_packets);
		pmiPutValue("network.interface.in.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_packets);
		pmiPutValue("network.interface.out.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_bytes);
		pmiPutValue("network.interface.in.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_bytes);
		pmiPutValue("network.interface.out.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_compressed);
		pmiPutValue("network.interface.in.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_compressed);
		pmiPutValue("network.interface.out.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->multicast);
		pmiPutValue("network.interface.in.mcasts", sndc->interface, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_net_edev_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_net_edev *snedc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_errors);
		pmiPutValue("network.interface.in.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_errors);
		pmiPutValue("network.interface.out.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->collisions);
		pmiPutValue("network.interface.collisions", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_dropped);
		pmiPutValue("network.interface.in.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_dropped);
		pmiPutValue("network.interface.out.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_carrier_errors);
		pmiPutValue("network.interface.out.carrier", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_frame_errors);
		pmiPutValue("network.interface.in.frame", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_fifo_errors);
		pmiPutValue("network.interface.in.fifo", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_fifo_errors);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_serial_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	char buf[64], serialno[64];
	struct stats_serial *ssc;

	for (i = 0; i < a->nr[curr]; i++) {

		ssc = (struct stats_serial *) ((char *) a->buf[curr] + i * a->msize);

		snprintf(serialno, sizeof(serialno), "serial%u", ssc->line);

		snprintf(buf, sizeof(buf), "%u", ssc->rx);
		pmiPutValue("tty.serial.rx", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->tx);
		pmiPutValue("tty.serial.tx", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->frame);
		pmiPutValue("tty.serial.frame", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->parity);
		pmiPutValue("tty.serial.parity", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->brk);
		pmiPutValue("tty.serial.brk", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->overrun);
		pmiPutValue("tty.serial.overrun", serialno, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfs_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_rpccnt);
	pmiPutValue("rpc.client.rpccnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_rpcretrans);
	pmiPutValue("rpc.client.rpcretrans", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_readcnt);
	pmiPutValue("nfs.client.reqs", "read", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_writecnt);
	pmiPutValue("nfs.client.reqs", "write", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_accesscnt);
	pmiPutValue("nfs.client.reqs", "access", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_getattcnt);
	pmiPutValue("nfs.client.reqs", "getattr", buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display NFS server statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfsd_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rpccnt);
	pmiPutValue("rpc.server.rpccnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rpcbad);
	pmiPutValue("rpc.server.rpcbadclnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_netcnt);
	pmiPutValue("rpc.server.netcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_netudpcnt);
	pmiPutValue("rpc.server.netudpcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_nettcpcnt);
	pmiPutValue("rpc.server.nettcpcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rchits);
	pmiPutValue("rpc.server.rchits", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rcmisses);
	pmiPutValue("rpc.server.rcmisses", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_readcnt);
	pmiPutValue("nfs.server.reqs", "read", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_writecnt);
	pmiPutValue("nfs.server.reqs", "write", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_accesscnt);
	pmiPutValue("nfs.server.reqs", "access", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_getattcnt);
	pmiPutValue("nfs.server.reqs", "getattr", buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->sock_inuse);
	pmiPutValue("network.sockstat.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_inuse);
	pmiPutValue("network.sockstat.tcp.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp_inuse);
	pmiPutValue("network.sockstat.udp.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw_inuse);
	pmiPutValue("network.sockstat.raw.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag_inuse);
	pmiPutValue("network.sockstat.frag.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_tw);
	pmiPutValue("network.sockstat.tcp.tw", NULL, buf);
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
__print_funct_t pcp_print_net_ip_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", snic->InReceives);
	pmiPutValue("network.ip.inreceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ForwDatagrams);
	pmiPutValue("network.ip.forwdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InDelivers);
	pmiPutValue("network.ip.indelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutRequests);
	pmiPutValue("network.ip.outrequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmReqds);
	pmiPutValue("network.ip.reasmreqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmOKs);
	pmiPutValue("network.ip.reasmoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragOKs);
	pmiPutValue("network.ip.fragoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragCreates);
	pmiPutValue("network.ip.fragcreates", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sneic->InHdrErrors);
	pmiPutValue("network.ip.inhdrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InAddrErrors);
	pmiPutValue("network.ip.inaddrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InUnknownProtos);
	pmiPutValue("network.ip.inunknownprotos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InDiscards);
	pmiPutValue("network.ip.indiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutDiscards);
	pmiPutValue("network.ip.outdiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutNoRoutes);
	pmiPutValue("network.ip.outnoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->ReasmFails);
	pmiPutValue("network.ip.reasmfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->FragFails);
	pmiPutValue("network.ip.fragfails", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InMsgs);
	pmiPutValue("network.icmp.inmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutMsgs);
	pmiPutValue("network.icmp.outmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchos);
	pmiPutValue("network.icmp.inechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchoReps);
	pmiPutValue("network.icmp.inechoreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchos);
	pmiPutValue("network.icmp.outechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchoReps);
	pmiPutValue("network.icmp.outechoreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InTimestamps);
	pmiPutValue("network.icmp.intimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InTimestampReps);
	pmiPutValue("network.icmp.intimestampreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutTimestamps);
	pmiPutValue("network.icmp.outtimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutTimestampReps);
	pmiPutValue("network.icmp.outtimestampreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InAddrMasks);
	pmiPutValue("network.icmp.inaddrmasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InAddrMaskReps);
	pmiPutValue("network.icmp.inaddrmaskreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutAddrMasks);
	pmiPutValue("network.icmp.outaddrmasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutAddrMaskReps);
	pmiPutValue("network.icmp.outaddrmaskreps", NULL, buf);
#endif	/* HAVE_PCP */
}


/*
 ***************************************************************************
 * Display ICMP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InErrors);
	pmiPutValue("network.icmp.inerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutErrors);
	pmiPutValue("network.icmp.outerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InDestUnreachs);
	pmiPutValue("network.icmp.indestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutDestUnreachs);
	pmiPutValue("network.icmp.outdestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InTimeExcds);
	pmiPutValue("network.icmp.intimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutTimeExcds);
	pmiPutValue("network.icmp.outtimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InParmProbs);
	pmiPutValue("network.icmp.inparmprobs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutParmProbs);
	pmiPutValue("network.icmp.outparmprobs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InSrcQuenchs);
	pmiPutValue("network.icmp.insrcquenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutSrcQuenchs);
	pmiPutValue("network.icmp.outsrcquenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InRedirects);
	pmiPutValue("network.icmp.inredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutRedirects);
	pmiPutValue("network.icmp.outredirects", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display TCP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_tcp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->ActiveOpens);
	pmiPutValue("network.tcp.activeopens", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->PassiveOpens);
	pmiPutValue("network.tcp.passiveopens", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->InSegs);
	pmiPutValue("network.tcp.insegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->OutSegs);
	pmiPutValue("network.tcp.outsegs", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display TCP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_etcp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->AttemptFails);
	pmiPutValue("network.tcp.attemptfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->EstabResets);
	pmiPutValue("network.tcp.estabresets", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->RetransSegs);
	pmiPutValue("network.tcp.retranssegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->InErrs);
	pmiPutValue("network.tcp.inerrs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->OutRsts);
	pmiPutValue("network.tcp.outrsts", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display UDP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InDatagrams);
	pmiPutValue("network.udp.indatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->OutDatagrams);
	pmiPutValue("network.udp.outdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->NoPorts);
	pmiPutValue("network.udp.noports", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InErrors);
	pmiPutValue("network.udp.inerrors", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->tcp6_inuse);
	pmiPutValue("network.sockstat.tcp6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp6_inuse);
	pmiPutValue("network.sockstat.udp6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw6_inuse);
	pmiPutValue("network.sockstat.raw6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag6_inuse);
	pmiPutValue("network.sockstat.frag6.inuse", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", snic->InReceives6);
	pmiPutValue("network.ip6.inreceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutForwDatagrams6);
	pmiPutValue("network.ip6.outforwdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InDelivers6);
	pmiPutValue("network.ip6.indelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutRequests6);
	pmiPutValue("network.ip6.outrequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmReqds6);
	pmiPutValue("network.ip6.reasmreqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmOKs6);
	pmiPutValue("network.ip6.reasmoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InMcastPkts6);
	pmiPutValue("network.ip6.inmcastpkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutMcastPkts6);
	pmiPutValue("network.ip6.outmcastpkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragOKs6);
	pmiPutValue("network.ip6.fragoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragCreates6);
	pmiPutValue("network.ip6.fragcreates", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sneic->InHdrErrors6);
	pmiPutValue("network.ip6.inhdrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InAddrErrors6);
	pmiPutValue("network.ip6.inaddrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InUnknownProtos6);
	pmiPutValue("network.ip6.inunknownprotos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InTooBigErrors6);
	pmiPutValue("network.ip6.intoobigerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InDiscards6);
	pmiPutValue("network.ip6.indiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutDiscards6);
	pmiPutValue("network.ip6.outdiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InNoRoutes6);
	pmiPutValue("network.ip6.innoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutNoRoutes6);
	pmiPutValue("network.ip6.outnoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->ReasmFails6);
	pmiPutValue("network.ip6.reasmfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->FragFails6);
	pmiPutValue("network.ip6.fragfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InTruncatedPkts6);
	pmiPutValue("network.ip6.intruncatedpkts", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InMsgs6);
	pmiPutValue("network.icmp6.inmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutMsgs6);
	pmiPutValue("network.icmp6.outmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchos6);
	pmiPutValue("network.icmp6.inechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchoReplies6);
	pmiPutValue("network.icmp6.inechoreplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchoReplies6);
	pmiPutValue("network.icmp6.outechoreplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembQueries6);
	pmiPutValue("network.icmp6.ingroupmembqueries", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembResponses6);
	pmiPutValue("network.icmp6.ingroupmembresponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutGroupMembResponses6);
	pmiPutValue("network.icmp6.outgroupmembresponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembReductions6);
	pmiPutValue("network.icmp6.ingroupmembreductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutGroupMembReductions6);
	pmiPutValue("network.icmp6.outgroupmembreductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InRouterSolicits6);
	pmiPutValue("network.icmp6.inroutersolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutRouterSolicits6);
	pmiPutValue("network.icmp6.outroutersolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InRouterAdvertisements6);
	pmiPutValue("network.icmp6.inrouteradvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InNeighborSolicits6);
	pmiPutValue("network.icmp6.inneighborsolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutNeighborSolicits6);
	pmiPutValue("network.icmp6.outneighborsolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InNeighborAdvertisements6);
	pmiPutValue("network.icmp6.inneighboradvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutNeighborAdvertisements6);
	pmiPutValue("network.icmp6.outneighboradvertisements", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display ICMPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InErrors6);
	pmiPutValue("network.icmp6.inerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InDestUnreachs6);
	pmiPutValue("network.icmp6.indestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutDestUnreachs6);
	pmiPutValue("network.icmp6.outdestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InTimeExcds6);
	pmiPutValue("network.icmp6.intimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutTimeExcds6);
	pmiPutValue("network.icmp6.outtimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InParmProblems6);
	pmiPutValue("network.icmp6.inparmproblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutParmProblems6);
	pmiPutValue("network.icmp6.outparmproblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InRedirects6);
	pmiPutValue("network.icmp6.inredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutRedirects6);
	pmiPutValue("network.icmp6.outredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InPktTooBigs6);
	pmiPutValue("network.icmp6.inpkttoobigs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutPktTooBigs6);
	pmiPutValue("network.icmp6.outpkttoobigs", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display UDPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp6_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InDatagrams6);
	pmiPutValue("network.udp6.indatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->OutDatagrams6);
	pmiPutValue("network.udp6.outdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->NoPorts6);
	pmiPutValue("network.udp6.noports", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InErrors6);
	pmiPutValue("network.udp6.inerrors", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_cpufreq_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_cpufreq *spc;
	char buf[64], cpuno[64];

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			continue;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
		}

		snprintf(buf, sizeof(buf), "%f", ((double) spc->cpufreq) / 100);
		pmiPutValue("hinv.cpu.clock", cpuno, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_fan_stats(struct activity *a, int curr)
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

		snprintf(buf, sizeof(buf), "%s", spc->device);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_temp_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_pwr_temp *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "temp%d", i + 1);

		snprintf(buf, sizeof(buf), "%f", spc->temp);
		pmiPutValue("power.temp.celsius", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->temp_max - spc->temp_min) ?
			 (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			 0.0);
		pmiPutValue("power.temp.percent", instance, buf);

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
__print_funct_t pcp_print_pwr_in_stats(struct activity *a, int curr)
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
		pmiPutValue("power.in.voltage", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->in_max - spc->in_min) ?
			 (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			 0.0);
		pmiPutValue("power.in.percent", instance, buf);

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
__print_funct_t pcp_print_huge_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", smc->frhkb * 1024);
	pmiPutValue("mem.util.hugepagesFreeBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->tlhkb * 1024);
	pmiPutValue("mem.util.hugepagesTotalBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->rsvdhkb * 1024);
	pmiPutValue("mem.util.hugepagesRsvdBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->surphkb * 1024);
	pmiPutValue("mem.util.hugepagesSurpBytes", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display USB devices in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_usb_stats(struct activity *a, int curr)
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
 ***************************************************************************
 */
__print_funct_t pcp_print_filesystem_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_filesystem *sfc;
	char buf[64];
	char *dev_name;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		/* Get name to display (persistent or standard fs name, or mount point) */
		dev_name = get_fs_name_to_display(a, flags, sfc);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, dev_name))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%llu", sfc->f_blocks / 1024);
		pmiPutValue("filesys.capacity", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_bfree / 1024);
		pmiPutValue("filesys.free", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 (sfc->f_blocks - sfc->f_bfree) / 1024);
		pmiPutValue("filesys.used", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				       : 0.0);
		pmiPutValue("filesys.full", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_files);
		pmiPutValue("filesys.maxfiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_ffree);
		pmiPutValue("filesys.freefiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_files - sfc->f_ffree);
		pmiPutValue("filesys.usedfiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_bavail / 1024);
		pmiPutValue("filesys.avail", dev_name, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_softnet_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_softnet *ssnc;
	char buf[64], cpuno[64];
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

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->processed);
		pmiPutValue("network.softnet.percpu.processed", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->dropped);
		pmiPutValue("network.softnet.percpu.dropped", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->time_squeeze);
		pmiPutValue("network.softnet.percpu.time_squeeze", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->received_rps);
		pmiPutValue("network.softnet.percpu.received_rps", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->flow_limit);
		pmiPutValue("network.softnet.percpu.flow_limit", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->backlog_len);
		pmiPutValue("network.softnet.percpu.backlog_length", cpuno, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_fchost_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	int i;
	struct stats_fchost *sfcc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_rxframes);
		pmiPutValue("fchost.in.frames", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_txframes);
		pmiPutValue("fchost.out.frames", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_rxwords * 4);
		pmiPutValue("fchost.in.bytes", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_txwords * 4);
		pmiPutValue("fchost.out.bytes", sfcc->fchost_name, buf);
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
 ***************************************************************************
 */
__print_funct_t pcp_print_psicpu_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_10 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_60 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_300 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_cpu_total);
	pmiPutValue("kernel.all.pressure.cpu.some.total", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psiio_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_10 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_60 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_300 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_io_total);
	pmiPutValue("kernel.all.pressure.io.some.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_10 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_60 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_300 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_io_total);
	pmiPutValue("kernel.all.pressure.io.full.total", NULL, buf);
#endif	/* HAVE_PCP */
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psimem_stats(struct activity *a, int curr)
{
#ifdef HAVE_PCP
	char buf[64];
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_10 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_60 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_300 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_mem_total);
	pmiPutValue("kernel.all.pressure.memory.some.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_10 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_60 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_300 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_mem_total);
	pmiPutValue("kernel.all.pressure.memory.full.total", NULL, buf);
#endif	/* HAVE_PCP */
}
