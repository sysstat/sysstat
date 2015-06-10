/*
 * sar: report system activity
 * (C) 1999-2015 by Sebastien GODARD (sysstat <at> orange.fr)
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
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                   *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "version.h"
#include "sa.h"
#include "common.h"
#include "ioconf.h"
#include "pr_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }

/* Interval and count parameters */
long interval = -1, count = 0;

/* TRUE if a header line must be printed */
int dis = TRUE;

unsigned int flags = 0;
unsigned int dm_major;	/* Device-mapper major number */

char timestamp[2][TIMESTAMP_LEN];

unsigned long avg_count = 0;

/* File header */
struct file_header file_hdr;

/* Current record header */
struct record_header record_hdr[3];

/*
 * Activity sequence.
 * This array must always be entirely filled (even with trailing zeros).
 */
unsigned int id_seq[NR_ACT];

struct tm rectime;

/* Contain the date specified by -s and -e options */
struct tstamp tm_start, tm_end;

char *args[MAX_ARGV_NR];

extern struct activity *act[];

struct sigaction int_act;
int sigint_caught = 0;

/*
 ***************************************************************************
 * Print usage title message.
 *
 * IN:
 * @progname	Name of sysstat command
 ***************************************************************************
 */
void print_usage_title(FILE *fp, char *progname)
{
	fprintf(fp, _("Usage: %s [ options ] [ <interval> [ <count> ] ]\n"),
		progname);
}

/*
 ***************************************************************************
 * Print usage and exit.
 *
 * IN:
 * @progname	Name of sysstat command
 ***************************************************************************
 */
void usage(char *progname)
{
	print_usage_title(stderr, progname);
	fprintf(stderr, _("Options are:\n"
			  "[ -A ] [ -B ] [ -b ] [ -C ] [ -D ] [ -d ] [ -F [ MOUNTS ] ] [ -H ] [ -h ]\n"
			  "[ -p ] [ -q ] [ -R ] [ -r [ ALL ] ] [ -S ] [ -t ] [ -u [ ALL ] ] [ -V ]\n"
			  "[ -v ] [ -W ] [ -w ] [ -y ] [ --sadc ]\n"
			  "[ -I { <int> [,...] | SUM | ALL | XALL } ] [ -P { <cpu> [,...] | ALL } ]\n"
			  "[ -m { <keyword> [,...] | ALL } ] [ -n { <keyword> [,...] | ALL } ]\n"
			  "[ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ -f [ <filename> ] | -o [ <filename> ] | -[0-9]+ ]\n"
			  "[ -i <interval> ] [ -s [ <hh:mm[:ss]> ] ] [ -e [ <hh:mm[:ss]> ] ]\n"));
	exit(1);
}

/*
 ***************************************************************************
 * Display a short help message and exit.
 *
 * IN:
 * @progname	Name of sysstat command
 ***************************************************************************
 */
void display_help(char *progname)
{
	print_usage_title(stdout, progname);
	printf(_("Main options and reports:\n"));
	printf(_("\t-B\tPaging statistics\n"));
	printf(_("\t-b\tI/O and transfer rate statistics\n"));
	printf(_("\t-d\tBlock devices statistics\n"));
	printf(_("\t-F [ MOUNTS ]\n"));
	printf(_("\t\tFilesystems statistics\n"));
	printf(_("\t-H\tHugepages utilization statistics\n"));
	printf(_("\t-I { <int> | SUM | ALL | XALL }\n"
		 "\t\tInterrupts statistics\n"));
	printf(_("\t-m { <keyword> [,...] | ALL }\n"
		 "\t\tPower management statistics\n"
		 "\t\tKeywords are:\n"
		 "\t\tCPU\tCPU instantaneous clock frequency\n"
		 "\t\tFAN\tFans speed\n"
		 "\t\tFREQ\tCPU average clock frequency\n"
		 "\t\tIN\tVoltage inputs\n"
		 "\t\tTEMP\tDevices temperature\n"
		 "\t\tUSB\tUSB devices plugged into the system\n"));
	printf(_("\t-n { <keyword> [,...] | ALL }\n"
		 "\t\tNetwork statistics\n"
		 "\t\tKeywords are:\n"
		 "\t\tDEV\tNetwork interfaces\n"
		 "\t\tEDEV\tNetwork interfaces (errors)\n"
		 "\t\tNFS\tNFS client\n"
		 "\t\tNFSD\tNFS server\n"
		 "\t\tSOCK\tSockets\t(v4)\n"
		 "\t\tIP\tIP traffic\t(v4)\n"
		 "\t\tEIP\tIP traffic\t(v4) (errors)\n"
		 "\t\tICMP\tICMP traffic\t(v4)\n"
		 "\t\tEICMP\tICMP traffic\t(v4) (errors)\n"
		 "\t\tTCP\tTCP traffic\t(v4)\n"
		 "\t\tETCP\tTCP traffic\t(v4) (errors)\n"
		 "\t\tUDP\tUDP traffic\t(v4)\n"
		 "\t\tSOCK6\tSockets\t(v6)\n"
		 "\t\tIP6\tIP traffic\t(v6)\n"
		 "\t\tEIP6\tIP traffic\t(v6) (errors)\n"
		 "\t\tICMP6\tICMP traffic\t(v6)\n"
		 "\t\tEICMP6\tICMP traffic\t(v6) (errors)\n"
		 "\t\tUDP6\tUDP traffic\t(v6)\n"
		 "\t\tFC\tFibre channel HBAs\n"));
	printf(_("\t-q\tQueue length and load average statistics\n"));
	printf(_("\t-R\tMemory statistics\n"));
	printf(_("\t-r [ ALL ]\n"
		 "\t\tMemory utilization statistics\n"));
	printf(_("\t-S\tSwap space utilization statistics\n"));
	printf(_("\t-u [ ALL ]\n"
		 "\t\tCPU utilization statistics\n"));
	printf(_("\t-v\tKernel tables statistics\n"));
	printf(_("\t-W\tSwapping statistics\n"));
	printf(_("\t-w\tTask creation and system switching statistics\n"));
	printf(_("\t-y\tTTY devices statistics\n"));
	exit(0);
}

