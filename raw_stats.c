/*
 * raw_stats.c: Functions used by sar to display statistics in raw format.
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
		strncpy(hline, hdr_line, sizeof(hline) - 1);
		hline[sizeof(hline) - 1] = '\0';
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
		strncpy(field, hl, sizeof(field));
		field[sizeof(field) - 1] = '\0';
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
	if (DISPLAY_DEBUG_MODE(flags)) {
		if (valc < valp) {
			/* Field's value has decreased */
			printf(" [DEC]");
		}
	}
	printf("; %llu; %llu;", valp, valc);
}

/*
 ***************************************************************************
 * Display CPU statistics in raw format.
 * Note: Values displayed for CPU "all" may slightly differ from those you
 * would get if you were displaying them in pr_stats.c:print_cpu_stats().
 * This is because values for CPU "all" are recalculated there as the sum of
 * all individual CPU values (done by a call to get_global_cpu_statistics()
 * function).
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

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_cpu) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * scc = (struct stats_cpu *) a->buf[...] + i;
		 */
		scc = (struct stats_cpu *) ((char *) a->buf[curr] + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		/* Yes: Display it */
		printf("%s; %s", timestr, pfield(a->hdr_line, DISPLAY_CPU_ALL(a->opt_flags)));
		if (DISPLAY_DEBUG_MODE(flags) && i) {
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
		printf("; %d;", i - 1);

		if (DISPLAY_CPU_DEF(a->opt_flags)) {
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_user, scc->cpu_user);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_nice, scc->cpu_nice);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
			     scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_iowait, scc->cpu_iowait);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_steal, scc->cpu_steal);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_idle, scc->cpu_idle);
		}
		else if (DISPLAY_CPU_ALL(a->opt_flags)) {
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_user - scp->cpu_guest, scc->cpu_user - scc->cpu_guest);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_nice - scp->cpu_guest_nice, scc->cpu_nice - scc->cpu_guest_nice);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_sys, scc->cpu_sys);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_iowait, scc->cpu_iowait);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_steal, scc->cpu_steal);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_hardirq, scc->cpu_hardirq);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_softirq, scc->cpu_softirq);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_guest, scc->cpu_guest);
			printf(" %s", pfield(NULL, 0));
			pval(scp->cpu_guest_nice, scc->cpu_guest_nice);
			printf(" %s", pfield(NULL, 0));
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

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) spp->processes, (unsigned long long) spc->processes);
	printf(" %s", pfield(NULL, 0));
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

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		sic = (struct stats_irq *) ((char *) a->buf[curr]  + i * a->msize);
		sip = (struct stats_irq *) ((char *) a->buf[!curr] + i * a->msize);

		/* Should current interrupt (including int "sum") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {

			/* Yes: Display it */
			printf("%s; %s; %d;", timestr,
			       pfield(a->hdr_line, FIRST), i - 1);
			printf(" %s", pfield(NULL, 0));
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

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) ssp->pswpin, (unsigned long long) ssc->pswpin);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) ssp->pswpout, (unsigned long long) ssc->pswpout);
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

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) spp->pgpgin, (unsigned long long) spc->pgpgin);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgpgout, (unsigned long long) spc->pgpgout);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgfault, (unsigned long long) spc->pgfault);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgmajfault, (unsigned long long) spc->pgmajfault);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgfree, (unsigned long long) spc->pgfree);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgscan_kswapd, (unsigned long long) spc->pgscan_kswapd);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgscan_direct, (unsigned long long) spc->pgscan_direct);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) spp->pgsteal, (unsigned long long) spc->pgsteal);
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

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval(sip->dk_drive, sic->dk_drive);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_rio, sic->dk_drive_rio);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_wio, sic->dk_drive_wio);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_dio, sic->dk_drive_dio);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_rblk, sic->dk_drive_rblk);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_wblk, sic->dk_drive_wblk);
	printf(" %s", pfield(NULL, 0));
	pval(sip->dk_drive_dblk, sic->dk_drive_dblk);
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

	if (DISPLAY_MEMORY(a->opt_flags)) {
		printf("%s; %s; %llu;", timestr, pfield(a->hdr_line, FIRST), smc->frmkb);
		printf(" %s; %llu;", pfield(NULL, 0), smc->availablekb);
		printf(" kbttlmem; %llu;", smc->tlmkb);
		pfield(NULL, 0); /* Skip kbmemused */
		pfield(NULL, 0); /* Skip %memused */
		printf(" %s; %llu;", pfield(NULL, 0), smc->bufkb);
		printf(" %s; %llu;", pfield(NULL, 0), smc->camkb);
		printf(" %s; %llu;", pfield(NULL, 0), smc->comkb);
		pfield(NULL, 0); /* Skip %commit */
		printf(" %s; %llu;", pfield(NULL, 0), smc->activekb);
		printf(" %s; %llu;", pfield(NULL, 0), smc->inactkb);
		printf(" %s; %llu;", pfield(NULL, 0), smc->dirtykb);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {
			printf(" %s; %llu;", pfield(NULL, 0), smc->anonpgkb);
			printf(" %s; %llu;", pfield(NULL, 0), smc->slabkb);
			printf(" %s; %llu;", pfield(NULL, 0), smc->kstackkb);
			printf(" %s; %llu;", pfield(NULL, 0), smc->pgtblkb);
			printf(" %s; %llu;", pfield(NULL, 0), smc->vmusedkb);
		}
		printf("\n");
	}

	if (DISPLAY_SWAP(a->opt_flags)) {
		printf("%s; %s; %llu;", timestr, pfield(a->hdr_line, SECOND), smc->frskb);
		printf(" kbttlswp; %llu;", smc->tlskb);
		pfield(NULL, 0); /* Skip kbswpused */
		pfield(NULL, 0); /* Skip %swpused */
		printf(" %s; %llu;", pfield(NULL, 0), smc->caskb);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display kernel tables statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_ktables_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	printf("%s; %s; %llu;", timestr, pfield(a->hdr_line, FIRST), skc->dentry_stat);
	printf(" %s; %llu;", pfield(NULL, 0), skc->file_used);
	printf(" %s; %llu;", pfield(NULL, 0), skc->inode_used);
	printf(" %s; %llu;", pfield(NULL, 0), skc->pty_nr);
	printf("\n");
}

