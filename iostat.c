/*
 * iostat: report CPU and I/O statistics
 * (C) 1998-2018 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "version.h"
#include "iostat.h"
#include "ioconf.h"
#include "rd_stats.h"
#include "count.h"

#include <locale.h>	/* For setlocale() */
#ifdef USE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef USE_SCCSID
#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }
#endif

struct stats_cpu *st_cpu[2];
unsigned long long uptime_cs[2] = {0, 0};
unsigned long long tot_jiffies[2] = {0, 0};
struct io_stats *st_iodev[2];
struct io_hdr_stats *st_hdr_iodev;
struct io_dlist *st_dev_list;

/* Last group name entered on the command line */
char group_name[MAX_NAME_LEN];
/* Number of decimal places */
int dplaces_nr = -1;

int iodev_nr = 0;	/* Nb of devices and partitions found. Includes nb of device groups */
int group_nr = 0;	/* Nb of device groups */
int cpu_nr = 0;		/* Nb of processors on the machine */
int dlist_idx = 0;	/* Nb of devices entered on the command line */
int flags = 0;		/* Flag for common options and system state */
unsigned int dm_major;	/* Device-mapper major number */

long interval = 0;
char timestamp[TIMESTAMP_LEN];

struct sigaction alrm_act;

/*
 ***************************************************************************
 * Print usage and exit.
 *
 * IN:
 * @progname	Name of sysstat command.
 ***************************************************************************
 */
void usage(char *progname)
{
	fprintf(stderr, _("Usage: %s [ options ] [ <interval> [ <count> ] ]\n"),
		progname);
#ifdef DEBUG
	fprintf(stderr, _("Options are:\n"
			  "[ -c ] [ -d ] [ -h ] [ -k | -m ] [ -N ] [ -s ] [ -t ] [ -V ] [ -x ] [ -y ] [ -z ]\n"
			  "[ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ -o JSON ]\n"
			  "[ [ -H ] -g <group_name> ] [ -p [ <device> [,...] | ALL ] ]\n"
			  "[ <device> [...] | ALL ] [ --debuginfo ]\n"));
#else
	fprintf(stderr, _("Options are:\n"
			  "[ -c ] [ -d ] [ -h ] [ -k | -m ] [ -N ] [ -s ] [ -t ] [ -V ] [ -x ] [ -y ] [ -z ]\n"
			  "[ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ -o JSON ]\n"
			  "[ [ -H ] -g <group_name> ] [ -p [ <device> [,...] | ALL ] ]\n"
			  "[ <device> [...] | ALL ]\n"));
#endif
	exit(1);
}

/*
 ***************************************************************************
 * Set disk output unit. Unit will be kB/s unless POSIXLY_CORRECT
 * environment variable has been set, in which case the output will be
 * expressed in blocks/s.
 ***************************************************************************
 */
void set_disk_output_unit(void)
{
	if (DISPLAY_KILOBYTES(flags) || DISPLAY_MEGABYTES(flags))
		return;

	/* Check POSIXLY_CORRECT environment variable */
	if (getenv(ENV_POSIXLY_CORRECT) == NULL) {
		/* Variable not set: Unit is kB/s and not blocks/s */
		flags |= I_D_KILOBYTES;
	}
}

/*
 ***************************************************************************
 * SIGALRM signal handler. No need to reset the handler here.
 *
 * IN:
 * @sig	Signal number.
 ***************************************************************************
 */
void alarm_handler(int sig)
{
	alarm(interval);
}

/*
 ***************************************************************************
 * Initialize stats common structures.
 ***************************************************************************
 */