/*
 ***************************************************************************
 * Give a hint to the user about where is located the data collector.
 ***************************************************************************
 */
void which_sadc(void)
{
	struct stat buf;

	if (stat(SADC_PATH, &buf) < 0) {
		printf(_("Data collector will be sought in PATH\n"));
	}
	else {
		printf(_("Data collector found: %s\n"), SADC_PATH);
	}
	exit(0);
}


/*
 ***************************************************************************
 * SIGINT signal handler.
 *
 * IN:
 * @sig	Signal number.
 ***************************************************************************
 */
void int_handler(int sig)
{
	sigint_caught = 1;
	printf("\n");	/* Skip "^C" displayed on screen */

}

/*
 ***************************************************************************
 * Init some structures.
 ***************************************************************************
 */
void init_structures(void)
{
	int i;

	for (i = 0; i < 3; i++)
		memset(&record_hdr[i], 0, RECORD_HEADER_SIZE);
}

/*
 ***************************************************************************
 * Allocate memory for sadc args.
 *
 * IN:
 * @i		Argument number.
 * @ltemp	Argument value.
 ***************************************************************************
 */
void salloc(int i, char *ltemp)
{
	if ((args[i] = (char *) malloc(strlen(ltemp) + 1)) == NULL) {
		perror("malloc");
		exit(4);
	}
	strcpy(args[i], ltemp);
}

/*
 ***************************************************************************
 * Display an error message. Happens when the data collector doesn't send
 * enough data.
 ***************************************************************************
 */
void print_read_error(void)
{
	fprintf(stderr, _("End of data collecting unexpected\n"));
	exit(3);
}

/*
 ***************************************************************************
 * Check that every selected activity actually belongs to the sequence list.
 * If not, then the activity should be unselected since it will not be sent
 * by sadc. An activity can be not sent if its number of items is null.
 *
 * IN:
 * @act_nr	Size of sequence list.
 ***************************************************************************
 */
void reverse_check_act(unsigned int act_nr)
{
	int i, j;

	for (i = 0; i < NR_ACT; i++) {

		if (IS_SELECTED(act[i]->options)) {

			for (j = 0; j < act_nr; j++) {
				if (id_seq[j] == act[i]->id)
					break;
			}
			if (j == act_nr)
				act[i]->options &= ~AO_SELECTED;
		}
	}
}

/*
 ***************************************************************************
 * Fill the (struct tm) rectime structure with current record's time,
 * based on current record's time data saved in file.
 * The resulting timestamp is expressed in the locale of the file creator
 * or in the user's own locale depending on whether option -t has been used
 * or not.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * 1 if an error was detected, or 0 otherwise.
 ***************************************************************************
*/
int sar_get_record_timestamp_struct(int curr)
{
	struct tm *ltm;

	/* Check if option -t was specified on the command line */
	if (PRINT_TRUE_TIME(flags)) {
		/* -t */
		rectime.tm_hour = record_hdr[curr].hour;
		rectime.tm_min  = record_hdr[curr].minute;
		rectime.tm_sec  = record_hdr[curr].second;
	}
	else {
		if ((ltm = localtime((const time_t *) &record_hdr[curr].ust_time)) == NULL)
			/*
			 * An error was detected.
			 * The rectime structure has NOT been updated.
			 */
			return 1;

		rectime = *ltm;
	}

	return 0;
}

/*
 ***************************************************************************
 * Determine if a stat header line has to be displayed.
 *
 * RETURNS:
 * TRUE if a header line has to be displayed.
 ***************************************************************************
*/
int check_line_hdr(void)
{
	int i, rc = FALSE;

	/* Get number of options entered on the command line */
	if (get_activity_nr(act, AO_SELECTED, COUNT_OUTPUTS) > 1)
		return TRUE;

	for (i = 0; i < NR_ACT; i++) {
		if (IS_SELECTED(act[i]->options)) {
			/* Special processing for activities using a bitmap */
			if (act[i]->bitmap) {
				if (count_bits(act[i]->bitmap->b_array,
					       BITMAP_SIZE(act[i]->bitmap->b_size)) > 1) {
					rc = TRUE;
				}
			}
			else if (act[i]->nr > 1) {
				rc = TRUE;
			}
			/* Stop now since we have only one selected activity */
			break;
		}
	}

	return rc;
}

