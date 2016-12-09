/*
 * sar and sadf common routines.
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
#include <time.h>
#include <errno.h>
#include <unistd.h>	/* For STDOUT_FILENO, among others */
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "version.h"
#include "sa.h"
#include "common.h"
#include "ioconf.h"
#include "rd_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

int default_file_used = FALSE;
extern struct act_bitmap cpu_bitmap;

/*
 ***************************************************************************
 * Init a bitmap (CPU, IRQ, etc.).
 *
 * IN:
 * @value	Value used to init bitmap.
 * @sz		Size of the bitmap in bytes.
 *
 * OUT:
 * @bitmap	Bitmap initialized.
 ***************************************************************************
 */
void set_bitmap(unsigned char bitmap[], unsigned char value, unsigned int sz)
{
	register int i;

	for (i = 0; i < sz; i++) {
		bitmap[i] = value;
	}
}

/*
 ***************************************************************************
 * Allocate structures.
 *
 * IN:
 * @act	Array of activities.
 ***************************************************************************
 */
void allocate_structures(struct activity *act[])
{
	int i, j;

	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->nr > 0) {
			for (j = 0; j < 3; j++) {
				SREALLOC(act[i]->buf[j], void,
						(size_t) act[i]->msize * (size_t) act[i]->nr * (size_t) act[i]->nr2);
			}
		}
	}
}

/*
 ***************************************************************************
 * Free structures.
 *
 * IN:
 * @act	Array of activities.
 ***************************************************************************
 */
void free_structures(struct activity *act[])
{
	int i, j;

	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->nr > 0) {
			for (j = 0; j < 3; j++) {
				if (act[i]->buf[j]) {
					free(act[i]->buf[j]);
					act[i]->buf[j] = NULL;
				}
			}
		}
	}
}

/*
  ***************************************************************************
  * Try to get device real name from sysfs tree.
  *
  * IN:
  * @major	Major number of the device.
  * @minor	Minor number of the device.
  *
  * RETURNS:
  * The name of the device, which may be the real name (as it appears in /dev)
  * or NULL.
  ***************************************************************************
  */
char *get_devname_from_sysfs(unsigned int major, unsigned int minor)
{
	static char link[32], target[PATH_MAX];
	char *devname;
	ssize_t r;

	snprintf(link, 32, "%s/%u:%u", SYSFS_DEV_BLOCK, major, minor);

	/* Get full path to device knowing its major and minor numbers */
	r = readlink(link, target, PATH_MAX);
	if (r <= 0 || r >= PATH_MAX) {
		return (NULL);
	}

	target[r] = '\0';

	/* Get device name */
	devname = basename(target);
	if (!devname || strnlen(devname, FILENAME_MAX) == 0) {
		return (NULL);
	}

	return (devname);
}

/*
 ***************************************************************************
 * Get device real name if possible.
 * Warning: This routine may return a bad name on 2.4 kernels where
 * disk activities are read from /proc/stat.
 *
 * IN:
 * @major	Major number of the device.
 * @minor	Minor number of the device.
 * @pretty	TRUE if the real name of the device (as it appears in /dev)
 * 		should be returned.
 *
 * RETURNS:
 * The name of the device, which may be the real name (as it appears in /dev)
 * or a string with the following format devM-n.
 ***************************************************************************
 */
char *get_devname(unsigned int major, unsigned int minor, int pretty)
{
	static char buf[32];
	char *name;

	snprintf(buf, 32, "dev%u-%u", major, minor);

	if (!pretty)
		return (buf);

	name = get_devname_from_sysfs(major, minor);
	if (name != NULL)
		return (name);

	name = ioc_name(major, minor);
	if ((name != NULL) && strcmp(name, K_NODEV))
		return (name);

	return (buf);
}

/*
 ***************************************************************************
 * Check if we are close enough to desired interval.
 *
 * IN:
 * @uptime_ref	Uptime used as reference. This is the system uptime for the
 *		first sample statistics, or the first system uptime after a
 *		LINUX RESTART.
 * @uptime	Current system uptime.
 * @reset	TRUE if @last_uptime should be reset with @uptime_ref.
 * @interval	Interval of time.
 *
 * RETURNS:
 * 1 if we are actually close enough to desired interval, 0 otherwise.
 ***************************************************************************
*/
int next_slice(unsigned long long uptime_ref, unsigned long long uptime,
	       int reset, long interval)
{
	unsigned long file_interval, entry;
	static unsigned long long last_uptime = 0;
	int min, max, pt1, pt2;
	double f;

	/* uptime is expressed in jiffies (basis of 1 processor) */
	if (!last_uptime || reset) {
		last_uptime = uptime_ref;
	}

	/* Interval cannot be greater than 0xffffffff here */
	f = ((double) ((uptime - last_uptime) & 0xffffffff)) / HZ;
	file_interval = (unsigned long) f;
	if ((f * 10) - (file_interval * 10) >= 5) {
		file_interval++; /* Rounding to correct value */
	}

	last_uptime = uptime;

	/*
	 * A few notes about the "algorithm" used here to display selected entries
	 * from the system activity file (option -f with -i flag):
	 * Let 'Iu' be the interval value given by the user on the command line,
	 *     'If' the interval between current and previous line in the system
	 * activity file,
	 * and 'En' the nth entry (identified by its time stamp) of the file.
	 * We choose In = [ En - If/2, En + If/2 [ if If is even,
	 *        or In = [ En - If/2, En + If/2 ] if not.
	 * En will be displayed if
	 *       (Pn * Iu) or (P'n * Iu) belongs to In
	 * with  Pn = En / Iu and P'n = En / Iu + 1
	 */
	f = ((double) ((uptime - uptime_ref) & 0xffffffff)) / HZ;
	entry = (unsigned long) f;
	if ((f * 10) - (entry * 10) >= 5) {
		entry++;
	}

	min = entry - (file_interval / 2);
	max = entry + (file_interval / 2) + (file_interval & 0x1);
	pt1 = (entry / interval) * interval;
	pt2 = ((entry / interval) + 1) * interval;

	return (((pt1 >= min) && (pt1 < max)) || ((pt2 >= min) && (pt2 < max)));
}

/*
 ***************************************************************************
 * Use time stamp to fill tstamp structure.
 *
 * IN:
 * @timestamp	Timestamp to decode (format: HH:MM:SS).
 *
 * OUT:
 * @tse		Structure containing the decoded timestamp.
 *
 * RETURNS:
 * 0 if the timestamp has been successfully decoded, 1 otherwise.
 ***************************************************************************
 */
int decode_timestamp(char timestamp[], struct tstamp *tse)
{
	timestamp[2] = timestamp[5] = '\0';
	tse->tm_sec  = atoi(&timestamp[6]);
	tse->tm_min  = atoi(&timestamp[3]);
	tse->tm_hour = atoi(timestamp);

	if ((tse->tm_sec < 0) || (tse->tm_sec > 59) ||
	    (tse->tm_min < 0) || (tse->tm_min > 59) ||
	    (tse->tm_hour < 0) || (tse->tm_hour > 23))
		return 1;

	tse->use = TRUE;

	return 0;
}

/*
 ***************************************************************************
 * Compare two timestamps.
 *
 * IN:
 * @rectime	Date and time for current sample.
 * @tse		Timestamp used as reference.
 *
 * RETURNS:
 * A positive value if @rectime is greater than @tse,
 * a negative one otherwise.
 ***************************************************************************
 */
int datecmp(struct tm *rectime, struct tstamp *tse)
{
	if (rectime->tm_hour == tse->tm_hour) {
		if (rectime->tm_min == tse->tm_min)
			return (rectime->tm_sec - tse->tm_sec);
		else
			return (rectime->tm_min - tse->tm_min);
	}
	else
		return (rectime->tm_hour - tse->tm_hour);
}

/*
 ***************************************************************************
 * Parse a timestamp entered on the command line (hh:mm[:ss]) and decode it.
 *
 * IN:
 * @argv		Arguments list.
 * @opt			Index in the arguments list.
 * @def_timestamp	Default timestamp to use.
 *
 * OUT:
 * @tse			Structure containing the decoded timestamp.
 *
 * RETURNS:
 * 0 if the timestamp has been successfully decoded, 1 otherwise.
 ***************************************************************************
 */
int parse_timestamp(char *argv[], int *opt, struct tstamp *tse,
		    const char *def_timestamp)
{
	char timestamp[9];

	if (argv[++(*opt)]) {
		switch (strlen(argv[*opt])) {

			case 5:
				strncpy(timestamp, argv[(*opt)++], 5);
				strcat(timestamp,":00");
				break;

			case 8:
				strncpy(timestamp, argv[(*opt)++], 8);
				break;

			default:
				strncpy(timestamp, def_timestamp, 8);
				break;
		}
	} else {
		strncpy(timestamp, def_timestamp, 8);
	}
	timestamp[8] = '\0';

	return decode_timestamp(timestamp, tse);
}