/*
 ***************************************************************************
 * Display queue and load statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_queue_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	printf("%s; %s; %llu;", timestr, pfield(a->hdr_line, FIRST), sqc->nr_running);
	printf(" %s; %llu;", pfield(NULL, 0), sqc->nr_threads);
	printf(" %s; %u;", pfield(NULL, 0), sqc->load_avg_1);
	printf(" %s; %u;", pfield(NULL, 0), sqc->load_avg_5);
	printf(" %s; %u;", pfield(NULL, 0), sqc->load_avg_15);
	printf(" %s; %llu;", pfield(NULL, 0), sqc->procs_blocked);
	printf("\n");
}

/*
 ***************************************************************************
 * Display serial lines statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_serial_stats(struct activity *a, char *timestr, int curr)
{
	int i, j, j0, found;
	struct stats_serial *ssc, *ssp;

	for (i = 0; i < a->nr[curr]; i++) {

		found = FALSE;
		ssc = (struct stats_serial *) ((char *) a->buf[curr]  + i * a->msize);

		if (a->nr[!curr] > 0) {

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

		printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
		if (!found && DISPLAY_DEBUG_MODE(flags)) {
			printf(" [NEW]");
		}
		printf("; %u;", ssc->line);
		if (!found) {
			printf("\n");
			continue;
		}

		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->rx, (unsigned long long)ssc->rx);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->tx, (unsigned long long) ssc->tx);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->frame, (unsigned long long) ssc->frame);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->parity, (unsigned long long) ssc->parity);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->brk, (unsigned long long) ssc->brk);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssp->overrun, (unsigned long long) ssc->overrun);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display disks statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_disk_stats(struct activity *a, char *timestr, int curr)
{
	int i, j;
	struct stats_disk *sdc,	*sdp, sdpzero;
	char *dev_name;

	memset(&sdpzero, 0, STATS_DISK_SIZE);

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

		printf("%s; major; %u; minor; %u; %s",
		       timestr, sdc->major, sdc->minor, pfield(a->hdr_line, FIRST));

		j = check_disk_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sdp = &sdpzero;
			if (DISPLAY_DEBUG_MODE(flags)) {
				printf(" [%s]", j == -1 ? "NEW" : "BCK");
			}
		}
		else {
			sdp = (struct stats_disk *) ((char *) a->buf[!curr] + j * a->msize);
		}

		printf("; %s;", dev_name);
		printf(" %s", pfield(NULL, 0));
		pval(sdp->nr_ios, sdc->nr_ios);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sdp->rd_sect, (unsigned long long) sdc->rd_sect);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sdp->wr_sect, (unsigned long long) sdc->wr_sect);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sdp->dc_sect, (unsigned long long) sdc->dc_sect);
		printf(" rd_ticks");
		pval((unsigned long long) sdp->rd_ticks, (unsigned long long) sdc->rd_ticks);
		printf(" wr_ticks");
		pval((unsigned long long) sdp->wr_ticks, (unsigned long long) sdc->wr_ticks);
		printf(" dc_ticks");
		pval((unsigned long long) sdp->dc_ticks, (unsigned long long) sdc->dc_ticks);
		printf(" tot_ticks");
		pval((unsigned long long) sdp->tot_ticks, (unsigned long long) sdc->tot_ticks);
		pfield(NULL, 0); /* Skip areq-sz */
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sdp->rq_ticks, (unsigned long long) sdc->rq_ticks);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display network interfaces statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_dev_stats(struct activity *a, char *timestr, int curr)
{
	int i, j;
	struct stats_net_dev *sndc, *sndp, sndzero;

	memset(&sndzero, 0, STATS_NET_DEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
		j = check_net_dev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sndp = &sndzero;
			if (DISPLAY_DEBUG_MODE(flags)) {
				printf(" [%s]", j == -1 ? "NEW" : "BCK");
			}
		}
		else {
			sndp = (struct stats_net_dev *) ((char *) a->buf[!curr] + j * a->msize);
		}
		printf("; %s;", sndc->interface);

		printf(" %s", pfield(NULL, 0));
		pval(sndp->rx_packets, sndc->rx_packets);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->tx_packets, sndc->tx_packets);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->rx_bytes, sndc->rx_bytes);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->tx_bytes, sndc->tx_bytes);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->rx_compressed, sndc->rx_compressed);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->tx_compressed, sndc->tx_compressed);
		printf(" %s", pfield(NULL, 0));
		pval(sndp->multicast, sndc->multicast);
		printf(" speed; %u; duplex; %u;\n", sndc->speed, sndc->duplex);
	}
}