/*
 ***************************************************************************
 * Set current record's timestamp string.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @len		Maximum length of timestamp string.
 *
 * OUT:
 * @cur_time	Timestamp string.
 *
 * RETURNS:
 * 1 if an error was detected, or 0 otherwise.
 ***************************************************************************
*/
int set_record_timestamp_string(int curr, char *cur_time, int len)
{
	/* Fill timestamp structure */
	if (sar_get_record_timestamp_struct(curr))
		/* Error detected */
		return 1;

	/* Set cur_time date value */
	strftime(cur_time, len, "%X", &rectime);

	return 0;
}

/*
 ***************************************************************************
 * Print statistics average.
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @read_from_file	Set to TRUE if stats are read from a system activity
 * 			data file.
 * @act_id		Activity that can be displayed, or ~0 for all.
 *			Remember that when reading stats from a file, only
 *			one activity can be displayed at a time.
 ***************************************************************************
 */
void write_stats_avg(int curr, int read_from_file, unsigned int act_id)
{
	int i;
	unsigned long long itv, g_itv;
	static __nr_t cpu_nr = -1;

	if (cpu_nr < 0)
		cpu_nr = act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr;

	/* Interval value in jiffies */
	g_itv = get_interval(record_hdr[2].uptime, record_hdr[curr].uptime);

	if (cpu_nr > 1)
		itv = get_interval(record_hdr[2].uptime0, record_hdr[curr].uptime0);
	else
		itv = g_itv;

	strncpy(timestamp[curr], _("Average:"), TIMESTAMP_LEN);
	timestamp[curr][TIMESTAMP_LEN - 1] = '\0';
	strcpy(timestamp[!curr], timestamp[curr]);

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	for (i = 0; i < NR_ACT; i++) {

		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {
			/* Display current average activity statistics */
			(*act[i]->f_print_avg)(act[i], 2, curr,
					       NEED_GLOBAL_ITV(act[i]->options) ? g_itv : itv);
		}
	}

	if (read_from_file) {
		/*
		 * Reset number of lines printed only if we read stats
		 * from a system activity file.
		 */
		avg_count = 0;
	}
}

/*
 ***************************************************************************
 * Print system statistics.
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @read_from_file	Set to TRUE if stats are read from a system activity
 * 			data file.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @reset		Set to TRUE if last_uptime variable should be
 * 			reinitialized (used in next_slice() function).
 * @act_id		Activity that can be displayed or ~0 for all.
 *			Remember that when reading stats from a file, only
 *			one activity can be displayed at a time.
 * @reset_cd		TRUE if static cross_day variable should be reset
 * 			(see below).
 *
 * OUT:
 * @cnt			Number of remaining lines to display.
 *
 * RETURNS:
 * 1 if stats have been successfully displayed, and 0 otherwise.
 ***************************************************************************
 */
int write_stats(int curr, int read_from_file, long *cnt, int use_tm_start,
		int use_tm_end, int reset, unsigned int act_id, int reset_cd)
{
	int i;
	unsigned long long itv, g_itv;
	static int cross_day = 0;
	static __nr_t cpu_nr = -1;

	if (cpu_nr < 0)
		cpu_nr = act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr;

	if (reset_cd) {
		/*
		 * cross_day is a static variable that is set to 1 when the first
		 * record of stats from a new day is read from a unique data file
		 * (in the case where the file contains data from two consecutive
		 * days). When set to 1, every following records timestamp will
		 * have its hour value increased by 24.
		 * Yet when a new activity (being read from the file) is going to
		 * be displayed, we start reading the file from the beginning
		 * again, and so cross_day should be reset in this case.
		 */
		cross_day = 0;
	}

	/* Check time (1) */
	if (read_from_file) {
		if (!next_slice(record_hdr[2].uptime0, record_hdr[curr].uptime0,
				reset, interval))
			/* Not close enough to desired interval */
			return 0;
	}

	/* Set previous timestamp */
	if (set_record_timestamp_string(!curr, timestamp[!curr], 16))
		return 0;
	/* Set current timestamp */
	if (set_record_timestamp_string(curr,  timestamp[curr],  16))
		return 0;

	/* Check if we are beginning a new day */
	if (use_tm_start && record_hdr[!curr].ust_time &&
	    (record_hdr[curr].ust_time > record_hdr[!curr].ust_time) &&
	    (record_hdr[curr].hour < record_hdr[!curr].hour)) {
		cross_day = 1;
	}

	if (cross_day) {
		/*
		 * This is necessary if we want to properly handle something like:
		 * sar -s time_start -e time_end with
		 * time_start(day D) > time_end(day D+1)
		 */
		rectime.tm_hour +=24;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(&rectime, &tm_start) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval values */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr],
		      cpu_nr, &itv, &g_itv);

	/* Check time (3) */
	if (use_tm_end && (datecmp(&rectime, &tm_end) > 0)) {
		/* It's too late... */
		*cnt = 0;
		return 0;
	}

	avg_count++;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	for (i = 0; i < NR_ACT; i++) {

		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {
			/* Display current activity statistics */
			(*act[i]->f_print)(act[i], !curr, curr,
					   NEED_GLOBAL_ITV(act[i]->options) ? g_itv : itv);
		}
	}

	return 1;
}

