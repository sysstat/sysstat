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
extern unsigned int dm_major;

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
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_user - scp->cpu_guest, scc->cpu_user - scc->cpu_guest);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_nice - scp->cpu_guest_nice, scc->cpu_nice - scc->cpu_guest_nice);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_sys, scc->cpu_sys);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_iowait, scc->cpu_iowait);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_steal, scc->cpu_steal);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_hardirq, scc->cpu_hardirq);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_softirq, scc->cpu_softirq);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_guest, scc->cpu_guest);
			printf(" %s:", pfield(NULL, 0));
			pval(scp->cpu_guest_nice, scc->cpu_guest_nice);
			printf(" %s:", pfield(NULL, 0));
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

	printf("%s %s:", timestr, pfield(a->hdr_line, FIRST));
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
			       pfield(a->hdr_line, FIRST), i - 1);
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

	printf("%s %s:", timestr, pfield(a->hdr_line, FIRST));
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

	printf("%s %s:", timestr, pfield(a->hdr_line, FIRST));
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

	printf("%s %s:", timestr, pfield(a->hdr_line, FIRST));
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

	printf("%s %s:%u", timestr, pfield(a->hdr_line, FIRST), skc->dentry_stat);
	printf(" %s:%u", pfield(NULL, 0), skc->file_used);
	printf(" %s:%u", pfield(NULL, 0), skc->inode_used);
	printf(" %s:%u", pfield(NULL, 0), skc->pty_nr);
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

	printf("%s %s:%lu", timestr, pfield(a->hdr_line, FIRST), sqc->nr_running);
	printf(" %s:%u", pfield(NULL, 0), sqc->nr_threads);
	printf(" %s:%u", pfield(NULL, 0), sqc->load_avg_1);
	printf(" %s:%u", pfield(NULL, 0), sqc->load_avg_5);
	printf(" %s:%u", pfield(NULL, 0), sqc->load_avg_15);
	printf(" %s:%lu", pfield(NULL, 0), sqc->procs_blocked);
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
	int i;
	struct stats_serial *ssc, *ssp;

	for (i = 0; i < a->nr; i++) {

		ssc = (struct stats_serial *) ((char *) a->buf[curr]  + i * a->msize);
		ssp = (struct stats_serial *) ((char *) a->buf[!curr] + i * a->msize);

		printf("%s %s:", timestr, pfield(a->hdr_line, FIRST));
		pval(ssp->line, ssc->line);

		if (ssc->line == 0) {
			if (DISPLAY_HINTS(flags)) {
				printf(" [SKP]");
			}
			printf("\n");
			continue;
		}

		if (ssc->line == ssp->line) {
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->rx, ssc->rx);
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->tx, ssc->tx);
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->frame, ssc->frame);
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->parity, ssc->parity);
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->brk, ssc->brk);
			printf(" %s:", pfield(NULL, 0));
			pval(ssp->overrun, ssc->overrun);
		}
		else if (DISPLAY_HINTS(flags)) {
			printf(" [NEW]");
		}

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
	char *dev_name, *persist_dev_name;

	memset(&sdpzero, 0, STATS_DISK_SIZE);

	for (i = 0; i < a->nr; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		printf("%s major:%u minor:%u", timestr, sdc->major, sdc->minor);

		if (!(sdc->major + sdc->minor)) {
			if (DISPLAY_HINTS(flags)) {
				printf(" [SKP]");
			}
			printf("\n");
			continue;
		}

		j = check_disk_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sdp = &sdpzero;
			if (DISPLAY_HINTS(flags)) {
				printf(" [NEW]");
			}
		}
		else {
			sdp = (struct stats_disk *) ((char *) a->buf[!curr] + j * a->msize);
		}

		dev_name = NULL;
		persist_dev_name = NULL;

		if (DISPLAY_PERSIST_NAME_S(flags)) {
			persist_dev_name = get_persistent_name_from_pretty(get_devname(sdc->major, sdc->minor, TRUE));
		}

		if (persist_dev_name) {
			dev_name = persist_dev_name;
		}
		else {
			/* Always use pretty option (-p) */
			if (sdc->major == dm_major) {
				dev_name = transform_devmapname(sdc->major, sdc->minor);
			}

			if (!dev_name) {
				dev_name = get_devname(sdc->major, sdc->minor, TRUE);
			}
		}

		printf(" %s:%s", pfield(a->hdr_line, FIRST), dev_name);
		printf(" %s:", pfield(NULL, 0));
		pval(sdp->nr_ios, sdc->nr_ios);
		printf(" %s:", pfield(NULL, 0));
		pval(sdp->rd_sect, sdc->rd_sect);
		printf(" %s:", pfield(NULL, 0));
		pval(sdp->wr_sect, sdc->wr_sect);
		printf(" tot_ticks:");
		pval(sdp->tot_ticks, sdc->tot_ticks);
		pfield(NULL, 0); /* Skip avgrq-sz */
		printf(" %s:", pfield(NULL, 0));
		pval(sdp->rq_ticks, sdc->rq_ticks);
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

	for (i = 0; i < a->nr; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (!strcmp(sndc->interface, ""))
			break;

		printf("%s %s:%s", timestr, pfield(a->hdr_line, FIRST), sndc->interface);

		j = check_net_dev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			sndp = &sndzero;
			if (DISPLAY_HINTS(flags)) {
				printf(" [NEW]");
			}
		}
		else {
			sndp = (struct stats_net_dev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		printf(" %s:", pfield(NULL, 0));
		pval(sndp->rx_packets, sndc->rx_packets);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->tx_packets, sndc->tx_packets);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->rx_bytes, sndc->rx_bytes);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->tx_bytes, sndc->tx_bytes);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->rx_compressed, sndc->rx_compressed);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->tx_compressed, sndc->tx_compressed);
		printf(" %s:", pfield(NULL, 0));
		pval(sndp->multicast, sndc->multicast);
		printf(" speed:%u duplex:%u\n", sndc->speed, sndc->duplex);
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

	for (i = 0; i < a->nr; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (!strcmp(snedc->interface, ""))
			break;

		printf("%s %s:%s", timestr, pfield(a->hdr_line, FIRST), snedc->interface);

		j = check_net_edev_reg(a, curr, !curr, i);
		if (j < 0) {
			/* This is a newly registered interface. Previous stats are zero */
			snedp = &snedzero;
			if (DISPLAY_HINTS(flags)) {
				printf(" [NEW]");
			}
		}
		else {
			snedp = (struct stats_net_edev *) ((char *) a->buf[!curr] + j * a->msize);
		}

		printf(" %s:", pfield(NULL, 0));
		pval(snedp->rx_errors, snedc->rx_errors);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->tx_errors, snedc->tx_errors);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->collisions, snedc->collisions);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->rx_dropped, snedc->rx_dropped);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->tx_dropped, snedc->tx_dropped);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->tx_carrier_errors, snedc->tx_carrier_errors);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->rx_frame_errors, snedc->rx_frame_errors);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->rx_fifo_errors, snedc->rx_fifo_errors);
		printf(" %s:", pfield(NULL, 0));
		pval(snedp->tx_fifo_errors, snedc->tx_fifo_errors);
		printf("\n");
	}
}