/*
 ***************************************************************************
 * Look for the most recent of saDD and saYYYYMMDD to decide which one to
 * use. If neither exists then use saDD by default.
 *
 * IN:
 * @sa_dir	Directory where standard daily data files are saved.
 * @rectime	Structure containing the current date.
 *
 * OUT:
 * @sa_name	0 to use saDD data files,
 * 		1 to use saYYYYMMDD data files.
 ***************************************************************************
 */
void guess_sa_name(char *sa_dir, struct tm *rectime, int *sa_name)
{
	char filename[MAX_FILE_LEN];
	struct stat sb;
	time_t sa_mtime;

	/* Use saDD by default */
	*sa_name = 0;

	/* Look for saYYYYMMDD */
	snprintf(filename, MAX_FILE_LEN,
		 "%s/sa%04d%02d%02d", sa_dir,
		 rectime->tm_year + 1900,
		 rectime->tm_mon + 1,
		 rectime->tm_mday);
	filename[MAX_FILE_LEN - 1] = '\0';

	if (stat(filename, &sb) < 0)
		/* Cannot find or access saYYYYMMDD, so use saDD */
		return;
	sa_mtime = sb.st_mtime;

	/* Look for saDD */
	snprintf(filename, MAX_FILE_LEN,
		 "%s/sa%02d", sa_dir,
		 rectime->tm_mday);
	filename[MAX_FILE_LEN - 1] = '\0';

	if (stat(filename, &sb) < 0) {
		/* Cannot find or access saDD, so use saYYYYMMDD */
		*sa_name = 1;
		return;
	}

	if (sa_mtime > sb.st_mtime) {
		/* saYYYYMMDD is more recent than saDD, so use it */
		*sa_name = 1;
	}
}

/*
 ***************************************************************************
 * Set current daily data file name.
 *
 * IN:
 * @datafile	If not an empty string then this is the alternate directory
 *		location where daily data files will be saved.
 * @d_off	Day offset (number of days to go back in the past).
 * @sa_name	0 for saDD data files,
 * 		1 for saYYYYMMDD data files,
 * 		-1 if unknown. In this case, will look for the most recent
 * 		of saDD and saYYYYMMDD and use it.
 *
 * OUT:
 * @datafile	Name of daily data file.
 ***************************************************************************
 */
void set_default_file(char *datafile, int d_off, int sa_name)
{
	char sa_dir[MAX_FILE_LEN];
	struct tm rectime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL};

	/* Set directory where daily data files will be saved */
	if (datafile[0]) {
		strncpy(sa_dir, datafile, MAX_FILE_LEN);
	}
	else {
		strncpy(sa_dir, SA_DIR, MAX_FILE_LEN);
	}
	sa_dir[MAX_FILE_LEN - 1] = '\0';

	get_time(&rectime, d_off);
	if (sa_name < 0) {
		/*
		 * Look for the most recent of saDD and saYYYYMMDD
		 * and use it. If neither exists then use saDD.
		 * sa_name is set accordingly.
		 */
		guess_sa_name(sa_dir, &rectime, &sa_name);
	}
	if (sa_name) {
		/* Using saYYYYMMDD data files */
		snprintf(datafile, MAX_FILE_LEN,
			 "%s/sa%04d%02d%02d", sa_dir,
			 rectime.tm_year + 1900,
			 rectime.tm_mon + 1,
			 rectime.tm_mday);
	}
	else {
		/* Using saDD data files */
		snprintf(datafile, MAX_FILE_LEN,
			 "%s/sa%02d", sa_dir,
			 rectime.tm_mday);
	}
	datafile[MAX_FILE_LEN - 1] = '\0';
	default_file_used = TRUE;
}

/*
 ***************************************************************************
 * Check data file type. If it is a directory then this is the alternate
 * location where daily data files will be saved.
 *
 * IN:
 * @datafile	Name of the daily data file. May be a directory.
 * @d_off	Day offset (number of days to go back in the past).
 * @sa_name	0 for saDD data files,
 * 		1 for saYYYYMMDD data files,
 * 		-1 if unknown. In this case, will look for the most recent
 * 		of saDD and saYYYYMMDD and use it.
 *
 *
 * OUT:
 * @datafile	Name of the daily data file. This is now a plain file, not
 * 		a directory.
 *
 * RETURNS:
 * 1 if @datafile was a directory, and 0 otherwise.
 ***************************************************************************
 */
int check_alt_sa_dir(char *datafile, int d_off, int sa_name)
{
	struct stat sb;

	if (stat(datafile, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			/*
			 * This is a directory: So append
			 * the default file name to it.
			 */
			set_default_file(datafile, d_off, sa_name);
			return 1;
		}
	}

	return 0;
}

/*
 ***************************************************************************
 * Set interval value.
 *
 * IN:
 * @record_hdr_curr	Record with current sample statistics.
 * @record_hdr_prev	Record with previous sample statistics.
 * @nr_proc		Number of CPU, including CPU "all".
 *
 * OUT:
 * @itv			Interval in jiffies.
 * @g_itv		Interval in jiffies multiplied by the # of proc.
 ***************************************************************************
 */
void get_itv_value(struct record_header *record_hdr_curr,
		   struct record_header *record_hdr_prev,
		   unsigned int nr_proc,
		   unsigned long long *itv, unsigned long long *g_itv)
{
	/* Interval value in jiffies */
	*g_itv = get_interval(record_hdr_prev->uptime,
			      record_hdr_curr->uptime);

	if (nr_proc > 2) {
		*itv = get_interval(record_hdr_prev->uptime0,
				    record_hdr_curr->uptime0);
	}
	else {
		*itv = *g_itv;
	}
}

/*
 ***************************************************************************
 * Fill the rectime structure with the file's creation date, based on file's
 * time data saved in file header.
 * The resulting timestamp is expressed in the locale of the file creator or
 * in the user's own locale, depending on whether option -t has been used
 * or not.
 *
 * IN:
 * @flags	Flags for common options and system state.
 * @file_hdr	System activity file standard header.
 *
 * OUT:
 * @rectime	Date (and possibly time) from file header. Only the date,
 * 		not the time, should be used by the caller.
 ***************************************************************************
 */
void get_file_timestamp_struct(unsigned int flags, struct tm *rectime,
			       struct file_header *file_hdr)
{
	struct tm *loc_t;

	if (PRINT_TRUE_TIME(flags)) {
		/* Get local time. This is just to fill fields with a default value. */
		get_time(rectime, 0);

		rectime->tm_mday = file_hdr->sa_day;
		rectime->tm_mon  = file_hdr->sa_month;
		rectime->tm_year = file_hdr->sa_year;
		/*
		 * Call mktime() to set DST (Daylight Saving Time) flag.
		 * Has anyone a better way to do it?
		 */
		rectime->tm_hour = rectime->tm_min = rectime->tm_sec = 0;
		mktime(rectime);
	}
	else {
		if ((loc_t = localtime((const time_t *) &file_hdr->sa_ust_time)) != NULL) {
			*rectime = *loc_t;
		}
	}
}

/*
 ***************************************************************************
 * Print report header.
 *
 * IN:
 * @flags	Flags for common options and system state.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	Number of CPU (value in [1, NR_CPUS + 1]).
 * 		1 means that there is only one proc and non SMP kernel.
 * 		2 means one proc and SMP kernel.
 * 		Etc.
 *
 * OUT:
 * @rectime	Date and time from file header.
 ***************************************************************************
 */
void print_report_hdr(unsigned int flags, struct tm *rectime,
		      struct file_header *file_hdr, int cpu_nr)
{

	/* Get date of file creation */
	get_file_timestamp_struct(flags, rectime, file_hdr);

	/* Display the header */
	print_gal_header(rectime, file_hdr->sa_sysname, file_hdr->sa_release,
			 file_hdr->sa_nodename, file_hdr->sa_machine,
			 cpu_nr > 1 ? cpu_nr - 1 : 1);
}

/*
 ***************************************************************************
 * Network interfaces may now be registered (and unregistered) dynamically.
 * This is what we try to guess here.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @ref		Index in array for sample statistics used as reference.
 * @pos		Index on current network interface.
 *
 * RETURNS:
 * Position of current network interface in array of sample statistics used
 * as reference, or -1 if it is a new interface (it was not present in array
 * of stats used as reference).
 *
 * Note: A newly registered interface, if it is supernumerary, may make the
 * last interface in the array going out of the list. Yet an interface going
 * out of the list still exists in the /proc/net/dev file. Should it go back
 * in the list (e.g. if some other interfaces have been unregistered) then
 * its counters will jump as if starting from zero.
 ***************************************************************************
 */