/*
 ***************************************************************************
 * Display stats since system startup.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void write_stats_startup(int curr)
{
	int i;

	/* Set to 0 previous structures corresponding to boot time */
	memset(&record_hdr[!curr], 0, RECORD_HEADER_SIZE);
	record_hdr[!curr].record_type = R_STATS;
	record_hdr[!curr].hour        = record_hdr[curr].hour;
	record_hdr[!curr].minute      = record_hdr[curr].minute;
	record_hdr[!curr].second      = record_hdr[curr].second;
	record_hdr[!curr].ust_time    = record_hdr[curr].ust_time;

	for (i = 0; i < NR_ACT; i++) {
		if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {
			memset(act[i]->buf[!curr], 0,
			       (size_t) act[i]->msize * (size_t) act[i]->nr * (size_t) act[i]->nr2);
		}
	}

	flags |= S_F_SINCE_BOOT;
	dis = TRUE;

	write_stats(curr, USE_SADC, &count, NO_TM_START, NO_TM_END, NO_RESET,
		    ALL_ACTIVITIES, TRUE);

	exit(0);
}

/*
 ***************************************************************************
 * Read data sent by the data collector.
 *
 * IN:
 * @size	Number of bytes of data to read.
 *
 * OUT:
 * @buffer	Buffer where data will be saved.
 *
 * RETURNS:
 * 1 if end of file has been reached, 0 otherwise.
 ***************************************************************************
 */
int sa_read(void *buffer, int size)
{
	int n;

	while (size) {

		if ((n = read(STDIN_FILENO, buffer, size)) < 0) {
			perror("read");
			exit(2);
		}

		if (!n)
			return 1;	/* EOF */

		size -= n;
		buffer = (char *) buffer + n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Print a Linux restart message (contents of a RESTART record) or a
 * comment (contents of a COMMENT record).
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @rtype		Record type to display.
 * @ifd			Input file descriptor.
 * @file		Name of file being read.
 * @file_magic		file_magic structure filled with file magic header
 * 			data.
 *
 * RETURNS:
 * 1 if the record has been successfully displayed, and 0 otherwise.
 ***************************************************************************
 */
int sar_print_special(int curr, int use_tm_start, int use_tm_end, int rtype,
		      int ifd, char *file, struct file_magic *file_magic)
{
	char cur_time[26];
	int dp = 1;
	unsigned int new_cpu_nr;

	if (set_record_timestamp_string(curr, cur_time, 26))
		return 0;

	/* The record must be in the interval specified by -s/-e options */
	if ((use_tm_start && (datecmp(&rectime, &tm_start) < 0)) ||
	    (use_tm_end && (datecmp(&rectime, &tm_end) > 0))) {
		dp = 0;
	}

	if (rtype == R_RESTART) {
		/* Don't forget to read the volatile activities structures */
		new_cpu_nr = read_vol_act_structures(ifd, act, file, file_magic,
						     file_hdr.sa_vol_act_nr);

		if (dp) {
			printf("\n%-11s       LINUX RESTART\t(%d CPU)\n",
			       cur_time, new_cpu_nr > 1 ? new_cpu_nr - 1 : 1);
			return 1;
		}
	}
	else if (rtype == R_COMMENT) {
		char file_comment[MAX_COMMENT_LEN];

		/* Don't forget to read comment record even if it won't be displayed... */
		replace_nonprintable_char(ifd, file_comment);

		if (dp && DISPLAY_COMMENT(flags)) {
			printf("%-11s  COM %s\n", cur_time, file_comment);
			return 1;
		}
	}

	return 0;
}

/*
 ***************************************************************************
 * Read the various statistics sent by the data collector (sadc).
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_sadc_stat_bunch(int curr)
{
	int i, p;

	/* Read record header (type is always R_STATS since it is read from sadc) */
	if (sa_read(&record_hdr[curr], RECORD_HEADER_SIZE)) {
		print_read_error();
	}

	for (i = 0; i < NR_ACT; i++) {

		if (!id_seq[i])
			continue;
		p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);

		if (sa_read(act[p]->buf[curr], act[p]->fsize * act[p]->nr * act[p]->nr2)) {
			print_read_error();
		}
	}
}

/*
 ***************************************************************************
 * Read stats for current activity from file and display them.
 *
 * IN:
 * @ifd		Input file descriptor.
 * @fpos	Position in file where reading must start.
 * @curr	Index in array for current sample statistics.
 * @rows	Number of rows of screen.
 * @act_id	Activity to display.
 * @file_actlst	List of activities in file.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 *
 * OUT:
 * @curr	Index in array for next sample statistics.
 * @cnt		Number of remaining lines of stats to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @reset	Set to TRUE if last_uptime variable should be
 * 		reinitialized (used in next_slice() function).
 ***************************************************************************
 */