void init_stats(void)
{
	int i;

	/* Allocate structures for CPUs "all" and 0 */
	for (i = 0; i < 2; i++) {
		if ((st_cpu[i] = (struct stats_cpu *) malloc(STATS_CPU_SIZE * 2)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_cpu[i], 0, STATS_CPU_SIZE * 2);
	}
}

/*
 ***************************************************************************
 * Set every device entry to unregistered status. But don't change status
 * for group entries (whose status is DISK_GROUP).
 *
 * IN:
 * @iodev_nr		Number of devices and partitions.
 * @st_hdr_iodev	Pointer on first structure describing a device/partition.
 ***************************************************************************
 */
void set_entries_unregistered(int iodev_nr, struct io_hdr_stats *st_hdr_iodev)
{
	int i;
	struct io_hdr_stats *shi = st_hdr_iodev;

	for (i = 0; i < iodev_nr; i++, shi++) {
		if (shi->status == DISK_REGISTERED) {
			shi->status = DISK_UNREGISTERED;
		}
	}
}

/*
 ***************************************************************************
 * Free unregistered entries (mark them as unused).
 *
 * IN:
 * @iodev_nr		Number of devices and partitions.
 * @st_hdr_iodev	Pointer on first structure describing a device/partition.
 ***************************************************************************
 */
void free_unregistered_entries(int iodev_nr, struct io_hdr_stats *st_hdr_iodev)
{
	int i;
	struct io_hdr_stats *shi = st_hdr_iodev;

	for (i = 0; i < iodev_nr; i++, shi++) {
		if (shi->status == DISK_UNREGISTERED) {
			shi->used = FALSE;
		}
	}
}

/*
 ***************************************************************************
 * Allocate and init I/O device structures.
 *
 * IN:
 * @dev_nr	Number of devices and partitions (also including groups
 *		if option -g has been used).
 ***************************************************************************
 */
void salloc_device(int dev_nr)
{
	int i;

	for (i = 0; i < 2; i++) {
		if ((st_iodev[i] =
		     (struct io_stats *) malloc(IO_STATS_SIZE * dev_nr)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_iodev[i], 0, IO_STATS_SIZE * dev_nr);
	}

	if ((st_hdr_iodev =
	     (struct io_hdr_stats *) malloc(IO_HDR_STATS_SIZE * dev_nr)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(st_hdr_iodev, 0, IO_HDR_STATS_SIZE * dev_nr);
}

/*
 ***************************************************************************
 * Allocate structures for devices entered on the command line.
 *
 * IN:
 * @list_len	Number of arguments on the command line.
 ***************************************************************************
 */
void salloc_dev_list(int list_len)
{
	if ((st_dev_list = (struct io_dlist *) malloc(IO_DLIST_SIZE * list_len)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(st_dev_list, 0, IO_DLIST_SIZE * list_len);
}

/*
 ***************************************************************************
 * Free structures used for devices entered on the command line.
 ***************************************************************************
 */
void sfree_dev_list(void)
{
	free(st_dev_list);
}

/*
 ***************************************************************************
 * Look for the device in the device list and store it if not found.
 *
 * IN:
 * @dlist_idx	Length of the device list.
 * @device_name	Name of the device.
 *
 * OUT:
 * @dlist_idx	Length of the device list.
 *
 * RETURNS:
 * Position of the device in the list.
 ***************************************************************************
 */
int update_dev_list(int *dlist_idx, char *device_name)
{
	int i;
	struct io_dlist *sdli = st_dev_list;

	for (i = 0; i < *dlist_idx; i++, sdli++) {
		if (!strcmp(sdli->dev_name, device_name))
			break;
	}

	if (i == *dlist_idx) {
		/*
		 * Device not found: Store it.
		 * Group names will be distinguished from real device names
		 * thanks to their names which begin with a space.
		 */
		(*dlist_idx)++;
		strncpy(sdli->dev_name, device_name, MAX_NAME_LEN - 1);
	}

	return i;
}

/*
 ***************************************************************************
 * Allocate and init structures, according to system state.
 ***************************************************************************
 */
void io_sys_init(void)
{
	/* Allocate and init stat common counters */
	init_stats();

	/* How many processors on this machine? */
	cpu_nr = get_cpu_nr(~0, FALSE);

	/* Get number of block devices and partitions in /proc/diskstats */
	if ((iodev_nr = get_diskstats_dev_nr(CNT_PART, CNT_ALL_DEV)) > 0) {
		flags |= I_F_HAS_DISKSTATS;
		iodev_nr += NR_DEV_PREALLOC;
	}

	if (!HAS_DISKSTATS(flags) ||
	    (DISPLAY_PARTITIONS(flags) && !DISPLAY_PART_ALL(flags))) {
		/*
		 * If /proc/diskstats exists but we also want stats for the partitions
		 * of a particular device, stats will have to be found in /sys. So we
		 * need to know if /sys is mounted or not, and set flags accordingly.
		 */

		/* Get number of block devices (and partitions) in sysfs */
		if ((iodev_nr = get_sysfs_dev_nr(DISPLAY_PARTITIONS(flags))) > 0) {
			flags |= I_F_HAS_SYSFS;
			iodev_nr += NR_DEV_PREALLOC;
		}
		else {
			fprintf(stderr, _("Cannot find disk data\n"));
			exit(2);
		}
	}

	/* Also allocate stat structures for "group" devices */
	iodev_nr += group_nr;

	/*
	 * Allocate structures for number of disks found, but also
	 * for groups of devices if option -g has been entered on the command line.
	 * iodev_nr must be <> 0.
	 */
	salloc_device(iodev_nr);
}

/*
 ***************************************************************************
 * When group stats are to be displayed (option -g entered on the command
 * line), save devices and group names in the io_hdr_stats structures. This
 * is normally done later when stats are actually read from /proc or /sys
 * files (via a call to save_stats() function), but here we want to make
 * sure that the structures are ordered and that each device belongs to its
 * proper group.
 * Note that we can still have an unexpected device that gets attached to a
 * group as devices can be registered or unregistered dynamically.
 ***************************************************************************
 */
void presave_device_list(void)
{
	int i;
	struct io_hdr_stats *shi = st_hdr_iodev;
	struct io_dlist *sdli = st_dev_list;

	if (dlist_idx>0) {
		/* First, save the last group name entered on the command line in the list */
		update_dev_list(&dlist_idx, group_name);

		/* Now save devices and group names in the io_hdr_stats structures */
		for (i = 0; (i < dlist_idx) && (i < iodev_nr); i++, shi++, sdli++) {
			strncpy(shi->name, sdli->dev_name, MAX_NAME_LEN);
			shi->name[MAX_NAME_LEN - 1] = '\0';
			shi->used = TRUE;
			if (shi->name[0] == ' ') {
				/* Current device name is in fact the name of a group */
				shi->status = DISK_GROUP;
			}
			else {
				shi->status = DISK_REGISTERED;
			}
		}
	}
	else {
		/*
		 * No device names have been entered on the command line but
		 * the name of a group. Save that name at the end of the
		 * data table so that all devices that will be read will be
		 * included in that group.
		 */
		shi += iodev_nr - 1;
		strncpy(shi->name, group_name, MAX_NAME_LEN);
		shi->name[MAX_NAME_LEN - 1] = '\0';
		shi->used = TRUE;
		shi->status = DISK_GROUP;
	}
}

/*
 ***************************************************************************
 * Free various structures.
 ***************************************************************************
*/
void io_sys_free(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		/* Free CPU structures */
		free(st_cpu[i]);

		/* Free I/O device structures */
		free(st_iodev[i]);
	}

	free(st_hdr_iodev);
}

/*
 ***************************************************************************
 * Save stats for current device or partition.
 *
 * IN:
 * @name		Name of the device/partition.
 * @curr		Index in array for current sample statistics.
 * @st_io		Structure with device or partition to save.
 * @iodev_nr		Number of devices and partitions.
 * @st_hdr_iodev	Pointer on structures describing a device/partition.
 *
 * OUT:
 * @st_hdr_iodev	Pointer on structures describing a device/partition.
 ***************************************************************************
 */
void save_stats(char *name, int curr, void *st_io, int iodev_nr,
		struct io_hdr_stats *st_hdr_iodev)
{
	int i;
	struct io_hdr_stats *st_hdr_iodev_i;
	struct io_stats *st_iodev_i;

	/* Look for device in data table */
	for (i = 0; i < iodev_nr; i++) {
		st_hdr_iodev_i = st_hdr_iodev + i;
		if (!strcmp(st_hdr_iodev_i->name, name)) {
			break;
		}
	}

	if (i == iodev_nr) {
		/*
		 * This is a new device: Look for an unused entry to store it.
		 * Thus we are able to handle dynamically registered devices.
		 */
		for (i = 0; i < iodev_nr; i++) {
			st_hdr_iodev_i = st_hdr_iodev + i;
			if (!st_hdr_iodev_i->used) {
				/* Unused entry found... */
				st_hdr_iodev_i->used = TRUE; /* Indicate it is now used */
				strncpy(st_hdr_iodev_i->name, name, MAX_NAME_LEN - 1);
				st_hdr_iodev_i->name[MAX_NAME_LEN - 1] = '\0';
				st_iodev_i = st_iodev[!curr] + i;
				memset(st_iodev_i, 0, IO_STATS_SIZE);
				break;
			}
		}
	}
	if (i < iodev_nr) {
		st_hdr_iodev_i = st_hdr_iodev + i;
		if (st_hdr_iodev_i->status == DISK_UNREGISTERED) {
			st_hdr_iodev_i->status = DISK_REGISTERED;
			if (st_hdr_iodev_i->used == FALSE) {
				st_iodev_i = st_iodev[!curr] + i;
				memset(st_iodev_i, 0, IO_STATS_SIZE);
				st_hdr_iodev_i->used = TRUE;
			}
		}
		st_iodev_i = st_iodev[curr] + i;
		*st_iodev_i = *((struct io_stats *) st_io);
	}
	/*
	 * else it was a new device
	 * but there was no free structure to store it.
	 */
}

/*
 ***************************************************************************
 * Read sysfs stat for current block device or partition.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @filename	File name where stats will be read.
 * @dev_name	Device or partition name.
 *
 * RETURNS:
 * 0 if file couldn't be opened, 1 otherwise.
 ***************************************************************************
 */
int read_sysfs_file_stat(int curr, char *filename, char *dev_name)
{
	FILE *fp;
	struct io_stats sdev;
	int i;
	unsigned int ios_pgr, tot_ticks, rq_ticks, wr_ticks;
	unsigned long rd_ios, rd_merges_or_rd_sec, wr_ios, wr_merges;
	unsigned long rd_sec_or_wr_ios, wr_sec, rd_ticks_or_wr_sec;

	/* Try to read given stat file */
	if ((fp = fopen(filename, "r")) == NULL)
		return 0;

	i = fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu %u %u %u %u",
		   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
		   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks);

	if (i == 11) {
		/* Device or partition */
		sdev.rd_ios     = rd_ios;
		sdev.rd_merges  = rd_merges_or_rd_sec;
		sdev.rd_sectors = rd_sec_or_wr_ios;
		sdev.rd_ticks   = (unsigned int) rd_ticks_or_wr_sec;
		sdev.wr_ios     = wr_ios;
		sdev.wr_merges  = wr_merges;
		sdev.wr_sectors = wr_sec;
		sdev.wr_ticks   = wr_ticks;
		sdev.ios_pgr    = ios_pgr;
		sdev.tot_ticks  = tot_ticks;
		sdev.rq_ticks   = rq_ticks;
	}
	else if (i == 4) {
		/* Partition without extended statistics */
		sdev.rd_ios     = rd_ios;
		sdev.rd_sectors = rd_merges_or_rd_sec;
		sdev.wr_ios     = rd_sec_or_wr_ios;
		sdev.wr_sectors = rd_ticks_or_wr_sec;
	}

	if ((i == 11) || !DISPLAY_EXTENDED(flags)) {
		/*
		 * In fact, we _don't_ save stats if it's a partition without
		 * extended stats and yet we want to display ext stats.
		 */
		save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);
	}

	fclose(fp);

	return 1;
}

/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a device.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dev_name	Device name.
 ***************************************************************************
 */
void read_sysfs_dlist_part_stat(int curr, char *dev_name)
{
	DIR *dir;
	struct dirent *drd;
	char dfile[MAX_PF_NAME], filename[MAX_PF_NAME + 512];

	snprintf(dfile, sizeof(dfile), "%s/%s", SYSFS_BLOCK, dev_name);
	dfile[sizeof(dfile) - 1] = '\0';

	/* Open current device directory in /sys/block */
	if ((dir = opendir(dfile)) == NULL)
		return;

	/* Get current entry */
	while ((drd = readdir(dir)) != NULL) {
		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(filename, sizeof(filename), "%s/%s/%s", dfile, drd->d_name, S_STAT);
		filename[sizeof(filename) - 1] = '\0';

		/* Read current partition stats */
		read_sysfs_file_stat(curr, filename, drd->d_name);
	}

	/* Close device directory */
	closedir(dir);
}

/*
 ***************************************************************************
 * Read stats from the sysfs filesystem for the devices entered on the
 * command line.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_sysfs_dlist_stat(int curr)
{
	int dev, ok;
	char filename[MAX_PF_NAME];
	char *slash;
	struct io_dlist *st_dev_list_i;

	/* Every I/O device (or partition) is potentially unregistered */
	set_entries_unregistered(iodev_nr, st_hdr_iodev);

	for (dev = 0; dev < dlist_idx; dev++) {
		st_dev_list_i = st_dev_list + dev;

		/* Some devices may have a slash in their name (eg. cciss/c0d0...) */
		while ((slash = strchr(st_dev_list_i->dev_name, '/'))) {
			*slash = '!';
		}

		snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
			 SYSFS_BLOCK, st_dev_list_i->dev_name, S_STAT);
		filename[MAX_PF_NAME - 1] = '\0';

		/* Read device stats */
		ok = read_sysfs_file_stat(curr, filename, st_dev_list_i->dev_name);

		if (ok && st_dev_list_i->disp_part) {
			/* Also read stats for its partitions */
			read_sysfs_dlist_part_stat(curr, st_dev_list_i->dev_name);
		}
	}

	/* Free structures corresponding to unregistered devices */
	free_unregistered_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read stats from the sysfs filesystem for every block devices found.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_sysfs_stat(int curr)
{
	DIR *dir;
	struct dirent *drd;
	char filename[MAX_PF_NAME];
	int ok;

	/* Every I/O device entry is potentially unregistered */
	set_entries_unregistered(iodev_nr, st_hdr_iodev);

	/* Open /sys/block directory */
	if ((dir = opendir(SYSFS_BLOCK)) != NULL) {

		/* Get current entry */
		while ((drd = readdir(dir)) != NULL) {
			if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
				continue;
			snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
				 SYSFS_BLOCK, drd->d_name, S_STAT);
			filename[MAX_PF_NAME - 1] = '\0';

			/* If current entry is a directory, try to read its stat file */
			ok = read_sysfs_file_stat(curr, filename, drd->d_name);

			/*
			 * If '-p ALL' was entered on the command line,
			 * also try to read stats for its partitions
			 */
			if (ok && DISPLAY_PART_ALL(flags)) {
				read_sysfs_dlist_part_stat(curr, drd->d_name);
			}
		}

		/* Close /sys/block directory */
		closedir(dir);
	}

	/* Free structures corresponding to unregistered devices */
	free_unregistered_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Read stats from /proc/diskstats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_diskstats_stat(int curr)
{
	FILE *fp;
	char line[256], dev_name[MAX_NAME_LEN];
	char *dm_name;
	struct io_stats sdev;
	int i;
	unsigned int ios_pgr, tot_ticks, rq_ticks, wr_ticks;
	unsigned long rd_ios, rd_merges_or_rd_sec, rd_ticks_or_wr_sec, wr_ios;
	unsigned long wr_merges, rd_sec_or_wr_ios, wr_sec;
	char *ioc_dname;
	unsigned int major, minor;

	/* Every I/O device entry is potentially unregistered */
	set_entries_unregistered(iodev_nr, st_hdr_iodev);

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		return;

	while (fgets(line, sizeof(line), fp) != NULL) {

		/* major minor name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq */
		i = sscanf(line, "%u %u %s %lu %lu %lu %lu %lu %lu %lu %u %u %u %u",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
			   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks);

		if (i == 14) {
			/* Device or partition */
			if (!dlist_idx && !DISPLAY_PARTITIONS(flags) &&
			    !is_device(dev_name, ACCEPT_VIRTUAL_DEVICES))
				continue;
			sdev.rd_ios     = rd_ios;
			sdev.rd_merges  = rd_merges_or_rd_sec;
			sdev.rd_sectors = rd_sec_or_wr_ios;
			sdev.rd_ticks   = (unsigned int) rd_ticks_or_wr_sec;
			sdev.wr_ios     = wr_ios;
			sdev.wr_merges  = wr_merges;
			sdev.wr_sectors = wr_sec;
			sdev.wr_ticks   = wr_ticks;
			sdev.ios_pgr    = ios_pgr;
			sdev.tot_ticks  = tot_ticks;
			sdev.rq_ticks   = rq_ticks;
		}
		else if (i == 7) {
			/* Partition without extended statistics */
			if (DISPLAY_EXTENDED(flags) ||
			    (!dlist_idx && !DISPLAY_PARTITIONS(flags)))
				continue;

			sdev.rd_ios     = rd_ios;
			sdev.rd_sectors = rd_merges_or_rd_sec;
			sdev.wr_ios     = rd_sec_or_wr_ios;
			sdev.wr_sectors = rd_ticks_or_wr_sec;
		}
		else
			/* Unknown entry: Ignore it */
			continue;

		if ((ioc_dname = ioc_name(major, minor)) != NULL) {
			if (strcmp(dev_name, ioc_dname) && strcmp(ioc_dname, K_NODEV)) {
				/*
				 * No match: Use name generated from sysstat.ioconf data
				 * (if different from "nodev") works around known issues
				 * with EMC PowerPath.
				 */
				strncpy(dev_name, ioc_dname, MAX_NAME_LEN - 1);
				dev_name[MAX_NAME_LEN - 1] = '\0';
			}
		}

		if ((DISPLAY_DEVMAP_NAME(flags)) && (major == dm_major)) {
			/*
			 * If the device is a device mapper device, try to get its
			 * assigned name of its logical device.
			 */
			dm_name = transform_devmapname(major, minor);
			if (dm_name) {
				strncpy(dev_name, dm_name, MAX_NAME_LEN - 1);
				dev_name[MAX_NAME_LEN - 1] = '\0';
			}
		}

		save_stats(dev_name, curr, &sdev, iodev_nr, st_hdr_iodev);
	}
	fclose(fp);

	/* Free structures corresponding to unregistered devices */
	free_unregistered_entries(iodev_nr, st_hdr_iodev);
}

/*
 ***************************************************************************
 * Compute stats for device groups using stats of every device belonging
 * to each of these groups.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void compute_device_groups_stats(int curr)
{
	struct io_stats gdev, *ioi;
	struct io_hdr_stats *shi = st_hdr_iodev;
	int i, nr_disks;

	memset(&gdev, 0, IO_STATS_SIZE);
	nr_disks = 0;

	for (i = 0; i < iodev_nr; i++, shi++) {
		if (shi->used && (shi->status == DISK_REGISTERED)) {
			ioi = st_iodev[curr] + i;

			if (!DISPLAY_UNFILTERED(flags)) {
				if (!ioi->rd_ios && !ioi->wr_ios)
					continue;
			}

			gdev.rd_ios     += ioi->rd_ios;
			gdev.rd_merges  += ioi->rd_merges;
			gdev.rd_sectors += ioi->rd_sectors;
			gdev.rd_ticks   += ioi->rd_ticks;
			gdev.wr_ios     += ioi->wr_ios;
			gdev.wr_merges  += ioi->wr_merges;
			gdev.wr_sectors += ioi->wr_sectors;
			gdev.wr_ticks   += ioi->wr_ticks;
			gdev.ios_pgr    += ioi->ios_pgr;
			gdev.tot_ticks  += ioi->tot_ticks;
			gdev.rq_ticks   += ioi->rq_ticks;
			nr_disks++;
		}
		else if (shi->status == DISK_GROUP) {
			save_stats(shi->name, curr, &gdev, iodev_nr, st_hdr_iodev);
			shi->used = nr_disks;
			nr_disks = 0;
			memset(&gdev, 0, IO_STATS_SIZE);
		}
	}
}

/*
 ***************************************************************************
 * Write current sample's timestamp, either in plain or JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void write_sample_timestamp(int tab, struct tm *rectime)
{
	if (DISPLAY_ISO(flags)) {
		strftime(timestamp, sizeof(timestamp), "%FT%T%z", rectime);
	}
	else {
		strftime(timestamp, sizeof(timestamp), "%x %X", rectime);
	}
	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf(tab, "\"timestamp\": \"%s\",", timestamp);
	}
	else {
		printf("%s\n", timestamp);
	}
}

/*
 ***************************************************************************
 * Display CPU utilization in plain format.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 ***************************************************************************
 */
void write_plain_cpu_stat(int curr, unsigned long long deltot_jiffies)
{
	printf("avg-cpu:  %%user   %%nice %%system %%iowait  %%steal   %%idle\n");

	printf("       ");
	cprintf_pc(DISPLAY_UNIT(flags), 6, 7, 2,
		   ll_sp_value(st_cpu[!curr]->cpu_user, st_cpu[curr]->cpu_user, deltot_jiffies),
		   ll_sp_value(st_cpu[!curr]->cpu_nice, st_cpu[curr]->cpu_nice, deltot_jiffies),
		   /*
		    * Time spent in system mode also includes time spent servicing
		    * hard and soft interrupts.
		    */
		   ll_sp_value(st_cpu[!curr]->cpu_sys + st_cpu[!curr]->cpu_softirq +
			       st_cpu[!curr]->cpu_hardirq,
			       st_cpu[curr]->cpu_sys + st_cpu[curr]->cpu_softirq +
			       st_cpu[curr]->cpu_hardirq, deltot_jiffies),
		   ll_sp_value(st_cpu[!curr]->cpu_iowait, st_cpu[curr]->cpu_iowait, deltot_jiffies),
		   ll_sp_value(st_cpu[!curr]->cpu_steal, st_cpu[curr]->cpu_steal, deltot_jiffies),
		   (st_cpu[curr]->cpu_idle < st_cpu[!curr]->cpu_idle) ?
		   0.0 :
		   ll_sp_value(st_cpu[!curr]->cpu_idle, st_cpu[curr]->cpu_idle, deltot_jiffies));

	printf("\n\n");
}

/*
 ***************************************************************************
 * Display CPU utilization in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @curr	Index in array for current sample statistics.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 ***************************************************************************
 */
void write_json_cpu_stat(int tab, int curr, unsigned long long deltot_jiffies)
{
	xprintf0(tab, "\"avg-cpu\":  {\"user\": %.2f, \"nice\": %.2f, \"system\": %.2f,"
		      " \"iowait\": %.2f, \"steal\": %.2f, \"idle\": %.2f}",
		 ll_sp_value(st_cpu[!curr]->cpu_user, st_cpu[curr]->cpu_user, deltot_jiffies),
		 ll_sp_value(st_cpu[!curr]->cpu_nice, st_cpu[curr]->cpu_nice, deltot_jiffies),
		 /*
		  * Time spent in system mode also includes time spent servicing
		  * hard and soft interrupts.
		  */
		 ll_sp_value(st_cpu[!curr]->cpu_sys + st_cpu[!curr]->cpu_softirq +
			     st_cpu[!curr]->cpu_hardirq,
			     st_cpu[curr]->cpu_sys + st_cpu[curr]->cpu_softirq +
			     st_cpu[curr]->cpu_hardirq, deltot_jiffies),
		 ll_sp_value(st_cpu[!curr]->cpu_iowait, st_cpu[curr]->cpu_iowait, deltot_jiffies),
		 ll_sp_value(st_cpu[!curr]->cpu_steal, st_cpu[curr]->cpu_steal, deltot_jiffies),
		 (st_cpu[curr]->cpu_idle < st_cpu[!curr]->cpu_idle) ?
		 0.0 :
		 ll_sp_value(st_cpu[!curr]->cpu_idle, st_cpu[curr]->cpu_idle, deltot_jiffies));
}

/*
 ***************************************************************************
 * Display CPU utilization in plain or JSON format.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @tab		Number of tabs to print (JSON format only).
 ***************************************************************************
 */
void write_cpu_stat(int curr, int tab)
{
	unsigned long long deltot_jiffies;

	/*
	 * Compute the total number of jiffies spent by all processors.
	 * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
	 * already include them.
	 */
	tot_jiffies[curr] = st_cpu[curr]->cpu_user + st_cpu[curr]->cpu_nice +
			    st_cpu[curr]->cpu_sys + st_cpu[curr]->cpu_idle +
			    st_cpu[curr]->cpu_iowait + st_cpu[curr]->cpu_hardirq +
			    st_cpu[curr]->cpu_steal + st_cpu[curr]->cpu_softirq;

	/* Total number of jiffies spent on the interval */
	deltot_jiffies = get_interval(tot_jiffies[!curr], tot_jiffies[curr]);

#ifdef DEBUG
		if (DISPLAY_DEBUG(flags)) {
			/* Debug output */
			fprintf(stderr, "deltot_jiffies=%llu st_cpu[curr]{ cpu_user=%llu cpu_nice=%llu "
					"cpu_sys=%llu cpu_idle=%llu cpu_iowait=%llu cpu_steal=%llu "
					"cpu_hardirq=%llu cpu_softirq=%llu cpu_guest=%llu "
					"cpu_guest_nice=%llu }\n",
				deltot_jiffies,
				st_cpu[curr]->cpu_user,
				st_cpu[curr]->cpu_nice,
				st_cpu[curr]->cpu_sys,
				st_cpu[curr]->cpu_idle,
				st_cpu[curr]->cpu_iowait,
				st_cpu[curr]->cpu_steal,
				st_cpu[curr]->cpu_hardirq,
				st_cpu[curr]->cpu_softirq,
				st_cpu[curr]->cpu_guest,
				st_cpu[curr]->cpu_guest_nice);
		}
#endif

	if (DISPLAY_JSON_OUTPUT(flags)) {
		write_json_cpu_stat(tab, curr, deltot_jiffies);
	}
	else {
		write_plain_cpu_stat(curr, deltot_jiffies);
	}
}

/*
 ***************************************************************************
 * Display disk stats header in plain or JSON format.
 *
 * OUT:
 * @fctr	Conversion factor.
 * @tab		Number of tabs to print (JSON format only).
 ***************************************************************************
 */
void write_disk_stat_header(int *fctr, int *tab)
{
	if (DISPLAY_KILOBYTES(flags)) {
		*fctr = 2;
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		*fctr = 2048;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf((*tab)++, "\"disk\": [");
		return;
	}

	if (!DISPLAY_HUMAN_READ(flags)) {
		printf("Device       ");
	}
	if (DISPLAY_EXTENDED(flags)) {
		/* Extended stats */
		if (DISPLAY_SHORT_OUTPUT(flags)) {
			printf("      tps");
			if (DISPLAY_MEGABYTES(flags)) {
				printf("      MB/s");
			}
			else if (DISPLAY_KILOBYTES(flags)) {
				printf("      kB/s");
			}
			else {
				printf("     sec/s");
			}
			printf("    rqm/s   await aqu-sz  areq-sz  %%util");
		}
		else {
			printf("     r/s     w/s");
			if (DISPLAY_MEGABYTES(flags)) {
				printf("     rMB/s     wMB/s");
			}
			else if (DISPLAY_KILOBYTES(flags)) {
				printf("     rkB/s     wkB/s");
			}
			else {
				printf("    rsec/s    wsec/s");
			}
			printf("   rrqm/s   wrqm/s  %%rrqm  %%wrqm r_await w_await"
			       " aqu-sz rareq-sz wareq-sz  svctm  %%util");
		}
	}
	else {
		/* Basic stats */
		printf("      tps");
		if (DISPLAY_KILOBYTES(flags)) {
			printf("    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn");
		}
		else if (DISPLAY_MEGABYTES(flags)) {
			printf("    MB_read/s    MB_wrtn/s    MB_read    MB_wrtn");
		}
		else {
			printf("   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn");
		}
	}
	if (DISPLAY_HUMAN_READ(flags)) {
		printf(" Device");
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys,
 * in plain format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @xds		Extended stats for current device.
 * @xios	Additional extended statistics for current device.
 ***************************************************************************
 */
void write_plain_ext_stat(unsigned long long itv, int fctr,
			  struct io_hdr_stats *shi, struct io_stats *ioi,
			  struct io_stats *ioj, char *devname, struct ext_disk_stats *xds,
			  struct ext_io_stats *xios)
{
	if (!DISPLAY_HUMAN_READ(flags)) {
		cprintf_in(IS_STR, "%-13s", devname, 0);
	}

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		/* tps */
		cprintf_f(NO_UNIT, 1, 8, 2,
			  S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv));
		/* kB/s */
		if (!DISPLAY_UNIT(flags)) {
			xios->sectors /= fctr;
		}
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 1, 9, 2,
			  xios->sectors);
		/* rqm/s */
		cprintf_f(NO_UNIT, 1, 8, 2,
			  S_VALUE(ioj->rd_merges + ioj->wr_merges, ioi->rd_merges + ioi->wr_merges, itv));
		/* await */
		cprintf_f(NO_UNIT, 1, 7, 2,
			  xds->await);
		/* aqu-sz */
		cprintf_f(NO_UNIT, 1, 6, 2,
			  S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
		/* areq-sz (in kB, not sectors) */
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 1, 8, 2,
			  xds->arqsz / 2);
		/*
		 * %util
		 * Again: Ticks in milliseconds.
		 * In the case of a device group (option -g), shi->used is the number of
		 * devices in the group. Else shi->used equals 1.
		 */
		cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2,
			   shi->used ? xds->util / 10.0 / (double) shi->used
				     : xds->util / 10.0);	/* shi->used should never be zero here */
	}
	else {
		/* r/s  w/s */
		cprintf_f(NO_UNIT, 2, 7, 2,
			  S_VALUE(ioj->rd_ios, ioi->rd_ios, itv),
			  S_VALUE(ioj->wr_ios, ioi->wr_ios, itv));
		/* rkB/s  wkB/s */
		if (!DISPLAY_UNIT(flags)) {
			xios->rsectors /= fctr;
			xios->wsectors /= fctr;
		}
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 2, 9, 2,
			  xios->rsectors, xios->wsectors);
		/* rrqm/s  wrqm/s */
		cprintf_f(NO_UNIT, 2, 8, 2,
			  S_VALUE(ioj->rd_merges, ioi->rd_merges, itv),
			  S_VALUE(ioj->wr_merges, ioi->wr_merges, itv));
		/* %rrqm  %wrqm */
		cprintf_pc(DISPLAY_UNIT(flags), 2, 6, 2,
			   xios->rrqm_pc, xios->wrqm_pc);
		/* r_await  w_await */
		cprintf_f(NO_UNIT, 2, 7, 2,
			  xios->r_await, xios->w_await);
		/* aqu-sz */
		cprintf_f(NO_UNIT, 1, 6, 2,
			  S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
		/* rareq-sz  wareq-sz (in kB, not sectors) */
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 2, 8, 2,
			  xios->rarqsz / 2, xios->warqsz / 2);
		/* svctm - The ticks output is biased to output 1000 ticks per second */
		cprintf_f(NO_UNIT, 1, 6, 2, xds->svctm);
		/*
		 * %util
		 * Again: Ticks in milliseconds.
		 * In the case of a device group (option -g), shi->used is the number of
		 * devices in the group. Else shi->used equals 1.
		 */
		cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2,
			   shi->used ? xds->util / 10.0 / (double) shi->used
				     : xds->util / 10.0);	/* shi->used should never be zero here */
	}

	if (DISPLAY_HUMAN_READ(flags)) {
		cprintf_in(IS_STR, " %s", devname, 0);
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys,
 * in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @xds		Extended stats for current device.
 * @xios	Additional extended statistics for current device.
 ***************************************************************************
 */
void write_json_ext_stat(int tab, unsigned long long itv, int fctr,
			 struct io_hdr_stats *shi, struct io_stats *ioi,
			 struct io_stats *ioj, char *devname, struct ext_disk_stats *xds,
			 struct ext_io_stats *xios)
{
	char line[256];

	xprintf0(tab,
		 "{\"disk_device\": \"%s\", ",
		 devname);

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		printf("\"tps\": %.2f, \"",
		       S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv));
		if (DISPLAY_MEGABYTES(flags)) {
			printf("MB/s");
		}
		else if (DISPLAY_KILOBYTES(flags)) {
			printf("kB/s");
		}
		else {
			printf("sec/s");
		}
		printf("\": %.2f, \"rqm/s\": %.2f, \"await\": %.2f, "
		       "\"aqu-sz\": %.2f, \"areq-sz\": %.2f, ",
		       xios->sectors /= fctr,
		       S_VALUE(ioj->rd_merges + ioj->wr_merges, ioi->rd_merges + ioi->wr_merges, itv),
		       xds->await,
		       S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0,
		       xds->arqsz / 2);
	}
	else {
		printf("\"r/s\": %.2f, \"w/s\": %.2f, ",
		       S_VALUE(ioj->rd_ios, ioi->rd_ios, itv),
		       S_VALUE(ioj->wr_ios, ioi->wr_ios, itv));
		if (DISPLAY_MEGABYTES(flags)) {
			sprintf(line, "\"rMB/s\": %%.2f, \"wMB/s\": %%.2f, ");
		}
		else if (DISPLAY_KILOBYTES(flags)) {
			sprintf(line, "\"rkB/s\": %%.2f, \"wkB/s\": %%.2f, ");
		}
		else {
			sprintf(line, "\"rsec/s\": %%.2f, \"wsec/s\": %%.2f, ");
		}
		printf(line,
		       xios->rsectors /= fctr,
		       xios->wsectors /= fctr);
		printf("\"rrqm/s\": %.2f, \"wrqm/s\": %.2f, \"rrqm\": %.2f, \"wrqm\": %.2f, "
		       "\"r_await\": %.2f, \"w_await\": %.2f, "
		       "\"aqu-sz\": %.2f, \"rareq-sz\": %.2f, \"wareq-sz\": %.2f,  \"svctm\": %.2f, ",
		       S_VALUE(ioj->rd_merges, ioi->rd_merges, itv),
		       S_VALUE(ioj->wr_merges, ioi->wr_merges, itv),
		       xios->rrqm_pc,
		       xios->wrqm_pc,
		       xios->r_await,
		       xios->w_await,
		       S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0,
		       xios->rarqsz / 2,
		       xios->warqsz / 2,
		       xds->svctm);
	}
	printf("\"util\": %.2f}",
		 shi->used ? xds->util / 10.0 / (double) shi->used
			   : xds->util / 10.0);	/* shi->used should never be zero here */
}