/*
 ***************************************************************************
 * Display network interfaces errors statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_edev_stats(struct activity *a, char *timestr, int curr)
{
	int i, j;
	struct stats_net_edev *snedc, *snedp, snedzero;

	memset(&snedzero, 0, STATS_NET_EDEV_SIZE);

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
		j = check_net_edev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			snedp = &snedzero;
			if (DISPLAY_DEBUG_MODE(flags)) {
				printf(" [%s]", j == -1 ? "NEW" : "BCK");
			}
		}
		else {
			snedp = (struct stats_net_edev *) ((char *) a->buf[!curr] + j * a->msize);
		}
		printf("; %s;", snedc->interface);

		printf(" %s", pfield(NULL, 0));
		pval(snedp->rx_errors, snedc->rx_errors);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->tx_errors, snedc->tx_errors);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->collisions, snedc->collisions);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->rx_dropped, snedc->rx_dropped);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->tx_dropped, snedc->tx_dropped);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->tx_carrier_errors, snedc->tx_carrier_errors);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->rx_frame_errors, snedc->rx_frame_errors);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->rx_fifo_errors, snedc->rx_fifo_errors);
		printf(" %s", pfield(NULL, 0));
		pval(snedp->tx_fifo_errors, snedc->tx_fifo_errors);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display NFS client statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_nfs_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snnp->nfs_rpccnt, (unsigned long long) snnc->nfs_rpccnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snnp->nfs_rpcretrans, (unsigned long long) snnc->nfs_rpcretrans);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snnp->nfs_readcnt, (unsigned long long) snnc->nfs_readcnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snnp->nfs_writecnt, (unsigned long long) snnc->nfs_writecnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snnp->nfs_accesscnt, (unsigned long long) snnc->nfs_accesscnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snnp->nfs_getattcnt, (unsigned long long) snnc->nfs_getattcnt);
	printf("\n");
}

/*
 ***************************************************************************
 * Display NFS server statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_nfsd_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr],
		*snndp = (struct stats_net_nfsd *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snndp->nfsd_rpccnt, (unsigned long long) snndc->nfsd_rpccnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_rpcbad, (unsigned long long) snndc->nfsd_rpcbad);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_netcnt, (unsigned long long) snndc->nfsd_netcnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_netudpcnt, (unsigned long long) snndc->nfsd_netudpcnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_nettcpcnt, (unsigned long long) snndc->nfsd_nettcpcnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_rchits, (unsigned long long) snndc->nfsd_rchits);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_rcmisses, (unsigned long long) snndc->nfsd_rcmisses);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_readcnt, (unsigned long long) snndc->nfsd_readcnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_writecnt, (unsigned long long) snndc->nfsd_writecnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_accesscnt, (unsigned long long) snndc->nfsd_accesscnt);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snndp->nfsd_getattcnt, (unsigned long long) snndc->nfsd_getattcnt);
	printf("\n");
}

/*
 ***************************************************************************
 * Display network socket statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_sock_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	printf("%s; %s; %u;", timestr, pfield(a->hdr_line, FIRST), snsc->sock_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->tcp_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->udp_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->raw_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->frag_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->tcp_tw);
	printf("\n");
}

/*
 ***************************************************************************
 * Display IP network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_ip_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval(snip->InReceives, snic->InReceives);
	printf(" %s", pfield(NULL, 0));
	pval(snip->ForwDatagrams, snic->ForwDatagrams);
	printf(" %s", pfield(NULL, 0));
	pval(snip->InDelivers, snic->InDelivers);
	printf(" %s", pfield(NULL, 0));
	pval(snip->OutRequests, snic->OutRequests);
	printf(" %s", pfield(NULL, 0));
	pval(snip->ReasmReqds, snic->ReasmReqds);
	printf(" %s", pfield(NULL, 0));
	pval(snip->ReasmOKs, snic->ReasmOKs);
	printf(" %s", pfield(NULL, 0));
	pval(snip->FragOKs, snic->FragOKs);
	printf(" %s", pfield(NULL, 0));
	pval(snip->FragCreates, snic->FragCreates);
	printf("\n");
}

/*
 ***************************************************************************
 * Display IP network errors statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_eip_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval(sneip->InHdrErrors, sneic->InHdrErrors);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InAddrErrors, sneic->InAddrErrors);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InUnknownProtos, sneic->InUnknownProtos);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InDiscards, sneic->InDiscards);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->OutDiscards, sneic->OutDiscards);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->OutNoRoutes, sneic->OutNoRoutes);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->ReasmFails, sneic->ReasmFails);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->FragFails, sneic->FragFails);
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMP network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_icmp_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snip->InMsgs, (unsigned long long) snic->InMsgs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutMsgs, (unsigned long long) snic->OutMsgs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InEchos, (unsigned long long) snic->InEchos);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InEchoReps, (unsigned long long) snic->InEchoReps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutEchos, (unsigned long long) snic->OutEchos);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutEchoReps, (unsigned long long) snic->OutEchoReps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InTimestamps, (unsigned long long) snic->InTimestamps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InTimestampReps, (unsigned long long) snic->InTimestampReps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutTimestamps, (unsigned long long) snic->OutTimestamps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutTimestampReps, (unsigned long long) snic->OutTimestampReps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InAddrMasks, (unsigned long long) snic->InAddrMasks);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InAddrMaskReps, (unsigned long long) snic->InAddrMaskReps);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutAddrMasks, (unsigned long long) snic->OutAddrMasks);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutAddrMaskReps, (unsigned long long) snic->OutAddrMaskReps);
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMP errors message statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_eicmp_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) sneip->InErrors, (unsigned long long) sneic->InErrors);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutErrors, (unsigned long long) sneic->OutErrors);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InDestUnreachs, (unsigned long long) sneic->InDestUnreachs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutDestUnreachs, (unsigned long long) sneic->OutDestUnreachs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InTimeExcds, (unsigned long long) sneic->InTimeExcds);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutTimeExcds, (unsigned long long) sneic->OutTimeExcds);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InParmProbs, (unsigned long long) sneic->InParmProbs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutParmProbs, (unsigned long long) sneic->OutParmProbs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InSrcQuenchs, (unsigned long long) sneic->InSrcQuenchs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutSrcQuenchs, (unsigned long long) sneic->OutSrcQuenchs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InRedirects, (unsigned long long) sneic->InRedirects);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutRedirects, (unsigned long long) sneic->OutRedirects);
	printf("\n");
}

/*
 ***************************************************************************
 * Display TCP network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_tcp_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) sntp->ActiveOpens, (unsigned long long) sntc->ActiveOpens);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sntp->PassiveOpens, (unsigned long long) sntc->PassiveOpens);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sntp->InSegs, (unsigned long long) sntc->InSegs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sntp->OutSegs, (unsigned long long) sntc->OutSegs);
	printf("\n");
}

/*
 ***************************************************************************
 * Display TCP network errors statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_etcp_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snetp->AttemptFails, (unsigned long long) snetc->AttemptFails);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snetp->EstabResets, (unsigned long long) snetc->EstabResets);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snetp->RetransSegs, (unsigned long long) snetc->RetransSegs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snetp->InErrs, (unsigned long long) snetc->InErrs);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snetp->OutRsts, (unsigned long long) snetc->OutRsts);
	printf("\n");
}

/*
 ***************************************************************************
 * Display UDP network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_udp_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snup->InDatagrams, (unsigned long long) snuc->InDatagrams);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->OutDatagrams, (unsigned long long) snuc->OutDatagrams);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->NoPorts, (unsigned long long) snuc->NoPorts);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->InErrors, (unsigned long long) snuc->InErrors);
	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 network socket statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_sock6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	printf("%s; %s; %u;", timestr, pfield(a->hdr_line, FIRST), snsc->tcp6_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->udp6_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->raw6_inuse);
	printf(" %s; %u;", pfield(NULL, 0), snsc->frag6_inuse);
	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_ip6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval(snip->InReceives6, snic->InReceives6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->OutForwDatagrams6, snic->OutForwDatagrams6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->InDelivers6, snic->InDelivers6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->OutRequests6, snic->OutRequests6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->ReasmReqds6, snic->ReasmReqds6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->ReasmOKs6, snic->ReasmOKs6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->InMcastPkts6, snic->InMcastPkts6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->OutMcastPkts6, snic->OutMcastPkts6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->FragOKs6, snic->FragOKs6);
	printf(" %s", pfield(NULL, 0));
	pval(snip->FragCreates6, snic->FragCreates6);
	printf("\n");
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_eip6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval(sneip->InHdrErrors6, sneic->InHdrErrors6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InAddrErrors6, sneic->InAddrErrors6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InUnknownProtos6, sneic->InUnknownProtos6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InTooBigErrors6, sneic->InTooBigErrors6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InDiscards6, sneic->InDiscards6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->OutDiscards6, sneic->OutDiscards6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InNoRoutes6, sneic->InNoRoutes6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->OutNoRoutes6, sneic->OutNoRoutes6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->ReasmFails6, sneic->ReasmFails6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->FragFails6, sneic->FragFails6);
	printf(" %s", pfield(NULL, 0));
	pval(sneip->InTruncatedPkts6, sneic->InTruncatedPkts6);
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_icmp6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snip->InMsgs6,
	     (unsigned long long) snic->InMsgs6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutMsgs6,
	     (unsigned long long) snic->OutMsgs6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InEchos6,
	     (unsigned long long) snic->InEchos6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InEchoReplies6,
	     (unsigned long long) snic->InEchoReplies6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutEchoReplies6,
	     (unsigned long long) snic->OutEchoReplies6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InGroupMembQueries6,
	     (unsigned long long) snic->InGroupMembQueries6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InGroupMembResponses6,
	     (unsigned long long) snic->InGroupMembResponses6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutGroupMembResponses6,
	     (unsigned long long) snic->OutGroupMembResponses6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InGroupMembReductions6,
	     (unsigned long long) snic->InGroupMembReductions6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutGroupMembReductions6,
	     (unsigned long long) snic->OutGroupMembReductions6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InRouterSolicits6,
	     (unsigned long long) snic->InRouterSolicits6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutRouterSolicits6,
	     (unsigned long long) snic->OutRouterSolicits6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InRouterAdvertisements6,
	     (unsigned long long) snic->InRouterAdvertisements6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InNeighborSolicits6,
	     (unsigned long long) snic->InNeighborSolicits6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutNeighborSolicits6,
	     (unsigned long long) snic->OutNeighborSolicits6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->InNeighborAdvertisements6,
	     (unsigned long long) snic->InNeighborAdvertisements6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snip->OutNeighborAdvertisements6,
	     (unsigned long long) snic->OutNeighborAdvertisements6);
	printf("\n");
}

/*
 ***************************************************************************
 * Display ICMPv6 error messages statistics in rw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_eicmp6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) sneip->InErrors6, (unsigned long long) sneic->InErrors6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InDestUnreachs6, (unsigned long long) sneic->InDestUnreachs6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutDestUnreachs6, (unsigned long long) sneic->OutDestUnreachs6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InTimeExcds6, (unsigned long long) sneic->InTimeExcds6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutTimeExcds6, (unsigned long long) sneic->OutTimeExcds6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InParmProblems6, (unsigned long long) sneic->InParmProblems6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutParmProblems6, (unsigned long long) sneic->OutParmProblems6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InRedirects6, (unsigned long long) sneic->InRedirects6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutRedirects6, (unsigned long long) sneic->OutRedirects6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->InPktTooBigs6, (unsigned long long) sneic->InPktTooBigs6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) sneip->OutPktTooBigs6, (unsigned long long) sneic->OutPktTooBigs6);
	printf("\n");
}

/*
 ***************************************************************************
 * Display UDPv6 network statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_net_udp6_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[!curr];

	printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
	pval((unsigned long long) snup->InDatagrams6, (unsigned long long) snuc->InDatagrams6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->OutDatagrams6, (unsigned long long) snuc->OutDatagrams6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->NoPorts6, (unsigned long long) snuc->NoPorts6);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) snup->InErrors6, (unsigned long long) snuc->InErrors6);
	printf("\n");
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_cpufreq_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_pwr_cpufreq *spc;

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) {
			/* Yes: Display it */
			printf("%s; %s; %d;", timestr, pfield(a->hdr_line, FIRST), i - 1);
			printf(" %s; %lu;\n", pfield(NULL, 0), spc->cpufreq);
		}
	}
}