void handle_curr_act_stats(int ifd, off_t fpos, int *curr, long *cnt, int *eosaf,
			   int rows, unsigned int act_id, int *reset,
			   struct file_activity *file_actlst, char *file,
			   struct file_magic *file_magic)
{
	int p, reset_cd;
	unsigned long lines = 0;
	unsigned char rtype;
	int davg = 0, next, inc;

	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	*cnt  = count;

	/* Assess number of lines printed */
	p = get_activity_position(act, act_id, EXIT_IF_NOT_FOUND);
	if (act[p]->bitmap) {
		inc = count_bits(act[p]->bitmap->b_array,
				 BITMAP_SIZE(act[p]->bitmap->b_size));
	}
	else {
		inc = act[p]->nr;
	}

	reset_cd = 1;

	do {
		/* Display count lines of stats */
		*eosaf = sa_fread(ifd, &record_hdr[*curr],
				  RECORD_HEADER_SIZE, SOFT_SIZE);
		rtype = record_hdr[*curr].record_type;

		if (!*eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
			/* Read the extra fields since it's not a special record */
			read_file_stat_bunch(act, *curr, ifd, file_hdr.sa_act_nr, file_actlst);
		}

		if ((lines >= rows) || !lines) {
			lines = 0;
			dis = 1;
		}
		else
			dis = 0;

		if (!*eosaf && (rtype != R_RESTART)) {

			if (rtype == R_COMMENT) {
				/* Display comment */
				next = sar_print_special(*curr, tm_start.use, tm_end.use,
						     R_COMMENT, ifd, file, file_magic);
				if (next) {
					/* A line of comment was actually displayed */
					lines++;
				}
				continue;
			}

			/* next is set to 1 when we were close enough to desired interval */
			next = write_stats(*curr, USE_SA_FILE, cnt, tm_start.use, tm_end.use,
					   *reset, act_id, reset_cd);
			reset_cd = 0;
			if (next && (*cnt > 0)) {
				(*cnt)--;
			}
			if (next) {
				davg++;
				*curr ^=1;
				lines += inc;
			}
			*reset = FALSE;
		}
	}
	while (*cnt && !*eosaf && (rtype != R_RESTART));

	if (davg) {
		write_stats_avg(!*curr, USE_SA_FILE, act_id);
	}

	*reset = TRUE;
}

/*
 ***************************************************************************
 * Read header data sent by sadc.
 ***************************************************************************
 */
void read_header_data(void)
{
	struct file_magic file_magic;
	struct file_activity file_act;
	int rc, i, p;
	char version[16];

	/* Read magic header */
	rc = sa_read(&file_magic, FILE_MAGIC_SIZE);

	sprintf(version, "%d.%d.%d.%d",
		file_magic.sysstat_version,
		file_magic.sysstat_patchlevel,
		file_magic.sysstat_sublevel,
		file_magic.sysstat_extraversion);
	if (!file_magic.sysstat_extraversion) {
		version[strlen(version) - 2] = '\0';
	}

	if (rc || (file_magic.sysstat_magic != SYSSTAT_MAGIC) ||
	    (file_magic.format_magic != FORMAT_MAGIC) ||
	    strcmp(version, VERSION)) {

		/* sar and sadc commands are not consistent */
		if (!rc && (file_magic.sysstat_magic == SYSSTAT_MAGIC)) {
			fprintf(stderr,
				_("Using a wrong data collector from a different sysstat version\n"));
		}

		goto input_error;
	}

	/*
	 * Read header data.
	 * No need to take into account file_magic.header_size. We are sure that
	 * sadc and sar are from the same version (we have checked FORMAT_MAGIC
	 * but also VERSION above) and thus the size of file_header is FILE_HEADER_SIZE.
	 */
	if (sa_read(&file_hdr, FILE_HEADER_SIZE)) {
		print_read_error();
	}

	if (file_hdr.sa_act_nr > NR_ACT)
		goto input_error;

	/* Read activity list */
	for (i = 0; i < file_hdr.sa_act_nr; i++) {

		if (sa_read(&file_act, FILE_ACTIVITY_SIZE)) {
			print_read_error();
		}

		p = get_activity_position(act, file_act.id, RESUME_IF_NOT_FOUND);

		if ((p < 0) || (act[p]->fsize != file_act.size)
			    || !file_act.nr
			    || !file_act.nr2
			    || (act[p]->magic != file_act.magic))
			/* Remember that we are reading data from sadc and not from a file... */
			goto input_error;

		id_seq[i]   = file_act.id;	/* We necessarily have "i < NR_ACT" */
		act[p]->nr  = file_act.nr;
		act[p]->nr2 = file_act.nr2;
	}

	while (i < NR_ACT) {
		id_seq[i++] = 0;
	}

	/* Check that all selected activties are actually sent by sadc */
	reverse_check_act(file_hdr.sa_act_nr);

	return;

input_error:

	/* Strange data sent by sadc...! */
	fprintf(stderr, _("Inconsistent input data\n"));

	exit(3);
}