/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys,
 * in plain or JSON format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @tab		Number of tabs to print (JSON output only).
 ***************************************************************************
 */
void write_ext_stat(unsigned long long itv, int fctr,
		    struct io_hdr_stats *shi, struct io_stats *ioi,
		    struct io_stats *ioj, int tab)
{
	char *devname = NULL;
	struct stats_disk sdc, sdp;
	struct ext_disk_stats xds;
	struct ext_io_stats xios;

	/*
	 * Counters overflows are possible, but don't need to be handled in
	 * a special way: The difference is still properly calculated if the
	 * result is of the same type as the two values.
	 * Exception is field rq_ticks which is incremented by the number of
	 * I/O in progress times the number of milliseconds spent doing I/O.
	 * But the number of I/O in progress (field ios_pgr) happens to be
	 * sometimes negative...
	 */
	sdc.nr_ios    = ioi->rd_ios + ioi->wr_ios;
	sdp.nr_ios    = ioj->rd_ios + ioj->wr_ios;

	sdc.tot_ticks = ioi->tot_ticks;
	sdp.tot_ticks = ioj->tot_ticks;

	sdc.rd_ticks  = ioi->rd_ticks;
	sdp.rd_ticks  = ioj->rd_ticks;
	sdc.wr_ticks  = ioi->wr_ticks;
	sdp.wr_ticks  = ioj->wr_ticks;

