/*
 * sar, sadc, sadf, mpstat and iostat common routines.
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
#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>	/* For STDOUT_FILENO, among others */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <libgen.h>

#include "version.h"
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

/* Number of ticks per second */
unsigned int hz;
/* Number of bit shifts to convert pages to kB */
unsigned int kb_shift;

/* Colors strings */
char sc_percent_high[MAX_SGR_LEN] = C_BOLD_RED;
char sc_percent_low[MAX_SGR_LEN] = C_BOLD_BLUE;
char sc_zero_int_stat[MAX_SGR_LEN] = C_LIGHT_YELLOW;
char sc_int_stat[MAX_SGR_LEN] = C_BOLD_YELLOW;
char sc_item_name[MAX_SGR_LEN] = C_LIGHT_GREEN;
char sc_sa_restart[MAX_SGR_LEN] = C_LIGHT_RED;
char sc_sa_comment[MAX_SGR_LEN] = C_LIGHT_CYAN;
char sc_normal[MAX_SGR_LEN] = C_NORMAL;

/* Type of persistent device names used in sar and iostat */
char persistent_name_type[MAX_FILE_LEN];

/*
 ***************************************************************************
 * Print sysstat version number and exit.
 ***************************************************************************
 */
void print_version(void)
{
	printf(_("sysstat version %s\n"), VERSION);
	printf("(C) Sebastien Godard (sysstat <at> orange.fr)\n");
	exit(0);
}

/*
 ***************************************************************************
 * Get local date and time.
 *
 * IN:
 * @d_off	Day offset (number of days to go back in the past).
 *
 * OUT:
 * @rectime	Current local date and time.
 *
 * RETURNS:
 * Value of time in seconds since the Epoch.
 ***************************************************************************
 */
time_t get_localtime(struct tm *rectime, int d_off)
{
	time_t timer;
	struct tm *ltm;

	time(&timer);
	timer -= SEC_PER_DAY * d_off;
	ltm = localtime(&timer);

	if (ltm) {
		*rectime = *ltm;
	}
	return timer;
}

/*
 ***************************************************************************
 * Get date and time expressed in UTC.
 *
 * IN:
 * @d_off	Day offset (number of days to go back in the past).
 *
 * OUT:
 * @rectime	Current date and time expressed in UTC.
 *
 * RETURNS:
 * Value of time in seconds since the Epoch.
 ***************************************************************************
 */
time_t get_gmtime(struct tm *rectime, int d_off)
{
	time_t timer;
	struct tm *ltm;

	time(&timer);
	timer -= SEC_PER_DAY * d_off;
	ltm = gmtime(&timer);

	if (ltm) {
		*rectime = *ltm;
	}
	return timer;
}

/*
 ***************************************************************************
 * Get date and time and take into account <ENV_TIME_DEFTM> variable.
 *
 * IN:
 * @d_off	Day offset (number of days to go back in the past).
 *
 * OUT:
 * @rectime	Current date and time.
 *
 * RETURNS:
 * Value of time in seconds since the Epoch.
 ***************************************************************************
 */
time_t get_time(struct tm *rectime, int d_off)
{
	static int utc = 0;
	char *e;

	if (!utc) {
		/* Read environment variable value once */
		if ((e = getenv(ENV_TIME_DEFTM)) != NULL) {
			utc = !strcmp(e, K_UTC);
		}
		utc++;
	}

	if (utc == 2)
		return get_gmtime(rectime, d_off);
	else
		return get_localtime(rectime, d_off);
}

/*
 ***************************************************************************
 * Count number of comma-separated values in arguments list. For example,
 * the number will be 3 for the list "foobar -p 1 -p 2,3,4 2 5".
 *
 * IN:
 * @arg_c	Number of arguments in the list.
 * @arg_v	Arguments list.
 *
 * RETURNS:
 * Number of comma-separated values in the list.
 ***************************************************************************
 */
