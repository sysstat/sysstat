/*
 * iostat: report CPU and I/O statistics
 * (C) 1998-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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

#ifdef TEST
extern int __env;
#endif

struct stats_cpu *st_cpu[2];
unsigned long long uptime_cs[2] = {0, 0};
unsigned long long tot_jiffies[2] = {0, 0};
struct io_device *dev_list = NULL;

/* Number of decimal places */
int dplaces_nr = -1;

int group_nr = 0;	/* Nb of device groups */
int cpu_nr = 0;		/* Nb of processors on the machine */
int flags = 0;		/* Flag for common options and system state */

long interval = 0;
char timestamp[TIMESTAMP_LEN];
char alt_dir[MAX_FILE_LEN];

struct sigaction alrm_act, int_act;
int sigint_caught = 0;

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
			  "[ { -f | +f } <directory> ] [ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ --pretty ] [ -o JSON ]\n"
			  "[ [ -H ] -g <group_name> ] [ -p [ <device> [,...] | ALL ] ]\n"
			  "[ <device> [...] | ALL ] [ --debuginfo ]\n"));
#else
	fprintf(stderr, _("Options are:\n"
			  "[ -c ] [ -d ] [ -h ] [ -k | -m ] [ -N ] [ -s ] [ -t ] [ -V ] [ -x ] [ -y ] [ -z ]\n"
			  "[ { -f | +f } <directory> ] [ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ --pretty ] [ -o JSON ]\n"
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
	if (__getenv(ENV_POSIXLY_CORRECT) == NULL) {
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
 * SIGINT signal handler.
 *
 * IN:
 * @sig	Signal number.
 **************************************************************************
 */
void int_handler(int sig)
{
	sigint_caught = 1;
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
 * Set every device entry to nonexistent status.
 *
 * IN:
 * @dlist	Pointer on the start of the linked list.
 ***************************************************************************
 */
void set_devices_nonexistent(struct io_device *dlist)
{
	while (dlist != NULL) {
		dlist->exist = FALSE;
		dlist = dlist->next;
	}
}

/*
 ***************************************************************************
 * Check if a device is present in the list, and add it if requested.
 * Also look for its type (device or partition) and save it.
 *
 * IN:
 * @dlist	Address of pointer on the start of the linked list.
 * @name	Device name.
 * @dtype	T_PART_DEV (=2) if the device and all its partitions should
 *		also be read (option -p used), T_GROUP (=3) if it's a group
 *		name, and 0 otherwise.
 *
 * RETURNS:
 * Pointer on the io_device structure in the list where the device is located
 * (whether it was already in the list or if it has been added).
 * NULL if the device name is too long or if the device doesn't exist and we
 * don't want to add it.
 ***************************************************************************
 */
struct io_device *add_list_device(struct io_device **dlist, char *name, int dtype)
{
	struct io_device *d, *ds;
	int i, rc = 0;

	if (strnlen(name, MAX_NAME_LEN) == MAX_NAME_LEN)
		/* Device name is too long */
		return NULL;

	while (*dlist != NULL) {
		d = *dlist;
		if ((i = strcmp(d->name, name)) == 0) {
			/* Device found in list */
			if ((dtype == T_PART_DEV) && (d->dev_tp == T_DEV)) {
				d->dev_tp = dtype;
			}
			d->exist = TRUE;
			return d;
		}
		if (!GROUP_DEFINED(flags) && !DISPLAY_EVERYTHING(flags) && (i > 0))
			/*
			 * If no group defined and we don't use /proc/diskstats,
			 * insert current device in alphabetical order.
			 * NB: Using /proc/diskstats ("iostat -p ALL") is a bit better than
			 * using alphabetical order because sda10 comes after sda9...
			 */
			break;

		dlist = &(d->next);
	}

	/* Device not found */
	ds = *dlist;

	/* Add device to the list */
	if ((*dlist = (struct io_device *) malloc(sizeof(struct io_device))) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(*dlist, 0, sizeof(struct io_device));

	d = *dlist;
	for (i = 0; i < 2; i++) {
		if ((d->dev_stats[i] = (struct io_stats *) malloc(sizeof(struct io_stats))) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(d->dev_stats[i], 0, sizeof(struct io_stats));
	}
	strncpy(d->name, name, MAX_NAME_LEN);
	d->name[MAX_NAME_LEN - 1] = '\0';
	d->exist = TRUE;
	d->next = ds;

	if (dtype == T_GROUP) {
		d->dev_tp = dtype;
	}
	else  {
		if (!alt_dir[0] || USE_ALL_DIR(flags)) {
			rc = is_device(SLASH_SYS, name, ACCEPT_VIRTUAL_DEVICES);
		}

		if (alt_dir[0] && (!USE_ALL_DIR(flags) || (USE_ALL_DIR(flags) && !rc))) {
			rc = is_device(alt_dir, name, ACCEPT_VIRTUAL_DEVICES);
		}

		if (rc) {
			d->dev_tp = (dtype == T_PART_DEV ? T_PART_DEV : T_DEV);
		}
		else {
			/* This is a partition (T_PART) */
			d->dev_tp = T_PART;
		}
	}

	return d;
}

/*
 ***************************************************************************
 * Get device major and minor numbers.
 *
 * IN:
 * @filename	Name of the device ("sda", "/dev/sdb1"...)
 *
 * OUT:
 * @major	Major number of the device.
 * @minor	Minor number of the device.
 *
 * RETURNS:
 * 0 on success, and -1 otherwise.
 ***************************************************************************
 */
int get_major_minor_nr(char filename[], int *major, int *minor)
{
	struct stat statbuf;
	char *bang;
	char dfile[MAX_PF_NAME];

	snprintf(dfile, sizeof(dfile), "%s%s", filename[0] == '/' ? "" : SLASH_DEV, filename);
	dfile[sizeof(dfile) - 1] = '\0';

	while ((bang = strchr(dfile, '!'))) {
		/*
		 * Some devices may have had a slash replaced with a bang character (eg. cciss!c0d0...)
		 * Restore their original names so that they can be found in /dev directory.
		 */
		*bang = '/';
	}

	if (__stat(dfile, &statbuf) < 0)
		return -1;

	*major = __major(statbuf.st_rdev);
	*minor = __minor(statbuf.st_rdev);

	return 0;
}

/*
 ***************************************************************************
 * Read sysfs stat for current block device or partition.
 *
 * IN:
 * @filename	File name where stats will be read.
 * @ios		Structure where stats will be saved.
 *
 * OUT:
 * @ios		Structure where stats have been saved.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_file_stat_work(char *filename, struct io_stats *ios)
{
	FILE *fp;
	struct io_stats sdev;
	int i;
	unsigned int ios_pgr, tot_ticks, rq_ticks, wr_ticks, dc_ticks, fl_ticks;
	unsigned long rd_ios, rd_merges_or_rd_sec, wr_ios, wr_merges;
	unsigned long rd_sec_or_wr_ios, wr_sec, rd_ticks_or_wr_sec;
	unsigned long dc_ios, dc_merges, dc_sec, fl_ios;

	/* Try to read given stat file */
	if ((fp = fopen(filename, "r")) == NULL)
		return -1;

	i = fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu %u %u %u %u %lu %lu %lu %u %lu %u",
		   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
		   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks,
		   &dc_ios, &dc_merges, &dc_sec, &dc_ticks,
		   &fl_ios, &fl_ticks);

	memset(&sdev, 0, sizeof(struct io_stats));

	if (i >= 11) {
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

		if (i >= 15) {
			/* Discard I/O */
			sdev.dc_ios     = dc_ios;
			sdev.dc_merges  = dc_merges;
			sdev.dc_sectors = dc_sec;
			sdev.dc_ticks   = dc_ticks;
		}

		if (i >= 17) {
			/* Flush I/O */
			sdev.fl_ios     = fl_ios;
			sdev.fl_ticks   = fl_ticks;
		}
	}
	else if (i == 4) {
		/* Partition without extended statistics */
		sdev.rd_ios     = rd_ios;
		sdev.rd_sectors = rd_merges_or_rd_sec;
		sdev.wr_ios     = rd_sec_or_wr_ios;
		sdev.wr_sectors = rd_ticks_or_wr_sec;
	}

	*ios = sdev;

	fclose(fp);

	return 0;
}

/*
 ***************************************************************************
 * Read sysfs stat for current whole device using /sys or an alternate
 * location.
 *
 * IN:
 * @devname	Device name for which stats have to be read.
 * @ios		Structure where stats will be saved.
 *
 * OUT:
 * @ios		Structure where stats have been saved.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_file_stat(char *devname, struct io_stats *ios)
{
	int rc = 0;
	char dfile[MAX_PF_NAME];

	if (!alt_dir[0] || USE_ALL_DIR(flags)) {
		/* Read stats for current whole device using /sys/block/ directory */
		snprintf(dfile, sizeof(dfile), "%s/%s/%s/%s",
			 SLASH_SYS, __BLOCK, devname, S_STAT);
		dfile[sizeof(dfile) - 1] = '\0';

		rc = read_sysfs_file_stat_work(dfile, ios);
	}

	if (alt_dir[0] && (!USE_ALL_DIR(flags) || (USE_ALL_DIR(flags) && (rc < 0)))) {
		/* Read stats for current whole device using an alternate /sys directory */
		snprintf(dfile, sizeof(dfile), "%s/%s/%s/%s",
			 alt_dir, __BLOCK, devname, S_STAT);
		dfile[sizeof(dfile) - 1] = '\0';

		rc = read_sysfs_file_stat_work(dfile, ios);
	}

	return rc;
}

/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a whole device. Devices are
 * saved in the linked list.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dname	Whole device name.
 * @sysdev	sysfs location.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_device_part_stat_work(int curr, char *dname, char *sysdev)
{
	DIR *dir;
	struct dirent *drd;
	struct io_stats sdev;
	struct io_device *d;
	char dfile[MAX_PF_NAME], filename[MAX_PF_NAME + 512];
	int major, minor;

	snprintf(dfile, sizeof(dfile), "%s/%s/%s", sysdev, __BLOCK, dname);
	dfile[sizeof(dfile) - 1] = '\0';

	/* Open current device directory in /sys/block */
	if ((dir = __opendir(dfile)) == NULL)
		return -1;

	/* Get current entry */
	while ((drd = __readdir(dir)) != NULL) {

		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(filename, sizeof(filename), "%s/%s/%s", dfile, drd->d_name, S_STAT);
		filename[sizeof(filename) - 1] = '\0';

		/* Read current partition stats */
		if (read_sysfs_file_stat_work(filename, &sdev) < 0)
			continue;

		d = add_list_device(&dev_list, drd->d_name, 0);
		if (d != NULL) {
			*(d->dev_stats[curr]) = sdev;

			if (!d->major) {
				/* Get major and minor numbers for given device */
				if (get_major_minor_nr(d->name, &major, &minor) == 0) {
					d->major = major;
					d->minor = minor;
				}
			}
		}
	}

	/* Close device directory */
	__closedir(dir);

	return 0;
}

/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a whole device.
 * Stats are from /sys or an alternate directory.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dname	Whole device name.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_device_part_stat(int curr, char *dname)
{
	int rc = 0;

	if (!alt_dir[0] || USE_ALL_DIR(flags)) {
		/* Read partition stats from /sys */
		rc = read_sysfs_device_part_stat_work(curr, dname, SLASH_SYS);
	}

	if (alt_dir[0] && (!USE_ALL_DIR(flags) || (USE_ALL_DIR(flags) && (rc < 0)))) {
		/* Read partition stats from an alternate /sys directory */
		rc = read_sysfs_device_part_stat_work(curr, dname, alt_dir);
	}

	return rc;
}

/*
 ***************************************************************************
 * Read sysfs stats for every whole device. Devices are	saved in the linked
 * list.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @sysblock	__sys/block directory location.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_all_devices_stat_work(int curr, char *sysblock)
{
	DIR *dir;
	struct dirent *drd;
	struct io_stats sdev;
	struct io_device *d;
	char dfile[MAX_PF_NAME];
	int major, minor;

	/* Open __sys/block directory */
	if ((dir = __opendir(sysblock)) == NULL)
		return -1;

	/* Get current entry */
	while ((drd = __readdir(dir)) != NULL) {

		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(dfile, sizeof(dfile), "%s/%s/%s", sysblock, drd->d_name, S_STAT);
		dfile[sizeof(dfile) - 1] = '\0';

		/* Read current whole device stats */
		if (read_sysfs_file_stat_work(dfile, &sdev) < 0)
			continue;

		d = add_list_device(&dev_list, drd->d_name, 0);
		if (d != NULL) {
			*(d->dev_stats[curr]) = sdev;

			if (!d->major) {
				/* Get major and minor numbers for given device */
				if (get_major_minor_nr(d->name, &major, &minor) == 0) {
					d->major = major;
					d->minor = minor;
				}
			}
		}
	}

	/* Close device directory */
	__closedir(dir);

	return 0;
}

/*
 ***************************************************************************
 * Read sysfs stats for every whole device from /sys or an alternate
 * location.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_all_devices_stat(int curr)
{
	int rc = 0;
	char sysblock[MAX_PF_NAME];

	if (!alt_dir[0] || USE_ALL_DIR(flags)) {
		/* Read all whole devices from /sys */
		rc = read_sysfs_all_devices_stat_work(curr, SYSFS_BLOCK);
	}

	if (alt_dir[0]) {
		snprintf(sysblock, sizeof(sysblock), "%s/%s", alt_dir, __BLOCK);
		sysblock[sizeof(sysblock) - 1] = '\0';
		/* Read stats from an alternate sys location */
		rc = read_sysfs_all_devices_stat_work(curr, sysblock);
	}

	return rc;
}

