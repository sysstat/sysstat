/*
 * sa_conv.c: Convert an old format sa file to the up-to-date format.
 * (C) 1999-2016 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "version.h"
#include "sadf.h"
#include "sa.h"
#include "sa_conv.h"

#ifdef USE_NLS
# include <locale.h>
# include <libintl.h>
# define _(string) gettext(string)
#else
# define _(string) (string)
#endif

/*
 ***************************************************************************
 * Read and upgrade file's magic data section.
 *
 * IN:
 * @dfile		System activity data file name.
 * @stdfd		File descriptor for STDOUT.
 *
 * OUT:
 * @fd			File descriptor for sa datafile to convert.
 * @file_magic		Pointer on file_magic structure.
 * @previous_format	TRUE is sa datafile has an old format which is no
 * 			longer compatible with current one.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
 ***************************************************************************
 */
int upgrade_magic_section(char dfile[], int *fd, int stdfd,
			  struct file_magic *file_magic, int *previous_format)
{
	struct file_magic fm;

	/* Open and read sa magic structure */
	sa_open_read_magic(fd, dfile, file_magic, TRUE);

	if ((file_magic->format_magic != FORMAT_MAGIC) &&
	    (file_magic->format_magic != PREVIOUS_FORMAT_MAGIC)) {
		fprintf(stderr, _("Cannot convert the format of this file\n"));
		return -1;
	}

	fprintf(stderr, "file_magic: ");
	if (file_magic->format_magic == PREVIOUS_FORMAT_MAGIC) {
		/*
		 * We have read too many bytes: file_magic structure
		 * was smaller with previous sysstat versions.
		 * Go back 4 (unsigned int header_size) + 64 (char pad[64]) bytes.
		 */
		if (lseek(*fd, -68, SEEK_CUR) < 0) {
			fprintf(stderr, "\nlseek: %s\n", strerror(errno));
			return -1;
		}
		/* Set format magic number to that of current version */
		*previous_format = TRUE;
		file_magic->format_magic = FORMAT_MAGIC;

		/* Fill new structure members */
		file_magic->header_size = FILE_HEADER_SIZE;
		memset(file_magic->pad, 0, sizeof(unsigned char) * FILE_MAGIC_PADDING);
	}

	/* Indicate that file has been upgraded */
	enum_version_nr(&fm);
	file_magic->upgraded = (fm.sysstat_patchlevel << 4) +
			       fm.sysstat_sublevel + 1;

	/* Write file_magic structure */
	if (write(stdfd, file_magic, FILE_MAGIC_SIZE) != FILE_MAGIC_SIZE) {
		fprintf(stderr, "\nwrite: %s\n", strerror(errno));
		return -1;
	}
	fprintf(stderr, "OK\n");

	return 0;
}

/*
 ***************************************************************************
 * Upgrade file_header structure (from 0x2171 format to current format).
 *
 * IN:
 * @buffer	Pointer on file's header structure (as read from file).
 *
 * OUT:
 * @file_hdr	Pointer on file_header structure (up-to-date format).
 ***************************************************************************
 */
void upgrade_file_header(void *buffer, struct file_header *file_hdr)
{
	struct file_header_2171 *f_hdr = (struct file_header_2171 *) buffer;

	file_hdr->sa_ust_time = f_hdr->sa_ust_time;
	file_hdr->sa_act_nr = f_hdr->sa_act_nr;
	file_hdr->sa_day = f_hdr->sa_day;
	file_hdr->sa_month = f_hdr->sa_month;
	file_hdr->sa_year = f_hdr->sa_year;
	file_hdr->sa_sizeof_long = f_hdr->sa_sizeof_long;
	strncpy(file_hdr->sa_sysname, f_hdr->sa_sysname, UTSNAME_LEN);
	file_hdr->sa_sysname[UTSNAME_LEN - 1] = '\0';
	strncpy(file_hdr->sa_nodename, f_hdr->sa_nodename, UTSNAME_LEN);
	file_hdr->sa_nodename[UTSNAME_LEN - 1] = '\0';
	strncpy(file_hdr->sa_release, f_hdr->sa_release, UTSNAME_LEN);
	file_hdr->sa_release[UTSNAME_LEN - 1] = '\0';
	strncpy(file_hdr->sa_machine, f_hdr->sa_machine, UTSNAME_LEN);
	file_hdr->sa_machine[UTSNAME_LEN - 1] = '\0';
	/* The last two values below will be updated later */
	file_hdr->sa_vol_act_nr = 0;
	file_hdr->sa_last_cpu_nr = 0;
}