/*
 ***************************************************************************
 * Read statistics from a system activity data file.
 *
 * IN:
 * @from_file	Input file name.
 ***************************************************************************
 */
void read_stats_from_file(char from_file[])
{
	struct file_magic file_magic;
	struct file_activity *file_actlst = NULL;
	int curr = 1, i, p;
	int ifd, rtype;
	int rows, eosaf = TRUE, reset = FALSE;
	long cnt = 1;
	off_t fpos;

	/* Get window size */
	rows = get_win_height();

	/* Read file headers and activity list */
	check_file_actlst(&ifd, from_file, act, &file_magic, &file_hdr,
			  &file_actlst, id_seq, FALSE);

	/* Perform required allocations */
	allocate_structures(act);

	/* Print report header */
	print_report_hdr(flags, &rectime, &file_hdr,
			 act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr);

	/* Read system statistics from file */
	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one, print it and
		 * (try to) get another one.
		 */
		do {
			if (sa_fread(ifd, &record_hdr[0], RECORD_HEADER_SIZE, SOFT_SIZE))
				/* End of sa data file */
				return;

			rtype = record_hdr[0].record_type;
			if ((rtype == R_RESTART) || (rtype == R_COMMENT)) {
				sar_print_special(0, tm_start.use, tm_end.use, rtype,
						  ifd, from_file, &file_magic);
			}
			else {
				/*
				 * OK: Previous record was not a special one.
				 * So read now the extra fields.
				 */
				read_file_stat_bunch(act, 0, ifd, file_hdr.sa_act_nr,
						     file_actlst);
				if (sar_get_record_timestamp_struct(0))
					/*
					 * An error was detected.
					 * The timestamp hasn't been updated.
					 */
					continue;
			}
		}
		while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
		       (tm_start.use && (datecmp(&rectime, &tm_start) < 0)) ||
		       (tm_end.use && (datecmp(&rectime, &tm_end) >=0)));

		/* Save the first stats collected. Will be used to compute the average */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		reset = TRUE;	/* Set flag to reset last_uptime variable */

		/* Save current file position */
		if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
			perror("lseek");
			exit(2);
		}

		/*
		 * Read and write stats located between two possible Linux restarts.
		 * Activities that should be displayed are saved in id_seq[] array.
		 */
		for (i = 0; i < NR_ACT; i++) {

			if (!id_seq[i])
				continue;

			p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
			if (!IS_SELECTED(act[p]->options))
				continue;

			if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
				handle_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, rows,
						      act[p]->id, &reset, file_actlst,
						      from_file, &file_magic);
			}
			else {
				unsigned int optf, msk;

				optf = act[p]->opt_flags;

				for (msk = 1; msk < 0x100; msk <<= 1) {
					if ((act[p]->opt_flags & 0xff) & msk) {
						act[p]->opt_flags &= (0xffffff00 + msk);

						handle_curr_act_stats(ifd, fpos, &curr, &cnt,
								      &eosaf, rows, act[p]->id,
								      &reset, file_actlst,
								      from_file, &file_magic);
						act[p]->opt_flags = optf;
					}
				}
			}
		}

		if (!cnt) {
			/* Go to next Linux restart, if possible */
			do {
				eosaf = sa_fread(ifd, &record_hdr[curr], RECORD_HEADER_SIZE,
						 SOFT_SIZE);
				rtype = record_hdr[curr].record_type;
				if (!eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
					read_file_stat_bunch(act, curr, ifd, file_hdr.sa_act_nr,
							     file_actlst);
				}
				else if (!eosaf && (rtype == R_COMMENT)) {
					/* This was a COMMENT record: print it */
					sar_print_special(curr, tm_start.use, tm_end.use, R_COMMENT,
							  ifd, from_file, &file_magic);
				}
			}
			while (!eosaf && (rtype != R_RESTART));
		}

		/* The last record we read was a RESTART one: Print it */
		if (!eosaf && (record_hdr[curr].record_type == R_RESTART)) {
			sar_print_special(curr, tm_start.use, tm_end.use, R_RESTART,
					  ifd, from_file, &file_magic);
		}
	}
	while (!eosaf);

	close(ifd);

	free(file_actlst);
}

/*
 ***************************************************************************
 * Read statistics sent by sadc, the data collector.
 ***************************************************************************
 */