/*
 ***************************************************************************
 * Read sysfs stats for a partition using __sys/dev/block/M:m/ directory.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @d		Device structure.
 * @sysdev	sysfs directory.
 *
 * RETURNS:
 * 0 on success, and -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_part_stat_work(int curr, struct io_device *d, char *sysdev)
{
	char dfile[MAX_PF_NAME];
	int major, minor;

	if (!d->major) {
		/* Get major and minor numbers for given device */
		if (get_major_minor_nr(d->name, &major, &minor) < 0)
			return -1;
		d->major = major;
		d->minor = minor;
	}

	/* Read stats for device */
	snprintf(dfile, sizeof(dfile), "%s/%s/%d:%d/%s",
		 sysdev, __DEV_BLOCK, d->major, d->minor, S_STAT);
	dfile[sizeof(dfile) - 1] = '\0';

	return read_sysfs_file_stat_work(dfile, d->dev_stats[curr]);
}

/*
 ***************************************************************************
 * Read sysfs stats for a partition using /sys/dev/block/M:m/ directory or
 * an alternate directory.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @d		Device structure.
 *
 * RETURNS:
 * 0 on success, and -1 otherwise.
 ***************************************************************************
 */
int read_sysfs_part_stat(int curr, struct io_device *d)
{
	int rc = 0;

	if (!alt_dir[0] || USE_ALL_DIR(flags)) {
		/* Read partition stats from /sys */
		rc = read_sysfs_part_stat_work(curr, d, SLASH_SYS);
	}

	if (alt_dir[0] && (!USE_ALL_DIR(flags) || (USE_ALL_DIR(flags) && (rc < 0)))) {
		/* Read partition stats from an alternate /sys directory */
		rc = read_sysfs_part_stat_work(curr, d, alt_dir);
	}

	return rc;
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
	struct io_device *dlist;

	for (dlist = dev_list; dlist != NULL; dlist = dlist->next) {
		if (dlist->exist)
			/* Device stats already read */
			continue;

		else if (dlist->dev_tp == T_PART) {
			/*
			 * This is a partition.
			 * Read its stats using /sys/dev/block/M:n/ directory.
			 */
			if (read_sysfs_part_stat(curr, dlist) == 0) {
				dlist->exist = TRUE;
			}
		}

		else if ((dlist->dev_tp == T_PART_DEV) || (dlist->dev_tp == T_DEV)) {
			/* Read stats for current whole device using /sys/block/ directory */
			if (read_sysfs_file_stat(dlist->name, dlist->dev_stats[curr]) == 0) {
				dlist->exist = TRUE;
			}

			if (dlist->dev_tp == T_PART_DEV) {
				/* Also read all its partitions now */
				read_sysfs_device_part_stat(curr, dlist->name);
			}
		}
	}

	/* Read all whole devices stats if requested ("iostat ALL ...") */
	if (DISPLAY_ALL_DEVICES(flags)) {
		read_sysfs_all_devices_stat(curr);
	}
}