/*
 ***************************************************************************
 * Display fan statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_fan_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_pwr_fan *spc;

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);

		printf("%s; %s; %d;", timestr, pfield(a->hdr_line, FIRST), i + 1);
		printf(" %s; %s;", pfield(NULL, 0), spc->device);
		printf(" %s; %f;", pfield(NULL, 0), spc->rpm);
		printf(" rpm_min; %f;\n", spc->rpm_min);
	}
}

/*
 ***************************************************************************
 * Display temperature statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_temp_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_pwr_temp *spc;

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);

		printf("%s; %s; %d;", timestr, pfield(a->hdr_line, FIRST), i + 1);
		printf(" %s; %s;", pfield(NULL, 0), spc->device);
		printf(" %s; %f;", pfield(NULL, 0), spc->temp);
		printf(" temp_min; %f;", spc->temp_min);
		printf(" temp_max; %f;\n", spc->temp_max);
	}
}

/*
 ***************************************************************************
 * Display voltage inputs statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_in_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_pwr_in *spc;

	for (i = 0; i < a->nr[curr]; i++) {
		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);

		printf("%s; %s; %d;", timestr, pfield(a->hdr_line, FIRST), i);
		printf(" %s; %s;", pfield(NULL, 0), spc->device);
		printf(" %s; %f;", pfield(NULL, 0), spc->in);
		printf(" in_min; %f;", spc->in_min);
		printf(" in_max; %f;\n", spc->in_max);
	}
}

/*
 ***************************************************************************
 * Display huge pages statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_huge_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	printf("%s; %s; %llu;", timestr, pfield(a->hdr_line, FIRST), smc->frhkb);
	printf(" hugtotal; %llu;", smc->tlhkb);
	pfield(NULL, 0); /* Skip kbhugused */
	pfield(NULL, 0); /* Skip %hugused */
	printf(" %s; %llu;", pfield(NULL, 0), smc->rsvdhkb);
	printf(" %s; %llu;\n", pfield(NULL, 0), smc->surphkb);
}