	sdc.rd_sect   = ioi->rd_sectors;
	sdp.rd_sect   = ioj->rd_sectors;
	sdc.wr_sect   = ioi->wr_sectors;
	sdp.wr_sect   = ioj->wr_sectors;

	compute_ext_disk_stats(&sdc, &sdp, itv, &xds);

	/* r_await  w_await */
	xios.r_await = (ioi->rd_ios - ioj->rd_ios) ?
		       (ioi->rd_ticks - ioj->rd_ticks) /
		       ((double) (ioi->rd_ios - ioj->rd_ios)) : 0.0;
	xios.w_await = (ioi->wr_ios - ioj->wr_ios) ?
		       (ioi->wr_ticks - ioj->wr_ticks) /
		       ((double) (ioi->wr_ios - ioj->wr_ios)) : 0.0;

	/* rkB/s  wkB/s */
	xios.rsectors = S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv);
	xios.wsectors = S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv);
	xios.sectors  = xios.rsectors + xios.wsectors;

	/* %rrqm  %wrqm */
	xios.rrqm_pc = (ioi->rd_merges - ioj->rd_merges) + (ioi->rd_ios - ioj->rd_ios) ?
		       (double) ((ioi->rd_merges - ioj->rd_merges)) /
		       ((ioi->rd_merges - ioj->rd_merges) + (ioi->rd_ios - ioj->rd_ios)) * 100 :
		       0.0;
	xios.wrqm_pc = (ioi->wr_merges - ioj->wr_merges) + (ioi->wr_ios - ioj->wr_ios) ?
		       (double) ((ioi->wr_merges - ioj->wr_merges)) /
		       ((ioi->wr_merges - ioj->wr_merges) + (ioi->wr_ios - ioj->wr_ios)) * 100 :
		       0.0;

	/* rareq-sz  wareq-sz (still in sectors, not kB) */
	xios.rarqsz = (ioi->rd_ios - ioj->rd_ios) ?
		      (ioi->rd_sectors - ioj->rd_sectors) / ((double) (ioi->rd_ios - ioj->rd_ios)) :
		      0.0;
	xios.warqsz = (ioi->wr_ios - ioj->wr_ios) ?
		      (ioi->wr_sectors - ioj->wr_sectors) / ((double) (ioi->wr_ios - ioj->wr_ios)) :
		      0.0;

	/* Get device name */
	if (DISPLAY_PERSIST_NAME_I(flags)) {
		devname = get_persistent_name_from_pretty(shi->name);
	}
	if (!devname) {
		devname = shi->name;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		write_json_ext_stat(tab, itv, fctr, shi, ioi, ioj, devname, &xds, &xios);
	}
	else {
		write_plain_ext_stat(itv, fctr, shi, ioi, ioj, devname, &xds, &xios);
	}
}

