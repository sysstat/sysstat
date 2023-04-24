/*
 * pr_stats.c: Functions used by sar to display statistics
 * (C) 1999-2023 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <stdlib.h>

#include "sa.h"
#include "ioconf.h"
#include "pr_xstats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern uint64_t flags;
extern int  dish;
extern int xinit;
extern char timestamp[][TIMESTAMP_LEN];
extern unsigned long avg_count;

/*
 ***************************************************************************
 * Display current activity header line.
 *
 * IN:
 * @p_timestamp		Timestamp for previous stat sample.
 * @a			Activity structure.
 * @pos			Header to display, FIRST being the first header (headers
 *			are delimited by the '|' character).
 * @iwidth		First column width (generally this is the item name).
 *			A negative value means that the corresponding field
 *			shall be displayed at the end of the line, with no
 *			indication of width.
 * @vwidth		Column width for stats values.
 * @offline_bitmap	Bitmap for offline items (e.g. CPU).
 ***************************************************************************
 */
void print_hdr_line(char *p_timestamp, struct activity *a, int pos, int iwidth, int vwidth,
		    const unsigned char *offline_bitmap)
{
	char hline[HEADER_LINE_LEN] = "";
	char cfld[16], dfld[16];
	char *hl, *tk, *it = NULL;
	int i = -1, j, k;
	int p = pos;

	strncpy(hline, a->hdr_line, sizeof(hline) - 1);
	hline[sizeof(hline) - 1] = '\0';
	for (hl = strtok(hline, "|"); hl && (pos > 0); hl = strtok(NULL, "|"), pos--);
	if (!hl)
		/* Bad @pos arg given to function */
		return;

	printf("\n%-11s", p_timestamp);

	if (strchr(hl, '&')) {
		j = strcspn(hl, "&");
		if ((a->opt_flags & 0xff00) & (1 << (8 + p))) {
			/* Display whole header line */
			*(hl + j) = ';';
		}
		else {
			/* Display only the first part of the header line */
			*(hl + j) = '\0';
		}
	}
	/* Display each field */
	for (tk = strtok(hl, ";"); tk; tk = strtok(NULL, ";"), i--) {

		if (strchr(tk, '*')) {
			strncpy(cfld, tk, sizeof(cfld) - 1);
			k = strcspn(cfld, "*");
			if (k >= strlen(cfld))
				continue;	/* Should not happen */
			cfld[k] = '\0';
			for (j = 0; (j < a->nr_ini) && (j < a->bitmap->b_size + 1); j++) {
				if (!(a->bitmap->b_array[j >> 3] & (1 << (j & 0x07))))
					/* Don't display current item if not selected */
					continue;
				if (offline_bitmap && (offline_bitmap[j >> 3] & (1 << (j & 0x07))))
					/* Don't display current item if offline */
					continue;
				if (j == 0) {
					printf(" %*s", vwidth, K_LOWERALL);
				}
				else {
					snprintf(dfld, sizeof(dfld), "%s%d%s", cfld, j - 1, cfld + k + 1);
					dfld[sizeof(dfld) - 1] = '\0';
					printf(" %*s", vwidth, dfld);
				}
			}
			continue;
		}
		if (iwidth > 0) {
			printf(" %*s", iwidth, tk);
			iwidth = 0;
			continue;
		}
		if ((iwidth < 0) && (iwidth == i)) {
			it = tk;
			iwidth = 0;
		}
		else {
			printf(" %*s", vwidth, tk);
		}
	}

	if (it) {
		printf(" %s", it);
	}
	printf("\n");
}

/*
 * **************************************************************************
 * Save CPU min and max values.
 *
 * IN:
 * @a			Activity structure with statistics.
 * @cpu			CPU number (0 for CPU "all").
 * @deltot_jiffies	Interval in jiffies.
 * @scc			Current statistics sample.
 * @scp			Previous statistics sample.
 ***************************************************************************
 */
void save_cpu_xstats(struct activity *a, int cpu, unsigned long long deltot_jiffies,
		     struct stats_cpu *scc, struct stats_cpu *scp)
{
	if (!cpu && !deltot_jiffies) {
		int j, k;

		/* Current CPU (which is not CPU "all") is tickless */
		if (DISPLAY_CPU_DEF(a->opt_flags)) {
			j  = 5;	/* -u */
		}
		else {	/* DISPLAY_CPU_ALL(a->opt_flags) */
			j = 9;	/* -u ALL */
		}
		for (k = 0; k < j; k++) {
			save_minmax(a, cpu * a->xnr + k, 0.0);
		}
		/* %idle = 100% */
		save_minmax(a, cpu * a->xnr + 9, 100.0);

		return;
	}

	if (DISPLAY_CPU_DEF(a->opt_flags)) {
		/* %user */
		save_minmax(a, cpu * a->xnr,
			    ll_sp_value(scp->cpu_user, scc->cpu_user, deltot_jiffies));
		/* %nice */
		save_minmax(a, cpu * a->xnr + 1,
			    ll_sp_value(scp->cpu_nice, scc->cpu_nice, deltot_jiffies));
		/* %system */
		save_minmax(a, cpu * a->xnr + 2,
			    ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
					scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
		   deltot_jiffies));
	}
	else {  /* DISPLAY_CPU_ALL(a->opt_flags) */
		/* %usr */
		save_minmax(a, cpu * a->xnr,
			    (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
			    0.0 :
			    ll_sp_value(scp->cpu_user - scp->cpu_guest,
					scc->cpu_user - scc->cpu_guest, deltot_jiffies));
		/* %nice */
		save_minmax(a, cpu * a->xnr + 1,
			    (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
			    0.0 :
			    ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
					scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies));
		/* %sys */
		save_minmax(a, cpu * a->xnr + 2,
			    ll_sp_value(scp->cpu_sys, scc->cpu_sys, deltot_jiffies));
		/* %irq */
		save_minmax(a, cpu * a->xnr + 5,
			    ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, deltot_jiffies));
		/* %soft */
		save_minmax(a, cpu * a->xnr + 6,
			    ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, deltot_jiffies));
		/* %guest */
		save_minmax(a, cpu * a->xnr + 7,
			    ll_sp_value(scp->cpu_guest, scc->cpu_guest, deltot_jiffies));
		/* %gnice */
		save_minmax(a, cpu * a->xnr + 8,
			    ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies));
	}

	/* %iowait */
	save_minmax(a, cpu * a->xnr + 3,
		    ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies));
	/* %steal */
	save_minmax(a, cpu * a->xnr + 4,
		    ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies));
	/* %idle */
	save_minmax(a, cpu * a->xnr + 9,
		    (scc->cpu_idle < scp->cpu_idle ? 0.0 :
		    ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies)));
}

/*
 * **************************************************************************
 * Print a line of CPU statistics.
 *
 * IN:
 * @a			Activity structure with statistics.
 * @cpu			CPU number (0 for CPU "all").
 * @deltot_jiffies	Interval in jiffies.
 * @scc			Current statistics sample.
 * @scp			Previous statistics sample.
 ***************************************************************************
 */
void print_oneline_cpu_stats(struct activity *a, int cpu, unsigned long long deltot_jiffies,
			     struct stats_cpu *scc, struct stats_cpu *scp)
{
	if (cpu == 0) {
		/* This is CPU "all" */
		cprintf_in(IS_STR, " %s", "    all", 0);
	}
	else {
		cprintf_in(IS_INT, " %7d", "", cpu - 1);

		if (!deltot_jiffies) {
			/*
			 * If the CPU is tickless then there is no change in CPU values
			 * but the sum of values is not zero.
			 * %user, %nice, %system, %iowait, %steal, ..., %idle
			 */
			cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 5, 9, 2,
				    0.0, 0.0, 0.0, 0.0, 0.0);

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 1, 9, 2, 100.0);
			}
			/*
			 * Four additional fields to display:
			 * %irq, %soft, %guest, %gnice.
			 */
			else if (DISPLAY_CPU_ALL(a->opt_flags)) {
				cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 5, 9, 2,
					    0.0, 0.0, 0.0, 0.0, 100.0);
			}
			printf("\n");

			return;
		}
	}

	if (DISPLAY_CPU_DEF(a->opt_flags)) {
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 5, 9, 2,
			    ll_sp_value(scp->cpu_user, scc->cpu_user, deltot_jiffies),
			    ll_sp_value(scp->cpu_nice, scc->cpu_nice, deltot_jiffies),
			    ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
					scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
					deltot_jiffies),
			    ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, deltot_jiffies),
			    ll_sp_value(scp->cpu_steal, scc->cpu_steal, deltot_jiffies));

		cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 2,
			    scc->cpu_idle < scp->cpu_idle ?
			    0.0 :
			    ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies));
	}
	else { /* DISPLAY_CPU_ALL(a->opt_flags) */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 9, 9, 2,
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
			    ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, deltot_jiffies));

		cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 2,
			    scc->cpu_idle < scp->cpu_idle ?
			    0.0 :
			    ll_sp_value(scp->cpu_idle, scc->cpu_idle, deltot_jiffies));
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Display CPU statistics.
 * NB: The stats are only calculated over the part of the time interval when
 * the CPU was online. As a consequence, the sum (%user + %nice + ... + %idle)
 * will always be 100% on the time interval even if the CPU has been offline
 * most of the time.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second (independent of the
 *		number of processors). Unused here.
 ***************************************************************************
 */
__print_funct_t print_cpu_stats(struct activity *a, int prev, int curr,
				unsigned long long itv)
{
	int i;
	unsigned long long deltot_jiffies = 1;
	struct stats_cpu *scc, *scp;
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->nr2 * a->xnr);
	}

	if (dish && !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST + DISPLAY_CPU_ALL(a->opt_flags), 7, 9,
			       NULL);
	}

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini
	 * (since @nr_ini counts up all CPU, even those offline).
	 * If this happens, it may be because the machine has been
	 * restarted with more CPU and no LINUX_RESTART has been
	 * inserted in file.
	 * No problem here with @nr_allocated. Having been able to
	 * read @nr[curr] structures shows that buffers are large enough.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/*
	 * Compute CPU "all" as sum of all individual CPU (on SMP machines)
	 * and look for offline CPU.
	 */
	if (a->nr_ini > 1) {
		deltot_jiffies = get_global_cpu_statistics(a, prev, curr,
							   flags, offline_cpu_bitmap);
	}

	/*
	 * Now display CPU statistics (including CPU "all"),
	 * except for offline CPU or CPU that the user doesn't want to see.
	 */
	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * Should current CPU (including CPU "all") be displayed?
		 * Note: @nr[curr] is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* Don't display CPU */
			continue;

		scc = (struct stats_cpu *) ((char *) a->buf[curr] + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[prev] + i * a->msize);

		if (i == 0) {
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
			/* Recalculate interval for current proc */
			deltot_jiffies = get_per_cpu_interval(scc, scp);
		}

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (prev != 2) {
				/* Save min and max values */
				save_cpu_xstats(a, i, deltot_jiffies, scc, scp);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST + DISPLAY_CPU_ALL(a->opt_flags), 7, 9,
					       NULL);
				print_cpu_xstats(DISPLAY_CPU_DEF(a->opt_flags), i,
						 H_MIN, a->spmin + i * a->xnr);
				print_cpu_xstats(DISPLAY_CPU_DEF(a->opt_flags), i,
						 H_MAX, a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);

		/* Print CPU stats */
		print_oneline_cpu_stats(a, i, deltot_jiffies, scc, scp);
	}
}