int check_net_dev_reg(struct activity *a, int curr, int ref, int pos)
{
	struct stats_net_dev *sndc, *sndp;
	int index = 0;

	sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + pos * a->msize);

	while (index < a->nr) {
		sndp = (struct stats_net_dev *) ((char *) a->buf[ref] + index * a->msize);
		if (!strcmp(sndc->interface, sndp->interface)) {
			/*
			 * Network interface found.
			 * If a counter has decreased, then we may assume that the
			 * corresponding interface was unregistered, then registered again.
			 */
			if ((sndc->rx_packets    < sndp->rx_packets)    ||
			    (sndc->tx_packets    < sndp->tx_packets)    ||
			    (sndc->rx_bytes      < sndp->rx_bytes)      ||
			    (sndc->tx_bytes      < sndp->tx_bytes)      ||
			    (sndc->rx_compressed < sndp->rx_compressed) ||
			    (sndc->tx_compressed < sndp->tx_compressed) ||
			    (sndc->multicast     < sndp->multicast)) {

				/*
				 * Special processing for rx_bytes (_packets) and
				 * tx_bytes (_packets) counters: If the number of
				 * bytes (packets) has decreased, whereas the number of
				 * packets (bytes) has increased, then assume that the
				 * relevant counter has met an overflow condition, and that
				 * the interface was not unregistered, which is all the
				 * more plausible that the previous value for the counter
				 * was > ULONG_MAX/2.
				 * NB: the average value displayed will be wrong in this case...
				 *
				 * If such an overflow is detected, just set the flag. There is no
				 * need to handle this in a special way: the difference is still
				 * properly calculated if the result is of the same type (i.e.
				 * unsigned long) as the two values.
				 */
				int ovfw = FALSE;

				if ((sndc->rx_bytes   < sndp->rx_bytes)   &&
				    (sndc->rx_packets > sndp->rx_packets) &&
				    (sndp->rx_bytes   > (~0UL >> 1))) {
					ovfw = TRUE;
				}
				if ((sndc->tx_bytes   < sndp->tx_bytes)   &&
				    (sndc->tx_packets > sndp->tx_packets) &&
				    (sndp->tx_bytes   > (~0UL >> 1))) {
					ovfw = TRUE;
				}
				if ((sndc->rx_packets < sndp->rx_packets) &&
				    (sndc->rx_bytes   > sndp->rx_bytes)   &&
				    (sndp->rx_packets > (~0UL >> 1))) {
					ovfw = TRUE;
				}
				if ((sndc->tx_packets < sndp->tx_packets) &&
				    (sndc->tx_bytes   > sndp->tx_bytes)   &&
				    (sndp->tx_packets > (~0UL >> 1))) {
					ovfw = TRUE;
				}

				if (!ovfw) {
					/*
					 * OK: assume here that the device was
					 * actually unregistered.
					 */
					memset(sndp, 0, STATS_NET_DEV_SIZE);
					strncpy(sndp->interface, sndc->interface, MAX_IFACE_LEN - 1);
				}
			}
			return index;
		}
		index++;
	}

	/* This is a newly registered interface */
	return -1;
}

/*
 ***************************************************************************
 * Network interfaces may now be registered (and unregistered) dynamically.
 * This is what we try to guess here.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @ref		Index in array for sample statistics used as reference.
 * @pos		Index on current network interface.
 *
 * RETURNS:
 * Position of current network interface in array of sample statistics used
 * as reference, or -1 if it is a newly registered interface.
 ***************************************************************************
 */
int check_net_edev_reg(struct activity *a, int curr, int ref, int pos)
{
	struct stats_net_edev *snedc, *snedp;
	int index = 0;

	snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + pos * a->msize);

	while (index < a->nr) {
		snedp = (struct stats_net_edev *) ((char *) a->buf[ref] + index * a->msize);
		if (!strcmp(snedc->interface, snedp->interface)) {
			/*
			 * Network interface found.
			 * If a counter has decreased, then we may assume that the
			 * corresponding interface was unregistered, then registered again.
			 */
			if ((snedc->tx_errors         < snedp->tx_errors)         ||
			    (snedc->collisions        < snedp->collisions)        ||
			    (snedc->rx_dropped        < snedp->rx_dropped)        ||
			    (snedc->tx_dropped        < snedp->tx_dropped)        ||
			    (snedc->tx_carrier_errors < snedp->tx_carrier_errors) ||
			    (snedc->rx_frame_errors   < snedp->rx_frame_errors)   ||
			    (snedc->rx_fifo_errors    < snedp->rx_fifo_errors)    ||
			    (snedc->tx_fifo_errors    < snedp->tx_fifo_errors)) {

				/*
				 * OK: assume here that the device was
				 * actually unregistered.
				 */
				memset(snedp, 0, STATS_NET_EDEV_SIZE);
				strncpy(snedp->interface, snedc->interface, MAX_IFACE_LEN - 1);
			}
			return index;
		}
		index++;
	}

	/* This is a newly registered interface */
	return -1;
}

/*
 ***************************************************************************
 * Disks may be registered dynamically (true in /proc/stat file).
 * This is what we try to guess here.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @ref		Index in array for sample statistics used as reference.
 * @pos		Index on current disk.
 *
 * RETURNS:
 * Position of current disk in array of sample statistics used as reference
 * or -1 if it is a newly registered device.
 ***************************************************************************
 */
int check_disk_reg(struct activity *a, int curr, int ref, int pos)
{
	struct stats_disk *sdc, *sdp;
	int index = 0;

	sdc = (struct stats_disk *) ((char *) a->buf[curr] + pos * a->msize);

	while (index < a->nr) {
		sdp = (struct stats_disk *) ((char *) a->buf[ref] + index * a->msize);
		if ((sdc->major == sdp->major) &&
		    (sdc->minor == sdp->minor)) {
			/*
			 * Disk found.
			 * If all the counters have decreased then the likelyhood
			 * is that the disk has been unregistered and a new disk inserted.
			 * If only one or two have decreased then the likelyhood
			 * is that the counter has simply wrapped.
			 */
			if ((sdc->nr_ios < sdp->nr_ios) &&
			    (sdc->rd_sect < sdp->rd_sect) &&
			    (sdc->wr_sect < sdp->wr_sect)) {

				memset(sdp, 0, STATS_DISK_SIZE);
				sdp->major = sdc->major;
				sdp->minor = sdc->minor;
			}
			return index;
		}
		index++;
	}

	/* This is a newly registered device */
	return -1;
}

/*
 ***************************************************************************
 * Allocate bitmaps for activities that have one.
 *
 * IN:
 * @act		Array of activities.
 ***************************************************************************
 */
void allocate_bitmaps(struct activity *act[])
{
	int i;

	for (i = 0; i < NR_ACT; i++) {
		/*
		 * If current activity has a bitmap which has not already
		 * been allocated, then allocate it.
		 * Note that a same bitmap may be used by several activities.
		 */
		if (act[i]->bitmap && !act[i]->bitmap->b_array) {
			SREALLOC(act[i]->bitmap->b_array, unsigned char,
				 BITMAP_SIZE(act[i]->bitmap->b_size));
		}
	}
}

/*
 ***************************************************************************
 * Free bitmaps for activities that have one.
 *
 * IN:
 * @act		Array of activities.
 ***************************************************************************
 */
void free_bitmaps(struct activity *act[])
{
	int i;

	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->bitmap && act[i]->bitmap->b_array) {
			free(act[i]->bitmap->b_array);
			/* Set pointer to NULL to prevent it from being freed again */
			act[i]->bitmap->b_array = NULL;
		}
	}
}

/*
 ***************************************************************************
 * Look for activity in array.
 *
 * IN:
 * @act		Array of activities.
 * @act_flag	Activity flag to look for.
 * @stop	TRUE if sysstat should exit when activity is not found.
 *
 * RETURNS:
 * Position of activity in array, or -1 if not found (this may happen when
 * reading data from a system activity file created by another version of
 * sysstat).
 ***************************************************************************
 */
int get_activity_position(struct activity *act[], unsigned int act_flag, int stop)
{
	int i;

	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->id == act_flag)
			return i;
	}

	if (stop) {
		PANIC((int) act_flag);
	}

	return -1;
}

/*
 ***************************************************************************
 * Count number of activities with given option.
 *
 * IN:
 * @act			Array of activities.
 * @option		Option that activities should have to be counted
 *			(eg. AO_COLLECTED...)
 * @count_outputs	TRUE if each output should be counted for activities with
 * 			multiple outputs.
 *
 * RETURNS:
 * Number of selected activities
 ***************************************************************************
 */