int count_csvalues(int arg_c, char **arg_v)
{
	int opt = 1;
	int nr = 0;
	char *t;

	while (opt < arg_c) {
		if (strchr(arg_v[opt], ',')) {
			for (t = arg_v[opt]; t; t = strchr(t + 1, ',')) {
				nr++;
			}
		}
		opt++;
	}

	return nr;
}

/*
 ***************************************************************************
 * Look for partitions of a given block device in /sys filesystem.
 *
 * IN:
 * @dev_name	Name of the block device.
 *
 * RETURNS:
 * Number of partitions for the given block device.
 ***************************************************************************
 */
int get_dev_part_nr(char *dev_name)
{
	DIR *dir;
	struct dirent *drd;
	char dfile[MAX_PF_NAME], line[MAX_PF_NAME];
	int part = 0;

	snprintf(dfile, MAX_PF_NAME, "%s/%s", SYSFS_BLOCK, dev_name);
	dfile[MAX_PF_NAME - 1] = '\0';

	/* Open current device directory in /sys/block */
	if ((dir = opendir(dfile)) == NULL)
		return 0;

	/* Get current file entry */
	while ((drd = readdir(dir)) != NULL) {
		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(line, MAX_PF_NAME, "%s/%s/%s", dfile, drd->d_name, S_STAT);
		line[MAX_PF_NAME - 1] = '\0';

		/* Try to guess if current entry is a directory containing a stat file */
		if (!access(line, R_OK)) {
			/* Yep... */
			part++;
		}
	}

	/* Close directory */
	closedir(dir);

	return part;
}

/*
 ***************************************************************************
 * Look for block devices present in /sys/ filesystem:
 * Check first that sysfs is mounted (done by trying to open /sys/block
 * directory), then find number of devices registered.
 *
 * IN:
 * @display_partitions	Set to TRUE if partitions must also be counted.
 *
 * RETURNS:
 * Total number of block devices (and partitions if @display_partitions was
 * set).
 ***************************************************************************
 */
int get_sysfs_dev_nr(int display_partitions)
{
	DIR *dir;
	struct dirent *drd;
	char line[MAX_PF_NAME];
	int dev = 0;

	/* Open /sys/block directory */
	if ((dir = opendir(SYSFS_BLOCK)) == NULL)
		/* sysfs not mounted, or perhaps this is an old kernel */
		return 0;

	/* Get current file entry in /sys/block directory */
	while ((drd = readdir(dir)) != NULL) {
		if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
			continue;
		snprintf(line, MAX_PF_NAME, "%s/%s/%s", SYSFS_BLOCK, drd->d_name, S_STAT);
		line[MAX_PF_NAME - 1] = '\0';

		/* Try to guess if current entry is a directory containing a stat file */
		if (!access(line, R_OK)) {
			/* Yep... */
			dev++;

			if (display_partitions) {
				/* We also want the number of partitions for this device */
				dev += get_dev_part_nr(drd->d_name);
			}
		}
	}

	/* Close /sys/block directory */
	closedir(dir);

	return dev;
}

/*
 ***************************************************************************
 * Read /proc/devices file and get device-mapper major number.
 * If device-mapper entry is not found in file, assume it's not active.
 *
 * RETURNS:
 * Device-mapper major number.
 ***************************************************************************
 */
unsigned int get_devmap_major(void)
{
	FILE *fp;
	char line[128];
	/*
	 * Linux uses 12 bits for the major number,
	 * so this shouldn't match any real device.
	 */
	unsigned int dm_major = ~0U;

	if ((fp = fopen(DEVICES, "r")) == NULL)
		return dm_major;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (strstr(line, "device-mapper")) {
			/* Read device-mapper major number */
			sscanf(line, "%u", &dm_major);
		}
	}

	fclose(fp);

	return dm_major;
}

/*
 ***************************************************************************
 * Returns whether S_TIME_FORMAT is set to ISO.
 *
 * RETURNS:
 * TRUE if S_TIME_FORMAT is set to ISO, or FALSE otherwise.
 ***************************************************************************
 */