/*
 ***************************************************************************
 * Write basic stats, read from /proc/diskstats or from sysfs, in plain
 * format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @rd_sec	Number of sectors read.
 * @wr_sec	Number of sectors written.
 ***************************************************************************
 */
void write_plain_basic_stat(unsigned long long itv, int fctr,
			    struct io_stats *ioi, struct io_stats *ioj,
			    char *devname, unsigned long long rd_sec,
			    unsigned long long wr_sec)
{
	double rsectors, wsectors;

	if (!DISPLAY_HUMAN_READ(flags)) {
		cprintf_in(IS_STR, "%-13s", devname, 0);
	}
	cprintf_f(NO_UNIT, 1, 8, 2,
		  S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv));
	rsectors = S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv);
	wsectors = S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv);
	if (!DISPLAY_UNIT(flags)) {
		rsectors /= fctr;
		wsectors /= fctr;
	}
	cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 2, 12, 2,
		  rsectors, wsectors);
	cprintf_u64(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 2, 10,
		    DISPLAY_UNIT(flags) ? (unsigned long long) rd_sec
					: (unsigned long long) rd_sec / fctr,
		    DISPLAY_UNIT(flags) ? (unsigned long long) wr_sec
					: (unsigned long long) wr_sec / fctr);
	if (DISPLAY_HUMAN_READ(flags)) {
		cprintf_in(IS_STR, " %s", devname, 0);
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Write basic stats, read from /proc/diskstats or from sysfs, in JSON
 * format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @rd_sec	Number of sectors read.
 * @wr_sec	Number of sectors written.
 ***************************************************************************
 */
void write_json_basic_stat(int tab, unsigned long long itv, int fctr,
			   struct io_stats *ioi, struct io_stats *ioj,
			   char *devname, unsigned long long rd_sec,
			   unsigned long long wr_sec)
{
	char line[256];

	xprintf0(tab,
		 "{\"disk_device\": \"%s\", \"tps\": %.2f, ",
		 devname,
		 S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv));
	if (DISPLAY_KILOBYTES(flags)) {
		sprintf(line, "\"kB_read/s\": %%.2f, \"kB_wrtn/s\": %%.2f, "
			"\"kB_read\": %%llu, \"kB_wrtn\": %%llu}");
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		sprintf(line, "\"MB_read/s\": %%.2f, \"MB_wrtn/s\": %%.2f, "
			"\"MB_read\": %%llu, \"MB_wrtn\": %%llu}");
	}
	else {
		sprintf(line, "\"Blk_read/s\": %%.2f, \"Blk_wrtn/s\": %%.2f, "
			"\"Blk_read\": %%llu, \"Blk_wrtn\": %%llu}");
	}
	printf(line,
	       S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
	       S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
	       (unsigned long long) rd_sec / fctr,
	       (unsigned long long) wr_sec / fctr);
}

