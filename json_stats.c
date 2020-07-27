/*
 * json_stats.c: Funtions used by sadf to display statistics in JSON format.
 * (C) 1999-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "sa.h"
#include "ioconf.h"
#include "json_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;

/*
 ***************************************************************************
 * Open or close "network" markup.
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Open or close action.
 ***************************************************************************
 */
void json_markup_network(int tab, int action)
{
	static int markup_state = CLOSE_JSON_MARKUP;

	if (action == markup_state)
		return;
	markup_state = action;

	if (action == OPEN_JSON_MARKUP) {
		/* Open markup */
		xprintf(tab, "\"network\": {");
	}
	else {
		/* Close markup */
		printf("\n");
		xprintf0(tab, "}");
	}
}

/*
 ***************************************************************************
 * Open or close "power-management" markup.
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Open or close action.
 ***************************************************************************
 */
void json_markup_power_management(int tab, int action)
{
	static int markup_state = CLOSE_JSON_MARKUP;

	if (action == markup_state)
		return;
	markup_state = action;

	if (action == OPEN_JSON_MARKUP) {
		/* Open markup */
		xprintf(tab, "\"power-management\": {");
	}
	else {
		/* Close markup */
		printf("\n");
		xprintf0(tab, "}");
	}
}

/*
 ***************************************************************************
 * Open or close "psi" markup.
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Open or close action.
 ***************************************************************************
 */
void json_markup_psi(int tab, int action)
{
	static int markup_state = CLOSE_JSON_MARKUP;

	if (action == markup_state)
		return;
	markup_state = action;

	if (action == OPEN_JSON_MARKUP) {
		/* Open markup */
		xprintf(tab, "\"psi\": {");
	}
	else {
		/* Close markup */
		printf("\n");
		xprintf0(tab, "}");
	}
}

/*
 ***************************************************************************
 * Display CPU statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second (independent of the
 *		number of processors). Unused here.
 ***************************************************************************
 */
__print_funct_t json_print_cpu_stats(struct activity *a, int curr, int tab,
				     unsigned long long itv)
{
	int i;
	int sep = FALSE;
	unsigned long long deltot_jiffies = 1;
	struct stats_cpu *scc, *scp;
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	char cpuno[16];

	xprintf(tab++, "\"cpu-load\": [");

	/* @nr[curr] cannot normally be greater than @nr_ini */
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

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		if (!i) {
			/* This is CPU "all" */
			strcpy(cpuno, "all");

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
			sprintf(cpuno, "%d", i - 1);

			/*
			 * Recalculate interval for current proc.
			 * If result is 0 then current CPU is a tickless one.
			 */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/* Current CPU is tickless */
				if (DISPLAY_CPU_DEF(a->opt_flags)) {
					xprintf0(tab, "{\"cpu\": \"%d\", "
						 "\"user\": %.2f, "
						 "\"nice\": %.2f, "
						 "\"system\": %.2f, "
						 "\"iowait\": %.2f, "
						 "\"steal\": %.2f, "
						 "\"idle\": %.2f}",
						 i - 1, 0.0, 0.0, 0.0, 0.0, 0.0, 100.0);
				}
				else if (DISPLAY_CPU_ALL(a->opt_flags)) {
					xprintf0(tab, "{\"cpu\": \"%d\", "
						 "\"usr\": %.2f, "
						 "\"nice\": %.2f, "
						 "\"sys\": %.2f, "
						 "\"iowait\": %.2f, "
						 "\"steal\": %.2f, "
						 "\"irq\": %.2f, "
						 "\"soft\": %.2f, "
						 "\"guest\": %.2f, "
						 "\"gnice\": %.2f, "
						 "\"idle\": %.2f}",
						 i - 1, 0.0, 0.0, 0.0, 0.0,
						 0.0, 0.0, 0.0, 0.0, 0.0, 100.0);
				}
				continue;
			}
		}

		if (DISPLAY_CPU_DEF(a->opt_flags)) {
			xprintf0(tab, "{\"cpu\": \"%s\", "
				 "\"user\": %.2f, "
				 "\"nice\": %.2f, "
				 "\"system\": %.2f, "
				 "\"iowait\": %.2f, "
				 "\"steal\": %.2f, "
				 "\"idle\": %.2f}",
				 cpuno,
				 ll_sp_value(scp->cpu_user, scc->cpu_user, deltot_jiffies),
				 ll_sp_value(scp->cpu_nice, scc->cpu_nice, deltot_jiffies),
				 ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
					     scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
					     deltot_jiffies),
				 ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies),
				 ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies),
				 scc->cpu_idle < scp->cpu_idle ?
				 0.0 :
				 ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies));
		}
		else if (DISPLAY_CPU_ALL(a->opt_flags)) {
			xprintf0(tab, "{\"cpu\": \"%s\", "
				 "\"usr\": %.2f, "
				 "\"nice\": %.2f, "
				 "\"sys\": %.2f, "
				 "\"iowait\": %.2f, "
				 "\"steal\": %.2f, "
				 "\"irq\": %.2f, "
				 "\"soft\": %.2f, "
				 "\"guest\": %.2f, "
				 "\"gnice\": %.2f, "
				 "\"idle\": %.2f}",
				 cpuno,
				 (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
				 0.0 :
				 ll_sp_value(scp->cpu_user - scp->cpu_guest,
					     scc->cpu_user - scc->cpu_guest, deltot_jiffies),
				 (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
				 0.0 :
				 ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
					     scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies),
				 ll_sp_value(scp->cpu_sys, scc->cpu_sys, deltot_jiffies),
				 ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies),
				 ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies),
				 ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, deltot_jiffies),
				 ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, deltot_jiffies),
				 ll_sp_value(scp->cpu_guest, scc->cpu_guest, deltot_jiffies),
				 ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies),
				 scc->cpu_idle < scp->cpu_idle ?
				 0.0 :
				 ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies));
		}
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display task creation and context switch statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pcsw_stats(struct activity *a, int curr, int tab,
				      unsigned long long itv)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];

	/* proc/s and cswch/s */
	xprintf0(tab, "\"process-and-context-switch\": {"
		 "\"proc\": %.2f, "
		 "\"cswch\": %.2f}",
		 S_VALUE(spp->processes, spc->processes, itv),
		 S_VALUE(spp->context_switch, spc->context_switch, itv));
}