/*
 ***************************************************************************
 * Upgrade stats_io structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_io(struct activity *act[], int p)
{
	struct stats_io *sic = (struct stats_io *) act[p]->buf[1];
	struct stats_io_8a *sip = (struct stats_io_8a *) act[p]->buf[0];

	sic->dk_drive = (unsigned long long) sip->dk_drive;
	sic->dk_drive_rio = (unsigned long long) sip->dk_drive_rio;
	sic->dk_drive_wio = (unsigned long long) sip->dk_drive_wio;
	sic->dk_drive_rblk = (unsigned long long) sip->dk_drive_rblk;
	sic->dk_drive_wblk = (unsigned long long) sip->dk_drive_wblk;
}

/*
 ***************************************************************************
 * Upgrade stats_queue structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_queue(struct activity *act[], int p)
{
	struct stats_queue *sqc = (struct stats_queue *) act[p]->buf[1];
	struct stats_queue_8a *sqp = (struct stats_queue_8a *) act[p]->buf[0];

	sqc->nr_running = sqp->nr_running;
	sqc->procs_blocked = 0;	/* New field */
	sqc->load_avg_1 = sqp->load_avg_1;
	sqc->load_avg_5 = sqp->load_avg_5;
	sqc->load_avg_15 = sqp->load_avg_15;
	sqc->nr_threads = sqp->nr_threads;
}

/*
 ***************************************************************************
 * Upgrade stats_disk structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_disk(struct activity *act[], int p)
{
	int i;
	struct stats_disk *sdc;
	struct stats_disk_8a *sdp;

	for (i = 0; i < act[p]->nr; i++) {
		sdp = (struct stats_disk_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
		sdc = (struct stats_disk *)    ((char *) act[p]->buf[1] + i * act[p]->fsize);

		sdc->nr_ios = (unsigned long long) sdp->nr_ios;
		sdc->rd_sect = (unsigned long) sdp->rd_sect;
		sdc->wr_sect = (unsigned long) sdp->wr_sect;
		sdc->rd_ticks = (unsigned int) sdp->rd_ticks;
		sdc->wr_ticks = (unsigned int) sdp->wr_ticks;
		sdc->tot_ticks = (unsigned int) sdp->tot_ticks;
		sdc->rq_ticks = (unsigned int) sdp->rq_ticks;
		sdc->major = sdp->major;
		sdc->minor = sdp->minor;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_dev structure (from ACTIVITY_MAGIC_BASE or
 * ACTIVITY_MAGIC_BASE + 1 format to ACTIVITY_MAGIC_BASE + 2 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @m_format	Structure format magic value.
 ***************************************************************************
 */