void read_stats(void)
{
	int curr = 1;
	unsigned long lines;
	unsigned int rows;
	int dis_hdr = 0;

	/* Don't buffer data if redirected to a pipe... */
	setbuf(stdout, NULL);

	/* Read stats header */
	read_header_data();

	if (!get_activity_nr(act, AO_SELECTED, COUNT_ACTIVITIES)) {
		fprintf(stderr, _("Requested activities not available\n"));
		exit(1);
	}

	/* Determine if a stat line header has to be displayed */
	dis_hdr = check_line_hdr();

	lines = rows = get_win_height();

	/* Perform required allocations */
	allocate_structures(act);

	/* Print report header */
	print_report_hdr(flags, &rectime, &file_hdr,
			 act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr);

	/* Read system statistics sent by the data collector */
	read_sadc_stat_bunch(0);

	if (!interval) {
		/* Display stats since boot time and exit */
		write_stats_startup(0);
	}

	/* Save the first stats collected. Will be used to compute the average */
	copy_structures(act, id_seq, record_hdr, 2, 0);

	/* Set a handler for SIGINT */
	memset(&int_act, 0, sizeof(int_act));
	int_act.sa_handler = int_handler;
	int_act.sa_flags = SA_RESTART;
	sigaction(SIGINT, &int_act, NULL);

	/* Main loop */
	do {

		/* Get stats */
		read_sadc_stat_bunch(curr);

		/* Print results */
		if (!dis_hdr) {
			dis = lines / rows;
			if (dis) {
				lines %= rows;
			}
			lines++;
		}
		write_stats(curr, USE_SADC, &count, NO_TM_START, tm_end.use,
			    NO_RESET, ALL_ACTIVITIES, TRUE);

		if (record_hdr[curr].record_type == R_LAST_STATS) {
			/* File rotation is happening: Re-read header data sent by sadc */
			read_header_data();
			allocate_structures(act);
		}

		if (count > 0) {
			count--;
		}
		if (count) {
			if (sigint_caught) {
				/* SIGINT signal caught => Display average stats */
				count = 0;
			}
			else {
				curr ^= 1;
			}
		}
	}
	while (count);

	/* Print statistics average */
	dis = dis_hdr;
	write_stats_avg(curr, USE_SADC, ALL_ACTIVITIES);
}