/*
 ***************************************************************************
 * Display tasks creation and context switches statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pcsw_stats(struct activity *a, int prev, int curr,
				 unsigned long long itv)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[prev];
	int g_fields[] = {1, 0};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) spc, (void *) spp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
		  S_VALUE(spp->processes,      spc->processes,      itv),
		  S_VALUE(spp->context_switch, spc->context_switch, itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_irq_stats(struct activity *a, int prev, int curr,
				unsigned long long itv)
{
	int c, i;
	struct stats_irq *stc_cpu_irq, *stp_cpu_irq, *stc_cpuall_irq, *stp_cpuall_irq;
	unsigned char masked_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	double val;

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->nr2 * a->xnr);
	}

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini
	 * (since @nr_ini counts up all CPU, even those offline).
	 * If this happens, it may be because the machine has been
	 * restarted with more CPU and no LINUX_RESTART has been
	 * inserted in file.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Identify offline and unselected CPU, and keep persistent statistics values */
	get_global_int_statistics(a, prev, curr, flags, masked_cpu_bitmap);

	/*
	 * Always display header line. The columns may vary if e.g. a CPU goes offline
	 * and/or comes back online.
	 */
	if (!((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, DISPLAY_PRETTY(flags) ? -1 : 0, 9,
		       masked_cpu_bitmap);
	}

	for (i = 0; i < a->nr2; i++) {
		stc_cpuall_irq = (struct stats_irq *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, stc_cpuall_irq->irq_name))
				/* Device not found */
				continue;
		}

		stp_cpuall_irq = (struct stats_irq *) ((char *) a->buf[prev] + i * a->msize);

		if (DISPLAY_ZERO_OMIT(flags) && (stc_cpuall_irq->irq_nr == stp_cpuall_irq->irq_nr))
			continue;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags) && (prev == 2)) {
			/* Display min and max values */
			print_hdr_line(timestamp[!curr], a, FIRST,
				       DISPLAY_PRETTY(flags) ? -1 : 0, 9,
				       masked_cpu_bitmap);
			print_irq_xstats(H_MIN, a, curr, i, stc_cpuall_irq->irq_name,
					 masked_cpu_bitmap, a->spmin);
			print_irq_xstats(H_MAX, a, curr, i, stc_cpuall_irq->irq_name,
					 masked_cpu_bitmap, a->spmax);
		}

		printf("%-11s", timestamp[curr]);

		if (!DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %9s", stc_cpuall_irq->irq_name, 0);
		}

		for (c = 0; (c < a->nr[curr]) && (c < a->bitmap->b_size + 1); c++) {

			stc_cpu_irq = (struct stats_irq *) ((char *) a->buf[curr] + c * a->msize * a->nr2
										  + i * a->msize);
			stp_cpu_irq = (struct stats_irq *) ((char *) a->buf[prev] + c * a->msize * a->nr2
										  + i * a->msize);

			/* Should current CPU (including CPU "all") be displayed? */
			if (masked_cpu_bitmap[c >> 3] & (1 << (c & 0x07)))
				/* No */
				continue;

			if (!c && (stc_cpu_irq->irq_nr < stp_cpu_irq->irq_nr)) {
				/*
				 * Total number among all CPU for a given interrupt may decrease if
				 * a CPU has gone offline. In this case we display "0.00" instead of
				 * the huge number that would appear otherwise.
				 */
				val = 0.0;
			}
			else {
				val = S_VALUE(stp_cpu_irq->irq_nr, stc_cpu_irq->irq_nr, itv);
			}
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2, val);
			if (DISPLAY_MINMAX(flags) && (prev != 2)) {
				/* Save min and max values */
				save_minmax(a, (c * a->nr2 + i) * a->xnr, val);
			}
		}
		if (DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %s", stc_cpuall_irq->irq_name, 0);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display swapping statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_swap_stats(struct activity *a, int prev, int curr,
				 unsigned long long itv)
{
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[prev];
	int g_fields[] = {0, 1};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) ssc, (void *) ssp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
		  S_VALUE(ssp->pswpin,  ssc->pswpin,  itv),
		  S_VALUE(ssp->pswpout, ssc->pswpout, itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display paging statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_paging_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) spc, (void *) spp,
				     itv, a->spmin, a->spmax, g_fields);
			/* Save min and max values for %vmeff */
			save_minmax(a, 8,
				    (spc->pgscan_kswapd + spc->pgscan_direct -
				    spp->pgscan_kswapd - spp->pgscan_direct) ?
				    SP_VALUE(spp->pgsteal, spc->pgsteal,
					     spc->pgscan_kswapd + spc->pgscan_direct -
					     spp->pgscan_kswapd - spp->pgscan_direct)
				    : 0.0);
		}
		else {
			/* Print min and max values */
			print_paging_xstats(H_MIN, a->spmin);
			print_paging_xstats(H_MAX, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 8, 9, 2,
		  S_VALUE(spp->pgpgin,        spc->pgpgin,        itv),
		  S_VALUE(spp->pgpgout,       spc->pgpgout,       itv),
		  S_VALUE(spp->pgfault,       spc->pgfault,       itv),
		  S_VALUE(spp->pgmajfault,    spc->pgmajfault,    itv),
		  S_VALUE(spp->pgfree,        spc->pgfree,        itv),
		  S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv),
		  S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv),
		  S_VALUE(spp->pgsteal,       spc->pgsteal,       itv));
	cprintf_xpc(DISPLAY_UNIT(flags), XLOW0, 1, 9, 2,
		   (spc->pgscan_kswapd + spc->pgscan_direct -
		   spp->pgscan_kswapd - spp->pgscan_direct) ?
		   SP_VALUE(spp->pgsteal, spc->pgsteal,
			    spc->pgscan_kswapd + spc->pgscan_direct -
			    spp->pgscan_kswapd - spp->pgscan_direct)
		   : 0.0);
	printf("\n");
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_io_stats(struct activity *a, int prev, int curr,
			       unsigned long long itv)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 4, 5, 3, 6};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sic, (void *) sip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	/*
	 * If we get negative values, this is probably because
	 * one or more devices/filesystems have been unmounted.
	 * We display 0.0 in this case though we should rather tell
	 * the user that the value cannot be calculated here.
	 */
	cprintf_f(NO_UNIT, FALSE, 7, 9, 2,
		  sic->dk_drive < sip->dk_drive ? 0.0 :
		  S_VALUE(sip->dk_drive, sic->dk_drive, itv),
		  sic->dk_drive_rio < sip->dk_drive_rio ? 0.0 :
		  S_VALUE(sip->dk_drive_rio, sic->dk_drive_rio, itv),
		  sic->dk_drive_wio < sip->dk_drive_wio ? 0.0 :
		  S_VALUE(sip->dk_drive_wio, sic->dk_drive_wio, itv),
		  sic->dk_drive_dio < sip->dk_drive_dio ? 0.0 :
		  S_VALUE(sip->dk_drive_dio, sic->dk_drive_dio, itv),
		  sic->dk_drive_rblk < sip->dk_drive_rblk ? 0.0 :
		  S_VALUE(sip->dk_drive_rblk, sic->dk_drive_rblk, itv),
		  sic->dk_drive_wblk < sip->dk_drive_wblk ? 0.0 :
		  S_VALUE(sip->dk_drive_wblk, sic->dk_drive_wblk, itv),
		  sic->dk_drive_dblk < sip->dk_drive_dblk ? 0.0 :
		  S_VALUE(sip->dk_drive_dblk, sic->dk_drive_dblk, itv));
	printf("\n");
}

/*
 * **************************************************************************
 * Display RAM memory utilization. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @smc		Structure with statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @unit	Default values unit.
 * @dispall	TRUE if all memory fields should be displayed.
 ***************************************************************************
 */
void print_ram_memory_stats(struct stats_memory *smc, int dispavg, int unit, int dispall)
{
	static unsigned long long
		avg_frmkb       = 0,
		avg_bufkb       = 0,
		avg_camkb       = 0,
		avg_comkb       = 0,
		avg_activekb    = 0,
		avg_inactkb     = 0,
		avg_dirtykb     = 0,
		avg_anonpgkb    = 0,
		avg_slabkb      = 0,
		avg_kstackkb    = 0,
		avg_pgtblkb     = 0,
		avg_vmusedkb    = 0,
		avg_availablekb = 0;
	unsigned long long nousedmem;

	if (!dispavg) {
		/* Display instantaneous values */
		nousedmem = smc->frmkb + smc->bufkb + smc->camkb + smc->slabkb;
		if (nousedmem > smc->tlmkb) {
			nousedmem = smc->tlmkb;
		}
		cprintf_u64(unit, 3, 9,
			    (unsigned long long) smc->frmkb,
			    (unsigned long long) smc->availablekb,
			    (unsigned long long) (smc->tlmkb - nousedmem));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    smc->tlmkb ?
			    SP_VALUE(nousedmem, smc->tlmkb, smc->tlmkb)
			    : 0.0);
		cprintf_u64(unit, 3, 9,
			    (unsigned long long) smc->bufkb,
			    (unsigned long long) smc->camkb,
			    (unsigned long long) smc->comkb);
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    (smc->tlmkb + smc->tlskb) ?
			    SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb)
			    : 0.0);
		cprintf_u64(unit, 3, 9,
			    (unsigned long long) smc->activekb,
			    (unsigned long long) smc->inactkb,
			    (unsigned long long) smc->dirtykb);

		if (dispall) {
			/* Display extended memory statistics */
			cprintf_u64(unit, 5, 9,
				    (unsigned long long) smc->anonpgkb,
				    (unsigned long long) smc->slabkb,
				    (unsigned long long) smc->kstackkb,
				    (unsigned long long) smc->pgtblkb,
				    (unsigned long long) smc->vmusedkb);
		}

		/*
		 * Will be used to compute the average.
		 * We assume that the total amount of memory installed can not vary
		 * during the interval given on the command line.
		 */
		avg_frmkb       += smc->frmkb;
		avg_bufkb       += smc->bufkb;
		avg_camkb       += smc->camkb;
		avg_comkb       += smc->comkb;
		avg_activekb    += smc->activekb;
		avg_inactkb     += smc->inactkb;
		avg_dirtykb     += smc->dirtykb;
		avg_anonpgkb    += smc->anonpgkb;
		avg_slabkb      += smc->slabkb;
		avg_kstackkb    += smc->kstackkb;
		avg_pgtblkb     += smc->pgtblkb;
		avg_vmusedkb    += smc->vmusedkb;
		avg_availablekb += smc->availablekb;
	}
	else {
		/* Display average values */
		nousedmem = avg_frmkb + avg_bufkb + avg_camkb + avg_slabkb;
		cprintf_f(unit, FALSE, 3, 9, 0,
			  (double) avg_frmkb / avg_count,
			  (double) avg_availablekb / avg_count,
			  (double) smc->tlmkb - ((double) nousedmem / avg_count));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    smc->tlmkb ?
			    SP_VALUE((double) (nousedmem / avg_count), smc->tlmkb, smc->tlmkb)
			    : 0.0);
		cprintf_f(unit, FALSE, 3, 9, 0,
			  (double) avg_bufkb / avg_count,
			  (double) avg_camkb / avg_count,
			  (double) avg_comkb / avg_count);
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    (smc->tlmkb + smc->tlskb) ?
			    SP_VALUE(0.0, (double) (avg_comkb / avg_count), smc->tlmkb + smc->tlskb)
			    : 0.0);
		cprintf_f(unit, FALSE, 3, 9, 0,
			  (double) avg_activekb / avg_count,
			  (double) avg_inactkb / avg_count,
			  (double) avg_dirtykb / avg_count);

		if (dispall) {
			cprintf_f(unit, FALSE, 5, 9, 0,
				  (double) avg_anonpgkb / avg_count,
				  (double) avg_slabkb / avg_count,
				  (double) avg_kstackkb / avg_count,
				  (double) avg_pgtblkb / avg_count,
				  (double) avg_vmusedkb / avg_count);
		}

		/* Reset average counters */
		avg_frmkb = avg_bufkb = avg_camkb = avg_comkb = 0;
		avg_activekb = avg_inactkb = avg_dirtykb = 0;
		avg_anonpgkb = avg_slabkb = avg_kstackkb = 0;
		avg_pgtblkb = avg_vmusedkb = avg_availablekb = 0;
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display RAM memory utilization. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @smc		Structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @unit	Default values unit.
 ***************************************************************************
 */