int is_iso_time_fmt(void)
{
	static int is_iso = -1;
	char *e;

	if (is_iso < 0) {
		is_iso = (((e = getenv(ENV_TIME_FMT)) != NULL) && !strcmp(e, K_ISO));
	}
	return is_iso;
}

/*
 ***************************************************************************
 * Print tabulations
 *
 * IN:
 * @nr_tab	Number of tabs to print.
 ***************************************************************************
 */
void prtab(int nr_tab)
{
	int i;

	for (i = 0; i < nr_tab; i++) {
		printf("\t");
	}
}

/*
 ***************************************************************************
 * printf() function modified for XML-like output. Don't print a CR at the
 * end of the line.
 *
 * IN:
 * @nr_tab	Number of tabs to print.
 * @fmtf	printf() format.
 ***************************************************************************
 */
void xprintf0(int nr_tab, const char *fmtf, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, fmtf);
	vsnprintf(buf, sizeof(buf), fmtf, args);
	va_end(args);

	prtab(nr_tab);
	printf("%s", buf);
}

/*
 ***************************************************************************
 * printf() function modified for XML-like output. Print a CR at the end of
 * the line.
 *
 * IN:
 * @nr_tab	Number of tabs to print.
 * @fmtf	printf() format.
 ***************************************************************************
 */
void xprintf(int nr_tab, const char *fmtf, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, fmtf);
	vsnprintf(buf, sizeof(buf), fmtf, args);
	va_end(args);

	prtab(nr_tab);
	printf("%s\n", buf);
}

/*
 ***************************************************************************
 * Print banner.
 *
 * IN:
 * @rectime	Date to display (don't use time fields).
 * @sysname	System name to display.
 * @release	System release number to display.
 * @nodename	Hostname to display.
 * @machine	Machine architecture to display.
 * @cpu_nr	Number of CPU.
 * @format	Set to FALSE for (default) plain output, and to TRUE for
 * 		JSON format output.
 *
 * RETURNS:
 * TRUE if S_TIME_FORMAT is set to ISO, or FALSE otherwise.
 ***************************************************************************
 */
int print_gal_header(struct tm *rectime, char *sysname, char *release,
		     char *nodename, char *machine, int cpu_nr, int format)
{
	char cur_date[64];
	int rc = 0;

	if (rectime == NULL) {
		strcpy(cur_date, "?/?/?");
	}
	else if (is_iso_time_fmt()) {
		strftime(cur_date, sizeof(cur_date), "%Y-%m-%d", rectime);
		rc = 1;
	}
	else {
		strftime(cur_date, sizeof(cur_date), "%x", rectime);
	}

	if (format == PLAIN_OUTPUT) {
		/* Plain output */
		printf("%s %s (%s) \t%s \t_%s_\t(%d CPU)\n", sysname, release, nodename,
		       cur_date, machine, cpu_nr);
	}
	else {
		/* JSON output */
		xprintf(0, "{\"sysstat\": {");
		xprintf(1, "\"hosts\": [");
		xprintf(2, "{");
		xprintf(3, "\"nodename\": \"%s\",", nodename);
		xprintf(3, "\"sysname\": \"%s\",", sysname);
		xprintf(3, "\"release\": \"%s\",", release);
		xprintf(3, "\"machine\": \"%s\",", machine);
		xprintf(3, "\"number-of-cpus\": %d,", cpu_nr);
		xprintf(3, "\"date\": \"%s\",", cur_date);
		xprintf(3, "\"statistics\": [");
	}

	return rc;
}

#ifdef USE_NLS
/*
 ***************************************************************************
 * Init National Language Support.
 ***************************************************************************
 */
void init_nls(void)
{
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	setlocale(LC_TIME, "");
	setlocale(LC_NUMERIC, "");

	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
}
#endif

/*
 ***************************************************************************
 * Get number of rows for current window.
 *
 * RETURNS:
 * Number of rows.
 ***************************************************************************
 */
