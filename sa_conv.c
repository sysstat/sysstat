/*
 * sa_conv.c: Convert an old format sa file to the up-to-date format.
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "version.h"
#include "sadf.h"
#include "sa_conv.h"

#ifdef USE_NLS
# include <locale.h>
# include <libintl.h>
# define _(string) gettext(string)
#else
# define _(string) (string)
#endif

extern int endian_mismatch;
extern unsigned int user_hz;
extern unsigned int act_types_nr[];
extern unsigned int rec_types_nr[];
extern unsigned int hdr_types_nr[];

unsigned int oact_types_nr[] = {OLD_FILE_ACTIVITY_ULL_NR, OLD_FILE_ACTIVITY_UL_NR, OLD_FILE_ACTIVITY_U_NR};

/*
 ***************************************************************************
 * Read and upgrade file's magic data section.
 *
 * IN:
 * @dfile	System activity data file name.
 * @stdfd	File descriptor for STDOUT.
 *
 * OUT:
 * @fd		File descriptor for sa datafile to convert.
 * @file_magic	File's magic structure.
 * @hdr_size	Size of header structure read from file.
 * @previous_format
 *		Format magic number of file to convert, converted to
 *		current endianness.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
 ***************************************************************************
 */
int upgrade_magic_section(char dfile[], int *fd, int stdfd, struct file_magic *file_magic,
			  unsigned int *hdr_size, int *previous_format, int *endian_mismatch)
{
	struct file_magic fm;
	unsigned int fm_types_nr[] = {FILE_MAGIC_ULL_NR, FILE_MAGIC_UL_NR, FILE_MAGIC_U_NR};

	/* Open and read sa magic structure */
	sa_open_read_magic(fd, dfile, file_magic, TRUE, endian_mismatch, FALSE);

	switch (file_magic->format_magic) {

		case FORMAT_MAGIC:
		case FORMAT_MAGIC_SWAPPED:
			*previous_format = FORMAT_MAGIC;
			return 0;
			break;

		case FORMAT_MAGIC_2171:
		case FORMAT_MAGIC_2171_SWAPPED:
			*previous_format = FORMAT_MAGIC_2171;
			break;

		case FORMAT_MAGIC_2173:
		case FORMAT_MAGIC_2173_SWAPPED:
			*previous_format = FORMAT_MAGIC_2173;
			break;

		default:
			fprintf(stderr, _("Cannot convert the format of this file\n"));
			return -1;
			break;
	}

	fprintf(stderr, "file_magic: ");
	if (*previous_format == FORMAT_MAGIC_2171) {
		/*
		 * We have read too many bytes: file_magic structure
		 * was smaller with previous sysstat versions.
		 * Go back 4 (unsigned int header_size) + 64 (char pad[64]) bytes.
		 */
		if (lseek(*fd, -68, SEEK_CUR) < 0) {
			fprintf(stderr, "\nlseek: %s\n", strerror(errno));
			return -1;
		}
	}

	/* Set format magic number to that of current version */
	file_magic->format_magic = (*endian_mismatch ? FORMAT_MAGIC_SWAPPED
						     : FORMAT_MAGIC);

	/* Save original header structure's size */
	*hdr_size = file_magic->header_size;

	/* Fill new structure members */
	file_magic->header_size = FILE_HEADER_SIZE;
	file_magic->hdr_types_nr[0] = FILE_HEADER_ULL_NR;
	file_magic->hdr_types_nr[1] = FILE_HEADER_UL_NR;
	file_magic->hdr_types_nr[2] = FILE_HEADER_U_NR;
	memset(file_magic->pad, 0, sizeof(unsigned char) * FILE_MAGIC_PADDING);

	/* Indicate that file has been upgraded */
	enum_version_nr(&fm);
	file_magic->upgraded = (fm.sysstat_patchlevel << 8) +
			       fm.sysstat_sublevel + 1;

	memcpy(&fm, file_magic, FILE_MAGIC_SIZE);
	/* Restore endianness before writing */
	if (*endian_mismatch) {
		/* Start swapping at field "header_size" position */
		swap_struct(fm_types_nr, &(fm.header_size), 0);
	}

	/* Write file's magic structure */
	if (write_all(stdfd, &fm, FILE_MAGIC_SIZE) != FILE_MAGIC_SIZE) {
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
 * @buffer	File's header structure (as read from file).
 * @previous_format
 *		Format magic number of file to convert.
 * @endian_mismatch
 *		TRUE if data read from file don't match current
 *		machine's endianness.
 *
 * OUT:
 * @file_hdr	File's header structure (up-to-date format).
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 * @vol_act_nr	Number of volatile activity structures following a restart
 *		record for 0x2173 file format.
 ***************************************************************************
 */
void upgrade_file_header(void *buffer, struct file_header *file_hdr, int previous_format,
			 int endian_mismatch, int *arch_64, unsigned int *vol_act_nr)
{
	struct file_header_2171 *f_hdr_2171 = (struct file_header_2171 *) buffer;
	struct file_header_2173 *f_hdr_2173 = (struct file_header_2173 *) buffer;
	unsigned int hdr_2171_types_nr[] = {FILE_HEADER_2171_ULL_NR, FILE_HEADER_2171_UL_NR, FILE_HEADER_2171_U_NR};
	unsigned int hdr_2173_types_nr[] = {FILE_HEADER_2173_ULL_NR, FILE_HEADER_2173_UL_NR, FILE_HEADER_2173_U_NR};
	int i;

	memset(file_hdr, 0, FILE_HEADER_SIZE);
	file_hdr->sa_hz = HZ;

	if (previous_format == FORMAT_MAGIC_2171) {
		*arch_64 = (f_hdr_2171->sa_sizeof_long == SIZEOF_LONG_64BIT);

		/* Normalize endianness for f_hdr_2171 structure */
		if (endian_mismatch) {
			swap_struct(hdr_2171_types_nr, f_hdr_2171, *arch_64);
		}

		file_hdr->sa_ust_time = (unsigned long long) f_hdr_2171->sa_ust_time;
		/* sa_cpu_nr field will be updated later */
		file_hdr->sa_act_nr = f_hdr_2171->sa_act_nr;
		file_hdr->sa_year = (int) f_hdr_2171->sa_year;
		file_hdr->sa_day = f_hdr_2171->sa_day;
		file_hdr->sa_month = f_hdr_2171->sa_month;
		file_hdr->sa_sizeof_long = f_hdr_2171->sa_sizeof_long;
		strncpy(file_hdr->sa_sysname, f_hdr_2171->sa_sysname, sizeof(file_hdr->sa_sysname));
		file_hdr->sa_sysname[sizeof(file_hdr->sa_sysname) - 1] = '\0';
		strncpy(file_hdr->sa_nodename, f_hdr_2171->sa_nodename, sizeof(file_hdr->sa_nodename));
		file_hdr->sa_nodename[sizeof(file_hdr->sa_nodename) - 1] = '\0';
		strncpy(file_hdr->sa_release, f_hdr_2171->sa_release, sizeof(file_hdr->sa_release));
		file_hdr->sa_release[sizeof(file_hdr->sa_release) - 1] = '\0';
		strncpy(file_hdr->sa_machine, f_hdr_2171->sa_machine, sizeof(file_hdr->sa_machine));
		file_hdr->sa_machine[sizeof(file_hdr->sa_machine) - 1] = '\0';
	}

	else if (previous_format == FORMAT_MAGIC_2173) {
		*arch_64 = (f_hdr_2173->sa_sizeof_long == SIZEOF_LONG_64BIT);

		/* Normalize endianness for f_hdr_2171 structure */
		if (endian_mismatch) {
			swap_struct(hdr_2173_types_nr, f_hdr_2173, *arch_64);
		}

		file_hdr->sa_ust_time = (unsigned long long) f_hdr_2173->sa_ust_time;
		/* sa_cpu_nr field will be updated later */
		file_hdr->sa_act_nr = f_hdr_2173->sa_act_nr;
		file_hdr->sa_year = (int) f_hdr_2173->sa_year;
		file_hdr->sa_day = f_hdr_2173->sa_day;
		file_hdr->sa_month = f_hdr_2173->sa_month;
		file_hdr->sa_sizeof_long = f_hdr_2173->sa_sizeof_long;
		strncpy(file_hdr->sa_sysname, f_hdr_2173->sa_sysname, UTSNAME_LEN);
		file_hdr->sa_sysname[UTSNAME_LEN - 1] = '\0';
		strncpy(file_hdr->sa_nodename, f_hdr_2173->sa_nodename, UTSNAME_LEN);
		file_hdr->sa_nodename[UTSNAME_LEN - 1] = '\0';
		strncpy(file_hdr->sa_release, f_hdr_2173->sa_release, UTSNAME_LEN);
		file_hdr->sa_release[UTSNAME_LEN - 1] = '\0';
		strncpy(file_hdr->sa_machine, f_hdr_2173->sa_machine, UTSNAME_LEN);
		file_hdr->sa_machine[UTSNAME_LEN - 1] = '\0';

		*vol_act_nr = f_hdr_2173->sa_vol_act_nr;
	}

	for (i = 0; i < 3; i++) {
		file_hdr->act_types_nr[i] = act_types_nr[i];
		file_hdr->rec_types_nr[i] = rec_types_nr[i];
	}
	file_hdr->act_size = FILE_ACTIVITY_SIZE;
	file_hdr->rec_size = RECORD_HEADER_SIZE;

	/*
	 * Note: @extra_next and @sa_tzname[] members are set to zero
	 * because file_hdr has been memset'd to zero.
	 */
}

/*
 ***************************************************************************
 * Read and upgrade file's header section.
 *
 * IN:
 * @dfile	System activity data file name.
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @act		Array of activities.
 * @file_magic	File's magic structure.
 * @hdr_size	Size of header structure read from file.
 * @previous_format
 *		Format magic number of file to convert.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 *
 * OUT:
 * @file_hdr	File's header structure (up-to-date format).
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 * @vol_act_nr	Number of volatile activity structures following a restart
 *		record for 0x2173 file format.
 * @ofile_actlst
 *		Activity list in file.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_header_section(char dfile[], int fd, int stdfd, struct activity *act[],
			   struct file_magic *file_magic, struct file_header *file_hdr,
			   unsigned int hdr_size, int previous_format, int *arch_64,
			   int endian_mismatch, unsigned int *vol_act_nr,
			   struct old_file_activity **ofile_actlst)
{
	struct old_file_activity *ofal;
	struct file_header fh;
	int i, n, p;
	unsigned int a_cpu = FALSE;
	void *buffer = NULL;

	fprintf(stderr, "file_header: ");

	/* Read file header structure */
	n = (previous_format == FORMAT_MAGIC_2171 ? FILE_HEADER_SIZE_2171
						  : hdr_size);
	SREALLOC(buffer, char, n);
	sa_fread(fd, buffer, (size_t) n, HARD_SIZE, UEOF_STOP);

	/* Upgrade file_header structure */
	upgrade_file_header(buffer, file_hdr, previous_format,
			    endian_mismatch, arch_64, vol_act_nr);

	free(buffer);

	/* Sanity check */
	if (file_hdr->sa_act_nr > MAX_NR_ACT)
		goto invalid_header;

	/* Read file activity list */
	SREALLOC(*ofile_actlst, struct old_file_activity, OLD_FILE_ACTIVITY_SIZE * file_hdr->sa_act_nr);
	ofal = *ofile_actlst;

	for (i = 0; i < file_hdr->sa_act_nr; i++, ofal++) {

		sa_fread(fd, ofal, OLD_FILE_ACTIVITY_SIZE, HARD_SIZE, UEOF_STOP);

		/* Normalize endianness for file_activity structures */
		if (endian_mismatch) {
			swap_struct(oact_types_nr, ofal, *arch_64);
		}

		if ((ofal->nr < 1) || (ofal->nr2 < 1))
			/*
			 * Every activity, known or unknown,
			 * should have at least one item and sub-item.
			 */
			goto invalid_header;

		if ((p = get_activity_position(act, ofal->id, RESUME_IF_NOT_FOUND)) >= 0) {
			/* This is a known activity, maybe with an unknown format */

			if ((ofal->id == A_CPU) && !a_cpu) {
				/*
				 * We have found the CPU activity: Set sa_cpu_nr field
				 * in file_header structure that should contains the
				 * number of CPU when the file was created.
				 */
				file_hdr->sa_cpu_nr = ofal->nr;
				a_cpu = TRUE;
			}

			/* Size of an activity cannot be zero */
			if (!ofal->size)
				goto invalid_header;

			/* Size of activity in file is larger than up-to-date activity size */
			if (ofal->size > act[p]->msize) {
				act[p]->msize = ofal->size;
			}

			/*
			 * When upgrading a file:
			 * ofal->size	: Size of an item for current activity, as
			 * 		  read from the file.
			 * act[p]->msize: Size of the buffer in memory where an item
			 * 		  for current activity, as read from the file,
			 * 		  will be saved. We have:
			 * 		  act[p]->msize >= {ofal->size ; act[p]->fsize}
			 * act[p]->fsize: Size of an item for current activity with
			 * 		  up-to-date format.
			 */
			act[p]->nr_ini = ofal->nr;
			act[p]->nr2    = ofal->nr2;
			/*
			 * Don't set act[p]->fsize! Should retain the size of an item
			 * for up-to-date format!
			 */
		}
	}

	if (!a_cpu) {
		/*
		 * CPU activity should always be in file for versions older than 11.7.1
		 * and have a known format (expected magical number).
		 */
		fprintf(stderr, _("\nCPU activity not found in file. Aborting...\n"));
		return -1;
	}

	memcpy(&fh, file_hdr, FILE_HEADER_SIZE);
	/* Restore endianness before writing */
	if (endian_mismatch) {
		/* Start swapping at field "header_size" position */
		swap_struct(hdr_types_nr, &fh, *arch_64);
	}

	/* Write file_header structure */
	if ((n = write_all(stdfd, &fh, FILE_HEADER_SIZE)) != FILE_HEADER_SIZE) {
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
 * Convert a long integer value to a long long integer value.
 * The long integer value may be 32 or 64-bit wide and come from a 32 or
 * 64-bit architecture.
 *
 * Note: Consider the value 0x01020304 read on a 32-bit machine.
 * Big-endian, saved as:   01 02 03 04
 * Lille-endian, saved as: 04 03 02 01
 * The value should be saved as a 64-bit value and endianness should be
 * preserved:
 * Big-endian:    00 00 00 00 01 02 03 04
 * Little-endian: 04 03 02 01 00 00 00 00
 *
 * IN:
 * @buffer	Address of value to convert.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
unsigned long long moveto_long_long(void *buffer, int endian_mismatch, int arch_64)
{
	unsigned int *u_int;
	unsigned long long *ull_int, ull_i;

	if (arch_64) {
		ull_int = (unsigned long long *) buffer;
		return *ull_int;
	}

	u_int = (unsigned int *) buffer;
	if (endian_mismatch) {
		ull_i = (unsigned long long) *u_int;
		return (ull_i >> 32) | (ull_i << 32);
	}
	else {
		return (unsigned long long) *u_int;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_cpu structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @st_size	Size of the structure read from file.
 ***************************************************************************
 */
void upgrade_stats_cpu(struct activity *act[], int p, int st_size)
{
	int i;
	struct stats_cpu *scc;
	struct stats_cpu_8a *scp;

	for (i = 0; i < act[p]->nr_ini; i++) {
		/*
		 * For previous structure's format: Use msize (which has possibly been set to
		 * the size of the structure read from disk if it's greater than that of
		 * current format: See upgrade_header_section()).
		 * For current structure format: Use fsize. This is the normal size of
		 * structure's up-to-date format that will be written to file.
		 */
		scp = (struct stats_cpu_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
		scc = (struct stats_cpu *)    ((char *) act[p]->buf[1] + i * act[p]->fsize);

		scc->cpu_user = scp->cpu_user;
		scc->cpu_nice = scp->cpu_nice;
		scc->cpu_sys = scp->cpu_sys;
		scc->cpu_idle = scp->cpu_idle;
		scc->cpu_iowait = scp->cpu_iowait;
		scc->cpu_steal = scp->cpu_steal;
		scc->cpu_hardirq = scp->cpu_hardirq;
		scc->cpu_softirq = scp->cpu_softirq;
		scc->cpu_guest = scp->cpu_guest;

		if (st_size >= STATS_CPU_8A_SIZE) {
			/* guest_nice field has been added without a structure format change */
			scc->cpu_guest_nice = scp->cpu_guest_nice;
		}
	}
}

/*
 ***************************************************************************
 * Upgrade stats_pcsw structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_pcsw(struct activity *act[], int p)
{
	struct stats_pcsw *spc = (struct stats_pcsw *) act[p]->buf[1];
	struct stats_pcsw_8a *spp = (struct stats_pcsw_8a *) act[p]->buf[0];

	spc->context_switch = spp->context_switch;
	/*
	 * Copy a long into a long. Take into account that file may have been
	 * created on a 64-bit machine and we may be converting on a 32-bit machine.
	 */
	memcpy(&spc->processes, &spp->processes, 8);
}

/*
 ***************************************************************************
 * Upgrade stats_irq structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_irq(struct activity *act[], int p)
{
	int i;
	struct stats_irq *sic;
	struct stats_irq_8a *sip;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sip = (struct stats_irq_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
		sic = (struct stats_irq *)    ((char *) act[p]->buf[1] + i * act[p]->fsize);

		sic->irq_nr = sip->irq_nr;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_io structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 ***************************************************************************
 */
void upgrade_stats_io(struct activity *act[], int p, int endian_mismatch)
{
	struct stats_io *sic = (struct stats_io *) act[p]->buf[1];
	struct stats_io_8a *sip = (struct stats_io_8a *) act[p]->buf[0];

	sic->dk_drive = moveto_long_long(&sip->dk_drive, endian_mismatch, FALSE);
	sic->dk_drive_rio = moveto_long_long(&sip->dk_drive_rio, endian_mismatch, FALSE);
	sic->dk_drive_wio = moveto_long_long(&sip->dk_drive_wio, endian_mismatch, FALSE);
	sic->dk_drive_rblk = moveto_long_long(&sip->dk_drive_rblk, endian_mismatch, FALSE);
	sic->dk_drive_wblk = moveto_long_long(&sip->dk_drive_wblk, endian_mismatch, FALSE);
}

/*
 ***************************************************************************
 * Upgrade stats_memory structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @st_size	Size of the structure read from file.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_memory(struct activity *act[], int p, int st_size,
			  int endian_mismatch, int arch_64)
{
	struct stats_memory *smc = (struct stats_memory *) act[p]->buf[1];
	struct stats_memory_8a *smp = (struct stats_memory_8a *) act[p]->buf[0];

	smc->frmkb = moveto_long_long(&smp->frmkb, endian_mismatch, arch_64);
	smc->bufkb = moveto_long_long(&smp->bufkb, endian_mismatch, arch_64);
	smc->camkb = moveto_long_long(&smp->camkb, endian_mismatch, arch_64);
	smc->tlmkb = moveto_long_long(&smp->tlmkb, endian_mismatch, arch_64);
	smc->frskb = moveto_long_long(&smp->frskb, endian_mismatch, arch_64);
	smc->tlskb = moveto_long_long(&smp->tlskb, endian_mismatch, arch_64);
	smc->caskb = moveto_long_long(&smp->caskb, endian_mismatch, arch_64);
	smc->comkb = moveto_long_long(&smp->comkb, endian_mismatch, arch_64);
	smc->activekb = moveto_long_long(&smp->activekb, endian_mismatch, arch_64);
	smc->inactkb = moveto_long_long(&smp->inactkb, endian_mismatch, arch_64);

	/* Some fields have been added without a structure format change */
	if (st_size >= STATS_MEMORY_8A_1_SIZE) {
				smc->dirtykb = moveto_long_long(&smp->dirtykb, endian_mismatch, arch_64);
	}
	if (st_size >= STATS_MEMORY_8A_2_SIZE) {
		smc->anonpgkb = moveto_long_long(&smp->anonpgkb, endian_mismatch, arch_64);
		smc->slabkb = moveto_long_long(&smp->slabkb, endian_mismatch, arch_64);
		smc->kstackkb = moveto_long_long(&smp->kstackkb, endian_mismatch, arch_64);
		smc->pgtblkb = moveto_long_long(&smp->pgtblkb, endian_mismatch, arch_64);
		smc->vmusedkb = moveto_long_long(&smp->vmusedkb, endian_mismatch, arch_64);
	}
	if (st_size >= STATS_MEMORY_8A_SIZE) {
		smc->availablekb = moveto_long_long(&(smp->availablekb), endian_mismatch, arch_64);;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_ktables structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 ***************************************************************************
 */
void upgrade_stats_ktables(struct activity *act[], int p, int endian_mismatch)
{
	struct stats_ktables *skc = (struct stats_ktables *) act[p]->buf[1];
	struct stats_ktables_8a *skp = (struct stats_ktables_8a *) act[p]->buf[0];

	skc->file_used = moveto_long_long(&skp->file_used, endian_mismatch, FALSE);
	skc->inode_used = moveto_long_long(&skp->inode_used, endian_mismatch, FALSE);
	skc->dentry_stat = moveto_long_long(&skp->dentry_stat, endian_mismatch, FALSE);
	skc->pty_nr = moveto_long_long(&skp->pty_nr, endian_mismatch, FALSE);
}

/*
 ***************************************************************************
 * Upgrade stats_queue structure (from ACTIVITY_MAGIC_BASE or
 * ACTIVITY_MAGIC_BASE + 1 format to ACTIVITY_MAGIC_BASE + 2 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_queue(struct activity *act[], int p, unsigned int magic,
			 int endian_mismatch, int arch_64)
{
	struct stats_queue *sqc = (struct stats_queue *) act[p]->buf[1];

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_queue_8a *sqp = (struct stats_queue_8a *) act[p]->buf[0];

		sqc->nr_running = moveto_long_long(&sqp->nr_running, endian_mismatch, arch_64);
		sqc->procs_blocked = 0ULL;	/* New field */
		sqc->nr_threads = moveto_long_long(&sqp->nr_threads, endian_mismatch, FALSE);
		sqc->load_avg_1 = sqp->load_avg_1;
		sqc->load_avg_5 = sqp->load_avg_5;
		sqc->load_avg_15 = sqp->load_avg_15;
	}
	else {
		struct stats_queue_8b *sqp = (struct stats_queue_8b *) act[p]->buf[0];

		sqc->nr_running = moveto_long_long(&sqp->nr_running, endian_mismatch, arch_64);
		sqc->procs_blocked = moveto_long_long(&sqp->procs_blocked, endian_mismatch, arch_64);
		sqc->nr_threads = moveto_long_long(&sqp->nr_threads, endian_mismatch, FALSE);
		sqc->load_avg_1 = sqp->load_avg_1;
		sqc->load_avg_5 = sqp->load_avg_5;
		sqc->load_avg_15 = sqp->load_avg_15;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_serial structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @st_size	Size of the structure read from file.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 *
 * RETURNS:
 * Number of serial line structures that actually need to be written to
 * disk.
 ***************************************************************************
 */
int upgrade_stats_serial(struct activity *act[], int p, size_t st_size, int endian_mismatch)
{
	int i;
	unsigned int line;
	struct stats_serial *ssc;

	/* Copy TTY stats to target structure */
	memcpy(act[p]->buf[1], act[p]->buf[0], (size_t) act[p]->nr_ini * st_size);

	for (i = 0; i < act[p]->nr_ini; i++) {
		ssc = (struct stats_serial *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

		/* Line number now starts at 0 instead of 1 */
		line = (endian_mismatch ? __builtin_bswap32(ssc->line) : ssc->line);
		if (!line)
			break;
		line--;
		ssc->line = (endian_mismatch ? __builtin_bswap32(line) : line);
	}

	return i;
}

/*
 ***************************************************************************
 * Upgrade stats_disk structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_disk(struct activity *act[], int p, unsigned int magic,
			int endian_mismatch, int arch_64)
{
	int i;
	struct stats_disk *sdc;

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_disk_8a *sdp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			sdp = (struct stats_disk_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sdc = (struct stats_disk *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sdc->nr_ios = moveto_long_long(&sdp->nr_ios, endian_mismatch, arch_64);
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
	else {
		struct stats_disk_8b *sdp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			sdp = (struct stats_disk_8b *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sdc = (struct stats_disk *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sdc->nr_ios = sdp->nr_ios;
			memcpy(&sdc->rd_sect, &sdp->rd_sect, 8);
			memcpy(&sdc->wr_sect, &sdp->wr_sect, 8);
			sdc->rd_ticks = sdp->rd_ticks;
			sdc->wr_ticks = sdp->wr_ticks;
			sdc->tot_ticks = sdp->tot_ticks;
			sdc->rq_ticks = sdp->rq_ticks;
			sdc->major = sdp->major;
			sdc->minor = sdp->minor;
		}
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_dev structure (from ACTIVITY_MAGIC_BASE or
 * ACTIVITY_MAGIC_BASE + 1 format to ACTIVITY_MAGIC_BASE + 3 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_dev(struct activity *act[], int p, unsigned int magic,
			   int endian_mismatch, int arch_64)
{
	int i;
	struct stats_net_dev *sndc;

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_dev_8a *sndp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			sndp = (struct stats_net_dev_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sndc->rx_packets = moveto_long_long(&sndp->rx_packets, endian_mismatch, arch_64);
			sndc->tx_packets = moveto_long_long(&sndp->tx_packets, endian_mismatch, arch_64);
			sndc->rx_bytes = moveto_long_long(&sndp->rx_bytes, endian_mismatch, arch_64);
			sndc->tx_bytes = moveto_long_long(&sndp->tx_bytes, endian_mismatch, arch_64);
			sndc->rx_compressed = moveto_long_long(&sndp->rx_compressed, endian_mismatch, arch_64);
			sndc->tx_compressed = moveto_long_long(&sndp->tx_compressed, endian_mismatch, arch_64);
			sndc->multicast = moveto_long_long(&sndp->multicast, endian_mismatch, arch_64);
			sndc->speed = 0; /* New field */
			strncpy(sndc->interface, sndp->interface, sizeof(sndc->interface));
			sndc->interface[sizeof(sndc->interface) - 1] = '\0';
			sndc->duplex = '\0'; /* New field */
		}
	}
	else if (magic == ACTIVITY_MAGIC_BASE + 1) {
		struct stats_net_dev_8b *sndp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			sndp = (struct stats_net_dev_8b *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sndc->rx_packets = sndp->rx_packets;
			sndc->tx_packets = sndp->tx_packets;
			sndc->rx_bytes = sndp->rx_bytes;
			sndc->tx_bytes = sndp->tx_bytes;
			sndc->rx_compressed = sndp->rx_compressed;
			sndc->tx_compressed = sndp->tx_compressed;
			sndc->multicast = sndp->multicast;
			sndc->speed = 0; /* New field */
			strncpy(sndc->interface, sndp->interface, sizeof(sndc->interface));
			sndc->interface[sizeof(sndc->interface) - 1] = '\0';
			sndc->duplex = '\0'; /* New field */
		}
	}
	else {
		struct stats_net_dev_8c *sndp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			sndp = (struct stats_net_dev_8c *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			sndc->rx_packets = sndp->rx_packets;
			sndc->tx_packets = sndp->tx_packets;
			sndc->rx_bytes = sndp->rx_bytes;
			sndc->tx_bytes = sndp->tx_bytes;
			sndc->rx_compressed = sndp->rx_compressed;
			sndc->tx_compressed = sndp->tx_compressed;
			sndc->multicast = sndp->multicast;
			sndc->speed = sndp->speed;
			strncpy(sndc->interface, sndp->interface, sizeof(sndc->interface));
			sndc->interface[sizeof(sndc->interface) - 1] = '\0';
			sndc->duplex = sndp->duplex;
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
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_edev(struct activity *act[], int p, unsigned int magic,
			    int endian_mismatch, int arch_64)
{
	int i;
	struct stats_net_edev *snedc;

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_edev_8a *snedp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			snedp = (struct stats_net_edev_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			snedc = (struct stats_net_edev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			snedc->collisions = moveto_long_long(&snedp->collisions, endian_mismatch, arch_64);
			snedc->rx_errors = moveto_long_long(&snedp->rx_errors, endian_mismatch, arch_64);
			snedc->tx_errors = moveto_long_long(&snedp->tx_errors, endian_mismatch, arch_64);
			snedc->rx_dropped = moveto_long_long(&snedp->rx_dropped, endian_mismatch, arch_64);
			snedc->tx_dropped = moveto_long_long(&snedp->tx_dropped, endian_mismatch, arch_64);
			snedc->rx_fifo_errors = moveto_long_long(&snedp->rx_fifo_errors, endian_mismatch, arch_64);
			snedc->tx_fifo_errors = moveto_long_long(&snedp->tx_fifo_errors, endian_mismatch, arch_64);
			snedc->rx_frame_errors = moveto_long_long(&snedp->rx_frame_errors, endian_mismatch, arch_64);
			snedc->tx_carrier_errors = moveto_long_long(&snedp->tx_carrier_errors, endian_mismatch, arch_64);
			strncpy(snedc->interface, snedp->interface, sizeof(snedc->interface));
			snedc->interface[sizeof(snedc->interface) - 1] = '\0';
		}
	}
	else {
		struct stats_net_edev_8b *snedp;

		for (i = 0; i < act[p]->nr_ini; i++) {
			snedp = (struct stats_net_edev_8b *) ((char *) act[p]->buf[0] + i * act[p]->msize);
			snedc = (struct stats_net_edev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);

			snedc->collisions = snedp->collisions;
			snedc->rx_errors = snedp->rx_errors;
			snedc->tx_errors = snedp->tx_errors;
			snedc->rx_dropped = snedp->rx_dropped;
			snedc->tx_dropped = snedp->tx_dropped;
			snedc->rx_fifo_errors = snedp->rx_fifo_errors;
			snedc->tx_fifo_errors = snedp->tx_fifo_errors;
			snedc->rx_frame_errors = snedp->rx_frame_errors;
			snedc->tx_carrier_errors = snedp->tx_carrier_errors;
			strncpy(snedc->interface, snedp->interface, sizeof(snedc->interface));
			snedc->interface[sizeof(snedc->interface) - 1] = '\0';
		}
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
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_ip(struct activity *act[], int p, unsigned int magic,
			  int endian_mismatch, int arch_64)
{
	struct stats_net_ip *snic = (struct stats_net_ip *) act[p]->buf[1];

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_ip_8a *snip = (struct stats_net_ip_8a *) act[p]->buf[0];

		snic->InReceives = moveto_long_long(&snip->InReceives, endian_mismatch, arch_64);
		snic->ForwDatagrams = moveto_long_long(&snip->ForwDatagrams, endian_mismatch, arch_64);
		snic->InDelivers = moveto_long_long(&snip->InDelivers, endian_mismatch, arch_64);
		snic->OutRequests = moveto_long_long(&snip->OutRequests, endian_mismatch, arch_64);
		snic->ReasmReqds = moveto_long_long(&snip->ReasmReqds, endian_mismatch, arch_64);
		snic->ReasmOKs = moveto_long_long(&snip->ReasmOKs, endian_mismatch, arch_64);
		snic->FragOKs = moveto_long_long(&snip->FragOKs, endian_mismatch, arch_64);
		snic->FragCreates = moveto_long_long(&snip->FragCreates, endian_mismatch, arch_64);
	}
	else {
		struct stats_net_ip_8b *snip = (struct stats_net_ip_8b *) act[p]->buf[0];

		snic->InReceives = snip->InReceives;
		snic->ForwDatagrams = snip->ForwDatagrams;
		snic->InDelivers = snip->InDelivers;
		snic->OutRequests = snip->OutRequests;
		snic->ReasmReqds = snip->ReasmReqds;
		snic->ReasmOKs = snip->ReasmOKs;
		snic->FragOKs = snip->FragOKs;
		snic->FragCreates = snip->FragCreates;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_eip structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_eip(struct activity *act[], int p, unsigned int magic,
			   int endian_mismatch, int arch_64)
{
	struct stats_net_eip *sneic = (struct stats_net_eip *) act[p]->buf[1];

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_eip_8a *sneip = (struct stats_net_eip_8a *) act[p]->buf[0];

		sneic->InHdrErrors = moveto_long_long(&sneip->InHdrErrors, endian_mismatch, arch_64);
		sneic->InAddrErrors = moveto_long_long(&sneip->InAddrErrors, endian_mismatch, arch_64);
		sneic->InUnknownProtos = moveto_long_long(&sneip->InUnknownProtos, endian_mismatch, arch_64);
		sneic->InDiscards = moveto_long_long(&sneip->InDiscards, endian_mismatch, arch_64);
		sneic->OutDiscards = moveto_long_long(&sneip->OutDiscards, endian_mismatch, arch_64);
		sneic->OutNoRoutes = moveto_long_long(&sneip->OutNoRoutes, endian_mismatch, arch_64);
		sneic->ReasmFails = moveto_long_long(&sneip->ReasmFails, endian_mismatch, arch_64);
		sneic->FragFails = moveto_long_long(&sneip->FragFails, endian_mismatch, arch_64);
	}
	else {
		struct stats_net_eip_8b *sneip = (struct stats_net_eip_8b *) act[p]->buf[0];

		sneic->InHdrErrors = sneip->InHdrErrors;
		sneic->InAddrErrors = sneip->InAddrErrors;
		sneic->InUnknownProtos = sneip->InUnknownProtos;
		sneic->InDiscards = sneip->InDiscards;
		sneic->OutDiscards = sneip->OutDiscards;
		sneic->OutNoRoutes = sneip->OutNoRoutes;
		sneic->ReasmFails = sneip->ReasmFails;
		sneic->FragFails = sneip->FragFails;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_ip6 structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_ip6(struct activity *act[], int p, unsigned int magic,
			   int endian_mismatch, int arch_64)
{
	struct stats_net_ip6 *snic6 = (struct stats_net_ip6 *) act[p]->buf[1];

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_ip6_8a *snip6 = (struct stats_net_ip6_8a *) act[p]->buf[0];

		snic6->InReceives6 = moveto_long_long(&snip6->InReceives6, endian_mismatch, arch_64);
		snic6->OutForwDatagrams6 = moveto_long_long(&snip6->OutForwDatagrams6, endian_mismatch, arch_64);
		snic6->InDelivers6 = moveto_long_long(&snip6->InDelivers6, endian_mismatch, arch_64);
		snic6->OutRequests6 = moveto_long_long(&snip6->OutRequests6, endian_mismatch, arch_64);
		snic6->ReasmReqds6 = moveto_long_long(&snip6->ReasmReqds6, endian_mismatch, arch_64);
		snic6->ReasmOKs6 = moveto_long_long(&snip6->ReasmOKs6, endian_mismatch, arch_64);
		snic6->InMcastPkts6 = moveto_long_long(&snip6->InMcastPkts6, endian_mismatch, arch_64);
		snic6->OutMcastPkts6 = moveto_long_long(&snip6->OutMcastPkts6, endian_mismatch, arch_64);
		snic6->FragOKs6 = moveto_long_long(&snip6->FragOKs6, endian_mismatch, arch_64);
		snic6->FragCreates6 = moveto_long_long(&snip6->FragCreates6, endian_mismatch, arch_64);
	}
	else {
		struct stats_net_ip6_8b *snip6 = (struct stats_net_ip6_8b *) act[p]->buf[0];

		snic6->InReceives6 = snip6->InReceives6;
		snic6->OutForwDatagrams6 = snip6->OutForwDatagrams6;
		snic6->InDelivers6 = snip6->InDelivers6;
		snic6->OutRequests6 = snip6->OutRequests6;
		snic6->ReasmReqds6 = snip6->ReasmReqds6;
		snic6->ReasmOKs6 = snip6->ReasmOKs6;
		snic6->InMcastPkts6 = snip6->InMcastPkts6;
		snic6->OutMcastPkts6 = snip6->OutMcastPkts6;
		snic6->FragOKs6 = snip6->FragOKs6;
		snic6->FragCreates6 = snip6->FragCreates6;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_net_eip6 structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @magic	Structure format magic value.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_net_eip6(struct activity *act[], int p, unsigned int magic,
			    int endian_mismatch, int arch_64)
{
	struct stats_net_eip6 *sneic6 = (struct stats_net_eip6 *) act[p]->buf[1];

	if (magic == ACTIVITY_MAGIC_BASE) {
		struct stats_net_eip6_8a *sneip6 = (struct stats_net_eip6_8a *) act[p]->buf[0];

		sneic6->InHdrErrors6 = moveto_long_long(&sneip6->InHdrErrors6, endian_mismatch, arch_64);
		sneic6->InAddrErrors6 = moveto_long_long(&sneip6->InAddrErrors6, endian_mismatch, arch_64);
		sneic6->InUnknownProtos6 = moveto_long_long(&sneip6->InUnknownProtos6, endian_mismatch, arch_64);
		sneic6->InTooBigErrors6 = moveto_long_long(&sneip6->InTooBigErrors6, endian_mismatch, arch_64);
		sneic6->InDiscards6 = moveto_long_long(&sneip6->InDiscards6, endian_mismatch, arch_64);
		sneic6->OutDiscards6 = moveto_long_long(&sneip6->OutDiscards6, endian_mismatch, arch_64);
		sneic6->InNoRoutes6 = moveto_long_long(&sneip6->InNoRoutes6, endian_mismatch, arch_64);
		sneic6->OutNoRoutes6 = moveto_long_long(&sneip6->OutNoRoutes6, endian_mismatch, arch_64);
		sneic6->ReasmFails6 = moveto_long_long(&sneip6->ReasmFails6, endian_mismatch, arch_64);
		sneic6->FragFails6 = moveto_long_long(&sneip6->FragFails6, endian_mismatch, arch_64);
		sneic6->InTruncatedPkts6 = moveto_long_long(&sneip6->InTruncatedPkts6, endian_mismatch, arch_64);
	}
	else {
		struct stats_net_eip6_8b *sneip6 = (struct stats_net_eip6_8b *) act[p]->buf[0];

		sneic6->InHdrErrors6 = sneip6->InHdrErrors6;
		sneic6->InAddrErrors6 = sneip6->InAddrErrors6;
		sneic6->InUnknownProtos6 = sneip6->InUnknownProtos6;
		sneic6->InTooBigErrors6 = sneip6->InTooBigErrors6;
		sneic6->InDiscards6 = sneip6->InDiscards6;
		sneic6->OutDiscards6 = sneip6->OutDiscards6;
		sneic6->InNoRoutes6 = sneip6->InNoRoutes6;
		sneic6->OutNoRoutes6 = sneip6->OutNoRoutes6;
		sneic6->ReasmFails6 = sneip6->ReasmFails6;
		sneic6->FragFails6 = sneip6->FragFails6;
		sneic6->InTruncatedPkts6 = sneip6->InTruncatedPkts6;
	}
}

/*
 ***************************************************************************
 * Upgrade stats_huge structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 ***************************************************************************
 */
void upgrade_stats_huge(struct activity *act[], int p,
			int endian_mismatch, int arch_64)
{
	struct stats_huge *shc = (struct stats_huge *) act[p]->buf[1];
	struct stats_huge_8a *shp = (struct stats_huge_8a *) act[p]->buf[0];

	shc->frhkb = moveto_long_long(&shp->frhkb, endian_mismatch, arch_64);
	shc->tlhkb = moveto_long_long(&shp->tlhkb, endian_mismatch, arch_64);
}

/*
 ***************************************************************************
 * Upgrade stats_pwr_wghfreq structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
void upgrade_stats_pwr_wghfreq(struct activity *act[], int p)
{
	int i, k;

	struct stats_pwr_wghfreq *spc, *spc_k;
	struct stats_pwr_wghfreq_8a *spp, *spp_k;

	for (i = 0; i < act[p]->nr_ini; i++) {
		spp = (struct stats_pwr_wghfreq_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize * act[p]->nr2);
		spc = (struct stats_pwr_wghfreq *) ((char *) act[p]->buf[1] + i * act[p]->fsize * act[p]->nr2);

		for (k = 0; k < act[p]->nr2; k++) {
			spp_k = (struct stats_pwr_wghfreq_8a *) ((char *) spp + k * act[p]->msize);
			if (!spp_k->freq)
				break;
			spc_k = (struct stats_pwr_wghfreq *) ((char *) spc + k * act[p]->fsize);

			spc_k->time_in_state = spp_k->time_in_state;
			memcpy(&spc_k->freq, &spp_k->freq, 8);
		}
	}
}

/*
 ***************************************************************************
 * Upgrade stats_filesystem structure (from ACTIVITY_MAGIC_BASE format to
 * ACTIVITY_MAGIC_BASE + 1 format).
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 * @st_size	Size of the structure read from file.
 ***************************************************************************
 */
void upgrade_stats_filesystem(struct activity *act[], int p, int st_size)
{
	int i;
	struct stats_filesystem *sfc;
	struct stats_filesystem_8a *sfp;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sfp = (struct stats_filesystem_8a *) ((char *) act[p]->buf[0] + i * act[p]->msize);
		sfc = (struct stats_filesystem *)    ((char *) act[p]->buf[1] + i * act[p]->fsize);

		sfc->f_blocks = sfp->f_blocks;
		sfc->f_bfree = sfp->f_bfree;
		sfc->f_bavail = sfp->f_bavail;
		sfc->f_files = sfp->f_files;
		sfc->f_ffree = sfp->f_ffree;
		strncpy(sfc->fs_name, sfp->fs_name, sizeof(sfc->fs_name));
		sfc->fs_name[sizeof(sfc->fs_name) - 1] = '\0';

		if (st_size <= STATS_FILESYSTEM_8A_1_SIZE) {
			/* mountp didn't exist with older versions */
			sfc->mountp[0] = '\0';
		}
		else {
			strncpy(sfc->mountp, sfp->mountp, sizeof(sfc->mountp));
			sfc->mountp[sizeof(sfc->mountp) - 1] = '\0';
		}
	}
}

/*
 ***************************************************************************
 * Count number of stats_disk structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_disk(struct activity *act[], int p)
{
	int i;
	__nr_t nr_dsk = 0;
	struct stats_disk *sdc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sdc = (struct stats_disk *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!(sdc->major + sdc->minor))
			break;

		nr_dsk++;
	}

	return nr_dsk;
}

/*
 ***************************************************************************
 * Count number of stats_net_dev structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_net_dev(struct activity *act[], int p)
{
	int i;
	__nr_t nr_dev = 0;
	struct stats_net_dev *sndc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sndc = (struct stats_net_dev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!strcmp(sndc->interface, ""))
			break;

		nr_dev++;
	}

	return nr_dev;
}

/*
 ***************************************************************************
 * Count number of stats_net_edev structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_net_edev(struct activity *act[], int p)
{
	int i;
	__nr_t nr_edev = 0;
	struct stats_net_edev *snedc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		snedc = (struct stats_net_edev *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!strcmp(snedc->interface, ""))
			break;

		nr_edev++;
	}

	return nr_edev;
}

/*
 ***************************************************************************
 * Count number of stats_pwr_usb structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_pwr_usb(struct activity *act[], int p)
{
	int i;
	__nr_t nr_usb = 0;
	struct stats_pwr_usb *suc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		suc = (struct stats_pwr_usb *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!suc->bus_nr)
			break;

		nr_usb++;
	}

	return nr_usb;
}

/*
 ***************************************************************************
 * Count number of stats_filesystem structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_filesystem(struct activity *act[], int p)
{
	int i;
	__nr_t nr_fs = 0;
	struct stats_filesystem *sfc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sfc = (struct stats_filesystem *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!sfc->f_blocks)
			break;

		nr_fs++;
	}

	return nr_fs;
}

/*
 ***************************************************************************
 * Count number of stats_fchost structures that need to be written.
 *
 * IN:
 * @act		Array of activities.
 * @p		Position of activity in array.
 ***************************************************************************
 */
__nr_t count_stats_fchost(struct activity *act[], int p)
{
	int i;
	__nr_t nr_fc = 0;
	struct stats_fchost *sfcc;

	for (i = 0; i < act[p]->nr_ini; i++) {
		sfcc = (struct stats_fchost *) ((char *) act[p]->buf[1] + i * act[p]->fsize);
		if (!sfcc->fchost_name[0])
			break;

		nr_fc++;
	}

	return nr_fc;
}

/*
 ***************************************************************************
 * Upgrade file's activity list section.
 *
 * IN:
 * @stdfd		File descriptor for STDOUT.
 * @act			Array of activities.
 * @file_hdr		Pointer on file_header structure.
 * @ofile_actlst	Activity list in file.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 *
 * OUT:
 * @file_actlst		Activity list with up-to-date format.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_activity_section(int stdfd, struct activity *act[],
			     struct file_header *file_hdr,
			     struct old_file_activity *ofile_actlst,
			     struct file_activity **file_actlst,
			     int endian_mismatch, int arch_64)
{
	int i, j, p;
	struct old_file_activity *ofal;
	struct file_activity *fal, fa;

	fprintf(stderr, "file_activity: ");

	SREALLOC(*file_actlst, struct file_activity, FILE_ACTIVITY_SIZE * file_hdr->sa_act_nr);
	fal = *file_actlst;
	ofal = ofile_actlst;

	for (i = 0; i < file_hdr->sa_act_nr; i++, ofal++, fal++) {

		/* Every activity should be known at the moment (may change in the future) */
		p = get_activity_position(act, ofal->id, EXIT_IF_NOT_FOUND);
		fal->id = ofal->id;
		fal->nr = ofal->nr;
		fal->nr2 = ofal->nr2;
		fal->magic = act[p]->magic;	/* Update activity magic number */
		fal->has_nr = HAS_COUNT_FUNCTION(act[p]->options);
		/* Also update its size, which may have changed with recent versions */
		fal->size = act[p]->fsize;
		for (j = 0; j < 3; j++) {
			fal->types_nr[j] = act[p]->gtypes_nr[j];
		}

		memcpy(&fa, fal, FILE_ACTIVITY_SIZE);
		/* Restore endianness before writing */
		if (endian_mismatch) {
			/* Start swapping at field "header_size" position */
			swap_struct(act_types_nr, &fa, arch_64);
		}

		/*
		 * Even unknown activities must be written
		 * (they are counted in sa_act_nr).
		 */
		if (write_all(stdfd, &fa, FILE_ACTIVITY_SIZE) != FILE_ACTIVITY_SIZE) {
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
 * Upgrade a record header.
 *
 * IN:
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @orec_hdr	Record's header structure to convert.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_record_header(int fd, int stdfd, struct old_record_header *orec_hdr,
			  int endian_mismatch, int arch_64)
{
	struct record_header rec_hdr;

	memset(&rec_hdr, 0, sizeof(struct record_header));

	/* Convert current record header */
	rec_hdr.uptime_cs = orec_hdr->uptime0 * 100 / HZ;	/* Uptime in cs, not jiffies */
	rec_hdr.ust_time = (unsigned long long) orec_hdr->ust_time;
	rec_hdr.record_type = orec_hdr->record_type;
	rec_hdr.hour = orec_hdr->hour;
	rec_hdr.minute = orec_hdr->minute;
	rec_hdr.second = orec_hdr->second;

	/* Restore endianness before writing */
	if (endian_mismatch) {
		swap_struct(rec_types_nr, &rec_hdr, arch_64);
	}

	/* Write record header */
	if (write_all(stdfd, &rec_hdr, RECORD_HEADER_SIZE) != RECORD_HEADER_SIZE) {
		fprintf(stderr, "\nwrite: %s\n", strerror(errno));
		return -1;
	}

	fprintf(stderr, "H");

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
	sa_fread(fd, file_comment, sizeof(file_comment), HARD_SIZE, UEOF_STOP);
	file_comment[sizeof(file_comment) - 1] = '\0';

	/* Then write it. No changes at this time */
	if (write_all(stdfd, file_comment, sizeof(file_comment)) != sizeof(file_comment)) {
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
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @act		Array of activities.
 * @file_hdr	Pointer on file_header structure.
 * @previous_format
 *		TRUE is sa datafile has an old format which is no longer
 *		compatible with current one.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 * @vol_act_nr	Number of volatile activity structures.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_restart_record(int fd, int stdfd, struct activity *act[],
			   struct file_header *file_hdr, int previous_format,
			   int endian_mismatch, int arch_64, unsigned int vol_act_nr)
{

	int i, p;
	struct old_file_activity ofile_act;
	/* Number of cpu read in the activity list. See upgrade_header_section() */
	__nr_t cpu_nr = file_hdr->sa_cpu_nr;

	if (previous_format == FORMAT_MAGIC_2173) {
		/*
		 * For versions from 10.3.1 to 11.6.x,
		 * the restart record is followed by a list
		 * of volatile activity structures. Among them is A_CPU activity.
		 */
		for (i = 0; i < vol_act_nr; i++) {
			sa_fread(fd, &ofile_act, OLD_FILE_ACTIVITY_SIZE, HARD_SIZE, UEOF_STOP);

			/* Normalize endianness for file_activity structures */
			if (endian_mismatch) {
				swap_struct(oact_types_nr, &ofile_act, arch_64);
			}

			if (ofile_act.id && (ofile_act.nr > 0)) {
				p = get_activity_position(act, ofile_act.id, EXIT_IF_NOT_FOUND);
				act[p]->nr_ini = ofile_act.nr;

				if (ofile_act.id == A_CPU) {
					cpu_nr = ofile_act.nr;
				}
			}
		}
		/* Reallocate structures */
		allocate_structures(act);
	}

	/* Restore endianness before writing */
	if (endian_mismatch) {
		cpu_nr = __builtin_bswap32(cpu_nr);
	}

	/* Write new number of CPU following the restart record */
	if (write_all(stdfd, &cpu_nr, sizeof(__nr_t)) != sizeof(__nr_t)) {
		fprintf(stderr, "\nwrite: %s\n", strerror(errno));
		return -1;
	}

	fprintf(stderr, "R");

	return 0;
}

/*
 ***************************************************************************
 * Upgrade a record which is not a COMMENT or a RESTART one.
 *
 * IN:
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @act		Array of activities.
 * @file_hdr	Pointer on file_header structure (up-to-date format).
 * @ofile_actlst
 *		Activity list in file.
 * @file_actlst	Activity list in file (up-to-date format).
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_common_record(int fd, int stdfd, struct activity *act[], struct file_header *file_hdr,
			  struct old_file_activity *ofile_actlst, struct file_activity *file_actlst,
			  int endian_mismatch, int arch_64)
{
	int i, j, k, p;
	__nr_t nr_struct, nr;
	struct old_file_activity *ofal = ofile_actlst;
	struct file_activity *fal = file_actlst;
	char cc;

	/*
	 * This is not a special record, so read the extra fields,
	 * even if the format of the activity is unknown.
	 */
	for (i = 0; i < file_hdr->sa_act_nr; i++, ofal++, fal++) {

		/* Every activity should be known at the moment (may change in the future) */
		p = get_activity_position(act, fal->id, EXIT_IF_NOT_FOUND);

		/* Warning: Stats structures keep their original endianness here */
		if ((act[p]->nr_ini > 0) &&
		    ((act[p]->nr_ini > 1) || (act[p]->nr2 > 1)) &&
		    (act[p]->msize > ofal->size)) {
			for (j = 0; j < act[p]->nr_ini; j++) {
				for (k = 0; k < act[p]->nr2; k++) {
					sa_fread(fd,
						 (char *) act[p]->buf[0] + (j * act[p]->nr2 + k) * act[p]->msize,
						 (size_t) ofal->size, HARD_SIZE, UEOF_STOP);
				}
			}
		}
		else if (act[p]->nr_ini > 0) {
			sa_fread(fd, act[p]->buf[0],
				 (size_t) ofal->size * (size_t) act[p]->nr_ini * (size_t) act[p]->nr2,
				 HARD_SIZE, UEOF_STOP);
		}

		nr_struct = act[p]->nr_ini;
		/*
		 * NB: Cannot upgrade a stats structure with
		 * a magic number higher than currently known.
		 */
		if (ofal->magic < act[p]->magic) {
			cc = 'u';

			/* Known activity but old format */
			switch (fal->id) {

				case A_CPU:
					upgrade_stats_cpu(act, p, ofal->size);
					break;

				case A_PCSW:
					upgrade_stats_pcsw(act, p);
					break;

				case A_IRQ:
					upgrade_stats_irq(act, p);
					break;

				case A_IO:
					upgrade_stats_io(act, p, endian_mismatch);
					break;

				case A_QUEUE:
					upgrade_stats_queue(act, p, ofal->magic,
							    endian_mismatch, arch_64);
					break;

				case A_MEMORY:
					upgrade_stats_memory(act, p, ofal->size,
							     endian_mismatch, arch_64);
					break;

				case A_KTABLES:
					upgrade_stats_ktables(act, p, endian_mismatch);
					break;

				case A_SERIAL:
					nr_struct = upgrade_stats_serial(act, p, ofal->size,
									 endian_mismatch);
					break;

				case A_DISK:
					upgrade_stats_disk(act, p, ofal->magic,
							   endian_mismatch, arch_64);
					break;

				case A_NET_DEV:
					upgrade_stats_net_dev(act, p, ofal->magic,
							      endian_mismatch, arch_64);
					break;

				case A_NET_EDEV:
					upgrade_stats_net_edev(act, p, ofal->magic,
							       endian_mismatch, arch_64);
					break;

				case A_NET_IP:
					upgrade_stats_net_ip(act, p, ofal->magic,
							     endian_mismatch, arch_64);
					break;

				case A_NET_EIP:
					upgrade_stats_net_eip(act, p, ofal->magic,
							      endian_mismatch, arch_64);
					break;

				case A_NET_IP6:
					upgrade_stats_net_ip6(act, p, ofal->magic,
							      endian_mismatch, arch_64);
					break;

				case A_NET_EIP6:
					upgrade_stats_net_eip6(act, p, ofal->magic,
							       endian_mismatch, arch_64);
					break;

				case A_HUGE:
					upgrade_stats_huge(act, p, endian_mismatch, arch_64);
					break;

				case A_PWR_FREQ:
					upgrade_stats_pwr_wghfreq(act, p);
					break;

				case A_FS:
					upgrade_stats_filesystem(act, p, ofal->size);
					break;
				}
		}
		else {
			cc = '.';
			/* Known activity with current up-to-date format */
			for (j = 0; j < act[p]->nr_ini; j++) {
				for (k = 0; k < act[p]->nr2; k++) {
					memcpy((char *) act[p]->buf[1] + (j * act[p]->nr2 + k) * act[p]->msize,
					       (char *) act[p]->buf[0] + (j * act[p]->nr2 + k) * act[p]->msize,
					       fal->size);
				}
			}
		}

		if (fal->has_nr) {

			switch (fal->id) {

				case A_SERIAL:
					/* Nothing to do: Already done in upgrade_stats_serial() */
					break;

				case A_DISK:
					nr_struct = count_stats_disk(act, p);
					break;

				case A_NET_DEV:
					nr_struct = count_stats_net_dev(act, p);
					break;

				case A_NET_EDEV:
					nr_struct = count_stats_net_edev(act, p);
					break;

				case A_PWR_USB:
					nr_struct = count_stats_pwr_usb(act, p);
					break;

				case A_FS:
					nr_struct = count_stats_filesystem(act, p);
					break;

				case A_NET_FC:
					nr_struct = count_stats_fchost(act, p);
					break;
			}

			/* Restore endianness before writing */
			if (endian_mismatch) {
				nr = __builtin_bswap32(nr_struct);
			}
			else {
				nr = nr_struct;
			}

			/* Write number of structures for current activity */
			if (write_all(stdfd, &nr, sizeof(__nr_t)) != sizeof(__nr_t))
				goto write_error;

			fprintf(stderr, "n");
		}

		for (j = 0; j < nr_struct; j++) {
			for (k = 0; k < act[p]->nr2; k++) {
				if (write_all(stdfd,
					      (char *) act[p]->buf[1] + (j * act[p]->nr2 + k) * act[p]->fsize,
					       act[p]->fsize) != act[p]->fsize)
					goto write_error;
			}
		}
		fprintf(stderr, "%c", cc);
	}

	return 0;

write_error:

	fprintf(stderr, "\nwrite: %s\n", strerror(errno));

	return -1;

}

/*
 ***************************************************************************
 * Upgrade statistics records.
 *
 * IN:
 * @fd		File descriptor for sa datafile to convert.
 * @stdfd	File descriptor for STDOUT.
 * @act		Array of activities.
 * @file_hdr	Pointer on file_header structure.
 * @ofile_actlst
 *		Activity list in file.
 * @file_actlst	Activity list with up-to-date format.
 * @previous_format
 *		TRUE is sa datafile has an old format which is no longer * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.

 *		compatible with current one.
 * @endian_mismatch
 *		TRUE if data read from file don't match current	machine's
 *		endianness.
 * @arch_64	TRUE if file's data come from a 64-bit machine.
 * @vol_act_nr	Number of volatile activity structures.
 *
 * RETURNS:
 * -1 on error, 0 otherwise.
***************************************************************************
 */
int upgrade_stat_records(int fd, int stdfd, struct activity *act[], struct file_header *file_hdr,
			 struct old_file_activity *ofile_actlst, struct file_activity *file_actlst,
			 int previous_format, int endian_mismatch, int arch_64,
			 unsigned int vol_act_nr)
{
	int rtype;
	int eosaf;
	struct old_record_header orec_hdr;
	unsigned int orec_types_nr[] = {OLD_RECORD_HEADER_ULL_NR, OLD_RECORD_HEADER_UL_NR, OLD_RECORD_HEADER_U_NR};

	fprintf(stderr, _("Statistics:\n"));

	do {
		eosaf = sa_fread(fd, &orec_hdr, OLD_RECORD_HEADER_SIZE, SOFT_SIZE, UEOF_STOP);

		/* Normalize endianness */
		if (endian_mismatch) {
			swap_struct(orec_types_nr, &orec_hdr, arch_64);
		}
		rtype = orec_hdr.record_type;

		if (!eosaf) {
			/* Upgrade current record header */
			if (upgrade_record_header(fd, stdfd, &orec_hdr,
						  endian_mismatch, arch_64) < 0)
				return -1;

			if (rtype == R_COMMENT) {
				/* Upgrade the COMMENT record */
				if (upgrade_comment_record(fd, stdfd) < 0)
					return -1;
			}
			else if (rtype == R_RESTART) {
				/* Upgrade the RESTART record */
				if (upgrade_restart_record(fd, stdfd, act, file_hdr,
							   previous_format, endian_mismatch,
							   arch_64, vol_act_nr) < 0)
					return -1;
			}
			else {
				/* Upgrade current statistics record */
				if (upgrade_common_record(fd, stdfd, act, file_hdr, ofile_actlst,
							  file_actlst, endian_mismatch, arch_64) < 0)
					return -1;
			}
		}
	}
	while (!eosaf);

	fprintf(stderr, "\n");

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
	if (exit_code) {
		exit(exit_code);
	}
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
	int fd = 0, stdfd = 0, previous_format = 0;
	int arch_64 = TRUE;
	unsigned int vol_act_nr = 0, hdr_size;
	struct file_magic file_magic;
	struct file_header file_hdr;
	struct file_activity *file_actlst = NULL;
	struct old_file_activity *ofile_actlst = NULL;

	/* Open stdout */
	if ((stdfd = dup(STDOUT_FILENO)) < 0) {
		perror("dup");
		upgrade_exit(0, 0, 2);
	}

	/* Upgrade file's magic section */
	if (upgrade_magic_section(dfile, &fd, stdfd, &file_magic, &hdr_size,
				  &previous_format, &endian_mismatch) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}
	if (previous_format == FORMAT_MAGIC) {
		/* Nothing to do at the present time */
		fprintf(stderr, _("\nFile format already up-to-date\n"));
		goto success;
	}

	if (!user_hz) {
		/* Get HZ */
		get_HZ();
	}
	else {
		/* HZ set on the command line with option -O */
		hz = user_hz;
	}
	fprintf(stderr, _("HZ: Using current value: %lu\n"), HZ);

	/* Upgrade file's header section */
	if (upgrade_header_section(dfile, fd, stdfd, act, &file_magic,
				   &file_hdr, hdr_size, previous_format, &arch_64,
				   endian_mismatch, &vol_act_nr, &ofile_actlst) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	/* Upgrade file's activity list section */
	if (upgrade_activity_section(stdfd, act, &file_hdr,
				     ofile_actlst, &file_actlst,
				     endian_mismatch, arch_64) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	/* Perform required allocations */
	allocate_structures(act);

	/* Upgrade statistics records */
	if (upgrade_stat_records(fd, stdfd, act, &file_hdr, ofile_actlst, file_actlst,
				 previous_format, endian_mismatch, arch_64,
				 vol_act_nr) < 0) {
		upgrade_exit(fd, stdfd, 2);
	}

	free(file_actlst);
	free(ofile_actlst);
	free_structures(act);

	fprintf(stderr,
		_("File successfully converted to sysstat format version %s\n"),
		VERSION);

success:
	upgrade_exit(fd, stdfd, 0);
}