void stub_print_ram_memory_stats(struct activity *a, struct stats_memory *smc,
				 int curr, int dispavg, int unit)
{
	int g_fields[] = {0, 4, 5, -1, -1, -1, -1, 6, 8, 9, 10, 11, 12, 13, 14, 15, 1};

	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			unsigned long long nousedmem;

			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) smc, NULL,
				     0, a->spmin, a->spmax, g_fields);

			/* Save min and max values for %memused */
			nousedmem = smc->frmkb + smc->bufkb + smc->camkb + smc->slabkb;
			if (nousedmem > smc->tlmkb) {
				nousedmem = smc->tlmkb;
			}
			save_minmax(a, 3,
				    smc->tlmkb ? SP_VALUE(nousedmem, smc->tlmkb, smc->tlmkb) : 0.0);
			/* Save min and max values for %commit */
			save_minmax(a, 7, (smc->tlmkb + smc->tlskb)
					  ? SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb)
					  : 0.0);
			/* Save min and max values for memued */
			save_minmax(a, 2, (double) (smc->tlmkb - nousedmem));
		}
		else {
			/* Print min and max values */
			print_ram_memory_xstats(H_MIN, a->spmin,
						unit, DISPLAY_MEM_ALL(a->opt_flags));
			print_ram_memory_xstats(H_MAX, a->spmax,
						unit, DISPLAY_MEM_ALL(a->opt_flags));
		}
	}

	printf("%-11s", timestamp[curr]);

	print_ram_memory_stats(smc, dispavg, unit, DISPLAY_MEM_ALL(a->opt_flags));
}

/*
 * **************************************************************************
 * Display swap memory utilization. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @smc		Structure with statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @unit	Default values unit.
 ***************************************************************************
 */
void print_swap_memory_stats(struct stats_memory *smc, int dispavg, int unit)
{
	static unsigned long long
	avg_frskb = 0,
	avg_tlskb = 0,
	avg_caskb = 0;

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(unit, 2, 9,
			    (unsigned long long) smc->frskb,
			    (unsigned long long) (smc->tlskb - smc->frskb));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    smc->tlskb ?
			    SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb)
			    : 0.0);
		cprintf_u64(unit, 1, 9,
			    (unsigned long long) smc->caskb);
		cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 1, 9, 2,
			    (smc->tlskb - smc->frskb) ?
			    SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb)
			    : 0.0);

		/*
		 * Will be used to compute the average.
		 * We assume that the total amount of swap space may vary.
		 */
		avg_frskb += smc->frskb;
		avg_tlskb += smc->tlskb;
		avg_caskb += smc->caskb;
	}
	else {
		/* Display average values */
		cprintf_f(unit, FALSE, 2, 9, 0,
			  (double) avg_frskb / avg_count,
			  ((double) avg_tlskb / avg_count) -
			  ((double) avg_frskb / avg_count));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			    avg_tlskb ?
			    SP_VALUE((double) avg_frskb / avg_count,
				     (double) avg_tlskb / avg_count,
				     (double) avg_tlskb / avg_count)
			    : 0.0);
		cprintf_f(unit, FALSE, 1, 9, 0,
			  (double) avg_caskb / avg_count);
		cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 1, 9, 2,
			    (avg_tlskb != avg_frskb) ?
			    SP_VALUE(0.0, (double) avg_caskb / avg_count,
				     ((double) avg_tlskb / avg_count) -
				     ((double) avg_frskb / avg_count))
			    : 0.0);

		/* Reset average counters */
		avg_frskb = avg_tlskb = avg_caskb = 0;
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display swap memory utilization. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @smc		Structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @unit	Default values unit.
 ***************************************************************************
 */
void stub_print_swap_memory_stats(struct activity *a, struct stats_memory *smc,
				  int curr, int dispavg, int unit)
{
	int g_fields[] = {-1, -1, -1, -1, 16, -1, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, SECOND, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) smc, NULL,
				     0, a->spmin, a->spmax, g_fields);

			/* Save min and max values for %swpused */
			save_minmax(a, 18,
				    smc->tlskb ? SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb)
					       : 0.0);
			/* Save min and max values for %swpcad */
			save_minmax(a, 20,
				    (smc->tlskb - smc->frskb) ?
				    SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb) :
				    0.0);
			/* Save min and max values for swpused */
			save_minmax(a, 17, (double) (smc->tlskb - smc->frskb));
		}
		else {
			/* Print min and max values */
			print_swap_memory_xstats(H_MIN, a->spmin,
						 unit, DISPLAY_MEM_ALL(a->opt_flags));
			print_swap_memory_xstats(H_MAX, a->spmax,
						 unit, DISPLAY_MEM_ALL(a->opt_flags));
		}
	}

	printf("%-11s", timestamp[curr]);

	print_swap_memory_stats(smc, dispavg, unit);
}

/*
 ***************************************************************************
 * Display memory and swap utilization. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_memory_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];
	int unit = NO_UNIT;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}

	if (DISPLAY_UNIT(flags)) {
		/* Default values unit is kB */
		unit = UNIT_KILOBYTE;
	}

	if (DISPLAY_MEMORY(a->opt_flags)) {
		stub_print_ram_memory_stats(a, smc, curr, dispavg, unit);
	}

	if (DISPLAY_SWAP(a->opt_flags)) {
		stub_print_swap_memory_stats(a, smc, curr, dispavg, unit);
	}
}

/*
 ***************************************************************************
 * Display memory and swap statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_memory_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	stub_print_memory_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_memory_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	stub_print_memory_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display kernel tables statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
void stub_print_ktables_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];
	int g_fields[] = {1, 2, 0, 3};
	static unsigned long long
		avg_dentry_stat = 0,
		avg_file_used   = 0,
		avg_inode_used  = 0,
		avg_pty_nr      = 0;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}

	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) skc, NULL,
				     0, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genu64_xstats(H_MIN, a->xnr, a->spmin);
			print_genu64_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(NO_UNIT, 4, 9,
			    (unsigned long long) skc->dentry_stat,
			    (unsigned long long) skc->file_used,
			    (unsigned long long) skc->inode_used,
			    (unsigned long long) skc->pty_nr);

		/*
		 * Will be used to compute the average.
		 * Note: Overflow unlikely to happen but not impossible...
		 */
		avg_dentry_stat += skc->dentry_stat;
		avg_file_used   += skc->file_used;
		avg_inode_used  += skc->inode_used;
		avg_pty_nr      += skc->pty_nr;
	}
	else {
		/* Display average values */
		cprintf_f(NO_UNIT, FALSE, 4, 9, 0,
			  (double) avg_dentry_stat / avg_count,
			  (double) avg_file_used   / avg_count,
			  (double) avg_inode_used  / avg_count,
			  (double) avg_pty_nr      / avg_count);

		/* Reset average counters */
		avg_dentry_stat = avg_file_used = avg_inode_used = avg_pty_nr = 0;
	}

	printf("\n");
}

/*
 ***************************************************************************
 * Display kernel tables statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_ktables_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	stub_print_ktables_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average kernel tables statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_ktables_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_ktables_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display queue and load statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_queue_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];
	int g_fields[] = {0, 5, 1, 2, 3, 4};
	static unsigned long long
		avg_nr_running    = 0,
		avg_nr_threads    = 0,
		avg_load_avg_1    = 0,
		avg_load_avg_5    = 0,
		avg_load_avg_15   = 0,
		avg_procs_blocked = 0;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sqc, NULL,
				     0, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_queue_xstats(H_MIN, a->spmin);
			print_queue_xstats(H_MAX, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(NO_UNIT, 2, 9,
			    (unsigned long long) sqc->nr_running,
			    (unsigned long long) sqc->nr_threads);
		cprintf_f(NO_UNIT, FALSE, 3, 9, 2,
			  (double) sqc->load_avg_1  / 100,
			  (double) sqc->load_avg_5  / 100,
			  (double) sqc->load_avg_15 / 100);
		cprintf_u64(NO_UNIT, 1, 9,
			    (unsigned long long) sqc->procs_blocked);

		/* Will be used to compute the average */
		avg_nr_running    += sqc->nr_running;
		avg_nr_threads    += sqc->nr_threads;
		avg_load_avg_1    += sqc->load_avg_1;
		avg_load_avg_5    += sqc->load_avg_5;
		avg_load_avg_15   += sqc->load_avg_15;
		avg_procs_blocked += sqc->procs_blocked;
	}
	else {
		/* Display average values */
		cprintf_f(NO_UNIT, FALSE, 2, 9, 0,
			  (double) avg_nr_running / avg_count,
			  (double) avg_nr_threads / avg_count);
		cprintf_f(NO_UNIT, FALSE, 3, 9, 2,
			  (double) avg_load_avg_1  / (avg_count * 100),
			  (double) avg_load_avg_5  / (avg_count * 100),
			  (double) avg_load_avg_15 / (avg_count * 100));
		cprintf_f(NO_UNIT, FALSE, 1, 9, 0,
			  (double) avg_procs_blocked / avg_count);

		/* Reset average counters */
		avg_nr_running = avg_nr_threads = 0;
		avg_load_avg_1 = avg_load_avg_5 = avg_load_avg_15 = 0;
		avg_procs_blocked = 0;
	}

	printf("\n");
}