int get_activity_nr(struct activity *act[], unsigned int option, int count_outputs)
{
	int i, n = 0;
	unsigned int msk;

	for (i = 0; i < NR_ACT; i++) {
		if ((act[i]->options & option) == option) {

			if (HAS_MULTIPLE_OUTPUTS(act[i]->options) && count_outputs) {
				for (msk = 1; msk < 0x100; msk <<= 1) {
					if ((act[i]->opt_flags & 0xff) & msk) {
						n++;
					}
				}
			}
			else {
				n++;
			}
		}
	}

	return n;
}

/*
 ***************************************************************************
 * Select all activities, even if they have no associated items.
 *
 * IN:
 * @act		Array of activities.
 *
 * OUT:
 * @act		Array of activities, all of the being selected.
 ***************************************************************************
 */
void select_all_activities(struct activity *act[])
{
	int i;

	for (i = 0; i < NR_ACT; i++) {
		act[i]->options |= AO_SELECTED;
	}
}

/*
 ***************************************************************************
 * Select CPU activity if no other activities have been explicitly selected.
 * Also select CPU "all" if no other CPU has been selected.
 *
 * IN:
 * @act		Array of activities.
 *
 * OUT:
 * @act		Array of activities with CPU activity selected if needed.
 ***************************************************************************
 */
void select_default_activity(struct activity *act[])
{
	int p;

	p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);

	/* Default is CPU activity... */
	if (!get_activity_nr(act, AO_SELECTED, COUNT_ACTIVITIES)) {
		/*
		 * Still OK even when reading stats from a file
		 * since A_CPU activity is always recorded.
		 */
		act[p]->options |= AO_SELECTED;
	}

	/*
	 * If no CPU's have been selected then select CPU "all".
	 * cpu_bitmap bitmap may be used by several activities (A_CPU, A_PWR_CPUFREQ...)
	 */
	if (!count_bits(cpu_bitmap.b_array, BITMAP_SIZE(cpu_bitmap.b_size))) {
		cpu_bitmap.b_array[0] |= 0x01;
	}
}

/*
 ***************************************************************************
 * Read data from a system activity data file.
 *
 * IN:
 * @ifd		Input file descriptor.
 * @buffer	Buffer where data are read.
 * @size	Number of bytes to read.
 * @mode	If set to HARD_SIZE, indicate that an EOF should be considered
 * 		as an error.
 *
 * RETURNS:
 * 1 if EOF has been reached, 0 otherwise.
 ***************************************************************************
 */
int sa_fread(int ifd, void *buffer, int size, int mode)
{
	int n;

	if ((n = read(ifd, buffer, size)) < 0) {
		fprintf(stderr, _("Error while reading system activity file: %s\n"),
			strerror(errno));
		close(ifd);
		exit(2);
	}

	if (!n && (mode == SOFT_SIZE))
		return 1;	/* EOF */

	if (n < size) {
		fprintf(stderr, _("End of system activity file unexpected\n"));
		close(ifd);
		exit(2);
	}

	return 0;
}

/*
 ***************************************************************************
 * Display sysstat version used to create system activity data file.
 *
 * IN:
 * @st		Output stream (stderr or stdout).
 * @file_magic	File magic header.
 ***************************************************************************
 */
void display_sa_file_version(FILE *st, struct file_magic *file_magic)
{
	fprintf(st, _("File created by sar/sadc from sysstat version %d.%d.%d"),
		file_magic->sysstat_version,
		file_magic->sysstat_patchlevel,
		file_magic->sysstat_sublevel);

	if (file_magic->sysstat_extraversion) {
		fprintf(st, ".%d", file_magic->sysstat_extraversion);
	}
	fprintf(st, "\n");
}

/*
 ***************************************************************************
 * An invalid system activity file has been opened for reading.
 * If this file was created by an old version of sysstat, tell it to the
 * user...
 *
 * IN:
 * @fd		Descriptor of the file that has been opened.
 * @file_magic	file_magic structure filled with file magic header data.
 *		May contain invalid data.
 * @file	Name of the file being read.
 * @n		Number of bytes read while reading file magic header.
 * 		This function may also be called after failing to read file
 *		standard header, or if CPU activity has not been found in
 *		file. In this case, n is set to 0.
 ***************************************************************************
 */
void handle_invalid_sa_file(int *fd, struct file_magic *file_magic, char *file,
			    int n)
{
	unsigned short sm;

	fprintf(stderr, _("Invalid system activity file: %s\n"), file);

	if (n == FILE_MAGIC_SIZE) {
		sm = (file_magic->sysstat_magic << 8) | (file_magic->sysstat_magic >> 8);
		if ((file_magic->sysstat_magic == SYSSTAT_MAGIC) || (sm == SYSSTAT_MAGIC)) {
			/*
			 * This is a sysstat file, but this file has an old format
			 * or its internal endian format doesn't match.
			 */
			display_sa_file_version(stderr, file_magic);

			if (sm == SYSSTAT_MAGIC) {
				fprintf(stderr, _("Endian format mismatch\n"));
			}
			else {
				fprintf(stderr,
					_("Current sysstat version cannot read the format of this file (%#x)\n"),
					file_magic->format_magic);
			}
		}
	}

	close (*fd);
	exit(3);
}

/*
 ***************************************************************************
 * Move structures data.
 *
 * IN:
 * @act		Array of activities.
 * @id_seq	Activity sequence in file.
 * @record_hdr	Current record header.
 * @dest	Index in array where stats have to be copied to.
 * @src		Index in array where stats to copy are.
 ***************************************************************************
 */
void copy_structures(struct activity *act[], unsigned int id_seq[],
		     struct record_header record_hdr[], int dest, int src)
{
	int i, p;

	memcpy(&record_hdr[dest], &record_hdr[src], RECORD_HEADER_SIZE);

	for (i = 0; i < NR_ACT; i++) {

		if (!id_seq[i])
			continue;

		p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
		if ((act[p]->nr < 1) || (act[p]->nr2 < 1)) {
			PANIC(1);
		}

		memcpy(act[p]->buf[dest], act[p]->buf[src],
		       (size_t) act[p]->msize * (size_t) act[p]->nr * (size_t) act[p]->nr2);
	}
}

/*
 ***************************************************************************
 * Read varying part of the statistics from a daily data file.
 *
 * IN:
 * @act		Array of activities.
 * @curr	Index in array for current sample statistics.
 * @ifd		Input file descriptor.
 * @act_nr	Number of activities in file.
 * @file_actlst	Activity list in file.
 ***************************************************************************
 */
void read_file_stat_bunch(struct activity *act[], int curr, int ifd, int act_nr,
			  struct file_activity *file_actlst)
{
	int i, j, k, p;
	struct file_activity *fal = file_actlst;
	off_t offset;

	for (i = 0; i < act_nr; i++, fal++) {

		if (((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) < 0) ||
		    (act[p]->magic != fal->magic)) {
			/*
			 * Ignore current activity in file, which is unknown to
			 * current sysstat version or has an unknown format.
			 */
			offset = (off_t) fal->size * (off_t) fal->nr * (off_t) fal->nr2;
			if (lseek(ifd, offset, SEEK_CUR) < offset) {
				close(ifd);
				perror("lseek");
				exit(2);
			}
		}
		else if ((act[p]->nr > 0) &&
			 ((act[p]->nr > 1) || (act[p]->nr2 > 1)) &&
			 (act[p]->msize > act[p]->fsize)) {
			for (j = 0; j < act[p]->nr; j++) {
				for (k = 0; k < act[p]->nr2; k++) {
					sa_fread(ifd,
						 (char *) act[p]->buf[curr] + (j * act[p]->nr2 + k) * act[p]->msize,
						 act[p]->fsize, HARD_SIZE);
				}
			}
		}
		else if (act[p]->nr > 0) {
			sa_fread(ifd, act[p]->buf[curr], act[p]->fsize * act[p]->nr * act[p]->nr2, HARD_SIZE);
		}
		else {
			PANIC(p);
		}
	}
}

/*
 ***************************************************************************
 * Open a sysstat activity data file and read its magic structure.
 *
 * IN:
 * @dfile	Name of system activity data file.
 * @ignore	Set to 1 if a true sysstat activity file but with a bad
 * 		format should not yield an error message. Useful with
 * 		sadf -H and sadf -c.
 *
 * OUT:
 * @fd		System activity data file descriptor.
 * @file_magic	file_magic structure containing data read from file magic
 *		header.
 *
 * RETURNS:
 * -1 if data file is a sysstat file with an old format, 0 otherwise.
 ***************************************************************************
 */
