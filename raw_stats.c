/*
 * raw_stats.c: Functions used by sar to display statistics in raw format.
 * (C) 1999-2017 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include "raw_stats.h"

extern unsigned int flags;


/*
 ***************************************************************************
 * Display current field name.
 *
 * IN:
 * @hdr_line	On the first call, complete header line, containing all the
 *		metric names. In each subsequent call, must be NULL.
 * @pos		Index in @hdr_line string, 0 being the first one (headers
 * 		are delimited by the '|' character).
 *
 * RETURNS:
 * Pointer on string containing field name.
 ***************************************************************************
 */
char *pfield(char *hdr_line, int pos)
{
	char hline[HEADER_LINE_LEN] = "";
	static char field[HEADER_LINE_LEN] = "";
	static int idx = 0;
	char *hl;
	int i, j = 0;

	if (hdr_line) {
		strncpy(hline, hdr_line, HEADER_LINE_LEN - 1);
		hline[HEADER_LINE_LEN - 1] = '\0';
		idx = 0;

		for (hl = strtok(hline, "|"); hl && (pos > 0); hl = strtok(NULL, "|"), pos--);
		if (!hl) {
			/* Bad @pos arg given to function */
			strcpy(field, "");
			return field;
		}
		if (strchr(hl, '&')) {
			j = strcspn(hl, "&");
			*(hl + j) = ';';
		}
		strcpy(field, hl);
	}

	/* Display current field */
	if (strchr(field + idx, ';')) {
		j = strcspn(field + idx, ";");
		*(field + idx + j) = '\0';
	}
	i = idx;
	idx += j + 1;

	return field + i;
}

/*
 ***************************************************************************
 * Display field values.
 *
 * IN:
 * @valp	Field's value from previous statistics sample.
 * @valc	Field's value from current statistics sample.
 ***************************************************************************
 */
void pval(unsigned long long valp, unsigned long long valc)
{
	printf("%llu>%llu", valp, valc);
	if (DISPLAY_HINTS(flags)) {
		if (valc < valp) {
			/* Field's value has decreased */
			printf(" [DEC]");
		}
	}
}

/*
 ***************************************************************************
 * Display CPU statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current statistics sample.
 ***************************************************************************
 */
__print_funct_t raw_print_cpu_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_cpu *scc, *scp;

	for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_cpu) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * scc = (struct stats_cpu *) a->buf[...] + i;
		 */
		scc = (struct stats_cpu *) ((char *) a->buf[curr] + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		/*
		 * Note: a->nr is in [1, NR_CPUS + 1].
		 * Bitmap size is provided for (NR_CPUS + 1) CPUs.
		 * Anyway, NR_CPUS may vary between the version of sysstat
		 * used by sadc to create a file, and the version of sysstat
		 * used by sar to read it...
		 */

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		/* Yes: Display it */
		printf("%s %s:%d", timestr,
		       pfield(a->hdr_line, DISPLAY_CPU_ALL(a->opt_flags)), i - 1);

		if (DISPLAY_HINTS(flags) && i) {
			if ((scc->cpu_user + scc->cpu_nice + scc->cpu_sys +
			     scc->cpu_iowait + scc->cpu_idle + scc->cpu_steal +
			     scc->cpu_hardirq + scc->cpu_softirq) == 0) {
				/* CPU is offline */
				printf(" [OFF]");
			}
			else {
				if (!get_per_cpu_interval(scc, scp)) {
					/* CPU is tickless */
					printf(" [TLS]");
				}
			}
		}

		if (DISPLAY_CPU_DEF(a->opt_flags)) {
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_user, scc->cpu_user);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_nice, scc->cpu_nice);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
			     scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_iowait, scc->cpu_iowait);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_steal, scc->cpu_steal);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_idle, scc->cpu_idle);
		}
		else if (DISPLAY_CPU_ALL(a->opt_flags)) {
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_user - scp->cpu_guest, scc->cpu_user - scc->cpu_guest);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_nice - scp->cpu_guest_nice, scc->cpu_nice - scc->cpu_guest_nice);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_sys, scc->cpu_sys);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_iowait, scc->cpu_iowait);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_steal, scc->cpu_steal);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_hardirq, scc->cpu_hardirq);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_softirq, scc->cpu_softirq);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_guest, scc->cpu_guest);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_guest_nice, scc->cpu_guest_nice);
			printf(" %s:", pfield(NULL, 1));
			pval(scp->cpu_idle, scc->cpu_idle);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display tasks creation and context switches statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pcsw_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];

	printf("%s %s:", timestr, pfield(a->hdr_line, 0));
	pval(spp->processes, spc->processes);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->context_switch, spc->context_switch);
	printf("\n");
}