/*
 ***************************************************************************
 * Display queue and load statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_queue_stats(struct activity *a, int prev, int curr,
				  unsigned long long itv)
{
	stub_print_queue_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average queue and load statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_queue_stats(struct activity *a, int prev, int curr,
				      unsigned long long itv)
{
	stub_print_queue_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display serial lines statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_serial_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_serial *ssc, *ssp;
	int g_fields[] = {0, 1, 2, 3, 4, 5};
	unsigned int local_types_nr[] = {0, 6, 0};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
		!((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		ssc = (struct stats_serial *) ((char *) a->buf[curr] + i * a->msize);

		if (WANT_SINCE_BOOT(flags)) {
			/*
			 * We want to display statistics since boot time.
			 * Take the first structure from buf[prev]: This is a
			 * structure that only contains 0 (it has been set to 0
			 * when it has been allocated), and which exists since
			 * there is the same number of allocated structures for
			 * buf[prev] and bur[curr] (even if nothing has been read).
			 */
			ssp = (struct stats_serial *) ((char *) a->buf[prev]);
			found = TRUE;
		}
		else {
			found = FALSE;

			if (a->nr[prev] > 0) {
				/* Look for corresponding serial line in previous iteration */
				j = i;

				if (j >= a->nr[prev]) {
					j = a->nr[prev] - 1;
				}

				j0 = j;

				do {
					ssp = (struct stats_serial *) ((char *) a->buf[prev] + j * a->msize);
					if (ssc->line == ssp->line) {
						found = TRUE;
						break;
					}
					if (++j >= a->nr[prev]) {
						j = 0;
					}
				}
				while (j != j0);
			}
		}

		if (!found)
			continue;

		if (DISPLAY_ZERO_OMIT(flags) && !memcmp(ssp, ssc, STATS_SERIAL_SIZE))
			continue;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			char name[16];
			int k, pos;

			snprintf(name, sizeof(name), "%d", ssc->line);
			name[sizeof(name) - 1] = '\0';

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), name, sizeof(name), &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values */
				save_extrema(local_types_nr, (void *) ssc, (void *) ssp,
					     itv, a->spmin + pos, a->spmax + pos, g_fields);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
				print_serial_xstats(H_MIN, name, a->spmin + pos);
				print_serial_xstats(H_MAX, name, a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_INT, "       %3d", "", ssc->line);

		cprintf_f(NO_UNIT, FALSE, 6, 9, 2,
			  S_VALUE(ssp->rx,      ssc->rx,      itv),
			  S_VALUE(ssp->tx,      ssc->tx,      itv),
			  S_VALUE(ssp->frame,   ssc->frame,   itv),
			  S_VALUE(ssp->parity,  ssc->parity,  itv),
			  S_VALUE(ssp->brk,     ssc->brk,     itv),
			  S_VALUE(ssp->overrun, ssc->overrun, itv));
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display disks statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_disk_stats(struct activity *a, int prev, int curr,
				 unsigned long long itv)
{
	int i, j;
	struct stats_disk *sdc,	*sdp, sdpzero;
	struct ext_disk_stats xds;
	char *dev_name;
	int unit = NO_UNIT;
	double rkB, wkB, dkB;
	int g_fields[] = {0};
	unsigned int local_types_nr[] = {1, 0, 0};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	memset(&sdpzero, 0, STATS_DISK_SIZE);

	if (DISPLAY_UNIT(flags)) {
		/* Default values unit is kB */
		unit = UNIT_KILOBYTE;
	}

	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
		!((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, DISPLAY_PRETTY(flags) ? -1 : 0, 9, NULL);
	}

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

		if (!WANT_SINCE_BOOT(flags)) {
			j = check_disk_reg(a, curr, prev, i);
		}
		else {
			j = -1;
		}
		if (j < 0) {
			/*
			 * This is a newly registered device or we want stats since boot time.
			 * Previous stats are zero.
			 */
			sdp = &sdpzero;
		}
		else {
			sdp = (struct stats_disk *) ((char *) a->buf[prev] + j * a->msize);
		}

		if (DISPLAY_ZERO_OMIT(flags) && !memcmp(sdp, sdc, STATS_DISK_SIZE))
			continue;

		/* Compute service time, etc. */
		compute_ext_disk_stats(sdc, sdp, itv, &xds);
		rkB = S_VALUE(sdp->rd_sect, sdc->rd_sect, itv) / 2;
		wkB = S_VALUE(sdp->wr_sect, sdc->wr_sect, itv) / 2;
		dkB = S_VALUE(sdp->dc_sect, sdc->dc_sect, itv) / 2;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			int k, pos;

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), dev_name, MAX_NAME_LEN, &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values */
				save_extrema(local_types_nr, (void *) sdc, (void *) sdp,
					     itv, a->spmin + pos, a->spmax + pos, g_fields);

				save_minmax(a, pos + 1, rkB);
				save_minmax(a, pos + 2, wkB);
				save_minmax(a, pos + 3, dkB);
				save_minmax(a, pos + 4, xds.arqsz / 2);
				save_minmax(a, pos + 5,
					    S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0);
				save_minmax(a, pos + 6, xds.await);
				save_minmax(a, pos + 7, xds.util / 10.0);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST,
					       DISPLAY_PRETTY(flags) ? -1 : 0, 9, NULL);
				print_disk_xstats(H_MIN, unit, dev_name,
						  a->spmin + pos);
				print_disk_xstats(H_MAX, unit, dev_name,
						  a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);

		if (!DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %9s", dev_name, 0);
		}
		cprintf_f(NO_UNIT, FALSE, 1, 9, 2,
			  S_VALUE(sdp->nr_ios, sdc->nr_ios,  itv));
		cprintf_f(unit, FALSE, 3, 9, 2, rkB, wkB, dkB);
		/* See iostat for explanations */
		cprintf_f(unit, FALSE, 1, 9, 2,
			  xds.arqsz / 2);
		cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
			  S_VALUE(sdp->rq_ticks, sdc->rq_ticks, itv) / 1000.0,
			  xds.await);
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			   xds.util / 10.0);
		if (DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %s", dev_name, 0);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display network interfaces statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_dev_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	int i, j;
	struct stats_net_dev *sndc, *sndp, sndzero;
	double rxkb, txkb, ifutil;
	int unit = NO_UNIT;
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6};
	unsigned int local_types_nr[] = {7, 0, 0};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	memset(&sndzero, 0, STATS_NET_DEV_SIZE);

	if (DISPLAY_UNIT(flags)) {
		/* Default values unit is bytes */
		unit = UNIT_BYTE;
	}

	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
	    !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, DISPLAY_PRETTY(flags) ? -1 : 0, 9,
			       NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		if (!WANT_SINCE_BOOT(flags)) {
			j = check_net_dev_reg(a, curr, prev, i);
		}
		else {
			j = -1;
		}
		if (j < 0) {
			/*
			 * This is a newly registered interface or we want stats since boot time.
			 * Previous stats are zero.
			 */
			sndp = &sndzero;
		}
		else {
			sndp = (struct stats_net_dev *) ((char *) a->buf[prev] + j * a->msize);
		}

		if (DISPLAY_ZERO_OMIT(flags) && !memcmp(sndp, sndc, STATS_NET_DEV_SIZE2CMP))
			continue;

		rxkb = S_VALUE(sndp->rx_bytes, sndc->rx_bytes, itv);
		txkb = S_VALUE(sndp->tx_bytes, sndc->tx_bytes, itv);
		ifutil = compute_ifutil(sndc, rxkb, txkb);

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			int k, pos;

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), sndc->interface, MAX_IFACE_LEN, &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values */
				save_extrema(local_types_nr, (void *) sndc, (void *) sndp,
					     itv, a->spmin + pos, a->spmax + pos, g_fields);

				/* Save min and max values for %ifutil */
				save_minmax(a, pos + 7, ifutil);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST,
					       DISPLAY_PRETTY(flags) ? -1 : 0, 9, NULL);
				print_net_dev_xstats(H_MIN, unit, sndc->interface,
						     a->spmin + pos);
				print_net_dev_xstats(H_MAX, unit, sndc->interface,
						     a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);

		if (!DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %9s", sndc->interface, 0);
		}

		cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
			  S_VALUE(sndp->rx_packets, sndc->rx_packets, itv),
			  S_VALUE(sndp->tx_packets, sndc->tx_packets, itv));
		cprintf_f(unit, FALSE, 2, 9, 2,
			  unit < 0 ? rxkb / 1024 : rxkb,
			  unit < 0 ? txkb / 1024 : txkb);
		cprintf_f(NO_UNIT, FALSE, 3, 9, 2,
			  S_VALUE(sndp->rx_compressed, sndc->rx_compressed, itv),
			  S_VALUE(sndp->tx_compressed, sndc->tx_compressed, itv),
			  S_VALUE(sndp->multicast,     sndc->multicast,     itv));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, ifutil);
		if (DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %s", sndc->interface, 0);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display network interface errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_edev_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	int i, j;
	struct stats_net_edev *snedc, *snedp, snedzero;
	int g_fields[] = {2, 0, 1, 3, 4, 7, 8, 6, 5};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	memset(&snedzero, 0, STATS_NET_EDEV_SIZE);

	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
		!((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, DISPLAY_PRETTY(flags) ? -1 : 0, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		if (!WANT_SINCE_BOOT(flags)) {
			j = check_net_edev_reg(a, curr, prev, i);
		}
		else {
			j = -1;
		}
		if (j < 0) {
			/*
			 * This is a newly registered interface or we want stats since boot time.
			 * Previous stats are zero.
			 */
			snedp = &snedzero;
		}
		else {
			snedp = (struct stats_net_edev *) ((char *) a->buf[prev] + j * a->msize);
		}

		if (DISPLAY_ZERO_OMIT(flags) && !memcmp(snedp, snedc, STATS_NET_EDEV_SIZE2CMP))
			continue;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			int k, pos;

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), snedc->interface, MAX_IFACE_LEN, &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values */
				save_extrema(a->gtypes_nr, (void *) snedc, (void *) snedp,
					     itv, a->spmin + pos, a->spmax + pos, g_fields);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST,
					       DISPLAY_PRETTY(flags) ? -1 : 0, 9, NULL);
				print_net_edev_xstats(H_MIN, snedc->interface,
						      a->spmin + pos);
				print_net_edev_xstats(H_MAX, snedc->interface,
						      a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);

		if (!DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %9s", snedc->interface, 0);
		}
		cprintf_f(NO_UNIT, FALSE, 9, 9, 2,
			  S_VALUE(snedp->rx_errors,         snedc->rx_errors,         itv),
			  S_VALUE(snedp->tx_errors,         snedc->tx_errors,         itv),
			  S_VALUE(snedp->collisions,        snedc->collisions,        itv),
			  S_VALUE(snedp->rx_dropped,        snedc->rx_dropped,        itv),
			  S_VALUE(snedp->tx_dropped,        snedc->tx_dropped,        itv),
			  S_VALUE(snedp->tx_carrier_errors, snedc->tx_carrier_errors, itv),
			  S_VALUE(snedp->rx_frame_errors,   snedc->rx_frame_errors,   itv),
			  S_VALUE(snedp->rx_fifo_errors,    snedc->rx_fifo_errors,    itv),
			  S_VALUE(snedp->tx_fifo_errors,    snedc->tx_fifo_errors,    itv));
		if (DISPLAY_PRETTY(flags)) {
			cprintf_in(IS_STR, " %s", snedc->interface, 0);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display NFS client statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_nfs_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snnc, (void *) snnp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 6, 9, 2,
		  S_VALUE(snnp->nfs_rpccnt,     snnc->nfs_rpccnt,     itv),
		  S_VALUE(snnp->nfs_rpcretrans, snnc->nfs_rpcretrans, itv),
		  S_VALUE(snnp->nfs_readcnt,    snnc->nfs_readcnt,    itv),
		  S_VALUE(snnp->nfs_writecnt,   snnc->nfs_writecnt,   itv),
		  S_VALUE(snnp->nfs_accesscnt,  snnc->nfs_accesscnt,  itv),
		  S_VALUE(snnp->nfs_getattcnt,  snnc->nfs_getattcnt,  itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display NFS server statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_nfsd_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr],
		*snndp = (struct stats_net_nfsd *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snndc, (void *) snndp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 11, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display network sockets statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_net_sock_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];
	static unsigned long long
		avg_sock_inuse = 0,
		avg_tcp_inuse  = 0,
		avg_udp_inuse  = 0,
		avg_raw_inuse  = 0,
		avg_frag_inuse = 0,
		avg_tcp_tw     = 0;
	int g_fields[] = {0, 1, 5, 2, 3, 4};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snsc, NULL,
				     0, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genu64_xstats(H_MIN, a->xnr, a->spmin);
			print_genu64_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(NO_UNIT, 6, 9,
			    (unsigned long long) snsc->sock_inuse,
			    (unsigned long long) snsc->tcp_inuse,
			    (unsigned long long) snsc->udp_inuse,
			    (unsigned long long) snsc->raw_inuse,
			    (unsigned long long) snsc->frag_inuse,
			    (unsigned long long) snsc->tcp_tw);

		/* Will be used to compute the average */
		avg_sock_inuse += snsc->sock_inuse;
		avg_tcp_inuse  += snsc->tcp_inuse;
		avg_udp_inuse  += snsc->udp_inuse;
		avg_raw_inuse  += snsc->raw_inuse;
		avg_frag_inuse += snsc->frag_inuse;
		avg_tcp_tw     += snsc->tcp_tw;
	}
	else {
		/* Display average values */
		cprintf_f(NO_UNIT, FALSE, 6, 9, 0,
			  (double) avg_sock_inuse / avg_count,
			  (double) avg_tcp_inuse  / avg_count,
			  (double) avg_udp_inuse  / avg_count,
			  (double) avg_raw_inuse  / avg_count,
			  (double) avg_frag_inuse / avg_count,
			  (double) avg_tcp_tw     / avg_count);

		/* Reset average counters */
		avg_sock_inuse = avg_tcp_inuse = avg_udp_inuse = 0;
		avg_raw_inuse = avg_frag_inuse = avg_tcp_tw = 0;
	}

	printf("\n");
}

