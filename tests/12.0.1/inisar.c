/*
 * sar: report system activity
 * (C) 1999-2018 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "iniversion.h"
#include "sa.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef USE_SCCSID
#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }
#endif

/* Interval and count parameters */
long interval = -1, count = 0;

/* TRUE if a header line must be printed */
int dis = TRUE;
/* TRUE if data read from file don't match current machine's endianness */
int endian_mismatch = FALSE;
/* TRUE if file's data come from a 64 bit machine */
int arch_64 = FALSE;
/* Number of decimal places */
int dplaces_nr = -1;

unsigned int flags = 0;
unsigned int dm_major;	/* Device-mapper major number */

char timestamp[2][TIMESTAMP_LEN];
extern unsigned int rec_types_nr[];

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
extern struct report_format sar_fmt;

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
			  "[ -A ] [ -B ] [ -b ] [ -C ] [ -D ] [ -d ] [ -F [ MOUNT ] ] [ -H ] [ -h ]\n"
			  "[ -p ] [ -q ] [ -r [ ALL ] ] [ -S ] [ -t ] [ -u [ ALL ] ] [ -V ]\n"
			  "[ -v ] [ -W ] [ -w ] [ -y ] [ -z ]\n"
			  "[ -I { <int_list> | SUM | ALL } ] [ -P { <cpu_list> | ALL } ]\n"
			  "[ -m { <keyword> [,...] | ALL } ] [ -n { <keyword> [,...] | ALL } ]\n"
			  "[ --dev=<dev_list> ] [ --fs=<fs_list> ] [ --iface=<iface_list> ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --help ] [ --human ] [ --sadc ]\n"
			  "[ -j { ID | LABEL | PATH | UUID | ... } ]\n"
			  "[ -f [ <filename> ] | -o [ <filename> ] | -[0-9]+ ]\n"
			  "[ -i <interval> ] [ -s [ <hh:mm[:ss]> ] ] [ -e [ <hh:mm[:ss]> ] ]\n"));
	exit(1);
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
 * Display an error message.
 *
 * IN:
 * @error_code	Code of error message to display.
 ***************************************************************************
 */
void print_read_error(int error_code)
{
	switch (error_code) {

		case END_OF_DATA_UNEXPECTED:
			/* Happens when the data collector doesn't send enough data */
			fprintf(stderr, _("End of data collecting unexpected\n"));
			break;

		default:
			/* Strange data sent by sadc...! */
			fprintf(stderr, _("Inconsistent input data\n"));
			break;
	}
	exit(3);
}