int get_win_height(void)
{
	struct winsize win;
	/*
	 * This default value will be used whenever STDOUT
	 * is redirected to a pipe or a file
	 */
	int rows = 3600 * 24;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1) {
		if (win.ws_row > 2) {
			rows = win.ws_row - 2;
		}
	}
	return rows;
}

/*
 ***************************************************************************
 * Canonicalize and remove /dev from path name.
 *
 * IN:
 * @name	Device name (may begin with "/dev/" or can be a symlink).
 *
 * RETURNS:
 * Device basename.
 ***************************************************************************
 */
char *device_name(char *name)
{
	static char out[MAX_FILE_LEN];
	char *resolved_name;
	int i = 0;

	/* realpath() creates new string, so we need to free it later */
	resolved_name = realpath(name, NULL);

	/* If path doesn't exist, just return input */
	if (!resolved_name) {
		return name;
	}

	if (!strncmp(resolved_name, "/dev/", 5)) {
		i = 5;
	}
	strncpy(out, resolved_name + i, MAX_FILE_LEN);
	out[MAX_FILE_LEN - 1] = '\0';

	free(resolved_name);

	return out;
}

/*
 ***************************************************************************
 * Test whether given name is a device or a partition, using sysfs.
 * This is more straightforward that using ioc_iswhole() function from
 * ioconf.c which should be used only with kernels that don't have sysfs.
 *
 * IN:
 * @name		Device or partition name.
 * @allow_virtual	TRUE if virtual devices are also accepted.
 *			The device is assumed to be virtual if no
 *			/sys/block/<device>/device link exists.
 *
 * RETURNS:
 * TRUE if @name is not a partition.
 ***************************************************************************
 */
int is_device(char *name, int allow_virtual)
{
	char syspath[PATH_MAX];
	char *slash;

	/* Some devices may have a slash in their name (eg. cciss/c0d0...) */
	while ((slash = strchr(name, '/'))) {
		*slash = '!';
	}
	snprintf(syspath, sizeof(syspath), "%s/%s%s", SYSFS_BLOCK, name,
		 allow_virtual ? "" : "/device");

	return !(access(syspath, F_OK));
}

/*
 ***************************************************************************
 * Get page shift in kB.
 ***************************************************************************
 */
void get_kb_shift(void)
{
	int shift = 0;
	long size;

	/* One can also use getpagesize() to get the size of a page */
	if ((size = sysconf(_SC_PAGESIZE)) == -1) {
		perror("sysconf");
	}

	size >>= 10;	/* Assume that a page has a minimum size of 1 kB */

	while (size > 1) {
		shift++;
		size >>= 1;
	}

	kb_shift = (unsigned int) shift;
}

/*
 ***************************************************************************
 * Get number of clock ticks per second.
 ***************************************************************************
 */
void get_HZ(void)
{
	long ticks;

	if ((ticks = sysconf(_SC_CLK_TCK)) == -1) {
		perror("sysconf");
	}

	hz = (unsigned int) ticks;
}

/*
 ***************************************************************************
 * Workaround for CPU counters read from /proc/stat: Dyn-tick kernels
 * have a race issue that can make those counters go backward.
 ***************************************************************************
 */
double ll_sp_value(unsigned long long value1, unsigned long long value2,
		   unsigned long long itv)
{
	if (value2 < value1)
		return (double) 0;
	else
		return SP_VALUE(value1, value2, itv);
}

/*
 ***************************************************************************
 * Compute time interval.
 *
 * IN:
 * @prev_uptime	Previous uptime value in jiffies.
 * @curr_uptime	Current uptime value in jiffies.
 *
 * RETURNS:
 * Interval of time in jiffies.
 ***************************************************************************
 */
unsigned long long get_interval(unsigned long long prev_uptime,
				unsigned long long curr_uptime)
{
	unsigned long long itv;

	/* prev_time=0 when displaying stats since system startup */
	itv = curr_uptime - prev_uptime;

	if (!itv) {	/* Paranoia checking */
		itv = 1;
	}

	return itv;
}