/*
 ***************************************************************************
 * Write basic stats, read from /proc/diskstats or from sysfs, in plain or
 * JSON format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @shi		Structures describing the devices and partitions.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @tab		Number of tabs to print (JSON format only).
 ***************************************************************************
 */
void write_basic_stat(unsigned long long itv, int fctr,
		      struct io_hdr_stats *shi, struct io_stats *ioi,
		      struct io_stats *ioj, int tab)
{
	char *devname = NULL;
	unsigned long long rd_sec, wr_sec;

	/* Print device name */
	if (DISPLAY_PERSIST_NAME_I(flags)) {
		devname = get_persistent_name_from_pretty(shi->name);
	}
	if (!devname) {
		devname = shi->name;
	}

	/* Print stats coming from /sys or /proc/diskstats */
	rd_sec = ioi->rd_sectors - ioj->rd_sectors;
	if ((ioi->rd_sectors < ioj->rd_sectors) && (ioj->rd_sectors <= 0xffffffff)) {
		rd_sec &= 0xffffffff;
	}
	wr_sec = ioi->wr_sectors - ioj->wr_sectors;
	if ((ioi->wr_sectors < ioj->wr_sectors) && (ioj->wr_sectors <= 0xffffffff)) {
		wr_sec &= 0xffffffff;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		write_json_basic_stat(tab, itv, fctr, ioi, ioj, devname,
				      rd_sec, wr_sec);
	}
	else {
		write_plain_basic_stat(itv, fctr, ioi, ioj, devname,
				       rd_sec, wr_sec);
	}
}