int sa_open_read_magic(int *fd, char *dfile, struct file_magic *file_magic,
		       int ignore)
{
	int n;

	/* Open sa data file */
	if ((*fd = open(dfile, O_RDONLY)) < 0) {
		int saved_errno = errno;

		fprintf(stderr, _("Cannot open %s: %s\n"), dfile, strerror(errno));

		if ((saved_errno == ENOENT) && default_file_used) {
			fprintf(stderr, _("Please check if data collecting is enabled\n"));
		}
		exit(2);
	}

	/* Read file magic data */
	n = read(*fd, file_magic, FILE_MAGIC_SIZE);

	if ((n != FILE_MAGIC_SIZE) ||
	    (file_magic->sysstat_magic != SYSSTAT_MAGIC) ||
	    ((file_magic->format_magic != FORMAT_MAGIC) && !ignore)) {
		/* Display error message and exit */
		handle_invalid_sa_file(fd, file_magic, dfile, n);
	}
	if ((file_magic->sysstat_version > 10) ||
	    ((file_magic->sysstat_version == 10) && (file_magic->sysstat_patchlevel >= 3))) {
		/* header_size field exists only for sysstat versions 10.3.1 and later */
		if ((file_magic->header_size <= MIN_FILE_HEADER_SIZE) ||
		    (file_magic->header_size > MAX_FILE_HEADER_SIZE) ||
		    ((file_magic->header_size < FILE_HEADER_SIZE) && !ignore)) {
			/* Display error message and exit */
			handle_invalid_sa_file(fd, file_magic, dfile, n);
		}
	}
	if (file_magic->format_magic != FORMAT_MAGIC)
		/* This is an old sa datafile format */
		return -1;

	return 0;
}

/*
 ***************************************************************************
 * Open a data file, and perform various checks before reading.
 *
 * IN:
 * @dfile	Name of system activity data file.
 * @act		Array of activities.
 * @ignore	Set to 1 if a true sysstat activity file but with a bad
 * 		format should not yield an error message. Useful with
 * 		sadf -H and sadf -c.
 *
 * OUT:
 * @ifd		System activity data file descriptor.
 * @file_magic	file_magic structure containing data read from file magic
 *		header.
 * @file_hdr	file_hdr structure containing data read from file standard
 * 		header.
 * @file_actlst	Acvtivity list in file.
 * @id_seq	Activity sequence.
 ***************************************************************************
 */
void check_file_actlst(int *ifd, char *dfile, struct activity *act[],
		       struct file_magic *file_magic, struct file_header *file_hdr,
		       struct file_activity **file_actlst, unsigned int id_seq[],
		       int ignore)
{
	int i, j, p;
	unsigned int a_cpu = FALSE;
	struct file_activity *fal;
	void *buffer = NULL;

	/* Open sa data file and read its magic structure */
	if (sa_open_read_magic(ifd, dfile, file_magic, ignore) < 0)
		return;

	SREALLOC(buffer, char, file_magic->header_size);

	/* Read sa data file standard header and allocate activity list */
	sa_fread(*ifd, buffer, file_magic->header_size, HARD_SIZE);
	/*
	 * Data file header size may be greater than FILE_HEADER_SIZE, but
	 * anyway only the first FILE_HEADER_SIZE bytes can be interpreted.
	 */
	memcpy(file_hdr, buffer, FILE_HEADER_SIZE);
	free(buffer);

	/*
	 * Sanity check.
	 * Compare against MAX_NR_ACT and not NR_ACT because
	 * we are maybe reading a datafile from a future sysstat version
	 * with more activities than known today.
	 */
	if (file_hdr->sa_act_nr > MAX_NR_ACT) {
		/* Maybe a "false positive" sysstat datafile? */
		handle_invalid_sa_file(ifd, file_magic, dfile, 0);
	}

	SREALLOC(*file_actlst, struct file_activity, FILE_ACTIVITY_SIZE * file_hdr->sa_act_nr);
	fal = *file_actlst;

	/* Read activity list */
	j = 0;
	for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

		sa_fread(*ifd, fal, FILE_ACTIVITY_SIZE, HARD_SIZE);

		/*
		 * Every activity, known or unknown, should have
		 * at least one item and sub-item.
		 * Also check that the number of items and sub-items
		 * doesn't exceed a max value. This is necessary
		 * because we will use @nr and @nr2 to
		 * allocate memory to read the file contents. So we
		 * must make sure the file is not corrupted.
		 * NB: Another check will be made below for known
		 * activities which have each a specific max value.
		 */
		if ((fal->nr < 1) || (fal->nr2 < 1) ||
		    (fal->nr > NR_MAX) || (fal->nr2 > NR2_MAX)) {
			handle_invalid_sa_file(ifd, file_magic, dfile, 0);
		}

		if ((p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND)) < 0)
			/* Unknown activity */
			continue;

		if (act[p]->magic != fal->magic) {
			/* Bad magical number */
			if (ignore) {
				/*
				 * This is how sadf -H knows that this
				 * activity has an unknown format.
				 */
				act[p]->magic = ACTIVITY_MAGIC_UNKNOWN;
			}
			else
				continue;
		}

		/* Check max value for known activities */
		if (fal->nr > act[p]->nr_max) {
			handle_invalid_sa_file(ifd, file_magic, dfile, 0);
		}

		if (fal->id == A_CPU) {
			a_cpu = TRUE;
		}

		if (fal->size > act[p]->msize) {
			act[p]->msize = fal->size;
		}
		/*
		 * NOTA BENE:
		 * If current activity is a volatile one then fal->nr is the
		 * number of items (CPU at the present time as only CPU related
		 * activities are volatile today) for the statistics located
		 * between the start of the data file and the first restart mark.
		 * Volatile activities have a number of items which can vary
		 * in file. In this case, a RESTART record is followed by the
		 * volatile activity structures.
		 */
		act[p]->nr    = fal->nr;
		act[p]->nr2   = fal->nr2;
		act[p]->fsize = fal->size;
		/*
		 * This is a known activity with a known format
		 * (magical number). Only such activities will be displayed.
		 * (Well, this may also be an unknown format if we have entered sadf -H.)
		 */
		id_seq[j++] = fal->id;
	}

	if (!a_cpu) {
		/*
		 * CPU activity should always be in file
		 * and have a known format (expected magical number).
		 */
		handle_invalid_sa_file(ifd, file_magic, dfile, 0);
	}

	while (j < NR_ACT) {
		id_seq[j++] = 0;
	}

	/* Check that at least one selected activity is available in file */
	for (i = 0; i < NR_ACT; i++) {

		if (!IS_SELECTED(act[i]->options))
			continue;

		/* Here is a selected activity: Does it exist in file? */
		fal = *file_actlst;
		for (j = 0; j < file_hdr->sa_act_nr; j++, fal++) {
			if (act[i]->id == fal->id)
				break;
		}
		if (j == file_hdr->sa_act_nr) {
			/* No: Unselect it */
			act[i]->options &= ~AO_SELECTED;
		}
	}
	if (!get_activity_nr(act, AO_SELECTED, COUNT_ACTIVITIES)) {
		fprintf(stderr, _("Requested activities not available in file %s\n"),
			dfile);
		close(*ifd);
		exit(1);
	}
}

/*
 ***************************************************************************
 * Set number of items for current volatile activity and reallocate its
 * structures accordingly.
 * NB: As only activities related to CPU can be volatile, the number of
 * items corresponds in fact to the number of CPU.
 *
 * IN:
 * @act		Array of activities.
 * @act_nr	Number of items for current volatile activity.
 * @act_id	Activity identification for current volatile activity.
 *
 * RETURN:
 * -1 if unknown activity and 0 otherwise.
 ***************************************************************************
 */
int reallocate_vol_act_structures(struct activity *act[], unsigned int act_nr,
	                           unsigned int act_id)
{
	int j, p;

	if ((p = get_activity_position(act, act_id, RESUME_IF_NOT_FOUND)) < 0)
		/* Ignore unknown activity */
		return -1;

	act[p]->nr = act_nr;

	for (j = 0; j < 3; j++) {
		SREALLOC(act[p]->buf[j], void,
			 (size_t) act[p]->msize * (size_t) act[p]->nr * (size_t) act[p]->nr2);
	}

	return 0;
}

/*
 ***************************************************************************
 * Read the volatile activities structures following a RESTART record.
 * Then set number of items for each corresponding activity and reallocate
 * structures.
 *
 * IN:
 * @ifd		Input file descriptor.
 * @act		Array of activities.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 * @vol_act_nr	Number of volatile activities structures to read.
 *
 * RETURNS:
 * New number of items.
 *
 * NB: As only activities related to CPU can be volatile, the new number of
 * items corresponds in fact to the new number of CPU.
 ***************************************************************************
 */