/*
 ***************************************************************************
 * Since ticks may vary slightly from CPU to CPU, we'll want
 * to recalculate itv based on this CPU's tick count, rather
 * than that reported by the "cpu" line. Otherwise we
 * occasionally end up with slightly skewed figures, with
 * the skew being greater as the time interval grows shorter.
 *
 * IN:
 * @scc	Current sample statistics for current CPU.
 * @scp	Previous sample statistics for current CPU.
 *
 * RETURNS:
 * Interval of time based on current CPU.
 ***************************************************************************
 */
unsigned long long get_per_cpu_interval(struct stats_cpu *scc,
					struct stats_cpu *scp)
{
	unsigned long long ishift = 0LL;

	if ((scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest)) {
		/*
		 * Sometimes the nr of jiffies spent in guest mode given by the guest
		 * counter in /proc/stat is slightly higher than that included in
		 * the user counter. Update the interval value accordingly.
		 */
		ishift += (scp->cpu_user - scp->cpu_guest) -
		          (scc->cpu_user - scc->cpu_guest);
	}
	if ((scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice)) {
		/*
		 * Idem for nr of jiffies spent in guest_nice mode.
		 */
		ishift += (scp->cpu_nice - scp->cpu_guest_nice) -
		          (scc->cpu_nice - scc->cpu_guest_nice);
	}

	/*
	 * Don't take cpu_guest and cpu_guest_nice into account
	 * because cpu_user and cpu_nice already include them.
	 */
	return ((scc->cpu_user    + scc->cpu_nice   +
		 scc->cpu_sys     + scc->cpu_iowait +
		 scc->cpu_idle    + scc->cpu_steal  +
		 scc->cpu_hardirq + scc->cpu_softirq) -
		(scp->cpu_user    + scp->cpu_nice   +
		 scp->cpu_sys     + scp->cpu_iowait +
		 scp->cpu_idle    + scp->cpu_steal  +
		 scp->cpu_hardirq + scp->cpu_softirq) +
		 ishift);
}

/*
 ***************************************************************************
 * Unhandled situation: Panic and exit. Should never happen.
 *
 * IN:
 * @function	Function name where situation occured.
 * @error_code	Error code.
 ***************************************************************************
 */
void sysstat_panic(const char *function, int error_code)
{
	fprintf(stderr, "sysstat: %s[%d]: Internal error...\n",
		function, error_code);
	exit(1);
}

/*
 ***************************************************************************
 * Count number of bits set in an array.
 *
 * IN:
 * @ptr		Pointer to array.
 * @size	Size of array in bytes.
 *
 * RETURNS:
 * Number of bits set in the array.
 ***************************************************************************
*/
int count_bits(void *ptr, int size)
{
	int nr = 0, i, k;
	char *p;

	p = ptr;
	for (i = 0; i < size; i++, p++) {
		k = 0x80;
		while (k) {
			if (*p & k)
				nr++;
			k >>= 1;
		}
	}

	return nr;
}

/*
 ***************************************************************************
 * Compute "extended" device statistics (service time, etc.).
 *
 * IN:
 * @sdc		Structure with current device statistics.
 * @sdp		Structure with previous device statistics.
 * @itv		Interval of time in jiffies.
 *
 * OUT:
 * @xds		Structure with extended statistics.
 ***************************************************************************
*/
void compute_ext_disk_stats(struct stats_disk *sdc, struct stats_disk *sdp,
			    unsigned long long itv, struct ext_disk_stats *xds)
{
	double tput
		= ((double) (sdc->nr_ios - sdp->nr_ios)) * HZ / itv;

	xds->util  = S_VALUE(sdp->tot_ticks, sdc->tot_ticks, itv);
	xds->svctm = tput ? xds->util / tput : 0.0;
	/*
	 * Kernel gives ticks already in milliseconds for all platforms
	 * => no need for further scaling.
	 */
	xds->await = (sdc->nr_ios - sdp->nr_ios) ?
		((sdc->rd_ticks - sdp->rd_ticks) + (sdc->wr_ticks - sdp->wr_ticks)) /
		((double) (sdc->nr_ios - sdp->nr_ios)) : 0.0;
	xds->arqsz = (sdc->nr_ios - sdp->nr_ios) ?
		((sdc->rd_sect - sdp->rd_sect) + (sdc->wr_sect - sdp->wr_sect)) /
		((double) (sdc->nr_ios - sdp->nr_ios)) : 0.0;
}

