/*
 * sar, sadc, sadf, mpstat and iostat common routines.
 * (C) 1999-2025 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <limits.h>
#include <libgen.h>

#include "version.h"
#include "common.h"
#include "ioconf.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Number of decimal places */
extern int dplaces_nr;

/* Units (sectors, Bytes, kilobytes, etc.) */
char units[] = {'s', 'B', 'k', 'M', 'G', 'T', 'P', '?'};

/* Number of ticks per second */
unsigned long hz;
/* Number of bit shifts to convert pages to kB */
unsigned int kb_shift;

/* Colors strings */
char sc_percent_warn[MAX_SGR_LEN] = C_BOLD_MAGENTA;
char sc_percent_xtreme[MAX_SGR_LEN] = C_BOLD_RED;
char sc_zero_int_stat[MAX_SGR_LEN] = C_LIGHT_BLUE;
char sc_int_stat[MAX_SGR_LEN] = C_BOLD_BLUE;
char sc_item_name[MAX_SGR_LEN] = C_LIGHT_GREEN;
char sc_sa_restart[MAX_SGR_LEN] = C_LIGHT_RED;
char sc_sa_comment[MAX_SGR_LEN] = C_LIGHT_YELLOW;
char sc_trend_pos[MAX_SGR_LEN] = C_BOLD_GREEN;
char sc_trend_neg[MAX_SGR_LEN] = C_BOLD_RED;
char sc_normal[MAX_SGR_LEN] = C_NORMAL;

/*
 * Type of persistent device names in lowercase letters
 * (e.g. "uuid", "label", "path"...) Used in sar and iostat.
 */
char persistent_name_type[MAX_FILE_LEN];

/*
 ***************************************************************************
 * Print sysstat version number, environment variables and exit.
 *
 * IN:
 * @env		Array with environment variable names.
 * @n		Number of environment variables.
 ***************************************************************************
 */
void print_version(const char *env[], int n)
{
	char *e;
	int i;

	/* Display contents of environment variables */
	for (i = 0; i < n; i++) {
		if ((e = __getenv(env[i])) != NULL) {
			printf("%s=%s\n", env[i], e);
		}
	}

	/* Print sysstat version number */
	printf(_("sysstat version %s\n"), VERSION);
	printf("(C) Sebastien Godard (sysstat <at> orange.fr)\n");

	exit(0);
}

/*
 ***************************************************************************
 * Get date and time, expressed in UTC or in local time.
 *
 * IN:
 * @d_off	Day offset (number of days to go back in the past).
 * @utc		TRUE if date and time shall be expressed in UTC.
 *
 * OUT:
 * @rectime	Current local date and time.
 *
 * RETURNS:
 * Value of time in seconds since the Epoch (always in UTC) on success,
 * or (time_t) -1 on error.
 ***************************************************************************
 */