/*
 ***************************************************************************
 * Read stats from the diskstats file. Only used when "-p ALL" has been
 * entered on the command line.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @diskstats	Path to diskstats file (e.g. "/proc/diskstats").
 ***************************************************************************
 */
void read_diskstats_stat_work(int curr, char *diskstats)
{
	FILE *fp;
	char line[256], dev_name[MAX_NAME_LEN];
	struct io_device *d;
	struct io_stats sdev;
	int i;
	unsigned int ios_pgr, tot_ticks, rq_ticks, wr_ticks, dc_ticks, fl_ticks;
	unsigned long rd_ios, rd_merges_or_rd_sec, rd_ticks_or_wr_sec, wr_ios;
	unsigned long wr_merges, rd_sec_or_wr_ios, wr_sec;
	unsigned long dc_ios, dc_merges, dc_sec, fl_ios;
	unsigned int major, minor;

	if ((fp = fopen(diskstats, "r")) == NULL)
		return;

	while (fgets(line, sizeof(line), fp) != NULL) {

		memset(&sdev, 0, sizeof(struct io_stats));

		/* major minor name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq dcio dcmerge dcsect dcuse flio fltm */
		i = sscanf(line, "%u %u %s %lu %lu %lu %lu %lu %lu %lu %u %u %u %u %lu %lu %lu %u %lu %u",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
			   &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks,
			   &dc_ios, &dc_merges, &dc_sec, &dc_ticks,
			   &fl_ios, &fl_ticks);

		if (i >= 14) {
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

			if (i >= 18) {
				/* Discard I/O */
				sdev.dc_ios     = dc_ios;
				sdev.dc_merges  = dc_merges;
				sdev.dc_sectors = dc_sec;
				sdev.dc_ticks   = dc_ticks;
			}

			if (i >= 20) {
				/* Flush I/O */
				sdev.fl_ios     = fl_ios;
				sdev.fl_ticks   = fl_ticks;
			}
		}
		else if (i == 7) {
			/* Partition without extended statistics */
			if (DISPLAY_EXTENDED(flags))
				continue;

			sdev.rd_ios     = rd_ios;
			sdev.rd_sectors = rd_merges_or_rd_sec;
			sdev.wr_ios     = rd_sec_or_wr_ios;
			sdev.wr_sectors = rd_ticks_or_wr_sec;
		}
		else
			/* Unknown entry: Ignore it */
			continue;

		d = add_list_device(&dev_list, dev_name, 0);
		if (d != NULL) {
			*d->dev_stats[curr] = sdev;
			d->major = major;
			d->minor = minor;
		}
	}
	fclose(fp);
}

/*
 ***************************************************************************
 * Read stats from /proc/diskstats or an alternate diskstats file.
 * Only used when "-p ALL" has been entered on the command line.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_diskstats_stat(int curr)
{
	char diskstats[MAX_PF_NAME];

	if (!alt_dir[0] || USE_ALL_DIR(flags)) {
		/* Read stats from /proc/diskstats */
		read_diskstats_stat_work(curr, DISKSTATS);
	}

	if (alt_dir[0]) {
		snprintf(diskstats, sizeof(diskstats), "%s/%s", alt_dir, __DISKSTATS);
		diskstats[sizeof(diskstats) - 1] = '\0';
		/* Read stats from an alternate diskstats file */
		read_diskstats_stat_work(curr, diskstats);
	}
}

/*
 ***************************************************************************
 * Add current device statistics to corresponding group.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @iodev_nr		Number of devices and partitions.
 ***************************************************************************
 */