/*
 ***************************************************************************
 * Convert in-place input string to lowercase.
 *
 * IN:
 * @str		String to be converted.
 *
 * OUT:
 * @str		String in lowercase.
 *
 * RETURNS:
 * String in lowercase.
 ***************************************************************************
*/
char *strtolower(char *str)
{
	char *cp = str;

	while (*cp) {
		*cp = tolower(*cp);
		cp++;
	}

	return(str);
}

/*
 ***************************************************************************
 * Get persistent type name directory from type.
 *
 * IN:
 * @type	Persistent type name (UUID, LABEL, etc.)
 *
 * RETURNS:
 * Path to the persistent type name directory, or NULL if access is denied.
 ***************************************************************************
*/
char *get_persistent_type_dir(char *type)
{
	static char dir[32];

	snprintf(dir, 32, "%s-%s", DEV_DISK_BY, type);

	if (access(dir, R_OK)) {
		return (NULL);
	}

	return (dir);
}

/*
 ***************************************************************************
 * Get persistent name absolute path.
 *
 * IN:
 * @name	Persistent name.
 *
 * RETURNS:
 * Path to the persistent name, or NULL if file doesn't exist.
 ***************************************************************************
*/
char *get_persistent_name_path(char *name)
{
	static char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s/%s",
		 get_persistent_type_dir(persistent_name_type), name);

	if (access(path, F_OK)) {
		return (NULL);
	}

	return (path);
}

/*
 ***************************************************************************
 * Get files from persistent type name directory.
 *
 * RETURNS:
 * List of files in the persistent type name directory in alphabetical order.
 ***************************************************************************
*/
char **get_persistent_names(void)
{
	int n, i, k = 0;
	char *dir;
	char **files = NULL;
	struct dirent **namelist;

	/* Get directory name for selected persistent type */
	dir = get_persistent_type_dir(persistent_name_type);
	if (!dir)
		return (NULL);

	n = scandir(dir, &namelist, NULL, alphasort);
	if (n < 0)
		return (NULL);

	/* If directory is empty, it contains 2 entries: "." and ".." */
	if (n <= 2)
		/* Free list and return NULL */
		goto free_list;

	/* Ignore the "." and "..", but keep place for one last NULL. */
	files = (char **) calloc(n - 1, sizeof(char *));
	if (!files)
		goto free_list;

	/*
	 * i is for traversing namelist, k is for files.
	 * i != k because we are ignoring "." and ".." entries.
	 */
	for (i = 0; i < n; i++) {
		/* Ignore "." and "..". */
		if (!strcmp(".", namelist[i]->d_name) ||
		    !strcmp("..", namelist[i]->d_name))
			continue;

		files[k] = (char *) calloc(strlen(namelist[i]->d_name) + 1, sizeof(char));
		if (!files[k])
			continue;

		strcpy(files[k++], namelist[i]->d_name);
	}
	files[k] = NULL;

free_list:

	for (i = 0; i < n; i++) {
		free(namelist[i]);
	}
	free(namelist);

	return (files);
}