/*
 ***************************************************************************
 * Display weighted CPU frequency statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_wghfreq_stats(struct activity *a, char *timestr, int curr)
{
	int i, k;
	struct stats_pwr_wghfreq *spc, *spp, *spc_k, *spp_k;

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_wghfreq *) ((char *) a->buf[curr]  + i * a->msize * a->nr2);
		spp = (struct stats_pwr_wghfreq *) ((char *) a->buf[!curr] + i * a->msize * a->nr2);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		printf("%s; %s; %d;", timestr, pfield(a->hdr_line, FIRST), i - 1);

		for (k = 0; k < a->nr2; k++) {

			spc_k = (struct stats_pwr_wghfreq *) ((char *) spc + k * a->msize);
			if (!spc_k->freq)
				break;
			spp_k = (struct stats_pwr_wghfreq *) ((char *) spp + k * a->msize);

			printf(" freq; %lu;", spc_k->freq);
			printf(" tminst");
			pval(spp_k->time_in_state, spc_k->time_in_state);
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display USB devices statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_pwr_usb_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_pwr_usb *suc;

	for (i = 0; i < a->nr[curr]; i++) {
		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);

		printf("%s; %s; \"%s\";", timestr, pfield(a->hdr_line, FIRST), suc->manufacturer);
		printf(" %s; \"%s\";", pfield(NULL, 0), suc->product);
		printf(" %s; %d;", pfield(NULL, 0), suc->bus_nr);
		printf(" %s; %x;", pfield(NULL, 0), suc->vendor_id);
		printf(" %s; %x;", pfield(NULL, 0), suc->product_id);
		printf(" %s; %u;\n", pfield(NULL, 0), suc->bmaxpower);
	}
}

/*
 ***************************************************************************
 * Display filesystems statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_filesystem_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_filesystem *sfc;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list,
					      DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name))
				/* Device not found */
				continue;
		}

		printf("%s; %s; \"%s\";", timestr, pfield(a->hdr_line, FIRST + DISPLAY_MOUNT(a->opt_flags)),
		       DISPLAY_MOUNT(a->opt_flags) ? sfc->mountp : sfc->fs_name);
		printf(" f_bfree; %llu;", sfc->f_bfree);
		printf(" f_blocks; %llu;", sfc->f_blocks);
		printf(" f_bavail; %llu;", sfc->f_bavail);
		pfield(NULL, 0); /* Skip MBfsfree */
		pfield(NULL, 0); /* Skip MBfsused */
		pfield(NULL, 0); /* Skip %fsused */
		pfield(NULL, 0); /* Skip %ufsused */
		printf(" %s; %llu;", pfield(NULL, 0), sfc->f_ffree);
		printf(" f_files; %llu;\n", sfc->f_files);

	}
}