void upgrade_stats_net_dev(struct activity *act[], int p, unsigned int m_format)
{
	int i;
	struct stats_net_dev *sndc;

	if (m_format == ACTIVITY_MAGIC_BASE) {
		struct stats_net_dev_8a *sndp_a;

		for (i = 0; i < act[p]->nr; i++) {
			sndp_a = (struct stats_net_dev_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sndc->rx_packets = (unsigned long long) sndp_a->rx_packets;
			sndc->tx_packets = (unsigned long long) sndp_a->tx_packets;
			sndc->rx_bytes = (unsigned long long) sndp_a->rx_bytes;
			sndc->tx_bytes = (unsigned long long) sndp_a->tx_bytes;
			sndc->rx_compressed = (unsigned long long) sndp_a->rx_compressed;
			sndc->tx_compressed = (unsigned long long) sndp_a->tx_compressed;
			sndc->multicast = (unsigned long long) sndp_a->multicast;
			sndc->speed = 0; /* New field */
			strncpy(sndc->interface, sndp_a->interface, MAX_IFACE_LEN);
			sndc->interface[MAX_IFACE_LEN - 1] = '\0';
			sndc->duplex = '\0'; /* New field */
		}
	}
	else {
		struct stats_net_dev_8b *sndp_b;

		for (i = 0; i < act[p]->nr; i++) {
			sndp_b = (struct stats_net_dev_8b *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sndc->rx_packets = sndp_b->rx_packets;
			sndc->tx_packets = sndp_b->tx_packets;
			sndc->rx_bytes = sndp_b->rx_bytes;
			sndc->tx_bytes = sndp_b->tx_bytes;
			sndc->rx_compressed = sndp_b->rx_compressed;
			sndc->tx_compressed = sndp_b->tx_compressed;
			sndc->multicast = sndp_b->multicast;
			sndc->speed = 0; /* New field */
			strncpy(sndc->interface, sndp_b->interface, MAX_IFACE_LEN);
			sndc->interface[MAX_IFACE_LEN - 1] = '\0';
			sndc->duplex = '\0'; /* New field */
		}
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_edev structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_net_edev(struct activity *act[], int p)
{
	int i;
	struct stats_net_edev *snedc;
	struct stats_net_edev_8a *snedp;

	for (i = 0; i < act[p]->nr; i++) {
		snedp = (struct stats_net_edev_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
		snedc = (struct stats_net_edev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

		snedc->collisions = (unsigned long long) snedp->collisions;
		snedc->rx_errors = (unsigned long long) snedp->rx_errors;
		snedc->tx_errors = (unsigned long long) snedp->tx_errors;
		snedc->rx_dropped = (unsigned long long) snedp->rx_dropped;
		snedc->tx_dropped = (unsigned long long) snedp->tx_dropped;
		snedc->rx_fifo_errors = (unsigned long long) snedp->rx_fifo_errors;
		snedc->tx_fifo_errors = (unsigned long long) snedp->tx_fifo_errors;
		snedc->rx_frame_errors = (unsigned long long) snedp->rx_frame_errors;
		snedc->tx_carrier_errors = (unsigned long long) snedp->tx_carrier_errors;
		strncpy(snedc->interface, snedp->interface, MAX_IFACE_LEN);
		snedc->interface[MAX_IFACE_LEN - 1] = '\0';
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_ip structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_net_ip(struct activity *act[], int p)
{
	struct stats_net_ip *snic = (struct stats_net_ip *) act[p]->buf[1];
	struct stats_net_ip_8a *snip = (struct stats_net_ip_8a *) act[p]->buf[0];

	snic->InReceives = (unsigned long long) snip->InReceives;
	snic->ForwDatagrams = (unsigned long long) snip->ForwDatagrams;
	snic->InDelivers = (unsigned long long) snip->InDelivers;
	snic->OutRequests = (unsigned long long) snip->OutRequests;
	snic->ReasmReqds = (unsigned long long) snip->ReasmReqds;
	snic->ReasmOKs = (unsigned long long) snip->ReasmOKs;
	snic->FragOKs = (unsigned long long) snip->FragOKs;
	snic->FragCreates = (unsigned long long) snip->FragCreates;
}

/*
 ***************************************************************************
 * Upgrade stats_net_eip structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_net_eip(struct activity *act[], int p)
{
	struct stats_net_eip *sneic = (struct stats_net_eip *) act[p]->buf[1];
	struct stats_net_eip_8a *sneip = (struct stats_net_eip_8a *) act[p]->buf[0];

	sneic->InHdrErrors = (unsigned long long) sneip->InHdrErrors;
	sneic->InAddrErrors = (unsigned long long) sneip->InAddrErrors;
	sneic->InUnknownProtos = (unsigned long long) sneip->InUnknownProtos;
	sneic->InDiscards = (unsigned long long) sneip->InDiscards;
	sneic->OutDiscards = (unsigned long long) sneip->OutDiscards;
	sneic->OutNoRoutes = (unsigned long long) sneip->OutNoRoutes;
	sneic->ReasmFails = (unsigned long long) sneip->ReasmFails;
	sneic->FragFails = (unsigned long long) sneip->FragFails;
}

/*
 ***************************************************************************
 * Upgrade stats_net_ip6 structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_net_ip6(struct activity *act[], int p)
{
	struct stats_net_ip6 *snic6 = (struct stats_net_ip6 *) act[p]->buf[1];
	struct stats_net_ip6_8a *snip6 = (struct stats_net_ip6_8a *) act[p]->buf[0];

	snic6->InReceives6 = (unsigned long long) snip6->InReceives6;
	snic6->OutForwDatagrams6 = (unsigned long long) snip6->OutForwDatagrams6;
	snic6->InDelivers6 = (unsigned long long) snip6->InDelivers6;
	snic6->OutRequests6 = (unsigned long long) snip6->OutRequests6;
	snic6->ReasmReqds6 = (unsigned long long) snip6->ReasmReqds6;
	snic6->ReasmOKs6 = (unsigned long long) snip6->ReasmOKs6;
	snic6->InMcastPkts6 = (unsigned long long) snip6->InMcastPkts6;
	snic6->OutMcastPkts6 = (unsigned long long) snip6->OutMcastPkts6;
	snic6->FragOKs6 = (unsigned long long) snip6->FragOKs6;
	snic6->FragCreates6 = (unsigned long long) snip6->FragCreates6;
}

/*
 ***************************************************************************
 * Upgrade stats_net_eip6 structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_net_eip6(struct activity *act[], int p)
{
	struct stats_net_eip6 *sneic6 = (struct stats_net_eip6 *) act[p]->buf[1];
	struct stats_net_eip6_8a *sneip6 = (struct stats_net_eip6_8a *) act[p]->buf[0];

	sneic6->InHdrErrors6 = (unsigned long long) sneip6->InHdrErrors6;
	sneic6->InAddrErrors6 = (unsigned long long) sneip6->InAddrErrors6;
	sneic6->InUnknownProtos6 = (unsigned long long) sneip6->InUnknownProtos6;
	sneic6->InTooBigErrors6 = (unsigned long long) sneip6->InTooBigErrors6;
	sneic6->InDiscards6 = (unsigned long long) sneip6->InDiscards6;
	sneic6->OutDiscards6 = (unsigned long long) sneip6->OutDiscards6;
	sneic6->InNoRoutes6 = (unsigned long long) sneip6->InNoRoutes6;
	sneic6->OutNoRoutes6 = (unsigned long long) sneip6->OutNoRoutes6;
	sneic6->ReasmFails6 = (unsigned long long) sneip6->ReasmFails6;
	sneic6->FragFails6 = (unsigned long long) sneip6->FragFails6;
	sneic6->InTruncatedPkts6 = (unsigned long long) sneip6->InTruncatedPkts6;
}

/*
 ***************************************************************************
 * Read and upgrade file's header section.
 *
 * IN:
 * @dfile		System activity data file name.
 * @fd			File descriptor for sa datafile to convert.
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_magic		Pointer on file_magic structure.
 * @previous_format	TRUE is sa datafile has an old format which is no
 * 			longer compatible with current one.
 *
 * OUT:
 * @file_hdr		Pointer on file_header structure.
 * @file_actlst		Activity list in file.
 * @vol_id_seq		Sequence of volatile activities.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_header_section(char dfile[], int fd, int stdfd,
			   struct activity *act[], struct file_magic *file_magic,
			   struct file_header *file_hdr, int previous_format,
			   struct file_activity **file_actlst, unsigned int vol_id_seq[])
{
	int i, j, n, p;
	unsigned int a_cpu = FALSE;
	void *buffer = NULL;
	struct file_activity *fal;

	/* Read file header structure */
	fprintf(stderr, "file_header: ");

	if (previous_format) {
		/* Previous format had 2 unsigned int less */
		n = FILE_HEADER_SIZE - 8;
	}
	else {
		n = file_magic->header_size;
	}

	SREALLOC(buffer, char, n);

	sa_fread(fd, buffer, n, HARD_SIZE);

	if (previous_format) {
		/* Upgrade file_header structure */
		upgrade_file_header(buffer, file_hdr);
	}
	else {
		memcpy(file_hdr, buffer, MINIMUM(n, FILE_HEADER_SIZE));
	}

	free(buffer);

	/* Sanity check */
	if (file_hdr->sa_act_nr > MAX_NR_ACT)
		goto invalid_header;

	/* Read file activity list */
	SREALLOC(*file_actlst, struct file_activity, FILE_ACTIVITY_SIZE * file_hdr->sa_act_nr);
	fal = *file_actlst;

	j = 0;
	for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

		sa_fread(fd, fal, FILE_ACTIVITY_SIZE, HARD_SIZE);

		if ((fal->nr < 1) || (fal->nr2 < 1))
			/*
			 * Every activity, known or unknown,
			 * should have at least one item and sub-item.
			 */
			goto invalid_header;

		if ((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) >= 0) {
			/* This is a known activity, maybe with an unknown format */

			if (IS_VOLATILE(act[p]->options) && previous_format) {
				/*
				 * Current activity is known by current version
				 * as a volatile one: So increment the number of
				 * volatile activities in file's header (but only
				 * for old format data files, since up-to-date
				 * format data files already have the right value here).
				 */
				file_hdr->sa_vol_act_nr += 1;

				/*
				 * Create the sequence of volatile activities.
				 * Used only for old format datafiles.
				 * For new format datafiles, this is not necessary
				 * since this sequence already exists following
				 * the RESTART record.
				 */
				vol_id_seq[j++] = act[p]->id;
			}

			if (fal->id == A_CPU) {
				/*
				 * Old format data files don't know volatile
				 * activities. The number of CPU is a constant
				 * all along the file.
				 */
				if (previous_format) {
					file_hdr->sa_last_cpu_nr = fal->nr;
				}
				a_cpu = TRUE;
			}

			/* Size of an activity cannot be zero */
			if (!fal->size)
				goto invalid_header;

			/* Size of activity in file is larger than up-to-date activity size */
			if (fal->size > act[p]->msize) {
				act[p]->msize = fal->size;
			}

			/*
			 * When upgrading a file:
			 * fal->size	: Size of an item for current activity, as
			 * 		  read from the file.
			 * act[p]->msize: Size of the buffer in memory where an item
			 * 		  for current activity, as read from the file,
			 * 		  will be saved. We have:
			 * 		  act[p]->msize >= {fal->size ; act[p]->fsize}
			 * act[p]->fsize: Size of an item for current activity with
			 * 		  up-to-date format.
			 */
			act[p]->nr    = fal->nr;
			act[p]->nr2   = fal->nr2;
			/*
			 * Don't set act[p]->fsize! Should retain the size of an item
			 * for up-to-date format!
			 */
		}
	}

	if (!a_cpu) {
		/*
		 * CPU activity should always be in file
		 * and have a known format (expected magical number).
		 */
		fprintf(stderr, _("\nCPU activity not found in file. Aborting...\n"));
		return -1;
	}

	/* Write file_header structure */
	if ((n = write(stdfd, file_hdr, FILE_HEADER_SIZE)) != FILE_HEADER_SIZE) {
		fprintf(stderr, "\nwrite: %s\n", strerror(errno));
		return -1;
	}

	fprintf(stderr, "OK\n");

	return 0;

invalid_header:

	fprintf(stderr, _("\nInvalid data found. Aborting...\n"));

	return -1;

}

/*
 ***************************************************************************
 * Upgrade file's activity list section.
 *
 * IN:
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_hdr		Pointer on file_header structure.
 * @file_actlst		Activity list in file.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_activity_section(int stdfd, struct activity *act[],
			     struct file_header *file_hdr,
			     struct file_activity *file_actlst)
{
	int i, p;
	struct file_activity file_act;
	struct file_activity *fal;

	fprintf(stderr, "file_activity: ");

	fal = file_actlst;

	for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

		file_act = *fal;
		if ((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) >= 0) {
			/* Update activity magic number */
			file_act.magic = act[p]->magic;
			/* Also update its size, which may have changed with recent versions */
			file_act.size = act[p]->fsize;
		}

		/*
		 * Even unknown activities must be written
		 * (they are counted in sa_act_nr).
		 */
		if (write(stdfd, &file_act, FILE_ACTIVITY_SIZE) != FILE_ACTIVITY_SIZE) {
			fprintf(stderr, "\nwrite: %s\n", strerror(errno));
			return -1;
		}

		fprintf(stderr, "%s ", act[p]->name);
	}

	fprintf(stderr, "OK\n");

	return 0;
}

/*
 ***************************************************************************
 * Upgrade a COMMENT record.
 *
 * IN:
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_comment_record(int fd, int stdfd)
{
	char file_comment[MAX_COMMENT_LEN];

	/* Read the COMMENT record */
	sa_fread(fd, file_comment, MAX_COMMENT_LEN, HARD_SIZE);
	file_comment[MAX_COMMENT_LEN - 1] = '\0';

	/* Then write it. No changes at this time */
	if (write(stdfd, file_comment, MAX_COMMENT_LEN) != MAX_COMMENT_LEN) {
		fprintf(stderr, "\nwrite: %s\n", strerror(errno));
		return -1;
	}

	fprintf(stderr, "C");

	return 0;
}

/*
 ***************************************************************************
 * Upgrade a RESTART record.
 *
 * IN:
 * @fd			File descriptor for sa datafile to convert.
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_hdr		Pointer on file_header structure.
 * @previous_format	TRUE is sa datafile has an old format which is no
 * 			longer compatible with current one.
 * @vol_id_seq		Sequence of volatile activities.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_restart_record(int fd, int stdfd, struct activity *act[],
			   struct file_header *file_hdr, int previous_format,
			   unsigned int vol_id_seq[])
{
	int i, p;
	struct file_activity file_act;

	/*
	 * This is a RESTART record.
	 * Only new format data file have additional structures
	 * to read here for volatile activities.
	 */
	for (i = 0; i < file_hdr->sa_vol_act_nr; i++) {

		if (!previous_format) {
			sa_fread(fd, &file_act, FILE_ACTIVITY_SIZE, HARD_SIZE);
			if (file_act.id) {
				reallocate_vol_act_structures(act, file_act.nr, file_act.id);
			}
		}
		else {
			memset(&file_act, 0, FILE_ACTIVITY_SIZE);

			/* Old format: Sequence of volatile activities is in vol_id_seq */
			p = get_activity_position(act, vol_id_seq[i], EXIT_IF_NOT_FOUND);

			/* Set only the necessary fields */
			file_act.id = act[p]->id;
			file_act.nr = act[p]->nr;
		}

		if (write(stdfd, &file_act, FILE_ACTIVITY_SIZE) != FILE_ACTIVITY_SIZE) {
			fprintf(stderr, "\nwrite: %s\n", strerror(errno));
			return -1;
		}
	}

	fprintf(stderr, "R");

	return 0;
}

/*
 ***************************************************************************
 * Upgrade a record which is not a COMMENT or a RESTART one.
 *
 * IN:
 * @fd			File descriptor for sa datafile to convert.
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_hdr		Pointer on file_header structure.
 * @file_actlst		Activity list in file.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_common_record(int fd, int stdfd, struct activity *act[],
			   struct file_header *file_hdr,
			   struct file_activity *file_actlst)
{
	int i, j, k, p;
	struct file_activity *fal;
	void *buffer = NULL;
	size_t size;

	/*
	 * This is not a special record, so read the extra fields,
	 * even if the format of the activity is unknown.
	 */
	fal = file_actlst;
	for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

		if ((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) < 0) {
			/* An unknown activity should still be read and written */
			size = (size_t) fal->size * (size_t) fal->nr * (size_t) fal->nr2;
			if (!size) {
				/* Buffer may have been allocated from a previous iteration in the loop */
				if (buffer) {
					free (buffer);
				}
				return -1;
			}
			SREALLOC(buffer, void, size);
			sa_fread(fd, buffer, fal->size * fal->nr * fal->nr2, HARD_SIZE);
			if (write(stdfd, (char *) buffer, size) != size) {
				fprintf(stderr, "\nwrite: %s\n", strerror(errno));
				free(buffer);
				return -1;
			}
			continue;
		}

		if ((act[p]->nr > 0) &&
		    ((act[p]->nr > 1) || (act[p]->nr2 > 1)) &&
		    (act[p]->msize > fal->size)) {

			for (j = 0; j < act[p]->nr; j++) {
				for (k = 0; k < act[p]->nr2; k++) {
					sa_fread(fd,
						 (char *) act[p]->buf[0] + (j * act[p]->nr2 + k) * act[p]->msize,
						 fal->size, HARD_SIZE);
				}
			}
		}

		else if (act[p]->nr > 0) {
			sa_fread(fd, act[p]->buf[0], fal->size * act[p]->nr * act[p]->nr2, HARD_SIZE);
		}

		if (act[p]->magic != fal->magic) {

			/* Known activity but old format */
			switch (fal->id) {

				case A_IO:
					upgrade_stats_io(act, p);
					break;

				case A_QUEUE:
					upgrade_stats_queue(act, p);
					break;

				case A_DISK:
					upgrade_stats_disk(act, p);
					break;

				case A_NET_DEV:
					upgrade_stats_net_dev(act, p, fal->magic);
					break;

				case A_NET_EDEV:
					upgrade_stats_net_edev(act, p);
					break;

				case A_NET_IP:
					upgrade_stats_net_ip(act, p);
					break;

				case A_NET_EIP:
					upgrade_stats_net_eip(act, p);
					break;

				case A_NET_IP6:
					upgrade_stats_net_ip6(act, p);
					break;

				case A_NET_EIP6:
					upgrade_stats_net_eip6(act, p);
					break;
				}
		}
		else {
			/* Known activity with current up-to-date format */
			for (j = 0; j < act[p]->nr; j++) {
				for (k = 0; k < act[p]->nr2; k++) {
					memcpy((char *) act[p]->buf[1] + (j * act[p]->nr2 + k) * act[p]->msize,
					       (char *) act[p]->buf[0] + (j * act[p]->nr2 + k) * act[p]->msize,
					       fal->size);
				}
			}
		}

		for (j = 0; j < act[p]->nr; j++) {
			for (k = 0; k < act[p]->nr2; k++) {
				if (write(stdfd,
					       (char *) act[p]->buf[1] + (j * act[p]->nr2 + k) * act[p]->msize,
					       act[p]->fsize) !=
					       act[p]->fsize) {
					fprintf(stderr, "\nwrite: %s\n", strerror(errno));
					free(buffer);
					return -1;
				}
			}
		}
	}

	fprintf(stderr, ".");

	if (buffer) {
		free(buffer);
	}

	return 0;
}