/*
 ***************************************************************************
 * Get persistent name from pretty name.
 *
 * IN:
 * @pretty	Pretty name (e.g. sda, sda1, ..).
 *
 * RETURNS:
 * Persistent name.
 ***************************************************************************
*/
char *get_persistent_name_from_pretty(char *pretty)
{
	int i = -1;
	ssize_t r;
	char *link, *name;
	char **persist_names;
	char target[PATH_MAX];
	static char persist_name[FILENAME_MAX];

	persist_name[0] = '\0';

	/* Get list of files from persistent type name directory */
	persist_names = get_persistent_names();
	if (!persist_names)
		return (NULL);

	while (persist_names[++i]) {
		/* Get absolute path for current persistent name */
		link = get_persistent_name_path(persist_names[i]);
		if (!link)
			continue;

		/* Persistent name is usually a symlink: Read it... */
		r = readlink(link, target, PATH_MAX);
		if ((r <= 0) || (r >= PATH_MAX))
			continue;

		target[r] = '\0';

		/* ... and get device pretty name it points at */
		name = basename(target);
		if (!name || (name[0] == '\0'))
			continue;

		if (!strncmp(name, pretty, FILENAME_MAX)) {
			/* We have found pretty name for current persistent one */
			strncpy(persist_name, persist_names[i], FILENAME_MAX);
			persist_name[FILENAME_MAX - 1] = '\0';
			break;
		}
	}

	i = -1;
	while (persist_names[++i]) {
		free (persist_names[i]);
	}
	free (persist_names);

	if (strlen(persist_name) <= 0)
		return (NULL);

	return persist_name;
}

/*
 ***************************************************************************
 * Get pretty name (sda, sda1...) from persistent name.
 *
 * IN:
 * @persistent	Persistent name.
 *
 * RETURNS:
 * Pretty name.
 ***************************************************************************
*/
char *get_pretty_name_from_persistent(char *persistent)
{
	ssize_t r;
	char *link, *pretty, target[PATH_MAX];

	/* Get absolute path for persistent name */
	link = get_persistent_name_path(persistent);
	if (!link)
		return (NULL);

	/* Persistent name is usually a symlink: Read it... */
	r = readlink(link, target, PATH_MAX);
	if ((r <= 0) || (r >= PATH_MAX))
		return (NULL);

	target[r] = '\0';

	/* ... and get device pretty name it points at */
	pretty = basename(target);
	if (!pretty || (pretty[0] == '\0'))
		return (NULL);

	return pretty;
}

/*
 ***************************************************************************
 * Init color strings.
 ***************************************************************************
 */