__nr_t read_vol_act_structures(int ifd, struct activity *act[], char *file,
			       struct file_magic *file_magic,
			       unsigned int vol_act_nr)
{
	struct file_activity file_act;
	int item_nr = 0;
	int i, rc;

	for (i = 0; i < vol_act_nr; i++) {

		sa_fread(ifd, &file_act, FILE_ACTIVITY_SIZE, HARD_SIZE);

		if (file_act.id) {
			rc = reallocate_vol_act_structures(act, file_act.nr, file_act.id);
			if ((rc == 0) && !item_nr) {
				item_nr = file_act.nr;
			}
		}
		/* else ignore empty structures that may exist */
	}

	if (!item_nr) {
		/* All volatile activities structures cannot be empty */
		handle_invalid_sa_file(&ifd, file_magic, file, 0);
	}

	return item_nr;
}

/*
 ***************************************************************************
 * Parse sar activities options (also used by sadf).
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 * @caller	Indicate whether it's sar or sadf that called this function.
 *
 * OUT:
 * @act		Array of selected activities.
 * @flags	Common flags and system state.
 *
 * RETURNS:
 * 0 on success.
 ***************************************************************************
 */
int parse_sar_opt(char *argv[], int *opt, struct activity *act[],
		  unsigned int *flags, int caller)
{
	int i, p;

	for (i = 1; *(argv[*opt] + i); i++) {
		/*
		 * Note: argv[*opt] contains something like "-BruW"
		 *     *(argv[*opt] + i) will contain 'B', 'r', etc.
		 */

		switch (*(argv[*opt] + i)) {

		case 'A':
			select_all_activities(act);

			/* Force '-P ALL -I XALL -r ALL -u ALL' */

			p = get_activity_position(act, A_MEMORY, EXIT_IF_NOT_FOUND);
			act[p]->opt_flags |= AO_F_MEM_AMT + AO_F_MEM_DIA +
					     AO_F_MEM_SWAP + AO_F_MEM_ALL;

			p = get_activity_position(act, A_IRQ, EXIT_IF_NOT_FOUND);
			set_bitmap(act[p]->bitmap->b_array, ~0,
				   BITMAP_SIZE(act[p]->bitmap->b_size));

			p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);
			set_bitmap(act[p]->bitmap->b_array, ~0,
				   BITMAP_SIZE(act[p]->bitmap->b_size));
			act[p]->opt_flags = AO_F_CPU_ALL;

			p = get_activity_position(act, A_FILESYSTEM, EXIT_IF_NOT_FOUND);
			act[p]->opt_flags = AO_F_FILESYSTEM;
			break;

		case 'B':
			SELECT_ACTIVITY(A_PAGE);
			break;

		case 'b':
			SELECT_ACTIVITY(A_IO);
			break;

		case 'C':
			*flags |= S_F_COMMENT;
			break;

		case 'd':
			SELECT_ACTIVITY(A_DISK);
			break;

		case 'F':
			p = get_activity_position(act, A_FILESYSTEM, EXIT_IF_NOT_FOUND);
			act[p]->options |= AO_SELECTED;
			if (!*(argv[*opt] + i + 1) && argv[*opt + 1] && !strcmp(argv[*opt + 1], K_MOUNT)) {
				(*opt)++;
				act[p]->opt_flags |= AO_F_MOUNT;
				return 0;
			}
			else {
				act[p]->opt_flags |= AO_F_FILESYSTEM;
			}
			break;

		case 'H':
			p = get_activity_position(act, A_HUGE, EXIT_IF_NOT_FOUND);
			act[p]->options   |= AO_SELECTED;
			break;

		case 'j':
			if (argv[*opt + 1]) {
				(*opt)++;
				if (strnlen(argv[*opt], MAX_FILE_LEN) >= MAX_FILE_LEN - 1)
					return 1;

				strncpy(persistent_name_type, argv[*opt], MAX_FILE_LEN - 1);
				persistent_name_type[MAX_FILE_LEN - 1] = '\0';
				strtolower(persistent_name_type);
				if (!get_persistent_type_dir(persistent_name_type)) {
					fprintf(stderr, _("Invalid type of persistent device name\n"));
					return 2;
				}
				/*
				 * If persistent device name doesn't exist for device, use
				 * its pretty name.
				 */
				*flags |= S_F_PERSIST_NAME + S_F_DEV_PRETTY;
				return 0;
			}
			else {
				return 1;
			}
			break;

		case 'p':
			*flags |= S_F_DEV_PRETTY;
			break;

		case 'q':
			SELECT_ACTIVITY(A_QUEUE);
			break;

		case 'r':
			p = get_activity_position(act, A_MEMORY, EXIT_IF_NOT_FOUND);
			act[p]->options   |= AO_SELECTED;
			act[p]->opt_flags |= AO_F_MEM_AMT;
			if (!*(argv[*opt] + i + 1) && argv[*opt + 1] && !strcmp(argv[*opt + 1], K_ALL)) {
				(*opt)++;
				act[p]->opt_flags |= AO_F_MEM_ALL;
				return 0;
			}
			break;

		case 'R':
			p = get_activity_position(act, A_MEMORY, EXIT_IF_NOT_FOUND);
			act[p]->options   |= AO_SELECTED;
			act[p]->opt_flags |= AO_F_MEM_DIA;
			break;

		case 'S':
			p = get_activity_position(act, A_MEMORY, EXIT_IF_NOT_FOUND);
			act[p]->options   |= AO_SELECTED;
			act[p]->opt_flags |= AO_F_MEM_SWAP;
			break;

		case 't':
			/*
			 * Check sar option -t here (as it can be combined
			 * with other ones, eg. "sar -rtu ..."
			 * But sadf option -t is checked in sadf.c as it won't
			 * be entered as a sar option after "--".
			 */
			if (caller == C_SAR) {
				*flags |= S_F_TRUE_TIME;
			}
			else
				return 1;
			break;

		case 'u':
			p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);
			act[p]->options |= AO_SELECTED;
			if (!*(argv[*opt] + i + 1) && argv[*opt + 1] && !strcmp(argv[*opt + 1], K_ALL)) {
				(*opt)++;
				act[p]->opt_flags = AO_F_CPU_ALL;
				return 0;
			}
			else {
				act[p]->opt_flags = AO_F_CPU_DEF;
			}
			break;

		case 'v':
			SELECT_ACTIVITY(A_KTABLES);
			break;

		case 'w':
			SELECT_ACTIVITY(A_PCSW);
			break;

		case 'W':
			SELECT_ACTIVITY(A_SWAP);
			break;

		case 'y':
			SELECT_ACTIVITY(A_SERIAL);
			break;

		case 'V':
			print_version();
			break;

		default:
			return 1;
		}
	}
	return 0;
}