/*
 ***************************************************************************
 * Main entry to the sar program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int i, rc, opt = 1, args_idx = 2;
	int fd[2];
	int day_offset = 0;
	char from_file[MAX_FILE_LEN], to_file[MAX_FILE_LEN];
	char ltemp[20];

	/* Get HZ */
	get_HZ();

	/* Compute page shift in kB */
	get_kb_shift();

	from_file[0] = to_file[0] = '\0';

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	tm_start.use = tm_end.use = FALSE;

	/* Allocate and init activity bitmaps */
	allocate_bitmaps(act);

	init_structures();

	/* Process options */
	while (opt < argc) {

		if (!strcmp(argv[opt], "--sadc")) {
			/* Locate sadc */
			which_sadc();
		}

		else if (!strcmp(argv[opt], "-I")) {
			if (argv[++opt]) {
				/* Parse -I option */
				if (parse_sar_I_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-D")) {
			/* Option to tell sar to write to saYYYYMMDD data files */
			flags |= S_F_SA_YYYYMMDD;
			opt++;
		}

		else if (!strcmp(argv[opt], "-P")) {
			/* Parse -P option */
			if (parse_sa_P_opt(argv, &opt, &flags, act)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-o")) {
			if (to_file[0]) {
				/* Output file already specified */
				usage(argv[0]);
			}
			/* Save stats to a file */
			if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
				strncpy(to_file, argv[opt++], MAX_FILE_LEN);
				to_file[MAX_FILE_LEN - 1] = '\0';
			}
			else {
				strcpy(to_file, "-");
			}
		}

		else if (!strcmp(argv[opt], "-f")) {
			if (from_file[0] || day_offset) {
				/* Input file already specified */
				usage(argv[0]);
			}
			/* Read stats from a file */
			if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
				strncpy(from_file, argv[opt++], MAX_FILE_LEN);
				from_file[MAX_FILE_LEN - 1] = '\0';
				/* Check if this is an alternate directory for sa files */
				check_alt_sa_dir(from_file, day_offset, -1);
			}
			else {
				set_default_file(from_file, day_offset, -1);
			}
		}

		else if (!strcmp(argv[opt], "-s")) {
			/* Get time start */
			if (parse_timestamp(argv, &opt, &tm_start, DEF_TMSTART)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-e")) {
			/* Get time end */
			if (parse_timestamp(argv, &opt, &tm_end, DEF_TMEND)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-h")) {
			/* Display help message */
			display_help(argv[0]);
		}

		else if (!strcmp(argv[opt], "-i")) {
			if (!argv[++opt] || (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
				usage(argv[0]);
			}
			interval = atol(argv[opt++]);
			if (interval < 1) {
				usage(argv[0]);
			}
			flags |= S_F_INTERVAL_SET;
		}

		else if (!strcmp(argv[opt], "-m")) {
			if (argv[++opt]) {
				/* Parse option -m */
				if (parse_sar_m_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-n")) {
			if (argv[++opt]) {
				/* Parse option -n */
				if (parse_sar_n_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if ((strlen(argv[opt]) > 1) &&
			 (strlen(argv[opt]) < 4) &&
			 !strncmp(argv[opt], "-", 1) &&
			 (strspn(argv[opt] + 1, DIGITS) == (strlen(argv[opt]) - 1))) {
			if (from_file[0] || day_offset) {
				/* Input file already specified */
				usage(argv[0]);
			}
			day_offset = atoi(argv[opt++] + 1);
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			/* Other options not previously tested */
			if ((rc = parse_sar_opt(argv, &opt, act, &flags, C_SAR)) != 0) {
				if (rc == 1) {
					usage(argv[0]);
				}
				exit(1);
			}
			opt++;
		}

		else if (interval < 0) {
			/* Get interval */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			interval = atol(argv[opt++]);
			if (interval < 0) {
				usage(argv[0]);
			}
		}

		else {
			/* Get count value */
			if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
			    !interval) {
				usage(argv[0]);
			}
			if (count) {
				/* Count parameter already set */
				usage(argv[0]);
			}
			count = atol(argv[opt++]);
			if (count < 1) {
				usage(argv[0]);
			}
		}
	}

	/* 'sar' is equivalent to 'sar -f' */
	if ((argc == 1) ||
	    ((interval < 0) && !from_file[0] && !to_file[0])) {
		set_default_file(from_file, day_offset, -1);
	}

	if (tm_start.use && tm_end.use && (tm_end.tm_hour < tm_start.tm_hour)) {
		tm_end.tm_hour += 24;
	}

	/*
	 * Check option dependencies.
	 */
	/* You read from a file OR you write to it... */
	if (from_file[0] && to_file[0]) {
		fprintf(stderr, _("-f and -o options are mutually exclusive\n"));
		exit(1);
	}
	/* Use time start or option -i only when reading stats from a file */
	if ((tm_start.use || INTERVAL_SET(flags)) && !from_file[0]) {
		fprintf(stderr,
			_("Not reading from a system activity file (use -f option)\n"));
		exit(1);
	}
	/* Don't print stats since boot time if -o or -f options are used */
	if (!interval && (from_file[0] || to_file[0])) {
		usage(argv[0]);
	}

	/* Cannot enter a day shift with -o option */
	if (to_file[0] && day_offset) {
		usage(argv[0]);
	}

	if (USE_PRETTY_OPTION(flags)) {
		dm_major = get_devmap_major();
	}

	if (!count) {
		/*
		 * count parameter not set: Display all the contents of the file
		 * or generate a report continuously.
		 */
		count = -1;
	}

	/* Default is CPU activity... */
	select_default_activity(act);

	/* Reading stats from file: */
	if (from_file[0]) {
		if (interval < 0) {
			interval = 1;
		}

		/* Read stats from file */
		read_stats_from_file(from_file);

		/* Free stuctures and activity bitmaps */
		free_bitmaps(act);
		free_structures(act);

		return 0;
	}

	/* Reading stats from sadc: */

	/* Create anonymous pipe */
	if (pipe(fd) == -1) {
		perror("pipe");
		exit(4);
	}

	switch (fork()) {

	case -1:
		perror("fork");
		exit(4);
		break;

	case 0: /* Child */
		if (dup2(fd[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			exit(4);
		}
		CLOSE_ALL(fd);

		/*
		 * Prepare options for sadc.
		 */
		/* Program name */
		salloc(0, SADC);

		/* Interval value */
		if (interval < 0) {
			usage(argv[0]);
		}
		else if (!interval) {
			strcpy(ltemp, "1");
		}
		else {
			sprintf(ltemp, "%ld", interval);
		}
		salloc(1, ltemp);

		/* Count number */
		if (count >= 0) {
			sprintf(ltemp, "%ld", count + 1);
			salloc(args_idx++, ltemp);
		}

		/* Flags to be passed to sadc */
		salloc(args_idx++, "-z");

		/* Writing data to a file (option -o) */
		if (to_file[0]) {
			/* Set option -D if entered */
			if (USE_SA_YYYYMMDD(flags)) {
				salloc(args_idx++, "-D");
			}
			/* Collect all possible activities (option -S XALL for sadc) */
			salloc(args_idx++, "-S");
			salloc(args_idx++, K_XALL);
			/* Outfile arg */
			salloc(args_idx++, to_file);
		}
		else {
			/*
			 * If option -o hasn't been used, then tell sadc
			 * to collect only activities that will be displayed.
			 */
			int act_id = 0;

			for (i = 0; i < NR_ACT; i++) {
				if (IS_SELECTED(act[i]->options)) {
					act_id |= act[i]->group;
				}
			}
			if (act_id) {
				act_id <<= 8;
				snprintf(ltemp, 19, "%d", act_id);
				ltemp[19] = '\0';
				salloc(args_idx++, "-S");
				salloc(args_idx++, ltemp);
			}
		}

		/* Last arg is NULL */
		args[args_idx] = NULL;

		/* Call now the data collector */
		execv(SADC_PATH, args);
		execvp(SADC, args);
		/*
		 * Note: Don't use execl/execlp since we don't have a fixed number of
		 * args to give to sadc.
		 */
		fprintf(stderr, _("Cannot find the data collector (%s)\n"), SADC);
		perror("exec");
		exit(4);
		break;

	default: /* Parent */
		if (dup2(fd[0], STDIN_FILENO) < 0) {
			perror("dup2");
			exit(4);
		}
		CLOSE_ALL(fd);

		/* Get now the statistics */
		read_stats();

		break;
	}

	/* Free structures and activity bitmaps */
	free_bitmaps(act);
	free_structures(act);

	return 0;
}