/*
 ***************************************************************************
 * Display interrupts statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_irq_stats(struct activity *a, int curr, int tab,
				     unsigned long long itv)
{
	int i;
	struct stats_irq *sic, *sip;
	int sep = FALSE;
	char irqno[16];

	xprintf(tab++, "\"interrupts\": [");

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		sic = (struct stats_irq *) ((char *) a->buf[curr]  + i * a->msize);
		sip = (struct stats_irq *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			/* Yes: Display it */

			if (sep) {
				printf(",\n");
			}
			sep = TRUE;

			if (!i) {
				/* This is interrupt "sum" */
				strcpy(irqno, "sum");
			}
			else {
				sprintf(irqno, "%d", i - 1);
			}

			xprintf0(tab, "{\"intr\": \"%s\", "
				 "\"value\": %.2f}",
				 irqno,
				 S_VALUE(sip->irq_nr, sic->irq_nr, itv));
		}
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display swapping statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_swap_stats(struct activity *a, int curr, int tab,
				      unsigned long long itv)
{
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[!curr];

	xprintf0(tab, "\"swap-pages\": {"
		 "\"pswpin\": %.2f, "
		 "\"pswpout\": %.2f}",
		 S_VALUE(ssp->pswpin,  ssc->pswpin,  itv),
		 S_VALUE(ssp->pswpout, ssc->pswpout, itv));
}

/*
 ***************************************************************************
 * Display paging statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_paging_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];

	xprintf0(tab, "\"paging\": {"
		 "\"pgpgin\": %.2f, "
		 "\"pgpgout\": %.2f, "
		 "\"fault\": %.2f, "
		 "\"majflt\": %.2f, "
		 "\"pgfree\": %.2f, "
		 "\"pgscank\": %.2f, "
		 "\"pgscand\": %.2f, "
		 "\"pgsteal\": %.2f, "
		 "\"vmeff-percent\": %.2f}",
		 S_VALUE(spp->pgpgin,        spc->pgpgin,        itv),
		 S_VALUE(spp->pgpgout,       spc->pgpgout,       itv),
		 S_VALUE(spp->pgfault,       spc->pgfault,       itv),
		 S_VALUE(spp->pgmajfault,    spc->pgmajfault,    itv),
		 S_VALUE(spp->pgfree,        spc->pgfree,        itv),
		 S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv),
		 S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv),
		 S_VALUE(spp->pgsteal,       spc->pgsteal,       itv),
		 (spc->pgscan_kswapd + spc->pgscan_direct -
		  spp->pgscan_kswapd - spp->pgscan_direct) ?
		 SP_VALUE(spp->pgsteal, spc->pgsteal,
			  spc->pgscan_kswapd + spc->pgscan_direct -
			  spp->pgscan_kswapd - spp->pgscan_direct) : 0.0);
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_io_stats(struct activity *a, int curr, int tab,
				    unsigned long long itv)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[!curr];

	xprintf0(tab, "\"io\": {"
		 "\"tps\": %.2f, "
		 "\"io-reads\": {"
		 "\"rtps\": %.2f, "
		 "\"bread\": %.2f}, "
		 "\"io-writes\": {"
		 "\"wtps\": %.2f, "
		 "\"bwrtn\": %.2f}, "
		 "\"io-discard\": {"
		 "\"dtps\": %.2f, "
		 "\"bdscd\": %.2f}}",
		 /*
		  * If we get negative values, this is probably because
		  * one or more devices/filesystems have been unmounted.
		  * We display 0.0 in this case though we should rather tell
		  * the user that the value cannot be calculated here.
		  */
		 sic->dk_drive < sip->dk_drive ? 0.0 :
		 S_VALUE(sip->dk_drive, sic->dk_drive, itv),
		 sic->dk_drive_rio < sip->dk_drive_rio ? 0.0 :
		 S_VALUE(sip->dk_drive_rio, sic->dk_drive_rio, itv),
		 sic->dk_drive_rblk < sip->dk_drive_rblk ? 0.0 :
		 S_VALUE(sip->dk_drive_rblk, sic->dk_drive_rblk, itv),
		 sic->dk_drive_wio < sip->dk_drive_wio ? 0.0 :
		 S_VALUE(sip->dk_drive_wio, sic->dk_drive_wio, itv),
		 sic->dk_drive_wblk < sip->dk_drive_wblk ? 0.0 :
		 S_VALUE(sip->dk_drive_wblk, sic->dk_drive_wblk, itv),
		 sic->dk_drive_dio < sip->dk_drive_dio ? 0.0 :
		 S_VALUE(sip->dk_drive_dio, sic->dk_drive_dio, itv),
		 sic->dk_drive_dblk < sip->dk_drive_dblk ? 0.0 :
		 S_VALUE(sip->dk_drive_dblk, sic->dk_drive_dblk, itv));
}

/*
 ***************************************************************************
 * Display memory statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_memory_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];
	int sep = FALSE;
	unsigned long long nousedmem;

	xprintf0(tab, "\"memory\": {");

	if (DISPLAY_MEMORY(a->opt_flags)) {

		sep = TRUE;

		nousedmem = smc->frmkb + smc->bufkb + smc->camkb + smc->slabkb;
		if (nousedmem > smc->tlmkb) {
			nousedmem = smc->tlmkb;
		}

		printf("\"memfree\": %llu, "
		       "\"avail\": %llu, "
		       "\"memused\": %llu, "
		       "\"memused-percent\": %.2f, "
		       "\"buffers\": %llu, "
		       "\"cached\": %llu, "
		       "\"commit\": %llu, "
		       "\"commit-percent\": %.2f, "
		       "\"active\": %llu, "
		       "\"inactive\": %llu, "
		       "\"dirty\": %llu",
		       smc->frmkb,
		       smc->availablekb,
		       smc->tlmkb - nousedmem,
		       smc->tlmkb ?
		       SP_VALUE(nousedmem, smc->tlmkb, smc->tlmkb) :
		       0.0,
		       smc->bufkb,
		       smc->camkb,
		       smc->comkb,
		       (smc->tlmkb + smc->tlskb) ?
		       SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb) :
		       0.0,
		       smc->activekb,
		       smc->inactkb,
		       smc->dirtykb);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {
			/* Display extended memory stats */
			printf(", \"anonpg\": %llu, "
			       "\"slab\": %llu, "
			       "\"kstack\": %llu, "
			       "\"pgtbl\": %llu, "
			       "\"vmused\": %llu",
			       smc->anonpgkb,
			       smc->slabkb,
			       smc->kstackkb,
			       smc->pgtblkb,
			       smc->vmusedkb);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		if (sep) {
			printf(", ");
		}
		sep = TRUE;

		printf("\"swpfree\": %llu, "
		       "\"swpused\": %llu, "
		       "\"swpused-percent\": %.2f, "
		       "\"swpcad\": %llu, "
		       "\"swpcad-percent\": %.2f",
		       smc->frskb,
		       smc->tlskb - smc->frskb,
		       smc->tlskb ?
		       SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb) :
		       0.0,
		       smc->caskb,
		       (smc->tlskb - smc->frskb) ?
		       SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb) :
		       0.0);
	}

	printf("}");
}