/*
 ***************************************************************************
 * Upgrade statistics records.
 *
 * IN:
 * @fd			File descriptor for sa datafile to convert.
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_hdr		Pointer on file_header structure.
 * @previous_format	TRUE is sa datafile has an old format which is no
 * 			longer compatible with current one.
 * @file_actlst		Activity list in file.
 * @vol_id_seq		Sequence of volatile activities.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_stat_records(int fd, int stdfd, struct activity *act[],
			 struct file_header *file_hdr, int previous_format,
			 struct file_activity *file_actlst, unsigned int vol_id_seq[])
{
	int rtype;
	int eosaf;
	struct record_header record_hdr;

	fprintf(stderr, _("Statistics: "));

	do {
		eosaf = sa_fread(fd, &record_hdr, RECORD_HEADER_SIZE, SOFT_SIZE);
		rtype = record_hdr.record_type;

		if (!eosaf) {

			if (write(stdfd, &record_hdr, RECORD_HEADER_SIZE)
				!= RECORD_HEADER_SIZE) {
				fprintf(stderr, "\nwrite: %s\n", strerror(errno));
				return -1;
			}

			if (rtype == R_COMMENT) {
				/* Upgrade the COMMENT record */
				if (upgrade_comment_record(fd, stdfd) < 0)
					return -1;
			}

			else if (rtype == R_RESTART) {
				/* Upgrade the RESTART record */
				if (upgrade_restart_record(fd, stdfd, act, file_hdr,
							   previous_format, vol_id_seq) < 0)
					return -1;
			}

			else {
				/* Upgrade current statistics record */
				if (upgrade_common_record(fd, stdfd, act, file_hdr,
					                  file_actlst) < 0)
					return -1;
			}
		}
	}
	while (!eosaf);

	return 0;
}