/*
 ***************************************************************************
 * Display network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_sock_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	stub_print_net_sock_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_net_sock_stats(struct activity *a, int prev, int curr,
					 unsigned long long itv)
{
	stub_print_net_sock_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display IP network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_ip_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snic, (void *) snip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 8, 9, 2,
		  S_VALUE(snip->InReceives,    snic->InReceives,    itv),
		  S_VALUE(snip->ForwDatagrams, snic->ForwDatagrams, itv),
		  S_VALUE(snip->InDelivers,    snic->InDelivers,    itv),
		  S_VALUE(snip->OutRequests,   snic->OutRequests,   itv),
		  S_VALUE(snip->ReasmReqds,    snic->ReasmReqds,    itv),
		  S_VALUE(snip->ReasmOKs,      snic->ReasmOKs,      itv),
		  S_VALUE(snip->FragOKs,       snic->FragOKs,       itv),
		  S_VALUE(snip->FragCreates,   snic->FragCreates,   itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display IP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_eip_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sneic, (void *) sneip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 8, 9, 2,
		  S_VALUE(sneip->InHdrErrors,     sneic->InHdrErrors,     itv),
		  S_VALUE(sneip->InAddrErrors,    sneic->InAddrErrors,    itv),
		  S_VALUE(sneip->InUnknownProtos, sneic->InUnknownProtos, itv),
		  S_VALUE(sneip->InDiscards,      sneic->InDiscards,      itv),
		  S_VALUE(sneip->OutDiscards,     sneic->OutDiscards,     itv),
		  S_VALUE(sneip->OutNoRoutes,     sneic->OutNoRoutes,     itv),
		  S_VALUE(sneip->ReasmFails,      sneic->ReasmFails,      itv),
		  S_VALUE(sneip->FragFails,       sneic->FragFails,       itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMP network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_icmp_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snic, (void *) snip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 14, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_eicmp_stats(struct activity *a, int prev, int curr,
				      unsigned long long itv)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sneic, (void *) sneip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 12, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display TCP network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_tcp_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sntc, (void *) sntp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 4, 9, 2,
		  S_VALUE(sntp->ActiveOpens,  sntc->ActiveOpens,  itv),
		  S_VALUE(sntp->PassiveOpens, sntc->PassiveOpens, itv),
		  S_VALUE(sntp->InSegs,       sntc->InSegs,       itv),
		  S_VALUE(sntp->OutSegs,      sntc->OutSegs,      itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display TCP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_etcp_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snetc, (void *) snetp,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 5, 9, 2,
		  S_VALUE(snetp->AttemptFails, snetc->AttemptFails, itv),
		  S_VALUE(snetp->EstabResets,  snetc->EstabResets,  itv),
		  S_VALUE(snetp->RetransSegs,  snetc->RetransSegs,  itv),
		  S_VALUE(snetp->InErrs,       snetc->InErrs,       itv),
		  S_VALUE(snetp->OutRsts,      snetc->OutRsts,      itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display UDP network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_udp_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snuc, (void *) snup,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 4, 9, 2,
		  S_VALUE(snup->InDatagrams,  snuc->InDatagrams,  itv),
		  S_VALUE(snup->OutDatagrams, snuc->OutDatagrams, itv),
		  S_VALUE(snup->NoPorts,      snuc->NoPorts,      itv),
		  S_VALUE(snup->InErrors,     snuc->InErrors,     itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 sockets statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_net_sock6_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];
	static unsigned long long
		avg_tcp6_inuse  = 0,
		avg_udp6_inuse  = 0,
		avg_raw6_inuse  = 0,
		avg_frag6_inuse = 0;
	int g_fields[] = {0, 1, 2, 3};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snsc, NULL,
				     0, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genu64_xstats(H_MIN, a->xnr, a->spmin);
			print_genu64_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(NO_UNIT, 4, 9,
			    (unsigned long long) snsc->tcp6_inuse,
			    (unsigned long long) snsc->udp6_inuse,
			    (unsigned long long) snsc->raw6_inuse,
			    (unsigned long long) snsc->frag6_inuse);

		/* Will be used to compute the average */
		avg_tcp6_inuse  += snsc->tcp6_inuse;
		avg_udp6_inuse  += snsc->udp6_inuse;
		avg_raw6_inuse  += snsc->raw6_inuse;
		avg_frag6_inuse += snsc->frag6_inuse;
	}
	else {
		/* Display average values */
		cprintf_f(NO_UNIT, FALSE, 4, 9, 0,
			  (double) avg_tcp6_inuse  / avg_count,
			  (double) avg_udp6_inuse  / avg_count,
			  (double) avg_raw6_inuse  / avg_count,
			  (double) avg_frag6_inuse / avg_count);

		/* Reset average counters */
		avg_tcp6_inuse = avg_udp6_inuse = avg_raw6_inuse = avg_frag6_inuse = 0;
	}

	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_sock6_stats(struct activity *a, int prev, int curr,
				      unsigned long long itv)
{
	stub_print_net_sock6_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average IPv6 sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_net_sock6_stats(struct activity *a, int prev, int curr,
					  unsigned long long itv)
{
	stub_print_net_sock6_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display IPv6 network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_ip6_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snic, (void *) snip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 10, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_eip6_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sneic, (void *) sneip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 11, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMPv6 network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_icmp6_stats(struct activity *a, int prev, int curr,
				      unsigned long long itv)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snic, (void *) snip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 17, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_eicmp6_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) sneic, (void *) sneip,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 11, 9, 2,
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
	printf("\n");
}

/*
 ***************************************************************************
 * Display UDPv6 network traffic statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_net_udp6_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[prev];
	int g_fields[] = {0, 1, 2, 3};

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) snuc, (void *) snup,
				     itv, a->spmin, a->spmax, g_fields);
		}
		else {
			/* Print min and max values */
			print_genf_xstats(H_MIN, a->xnr, a->spmin);
			print_genf_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);
	cprintf_f(NO_UNIT, FALSE, 4, 9, 2,
		  S_VALUE(snup->InDatagrams6,  snuc->InDatagrams6,  itv),
		  S_VALUE(snup->OutDatagrams6, snuc->OutDatagrams6, itv),
		  S_VALUE(snup->NoPorts6,      snuc->NoPorts6,      itv),
		  S_VALUE(snup->InErrors6,     snuc->InErrors6,     itv));
	printf("\n");
}

/*
 ***************************************************************************
 * Display CPU frequency statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
void stub_print_pwr_cpufreq_stats(struct activity *a, int curr, int dispavg)
{
	int i;
	struct stats_pwr_cpufreq *spc;
	static __nr_t nr_alloc = 0;
	static unsigned long long
		*avg_cpufreq = NULL;
	char name[16];

	if (!avg_cpufreq || (a->nr[curr] > nr_alloc)) {
		/* Allocate array of CPU frequency */
		SREALLOC(avg_cpufreq, unsigned long long, sizeof(unsigned long long) * a->nr[curr]);
		if (a->nr[curr] > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_cpufreq + nr_alloc, 0,
			       sizeof(unsigned long long) * (a->nr[curr] - nr_alloc));
		}
		nr_alloc = a->nr[curr];
	}

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if (dish && !(dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
	}

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_pwr_cpufreq) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * spc = (struct stats_pwr_cpufreq *) a->buf[...] + i;
		 */
		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		if (!spc->cpufreq)
			/* This CPU is offline: Don't display it */
			continue;

		/*
		 * Note: @nr[curr] is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			strcpy(name, "     all");
		}
		else {
			snprintf(name, sizeof(name), "     %3d", i - 1);
			name[sizeof(name) - 1] = '\0';
		}

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (!dispavg) {
				/* Save min and max values */
				save_minmax(a, i * a->xnr, ((double) spc->cpufreq) / 100);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
				print_pwr_cpufreq_xstats(H_MIN, name, a->spmin + i * a->xnr);
				print_pwr_cpufreq_xstats(H_MAX, name, a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_STR, "%s", name, 0);

		if (!dispavg) {
			/* Display instantaneous values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2,
				  ((double) spc->cpufreq) / 100);

			/*
			 * Will be used to compute the average.
			 * Note: Overflow unlikely to happen but not impossible...
			 */
			avg_cpufreq[i] += spc->cpufreq;
		}
		else {
			/* Display average values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2,
				  (double) avg_cpufreq[i] / (100 * avg_count));
		}

		printf("\n");
	}

	if (dispavg && avg_cpufreq) {
		/* Array of CPU frequency no longer needed: Free it! */
		free(avg_cpufreq);
		avg_cpufreq = NULL;
		nr_alloc = 0;
	}
}