/*
 ***************************************************************************
 * Display Fibre Channel HBA statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_fchost_stats(struct activity *a, char *timestr, int curr)
{
	int i, j, j0, found;
	struct stats_fchost *sfcc, *sfcp, sfczero;

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

		printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));

		if (!found) {
			/* This is a newly registered host. Previous stats are zero */
			sfcp = &sfczero;
			if (DISPLAY_DEBUG_MODE(flags)) {
				printf(" [NEW]");
			}
		}

		printf("; %s;", sfcc->fchost_name);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sfcp->f_rxframes, (unsigned long long) sfcc->f_rxframes);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sfcp->f_txframes, (unsigned long long) sfcc->f_txframes);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sfcp->f_rxwords, (unsigned long long) sfcc->f_rxwords);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) sfcp->f_txwords, (unsigned long long) sfcc->f_txwords);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display softnet statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_softnet_stats(struct activity *a, char *timestr, int curr)
{
	int i;
	struct stats_softnet *ssnc, *ssnp;

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Don't display CPU "all" which doesn't exist in file */
	for (i = 1; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * The size of a->buf[...] CPU structure may be different from the default
		 * sizeof(struct stats_pwr_cpufreq) value if data have been read from a file!
		 * That's why we don't use a syntax like:
		 * ssnc = (struct stats_softnet *) a->buf[...] + i;
                 */
                ssnc = (struct stats_softnet *) ((char *) a->buf[curr]  + i * a->msize);
                ssnp = (struct stats_softnet *) ((char *) a->buf[!curr] + i * a->msize);

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

		/* Yes: Display current CPU stats */
		printf("%s; %s", timestr, pfield(a->hdr_line, FIRST));
		if (DISPLAY_DEBUG_MODE(flags) && i) {
			if (ssnc->processed + ssnc->dropped + ssnc->time_squeeze +
			    ssnc->received_rps + ssnc->flow_limit == 0) {
				/* CPU is considered offline */
				printf(" [OFF]");
			}
		}
		printf("; %d;", i - 1);

		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssnp->processed, (unsigned long long) ssnc->processed);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssnp->dropped, (unsigned long long) ssnc->dropped);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssnp->time_squeeze, (unsigned long long) ssnc->time_squeeze);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssnp->received_rps, (unsigned long long) ssnc->received_rps);
		printf(" %s", pfield(NULL, 0));
		pval((unsigned long long) ssnp->flow_limit, (unsigned long long) ssnc->flow_limit);
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display pressure-stall CPU statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_psicpu_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr],
		*psip = (struct stats_psi_cpu *) a->buf[!curr];

	printf("%s; %s; %lu;", timestr, pfield(a->hdr_line, FIRST), psic->some_acpu_10);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_acpu_60);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_acpu_300);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) psip->some_cpu_total, (unsigned long long) psic->some_cpu_total);
	printf("\n");
}