time_t get_xtime(struct tm *rectime, int d_off, int utc)
{
	time_t timer;

	if ((timer = __time(NULL)) == (time_t) -1)
		return (time_t) -1;
	timer -= SEC_PER_DAY * d_off;

	if (utc) {
		/* Get date and time in UTC */
		if (gmtime_r(&timer, rectime) == NULL)
			return (time_t) -1;
	}
	else {
		/* Get date and time in local time */
		if (localtime_r(&timer, rectime) == NULL)
			return (time_t) -1;
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

	if (!utc) {
		char *e;

		/* Read environment variable value once */
		if ((e = __getenv(ENV_TIME_DEFTM)) != NULL) {
			utc = !strcmp(e, K_UTC);
		}
		utc++;
	}

	return get_xtime(rectime, d_off, utc == 2);
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
 * Test whether given name is a device or a partition, using sysfs.
 *
 * IN:
 * @sysdev		sysfs location.
 * @name		Device or partition name.
 * @allow_virtual	TRUE if virtual devices are also accepted.
 *			The device is assumed to be virtual if no
 *			/sys/block/<device>/device link exists.
 *
 * RETURNS:
 * TRUE if @name is a device, and FALSE if it's a partition.
 ***************************************************************************
 */
int is_device(const char *sysdev, char *name, int allow_virtual)
{
	char syspath[PATH_MAX];
	char *slash;

	/* Some devices may have a slash in their name (eg. cciss/c0d0...) */
	while ((slash = strchr(name, '/'))) {
		*slash = '!';
	}
	snprintf(syspath, sizeof(syspath), "%s/%s/%s%s", sysdev, __BLOCK, name,
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
		exit(2);
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
		exit(2);
	}

	hz = (unsigned long) ticks;
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
 * Extract WWWN identifiers from filename, as read from /dev/disk/by-id.
 *
 * Sample valid names read from /dev/disk/by-id:
 * wwn-0x5000cca369f193ac
 * wwn-0x5000cca369f193ac-part12
 * wwn-0x600605b00a2bdf00242b28c10dcb1999
 * wwn-0x600605b00a2bdf00242b28c10dcb1999-part2
 *
 * WWN ids like these ones are ignored:
 * wwn-0x5438850077615869953x
 * wwn-0x5438850077615869953x-part1
 *
 * IN:
 * @name	Filename read from /dev/disk/by-id.
 *
 * OUT:
 * @wwn		WWN identifier (8 or 16 hex characters).
 * @part-nr	Partition number if applicable.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
*/
int extract_wwnid(const char *name, unsigned long long *wwn, unsigned int *part_nr)
{
	char id[17];
	char *s;
	int wwnlen = strlen(name);

	if ((name == NULL) || (wwn == NULL) || (part_nr == NULL))
		return -1;

	*wwn = *(wwn + 1) = 0ULL;
	*part_nr = 0;

	/* Check name */
	if ((wwnlen < WWN_PREFIX_LEN + WWN_SHORT_LEN) ||
	    (strncmp(name, WWN_PREFIX, WWN_PREFIX_LEN) != 0))
		return -1;

	/* Is there a partition number? */
	if ((s = strstr(name, PARTITION_SUFFIX)) != NULL) {
		/* Yes: Get partition number */
		if (sscanf(s + strlen(PARTITION_SUFFIX), "%u", part_nr) != 1)
			return -1;
		wwnlen = s - name - WWN_PREFIX_LEN;
	}
	else {
		wwnlen -= WWN_PREFIX_LEN;	/* Don't count "wwn-0x" */
	}

	/* Check WWN length */
	if ((wwnlen != WWN_SHORT_LEN) && (wwnlen != WWN_LONG_LEN))
		return -1;

	/* Extract first 16 hex chars of WWN */
	strncpy(id, name + WWN_PREFIX_LEN, sizeof(id) - 1);
	id[sizeof(id) - 1] = '\0';
	if (sscanf(id, "%llx", wwn) != 1)
		return -1;

	if (strlen(name) == WWN_SHORT_LEN)
		/* This is a short (16 hex chars) WWN id */
		return 0;

	/* Extract second part of WWN */
	if (sscanf(name + WWN_PREFIX_LEN + WWN_SHORT_LEN, "%llx", wwn + 1) != 1)
		return -1;

	return 0;
}

/*
 ***************************************************************************
 * Get WWWN identifiers from a pretty filename using links present in
 * /dev/disk/by-id directory.
 *
 * IN:
 * @pretty	Pretty name (e.g. sda, sdb3...).
 *
 * OUT:
 * @wwn		WWN identifier (8 or 16 hex characters).
 * @part-nr	Partition number if applicable.
 *
 * RETURNS:
 * 0 on success, -1 otherwise.
 ***************************************************************************
*/
int get_wwnid_from_pretty(char *pretty, unsigned long long *wwn, unsigned int *part_nr)
{
	DIR *dir;
	struct dirent *drd;
	ssize_t r;
	char link[PATH_MAX], target[PATH_MAX], wwn_name[FILENAME_MAX];
	char *name;
	int rc = -1;

	/* Open  /dev/disk/by-id directory */
	if ((dir = opendir(DEV_DISK_BY_ID)) == NULL)
		return -1;

	/* Get current id */
	while ((drd = readdir(dir)) != NULL) {

		if (strncmp(drd->d_name, WWN_PREFIX, WWN_PREFIX_LEN) != 0)
			continue;

		/* Get absolute path for current persistent name */
		snprintf(link, sizeof(link), "%s/%s", DEV_DISK_BY_ID, drd->d_name);

		/* Persistent name is usually a symlink: Read it... */
		r = readlink(link, target, sizeof(target));
		if ((r <= 0) || (r >= (ssize_t) sizeof(target)))
			continue;

		target[r] = '\0';

		/* ... and get device pretty name it points at */
		name = basename(target);
		if (!name || (*name == '\0'))
			continue;

		if (strncmp(name, pretty, FILENAME_MAX) == 0) {
			/* We have found pretty name for current persistent one */
			strncpy(wwn_name, drd->d_name, sizeof(wwn_name) - 1);
			wwn_name[sizeof(wwn_name) - 1] = '\0';

			/* Try to extract WWN */
			if (!extract_wwnid(wwn_name, wwn, part_nr)) {
				/* WWN successfully extracted */
				rc = 0;
				break;
			}
		}
	}

	/* Close directory */
	closedir(dir);

	return rc;
}

/*
 ***************************************************************************
 * Check if a directory exists.
 *
 * IN:
 * @dirname	Name of the directory.
 *
 * RETURNS:
 * TRUE if @dirname is actually an existing directory.
 ***************************************************************************
 */
int check_dir(char *dirname)
{
	struct stat sb;

	if (!stat(dirname, &sb) && S_ISDIR(sb.st_mode))
		return 1;

	return 0;
}

/*
 * **************************************************************************
 * Check if the multiplication of the 3 values may be greater than UINT_MAX.
 *
 * IN:
 * @val1	First value.
 * @val2	Second value.
 * @val3	Third value.
 *
 * RETURNS:
 * Multiplication of the 3 values.
 ***************************************************************************
 */
size_t mul_check_overflow3(size_t val1, size_t val2, size_t val3)
{
	if ((val1 != 0) && (val2 != 0) && (val3 != 0)) {
		if ((val1 > UINT_MAX) ||
		    (val2 > UINT_MAX / val1) ||
		    (val3 > UINT_MAX / (val1 * val2))) {
#ifdef DEBUG
			fprintf(stderr, "%s: Overflow detected (%zu,%zu,%zu). Aborting...\n",
				__FUNCTION__, val1, val2, val3);
#endif
			exit(4);
		}
	}

	return (val1 * val2 * val3);
}

/*
 * **************************************************************************
 * Check if the multiplication of the 4 values may be greater than UINT_MAX.
 *
 * IN:
 * @val1	First value.
 * @val2	Second value.
 * @val3	Third value.
 * @val4	Fourth value.
 *
 * RETURNS:
 * Multiplication of the 4 values.
 ***************************************************************************
 */
size_t mul_check_overflow4(size_t val1, size_t val2, size_t val3, size_t val4)
{
	if ((val1 != 0) && (val2 != 0) && (val3 != 0) && (val4 != 0)) {
		if ((val1 > UINT_MAX) ||
		    (val2 > UINT_MAX / val1) ||
		    (val3 > UINT_MAX / (val1 * val2)) ||
		    (val4 > UINT_MAX / (val1 * val2 * val3))) {
#ifdef DEBUG
			fprintf(stderr, "%s: Overflow detected (%zu,%zu,%zu,%zu). Aborting...\n",
				__FUNCTION__, val1, val2, val3, val4);
#endif
			exit(4);
		}
	}

	return (val1 * val2 * val3 * val4);
}

#ifndef SOURCE_SADC
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

		if (strstr(line, DEVICE_MAPPER) != NULL) {
			/* Read device-mapper major number */
			if (sscanf(line, "%u", &dm_major) == 1)
				break;
			dm_major = ~0U;	/* Reset to invalid value if sscanf fails */
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

	if (is_iso < 0) {
		char *e;

		is_iso = (((e = __getenv(ENV_TIME_FMT)) != NULL) && (strcmp(e, K_ISO) == 0));
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
 * Get report date as a string of characters.
 *
 * IN:
 * @tm_time	Date to display (don't use time fields).
 * @cur_date	String where date will be saved.
 * @sz		Max size of cur_date string.
 *
 * OUT:
 * @cur_date	Date (string format).
 *
 * RETURNS:
 * TRUE if S_TIME_FORMAT is set to ISO, or FALSE otherwise.
 * It returns -1 on error.
 ***************************************************************************
 */
int set_report_date(struct tm *tm_time, char cur_date[], int sz)
{
	if (tm_time != NULL) {
		const char *date_format = is_iso_time_fmt() ?
					  DATE_FORMAT_ISO :
					  DATE_FORMAT_LOCAL;

		if (strftime(cur_date, sz, date_format, tm_time) != 0)
			return is_iso_time_fmt();
	}

	strncpy(cur_date, DEFAULT_ERROR_DATE, sz - 1);
	cur_date[sz - 1] = '\0';

	return -1;
}

/*
 ***************************************************************************
 * Print banner.
 *
 * IN:
 * @tm_time	Date to display (don't use time fields).
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
 * It returns -1 on error.
 ***************************************************************************
 */
int print_gal_header(struct tm *tm_time, char *sysname, char *release,
		     char *nodename, char *machine, int cpu_nr, int format)
{
	char cur_date[TIMESTAMP_LEN];
	int rc = 0;

	rc = set_report_date(tm_time, cur_date, sizeof(cur_date));

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

/*
 ***************************************************************************
 * Get number of rows for current window.
 * If stdout is not a terminal then use the value given by environment
 * variable S_REPEAT_HEADER if existent.
 *
 * RETURNS:
 * Number of rows.
 ***************************************************************************
 */
int get_win_height(void)
{
	struct winsize win;
	char *e;
	/*
	 * This default value will be used whenever STDOUT
	 * is redirected to a pipe or a file and S_REPEAT_HEADER variable is not set
	 */
	int rows = DEFAULT_ROWS;

	/* Get number of lines of current terminal */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1) {
		if (win.ws_row > 2) {
			rows = win.ws_row - 2;
		}
	}
	/* STDOUT is not a terminal. Look for S_REPEAT_HEADER variable's value instead */
	else if ((e = __getenv(ENV_REPEAT_HEADER)) != NULL) {
		/* Check if the environment variable contains only digits */
		if (strspn(e, DIGITS) == strlen(e)) {
			int v = atol(e);
			if (v > 0) {
				rows = v;
			}
		}
	}

	/* Ensure rows is at least MIN_ROWS */
	return (rows < MIN_ROWS) ? MIN_ROWS : rows;
}

/*
 ***************************************************************************
 * Canonicalize and remove /dev from path name. If the device has a slash
 * character in its name, replace it with a bang character ('!'), e.g.:
 * cciss/c0d0 -> cciss!c0d0
 * cciss/c0d0p1 -> cciss!c0d0p1
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
	char *resolved_name = NULL, *slash;
	int i = 0;

	/* realpath() creates new string, so we need to free it later */
	resolved_name = __realpath(name, NULL);

	/* If path doesn't exist, just return input */
	if (!resolved_name) {
		return name;
	}

#ifdef DEBUG
	fprintf(stderr, "Real pathname: %s (%s)\n", resolved_name, name);
#endif

	if (strncmp(resolved_name, DEV_PREFIX, DEV_PREFIX_LEN) == 0) {
		i = DEV_PREFIX_LEN;
	}
	snprintf(out, sizeof(out), "%s", resolved_name + i);
	out[sizeof(out) - 1] = '\0';

	/* Some devices may have a slash in their name (eg. cciss/c0d0...) */
	while ((slash = strchr(out, '/'))) {
		*slash = '!';
	}

	free(resolved_name);
	return out;
}

/*
 ***************************************************************************
 * Escape a '\' character in a JSON string (replace '\' with "\\").
 *
 * IN:
 * @str		String which may contain '\' characters.
 *
 * RETURNS:
 * String where '\' characters have been escaped.
 ***************************************************************************
 */
char *escape_bs_char(const char str[])
{
	static char buffer[MAX_NAME_LEN];
	int i = 0, j = 0;

	if (str != NULL) {
		while (str[i] != '\0' && j < MAX_NAME_LEN - 1) {
			if (str[i] == '\\') {
				if (j < MAX_NAME_LEN - 2) {
					buffer[j++] = '\\';
					buffer[j++] = '\\';
				} else {
					break;
				}
			} else {
				buffer[j++] = str[i];
			}
			i++;
		}
	}

	buffer[j] = '\0';
	return buffer;
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
 * @prev_uptime	Previous uptime value (in jiffies or 1/100th of a second).
 * @curr_uptime	Current uptime value (in jiffies or 1/100th of a second).
 *
 * RETURNS:
 * Interval of time in jiffies or 1/100th of a second.
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
 * Path to the persistent type name directory, or NULL if access is denied
 * or strings have been truncated.
 ***************************************************************************
*/
char *get_persistent_type_dir(char *type)
{
	static char dir[PATH_MAX];
	int n;

	n = snprintf(dir, sizeof(dir), "%s-%s", DEV_DISK_BY, type);

	if ((n >= sizeof(dir)) || (n < 0) || (access(dir, R_OK) != 0)) {
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
 * Path to the persistent name, or NULL if file doesn't exist or strings
 * have been truncated.
 ***************************************************************************
*/
char *get_persistent_name_path(char *name)
{
	static char path[PATH_MAX];
	int n;

	n = snprintf(path, sizeof(path), "%s/%s",
		     get_persistent_type_dir(persistent_name_type), name);

	if ((n >= sizeof(path)) || (n < 0) || (access(path, F_OK) != 0)) {
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

	/* Empty directories contain only "." and ".." */
	if (n <= 2)
		/* Free list and return NULL */
		goto free_list;

	/* Ignore "." and ".." but keep place for one last NULL (n-2+1 = n-1) */
	files = (char **) calloc(n - 1, sizeof(char *));
	if (!files)
		goto free_list;

	/*
	 * i is for traversing namelist, k is for files.
	 * i != k because we are ignoring "." and ".." entries.
	 */
	for (i = 0; i < n; i++) {
		/* Ignore "." and ".." */
		if ((strcmp(namelist[i]->d_name, ".") == 0) ||
		    (strcmp(namelist[i]->d_name, "..") == 0))
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
		r = readlink(link, target, sizeof(target));
		if ((r <= 0) || (r >= sizeof(target)))
			continue;

		target[r] = '\0';

		/* ... and get device pretty name it points at */
		name = basename(target);
		if (!name || (name[0] == '\0'))
			continue;

		if (strncmp(name, pretty, FILENAME_MAX) == 0) {
			/* We have found pretty name for current persistent one */
			strncpy(persist_name, persist_names[i], sizeof(persist_name));
			persist_name[sizeof(persist_name) - 1] = '\0';
			break;
		}
	}

	i = -1;
	while (persist_names[++i]) {
		free (persist_names[i]);
	}
	free (persist_names);

	if (!strlen(persist_name))
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
	r = readlink(link, target, sizeof(target));
	if ((r <= 0) || (r >= sizeof(target)))
		return (NULL);

	target[r] = '\0';

	/* ... and get device pretty name it points at */
	pretty = basename(target);
	if (!pretty || (pretty[0] == '\0'))
		return (NULL);

	return pretty;
}

/*
 * **************************************************************************
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
	static char link[256], target[PATH_MAX];
	char *devname;
	ssize_t r;

	snprintf(link, sizeof(link), "%s/%u:%u", SYSFS_DEV_BLOCK, major, minor);

	/* Get full path to device knowing its major and minor numbers */
	r = readlink(link, target, sizeof(target));
	if (r <= 0 || r >= sizeof(target))
		return (NULL);

	target[r] = '\0';

	/* Get device name */
	devname = basename(target);
	if (!devname || strnlen(devname, FILENAME_MAX) == 0) {
		return (NULL);
	}

	return (devname);
}

/*
 * **************************************************************************
 * Get device real name if possible.
 *
 * IN:
 * @major	Major number of the device.
 * @minor	Minor number of the device.
 *
 * RETURNS:
 * The name of the device, which may be the real name (as it appears in /dev)
 * or a string with the following format devM-n.
 ***************************************************************************
 */
char *get_devname(unsigned int major, unsigned int minor)
{
	static char buf[32];
	char *name;

	name = get_devname_from_sysfs(major, minor);
	if (name != NULL)
		return (name);

	name = ioc_name(major, minor);
	if ((name != NULL) && (strcmp(name, K_NODEV) != 0))
		return (name);

	snprintf(buf, sizeof(buf), DEF_DEVICE_NAME, major, minor);
	return (buf);
}

/*
 * **************************************************************************
 * Get device name (whether pretty-printed, persistent or not).
 *
 * IN:
 * @major		Major number of the device.
 * @minor		Minor number of the device.
 * @wwn			WWN identifier of the device (0 if unknown).
 * @part_nr		Partition number (0 if unknown).
 * @disp_devmap_name	Display device mapper name.
 * @disp_persist_name	Display persistent name of the device.
 * @use_stable_id	Display stable-across-reboots name.
 * @dflt_name		Device name to use by default (if existent).
 *
 * RETURNS:
 * The name of the device.
 ***************************************************************************
 */
char *get_device_name(unsigned int major, unsigned int minor, unsigned long long wwn[],
		      unsigned int part_nr, unsigned int disp_devmap_name,
		      unsigned int disp_persist_name, unsigned int use_stable_id,
		      char *dflt_name)
{
	static unsigned int dm_major = 0;
	char *dev_name = NULL, *persist_dev_name = NULL, *bang;
	static char sid[64], dname[MAX_NAME_LEN];

	if (disp_persist_name) {
		persist_dev_name = get_persistent_name_from_pretty(get_devname(major, minor));
	}

	if (persist_dev_name) {
		dev_name = persist_dev_name;
	}
	else {
		if (use_stable_id && (wwn[0] != 0)) {
			char xsid[32] = "", pn[16] = "";

			if (wwn[1] != 0) {
				sprintf(xsid, "%016llx", wwn[1]);
			}
			if (part_nr) {
				sprintf(pn, "-%u", part_nr);
			}
			snprintf(sid, sizeof(sid), "%#016llx%s%s", wwn[0], xsid, pn);
			dev_name = sid;
		}
		else if (disp_devmap_name) {
			if (!dm_major) {
				dm_major = get_devmap_major();
			}
			if (major == dm_major) {
				dev_name = transform_devmapname(major, minor);
			}
		}

		if (!dev_name) {
			if (dflt_name) {
				dev_name = dflt_name;
			}
			else {
				dev_name = get_devname(major, minor);
			}
		}
	}

	strncpy(dname, dev_name, sizeof(dname) - 1);
	dname[sizeof(dname) - 1] = '\0';

	while ((bang = strchr(dname, '!'))) {
		/*
		 * Some devices may have had a slash replaced with
		 * a bang character (eg. cciss!c0d0...)
		 * Restore their original names.
		 */
		*bang = '/';
	}

	return dname;
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
	if ((e = __getenv(ENV_COLORS)) == NULL
	     ? !isatty(STDOUT_FILENO)
	     : (!strcmp(e, C_NEVER) ||
		(strcmp(e, C_ALWAYS) && !isatty(STDOUT_FILENO)))) {
		/*
		 * Environment variable not set and stdout is not a terminal,
		 * or set to "never",
		 * or set to "auto" and stdout is not a terminal:
		 * Unset color strings.
		 */
		strcpy(sc_percent_warn, "");
		strcpy(sc_percent_xtreme, "");
		strcpy(sc_zero_int_stat, "");
		strcpy(sc_int_stat, "");
		strcpy(sc_item_name, "");
		strcpy(sc_sa_comment, "");
		strcpy(sc_sa_restart, "");
		strcpy(sc_trend_pos, "");
		strcpy(sc_trend_neg, "");
		strcpy(sc_normal, "");

		return;
	}

	/* Read S_COLORS_SGR environment variable */
	if ((e = __getenv(ENV_COLORS_SGR)) == NULL)
		/* Environment variable not set */
		return;

	for (p = strtok(e, ":"); p; p =strtok(NULL, ":")) {

		len = strlen(p);
		if ((len > 7) || (len < 3) || (*(p + 1) != '=') ||
		    (strspn(p + 2, ";0123456789") != (len - 2)))
			/* Ignore malformed codes */
			continue;

		switch (*p) {
			case 'M':
			case 'W':
				snprintf(sc_percent_warn, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case 'X':
			case 'H':
				snprintf(sc_percent_xtreme, MAX_SGR_LEN, "\e[%sm", p + 2);
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
			case '+':
				snprintf(sc_trend_pos, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
			case '-':
				snprintf(sc_trend_neg, MAX_SGR_LEN, "\e[%sm", p + 2);
				break;
		}
	}
}

/*
 ***************************************************************************
 * Print a value in human readable format. Such a value is a decimal number
 * followed by a unit (B, k, M, etc.)
 *
 * IN:
 * @unit	Default value unit.
 * @dval	Value to print.
 * @wi		Output width.
 ***************************************************************************
*/
void cprintf_unit(int unit, int wi, double dval)
{
	if (wi < 4) {
		/* E.g. 1.3M */
		wi = 4;
	}
	if (!unit) {
		/* Value is a number of sectors. Convert it to kB */
		dval /= 2;
		unit = 2;
	}
	while (dval >= 1024) {
		dval /= 1024;
		unit++;
	}
	printf(" %*.*f", wi - 1, dplaces_nr ? 1 : 0, dval);
	printf("%s", sc_normal);

	/* Display unit */
	if (unit >= NR_UNITS) {
		unit = NR_UNITS - 1;
	}
	printf("%c", units[unit]);
}

/*
 ***************************************************************************
 * Print 64 bit unsigned values using colors, possibly followed by a unit.
 *
 * IN:
 * @unit	Default values unit. -1 if no unit should be displayed.
 * @num		Number of values to print.
 * @wi		Output width.
 ***************************************************************************
 */
void cprintf_u64(int unit, int num, int wi, ...)
{
	int i;
	uint64_t val;
	va_list args;

	va_start(args, wi);

	for (i = 0; i < num; i++) {
		val = va_arg(args, unsigned long long);
		if (!val) {
			printf("%s", sc_zero_int_stat);
		}
		else {
			printf("%s", sc_int_stat);
		}
		if (unit < 0) {
			printf(" %*"PRIu64, wi, val);
			printf("%s", sc_normal);
		}
		else {
			cprintf_unit(unit, wi, (double) val);
		}
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print hex values using colors.
 *
 * IN:
 * @num		Number of values to print.
 * @wi		Output width.
 ***************************************************************************
*/
void cprintf_x(int num, int wi, ...)
{
	int i;
	unsigned int val;
	va_list args;

	va_start(args, wi);

	for (i = 0; i < num; i++) {
		val = va_arg(args, unsigned int);
		printf("%s", sc_int_stat);
		printf(" %*x", wi, val);
		printf("%s", sc_normal);
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print "double" statistics values using colors, possibly followed by a
 * unit.
 *
 * IN:
 * @unit	Default values unit. -1 if no unit should be displayed.
 * @sign	TRUE if sign (+/-) should be explicitly displayed.
 * @num		Number of values to print.
 * @wi		Output width.
 * @wd		Number of decimal places.
 ***************************************************************************
*/
void cprintf_f(int unit, int sign, int num, int wi, int wd, ...)
{
	int i;
	double val, lim = 0.005;;
	va_list args;

	/*
	 * If there are decimal places then get the value
	 * entered on the command line (if existing).
	 */
	if ((wd > 0) && (dplaces_nr >= 0)) {
		wd = dplaces_nr;
	}

	/* Update limit value according to number of decimal places */
	if (wd == 1) {
		lim = 0.05;
	}

	va_start(args, wd);

	for (i = 0; i < num; i++) {
		val = va_arg(args, double);
		if (((wd > 0) && (val < lim) && (val > (lim * -1))) ||
		    ((wd == 0) && (val <= 0.5) && (val >= -0.5))) {	/* "Round half to even" law */
			printf("%s", sc_zero_int_stat);
		}
		else if (sign && (val <= -10.0)) {
			printf("%s", sc_percent_xtreme);
		}
		else if (sign && (val <= -5.0)) {
			printf("%s", sc_percent_warn);
		}
		else {
			printf("%s", sc_int_stat);
		}

		if (unit < 0) {
			if (sign) {
				printf(" %+*.*f", wi, wd, val);
			}
			else {
				printf(" %*.*f", wi, wd, val);
			}
			printf("%s", sc_normal);
		}
		else {
			cprintf_unit(unit, wi, val);
		}
	}

	va_end(args);
}

/*
 ***************************************************************************
 * Print "percent" statistics values using colors.
 *
 * IN:
 * @human	Set to > 0 if a percent sign (%) shall be displayed after
 *		the value.
 * @xtrem	Set to non 0 to indicate that extreme (low or high) values
 *		should be displayed in specific color if beyond predefined
 *		limits.
 * @num		Number of values to print.
 * @wi		Output width.
 * @wd		Number of decimal places.
 ***************************************************************************
*/
void cprintf_xpc(int human, int xtrem, int num, int wi, int wd, ...)
{
	int i;
	double val, lim = 0.005;
	va_list args;

	/*
	 * If there are decimal places then get the value
	 * entered on the command line (if existing).
	 */
	if ((wd > 0) && (dplaces_nr >= 0)) {
		wd = dplaces_nr;
	}

	/*
	 * If a percent sign is to be displayed, then there will be
	 * zero (or one) decimal place.
	 */
	if (human > 0) {
		if (wi < 4) {
			/* E.g., 100% */
			wi = 4;
		}
		/* Keep one place for the percent sign */
		wi -= 1;
		if (wd > 1) {
			wd -= 1;
		}
	}

	/* Update limit value according to number of decimal places */
	if (wd == 1) {
		lim = 0.05;
	}

	va_start(args, wd);

	for (i = 0; i < num; i++) {
		val = va_arg(args, double);
		if ((xtrem == XHIGH) && (val >= PERCENT_LIMIT_XHIGH)) {
			printf("%s", sc_percent_xtreme);
		}
		else if ((xtrem == XHIGH) && (val >= PERCENT_LIMIT_HIGH)) {
			printf("%s", sc_percent_warn);
		}
		else if ((xtrem == XLOW) && (val <= PERCENT_LIMIT_XLOW)) {
			printf("%s", sc_percent_xtreme);
		}
		else if ((xtrem == XLOW0) && (val <= PERCENT_LIMIT_XLOW) && (val >= lim)) {
			printf("%s", sc_percent_xtreme);
		}
		else if ((xtrem == XLOW) && (val <= PERCENT_LIMIT_LOW)) {
			printf("%s", sc_percent_warn);
		}
		else if ((xtrem == XLOW0) && (val <= PERCENT_LIMIT_LOW) && (val >= lim)) {
			printf("%s", sc_percent_warn);
		}
		else if (((wd > 0) && (val < lim)) ||
			 ((wd == 0) && (val <= 0.5))) {	/* "Round half to even" law */
			printf("%s", sc_zero_int_stat);
		}
		else {
			printf("%s", sc_int_stat);
		}
		printf(" %*.*f", wi, wd, val);
		printf("%s", sc_normal);
		if (human > 0) printf("%%");
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
	/* IS_RESTART and IS_DEBUG are the same value */
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

/*
 * **************************************************************************
 * Print trend string using selected color.
 *
 * IN:
 * @trend	Trend (TRUE: positive; FALSE: negative).
 * @format	Output format.
 * @tstring	String to display.
 ***************************************************************************
 */
void cprintf_tr(int trend, char *format, char *tstring)
{
	if (trend) {
		printf("%s", sc_trend_pos);
	}
	else {
		printf("%s", sc_trend_neg);
	}

	printf(format, tstring);

	printf("%s", sc_normal);
}

/*
 ***************************************************************************
 * Parse a string containing a numerical value (e.g. CPU or IRQ number).
 * The string should contain only one value, not a range of values.
 *
 * IN:
 * @s		String to parse.
 * @max_val	Upper limit that value should not reach.
 *
 * OUT:
 * @val		Value, or -1 if the string @s was empty.
 *
 * RETURNS:
 * 0 if the value has been properly read, 1 otherwise.
 ***************************************************************************
 */
int parse_valstr(const char *s, int max_val, int *val)
{
	if ((s == NULL) || (*s == '\0')) {
		*val = -1;
		return 0;
	}
	/* Check that string contains only digits */
	if (strspn(s, DIGITS) != strlen(s))
		return 1;

	*val = atoi(s);
	if ((*val < 0) || (*val >= max_val))
		return 1;

	return 0;
}

/*
 ***************************************************************************
 * Parse string containing a single value or a range of values
 * (e.g. "0,2-5,10-").
 *
 * IN:
 * @t		String to parse.
 * @max_val	Upper limit that value should not reach.
 *
 * OUT:
 * @val_low	Low value in range
 * @val		High value in range. @val_low and @val are the same if it's
 *		a single value.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_range_values(char *t, int max_val, int *val_low, int *val)
{
	char *s, *valstr, range[16];

	if ((t == NULL) || (*t == '\0'))
		return 1;

	/* Parse value or range of values */
	strncpy(range, t, sizeof(range) - 1);
	range[sizeof(range) - 1] = '\0';
	valstr = t;

	if ((s = strchr(range, '-')) != NULL) {
		/* Possible range of values */
		*s = '\0';
		if (parse_valstr(range, max_val, val_low) || (*val_low < 0))
			return 1;
		valstr = s + 1;
	}
	if (parse_valstr(valstr, max_val, val))
		return 1;
	if (s && *val < 0) {
		/* Range of values with no upper limit (e.g. "3-") */
		*val = max_val - 1;
	}
	if ((!s && (*val < 0)) || (s && (*val < *val_low)))
		/*
		 * Individual value: string cannot be empty.
		 * Range of values: n-m: m can be empty (e.g. "3-") but
		 * cannot be lower than n.
		 */
		return 1;
	if (!s) {
		*val_low = *val;
	}

	return 0;
}

/*
 ***************************************************************************
 * Parse string containing a set of coma-separated values or ranges of
 * values (e.g. "0,2-5,10-"). The ALL keyword is allowed and indicate that
 * all possible values are selected.
 *
 * IN:
 * @strargv	Current argument in list to parse.
 * @bitmap	Bitmap whose contents will indicate which values have been
 *		selected.
 * @max_val	Upper limit that value should not reach.
 * @__K_VALUE0	Keyword corresponding to the first bit in bitmap (e.g "all",
 *		"SUM"...)
 *
 * OUT:
 * @bitmap	Bitmap updated with selected values.
 *
 * RETURNS:
 * 0 on success, 1 otherwise.
 ***************************************************************************
 */
int parse_values(char *strargv, unsigned char bitmap[], int max_val, const char *__K_VALUE0)
{
	int i, val_low, val;
	char *t;

	if (strcmp(strargv, K_ALL) == 0) {
		/* Set bit for every possible values (CPU, IRQ, etc.) */
		memset(bitmap, ~0, BITMAP_SIZE(max_val));
		return 0;
	}

	for (t = strtok(strargv, ","); t; t = strtok(NULL, ",")) {
		if (strcmp(t, __K_VALUE0) == 0) {
			/*
			 * Set bit 0 in bitmap. This may correspond
			 * to CPU "all" or IRQ "SUM" for example.
			 */
			bitmap[0] |= 1;
		}
		else {
			/* Parse value or range of values */
			if (parse_range_values(t, max_val, &val_low, &val))
				return 1;

			for (i = val_low; i <= val; i++) {
				SET_CPU_BITMAP(bitmap, i + 1);
			}
		}
	}

	return 0;
}

/*
 * **************************************************************************
 * Write current sample's timestamp, either in plain or JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @rectime	Current date and time.
 * @xflags	Flag for common options and system state.
 ***************************************************************************
 */
void write_sample_timestamp(int tab, struct tm *rectime, uint64_t xflags)
{
	char timestamp[TIMESTAMP_LEN];

	if (rectime == NULL)
		return;

	if (DISPLAY_SEC_EPOCH(xflags)) {
		snprintf(timestamp, sizeof(timestamp), "%ld", mktime(rectime));
		timestamp[sizeof(timestamp) - 1] = '\0';
	}
	else if (DISPLAY_ISO(xflags)) {
		strftime(timestamp, sizeof(timestamp), DATE_TIME_FORMAT_ISO, rectime);
	}
	else {
		strftime(timestamp, sizeof(timestamp), DATE_TIME_FORMAT_LOCAL, rectime);
	}
	if (DISPLAY_JSON_OUTPUT(xflags)) {
		xprintf(tab, "\"timestamp\": \"%s\",", timestamp);
	}
	else {
		printf("%s\n", timestamp);
	}

#ifdef DEBUG
	if (DISPLAY_DEBUG(xflags)) {
		fprintf(stderr, "%s\n", timestamp);
	}
#endif
}

#endif /* SOURCE_SADC undefined */