/*
 ***************************************************************************
 * Check that every selected activity actually belongs to the sequence list.
 * If not, then the activity should be unselected since it will not be sent
 * by sadc. An activity can be not sent if its number of items is zero.
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
			else if (act[i]->nr_ini > 1) {
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
	unsigned long long itv;

	/* Interval value in 1/100th of a second */
	itv = get_interval(record_hdr[2].uptime_cs, record_hdr[curr].uptime_cs);

	strncpy(timestamp[curr], _("Average:"), TIMESTAMP_LEN);
	timestamp[curr][TIMESTAMP_LEN - 1] = '\0';
	memcpy(timestamp[!curr], timestamp[curr], TIMESTAMP_LEN);

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	for (i = 0; i < NR_ACT; i++) {

		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if (IS_SELECTED(act[i]->options) && (act[i]->nr[curr] > 0)) {
			/* Display current average activity statistics */
			(*act[i]->f_print_avg)(act[i], 2, curr, itv);
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
 * This is called when we read stats either from a file or from sadc.
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
	unsigned long long itv;
	static int cross_day = 0;

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
		if (!next_slice(record_hdr[2].uptime_cs, record_hdr[curr].uptime_cs,
				reset, interval))
			/* Not close enough to desired interval */
			return 0;
	}

	if (!is_iso_time_fmt())
		flags |= S_F_PREFD_TIME_OUTPUT;

	/* Get then set previous timestamp */
	if (sa_get_record_timestamp_struct(flags + S_F_LOCAL_TIME, &record_hdr[!curr],
					   &rectime, NULL))
		return 0;
	set_record_timestamp_string(flags, &record_hdr[!curr],
				    NULL, timestamp[!curr], TIMESTAMP_LEN, &rectime);

	/* Get then set current timestamp */
	if (sa_get_record_timestamp_struct(flags + S_F_LOCAL_TIME, &record_hdr[curr],
					   &rectime, NULL))
		return 0;
	set_record_timestamp_string(flags, &record_hdr[curr],
				    NULL, timestamp[curr], TIMESTAMP_LEN, &rectime);

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
		rectime.tm_hour += 24;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(&rectime, &tm_start) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval value in 1/100th of a second */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr], &itv);

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

		if (IS_SELECTED(act[i]->options) && (act[i]->nr[curr] > 0)) {
			/* Display current activity statistics */
			(*act[i]->f_print)(act[i], !curr, curr, itv);
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
		if (IS_SELECTED(act[i]->options) && (act[i]->nr[curr] > 0)) {
			/*
			 * Using nr[curr] and not nr[!curr] below because we initialize
			 * reference structures for each structure that has been
			 * currently read in memory.
			 * No problem with buffers allocation since they all have the
			 * same size.
			 */
			memset(act[i]->buf[!curr], 0,
			       (size_t) act[i]->msize * (size_t) act[i]->nr[curr] * (size_t) act[i]->nr2);
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
 * Display a restart message (contents of a R_RESTART record).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function (unused here).
 * @cur_date	Date string of current restart message (unused here).
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC (unused here).
 * @file_hdr	System activity file standard header (unused here).
 ***************************************************************************
 */
__printf_funct_t print_sar_restart(int *tab, int action, char *cur_date, char *cur_time,
				  int utc, struct file_header *file_hdr)
{
	char restart[64];

	printf("\n%-11s", cur_time);
	sprintf(restart, "  LINUX RESTART\t(%d CPU)\n",
		file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);
	cprintf_s(IS_RESTART, "%s", restart);

}

/*
 ***************************************************************************
 * Display a comment (contents of R_COMMENT record).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function (unused here).
 * @cur_date	Date string of current comment (unused here).
 * @cur_time	Time string of current comment.
 * @utc		True if @cur_time is expressed in UTC (unused here).
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header (unused here).
 ***************************************************************************
 */
__print_funct_t print_sar_comment(int *tab, int action, char *cur_date, char *cur_time, int utc,
				  char *comment, struct file_header *file_hdr)
{
	printf("%-11s", cur_time);
	cprintf_s(IS_COMMENT, "  COM %s\n", comment);
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
 * @rec_hdr_tmp	Temporary buffer where current record header will be saved.
 * @endian_mismatch
 *		TRUE if file's data don't match current machine's endianness.
 * @arch_64	TRUE if file's data come from a 64 bit machine.
 *
 * OUT:
 * @curr	Index in array for next sample statistics.
 * @cnt		Number of remaining lines of stats to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @reset	Set to TRUE if last_uptime variable should be reinitialized
 *		(used in next_slice() function).
 ***************************************************************************
 */
void handle_curr_act_stats(int ifd, off_t fpos, int *curr, long *cnt, int *eosaf,
			   int rows, unsigned int act_id, int *reset,
			   struct file_activity *file_actlst, char *file,
			   struct file_magic *file_magic, void *rec_hdr_tmp,
			   int endian_mismatch, int arch_64)
{
	int p, reset_cd;
	unsigned long lines = 0;
	unsigned char rtype;
	int davg = 0, next, inc = 0;

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

	/* Assess number of lines printed when a bitmap is used */
	p = get_activity_position(act, act_id, EXIT_IF_NOT_FOUND);
	if (act[p]->bitmap) {
		inc = count_bits(act[p]->bitmap->b_array,
				 BITMAP_SIZE(act[p]->bitmap->b_size));
	}
	reset_cd = 1;

	do {
		/*
		 * Display <count> lines of stats.
		 * Start with reading current sample's record header.
		 */
		*eosaf = read_record_hdr(ifd, rec_hdr_tmp, &record_hdr[*curr],
					 &file_hdr, arch_64, endian_mismatch);
		rtype = record_hdr[*curr].record_type;

		if (!*eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
			/* Read the extra fields since it's not a special record */
			read_file_stat_bunch(act, *curr, ifd, file_hdr.sa_act_nr, file_actlst,
					     endian_mismatch, arch_64, file, file_magic);
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
				next = print_special_record(&record_hdr[*curr], flags + S_F_LOCAL_TIME,
							    &tm_start, &tm_end, R_COMMENT, ifd,
							    &rectime, NULL, file, 0,
							    file_magic, &file_hdr, act, &sar_fmt,
							    endian_mismatch, arch_64);
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
				*curr ^= 1;

				if (inc) {
					lines += inc;
				}
				else {
					lines += act[p]->nr[*curr];
				}
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
	char rec_hdr_tmp[MAX_RECORD_HEADER_SIZE];
	int curr = 1, i, p;
	int ifd, rtype;
	int rows, eosaf = TRUE, reset = FALSE;
	long cnt = 1;
	off_t fpos;

	/* Get window size */
	rows = get_win_height();

	/* Read file headers and activity list */
	check_file_actlst(&ifd, from_file, act, &file_magic, &file_hdr,
			  &file_actlst, id_seq, FALSE, &endian_mismatch, &arch_64);

	/* Perform required allocations */
	allocate_structures(act);

	/* Print report header */
	print_report_hdr(flags, &rectime, &file_hdr);

	/* Read system statistics from file */
	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one, print it and
		 * (try to) get another one.
		 */
		do {
			if (read_record_hdr(ifd, rec_hdr_tmp, &record_hdr[0], &file_hdr,
					    arch_64, endian_mismatch)) {
				/* End of sa data file */
				return;
			}

			rtype = record_hdr[0].record_type;
			if ((rtype == R_RESTART) || (rtype == R_COMMENT)) {
				print_special_record(&record_hdr[0], flags + S_F_LOCAL_TIME,
						     &tm_start, &tm_end, rtype, ifd,
						     &rectime, NULL, from_file, 0, &file_magic,
						     &file_hdr, act, &sar_fmt, endian_mismatch, arch_64);
			}
			else {
				/*
				 * OK: Previous record was not a special one.
				 * So read now the extra fields.
				 */
				read_file_stat_bunch(act, 0, ifd, file_hdr.sa_act_nr,
						     file_actlst, endian_mismatch, arch_64,
						     from_file, &file_magic);
				if (sa_get_record_timestamp_struct(flags + S_F_LOCAL_TIME,
								   &record_hdr[0],
								   &rectime, NULL))
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
		 * Since we are reading from a file, we print all the stats for an
		 * activity before displaying the next activity.
		 * id_seq[] has been created in check_file_actlst(), retaining only
		 * activities known by current sysstat version.
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
						      from_file, &file_magic, rec_hdr_tmp,
						      endian_mismatch, arch_64);
			}
			else {
				unsigned int optf, msk;

				optf = act[p]->opt_flags;

				for (msk = 1; msk < 0x100; msk <<= 1) {
					if ((act[p]->opt_flags & 0xff) & msk) {
						act[p]->opt_flags &= (0xffffff00 + msk);

						handle_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
								      rows, act[p]->id, &reset, file_actlst,
								      from_file, &file_magic, rec_hdr_tmp,
								      endian_mismatch, arch_64);
						act[p]->opt_flags = optf;
					}
				}
			}
		}

		if (!cnt) {
			/* Go to next Linux restart, if possible */
			do {
				/* Read next record header */
				eosaf = read_record_hdr(ifd, rec_hdr_tmp, &record_hdr[curr],
							&file_hdr, arch_64, endian_mismatch);
				rtype = record_hdr[curr].record_type;

				if (!eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
					read_file_stat_bunch(act, curr, ifd, file_hdr.sa_act_nr,
							     file_actlst, endian_mismatch, arch_64,
							     from_file, &file_magic);
				}
				else if (!eosaf && (rtype == R_COMMENT)) {
					/* This was a COMMENT record: print it */
					print_special_record(&record_hdr[curr], flags + S_F_LOCAL_TIME,
							     &tm_start, &tm_end, R_COMMENT, ifd,
							     &rectime, NULL, from_file, 0,
							     &file_magic, &file_hdr, act, &sar_fmt,
							     endian_mismatch, arch_64);
				}
			}
			while (!eosaf && (rtype != R_RESTART));
		}

		/* The last record we read was a RESTART one: Print it */
		if (!eosaf && (record_hdr[curr].record_type == R_RESTART)) {
			print_special_record(&record_hdr[curr], flags + S_F_LOCAL_TIME,
					     &tm_start, &tm_end, R_RESTART, ifd,
					     &rectime, NULL, from_file, 0,
					     &file_magic, &file_hdr, act, &sar_fmt,
					     endian_mismatch, arch_64);
		}
	}
	while (!eosaf);

	close(ifd);

	free(file_actlst);
}

/*
 ***************************************************************************
 * Main entry to the sar program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int rc, opt = 1, p, q;
	int day_offset = 0;
	char from_file[MAX_FILE_LEN], to_file[MAX_FILE_LEN];

	/* Compute page shift in kB */
	get_kb_shift();

	from_file[0] = to_file[0] = '\0';

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	/* Init color strings */
	init_colors();

	tm_start.use = tm_end.use = FALSE;

	/* Allocate and init activity bitmaps */
	allocate_bitmaps(act);

	init_structures();

	/* Process options */
	while (opt < argc) {

		if (!strncmp(argv[opt], "--dev=", 6)) {
			/* Parse devices entered on the command line */
			p = get_activity_position(act, A_DISK, EXIT_IF_NOT_FOUND);
			parse_sa_devices(argv[opt], act[p], MAX_DEV_LEN, &opt, 6);
		}

		else if (!strncmp(argv[opt], "--fs=", 5)) {
			/* Parse devices entered on the command line */
			p = get_activity_position(act, A_FS, EXIT_IF_NOT_FOUND);
			parse_sa_devices(argv[opt], act[p], MAX_FS_LEN, &opt, 5);
		}

		else if (!strncmp(argv[opt], "--iface=", 8)) {
			/* Parse devices entered on the command line */
			p = get_activity_position(act, A_NET_DEV, EXIT_IF_NOT_FOUND);
			parse_sa_devices(argv[opt], act[p], MAX_IFACE_LEN, &opt, 8);
			q = get_activity_position(act, A_NET_EDEV, EXIT_IF_NOT_FOUND);
			act[q]->item_list = act[p]->item_list;
			act[q]->item_list_sz = act[p]->item_list_sz;
			act[q]->options |= AO_LIST_ON_CMDLINE;
		}

		else if (!strcmp(argv[opt], "--human")) {
			/* Display sizes in a human readable format */
			flags |= S_F_UNIT;
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

		else if (!strcmp(argv[opt], "-I")) {
			/* Parse -I option */
			if (parse_sar_I_opt(argv, &opt, act)) {
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
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			/* Parse option -m */
			if (parse_sar_m_opt(argv, &opt, act)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-n")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			/* Parse option -n */
			if (parse_sar_n_opt(argv, &opt, act)) {
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

		/* Free structures and activity bitmaps */
		free_bitmaps(act);
		free_structures(act);

		return 0;
	}

	return 0;
}