void compute_device_groups_stats(int curr, struct io_device *d, struct io_device *g)
{
	if (!DISPLAY_UNFILTERED(flags)) {
		if (!d->dev_stats[curr]->rd_ios &&
		    !d->dev_stats[curr]->wr_ios &&
		    !d->dev_stats[curr]->dc_ios &&
		    !d->dev_stats[curr]->fl_ios)
			return;
	}

	g->dev_stats[curr]->rd_ios     += d->dev_stats[curr]->rd_ios;
	g->dev_stats[curr]->rd_merges  += d->dev_stats[curr]->rd_merges;
	g->dev_stats[curr]->rd_sectors += d->dev_stats[curr]->rd_sectors;
	g->dev_stats[curr]->rd_ticks   += d->dev_stats[curr]->rd_ticks;
	g->dev_stats[curr]->wr_ios     += d->dev_stats[curr]->wr_ios;
	g->dev_stats[curr]->wr_merges  += d->dev_stats[curr]->wr_merges;
	g->dev_stats[curr]->wr_sectors += d->dev_stats[curr]->wr_sectors;
	g->dev_stats[curr]->wr_ticks   += d->dev_stats[curr]->wr_ticks;
	g->dev_stats[curr]->dc_ios     += d->dev_stats[curr]->dc_ios;
	g->dev_stats[curr]->dc_merges  += d->dev_stats[curr]->dc_merges;
	g->dev_stats[curr]->dc_sectors += d->dev_stats[curr]->dc_sectors;
	g->dev_stats[curr]->dc_ticks   += d->dev_stats[curr]->dc_ticks;
	g->dev_stats[curr]->fl_ios     += d->dev_stats[curr]->fl_ios;
	g->dev_stats[curr]->fl_ticks   += d->dev_stats[curr]->fl_ticks;
	g->dev_stats[curr]->ios_pgr    += d->dev_stats[curr]->ios_pgr;
	g->dev_stats[curr]->tot_ticks  += d->dev_stats[curr]->tot_ticks;
	g->dev_stats[curr]->rq_ticks   += d->dev_stats[curr]->rq_ticks;
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
 * @hpart	Indicate which part of the report should be displayed in
 *		human mode. A value of 0 indicates that output should not be
 *		broken in several parts.
 ***************************************************************************
 */
void write_disk_stat_header(int *fctr, int *tab, int hpart)
{
	char *units, *spc;

	if (DISPLAY_KILOBYTES(flags)) {
		*fctr = 2;
		units = "kB";
		spc = " ";
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		*fctr = 2048;
		units = "MB";
		spc = " ";
	}
	else if (DISPLAY_EXTENDED(flags)) {
		units = "sec";
		spc = "";
	}
	else {
		units = "Blk";
		spc = "";
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf((*tab)++, "\"disk\": [");
		return;
	}

	if (!DISPLAY_PRETTY(flags)) {
		printf("Device       ");
	}
	if (DISPLAY_EXTENDED(flags)) {
		/* Extended stats */
		if (DISPLAY_SHORT_OUTPUT(flags)) {
			printf("      tps     %s%s/s    rqm/s   await  areq-sz  aqu-sz  %%util",
			       spc, units);
		}
		else {
			if ((hpart == 1) || !hpart) {
				printf("     r/s    %sr%s/s   rrqm/s  %%rrqm r_await rareq-sz",
				       spc, units);
			}
			if ((hpart == 2) || !hpart) {
				printf("     w/s    %sw%s/s   wrqm/s  %%wrqm w_await wareq-sz",
				       spc, units);
			}
			if ((hpart == 3) || !hpart) {
			       printf("     d/s    %sd%s/s   drqm/s  %%drqm d_await dareq-sz",
				      spc, units);
			}
			if ((hpart == 4) || !hpart) {
			       printf("     f/s f_await  aqu-sz  %%util");
			}
		}
	}
	else {
		/* Basic stats */
		if (DISPLAY_SHORT_OUTPUT(flags)) {
			printf("      tps   %s%s_read/s    %s%s_w+d/s   %s%s_read    %s%s_w+d",
			       spc, units, spc, units, spc, units, spc, units);
		}
		else {
			printf("      tps   %s%s_read/s   %s%s_wrtn/s   %s%s_dscd/s   %s%s_read   %s%s_wrtn   %s%s_dscd",
			       spc, units, spc, units, spc, units, spc, units, spc, units, spc, units);
		}
	}
	if (DISPLAY_PRETTY(flags)) {
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
 * @hpart	Indicate which part of the report should be displayed in
 *		human mode. A value of 0 indicates that output should not be
 *		broken in several parts.
 * @d		Structure containing device description.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @xds		Extended stats for current device.
 * @xios	Additional extended statistics for current device.
 ***************************************************************************
 */
void write_plain_ext_stat(unsigned long long itv, int fctr, int hpart,
			  struct io_device *d, struct io_stats *ioi,
			  struct io_stats *ioj, char *devname, struct ext_disk_stats *xds,
			  struct ext_io_stats *xios)
{
	int n;

	/* If this is a group with no devices, skip it */
	if (d->dev_tp == T_GROUP)
		return;

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, "%-13s", devname, 0);
	}

	/* Compute number of devices in group */
	if (d->dev_tp > T_GROUP) {
		n = d->dev_tp - T_GROUP;
	}
	else {
		n = 1;
	}

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		/* tps */
		/* Origin (unmerged) flush operations are counted as writes */
		cprintf_f(NO_UNIT, 1, 8, 2,
			  S_VALUE(ioj->rd_ios + ioj->wr_ios + ioj->dc_ios,
				  ioi->rd_ios + ioi->wr_ios + ioi->dc_ios, itv));
		/* kB/s */
		if (!DISPLAY_UNIT(flags)) {
			xios->sectors /= fctr;
		}
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 1, 9, 2,
			  xios->sectors);
		/* rqm/s */
		cprintf_f(NO_UNIT, 1, 8, 2,
			  S_VALUE(ioj->rd_merges + ioj->wr_merges + ioj->dc_merges,
				  ioi->rd_merges + ioi->wr_merges + ioi->dc_merges, itv));
		/* await */
		cprintf_f(NO_UNIT, 1, 7, 2,
			  xds->await);
		/* areq-sz (in kB, not sectors) */
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 1, 8, 2,
			  xds->arqsz / 2);
		/* aqu-sz */
		cprintf_f(NO_UNIT, 1, 7, 2,
			  S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
		/*
		 * %util
		 * Again: Ticks in milliseconds.
		 */
		cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2, xds->util / 10.0 / (double) n);
	}
	else {
		if ((hpart == 1) || !hpart) {
			/* r/s */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  S_VALUE(ioj->rd_ios, ioi->rd_ios, itv));
			/* rkB/s */
			if (!DISPLAY_UNIT(flags)) {
				xios->rsectors /= fctr;
			}
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 1, 9, 2,
				  xios->rsectors);
			/* rrqm/s */
			cprintf_f(NO_UNIT, 1, 8, 2,
				  S_VALUE(ioj->rd_merges, ioi->rd_merges, itv));
			/* %rrqm */
			cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2,
				   xios->rrqm_pc);
			/* r_await */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  xios->r_await);
			/* rareq-sz  (in kB, not sectors) */
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 1, 8, 2,
				  xios->rarqsz / 2);
		}
		if ((hpart == 2) || !hpart) {
			/* w/s */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  S_VALUE(ioj->wr_ios, ioi->wr_ios, itv));
			/* wkB/s */
			if (!DISPLAY_UNIT(flags)) {
				xios->wsectors /= fctr;
			}
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 1, 9, 2,
				  xios->wsectors);
			/* wrqm/s */
			cprintf_f(NO_UNIT, 1, 8, 2,
				  S_VALUE(ioj->wr_merges, ioi->wr_merges, itv));
			/* %wrqm */
			cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2,
				   xios->wrqm_pc);
			/* w_await */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  xios->w_await);
			/* wareq-sz (in kB, not sectors) */
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 1, 8, 2,
				  xios->warqsz / 2);
		}
		if ((hpart == 3) || !hpart) {
			/* d/s */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  S_VALUE(ioj->dc_ios, ioi->dc_ios, itv));
			/* dkB/s */
			if (!DISPLAY_UNIT(flags)) {
				xios->dsectors /= fctr;
			}
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 1, 9, 2,
				  xios->dsectors);
			/* drqm/s */
			cprintf_f(NO_UNIT, 1, 8, 2,
				  S_VALUE(ioj->dc_merges, ioi->dc_merges, itv));
			/* %drqm */
			cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2,
				   xios->drqm_pc);
			/* d_await */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  xios->d_await);
			/* dareq-sz (in kB, not sectors) */
			cprintf_f(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 1, 8, 2,
				  xios->darqsz / 2);
		}
		if ((hpart == 4) || !hpart) {
			/* f/s */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  S_VALUE(ioj->fl_ios, ioi->fl_ios, itv));
			/* f_await */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  xios->f_await);
			/* aqu-sz */
			cprintf_f(NO_UNIT, 1, 7, 2,
				  S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
			/*
			 * %util
			 * Again: Ticks in milliseconds.
			 */
			if (d->dev_tp > T_GROUP) {
				n = d->dev_tp - T_GROUP;
			}
			else {
				n = 1;
			}
			cprintf_pc(DISPLAY_UNIT(flags), 1, 6, 2, xds->util / 10.0 / (double) n);
		}
	}

	if (DISPLAY_PRETTY(flags)) {
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
 * @d		Structure containing the device description.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @devname	Current device name.
 * @xds		Extended stats for current device.
 * @xios	Additional extended statistics for current device.
 ***************************************************************************
 */
void write_json_ext_stat(int tab, unsigned long long itv, int fctr,
			 struct io_device *d, struct io_stats *ioi,
			 struct io_stats *ioj, char *devname, struct ext_disk_stats *xds,
			 struct ext_io_stats *xios)
{
	int n;
	char line[256];

	/* If this is a group with no devices, skip it */
	if (d->dev_tp == T_GROUP)
		return;

	xprintf0(tab,
		 "{\"disk_device\": \"%s\", ",
		 devname);

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		printf("\"tps\": %.2f, \"",
		       /* Origin (unmerged) flush operations are counted as writes */
		       S_VALUE(ioj->rd_ios + ioj->wr_ios + ioj->dc_ios,
			       ioi->rd_ios + ioi->wr_ios + ioi->dc_ios, itv));
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
		       "\"areq-sz\": %.2f, \"aqu-sz\": %.2f, ",
		       xios->sectors /= fctr,
		       S_VALUE(ioj->rd_merges + ioj->wr_merges + ioj->dc_merges,
			       ioi->rd_merges + ioi->wr_merges + ioi->dc_merges, itv),
		       xds->await,
		       xds->arqsz / 2,
		       S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
	}
	else {
		printf("\"r/s\": %.2f, \"w/s\": %.2f, \"d/s\": %.2f, \"f/s\": %.2f, ",
		       S_VALUE(ioj->rd_ios, ioi->rd_ios, itv),
		       S_VALUE(ioj->wr_ios, ioi->wr_ios, itv),
		       S_VALUE(ioj->dc_ios, ioi->dc_ios, itv),
		       S_VALUE(ioj->fl_ios, ioi->fl_ios, itv));
		if (DISPLAY_MEGABYTES(flags)) {
			sprintf(line, "\"rMB/s\": %%.2f, \"wMB/s\": %%.2f, \"dMB/s\": %%.2f, ");
		}
		else if (DISPLAY_KILOBYTES(flags)) {
			sprintf(line, "\"rkB/s\": %%.2f, \"wkB/s\": %%.2f, \"dkB/s\": %%.2f, ");
		}
		else {
			sprintf(line, "\"rsec/s\": %%.2f, \"wsec/s\": %%.2f, \"dsec/s\": %%.2f, ");
		}
		printf(line,
		       xios->rsectors /= fctr,
		       xios->wsectors /= fctr,
		       xios->dsectors /= fctr);
		printf("\"rrqm/s\": %.2f, \"wrqm/s\": %.2f, \"drqm/s\": %.2f, "
		       "\"rrqm\": %.2f, \"wrqm\": %.2f, \"drqm\": %.2f, "
		       "\"r_await\": %.2f, \"w_await\": %.2f, \"d_await\": %.2f, \"f_await\": %.2f, "
		       "\"rareq-sz\": %.2f, \"wareq-sz\": %.2f, \"dareq-sz\": %.2f, "
		       "\"aqu-sz\": %.2f, ",
		       S_VALUE(ioj->rd_merges, ioi->rd_merges, itv),
		       S_VALUE(ioj->wr_merges, ioi->wr_merges, itv),
		       S_VALUE(ioj->dc_merges, ioi->dc_merges, itv),
		       xios->rrqm_pc,
		       xios->wrqm_pc,
		       xios->drqm_pc,
		       xios->r_await,
		       xios->w_await,
		       xios->d_await,
		       xios->f_await,
		       xios->rarqsz / 2,
		       xios->warqsz / 2,
		       xios->darqsz / 2,
		       S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0);
	}

	if (d->dev_tp > T_GROUP) {
		n = d->dev_tp - T_GROUP;
	}
	else {
		n = 1;
	}
	printf("\"util\": %.2f}", xds->util / 10.0 / (double) n);
}