/*
 ***************************************************************************
 * Close file descriptors and exit.
 *
 * IN:
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @exit_code	Exit code.
 ***************************************************************************
 */
void upgrade_exit(int fd, int stdfd, int exit_code)
{
	if (fd) {
		close(fd);
	}
	if (stdfd) {
		close(stdfd);
	}

	exit(exit_code);
}

/*
 ***************************************************************************
 * Convert a sysstat activity data file from a previous version to the
 * up-to-date format. Presently data files from sysstat version 9.1.6 and
 * later are converted to current sysstat version format.
 *
 * IN:
 * @dfile	System activity data file name.
 * @act		Array of activities.
 ***************************************************************************
 */
void convert_file(char dfile[], struct activity *act[])
{
	int fd = 0, stdfd = 0;
	int previous_format = FALSE;
	struct file_magic file_magic;
	struct file_header file_hdr;
	struct file_activity *file_actlst = NULL;
	unsigned int vol_id_seq[NR_ACT];

	/* Open stdout */
	if ((stdfd = dup(STDOUT_FILENO)) < 0) {
		perror("dup");
		upgrade_exit(0, 0, 2);
	}

	/* Upgrade file's magic section */
	if (upgrade_magic_section(dfile, &fd, stdfd, &file_magic,
				  &previous_format) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	/* Upgrade file's header section */
	if (upgrade_header_section(dfile, fd, stdfd, act,
				   &file_magic, &file_hdr,
				   previous_format, &file_actlst,
				   vol_id_seq) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	/* Upgrade file's activity list section */
	if (upgrade_activity_section(stdfd, act, &file_hdr,
				     file_actlst) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	/* Perform required allocations */
	allocate_structures(act);

	/* Upgrade statistics records */
	if (upgrade_stat_records(fd, stdfd, act, &file_hdr,
				 previous_format, file_actlst,
				 vol_id_seq) <0) {
		upgrade_exit(fd, stdfd, 2);
	}

	fprintf(stderr,
		_("\nFile successfully converted to sysstat format version %s\n"),
		VERSION);

	free(file_actlst);
	free_structures(act);
	close(fd);
	close(stdfd);
}