/*
 ***************************************************************************
 * Display interrupts statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_irq_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_irq *sic, *sip;

	for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

		sic = (struct stats_irq *) ((char *) a->buf[curr]  + i * a->msize);
		sip = (struct stats_irq *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			/* Yes: Display it */
			printf("%s %s:%d", timestr,
			       pfield(a->hdr_line, 0), i - 1);
			printf(" %s:", pfield(NULL, 0));
			pval(sip->irq_nr, sic->irq_nr);
			printf("\n");
		}
	}
}

/*
 ***************************************************************************
 * Display swapping statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_swap_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[!curr];

	printf("%s %s:", timestr, pfield(a->hdr_line, 0));
	pval(ssp->pswpin, ssc->pswpin);
	printf(" %s:", pfield(NULL, 0));
	pval(ssp->pswpout, ssc->pswpout);
	printf("\n");
}

/*
 ***************************************************************************
 * Display paging statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_paging_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];

	printf("%s %s:", timestr, pfield(a->hdr_line, 0));
	pval(spp->pgpgin, spc->pgpgin);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgpgout, spc->pgpgout);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgfault, spc->pgfault);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgmajfault, spc->pgmajfault);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgfree, spc->pgfree);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgscan_kswapd, spc->pgscan_kswapd);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgscan_direct, spc->pgscan_direct);
	printf(" %s:", pfield(NULL, 0));
	pval(spp->pgsteal, spc->pgsteal);
	printf("\n");
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_io_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[!curr];

	printf("%s %s:", timestr, pfield(a->hdr_line, 0));
	pval(sip->dk_drive, sic->dk_drive);
	printf(" %s:", pfield(NULL, 0));
	pval(sip->dk_drive_rio, sic->dk_drive_rio);
	printf(" %s:", pfield(NULL, 0));
	pval(sip->dk_drive_wio, sic->dk_drive_wio);
	printf(" %s:", pfield(NULL, 0));
	pval(sip->dk_drive_rblk, sic->dk_drive_rblk);
	printf(" %s:", pfield(NULL, 0));
	pval(sip->dk_drive_wblk, sic->dk_drive_wblk);
	printf("\n");
}

/*
 ***************************************************************************
 * Display memory statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_memory_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	if (DISPLAY_MEM_AMT(a->opt_flags)) {
		printf("%s %s:%lu", timestr, pfield(a->hdr_line, SECOND), smc->frmkb);
		printf(" %s:%lu", pfield(NULL, 0), smc->availablekb);
		printf(" kbttlmem:%lu", smc->tlmkb);
		pfield(NULL, 0); /* Skip kbmemused */
		pfield(NULL, 0); /* Skip %memused */
		printf(" %s:%lu", pfield(NULL, 0), smc->bufkb);
		printf(" %s:%lu", pfield(NULL, 0), smc->camkb);
		printf(" %s:%lu", pfield(NULL, 0), smc->comkb);
		pfield(NULL, 0); /* Skip %commit */
		printf(" %s:%lu", pfield(NULL, 0), smc->activekb);
		printf(" %s:%lu", pfield(NULL, 0), smc->inactkb);
		printf(" %s:%lu", pfield(NULL, 0), smc->dirtykb);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {
			printf(" %s:%lu", pfield(NULL, 0), smc->anonpgkb);
			printf(" %s:%lu", pfield(NULL, 0), smc->slabkb);
			printf(" %s:%lu", pfield(NULL, 0), smc->kstackkb);
			printf(" %s:%lu", pfield(NULL, 0), smc->pgtblkb);
			printf(" %s:%lu", pfield(NULL, 0), smc->vmusedkb);
		}
		printf("\n");
	}

	if (DISPLAY_SWAP(a->opt_flags)) {
		printf("%s %s:%lu", timestr, pfield(a->hdr_line, THIRD), smc->frskb);
		printf(" kbttlswp:%lu", smc->tlskb);
		pfield(NULL, 0); /* Skip kbswpused */
		pfield(NULL, 0); /* Skip %swpused */
		printf(" %s:%lu", pfield(NULL, 0), smc->caskb);
		printf("\n");
	}
}