/*
 ***************************************************************************
 * Display CPU frequency statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_cpufreq_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_pwr_cpufreq_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average CPU frequency statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_cpufreq_stats(struct activity *a, int prev, int curr,
					    unsigned long long itv)
{
	stub_print_pwr_cpufreq_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display fan statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
void stub_print_pwr_fan_stats(struct activity *a, int curr, int dispavg)
{
	int i;
	struct stats_pwr_fan *spc;
	static __nr_t nr_alloc = 0;
	static double *avg_fan = NULL;
	static double *avg_fan_min = NULL;

	/* Allocate arrays of fan RPMs */
	if (!avg_fan || (a->nr[curr] > nr_alloc)) {
		SREALLOC(avg_fan, double, sizeof(double) * a->nr[curr]);
		SREALLOC(avg_fan_min, double, sizeof(double) * a->nr[curr]);

		if (a->nr[curr] > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_fan + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
			memset(avg_fan_min + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
		}
		nr_alloc = a->nr[curr];
	}

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if (dish && !(dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (!dispavg) {
				/* Save min and max values */
				save_minmax(a, i * a->xnr, spc->rpm);
				save_minmax(a, i * a->xnr + 1, spc->rpm - spc->rpm_min);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
				print_pwr_fan_xstats(H_MIN, i, spc->device,
						     a->spmin + i * a->xnr);
				print_pwr_fan_xstats(H_MAX, i, spc->device,
						     a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_INT, "     %5d", "", i + 1);

		if (dispavg) {
			/* Display average values */
			cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
				  (double) avg_fan[i] / avg_count,
				  (double) (avg_fan[i] - avg_fan_min[i]) / avg_count);
		}
		else {
			/* Display instantaneous values */
			cprintf_f(NO_UNIT, FALSE, 2, 9, 2,
				  spc->rpm,
				  spc->rpm - spc->rpm_min);
			avg_fan[i]     += spc->rpm;
			avg_fan_min[i] += spc->rpm_min;
		}

		cprintf_in(IS_STR, " %s\n", spc->device, 0);
	}

	if (dispavg && avg_fan) {
		free(avg_fan);
		free(avg_fan_min);
		avg_fan = NULL;
		avg_fan_min = NULL;
		nr_alloc = 0;
	}
}

/*
 ***************************************************************************
 * Display fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_fan_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	stub_print_pwr_fan_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_fan_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_pwr_fan_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display device temperature statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
void stub_print_pwr_temp_stats(struct activity *a, int curr, int dispavg)
{
	int i;
	struct stats_pwr_temp *spc;
	double temppct;
	static __nr_t nr_alloc = 0;
	static double *avg_temp = NULL;
	static double *avg_temp_min = NULL, *avg_temp_max = NULL;

	/* Allocate arrays of temperatures */
	if (!avg_temp || (a->nr[curr] > nr_alloc)) {
		SREALLOC(avg_temp, double, sizeof(double) * a->nr[curr]);
		SREALLOC(avg_temp_min, double, sizeof(double) * a->nr[curr]);
		SREALLOC(avg_temp_max, double, sizeof(double) * a->nr[curr]);

		if (a->nr[curr] > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_temp + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
			memset(avg_temp_min + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
			memset(avg_temp_max + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
		}
		nr_alloc = a->nr[curr];
	}

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if (dish && !(dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);

		temppct = (spc->temp_max - spc->temp_min)
			  ? (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100
			  : 0.0;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (!dispavg) {
				/* Save min and max values */
				save_minmax(a, i * a->xnr, spc->temp);
				save_minmax(a, i * a->xnr + 1, temppct);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
				print_pwr_sensor_xstats(H_MIN, i + 1, spc->device,
							a->spmin + i * a->xnr);
				print_pwr_sensor_xstats(H_MAX, i + 1, spc->device,
							a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_INT, "     %5d", "", i + 1);

		if (dispavg) {
			/* Display average values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2, (double) avg_temp[i] / avg_count);
			cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
				   (avg_temp_max[i] - avg_temp_min[i]) ?
				   ((double) (avg_temp[i] / avg_count) - avg_temp_min[i]) / (avg_temp_max[i] - avg_temp_min[i]) * 100
				   : 0.0);
		}
		else {
			/* Display instantaneous values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2, spc->temp);
			cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, temppct);
			avg_temp[i] += spc->temp;
			/* Assume that min and max temperatures cannot vary */
			avg_temp_min[i] = spc->temp_min;
			avg_temp_max[i] = spc->temp_max;
		}

		cprintf_in(IS_STR, " %s\n", spc->device, 0);
	}

	if (dispavg && avg_temp) {
		free(avg_temp);
		free(avg_temp_min);
		free(avg_temp_max);
		avg_temp = NULL;
		avg_temp_min = NULL;
		avg_temp_max = NULL;
		nr_alloc = 0;
	}
}

/*
 ***************************************************************************
 * Display temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_temp_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	stub_print_pwr_temp_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_temp_stats(struct activity *a, int prev, int curr,
					 unsigned long long itv)
{
	stub_print_pwr_temp_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display voltage inputs statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
void stub_print_pwr_in_stats(struct activity *a, int curr, int dispavg)
{
	int i;
	struct stats_pwr_in *spc;
	double inpct;
	static __nr_t nr_alloc = 0;
	static double *avg_in = NULL;
	static double *avg_in_min = NULL, *avg_in_max = NULL;

	/* Allocate arrays of voltage inputs */
	if (!avg_in || (a->nr[curr] > nr_alloc)) {
		SREALLOC(avg_in, double, sizeof(double) * a->nr[curr]);
		SREALLOC(avg_in_min, double, sizeof(double) * a->nr[curr]);
		SREALLOC(avg_in_max, double, sizeof(double) * a->nr[curr]);

		if (a->nr[curr] > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_in + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
			memset(avg_in_min + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
			memset(avg_in_max + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
		}
		nr_alloc = a->nr[curr];
	}

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if (dish && !(dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);

		inpct = (spc->in_max - spc->in_min)
			? (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100
			: 0.0;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (!dispavg) {
				/* Save min and max values */
				save_minmax(a, i * a->xnr, spc->in);
				save_minmax(a, i * a->xnr + 1, inpct);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, -2, 9, NULL);
				print_pwr_sensor_xstats(H_MIN, i, spc->device,
							a->spmin + i * a->xnr);
				print_pwr_sensor_xstats(H_MAX, i, spc->device,
							a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_INT, "     %5d", "", i);

		if (dispavg) {
			/* Display average values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2, (double) avg_in[i] / avg_count);
			cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
				   (avg_in_max[i] - avg_in_min[i]) ?
				   ((double) (avg_in[i] / avg_count) - avg_in_min[i]) / (avg_in_max[i] - avg_in_min[i]) * 100
				   : 0.0);
		}
		else {
			/* Display instantaneous values */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 2, spc->in);
			cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, inpct);
			avg_in[i] += spc->in;
			/* Assume that min and max voltage inputs cannot vary */
			avg_in_min[i] = spc->in_min;
			avg_in_max[i] = spc->in_max;
		}

		cprintf_in(IS_STR, " %s\n", spc->device, 0);
	}

	if (dispavg && avg_in) {
		free(avg_in);
		free(avg_in_min);
		free(avg_in_max);
		avg_in = NULL;
		avg_in_min = NULL;
		avg_in_max = NULL;
		nr_alloc = 0;
	}
}

/*
 ***************************************************************************
 * Display voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_in_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	stub_print_pwr_in_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_in_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	stub_print_pwr_in_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display huge pages statistics. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_huge_stats(struct activity *a, int curr, int dispavg)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];
	static unsigned long long
		avg_frhkb = 0,
		avg_tlhkb = 0,
		avg_rsvdhkb = 0,
		avg_surphkb = 0;
	int unit = NO_UNIT;
	double hugpct;
	int g_fields[] = {0, -1, 3, 4};

	if (DISPLAY_UNIT(flags)) {
		/* Default values unit is kB */
		unit = UNIT_KILOBYTE;
	}

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || (dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	/* Compute %hugused value */
	hugpct = smc->tlhkb ? SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) : 0.0;

	/* Check if min and max values should be displayed */
	if (DISPLAY_MINMAX(flags)) {
		if (!dispavg) {
			/* Save min and max values */
			save_extrema(a->gtypes_nr, (void *) smc, NULL, 0,
				     a->spmin, a->spmax, g_fields);

			/* Save min and max values for kbhugused and %hugused */
			save_minmax(a, 1, (double) (smc->tlhkb - smc->frhkb));
			save_minmax(a, 2, hugpct);
		}
		else {
			/* Display min and max values */
			print_huge_xstats(H_MIN, unit, a->spmin);
			print_huge_xstats(H_MAX, unit, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_u64(unit, 2, 9,
			    (unsigned long long) smc->frhkb,
			    (unsigned long long) (smc->tlhkb - smc->frhkb));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			   smc->tlhkb ?
			   SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) : 0.0);
		cprintf_u64(unit, 2, 9,
			    (unsigned long long) smc->rsvdhkb,
			    (unsigned long long) (smc->surphkb));

		/* Will be used to compute the average */
		avg_frhkb += smc->frhkb;
		avg_tlhkb += smc->tlhkb;
		avg_rsvdhkb += smc->rsvdhkb;
		avg_surphkb += smc->surphkb;
	}
	else {
		/* Display average values */
		cprintf_f(unit, FALSE, 2, 9, 0,
			  (double) avg_frhkb / avg_count,
			  ((double) avg_tlhkb / avg_count) -
			  ((double) avg_frhkb / avg_count));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2,
			   avg_tlhkb ?
			   SP_VALUE((double) avg_frhkb / avg_count,
				    (double) avg_tlhkb / avg_count,
				    (double) avg_tlhkb / avg_count) : 0.0);
		cprintf_f(unit, FALSE, 2, 9, 0,
			  (double) avg_rsvdhkb / avg_count,
			  (double) avg_surphkb / avg_count);

		/* Reset average counters */
		avg_frhkb = avg_tlhkb = avg_rsvdhkb = avg_surphkb = 0;
	}

	printf("\n");
}

/*
 ***************************************************************************
 * Display huge pages statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_huge_stats(struct activity *a, int prev, int curr,
				 unsigned long long itv)
{
	stub_print_huge_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display huge pages statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_huge_stats(struct activity *a, int prev, int curr,
				     unsigned long long itv)
{
	stub_print_huge_stats(a, curr, TRUE);
}

/*
 ***************************************************************************
 * Display CPU weighted frequency statistics. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
void print_pwr_wghfreq_stats(struct activity *a, int prev, int curr,
			     unsigned long long itv)
{
	int i, k;
	struct stats_pwr_wghfreq *spc, *spp, *spc_k, *spp_k;
	unsigned long long tis, tisfreq;
	double wghmhz;

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	if (dish && !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
	}

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_pwr_wghfreq) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * spc = (struct stats_pwr_wghfreq *) a->buf[...] + i;
		 */
		spc = (struct stats_pwr_wghfreq *) ((char *) a->buf[curr] + i * a->msize * a->nr2);
		spp = (struct stats_pwr_wghfreq *) ((char *) a->buf[prev] + i * a->msize * a->nr2);

		/*
		 * Note: a->nr is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (prev == 2) {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
				print_pwr_wghfreq_xstats(H_MIN, i, a->spmin + i * a->xnr);
				print_pwr_wghfreq_xstats(H_MAX, i, a->spmax + i * a->xnr);
			}
		}

		/* Yes: Display it */
		printf("%-11s", timestamp[curr]);

		if (!i) {
			/* This is CPU "all" */
			cprintf_in(IS_STR, "%s", "     all", 0);
		}
		else {
			cprintf_in(IS_INT, "     %3d", "", i - 1);
		}

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

		/* Compute wghMHz value for current CPU */
		wghmhz = tis ? ((double) tisfreq) / tis : 0.0;

		if (DISPLAY_MINMAX(flags) && (prev != 2)) {
			/* Save min and max values */
			save_minmax(a, i * a->xnr, wghmhz);
		}

		/* Display weighted frequency for current CPU */
		cprintf_f(NO_UNIT, FALSE, 1, 9, 2, wghmhz);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display USB devices statistics. This function is used to
 * display instantaneous and summary statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
void stub_print_pwr_usb_stats(struct activity *a, int curr, int dispavg)
{
	int i, j;
	char fmt[16];
	struct stats_pwr_usb *suc, *sum;

	if (dish) {
		printf("\n%-11s     BUS  idvendor    idprod  maxpower",
		       (dispavg ? _("Summary:") : timestamp[!curr]));
		printf(" %-*s product\n", MAX_MANUF_LEN - 1, "manufact");
	}

	for (i = 0; i < a->nr[curr]; i++) {
		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);

		printf("%-11s", (dispavg ? _("Summary:") : timestamp[curr]));
		cprintf_in(IS_INT, "  %6d", "", suc->bus_nr);
		cprintf_x(2, 9,
			  suc->vendor_id,
			  suc->product_id);
		cprintf_u64(NO_UNIT, 1, 9,
			    /* bMaxPower is expressed in 2 mA units */
			    (unsigned long long) (suc->bmaxpower << 1));

		snprintf(fmt, sizeof(fmt), " %%-%ds", MAX_MANUF_LEN - 1);
		cprintf_s(IS_STR, fmt, suc->manufacturer);
		cprintf_s(IS_STR, " %s\n", suc->product);

		if (!dispavg) {
			/* Save current USB device in summary list */
			for (j = 0; j < a->nr_allocated; j++) {
				sum = (struct stats_pwr_usb *) ((char *) a->buf[2] + j * a->msize);

				if ((sum->bus_nr     == suc->bus_nr) &&
				    (sum->vendor_id  == suc->vendor_id) &&
				    (sum->product_id == suc->product_id))
					/* USB device found in summary list */
					break;
				if (!sum->bus_nr) {
					/*
					 * Current slot is free:
					 * Save USB device in summary list.
					 */
					*sum = *suc;
					a->nr[2] = j + 1;
					break;
				}
			}
			if (j == a->nr_allocated) {
				/*
				 * No free slot has been found for current device.
				 * So enlarge buffers then save device in list.
				 */
				reallocate_all_buffers(a, j, flags);
				sum = (struct stats_pwr_usb *) ((char *) a->buf[2] + j * a->msize);
				*sum = *suc;
				a->nr[2] = j + 1;
			}
		}
	}
}

/*
 ***************************************************************************
 * Display USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_usb_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	stub_print_pwr_usb_stats(a, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_usb_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_pwr_usb_stats(a, 2, TRUE);
}

/*
 ***************************************************************************
 * Display filesystems statistics. This function is used to
 * display instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 ***************************************************************************
 */