/*
 ***************************************************************************
 * Display kernel tables statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_ktables_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	xprintf0(tab, "\"kernel\": {"
		 "\"dentunusd\": %llu, "
		 "\"file-nr\": %llu, "
		 "\"inode-nr\": %llu, "
		 "\"pty-nr\": %llu}",
		 skc->dentry_stat,
		 skc->file_used,
		 skc->inode_used,
		 skc->pty_nr);
}

/*
 ***************************************************************************
 * Display queue and load statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_queue_stats(struct activity *a, int curr, int tab,
				       unsigned long long itv)
{
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	xprintf0(tab, "\"queue\": {"
		 "\"runq-sz\": %llu, "
		 "\"plist-sz\": %llu, "
		 "\"ldavg-1\": %.2f, "
		 "\"ldavg-5\": %.2f, "
		 "\"ldavg-15\": %.2f, "
		 "\"blocked\": %llu}",
		 sqc->nr_running,
		 sqc->nr_threads,
		 (double) sqc->load_avg_1 / 100,
		 (double) sqc->load_avg_5 / 100,
		 (double) sqc->load_avg_15 / 100,
		 sqc->procs_blocked);
}

/*
 ***************************************************************************
 * Display serial lines statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_serial_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_serial *ssc, *ssp;
	int sep = FALSE;

	xprintf(tab++, "\"serial\": [");

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

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"line\": %d, "
			 "\"rcvin\": %.2f, "
			 "\"xmtin\": %.2f, "
			 "\"framerr\": %.2f, "
			 "\"prtyerr\": %.2f, "
			 "\"brk\": %.2f, "
			 "\"ovrun\": %.2f}",
			 ssc->line,
			 S_VALUE(ssp->rx,      ssc->rx,      itv),
			 S_VALUE(ssp->tx,      ssc->tx,      itv),
			 S_VALUE(ssp->frame,   ssc->frame,   itv),
			 S_VALUE(ssp->parity,  ssc->parity,  itv),
			 S_VALUE(ssp->brk,     ssc->brk,     itv),
			 S_VALUE(ssp->overrun, ssc->overrun, itv));
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display disks statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_disk_stats(struct activity *a, int curr, int tab,
				      unsigned long long itv)
{
	int i, j;
	struct stats_disk *sdc,	*sdp, sdpzero;
	struct ext_disk_stats xds;
	int sep = FALSE;
	char *dev_name;

	memset(&sdpzero, 0, STATS_DISK_SIZE);

	xprintf(tab++, "\"disk\": [");

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

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"disk-device\": \"%s\", "
			 "\"tps\": %.2f, "
			 "\"rd_sec\": %.2f, "
			 "\"wr_sec\": %.2f, "
			 "\"dc_sec\": %.2f, "
			 "\"rkB\": %.2f, "
			 "\"wkB\": %.2f, "
			 "\"dkB\": %.2f, "
			 "\"avgrq-sz\": %.2f, "
			 "\"areq-sz\": %.2f, "
			 "\"avgqu-sz\": %.2f, "
			 "\"aqu-sz\": %.2f, "
			 "\"await\": %.2f, "
			 "\"util-percent\": %.2f}",
			 /* Confusion possible here between index and minor numbers */
			 dev_name,
			 S_VALUE(sdp->nr_ios, sdc->nr_ios, itv),
			 S_VALUE(sdp->rd_sect, sdc->rd_sect, itv), /* Unit = sectors (for backward compatibility) */
			 S_VALUE(sdp->wr_sect, sdc->wr_sect, itv),
			 S_VALUE(sdp->dc_sect, sdc->dc_sect, itv),
			 S_VALUE(sdp->rd_sect, sdc->rd_sect, itv) / 2,
			 S_VALUE(sdp->wr_sect, sdc->wr_sect, itv) / 2,
			 S_VALUE(sdp->dc_sect, sdc->dc_sect, itv) / 2,
			 /* See iostat for explanations */
			 xds.arqsz,	/* Unit = sectors (for backward compatibility) */
			 xds.arqsz / 2,
			 S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0,	/* For backward compatibility */
			 S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0,
			 xds.await,
			 xds.util / 10.0);
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display network interfaces statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_dev_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	int i, j;
	struct stats_net_dev *sndc, *sndp, sndzero;
	int sep = FALSE;
	double rxkb, txkb, ifutil;

	memset(&sndzero, 0, STATS_NET_DEV_SIZE);

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"net-dev\": [");

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

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		rxkb = S_VALUE(sndp->rx_bytes, sndc->rx_bytes, itv);
		txkb = S_VALUE(sndp->tx_bytes, sndc->tx_bytes, itv);
		ifutil = compute_ifutil(sndc, rxkb, txkb);

		xprintf0(tab, "{\"iface\": \"%s\", "
			 "\"rxpck\": %.2f, "
			 "\"txpck\": %.2f, "
			 "\"rxkB\": %.2f, "
			 "\"txkB\": %.2f, "
			 "\"rxcmp\": %.2f, "
			 "\"txcmp\": %.2f, "
			 "\"rxmcst\": %.2f, "
			 "\"ifutil-percent\": %.2f}",
			 sndc->interface,
			 S_VALUE(sndp->rx_packets,    sndc->rx_packets,    itv),
			 S_VALUE(sndp->tx_packets,    sndc->tx_packets,    itv),
			 rxkb / 1024,
			 txkb / 1024,
			 S_VALUE(sndp->rx_compressed, sndc->rx_compressed, itv),
			 S_VALUE(sndp->tx_compressed, sndc->tx_compressed, itv),
			 S_VALUE(sndp->multicast,     sndc->multicast,     itv),
			 ifutil);
	}

	printf("\n");
	xprintf0(--tab, "]");

	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display network interfaces errors statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_edev_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	int i, j;
	struct stats_net_edev *snedc, *snedp, snedzero;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	memset(&snedzero, 0, STATS_NET_EDEV_SIZE);

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"net-edev\": [");

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

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"iface\": \"%s\", "
			 "\"rxerr\": %.2f, "
			 "\"txerr\": %.2f, "
			 "\"coll\": %.2f, "
			 "\"rxdrop\": %.2f, "
			 "\"txdrop\": %.2f, "
			 "\"txcarr\": %.2f, "
			 "\"rxfram\": %.2f, "
			 "\"rxfifo\": %.2f, "
			 "\"txfifo\": %.2f}",
			 snedc->interface,
			 S_VALUE(snedp->rx_errors,         snedc->rx_errors,         itv),
			 S_VALUE(snedp->tx_errors,         snedc->tx_errors,         itv),
			 S_VALUE(snedp->collisions,        snedc->collisions,        itv),
			 S_VALUE(snedp->rx_dropped,        snedc->rx_dropped,        itv),
			 S_VALUE(snedp->tx_dropped,        snedc->tx_dropped,        itv),
			 S_VALUE(snedp->tx_carrier_errors, snedc->tx_carrier_errors, itv),
			 S_VALUE(snedp->rx_frame_errors,   snedc->rx_frame_errors,   itv),
			 S_VALUE(snedp->rx_fifo_errors,    snedc->rx_fifo_errors,    itv),
			 S_VALUE(snedp->tx_fifo_errors,    snedc->tx_fifo_errors,    itv));
	}

	printf("\n");
	xprintf0(--tab, "]");

	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display NFS client statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_nfs_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-nfs\": {"
		 "\"call\": %.2f, "
		 "\"retrans\": %.2f, "
		 "\"read\": %.2f, "
		 "\"write\": %.2f, "
		 "\"access\": %.2f, "
		 "\"getatt\": %.2f}",
		 S_VALUE(snnp->nfs_rpccnt,     snnc->nfs_rpccnt,     itv),
		 S_VALUE(snnp->nfs_rpcretrans, snnc->nfs_rpcretrans, itv),
		 S_VALUE(snnp->nfs_readcnt,    snnc->nfs_readcnt,    itv),
		 S_VALUE(snnp->nfs_writecnt,   snnc->nfs_writecnt,   itv),
		 S_VALUE(snnp->nfs_accesscnt,  snnc->nfs_accesscnt,  itv),
		 S_VALUE(snnp->nfs_getattcnt,  snnc->nfs_getattcnt,  itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display NFS server statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_nfsd_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr],
		*snndp = (struct stats_net_nfsd *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-nfsd\": {"
		 "\"scall\": %.2f, "
		 "\"badcall\": %.2f, "
		 "\"packet\": %.2f, "
		 "\"udp\": %.2f, "
		 "\"tcp\": %.2f, "
		 "\"hit\": %.2f, "
		 "\"miss\": %.2f, "
		 "\"sread\": %.2f, "
		 "\"swrite\": %.2f, "
		 "\"saccess\": %.2f, "
		 "\"sgetatt\": %.2f}",
		 S_VALUE(snndp->nfsd_rpccnt,    snndc->nfsd_rpccnt,    itv),
		 S_VALUE(snndp->nfsd_rpcbad,    snndc->nfsd_rpcbad,    itv),
		 S_VALUE(snndp->nfsd_netcnt,    snndc->nfsd_netcnt,    itv),
		 S_VALUE(snndp->nfsd_netudpcnt, snndc->nfsd_netudpcnt, itv),
		 S_VALUE(snndp->nfsd_nettcpcnt, snndc->nfsd_nettcpcnt, itv),
		 S_VALUE(snndp->nfsd_rchits,    snndc->nfsd_rchits,    itv),
		 S_VALUE(snndp->nfsd_rcmisses,  snndc->nfsd_rcmisses,  itv),
		 S_VALUE(snndp->nfsd_readcnt,   snndc->nfsd_readcnt,   itv),
		 S_VALUE(snndp->nfsd_writecnt,  snndc->nfsd_writecnt,  itv),
		 S_VALUE(snndp->nfsd_accesscnt, snndc->nfsd_accesscnt, itv),
		 S_VALUE(snndp->nfsd_getattcnt, snndc->nfsd_getattcnt, itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display network socket statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_sock_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-sock\": {"
		 "\"totsck\": %u, "
		 "\"tcpsck\": %u, "
		 "\"udpsck\": %u, "
		 "\"rawsck\": %u, "
		 "\"ip-frag\": %u, "
		 "\"tcp-tw\": %u}",
		 snsc->sock_inuse,
		 snsc->tcp_inuse,
		 snsc->udp_inuse,
		 snsc->raw_inuse,
		 snsc->frag_inuse,
		 snsc->tcp_tw);
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display IP network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_ip_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-ip\": {"
		 "\"irec\": %.2f, "
		 "\"fwddgm\": %.2f, "
		 "\"idel\": %.2f, "
		 "\"orq\": %.2f, "
		 "\"asmrq\": %.2f, "
		 "\"asmok\": %.2f, "
		 "\"fragok\": %.2f, "
		 "\"fragcrt\": %.2f}",
		 S_VALUE(snip->InReceives,    snic->InReceives,    itv),
		 S_VALUE(snip->ForwDatagrams, snic->ForwDatagrams, itv),
		 S_VALUE(snip->InDelivers,    snic->InDelivers,    itv),
		 S_VALUE(snip->OutRequests,   snic->OutRequests,   itv),
		 S_VALUE(snip->ReasmReqds,    snic->ReasmReqds,    itv),
		 S_VALUE(snip->ReasmOKs,      snic->ReasmOKs,      itv),
		 S_VALUE(snip->FragOKs,       snic->FragOKs,       itv),
		 S_VALUE(snip->FragCreates,   snic->FragCreates,   itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display IP network errors statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_eip_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-eip\": {"
		 "\"ihdrerr\": %.2f, "
		 "\"iadrerr\": %.2f, "
		 "\"iukwnpr\": %.2f, "
		 "\"idisc\": %.2f, "
		 "\"odisc\": %.2f, "
		 "\"onort\": %.2f, "
		 "\"asmf\": %.2f, "
		 "\"fragf\": %.2f}",
		 S_VALUE(sneip->InHdrErrors,     sneic->InHdrErrors,     itv),
		 S_VALUE(sneip->InAddrErrors,    sneic->InAddrErrors,    itv),
		 S_VALUE(sneip->InUnknownProtos, sneic->InUnknownProtos, itv),
		 S_VALUE(sneip->InDiscards,      sneic->InDiscards,      itv),
		 S_VALUE(sneip->OutDiscards,     sneic->OutDiscards,     itv),
		 S_VALUE(sneip->OutNoRoutes,     sneic->OutNoRoutes,     itv),
		 S_VALUE(sneip->ReasmFails,      sneic->ReasmFails,      itv),
		 S_VALUE(sneip->FragFails,       sneic->FragFails,       itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display ICMP network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_icmp_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-icmp\": {"
		 "\"imsg\": %.2f, "
		 "\"omsg\": %.2f, "
		 "\"iech\": %.2f, "
		 "\"iechr\": %.2f, "
		 "\"oech\": %.2f, "
		 "\"oechr\": %.2f, "
		 "\"itm\": %.2f, "
		 "\"itmr\": %.2f, "
		 "\"otm\": %.2f, "
		 "\"otmr\": %.2f, "
		 "\"iadrmk\": %.2f, "
		 "\"iadrmkr\": %.2f, "
		 "\"oadrmk\": %.2f, "
		 "\"oadrmkr\": %.2f}",
		 S_VALUE(snip->InMsgs,           snic->InMsgs,           itv),
		 S_VALUE(snip->OutMsgs,          snic->OutMsgs,          itv),
		 S_VALUE(snip->InEchos,          snic->InEchos,          itv),
		 S_VALUE(snip->InEchoReps,       snic->InEchoReps,       itv),
		 S_VALUE(snip->OutEchos,         snic->OutEchos,         itv),
		 S_VALUE(snip->OutEchoReps,      snic->OutEchoReps,      itv),
		 S_VALUE(snip->InTimestamps,     snic->InTimestamps,     itv),
		 S_VALUE(snip->InTimestampReps,  snic->InTimestampReps,  itv),
		 S_VALUE(snip->OutTimestamps,    snic->OutTimestamps,    itv),
		 S_VALUE(snip->OutTimestampReps, snic->OutTimestampReps, itv),
		 S_VALUE(snip->InAddrMasks,      snic->InAddrMasks,      itv),
		 S_VALUE(snip->InAddrMaskReps,   snic->InAddrMaskReps,   itv),
		 S_VALUE(snip->OutAddrMasks,     snic->OutAddrMasks,     itv),
		 S_VALUE(snip->OutAddrMaskReps,  snic->OutAddrMaskReps,  itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display ICMP errors message statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_eicmp_stats(struct activity *a, int curr, int tab,
					   unsigned long long itv)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-eicmp\": {"
		 "\"ierr\": %.2f, "
		 "\"oerr\": %.2f, "
		 "\"idstunr\": %.2f, "
		 "\"odstunr\": %.2f, "
		 "\"itmex\": %.2f, "
		 "\"otmex\": %.2f, "
		 "\"iparmpb\": %.2f, "
		 "\"oparmpb\": %.2f, "
		 "\"isrcq\": %.2f, "
		 "\"osrcq\": %.2f, "
		 "\"iredir\": %.2f, "
		 "\"oredir\": %.2f}",
		 S_VALUE(sneip->InErrors,        sneic->InErrors,        itv),
		 S_VALUE(sneip->OutErrors,       sneic->OutErrors,       itv),
		 S_VALUE(sneip->InDestUnreachs,  sneic->InDestUnreachs,  itv),
		 S_VALUE(sneip->OutDestUnreachs, sneic->OutDestUnreachs, itv),
		 S_VALUE(sneip->InTimeExcds,     sneic->InTimeExcds,     itv),
		 S_VALUE(sneip->OutTimeExcds,    sneic->OutTimeExcds,    itv),
		 S_VALUE(sneip->InParmProbs,     sneic->InParmProbs,     itv),
		 S_VALUE(sneip->OutParmProbs,    sneic->OutParmProbs,    itv),
		 S_VALUE(sneip->InSrcQuenchs,    sneic->InSrcQuenchs,    itv),
		 S_VALUE(sneip->OutSrcQuenchs,   sneic->OutSrcQuenchs,   itv),
		 S_VALUE(sneip->InRedirects,     sneic->InRedirects,     itv),
		 S_VALUE(sneip->OutRedirects,    sneic->OutRedirects,    itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display TCP network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_tcp_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-tcp\": {"
		 "\"active\": %.2f, "
		 "\"passive\": %.2f, "
		 "\"iseg\": %.2f, "
		 "\"oseg\": %.2f}",
		 S_VALUE(sntp->ActiveOpens,  sntc->ActiveOpens,  itv),
		 S_VALUE(sntp->PassiveOpens, sntc->PassiveOpens, itv),
		 S_VALUE(sntp->InSegs,       sntc->InSegs,       itv),
		 S_VALUE(sntp->OutSegs,      sntc->OutSegs,      itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display TCP network errors statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in XML output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_etcp_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-etcp\": {"
		 "\"atmptf\": %.2f, "
		 "\"estres\": %.2f, "
		 "\"retrans\": %.2f, "
		 "\"isegerr\": %.2f, "
		 "\"orsts\": %.2f}",
		 S_VALUE(snetp->AttemptFails, snetc->AttemptFails,  itv),
		 S_VALUE(snetp->EstabResets,  snetc->EstabResets,  itv),
		 S_VALUE(snetp->RetransSegs,  snetc->RetransSegs,  itv),
		 S_VALUE(snetp->InErrs,       snetc->InErrs,  itv),
		 S_VALUE(snetp->OutRsts,      snetc->OutRsts,  itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display UDP network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_udp_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-udp\": {"
		 "\"idgm\": %.2f, "
		 "\"odgm\": %.2f, "
		 "\"noport\": %.2f, "
		 "\"idgmerr\": %.2f}",
		 S_VALUE(snup->InDatagrams,  snuc->InDatagrams,  itv),
		 S_VALUE(snup->OutDatagrams, snuc->OutDatagrams, itv),
		 S_VALUE(snup->NoPorts,      snuc->NoPorts,      itv),
		 S_VALUE(snup->InErrors,     snuc->InErrors,     itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display IPv6 network socket statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_sock6_stats(struct activity *a, int curr, int tab,
					   unsigned long long itv)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-sock6\": {"
		 "\"tcp6sck\": %u, "
		 "\"udp6sck\": %u, "
		 "\"raw6sck\": %u, "
		 "\"ip6-frag\": %u}",
		 snsc->tcp6_inuse,
		 snsc->udp6_inuse,
		 snsc->raw6_inuse,
		 snsc->frag6_inuse);
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_ip6_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-ip6\": {"
		 "\"irec6\": %.2f, "
		 "\"fwddgm6\": %.2f, "
		 "\"idel6\": %.2f, "
		 "\"orq6\": %.2f, "
		 "\"asmrq6\": %.2f, "
		 "\"asmok6\": %.2f, "
		 "\"imcpck6\": %.2f, "
		 "\"omcpck6\": %.2f, "
		 "\"fragok6\": %.2f, "
		 "\"fragcr6\": %.2f}",
		 S_VALUE(snip->InReceives6,       snic->InReceives6,       itv),
		 S_VALUE(snip->OutForwDatagrams6, snic->OutForwDatagrams6, itv),
		 S_VALUE(snip->InDelivers6,       snic->InDelivers6,       itv),
		 S_VALUE(snip->OutRequests6,      snic->OutRequests6,      itv),
		 S_VALUE(snip->ReasmReqds6,       snic->ReasmReqds6,       itv),
		 S_VALUE(snip->ReasmOKs6,         snic->ReasmOKs6,         itv),
		 S_VALUE(snip->InMcastPkts6,      snic->InMcastPkts6,      itv),
		 S_VALUE(snip->OutMcastPkts6,     snic->OutMcastPkts6,     itv),
		 S_VALUE(snip->FragOKs6,          snic->FragOKs6,          itv),
		 S_VALUE(snip->FragCreates6,      snic->FragCreates6,      itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_eip6_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-eip6\": {"
		 "\"ihdrer6\": %.2f, "
		 "\"iadrer6\": %.2f, "
		 "\"iukwnp6\": %.2f, "
		 "\"i2big6\": %.2f, "
		 "\"idisc6\": %.2f, "
		 "\"odisc6\": %.2f, "
		 "\"inort6\": %.2f, "
		 "\"onort6\": %.2f, "
		 "\"asmf6\": %.2f, "
		 "\"fragf6\": %.2f, "
		 "\"itrpck6\": %.2f}",
		 S_VALUE(sneip->InHdrErrors6,     sneic->InHdrErrors6,     itv),
		 S_VALUE(sneip->InAddrErrors6,    sneic->InAddrErrors6,    itv),
		 S_VALUE(sneip->InUnknownProtos6, sneic->InUnknownProtos6, itv),
		 S_VALUE(sneip->InTooBigErrors6,  sneic->InTooBigErrors6,  itv),
		 S_VALUE(sneip->InDiscards6,      sneic->InDiscards6,      itv),
		 S_VALUE(sneip->OutDiscards6,     sneic->OutDiscards6,     itv),
		 S_VALUE(sneip->InNoRoutes6,      sneic->InNoRoutes6,      itv),
		 S_VALUE(sneip->OutNoRoutes6,     sneic->OutNoRoutes6,     itv),
		 S_VALUE(sneip->ReasmFails6,      sneic->ReasmFails6,      itv),
		 S_VALUE(sneip->FragFails6,       sneic->FragFails6,       itv),
		 S_VALUE(sneip->InTruncatedPkts6, sneic->InTruncatedPkts6, itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_icmp6_stats(struct activity *a, int curr, int tab,
					   unsigned long long itv)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-icmp6\": {"
		 "\"imsg6\": %.2f, "
		 "\"omsg6\": %.2f, "
		 "\"iech6\": %.2f, "
		 "\"iechr6\": %.2f, "
		 "\"oechr6\": %.2f, "
		 "\"igmbq6\": %.2f, "
		 "\"igmbr6\": %.2f, "
		 "\"ogmbr6\": %.2f, "
		 "\"igmbrd6\": %.2f, "
		 "\"ogmbrd6\": %.2f, "
		 "\"irtsol6\": %.2f, "
		 "\"ortsol6\": %.2f, "
		 "\"irtad6\": %.2f, "
		 "\"inbsol6\": %.2f, "
		 "\"onbsol6\": %.2f, "
		 "\"inbad6\": %.2f, "
		 "\"onbad6\": %.2f}",
		 S_VALUE(snip->InMsgs6,                    snic->InMsgs6,                    itv),
		 S_VALUE(snip->OutMsgs6,                   snic->OutMsgs6,                   itv),
		 S_VALUE(snip->InEchos6,                   snic->InEchos6,                   itv),
		 S_VALUE(snip->InEchoReplies6,             snic->InEchoReplies6,             itv),
		 S_VALUE(snip->OutEchoReplies6,            snic->OutEchoReplies6,            itv),
		 S_VALUE(snip->InGroupMembQueries6,        snic->InGroupMembQueries6,        itv),
		 S_VALUE(snip->InGroupMembResponses6,      snic->InGroupMembResponses6,      itv),
		 S_VALUE(snip->OutGroupMembResponses6,     snic->OutGroupMembResponses6,     itv),
		 S_VALUE(snip->InGroupMembReductions6,     snic->InGroupMembReductions6,     itv),
		 S_VALUE(snip->OutGroupMembReductions6,    snic->OutGroupMembReductions6,    itv),
		 S_VALUE(snip->InRouterSolicits6,          snic->InRouterSolicits6,          itv),
		 S_VALUE(snip->OutRouterSolicits6,         snic->OutRouterSolicits6,         itv),
		 S_VALUE(snip->InRouterAdvertisements6,    snic->InRouterAdvertisements6,    itv),
		 S_VALUE(snip->InNeighborSolicits6,        snic->InNeighborSolicits6,        itv),
		 S_VALUE(snip->OutNeighborSolicits6,       snic->OutNeighborSolicits6,       itv),
		 S_VALUE(snip->InNeighborAdvertisements6,  snic->InNeighborAdvertisements6,  itv),
		 S_VALUE(snip->OutNeighborAdvertisements6, snic->OutNeighborAdvertisements6, itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display ICMPv6 error messages statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_eicmp6_stats(struct activity *a, int curr, int tab,
					    unsigned long long itv)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-eicmp6\": {"
		 "\"ierr6\": %.2f, "
		 "\"idtunr6\": %.2f, "
		 "\"odtunr6\": %.2f, "
		 "\"itmex6\": %.2f, "
		 "\"otmex6\": %.2f, "
		 "\"iprmpb6\": %.2f, "
		 "\"oprmpb6\": %.2f, "
		 "\"iredir6\": %.2f, "
		 "\"oredir6\": %.2f, "
		 "\"ipck2b6\": %.2f, "
		 "\"opck2b6\": %.2f}",
		 S_VALUE(sneip->InErrors6,        sneic->InErrors6,        itv),
		 S_VALUE(sneip->InDestUnreachs6,  sneic->InDestUnreachs6,  itv),
		 S_VALUE(sneip->OutDestUnreachs6, sneic->OutDestUnreachs6, itv),
		 S_VALUE(sneip->InTimeExcds6,     sneic->InTimeExcds6,     itv),
		 S_VALUE(sneip->OutTimeExcds6,    sneic->OutTimeExcds6,    itv),
		 S_VALUE(sneip->InParmProblems6,  sneic->InParmProblems6,  itv),
		 S_VALUE(sneip->OutParmProblems6, sneic->OutParmProblems6, itv),
		 S_VALUE(sneip->InRedirects6,     sneic->InRedirects6,     itv),
		 S_VALUE(sneip->OutRedirects6,    sneic->OutRedirects6,    itv),
		 S_VALUE(sneip->InPktTooBigs6,    sneic->InPktTooBigs6,    itv),
		 S_VALUE(sneip->OutPktTooBigs6,   sneic->OutPktTooBigs6,   itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display UDPv6 network statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_net_udp6_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[!curr];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"net-udp6\": {"
		 "\"idgm6\": %.2f, "
		 "\"odgm6\": %.2f, "
		 "\"noport6\": %.2f, "
		 "\"idgmer6\": %.2f}",
		 S_VALUE(snup->InDatagrams6,  snuc->InDatagrams6,  itv),
		 S_VALUE(snup->OutDatagrams6, snuc->OutDatagrams6, itv),
		 S_VALUE(snup->NoPorts6,      snuc->NoPorts6,      itv),
		 S_VALUE(snup->InErrors6,     snuc->InErrors6,     itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_cpufreq_stats(struct activity *a, int curr, int tab,
					     unsigned long long itv)
{
	int i;
	struct stats_pwr_cpufreq *spc;
	int sep = FALSE;
	char cpuno[16];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"cpu-frequency\": [");

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			strcpy(cpuno, "all");
		}
		else {
			sprintf(cpuno, "%d", i - 1);
		}

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"number\": \"%s\", "
			 "\"frequency\": %.2f}",
			 cpuno,
			 ((double) spc->cpufreq) / 100);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display fan statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_fan_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	int i;
	struct stats_pwr_fan *spc;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"fan-speed\": [");

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"number\": %d, "
			 "\"rpm\": %llu, "
			 "\"drpm\": %llu, "
			 "\"device\": \"%s\"}",
			 i + 1,
			 (unsigned long long) spc->rpm,
			 (unsigned long long) (spc->rpm - spc->rpm_min),
			 spc->device);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display temperature statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_temp_stats(struct activity *a, int curr, int tab,
					  unsigned long long itv)
{
	int i;
	struct stats_pwr_temp *spc;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"temperature\": [");

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"number\": %d, "
			 "\"degC\": %.2f, "
			 "\"percent-temp\": %.2f, "
			 "\"device\": \"%s\"}",
			 i + 1,
			 spc->temp,
			 (spc->temp_max - spc->temp_min) ?
			 (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			 0.0,
			 spc->device);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display voltage inputs statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_in_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	int i;
	struct stats_pwr_in *spc;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"voltage-input\": [");

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"number\": %d, "
			 "\"inV\": %.2f, "
			 "\"percent-in\": %.2f, "
			 "\"device\": \"%s\"}",
			 i,
			 spc->in,
			 (spc->in_max - spc->in_min) ?
			 (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			 0.0,
			 spc->device);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display huge pages statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_huge_stats(struct activity *a, int curr, int tab,
				      unsigned long long itv)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	xprintf0(tab, "\"hugepages\": {"
		 "\"hugfree\": %llu, "
		 "\"hugused\": %llu, "
		 "\"hugused-percent\": %.2f, "
		 "\"hugrsvd\": %llu, "
		 "\"hugsurp\": %llu}",
		 smc->frhkb,
		 smc->tlhkb - smc->frhkb,
		 smc->tlhkb ?
		 SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) : 0.0,
		 smc->rsvdhkb,
		 smc->surphkb);
}

/*
 ***************************************************************************
 * Display weighted CPU frequency statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_wghfreq_stats(struct activity *a, int curr, int tab,
					     unsigned long long itv)
{
	int i, k;
	struct stats_pwr_wghfreq *spc, *spp, *spc_k, *spp_k;
	unsigned long long tis, tisfreq;
	int sep = FALSE;
	char cpuno[16];

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"cpu-weighted-frequency\": [");

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_wghfreq *) ((char *) a->buf[curr]  + i * a->msize * a->nr2);
		spp = (struct stats_pwr_wghfreq *) ((char *) a->buf[!curr] + i * a->msize * a->nr2);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		tisfreq = 0;
		tis = 0;

		for (k = 0; k < a->nr2; k++) {

			spc_k = (struct stats_pwr_wghfreq *) ((char *) spc + k * a->msize);
			if (!spc_k->freq)
				break;
			spp_k = (struct stats_pwr_wghfreq *) ((char *) spp + k * a->msize);

			tisfreq += (spc_k->freq / 1000) *
			           (spc_k->time_in_state - spp_k->time_in_state);
			tis     += (spc_k->time_in_state - spp_k->time_in_state);
		}

		if (!i) {
			/* This is CPU "all" */
			strcpy(cpuno, "all");
		}
		else {
			sprintf(cpuno, "%d", i - 1);
		}

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"number\": \"%s\", "
			 "\"weighted-frequency\": %.2f}",
			 cpuno,
			 tis ? ((double) tisfreq) / tis : 0.0);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display USB devices statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_pwr_usb_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	int i;
	struct stats_pwr_usb *suc;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_power_management(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"usb-devices\": [");

	for (i = 0; i < a->nr[curr]; i++) {
		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"bus_number\": %d, "
			 "\"idvendor\": \"%x\", "
			 "\"idprod\": \"%x\", "
			 "\"maxpower\": %u, "
			 "\"manufact\": \"%s\", "
			 "\"product\": \"%s\"}",
			 suc->bus_nr,
			 suc->vendor_id,
			 suc->product_id,
			 suc->bmaxpower << 1,
			 suc->manufacturer,
			 suc->product);
	}

	printf("\n");
	xprintf0(--tab, "]");
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_power_management(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display filesystems statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_filesystem_stats(struct activity *a, int curr, int tab,
					    unsigned long long itv)
{
	int i;
	struct stats_filesystem *sfc;
	int sep = FALSE;

	xprintf(tab++, "\"filesystems\": [");

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list,
					      DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name))
				/* Device not found */
				continue;
		}

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		xprintf0(tab, "{\"%s\": \"%s\", "
			 "\"MBfsfree\": %.0f, "
			 "\"MBfsused\": %.0f, "
			 "\"%%fsused\": %.2f, "
			 "\"%%ufsused\": %.2f, "
			 "\"Ifree\": %llu, "
			 "\"Iused\": %llu, "
			 "\"%%Iused\": %.2f}",
			 DISPLAY_MOUNT(a->opt_flags) ? "mountpoint" : "filesystem",
			 DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name,
			 (double) sfc->f_bfree / 1024 / 1024,
			 (double) (sfc->f_blocks - sfc->f_bfree) / 1024 / 1024,
			 sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				     : 0.0,
			 sfc->f_blocks ? SP_VALUE(sfc->f_bavail, sfc->f_blocks, sfc->f_blocks)
				     : 0.0,
			 sfc->f_ffree,
			 sfc->f_files - sfc->f_ffree,
			 sfc->f_files ? SP_VALUE(sfc->f_ffree, sfc->f_files, sfc->f_files)
				    : 0.0);
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display Fibre Channel HBA statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_fchost_stats(struct activity *a, int curr, int tab,
					unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_fchost *sfcc, *sfcp, sfczero;
	int sep = FALSE;

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	memset(&sfczero, 0, sizeof(struct stats_fchost));

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"fchosts\": [");

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

		if (sep)
			printf(",\n");

		sep = TRUE;

		xprintf0(tab, "{\"fchost\": \"%s\", "
			 "\"fch_rxf\": %.2f, "
			 "\"fch_txf\": %.2f, "
			 "\"fch_rxw\": %.2f, "
			 "\"fch_txw\": %.2f}",
			 sfcc->fchost_name,
			 S_VALUE(sfcp->f_rxframes, sfcc->f_rxframes, itv),
			 S_VALUE(sfcp->f_txframes, sfcc->f_txframes, itv),
			 S_VALUE(sfcp->f_rxwords,  sfcc->f_rxwords,  itv),
			 S_VALUE(sfcp->f_txwords,  sfcc->f_txwords,  itv));
	}

	printf("\n");
	xprintf0(--tab, "]");

	tab --;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display softnet statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_softnet_stats(struct activity *a, int curr, int tab,
					 unsigned long long itv)
{
	int i;
	struct stats_softnet *ssnc, *ssnp;
	int sep = FALSE;
	char cpuno[16];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	if (!IS_SELECTED(a->options) || (a->nr[curr] <= 0))
		goto close_json_markup;

	json_markup_network(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf(tab++, "\"softnet\": [");

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Compute statistics for CPU "all" */
	get_global_soft_statistics(a, !curr, curr, flags, offline_cpu_bitmap);

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * Should current CPU (including CPU "all") be displayed?
		 * Note: a->nr is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* No */
			continue;

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_pwr_cpufreq) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * ssnc = (struct stats_softnet *) a->buf[...] + i;
                 */
                ssnc = (struct stats_softnet *) ((char *) a->buf[curr]  + i * a->msize);
                ssnp = (struct stats_softnet *) ((char *) a->buf[!curr] + i * a->msize);

		if (sep) {
			printf(",\n");
		}
		sep = TRUE;

		if (!i) {
			/* This is CPU "all" */
			strcpy(cpuno, "all");
		}
		else {
			sprintf(cpuno, "%d", i - 1);
		}

		xprintf0(tab, "{\"cpu\": \"%s\", "
			 "\"total\": %.2f, "
			 "\"dropd\": %.2f, "
			 "\"squeezd\": %.2f, "
			 "\"rx_rps\": %.2f, "
			 "\"flw_lim\": %.2f}",
			 cpuno,
			 S_VALUE(ssnp->processed,    ssnc->processed,    itv),
			 S_VALUE(ssnp->dropped,      ssnc->dropped,      itv),
			 S_VALUE(ssnp->time_squeeze, ssnc->time_squeeze, itv),
			 S_VALUE(ssnp->received_rps, ssnc->received_rps, itv),
			 S_VALUE(ssnp->flow_limit,   ssnc->flow_limit,   itv));
	}

	printf("\n");
	xprintf0(--tab, "]");

	tab --;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_network(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_psicpu_stats(struct activity *a, int curr, int tab,
				        unsigned long long itv)
{
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr],
		*psip = (struct stats_psi_cpu *) a->buf[!curr];

	if (!IS_SELECTED(a->options))
		goto close_json_markup;

	json_markup_psi(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"psi-cpu\": {"
		 "\"some_avg10\": %.2f, "
		 "\"some_avg60\": %.2f, "
		 "\"some_avg300\": %.2f, "
		 "\"some_avg\": %.2f}",
		 (double) psic->some_acpu_10  / 100,
		 (double) psic->some_acpu_60  / 100,
		 (double) psic->some_acpu_300 / 100,
		 ((double) psic->some_cpu_total - psip->some_cpu_total) / (100 * itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_psi(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_psiio_stats(struct activity *a, int curr, int tab,
				       unsigned long long itv)
{
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr],
		*psip = (struct stats_psi_io *) a->buf[!curr];

	if (!IS_SELECTED(a->options))
		goto close_json_markup;

	json_markup_psi(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"psi-io\": {"
		 "\"some_avg10\": %.2f, "
		 "\"some_avg60\": %.2f, "
		 "\"some_avg300\": %.2f, "
		 "\"some_avg\": %.2f, "
		 "\"full_avg10\": %.2f, "
		 "\"full_avg60\": %.2f, "
		 "\"full_avg300\": %.2f, "
		 "\"full_avg\": %.2f}",
		 (double) psic->some_aio_10  / 100,
		 (double) psic->some_aio_60  / 100,
		 (double) psic->some_aio_300 / 100,
		 ((double) psic->some_io_total - psip->some_io_total) / (100 * itv),
		 (double) psic->full_aio_10  / 100,
		 (double) psic->full_aio_60  / 100,
		 (double) psic->full_aio_300 / 100,
		 ((double) psic->full_io_total - psip->full_io_total) / (100 * itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_psi(tab, CLOSE_JSON_MARKUP);
	}
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics in JSON.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @tab		Indentation in output.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t json_print_psimem_stats(struct activity *a, int curr, int tab,
				        unsigned long long itv)
{
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr],
		*psip = (struct stats_psi_mem *) a->buf[!curr];

	if (!IS_SELECTED(a->options))
		goto close_json_markup;

	json_markup_psi(tab, OPEN_JSON_MARKUP);
	tab++;

	xprintf0(tab, "\"psi-mem\": {"
		 "\"some_avg10\": %.2f, "
		 "\"some_avg60\": %.2f, "
		 "\"some_avg300\": %.2f, "
		 "\"some_avg\": %.2f, "
		 "\"full_avg10\": %.2f, "
		 "\"full_avg60\": %.2f, "
		 "\"full_avg300\": %.2f, "
		 "\"full_avg\": %.2f}",
		 (double) psic->some_amem_10  / 100,
		 (double) psic->some_amem_60  / 100,
		 (double) psic->some_amem_300 / 100,
		 ((double) psic->some_mem_total - psip->some_mem_total) / (100 * itv),
		 (double) psic->full_amem_10  / 100,
		 (double) psic->full_amem_60  / 100,
		 (double) psic->full_amem_300 / 100,
		 ((double) psic->full_mem_total - psip->full_mem_total) / (100 * itv));
	tab--;

close_json_markup:
	if (CLOSE_MARKUP(a->options)) {
		json_markup_psi(tab, CLOSE_JSON_MARKUP);
	}
}