/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys,
 * in plain or JSON format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @hpart	Indicate which part of the report should be displayed in
 *		human mode. A value of 0 indicates that output should not be
 *		broken in several parts.
 * @d		Structure containing device description.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @tab		Number of tabs to print (JSON output only).
 * @dname	Name to be used for display for current device.
 ***************************************************************************
 */
void write_ext_stat(unsigned long long itv, int fctr, int hpart,
		    struct io_device *d, struct io_stats *ioi,
		    struct io_stats *ioj, int tab, char *dname)
{
	struct stats_disk sdc, sdp;
	struct ext_disk_stats xds;
	struct ext_io_stats xios;

	memset(&xds, 0, sizeof(struct ext_disk_stats));
	memset(&xios, 0, sizeof(struct ext_io_stats));

	/*
	 * Counters overflows are possible, but don't need to be handled in
	 * a special way: The difference is still properly calculated if the
	 * result is of the same type as the two values.
	 * Exception is field rq_ticks which is incremented by the number of
	 * I/O in progress times the number of milliseconds spent doing I/O.
	 * But the number of I/O in progress (field ios_pgr) happens to be
	 * sometimes negative...
	 */

	if ((hpart == 4) || !hpart || DISPLAY_SHORT_OUTPUT(flags)) {
		/* Origin (unmerged) flush operations are counted as writes */
		sdc.nr_ios    = ioi->rd_ios + ioi->wr_ios + ioi->dc_ios;
		sdp.nr_ios    = ioj->rd_ios + ioj->wr_ios + ioj->dc_ios;

		sdc.tot_ticks = ioi->tot_ticks;
		sdp.tot_ticks = ioj->tot_ticks;

		sdc.rd_ticks  = ioi->rd_ticks;
		sdp.rd_ticks  = ioj->rd_ticks;
		sdc.wr_ticks  = ioi->wr_ticks;
		sdp.wr_ticks  = ioj->wr_ticks;
		sdc.dc_ticks  = ioi->dc_ticks;
		sdp.dc_ticks  = ioj->dc_ticks;

		sdc.rd_sect   = ioi->rd_sectors;
		sdp.rd_sect   = ioj->rd_sectors;
		sdc.wr_sect   = ioi->wr_sectors;
		sdp.wr_sect   = ioj->wr_sectors;
		sdc.dc_sect   = ioi->dc_sectors;
		sdp.dc_sect   = ioj->dc_sectors;

		compute_ext_disk_stats(&sdc, &sdp, itv, &xds);
	}