void init_colors(void)
{
	char *e, *p;
	int len;

	/* Read S_COLORS environment variable */
	if (((e = getenv(ENV_COLORS)) == NULL) ||
	    !strcmp(e, C_NEVER) ||
	    (strcmp(e, C_ALWAYS) && !isatty(STDOUT_FILENO))) {
		/*
		 * Environment variable not set, or set to "never",
		 * or set to "auto" and stdout is not a terminal:
		 * Unset color strings.
		 */
		strcpy(sc_percent_high, "");
		strcpy(sc_percent_low, "");
		strcpy(sc_zero_int_stat, "");
		strcpy(sc_int_stat, "");
		strcpy(sc_item_name, "");
		strcpy(sc_sa_comment, "");
		strcpy(sc_sa_restart, "");
		strcpy(sc_normal, "");

		return;
	}

	/* Read S_COLORS_SGR environment variable */
	if ((e = getenv(ENV_COLORS_SGR)) == NULL)
		/* Environment variable not set */
		return;

	for (p = strtok(e, ":"); p; p =strtok(NULL, ":")) {

		len = strlen(p);
		if ((len > 7) || (len < 3) || (*(p + 1) != '=') ||
		    (strspn(p + 2, ";0123456789") != (len - 2)))
			/* Ignore malformed codes */
			continue;

		switch (*p) {
			case 'H':
				snprintf(sc_percent_high, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'M':
				snprintf(sc_percent_low, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'Z':
				snprintf(sc_zero_int_stat, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'N':
				snprintf(sc_int_stat, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'I':
				snprintf(sc_item_name, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'C':
				snprintf(sc_sa_comment, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'R':
				snprintf(sc_sa_restart, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
		}
	}
}

/*
 ***************************************************************************
 * Print 64 bit unsigned values using colors.
 *
 * IN:
 * @num		Number of values to print.
 * @width	Output width.
 ***************************************************************************
*/
void cprintf_u64(int num, int width, ...)
{
	int i;
	uint64_t val;
	va_list args;

	va_start(args, width);

	for (i = 0; i < num; i++) {
		val = va_arg(args, unsigned long long);
		if (!val) {
			printf("%s", sc_zero_int_stat);
		}
		else {
			printf("%s", sc_int_stat);
		}
		printf(" %*"PRIu64, width, val);
		printf("%s", sc_normal);
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print hex values using colors.
 *
 * IN:
 * @num		Number of values to print.
 * @width	Output width.
 ***************************************************************************
*/
void cprintf_x(int num, int width, ...)
{
	int i;
	unsigned int val;
	va_list args;

	va_start(args, width);

	for (i = 0; i < num; i++) {
		val = va_arg(args, unsigned int);
		printf("%s", sc_int_stat);
		printf(" %*x", width, val);
		printf("%s", sc_normal);
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print "double" statistics values using colors.
 *
 * IN:
 * @num		Number of values to print.
 * @width	Output width.
 * @wd		Number of decimal places.
 ***************************************************************************
*/
void cprintf_f(int num, int wi, int wd, ...)
{
	int i;
	double val;
	va_list args;

	va_start(args, wd);

	for (i = 0; i < num; i++) {
		val = va_arg(args, double);
		if (((val < 0.005) && (val > -0.005)) ||
		    ((wd == 0) && (val < 0.5))) {
			printf("%s", sc_zero_int_stat);
		}
		else {
			printf("%s", sc_int_stat);
		}
		printf(" %*.*f", wi, wd, val);
		printf("%s", sc_normal);
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print "percent" statistics values using colors.
 *
 * IN:
 * @num		Number of values to print.
 * @width	Output width.
 * @wd		Number of decimal places.
 ***************************************************************************
*/
void cprintf_pc(int num, int wi, int wd, ...)
{
	int i;
	double val;
	va_list args;

	va_start(args, wd);

	for (i = 0; i < num; i++) {
		val = va_arg(args, double);
		if (val >= PERCENT_LIMIT_HIGH) {
			printf("%s", sc_percent_high);
		}
		else if (val >= PERCENT_LIMIT_LOW) {
			printf("%s", sc_percent_low);
		}
		else if (val < 0.005) {
			printf("%s", sc_zero_int_stat);
		}
		else {
			printf("%s", sc_int_stat);
		}
		printf(" %*.*f", wi, wd, val);
		printf("%s", sc_normal);
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print item name using selected color.
 * Only one name can be displayed. Name can be an integer or a string.
 *
 * IN:
 * @type	0 if name is an int, 1 if name is a string
 * @format	Output format.
 * @item_string	Item name (given as a string of characters).
 * @item_int	Item name (given as an integer value).
 ***************************************************************************
*/
void cprintf_in(int type, char *format, char *item_string, int item_int)
{
	printf("%s", sc_item_name);
	if (type) {
		printf(format, item_string);
	}
	else {
		printf(format, item_int);
	}
	printf("%s", sc_normal);
}

/*
 ***************************************************************************
 * Print a string using selected color.
 *
 * IN:
 * @type	Type of string to display.
 * @format	Output format.
 * @string	String to display.
 ***************************************************************************
*/
void cprintf_s(int type, char *format, char *string)
{
	if (type == IS_STR) {
		printf("%s", sc_int_stat);
	}
	else if (type == IS_ZERO) {
		printf("%s", sc_zero_int_stat);
	}
	else if (type == IS_RESTART) {
		printf("%s", sc_sa_restart);
	}
	else {
		/* IS_COMMENT */
		printf("%s", sc_sa_comment);
	}
	printf(format, string);
	printf("%s", sc_normal);
}