/*
 ***************************************************************************
 * Print everything now (stats and uptime).
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void write_stats(int curr, struct tm *rectime)
{
	int dev, i, fctr = 1, tab = 4, next = FALSE;
	unsigned long long itv;
	struct io_hdr_stats *shi;
	struct io_dlist *st_dev_list_i;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf(tab++, "{");
	}

	/* Print time stamp */
	if (DISPLAY_TIMESTAMP(flags)) {
		write_sample_timestamp(tab, rectime);
#ifdef DEBUG
		if (DISPLAY_DEBUG(flags)) {
			fprintf(stderr, "%s\n", timestamp);
		}
#endif
	}

	if (DISPLAY_CPU(flags)) {
		/* Display CPU utilization */
		write_cpu_stat(curr, tab);

		if (DISPLAY_JSON_OUTPUT(flags)) {
			if (DISPLAY_DISK(flags)) {
				printf(",");
			}
			printf("\n");
		}
	}

	/* Calculate time interval in 1/100th of a second */
	itv = get_interval(uptime_cs[!curr], uptime_cs[curr]);

	if (DISPLAY_DISK(flags)) {
		struct io_stats *ioi, *ioj;

		shi = st_hdr_iodev;

		/* Display disk stats header */
		write_disk_stat_header(&fctr, &tab);

		for (i = 0; i < iodev_nr; i++, shi++) {
			if (shi->used) {

				if (dlist_idx && !HAS_SYSFS(flags)) {
					/*
					 * With /proc/diskstats, stats for every device
					 * are read even if we have entered a list on devices
					 * on the command line. Thus we need to check
					 * if stats for current device are to be displayed.
					 */
					for (dev = 0; dev < dlist_idx; dev++) {
						st_dev_list_i = st_dev_list + dev;
						if (!strcmp(shi->name, st_dev_list_i->dev_name))
							break;
					}
					if (dev == dlist_idx)
						/* Device not found in list: Don't display it */
						continue;
				}

				ioi = st_iodev[curr] + i;
				ioj = st_iodev[!curr] + i;

				if (!DISPLAY_UNFILTERED(flags)) {
					if (!ioi->rd_ios && !ioi->wr_ios)
						continue;
				}

				if (DISPLAY_ZERO_OMIT(flags)) {
					if ((ioi->rd_ios == ioj->rd_ios) &&
						(ioi->wr_ios == ioj->wr_ios))
						/* No activity: Ignore it */
						continue;
				}

				if (DISPLAY_GROUP_TOTAL_ONLY(flags)) {
					if (shi->status != DISK_GROUP)
						continue;
				}
#ifdef DEBUG
				if (DISPLAY_DEBUG(flags)) {
					/* Debug output */
					fprintf(stderr, "name=%s itv=%llu fctr=%d ioi{ rd_sectors=%lu "
							"wr_sectors=%lu rd_ios=%lu rd_merges=%lu rd_ticks=%u "
							"wr_ios=%lu wr_merges=%lu wr_ticks=%u ios_pgr=%u tot_ticks=%u "
							"rq_ticks=%u }\n",
						shi->name,
						itv,
						fctr,
						ioi->rd_sectors,
						ioi->wr_sectors,
						ioi->rd_ios,
						ioi->rd_merges,
						ioi->rd_ticks,
						ioi->wr_ios,
						ioi->wr_merges,
						ioi->wr_ticks,
						ioi->ios_pgr,
						ioi->tot_ticks,
						ioi->rq_ticks
						);
				}
#endif

				if (DISPLAY_JSON_OUTPUT(flags) && next) {
					printf(",\n");
				}
				next = TRUE;

				if (DISPLAY_EXTENDED(flags)) {
					write_ext_stat(itv, fctr, shi, ioi, ioj, tab);
				}
				else {
					write_basic_stat(itv, fctr, shi, ioi, ioj, tab);
				}
			}
		}
		if (DISPLAY_JSON_OUTPUT(flags)) {
			printf("\n");
			xprintf(--tab, "]");
		}
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf0(--tab, "}");
	}
	else {
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Main loop: Read I/O stats from the relevant sources and display them.
 *
 * IN:
 * @count	Number of reports to print.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void rw_io_stat_loop(long int count, struct tm *rectime)
{
	int curr = 1;
	int skip = 0;

	/* Should we skip first report? */
	if (DISPLAY_OMIT_SINCE_BOOT(flags) && interval > 0) {
		skip = 1;
	}

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	do {
		/* Read system uptime (only for SMP machines) */
		read_uptime(&(uptime_cs[curr]));

		/* Read stats for CPU "all" */
		read_stat_cpu(st_cpu[curr], 1);

		if (dlist_idx) {
			/*
			 * A device or partition name was explicitly entered
			 * on the command line, with or without -p option
			 * (but not -p ALL).
			 */
			if (HAS_DISKSTATS(flags) && !DISPLAY_PARTITIONS(flags)) {
				read_diskstats_stat(curr);
			}
			else if (HAS_SYSFS(flags)) {
				read_sysfs_dlist_stat(curr);
			}
		}
		else {
			/*
			 * No devices nor partitions entered on the command line
			 * (for example if -p ALL was used).
			 */
			if (HAS_DISKSTATS(flags)) {
				read_diskstats_stat(curr);
			}
			else if (HAS_SYSFS(flags)) {
				read_sysfs_stat(curr);
			}
		}

		/* Compute device groups stats */
		if (group_nr > 0) {
			compute_device_groups_stats(curr);
		}

		/* Get time */
		get_localtime(rectime, 0);

		/* Check whether we should skip first report */
		if (!skip) {
			/* Print results */
			write_stats(curr, rectime);

			if (count > 0) {
				count--;
			}
			if (DISPLAY_JSON_OUTPUT(flags)) {
				if (count) {
				printf(",");
				}
				printf("\n");
			}
		}
		else {
			skip = 0;
		}

		if (count) {
			curr ^= 1;
			pause();
		}
	}
	while (count);

	if (DISPLAY_JSON_OUTPUT(flags)) {
		printf("\t\t\t]\n\t\t}\n\t]\n}}\n");
	}
}

/*
 ***************************************************************************
 * Main entry to the iostat program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int it = 0;
	int opt = 1;
	int i, report_set = FALSE;
	long count = 1;
	struct utsname header;
	struct io_dlist *st_dev_list_i;
	struct tm rectime;
	char *t, *persist_devname, *devname;

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	/* Init color strings */
	init_colors();

	/* Allocate structures for device list */
	if (argc > 1) {
		salloc_dev_list(argc - 1 + count_csvalues(argc, argv));
	}

	/* Process args... */
	while (opt < argc) {

		/* -p option used individually. See below for grouped use */
		if (!strcmp(argv[opt], "-p")) {
			flags |= I_D_PARTITIONS;
			if (argv[++opt] &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt])) &&
			    (strncmp(argv[opt], "-", 1))) {
				flags |= I_D_UNFILTERED;

				for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
					if (!strcmp(t, K_ALL)) {
						flags |= I_D_PART_ALL;
					}
					else {
						devname = device_name(t);
						if (DISPLAY_PERSIST_NAME_I(flags)) {
							/* Get device persistent name */
							persist_devname = get_pretty_name_from_persistent(devname);
							if (persist_devname != NULL) {
								devname = persist_devname;
							}
						}
						/* Store device name */
						i = update_dev_list(&dlist_idx, devname);
						st_dev_list_i = st_dev_list + i;
						st_dev_list_i->disp_part = TRUE;
					}
				}
				opt++;
			}
			else {
				flags |= I_D_PART_ALL;
			}
		}

		else if (!strcmp(argv[opt], "-g")) {
			/*
			 * Option -g: Stats for a group of devices.
			 * group_name contains the last group name entered on
			 * the command line. If we define an additional one, save
			 * the previous one in the list. We do that this way because we
			 * want the group name to appear in the list _after_ all
			 * the devices included in that group. The last group name
			 * will be saved in the list later, in presave_device_list() function.
			 */
			if (group_nr > 0) {
				update_dev_list(&dlist_idx, group_name);
			}
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			/*
			 * MAX_NAME_LEN - 2: one char for the heading space,
			 * and one for the trailing '\0'.
			 */
			snprintf(group_name, MAX_NAME_LEN, " %-.*s", MAX_NAME_LEN - 2, argv[opt++]);
			group_nr++;
		}

		else if (!strcmp(argv[opt], "--human")) {
			flags |= I_D_UNIT;
			opt++;
		}

		else if (!strncmp(argv[opt], "--dec=", 6) && (strlen(argv[opt]) == 7)) {
			/* Get number of decimal places */
			dplaces_nr = atoi(argv[opt] + 6);
			if ((dplaces_nr < 0) || (dplaces_nr > 2)) {
				usage(argv[0]);
			}
			opt++;
		}

		else if (!strcmp(argv[opt], "-j")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			if (strnlen(argv[opt], MAX_FILE_LEN) >= MAX_FILE_LEN - 1) {
				usage(argv[0]);
			}
			strncpy(persistent_name_type, argv[opt], MAX_FILE_LEN - 1);
			persistent_name_type[MAX_FILE_LEN - 1] = '\0';
			strtolower(persistent_name_type);
			/* Check that this is a valid type of persistent device name */
			if (!get_persistent_type_dir(persistent_name_type)) {
				fprintf(stderr, _("Invalid type of persistent device name\n"));
				exit(1);
			}
			/*
			 * Persistent names are usually long: Display
			 * them as human readable by default.
			 */
			flags |= I_D_PERSIST_NAME + I_D_HUMAN_READ;
			opt++;
		}

		else if (!strcmp(argv[opt], "-o")) {
			/* Select output format */
			if (argv[++opt] && !strcmp(argv[opt], K_JSON)) {
				flags |= I_D_JSON_OUTPUT;
				opt++;
			}
			else {
				usage(argv[0]);
			}
		}