/*
 ***************************************************************************
 * Parse sar "-m" option.
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 *
 * OUT:
 * @act		Array of selected activities.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_m_opt(char *argv[], int *opt, struct activity *act[])
{
	char *t;

	for (t = strtok(argv[*opt], ","); t; t = strtok(NULL, ",")) {
		if (!strcmp(t, K_CPU)) {
			SELECT_ACTIVITY(A_PWR_CPUFREQ);
		}
		else if (!strcmp(t, K_FAN)) {
			SELECT_ACTIVITY(A_PWR_FAN);
		}
		else if (!strcmp(t, K_IN)) {
			SELECT_ACTIVITY(A_PWR_IN);
		}
		else if (!strcmp(t, K_TEMP)) {
			SELECT_ACTIVITY(A_PWR_TEMP);
		}
		else if (!strcmp(t, K_FREQ)) {
			SELECT_ACTIVITY(A_PWR_WGHFREQ);
		}
		else if (!strcmp(t, K_USB)) {
			SELECT_ACTIVITY(A_PWR_USB);
		}
		else if (!strcmp(t, K_ALL)) {
			SELECT_ACTIVITY(A_PWR_CPUFREQ);
			SELECT_ACTIVITY(A_PWR_FAN);
			SELECT_ACTIVITY(A_PWR_IN);
			SELECT_ACTIVITY(A_PWR_TEMP);
			SELECT_ACTIVITY(A_PWR_WGHFREQ);
			SELECT_ACTIVITY(A_PWR_USB);
		}
		else
			return 1;
	}

	(*opt)++;
	return 0;
}

/*
 ***************************************************************************
 * Parse sar "-n" option.
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 *
 * OUT:
 * @act		Array of selected activities.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_n_opt(char *argv[], int *opt, struct activity *act[])
{
	char *t;

	for (t = strtok(argv[*opt], ","); t; t = strtok(NULL, ",")) {
		if (!strcmp(t, K_DEV)) {
			SELECT_ACTIVITY(A_NET_DEV);
		}
		else if (!strcmp(t, K_EDEV)) {
			SELECT_ACTIVITY(A_NET_EDEV);
		}
		else if (!strcmp(t, K_SOCK)) {
			SELECT_ACTIVITY(A_NET_SOCK);
		}
		else if (!strcmp(t, K_NFS)) {
			SELECT_ACTIVITY(A_NET_NFS);
		}
		else if (!strcmp(t, K_NFSD)) {
			SELECT_ACTIVITY(A_NET_NFSD);
		}
		else if (!strcmp(t, K_IP)) {
			SELECT_ACTIVITY(A_NET_IP);
		}
		else if (!strcmp(t, K_EIP)) {
			SELECT_ACTIVITY(A_NET_EIP);
		}
		else if (!strcmp(t, K_ICMP)) {
			SELECT_ACTIVITY(A_NET_ICMP);
		}
		else if (!strcmp(t, K_EICMP)) {
			SELECT_ACTIVITY(A_NET_EICMP);
		}
		else if (!strcmp(t, K_TCP)) {
			SELECT_ACTIVITY(A_NET_TCP);
		}
		else if (!strcmp(t, K_ETCP)) {
			SELECT_ACTIVITY(A_NET_ETCP);
		}
		else if (!strcmp(t, K_UDP)) {
			SELECT_ACTIVITY(A_NET_UDP);
		}
		else if (!strcmp(t, K_SOCK6)) {
			SELECT_ACTIVITY(A_NET_SOCK6);
		}
		else if (!strcmp(t, K_IP6)) {
			SELECT_ACTIVITY(A_NET_IP6);
		}
		else if (!strcmp(t, K_EIP6)) {
			SELECT_ACTIVITY(A_NET_EIP6);
		}
		else if (!strcmp(t, K_ICMP6)) {
			SELECT_ACTIVITY(A_NET_ICMP6);
		}
		else if (!strcmp(t, K_EICMP6)) {
			SELECT_ACTIVITY(A_NET_EICMP6);
		}
		else if (!strcmp(t, K_UDP6)) {
			SELECT_ACTIVITY(A_NET_UDP6);
		}
		else if (!strcmp(t, K_FC)) {
			SELECT_ACTIVITY(A_NET_FC);
		}
		else if (!strcmp(t, K_ALL)) {
			SELECT_ACTIVITY(A_NET_DEV);
			SELECT_ACTIVITY(A_NET_EDEV);
			SELECT_ACTIVITY(A_NET_SOCK);
			SELECT_ACTIVITY(A_NET_NFS);
			SELECT_ACTIVITY(A_NET_NFSD);
			SELECT_ACTIVITY(A_NET_IP);
			SELECT_ACTIVITY(A_NET_EIP);
			SELECT_ACTIVITY(A_NET_ICMP);
			SELECT_ACTIVITY(A_NET_EICMP);
			SELECT_ACTIVITY(A_NET_TCP);
			SELECT_ACTIVITY(A_NET_ETCP);
			SELECT_ACTIVITY(A_NET_UDP);
			SELECT_ACTIVITY(A_NET_SOCK6);
			SELECT_ACTIVITY(A_NET_IP6);
			SELECT_ACTIVITY(A_NET_EIP6);
			SELECT_ACTIVITY(A_NET_ICMP6);
			SELECT_ACTIVITY(A_NET_EICMP6);
			SELECT_ACTIVITY(A_NET_UDP6);
			SELECT_ACTIVITY(A_NET_FC);
		}
		else
			return 1;
	}

	(*opt)++;
	return 0;
}

/*
 ***************************************************************************
 * Parse sar "-I" option.
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 * @act		Array of activities.
 *
 * OUT:
 * @act		Array of activities, with interrupts activity selected.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sar_I_opt(char *argv[], int *opt, struct activity *act[])
{
	int i, p;
	unsigned char c;
	char *t;

	/* Select interrupt activity */
	p = get_activity_position(act, A_IRQ, EXIT_IF_NOT_FOUND);
	act[p]->options |= AO_SELECTED;

	for (t = strtok(argv[*opt], ","); t; t = strtok(NULL, ",")) {
		if (!strcmp(t, K_SUM)) {
			/* Select total number of interrupts */
			act[p]->bitmap->b_array[0] |= 0x01;
		}
		else if (!strcmp(t, K_ALL)) {
			/* Set bit for the first 16 individual interrupts */
			act[p]->bitmap->b_array[0] |= 0xfe;
			act[p]->bitmap->b_array[1] |= 0xff;
			act[p]->bitmap->b_array[2] |= 0x01;
		}
		else if (!strcmp(t, K_XALL)) {
			/* Set every bit except for total number of interrupts */
			c = act[p]->bitmap->b_array[0];
			set_bitmap(act[p]->bitmap->b_array, ~0,
				   BITMAP_SIZE(act[p]->bitmap->b_size));
			act[p]->bitmap->b_array[0] = 0xfe | c;
		}
		else {
			/* Get irq number */
			if (strspn(t, DIGITS) != strlen(t))
				return 1;
			i = atoi(t);
			if ((i < 0) || (i >= act[p]->bitmap->b_size))
				return 1;
			act[p]->bitmap->b_array[(i + 1) >> 3] |= 1 << ((i + 1) & 0x07);
		}
	}

	(*opt)++;
	return 0;
}

/*
 ***************************************************************************
 * Parse sar and sadf "-P" option.
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 * @act		Array of activities.
 *
 * OUT:
 * @flags	Common flags and system state.
 * @act		Array of activities, with CPUs selected.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_sa_P_opt(char *argv[], int *opt, unsigned int *flags, struct activity *act[])
{
	int i, p;
	char *t;

	p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);

	if (argv[++(*opt)]) {

		for (t = strtok(argv[*opt], ","); t; t = strtok(NULL, ",")) {
			if (!strcmp(t, K_ALL)) {
				/*
				 * Set bit for every processor.
				 * We still don't know if we are going to read stats
				 * from a file or not...
				 */
				set_bitmap(act[p]->bitmap->b_array, ~0,
					   BITMAP_SIZE(act[p]->bitmap->b_size));
			}
			else {
				/* Get cpu number */
				if (strspn(t, DIGITS) != strlen(t))
					return 1;
				i = atoi(t);
				if ((i < 0) || (i >= act[p]->bitmap->b_size))
					return 1;
				act[p]->bitmap->b_array[(i + 1) >> 3] |= 1 << ((i + 1) & 0x07);
			}
		}
		(*opt)++;
	}
	else
		return 1;

	return 0;
}

/*
 ***************************************************************************
 * Compute network interface utilization.
 *
 * IN:
 * @st_net_dev	Structure with network interface stats.
 * @rx		Number of bytes received per second.
 * @tx		Number of bytes transmitted per second.
 *
 * RETURNS:
 * NIC utilization (0-100%).
 ***************************************************************************
 */
double compute_ifutil(struct stats_net_dev *st_net_dev, double rx, double tx)
{
	unsigned long long speed;

	if (st_net_dev->speed) {

		speed = (unsigned long long) st_net_dev->speed * 1000000;

		if (st_net_dev->duplex == C_DUPLEX_FULL) {
			/* Full duplex */
			if (rx > tx) {
				return (rx * 800 / speed);
			}
			else {
				return (tx * 800 / speed);
			}
		}
		else {
			/* Half duplex */
			return ((rx + tx) * 800 / speed);
		}
	}

	return 0;
}

/*
 ***************************************************************************
 * Fill system activity file magic header.
 *
 * IN:
 * @file_magic	System activity file magic header.
 ***************************************************************************
 */
void enum_version_nr(struct file_magic *fm)
{
	char *v;
	char version[16];

	fm->sysstat_extraversion = 0;

	strcpy(version, VERSION);

	/* Get version number */
	if ((v = strtok(version, ".")) == NULL)
		return;
	fm->sysstat_version = atoi(v) & 0xff;

	/* Get patchlevel number */
	if ((v = strtok(NULL, ".")) == NULL)
		return;
	fm->sysstat_patchlevel = atoi(v) & 0xff;

	/* Get sublevel number */
	if ((v = strtok(NULL, ".")) == NULL)
		return;
	fm->sysstat_sublevel = atoi(v) & 0xff;

	/* Get extraversion number. Don't necessarily exist */
	if ((v = strtok(NULL, ".")) == NULL)
		return;
	fm->sysstat_extraversion = atoi(v) & 0xff;
}