__print_funct_t stub_print_filesystem_stats(struct activity *a, int prev, int curr, int dispavg)
{
	int i, j, j0, found;
	struct stats_filesystem *sfc, *sfp, *sfm;
	int unit = NO_UNIT;
	char *dev_name;
	double mbfsfree, mbfsused, fsusedpct, ufsusedpct, iusedpct;

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if (DISPLAY_UNIT(flags)) {
		/* Default values unit is B */
		unit = UNIT_BYTE;
	}

	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
		!(dispavg && DISPLAY_MINMAX(flags))) {
		print_hdr_line((dispavg ? _("Summary:") : timestamp[!curr]),
				a, FIRST + DISPLAY_MOUNT(a->opt_flags), -1, 9, NULL);
	}

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

		found = FALSE;
		if (DISPLAY_ZERO_OMIT(flags) && !dispavg) {

			if (a->nr[prev] > 0) {
				/* Look for corresponding fs in previous iteration */
				j = i;

				if (j >= a->nr[prev]) {
					j = a->nr[prev] - 1;
				}

				j0 = j;

				do {
					sfp = (struct stats_filesystem *) ((char *) a->buf[prev] + j * a->msize);
					if (!strcmp(sfp->fs_name, sfc->fs_name)) {
						found = TRUE;
						break;
					}
					if (++j >= a->nr[prev]) {
						j = 0;
					}
				}
				while (j != j0);
			}
		}

		if (DISPLAY_ZERO_OMIT(flags) &&
		    found &&
		    !memcmp(sfp, sfc, STATS_FILESYSTEM_SIZE2CMP))
			continue;

		/* Compute metrics */
		mbfsfree = (unit < 0 ? (double) sfc->f_bfree / 1024 / 1024
				     : (double) sfc->f_bfree);
		mbfsused = (unit < 0 ? (double) (sfc->f_blocks - sfc->f_bfree) / 1024 / 1024
				     : (double) (sfc->f_blocks - sfc->f_bfree));
		/* f_blocks is not zero. But test it anyway ;-) */
		fsusedpct = (sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
					   : 0.0);
		ufsusedpct = (sfc->f_blocks ? SP_VALUE(sfc->f_bavail, sfc->f_blocks, sfc->f_blocks)
					    : 0.0);
		iusedpct = (sfc->f_files ? SP_VALUE(sfc->f_ffree, sfc->f_files, sfc->f_files)
					 : 0.0);

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			int k, pos;

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), dev_name, MAX_FS_LEN, &k);
			pos = k * a->xnr;

			if (!dispavg) {
				/* Save min and max values */
				save_minmax(a, pos, mbfsfree);
				save_minmax(a, pos + 1, mbfsused);
				save_minmax(a, pos + 2, fsusedpct);
				save_minmax(a, pos + 3, ufsusedpct);
				save_minmax(a, pos + 4, (double) sfc->f_ffree);
				save_minmax(a, pos + 5, (double) (sfc->f_files - sfc->f_ffree));
				save_minmax(a, pos + 6, iusedpct);
			}
			else {
				/* Display min and max values */
				print_hdr_line((dispavg ? _("Summary:") : timestamp[!curr]),
					       a, FIRST + DISPLAY_MOUNT(a->opt_flags), -1, 9, NULL);
				print_filesystem_xstats(H_MIN, unit, dev_name,
							a->spmin + pos);
				print_filesystem_xstats(H_MAX, unit, dev_name,
							a->spmax + pos);
			}
		}

		/*
		 * "Last:": This corresponds to the last values displayed (collected)
		 * for this filesystem (displayed as "Summary" when option -x not used).
		 */
		printf("%-11s", (dispavg ? (DISPLAY_MINMAX(flags) ? _("Last:")
								  : _("Summary:"))
					 : timestamp[curr]));
		cprintf_f(unit, FALSE, 2, 9, 0, mbfsfree, mbfsused);
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 2, 9, 2,
			    fsusedpct, ufsusedpct);
		cprintf_u64(NO_UNIT, 2, 9,
			    (unsigned long long) sfc->f_ffree,
			    (unsigned long long) (sfc->f_files - sfc->f_ffree));
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, iusedpct);
		cprintf_in(IS_STR, " %s\n", dev_name, 0);

		if (!dispavg) {
			/* Save current filesystem in summary list */
			for (j = 0; j < a->nr_allocated; j++) {
				sfm = (struct stats_filesystem *) ((char *) a->buf[2] + j * a->msize);

				if (!strcmp(sfm->fs_name, sfc->fs_name) ||
				    !sfm->f_blocks) {
					/*
					 * Filesystem found in list (then save again its stats)
					 * or free slot (end of list).
					 */
					*sfm = *sfc;
					if (j >= a->nr[2]) {
						a->nr[2] = j + 1;
					}
					break;
				}
			}
			if (j == a->nr_allocated) {
				/*
				 * No free slot has been found for current filesystem.
				 * So enlarge buffers then save filesystem in list.
				 */
				reallocate_all_buffers(a, j, flags);
				sfm = (struct stats_filesystem *) ((char *) a->buf[2] + j * a->msize);
				*sfm = *sfc;
				a->nr[2] = j + 1;
			}
		}
	}
}

/*
 ***************************************************************************
 * Display filesystems statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_filesystem_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	stub_print_filesystem_stats(a, prev, curr, FALSE);
}

/*
 ***************************************************************************
 * Display average filesystems statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_filesystem_stats(struct activity *a, int prev, int curr,
					   unsigned long long itv)
{
	stub_print_filesystem_stats(a, prev, 2, TRUE);
}

/*
 ***************************************************************************
 * Display Fibre Channel HBA statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_fchost_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	int i, j, j0, found;
	struct stats_fchost *sfcc, *sfcp, sfczero;
	int g_fields[] = {0, 1, 2, 3};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}
	memset(&sfczero, 0, sizeof(struct stats_fchost));

	if (dish && !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, -1, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		if (WANT_SINCE_BOOT(flags)) {
			sfcp = (struct stats_fchost *) ((char *) a->buf[prev]);
			found = TRUE;
		}
		else {
			found = FALSE;

			if (a->nr[prev] > 0) {
				/* Look for corresponding structure in previous iteration */
				j = i;

				if (j >= a->nr[prev]) {
					j = a->nr[prev] - 1;
				}

				j0 = j;

				do {
					sfcp = (struct stats_fchost *) ((char *) a->buf[prev] + j * a->msize);
					if (!strcmp(sfcc->fchost_name, sfcp->fchost_name)) {
						found = TRUE;
						break;
					}

					if (++j >= a->nr[prev]) {
						j = 0;
					}
				}
				while (j != j0);
			}
		}

		if (!found) {
			/* This is a newly registered host */
			sfcp = &sfczero;
		}

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			int k, pos;

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), sfcc->fchost_name, MAX_FCH_LEN, &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values */
				save_extrema(a->gtypes_nr, (void *) sfcc, (void *) sfcp,
					     itv, a->spmin + pos, a->spmax + pos, g_fields);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, -1, 9, NULL);
				print_fchost_xstats(H_MIN, sfcc->fchost_name, a->spmin + pos);
				print_fchost_xstats(H_MAX, sfcc->fchost_name, a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);

		cprintf_f(NO_UNIT, FALSE, 4, 9, 2,
			  S_VALUE(sfcp->f_rxframes, sfcc->f_rxframes, itv),
			  S_VALUE(sfcp->f_txframes, sfcc->f_txframes, itv),
			  S_VALUE(sfcp->f_rxwords,  sfcc->f_rxwords,  itv),
			  S_VALUE(sfcp->f_txwords,  sfcc->f_txwords,  itv));
		cprintf_in(IS_STR, " %s\n", sfcc->fchost_name, 0);
	}
}

/*
 ***************************************************************************
 * Display softnet statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 * @dispavg	True if displaying average statistics.
 ***************************************************************************
 */
__print_funct_t stub_print_softnet_stats(struct activity *a, int prev, int curr,
					 unsigned long long itv, int dispavg)
{
	int i;
	struct stats_softnet
		*ssnc = (struct stats_softnet *) a->buf[curr],
		*ssnp = (struct stats_softnet *) a->buf[prev];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	static __nr_t nr_alloc = 0;
	static unsigned long long *avg_blg_len = NULL;
	unsigned int local_types_nr[] = {0, 0, 5};
	int g_fields[] = {0, 1, 2, 3, 4};

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	if ((dish || DISPLAY_ZERO_OMIT(flags)) &&
	    !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
	}

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini
	 * (since @nr_ini counts up all CPU, even those offline).
	 * If this happens, it may be because the machine has been
	 * restarted with more CPU and no LINUX_RESTART has been
	 * inserted in file.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Allocate array for CPU backlog lengths */
	if (!avg_blg_len || (a->nr_ini > nr_alloc)) {
		SREALLOC(avg_blg_len, unsigned long long, sizeof(unsigned long long) * a->nr_ini);

		if (a->nr_ini > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_blg_len + nr_alloc, 0,
			       sizeof(unsigned long long) * (a->nr_ini - nr_alloc));
		}
		nr_alloc = a->nr_ini;
	}

	/* Compute statistics for CPU "all" */
	get_global_soft_statistics(a, prev, curr, flags, offline_cpu_bitmap);

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
                ssnc = (struct stats_softnet *) ((char *) a->buf[curr] + i * a->msize);
                ssnp = (struct stats_softnet *) ((char *) a->buf[prev] + i * a->msize);

		if (DISPLAY_ZERO_OMIT(flags) && !memcmp(ssnp, ssnc, STATS_SOFTNET_SIZE))
			continue;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			if (!dispavg) {
				/* Save min and max values */
				save_extrema(local_types_nr, (void *) ssnc, (void *) ssnp,
					     itv, a->spmin + i * a->xnr, a->spmax + i * a->xnr,
					     g_fields);

				/* Save min and max values for blg_len */
				save_minmax(a, i * a->xnr + 5, (double) ssnc->backlog_len);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, 7, 9, NULL);
				print_softnet_xstats(H_MIN, i, a->spmin + i * a->xnr);
				print_softnet_xstats(H_MAX, i, a->spmax + i * a->xnr);
			}
		}

		printf("%-11s", timestamp[curr]);

		if (!i) {
			/* This is CPU "all" */
			cprintf_in(IS_STR, " %s", "    all", 0);
		}
		else {
			cprintf_in(IS_INT, " %7d", "", i - 1);
		}

		cprintf_f(NO_UNIT, FALSE, 5, 9, 2,
			  S_VALUE(ssnp->processed,    ssnc->processed,    itv),
			  S_VALUE(ssnp->dropped,      ssnc->dropped,      itv),
			  S_VALUE(ssnp->time_squeeze, ssnc->time_squeeze, itv),
			  S_VALUE(ssnp->received_rps, ssnc->received_rps, itv),
			  S_VALUE(ssnp->flow_limit,   ssnc->flow_limit,   itv));

		if (!dispavg) {
			/* Display instantaneous value */
			cprintf_u64(NO_UNIT, 1, 9,
				    (unsigned long long) ssnc->backlog_len);

			/* Used to compute average value */
			avg_blg_len[i] += (unsigned long long ) ssnc->backlog_len;
		}
		else {
			/* Display average value */
			cprintf_f(NO_UNIT, FALSE, 1, 9, 0,
				  (double) avg_blg_len[i] / avg_count);
		}

		printf("\n");
	}

	if (dispavg && avg_blg_len) {
		free(avg_blg_len);
		avg_blg_len = NULL;
		nr_alloc = 0;
	}
}

/*
 ***************************************************************************
 * Display softnet statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_softnet_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	stub_print_softnet_stats(a, prev, curr, itv, FALSE);
}

/*
 ***************************************************************************
 * Display average softnet statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_softnet_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_softnet_stats(a, prev, curr, itv, TRUE);
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
void stub_print_psicpu_stats(struct activity *a, int prev, int curr, int dispavg,
			     unsigned long long itv)
{
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr],
		*psip = (struct stats_psi_cpu *) a->buf[prev];
	static unsigned long long
		s_avg10  = 0,
		s_avg60  = 0,
		s_avg300 = 0;
	double scpupct;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	/* Compute %scpu value */
	scpupct = ((double) psic->some_cpu_total - psip->some_cpu_total) / (100 * itv);

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_minmax(a, 0, (double) psic->some_acpu_10  / 100);
			save_minmax(a, 1, (double) psic->some_acpu_60  / 100);
			save_minmax(a, 2, (double) psic->some_acpu_300 / 100);
			save_minmax(a, 3, scpupct);
		}
		else {
			/* Print min and max values */
			print_psi_xstats(H_MIN, a->xnr, a->spmin);
			print_psi_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) psic->some_acpu_10  / 100,
			   (double) psic->some_acpu_60  / 100,
			   (double) psic->some_acpu_300 / 100);

		/* Will be used to compute the average */
		s_avg10  += psic->some_acpu_10;
		s_avg60  += psic->some_acpu_60;
		s_avg300 += psic->some_acpu_300;
	}
	else {
		/* Display average values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) s_avg10  / (avg_count * 100),
			   (double) s_avg60  / (avg_count * 100),
			   (double) s_avg300 / (avg_count * 100));

		/* Reset average counters */
		s_avg10 = s_avg60 = s_avg300 = 0;
	}

	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, scpupct);
	printf("\n");
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_psicpu_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	stub_print_psicpu_stats(a, prev, curr, FALSE, itv);
}