	/* rkB/s  wkB/s dkB/s */
	xios.rsectors = S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv);
	xios.wsectors = S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv);
	xios.dsectors = S_VALUE(ioj->dc_sectors, ioi->dc_sectors, itv);

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		xios.sectors  = xios.rsectors + xios.wsectors + xios.dsectors;
	}
	else {
		if ((hpart == 1) || !hpart) {
			/* %rrqm */
			xios.rrqm_pc = (ioi->rd_merges - ioj->rd_merges) + (ioi->rd_ios - ioj->rd_ios) ?
				       (double) ((ioi->rd_merges - ioj->rd_merges)) /
				       ((ioi->rd_merges - ioj->rd_merges) + (ioi->rd_ios - ioj->rd_ios)) * 100 :
				       0.0;
			/* r_await */
			xios.r_await = (ioi->rd_ios - ioj->rd_ios) ?
				       (ioi->rd_ticks - ioj->rd_ticks) /
				       ((double) (ioi->rd_ios - ioj->rd_ios)) : 0.0;
			/* rareq-sz (still in sectors, not kB) */
			xios.rarqsz = (ioi->rd_ios - ioj->rd_ios) ?
				      (ioi->rd_sectors - ioj->rd_sectors) / ((double) (ioi->rd_ios - ioj->rd_ios)) :
				      0.0;
		}
		if ((hpart == 2) || !hpart) {
			/* %wrqm */
			xios.wrqm_pc = (ioi->wr_merges - ioj->wr_merges) + (ioi->wr_ios - ioj->wr_ios) ?
				       (double) ((ioi->wr_merges - ioj->wr_merges)) /
				       ((ioi->wr_merges - ioj->wr_merges) + (ioi->wr_ios - ioj->wr_ios)) * 100 :
				       0.0;
			/* w_await */
			xios.w_await = (ioi->wr_ios - ioj->wr_ios) ?
				       (ioi->wr_ticks - ioj->wr_ticks) /
				       ((double) (ioi->wr_ios - ioj->wr_ios)) : 0.0;
			/* wareq-sz (still in sectors, not kB) */
			xios.warqsz = (ioi->wr_ios - ioj->wr_ios) ?
				      (ioi->wr_sectors - ioj->wr_sectors) / ((double) (ioi->wr_ios - ioj->wr_ios)) :
				      0.0;
		}
		if ((hpart == 3) || !hpart) {
			/* %drqm */
			xios.drqm_pc = (ioi->dc_merges - ioj->dc_merges) + (ioi->dc_ios - ioj->dc_ios) ?
				       (double) ((ioi->dc_merges - ioj->dc_merges)) /
				       ((ioi->dc_merges - ioj->dc_merges) + (ioi->dc_ios - ioj->dc_ios)) * 100 :
				       0.0;
			/* d_await */
			xios.d_await = (ioi->dc_ios - ioj->dc_ios) ?
				       (ioi->dc_ticks - ioj->dc_ticks) /
				       ((double) (ioi->dc_ios - ioj->dc_ios)) : 0.0;
			/* dareq-sz (still in sectors, not kB) */
			xios.darqsz = (ioi->dc_ios - ioj->dc_ios) ?
				      (ioi->dc_sectors - ioj->dc_sectors) / ((double) (ioi->dc_ios - ioj->dc_ios)) :
				      0.0;
		}
		if ((hpart == 4) || !hpart) {
			/* f_await */
			xios.f_await = (ioi->fl_ios - ioj->fl_ios) ?
				       (ioi->fl_ticks - ioj->fl_ticks) /
				       ((double) (ioi->fl_ios - ioj->fl_ios)) : 0.0;
		}
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		write_json_ext_stat(tab, itv, fctr, d, ioi, ioj, dname, &xds, &xios);
	}
	else {
		write_plain_ext_stat(itv, fctr, hpart, d, ioi, ioj, dname, &xds, &xios);
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
 * @dc_sec	Number of sectors discarded.
 ***************************************************************************
 */
void write_plain_basic_stat(unsigned long long itv, int fctr,
			    struct io_stats *ioi, struct io_stats *ioj,
			    char *devname, unsigned long long rd_sec,
			    unsigned long long wr_sec, unsigned long long dc_sec)
{
	double rsectors, wsectors, dsectors;

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, "%-13s", devname, 0);
	}

	rsectors = S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv);
	wsectors = S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv);
	dsectors = S_VALUE(ioj->dc_sectors, ioi->dc_sectors, itv);
	if (!DISPLAY_UNIT(flags)) {
		rsectors /= fctr;
		wsectors /= fctr;
		dsectors /= fctr;
	}

	/* tps */
	cprintf_f(NO_UNIT, 1, 8, 2,
		  /* Origin (unmerged) flush operations are counted as writes */
		  S_VALUE(ioj->rd_ios + ioj->wr_ios + ioj->dc_ios,
			  ioi->rd_ios + ioi->wr_ios + ioi->dc_ios, itv));

	if (DISPLAY_SHORT_OUTPUT(flags)) {
		/* kB_read/s kB_w+d/s */
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 2, 12, 2,
			  rsectors, wsectors + dsectors);
		/* kB_read kB_w+d */
		cprintf_u64(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 2, 10,
			    DISPLAY_UNIT(flags) ? (unsigned long long) rd_sec
						: (unsigned long long) rd_sec / fctr,
			    DISPLAY_UNIT(flags) ? (unsigned long long) wr_sec + dc_sec
						: (unsigned long long) (wr_sec + dc_sec) / fctr);
	}
	else {
		/* kB_read/s kB_wrtn/s kB_dscd/s */
		cprintf_f(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 3, 12, 2,
			  rsectors, wsectors, dsectors);
		/* kB_read kB_wrtn kB_dscd */
		cprintf_u64(DISPLAY_UNIT(flags) ? UNIT_SECTOR : NO_UNIT, 3, 10,
			    DISPLAY_UNIT(flags) ? (unsigned long long) rd_sec
						: (unsigned long long) rd_sec / fctr,
			    DISPLAY_UNIT(flags) ? (unsigned long long) wr_sec
						: (unsigned long long) wr_sec / fctr,
			    DISPLAY_UNIT(flags) ? (unsigned long long) dc_sec
						: (unsigned long long) dc_sec / fctr);
	}

	if (DISPLAY_PRETTY(flags)) {
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
			   unsigned long long wr_sec, unsigned long long dc_sec)
{
	char line[256];

	xprintf0(tab,
		 "{\"disk_device\": \"%s\", \"tps\": %.2f, ",
		 devname,
		 /* Origin (unmerged) flush operations are counted as writes */
		 S_VALUE(ioj->rd_ios + ioj->wr_ios + ioj->dc_ios,
			 ioi->rd_ios + ioi->wr_ios + ioi->dc_ios, itv));
	if (DISPLAY_KILOBYTES(flags)) {
		sprintf(line, "\"kB_read/s\": %%.2f, \"kB_wrtn/s\": %%.2f, \"kB_dscd/s\": %%.2f, "
			"\"kB_read\": %%llu, \"kB_wrtn\": %%llu, \"kB_dscd\": %%llu}");
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		sprintf(line, "\"MB_read/s\": %%.2f, \"MB_wrtn/s\": %%.2f, \"MB_dscd/s\": %%.2f, "
			"\"MB_read\": %%llu, \"MB_wrtn\": %%llu, \"MB_dscd\": %%llu}");
	}
	else {
		sprintf(line, "\"Blk_read/s\": %%.2f, \"Blk_wrtn/s\": %%.2f, \"Blk_dscd/s\": %%.2f, "
			"\"Blk_read\": %%llu, \"Blk_wrtn\": %%llu, \"Blk_dscd\": %%llu}");
	}
	printf(line,
	       S_VALUE(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
	       S_VALUE(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
	       S_VALUE(ioj->dc_sectors, ioi->dc_sectors, itv) / fctr,
	       (unsigned long long) rd_sec / fctr,
	       (unsigned long long) wr_sec / fctr,
	       (unsigned long long) dc_sec / fctr);
}

/*
 ***************************************************************************
 * Write basic stats, read from /proc/diskstats or from sysfs, in plain or
 * JSON format.
 *
 * IN:
 * @itv		Interval of time.
 * @fctr	Conversion factor.
 * @d		Structure containing device description.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 * @tab		Number of tabs to print (JSON format only).
 * @dname	Name to be used for display for current device.
 ***************************************************************************
 */
void write_basic_stat(unsigned long long itv, int fctr,
		      struct io_device *d, struct io_stats *ioi,
		      struct io_stats *ioj, int tab, char *dname)
{
	unsigned long long rd_sec, wr_sec, dc_sec;

	/* Print stats coming from /sys or /proc/diskstats */
	rd_sec = ioi->rd_sectors - ioj->rd_sectors;
	if ((ioi->rd_sectors < ioj->rd_sectors) && (ioj->rd_sectors <= 0xffffffff)) {
		rd_sec &= 0xffffffff;
	}
	wr_sec = ioi->wr_sectors - ioj->wr_sectors;
	if ((ioi->wr_sectors < ioj->wr_sectors) && (ioj->wr_sectors <= 0xffffffff)) {
		wr_sec &= 0xffffffff;
	}
	dc_sec = ioi->dc_sectors - ioj->dc_sectors;
	if ((ioi->dc_sectors < ioj->dc_sectors) && (ioj->dc_sectors <= 0xffffffff)) {
		dc_sec &= 0xffffffff;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		write_json_basic_stat(tab, itv, fctr, ioi, ioj, dname,
				      rd_sec, wr_sec, dc_sec);
	}
	else {
		write_plain_basic_stat(itv, fctr, ioi, ioj, dname,
				       rd_sec, wr_sec, dc_sec);
	}
}

/*
 ***************************************************************************
 * Print everything now (stats and uptime).
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @rectime	Current date and time.
 * @skip	TRUE if nothing should be displayed (option -y). We must
 *		go through write_stats() anyway to compute groups statistics.
 ***************************************************************************
 */
void write_stats(int curr, struct tm *rectime, int skip)
{
	int h, hl = 0, hh = 0, fctr = 1, tab = 4, next = FALSE;
	unsigned long long itv;
	struct io_device *d, *dtmp, *g = NULL, *dnext = NULL;
	char *dev_name;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	if (DISPLAY_JSON_OUTPUT(flags) && !skip) {
		xprintf(tab++, "{");
	}

	/* Print time stamp */
	if (DISPLAY_TIMESTAMP(flags) && !skip) {
		write_sample_timestamp(tab, rectime);
#ifdef DEBUG
		if (DISPLAY_DEBUG(flags)) {
			fprintf(stderr, "%s\n", timestamp);
		}
#endif
	}

	if (DISPLAY_CPU(flags) && !skip) {
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
		struct io_stats *ioi, *ioj, iozero;

		memset(&iozero, 0, sizeof(struct io_stats));

		if (DISPLAY_PRETTY(flags) &&
		    DISPLAY_EXTENDED(flags) &&
		    !DISPLAY_SHORT_OUTPUT(flags) &&
		    !DISPLAY_JSON_OUTPUT(flags)) {
			hl = 1; hh = 4;
		}

		for (h = hl; h <= hh; h++) {

			if (!skip) {
				/* Display disk stats header */
				write_disk_stat_header(&fctr, &tab, h);
			}

			for (d = dev_list; ; d = dnext) {

				if (d == NULL) {
					if (g == NULL)
						/* No group processing in progress */
						break;
					/* Display last group before exit */
					dnext = NULL;
					d = g;
					g = NULL;
				}
				else {
					dnext = d->next;

					if (d->dev_tp >= T_GROUP) {
						/*
						 * This is a new group: Save group position
						 * and display previous one.
						 */
						if (g != NULL) {
							dtmp = g;
							g = d;
							d = dtmp;
							memset(g->dev_stats[curr], 0, sizeof(struct io_stats));
						}
						else {
							g = d;
							memset(g->dev_stats[curr], 0, sizeof(struct io_stats));
							continue;	/* No previous group to display */
						}
					}
				}

				if (!d->exist && (d->dev_tp < T_GROUP))
					/* Current device is non existent (e.g. it has been unregistered from the system */
					continue;

				if ((g != NULL) && (h == hl) && (d->dev_tp < T_GROUP)) {
					/* We are within a group: Increment number of disks in the group */
					(g->dev_tp)++;
					/* Add current device stats to group */
					compute_device_groups_stats(curr, d, g);
				}

				if (DISPLAY_GROUP_TOTAL_ONLY(flags) && (g != NULL) && (d->dev_tp < T_GROUP))
					continue;

				ioi = d->dev_stats[curr];
				ioj = d->dev_stats[!curr];
				/* Origin (unmerged) flush operations are counted as writes */
				if (!DISPLAY_UNFILTERED(flags)) {
					if (!ioi->rd_ios && !ioi->wr_ios && !ioi->dc_ios && !ioi->fl_ios)
						continue;
				}

				if (DISPLAY_ZERO_OMIT(flags)) {
					if ((ioi->rd_ios == ioj->rd_ios) &&
					    (ioi->wr_ios == ioj->wr_ios) &&
					    (ioi->dc_ios == ioj->dc_ios) &&
					    (ioi->fl_ios == ioj->fl_ios))
						/* No activity: Ignore it */
						continue;
				}

				/* Try to detect if device has been removed then inserted again */
				if (((ioi->rd_ios + ioi->wr_ios + ioi->dc_ios + ioi->fl_ios) <
					(ioj->rd_ios + ioj->wr_ios + ioj->dc_ios + ioj->fl_ios)) &&
				    (!ioj->rd_sectors || (ioi->rd_sectors < ioj->rd_sectors)) &&
				    (!ioj->wr_sectors || (ioi->wr_sectors < ioj->wr_sectors)) &&
				    (!ioj->dc_sectors || (ioi->dc_sectors < ioj->dc_sectors))) {
					    ioj = &iozero;
				}

				dev_name = get_device_name(d->major, d->minor, NULL, 0,
							   DISPLAY_DEVMAP_NAME(flags),
							   DISPLAY_PERSIST_NAME_I(flags),
							   FALSE, d->name);
#ifdef DEBUG
				if (DISPLAY_DEBUG(flags)) {
					/* Debug output */
					fprintf(stderr,
						"name=%s itv=%llu fctr=%d ioi{ rd_sectors=%lu "
						"wr_sectors=%lu dc_sectors=%lu "
						"rd_ios=%lu rd_merges=%lu rd_ticks=%u "
						"wr_ios=%lu wr_merges=%lu wr_ticks=%u "
						"dc_ios=%lu dc_merges=%lu dc_ticks=%u "
						"fl_ios=%lu fl_ticks=%u "
						"ios_pgr=%u tot_ticks=%u "
						"rq_ticks=%u }\n",
						dev_name,
						itv,
						fctr,
						ioi->rd_sectors,
						ioi->wr_sectors,
						ioi->dc_sectors,
						ioi->rd_ios,
						ioi->rd_merges,
						ioi->rd_ticks,
						ioi->wr_ios,
						ioi->wr_merges,
						ioi->wr_ticks,
						ioi->dc_ios,
						ioi->dc_merges,
						ioi->dc_ticks,
						ioi->fl_ios,
						ioi->fl_ticks,
						ioi->ios_pgr,
						ioi->tot_ticks,
						ioi->rq_ticks);
				}
#endif

				if (!skip) {
					if (DISPLAY_JSON_OUTPUT(flags) && next) {
						printf(",\n");
					}
					next = TRUE;

					if (DISPLAY_EXTENDED(flags)) {
						write_ext_stat(itv, fctr, h, d, ioi, ioj, tab, dev_name);
					}
					else {
						write_basic_stat(itv, fctr, d, ioi, ioj, tab, dev_name);
					}
				}
			}

			if ((h > 0) && (h < hh) && !skip) {
				printf("\n");
			}
		}
		if (DISPLAY_JSON_OUTPUT(flags) && !skip) {
			printf("\n");
			xprintf(--tab, "]");
		}
	}

	if (!skip) {
		if (DISPLAY_JSON_OUTPUT(flags)) {
			xprintf0(--tab, "}");
		}
		else {
			printf("\n");
		}
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

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Set a handler for SIGINT */
	memset(&int_act, 0, sizeof(int_act));
	int_act.sa_handler = int_handler;
	sigaction(SIGINT, &int_act, NULL);

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	do {
		/* Every device is potentially nonexistent */
		set_devices_nonexistent(dev_list);

		/* Read system uptime */
		read_uptime(&(uptime_cs[curr]));

		/* Read stats for CPU "all" */
		read_stat_cpu(st_cpu[curr], 1);

		/*
		 * Compute the total number of jiffies spent by all processors.
		 * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
		 * already include them.
		 */
		tot_jiffies[curr] = st_cpu[curr]->cpu_user + st_cpu[curr]->cpu_nice +
				    st_cpu[curr]->cpu_sys + st_cpu[curr]->cpu_idle +
				    st_cpu[curr]->cpu_iowait + st_cpu[curr]->cpu_hardirq +
				    st_cpu[curr]->cpu_steal + st_cpu[curr]->cpu_softirq;

		if (DISPLAY_EVERYTHING(flags)) {
			read_diskstats_stat(curr);
		}
		else {
			read_sysfs_dlist_stat(curr);
		}

		/* Get time */
		get_localtime(rectime, 0);

		/* Print results */
		write_stats(curr, rectime, skip);

		if (!skip) {
			if (count > 0) {
				count--;
			}
		}

		if (count) {
			curr ^= 1;
			__pause();

			if (sigint_caught) {
				/* SIGINT signal caught => Terminate JSON output properly */
				count = 0;
			}
			else if (DISPLAY_JSON_OUTPUT(flags) && count && !skip) {
				printf(",");
			}
			skip = 0;
		}
		printf("\n");
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
	struct tm rectime;
	char *t, *persist_devname, *devname;
	char group_name[MAX_NAME_LEN];

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	alt_dir[0] = '\0';

	/* Process args... */
	while (opt < argc) {

		/* -p option used individually. See below for grouped use */
		if (!strcmp(argv[opt], "-p")) {
			if (argv[++opt] &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt])) &&
			    (strncmp(argv[opt], "-", 1))) {
				flags |= I_D_UNFILTERED;

				for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
					if (!strcmp(t, K_ALL)) {
						flags |= I_D_EVERYTHING;
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
						add_list_device(&dev_list, devname, T_PART_DEV);
					}
				}
				opt++;
			}
			else {
				flags |= I_D_EVERYTHING;
			}
		}

		else if (!strcmp(argv[opt], "-g")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			flags |= I_F_GROUP_DEFINED;

			/*
			 * MAX_NAME_LEN - 2: one char for the heading space,
			 * and one for the trailing '\0'.
			 */
			snprintf(group_name, MAX_NAME_LEN, " %-.*s", MAX_NAME_LEN - 2, argv[opt++]);
			add_list_device(&dev_list, group_name, T_GROUP);
		}

		else if (!strcmp(argv[opt], "--human")) {
			flags |= I_D_UNIT;
			opt++;
		}

		else if (!strcmp(argv[opt], "--pretty")) {
			/* Display an easy-to-read CIFS report */
			flags |= I_D_PRETTY;
			opt++;
		}

#ifdef TEST
		else if (!strncmp(argv[opt], "--getenv", 8)) {
			__env = TRUE;
			opt++;
		}
#endif

		else if (!strncmp(argv[opt], "--dec=", 6) && (strlen(argv[opt]) == 7)) {
			/* Get number of decimal places */
			dplaces_nr = atoi(argv[opt] + 6);
			if ((dplaces_nr < 0) || (dplaces_nr > 2)) {
				usage(argv[0]);
			}
			opt++;
		}

		else if (!strcmp(argv[opt], "-f") || !strcmp(argv[opt], "+f")) {
			if (alt_dir[0] || !argv[opt + 1]) {
				usage(argv[0]);
			}
			if (argv[opt++][0] == '+') {
				flags |= I_D_ALL_DIR;
			}
			strncpy(alt_dir, argv[opt++], sizeof(alt_dir));
			alt_dir[sizeof(alt_dir) - 1] = '\0';
			if (!check_dir(alt_dir)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-j")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			if (strnlen(argv[opt], sizeof(persistent_name_type)) >= sizeof(persistent_name_type) - 1) {
				usage(argv[0]);
			}
			strncpy(persistent_name_type, argv[opt], sizeof(persistent_name_type) - 1);
			persistent_name_type[sizeof(persistent_name_type) - 1] = '\0';
			strtolower(persistent_name_type);
			/* Check that this is a valid type of persistent device name */
			if (!get_persistent_type_dir(persistent_name_type)) {
				fprintf(stderr, _("Invalid type of persistent device name\n"));
				exit(1);
			}
			/* Persistent names are usually long: Pretty display them */
			flags |= I_D_PERSIST_NAME + I_D_PRETTY;
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
					/* Option -h is equivalent to --pretty --human */
					flags |= I_D_PRETTY + I_D_UNIT;
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
					flags |= I_D_EVERYTHING;
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

		else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
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
				add_list_device(&dev_list, devname, 0);
			}
			else {
				flags |= I_D_ALL_DEVICES;
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

	/* Init color strings */
	init_colors();

	/* Default: Display CPU and DISK reports */
	if (!report_set) {
		flags |= I_D_CPU + I_D_DISK;
	}
	/*
	 * Also display DISK reports if options -p, -x or a device has been entered
	 * on the command line.
	 */
	if (DISPLAY_EVERYTHING(flags) || DISPLAY_EXTENDED(flags) ||
	    DISPLAY_UNFILTERED(flags)) {
		flags |= I_D_DISK;
	}

	if (!DISPLAY_UNFILTERED(flags)) {
		flags |= I_D_ALL_DEVICES;
	}
	/* Option -H can only be used with option -g */
	if (DISPLAY_GROUP_TOTAL_ONLY(flags) && !GROUP_DEFINED(flags)) {
		usage(argv[0]);
	}

	/* Select disk output unit (kB/s or blocks/s) */
	set_disk_output_unit();

	if (DISPLAY_JSON_OUTPUT(flags)) {
		/* Use a decimal point to make JSON code compliant with RFC7159 */
		setlocale(LC_NUMERIC, "C");
	}

	/* Allocate and init stat common counters */
	init_stats();

	/* How many processors on this machine? */
	cpu_nr = get_cpu_nr(~0, FALSE);

	get_localtime(&rectime, 0);

	/* Get system name, release number and hostname */
	__uname(&header);
	if (print_gal_header(&rectime, header.sysname, header.release,
			     header.nodename, header.machine, cpu_nr,
			     DISPLAY_JSON_OUTPUT(flags))) {
		flags |= I_D_ISO;
	}
	if (!DISPLAY_JSON_OUTPUT(flags)) {
		printf("\n");
	}

	/* Main loop */
	rw_io_stat_loop(count, &rectime);

	return 0;
}