#ifdef DEBUG
		else if (!strcmp(argv[opt], "--debuginfo")) {
			flags |= I_D_DEBUG;
			opt++;
		}
#endif

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'c':
					/* Display cpu usage */
					flags |= I_D_CPU;
					report_set = TRUE;
					break;

				case 'd':
					/* Display disk utilization */
					flags |= I_D_DISK;
					report_set = TRUE;
					break;

                                case 'H':
					/* Display stats only for the groups */
					flags |= I_D_GROUP_TOTAL_ONLY;
					break;

				case 'h':
					/*
					 * Display device utilization report
					 * in a human readable format. Also imply --human.
					 */
					flags |= I_D_HUMAN_READ + I_D_UNIT;
					break;

				case 'k':
					if (DISPLAY_MEGABYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in kB/s */
					flags |= I_D_KILOBYTES;
					break;

				case 'm':
					if (DISPLAY_KILOBYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in MB/s */
					flags |= I_D_MEGABYTES;
					break;

				case 'N':
					/* Display device mapper logical name */
					flags |= I_D_DEVMAP_NAME;
					break;

				case 'p':
					/* If option -p is grouped then it cannot take an arg */
					flags |= I_D_PARTITIONS + I_D_PART_ALL;
					break;

				case 's':
					/* Display short output */
					flags |= I_D_SHORT_OUTPUT;
					break;

				case 't':
					/* Display timestamp */
					flags |= I_D_TIMESTAMP;
					break;

				case 'x':
					/* Display extended stats */
					flags |= I_D_EXTENDED;
					break;

				case 'y':
					/* Don't display stats since system restart */
					flags |= I_D_OMIT_SINCE_BOOT;
					break;

				case 'z':
					/* Omit output for devices with no activity */
					flags |= I_D_ZERO_OMIT;
					break;

				case 'V':
					/* Print version number and exit */
					print_version();
					break;

				default:
					usage(argv[0]);
				}
			}
			opt++;
		}

		else if (!isdigit(argv[opt][0])) {
			/*
			 * By default iostat doesn't display unused devices.
			 * If some devices are explicitly entered on the command line
			 * then don't apply this rule any more.
			 */
			flags |= I_D_UNFILTERED;

			if (strcmp(argv[opt], K_ALL)) {
				/* Store device name entered on the command line */
				devname = device_name(argv[opt++]);
				if (DISPLAY_PERSIST_NAME_I(flags)) {
					persist_devname = get_pretty_name_from_persistent(devname);
					if (persist_devname != NULL) {
						devname = persist_devname;
					}
				}
				update_dev_list(&dlist_idx, devname);
			}
			else {
				opt++;
			}
		}

		else if (!it) {
			interval = atol(argv[opt++]);
			if (interval < 0) {
				usage(argv[0]);
			}
			count = -1;
			it = 1;
		}

		else if (it > 0) {
			count = atol(argv[opt++]);
			if ((count < 1) || !interval) {
				usage(argv[0]);
			}
			it = -1;
		}
		else {
			usage(argv[0]);
		}
	}

	if (!interval) {
		count = 1;
	}

	/* Default: Display CPU and DISK reports */
	if (!report_set) {
		flags |= I_D_CPU + I_D_DISK;
	}
	/*
	 * Also display DISK reports if options -p, -x or a device has been entered
	 * on the command line.
	 */
	if (DISPLAY_PARTITIONS(flags) || DISPLAY_EXTENDED(flags) ||
	    DISPLAY_UNFILTERED(flags)) {
		flags |= I_D_DISK;
	}

	/* Option -T can only be used with option -g */
	if (DISPLAY_GROUP_TOTAL_ONLY(flags) && !group_nr) {
		usage(argv[0]);
	}

	/* Select disk output unit (kB/s or blocks/s) */
	set_disk_output_unit();

	/* Ignore device list if '-p ALL' entered on the command line */
	if (DISPLAY_PART_ALL(flags)) {
		dlist_idx = 0;
	}

	if (DISPLAY_DEVMAP_NAME(flags)) {
		dm_major = get_devmap_major();
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		/* Use a decimal point to make JSON code compliant with RFC7159 */
		setlocale(LC_NUMERIC, "C");
	}

	/* Init structures according to machine architecture */
	io_sys_init();
	if (group_nr > 0) {
		/*
		 * If groups of devices have been defined
		 * then save devices and groups in the list.
		 */
		presave_device_list();
	}

	get_localtime(&rectime, 0);

	/* Get system name, release number and hostname */
	uname(&header);
	if (print_gal_header(&rectime, header.sysname, header.release,
			     header.nodename, header.machine, cpu_nr,
			     DISPLAY_JSON_OUTPUT(flags))) {
		flags |= I_D_ISO;
	}
	if (!DISPLAY_JSON_OUTPUT(flags)) {
		printf("\n");
	}

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Main loop */
	rw_io_stat_loop(count, &rectime);

	/* Free structures */
	io_sys_free();
	sfree_dev_list();

	return 0;
}