/*
 ***************************************************************************
 * Display average pressure-stall CPU statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_psicpu_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	stub_print_psicpu_stats(a, prev, curr, TRUE, itv);
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
void stub_print_psiio_stats(struct activity *a, int prev, int curr, int dispavg,
			    unsigned long long itv)
{
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr],
		*psip = (struct stats_psi_io *) a->buf[prev];
	static unsigned long long
		s_avg10  = 0,
		s_avg60  = 0,
		s_avg300 = 0,
		f_avg10  = 0,
		f_avg60  = 0,
		f_avg300 = 0;
	double siopct, fiopct;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	/* Compute %sio and %fio values */
	siopct = ((double) psic->some_io_total - psip->some_io_total) / (100 * itv);
	fiopct = ((double) psic->full_io_total - psip->full_io_total) / (100 * itv);

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_minmax(a, 0, (double) psic->some_aio_10  / 100);
			save_minmax(a, 1, (double) psic->some_aio_60  / 100);
			save_minmax(a, 2, (double) psic->some_aio_300 / 100);
			save_minmax(a, 3, siopct);
			save_minmax(a, 4, (double) psic->full_aio_10  / 100);
			save_minmax(a, 5, (double) psic->full_aio_60  / 100);
			save_minmax(a, 6, (double) psic->full_aio_300 / 100);
			save_minmax(a, 7, fiopct);
		}
		else {
			/* Print min and max values */
			print_psi_xstats(H_MIN, a->xnr, a->spmin);
			print_psi_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous "some" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) psic->some_aio_10  / 100,
			   (double) psic->some_aio_60  / 100,
			   (double) psic->some_aio_300 / 100);

		/* Will be used to compute the average */
		s_avg10  += psic->some_aio_10;
		s_avg60  += psic->some_aio_60;
		s_avg300 += psic->some_aio_300;
	}
	else {
		/* Display average "some" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) s_avg10  / (avg_count * 100),
			   (double) s_avg60  / (avg_count * 100),
			   (double) s_avg300 / (avg_count * 100));

		/* Reset average counters */
		s_avg10 = s_avg60 = s_avg300 = 0;
	}

	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, siopct);

	if (!dispavg) {
		/* Display instantaneous "full" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) psic->full_aio_10  / 100,
			   (double) psic->full_aio_60  / 100,
			   (double) psic->full_aio_300 / 100);

		/* Will be used to compute the average */
		f_avg10  += psic->full_aio_10;
		f_avg60  += psic->full_aio_60;
		f_avg300 += psic->full_aio_300;
	}
	else {
		/* Display average "full" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) f_avg10  / (avg_count * 100),
			   (double) f_avg60  / (avg_count * 100),
			   (double) f_avg300 / (avg_count * 100));

		/* Reset average counters */
		f_avg10 = f_avg60 = f_avg300 = 0;
	}

	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, fiopct);
	printf("\n");
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_psiio_stats(struct activity *a, int prev, int curr,
				  unsigned long long itv)
{
	stub_print_psiio_stats(a, prev, curr, FALSE, itv);
}

/*
 ***************************************************************************
 * Display average pressure-stall I/O statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_psiio_stats(struct activity *a, int prev, int curr,
				      unsigned long long itv)
{
	stub_print_psiio_stats(a, prev, curr, TRUE, itv);
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dispavg	TRUE if displaying average statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
void stub_print_psimem_stats(struct activity *a, int prev, int curr, int dispavg,
			     unsigned long long itv)
{
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr],
		*psip = (struct stats_psi_mem *) a->buf[prev];
	static unsigned long long
		s_avg10  = 0,
		s_avg60  = 0,
		s_avg300 = 0,
		f_avg10  = 0,
		f_avg60  = 0,
		f_avg300 = 0;
	double smempct, fmempct;

	if (xinit) {
		/* Init min and max values */
		init_extrema_values(a, a->xnr);
	}
	if (dish || ((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	/* Compute %smem and %fmem values */
	smempct = ((double) psic->some_mem_total - psip->some_mem_total) / (100 * itv);
	fmempct = ((double) psic->full_mem_total - psip->full_mem_total) / (100 * itv);

	if (DISPLAY_MINMAX(flags)) {
		if (prev != 2) {
			/* Save min and max values */
			save_minmax(a, 0, (double) psic->some_amem_10  / 100);
			save_minmax(a, 1, (double) psic->some_amem_60  / 100);
			save_minmax(a, 2, (double) psic->some_amem_300 / 100);
			save_minmax(a, 3, smempct);
			save_minmax(a, 4, (double) psic->full_amem_10  / 100);
			save_minmax(a, 5, (double) psic->full_amem_60  / 100);
			save_minmax(a, 6, (double) psic->full_amem_300 / 100);
			save_minmax(a, 7, fmempct);
		}
		else {
			/* Print min and max values */
			print_psi_xstats(H_MIN, a->xnr, a->spmin);
			print_psi_xstats(H_MAX, a->xnr, a->spmax);
		}
	}

	printf("%-11s", timestamp[curr]);

	if (!dispavg) {
		/* Display instantaneous "some" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) psic->some_amem_10  / 100,
			   (double) psic->some_amem_60  / 100,
			   (double) psic->some_amem_300 / 100);

		/* Will be used to compute the average */
		s_avg10  += psic->some_amem_10;
		s_avg60  += psic->some_amem_60;
		s_avg300 += psic->some_amem_300;
	}
	else {
		/* Display average "some" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) s_avg10  / (avg_count * 100),
			   (double) s_avg60  / (avg_count * 100),
			   (double) s_avg300 / (avg_count * 100));

		/* Reset average counters */
		s_avg10 = s_avg60 = s_avg300 = 0;
	}

	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, smempct);

	if (!dispavg) {
		/* Display instantaneous "full" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) psic->full_amem_10  / 100,
			   (double) psic->full_amem_60  / 100,
			   (double) psic->full_amem_300 / 100);

		/* Will be used to compute the average */
		f_avg10  += psic->full_amem_10;
		f_avg60  += psic->full_amem_60;
		f_avg300 += psic->full_amem_300;
	}
	else {
		/* Display average "full" values */
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 3, 9, 2,
			   (double) f_avg10  / (avg_count * 100),
			   (double) f_avg60  / (avg_count * 100),
			   (double) f_avg300 / (avg_count * 100));

		/* Reset average counters */
		f_avg10 = f_avg60 = f_avg300 = 0;
	}

	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, fmempct);
	printf("\n");
}

/*
 ***************************************************************************
 * Display pressure-stall memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_psimem_stats(struct activity *a, int prev, int curr,
				   unsigned long long itv)
{
	stub_print_psimem_stats(a, prev, curr, FALSE, itv);
}

/*
 ***************************************************************************
 * Display average pressure-stall memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_psimem_stats(struct activity *a, int prev, int curr,
				       unsigned long long itv)
{
	stub_print_psimem_stats(a, prev, curr, TRUE, itv);
}

/*
 * **************************************************************************
 * Display battery statistics. This function is used to display
 * instantaneous and average statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dispavg	True if displaying average statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
void stub_print_pwr_bat_stats(struct activity *a, int prev, int curr, int dispavg,
			      unsigned long long itv)
{
	int i;
	struct stats_pwr_bat *spbc, *spbp;
	static __nr_t nr_alloc = 0;
	static unsigned long *avg_bat_cap = NULL;
	double capmin;

	if (xinit && a->nr_allocated) {
		/*
		 * Init min and max values.
		 * Used only when reading from a file: Init happens when there is a
		 * LINUX RESTART message in file. The min and max values are those for
		 * the statistics located between two LINUX RESTART messages.
		 */
		init_extrema_values(a, a->nr_allocated * a->xnr);
	}

	/* Allocate arrays of battery capacities */
	if (!avg_bat_cap || (a->nr[curr] > nr_alloc)) {
		SREALLOC(avg_bat_cap, unsigned long, sizeof(unsigned long) * a->nr[curr]);

		if (a->nr[curr] > nr_alloc) {
			/* Init additional space allocated */
			memset(avg_bat_cap + nr_alloc, 0,
			       sizeof(double) * (a->nr[curr] - nr_alloc));
		}
		nr_alloc = a->nr[curr];
	}

	if (dish && !((prev == 2) && DISPLAY_MINMAX(flags))) {
		print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
	}

	for (i = 0; i < a->nr[curr]; i++) {
		spbc = (struct stats_pwr_bat *) ((char *) a->buf[curr] + i * a->msize);
		spbp = (struct stats_pwr_bat *) ((char *) a->buf[prev] + i * a->msize);

		/* Compute cap/min value */
		capmin = (double) (spbc->capacity - spbp->capacity) * 6000 / itv;

		/* Check if min and max values should be displayed */
		if (DISPLAY_MINMAX(flags)) {
			char name[16];
			int k, pos;

			snprintf(name, sizeof(name), "%d", spbc->bat_id);
			name[sizeof(name) - 1] = '\0';

			/* Look for item in list or add it to the list if non existent */
			add_list_item(&(a->xdev_list), name, sizeof(name), &k);
			pos = k * a->xnr;

			if (prev != 2) {
				/* Save min and max values for %ifutil */
				save_minmax(a, pos, (double) spbc->capacity);
				save_minmax(a, pos + 1, capmin);
			}
			else {
				/* Display min and max values */
				print_hdr_line(timestamp[!curr], a, FIRST, 0, 9, NULL);
				print_pwr_bat_xstats(H_MIN, name, a->spmin + pos);
				print_pwr_bat_xstats(H_MAX, name, a->spmax + pos);
			}
		}

		printf("%-11s", timestamp[curr]);
		cprintf_in(IS_INT, "     %5d", "", (int) spbc->bat_id);

		if (dispavg) {
			/* Display average values */
			cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 2,
				    (double) avg_bat_cap[i] / avg_count);
		}
		else {
			/* Display instantaneous values */
			cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 0,
				    (double) spbc->capacity);
			avg_bat_cap[i] += (unsigned int) spbc->capacity;
		}
		cprintf_f(NO_UNIT, TRUE, 1, 9, 2, capmin);

		if (!dispavg) {
			/* Print battery status */
			switch (spbc->status) {

				case BAT_STS_CHARGING:
					/* Unicode for North East Arrow */
					cprintf_tr(TRUE, " %11s", "\U00002197");
					break;

				case BAT_STS_DISCHARGING:
					/* Unicode for South East Arrow */
					cprintf_tr(FALSE, " %11s", "\U00002198");
					break;

				case BAT_STS_NOTCHARGING:
					/* Unicode for East Arrow */
					cprintf_tr(FALSE, " %11s", "\U00002192");
					break;

				case BAT_STS_FULL:
					/* Unicode for North Arrow */
					cprintf_tr(TRUE, " %11s", "\U00002191");
					break;

				default:
					printf(" %9s", "?");
			}
		}
		printf("\n");
	}

	if (dispavg && avg_bat_cap) {
		free(avg_bat_cap);
		avg_bat_cap = NULL;
		nr_alloc = 0;
	}
}

/*
 * **************************************************************************
 * Display battery statistics.fan
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_pwr_bat_stats(struct activity *a, int prev, int curr,
				    unsigned long long itv)
{
	stub_print_pwr_bat_stats(a, prev, curr, FALSE, itv);
}

/*
 * **************************************************************************
 * Display average baterry statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time in 1/100th of a second.
 ***************************************************************************
 */
__print_funct_t print_avg_pwr_bat_stats(struct activity *a, int prev, int curr,
					unsigned long long itv)
{
	stub_print_pwr_bat_stats(a, prev, curr, TRUE, itv);
}