/*
 ***************************************************************************
 * Display pressure-stall I/O statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_psiio_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr],
		*psip = (struct stats_psi_io *) a->buf[!curr];

	printf("%s; %s; %lu;", timestr, pfield(a->hdr_line, FIRST), psic->some_aio_10);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_aio_60);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_aio_300);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) psip->some_io_total, (unsigned long long) psic->some_io_total);

	printf(" %s; %lu;", pfield(NULL, 0), psic->full_aio_10);
	printf(" %s; %lu;", pfield(NULL, 0), psic->full_aio_60);
	printf(" %s; %lu;", pfield(NULL, 0), psic->full_aio_300);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) psip->full_io_total, (unsigned long long) psic->full_io_total);
	printf("\n");
}

/*
 ***************************************************************************
 * Display pressure-stall mem statistics in raw format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @timestr	Time for current statistics sample.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t raw_print_psimem_stats(struct activity *a, char *timestr, int curr)
{
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr],
		*psip = (struct stats_psi_mem *) a->buf[!curr];

	printf("%s; %s; %lu;", timestr, pfield(a->hdr_line, FIRST), psic->some_amem_10);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_amem_60);
	printf(" %s; %lu;", pfield(NULL, 0), psic->some_amem_300);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) psip->some_mem_total, (unsigned long long) psic->some_mem_total);

	printf(" %s; %lu;", pfield(NULL, 0), psic->full_amem_10);
	printf(" %s; %lu;", pfield(NULL, 0), psic->full_amem_60);
	printf(" %s; %lu;", pfield(NULL, 0), psic->full_amem_300);
	printf(" %s", pfield(NULL, 0));
	pval((unsigned long long) psip->full_mem_total, (unsigned long long) psic->full_mem_total);
	printf("\n");
}