/*
 ***************************************************************************
 * Read and replace unprintable characters in comment with ".".
 *
 * IN:
 * @ifd		Input file descriptor.
 * @comment	Comment.
 ***************************************************************************
 */
void replace_nonprintable_char(int ifd, char *comment)
{
	int i;

	/* Read comment */
	sa_fread(ifd, comment, MAX_COMMENT_LEN, HARD_SIZE);
	comment[MAX_COMMENT_LEN - 1] = '\0';

	/* Replace non printable chars */
	for (i = 0; i < strlen(comment); i++) {
		if (!isprint(comment[i]))
			comment[i] = '.';
	}
}

/*
 ***************************************************************************
 * Fill the rectime and loctime structures with current record's date and
 * time, based on current record's "number of seconds since the epoch" saved
 * in file.
 * For loctime (if given): The timestamp is expressed in local time.
 * For rectime: The timestamp is expressed in UTC, in local time, or in the
 * time of the file's creator depending on options entered by the user on the
 * command line.
 *
 * IN:
 * @l_flags	Flags indicating the type of time expected by the user.
 * 		S_F_LOCAL_TIME means time should be expressed in local time.
 * 		S_F_TRUE_TIME means time should be expressed in time of
 * 		file's creator.
 * 		Default is time expressed in UTC (except for sar, where it
 * 		is local time).
 * @record_hdr	Record header containing the number of seconds since the
 * 		epoch, and the HH:MM:SS of the file's creator.
 *
 * OUT:
 * @rectime	Structure where timestamp for current record has been saved
 * 		(in local time or in UTC depending on options used).
 * @loctime	If given, structure where timestamp for current record has
 * 		been saved (expressed in local time). This field will be used
 * 		for time comparison if options -s and/or -e have been used.
 *
 * RETURNS:
 * 1 if an error was detected, or 0 otherwise.
 ***************************************************************************
*/
int sa_get_record_timestamp_struct(unsigned int l_flags, struct record_header *record_hdr,
				   struct tm *rectime, struct tm *loctime)
{
	struct tm *ltm = NULL;
	int rc = 0;

	/* Fill localtime structure if given */
	if (loctime) {
		if ((ltm = localtime((const time_t *) &(record_hdr->ust_time))) != NULL) {
			*loctime = *ltm;
		}
		else {
			rc = 1;
		}
	}

	/* Fill generic rectime structure */
	if (PRINT_LOCAL_TIME(l_flags) && !ltm) {
		/* Get local time if not already done */
		ltm = localtime((const time_t *) &(record_hdr->ust_time));
	}

	if (!PRINT_LOCAL_TIME(l_flags) && !PRINT_TRUE_TIME(l_flags)) {
		/*
		 * Get time in UTC
		 * (the user doesn't want local time nor time of file's creator).
		 */
		ltm = gmtime((const time_t *) &(record_hdr->ust_time));
	}

	if (ltm) {
		/* Done even in true time mode so that we have some default values */
		*rectime = *ltm;
	}
	else {
		rc = 1;
	}

	if (PRINT_TRUE_TIME(l_flags)) {
		/* Time of file's creator */
		rectime->tm_hour = record_hdr->hour;
		rectime->tm_min  = record_hdr->minute;
		rectime->tm_sec  = record_hdr->second;
	}

	return rc;
}

/*
 ***************************************************************************
 * Set current record's timestamp strings (date and time) using the time
 * data saved in @rectime structure. The string may be the number of seconds
 * since the epoch if flag S_F_SEC_EPOCH has been set.
 *
 * IN:
 * @l_flags	Flags indicating the type of time expected by the user.
 * 		S_F_SEC_EPOCH means the time should be expressed in seconds
 * 		since the epoch (01/01/1970).
 * @record_hdr	Record header containing the number of seconds since the
 * 		epoch.
 * @cur_date	String where timestamp's date will be saved. May be NULL.
 * @cur_time	String where timestamp's time will be saved.
 * @len		Maximum length of timestamp strings.
 * @rectime	Structure with current timestamp (expressed in local time or
 *		in UTC depending on whether options -T or -t have been used
 * 		or not) that should be broken down in date and time strings.
 *
 * OUT:
 * @cur_date	Timestamp's date string (if expected).
 * @cur_time	Timestamp's time string. May contain the number of seconds
 *		since the epoch (01-01-1970) if corresponding option has
 * 		been used.
 ***************************************************************************
*/
void set_record_timestamp_string(unsigned int l_flags, struct record_header *record_hdr,
				 char *cur_date, char *cur_time, int len, struct tm *rectime)
{
	/* Set cur_time date value */
	if (PRINT_SEC_EPOCH(l_flags) && cur_date) {
		sprintf(cur_time, "%ld", record_hdr->ust_time);
		strcpy(cur_date, "");
	}
	else {
		/*
		 * If options -T or -t have been used then cur_time is
		 * expressed in local time. Else it is expressed in UTC.
		 */
		if (cur_date) {
			strftime(cur_date, len, "%Y-%m-%d", rectime);
		}
		if (USE_PREFD_TIME_OUTPUT(l_flags)) {
			strftime(cur_time, len, "%X", rectime);
		}
		else {
			strftime(cur_time, len, "%H:%M:%S", rectime);
		}
	}
}

/*
 ***************************************************************************
 * Print contents of a special (RESTART or COMMENT) record.
 *
 * IN:
 * @record_hdr	Current record header.
 * @l_flags	Flags for common options.
 * @tm_start	Structure filled when option -s has been used.
 * @tm_end	Structure filled when option -e has been used.
 * @rtype	Record type (R_RESTART or R_COMMENT).
 * @ifd		Input file descriptor.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have	been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record. May be NULL.
 * @file	Name of file being read.
 * @tab		Number of tabulations to print.
 * @file_magic	file_magic structure filled with file magic header data.
 * @file_hdr	System activity file standard header.
 * @act		Array of activities.
 * @ofmt		Pointer on report output format structure.
 *
 * OUT:
 * @rectime		Structure where timestamp (expressed in local time
 * 			or in UTC) has been saved.
 * @loctime		Structure where timestamp (expressed in local time)
 * 			has been saved (if requested).
 *
 * RETURNS:
 * 1 if the record has been successfully displayed, and 0 otherwise.
 ***************************************************************************
 */
int print_special_record(struct record_header *record_hdr, unsigned int l_flags,
			 struct tstamp *tm_start, struct tstamp *tm_end, int rtype, int ifd,
			 struct tm *rectime, struct tm *loctime, char *file, int tab,
			 struct file_magic *file_magic, struct file_header *file_hdr,
			 struct activity *act[], struct report_format *ofmt)
{
	char cur_date[TIMESTAMP_LEN], cur_time[TIMESTAMP_LEN];
	int dp = 1;
	unsigned int new_cpu_nr;

	/* Fill timestamp structure (rectime) for current record */
	if (sa_get_record_timestamp_struct(l_flags, record_hdr, rectime, loctime))
		return 0;

	/* If loctime is NULL, then use rectime for comparison */
	if (!loctime) {
		loctime = rectime;
	}

	/* The record must be in the interval specified by -s/-e options */
	if ((tm_start->use && (datecmp(loctime, tm_start) < 0)) ||
	    (tm_end->use && (datecmp(loctime, tm_end) > 0))) {
		/* Will not display the special record */
		dp = 0;
	}
	else {
		/* Set date and time strings to be displayed for current record */
		set_record_timestamp_string(l_flags, record_hdr,
					    cur_date, cur_time, TIMESTAMP_LEN, rectime);
	}

	if (rtype == R_RESTART) {
		/* Don't forget to read the volatile activities structures */
		new_cpu_nr = read_vol_act_structures(ifd, act, file, file_magic,
						     file_hdr->sa_vol_act_nr);

		if (!dp)
			return 0;

		if (*ofmt->f_restart) {
			(*ofmt->f_restart)(&tab, F_MAIN, cur_date, cur_time,
					   !PRINT_LOCAL_TIME(l_flags) &&
					   !PRINT_TRUE_TIME(l_flags), file_hdr,
					   new_cpu_nr);
		}
	}
	else if (rtype == R_COMMENT) {
		char file_comment[MAX_COMMENT_LEN];

		/* Read and replace non printable chars in comment */
		replace_nonprintable_char(ifd, file_comment);

		if (!dp || !DISPLAY_COMMENT(l_flags))
			return 0;

		if (*ofmt->f_comment) {
			(*ofmt->f_comment)(&tab, F_MAIN, cur_date, cur_time,
					   !PRINT_LOCAL_TIME(l_flags) &&
					   !PRINT_TRUE_TIME(l_flags), file_comment,
					   file_hdr);
		}
	}

	return 1;
}
