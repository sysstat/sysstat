/*
 * tapestat: report tape statistics
 * (C) 2015 Hewlett-Packard Development Company, L.P.
 *
 * Initial revision by Shane M. SEYMOUR (shane.seymour <at> hpe.com)
 * Modified for sysstat by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <dirent.h>
#define __DO_NOT_DEFINE_COMPILE
#include <regex.h>
#include <inttypes.h>
#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#undef HZ /* sys/param.h defines HZ but needed for MAXPATHLEN */
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

#include "version.h"
#include "tapestat.h"
#include "rd_stats.h"
#include "count.h"

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

#ifdef TEST
void int_handler(int n) { return; }
#endif

int cpu_nr = 0;		/* Nb of processors on the machine */
int flags = 0;		/* Flag for common options and system state */

long interval = 0;
char timestamp[TIMESTAMP_LEN];

struct sigaction alrm_act;

/* Number of decimal places */
int dplaces_nr = -1;

/*
 * For tape stats - it would be extremely rare for there to be a very large
 * number of tape drives attached to a system. I wouldn't expect to see more
 * than 20-30 in a very large configuration and discontinguous ones should
 * be even more rare. Because of this we keep the old and new data in a
 * simple data structure with the tape index being the number after the tape
 * drive, st0 at index 0, etc.
 */
int max_tape_drives = 0;
struct tape_stats *tape_new_stats = { NULL };
struct tape_stats *tape_old_stats = { NULL };
regex_t tape_reg;

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
	fprintf(stderr, _("Options are:\n"
			  "[ --human ] [ -k | -m ] [ -t ] [ -V ] [ -y ] [ -z ]\n"));
	exit(1);
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
 * Initialization.
 ***************************************************************************
 */
void tape_initialise(void)
{
	/* How many processors on this machine? */
	cpu_nr = get_cpu_nr(~0, FALSE);

	/* Compile regular expression for tape names */
        if (regcomp(&tape_reg, "^st[0-9]+$", REG_EXTENDED) != 0) {
		exit(1);
        }
}

/*
 ***************************************************************************
 * Free structures.
 ***************************************************************************
 */
void tape_uninitialise(void)
{
	regfree(&tape_reg);
	if (tape_old_stats != NULL) {
		free(tape_old_stats);
	}
	if (tape_new_stats != NULL) {
		free(tape_new_stats);
	}
}

/*
 ***************************************************************************
 * Get maximum number of tapes in the system.
 *
 * RETURNS:
 * Number of tapes found.
 ***************************************************************************
 */
int get_max_tape_drives(void)
{
	DIR *dir;
	struct dirent *entry;
	int new_max_tape_drives, tmp, num_stats_dir = 0;
	regmatch_t match;
	char stats_dir[MAXPATHLEN + 1];
	struct stat stat_buf;

	new_max_tape_drives = max_tape_drives;

	/* Open sysfs tree */
	dir = opendir(SYSFS_CLASS_TAPE_DIR);
	if (dir == NULL)
		return 0;

	while ((entry = readdir(dir)) != NULL) {
		if (regexec(&tape_reg, &entry->d_name[0], 1, &match, 0) == 0) {
			/* d_name[2] to skip the st at the front */
			tmp = atoi(&entry->d_name[2]) + 1;
			if (tmp > new_max_tape_drives) {
				new_max_tape_drives = tmp;
			}
		}
		snprintf(stats_dir, MAXPATHLEN, "%s/%s/%s",
			SYSFS_CLASS_TAPE_DIR, &entry->d_name[0], "stats");
		if (stat(stats_dir, &stat_buf) == 0) {
			if (S_ISDIR(stat_buf.st_mode)) {
				num_stats_dir++;
			}
		}
	}
	closedir(dir);

	/* If there are no stats directories make the new number of tape drives 0 */
	if (num_stats_dir == 0) {
		new_max_tape_drives = 0;
	}

	return new_max_tape_drives;
}

/*
 ***************************************************************************
 * Check if new tapes have been added and reallocate structures accordingly.
 ***************************************************************************
 */
void tape_check_tapes_and_realloc(void)
{
	int new_max_tape_drives, i;

	/* Count again number of tapes */
	new_max_tape_drives = get_max_tape_drives();

	if (new_max_tape_drives > max_tape_drives && new_max_tape_drives > 0) {
		/* New tapes found: Realloc structures */
		struct tape_stats *tape_old_stats_t = (struct tape_stats *)
			realloc(tape_old_stats,	sizeof(struct tape_stats) * new_max_tape_drives);
		struct tape_stats *tape_new_stats_t = (struct tape_stats *)
			realloc(tape_new_stats,	sizeof(struct tape_stats) * new_max_tape_drives);
		if ((tape_old_stats_t == NULL) || (tape_new_stats_t == NULL)) {
			if (tape_old_stats_t != NULL) {
				free(tape_old_stats_t);
				tape_old_stats_t = NULL;
			} else {
				free(tape_old_stats);
				tape_old_stats = NULL;
			}
			if (tape_new_stats_t != NULL) {
				free(tape_new_stats_t);
				tape_new_stats_t = NULL;
			} else {
				free(tape_new_stats);
				tape_new_stats = NULL;
			}

			perror("realloc");
			exit(4);
		}

		tape_old_stats = tape_old_stats_t;
		tape_new_stats = tape_new_stats_t;

		for (i = max_tape_drives; i < new_max_tape_drives; i++) {
			tape_old_stats[i].valid = TAPE_STATS_INVALID;
			tape_new_stats[i].valid = TAPE_STATS_INVALID;
		}
		max_tape_drives = new_max_tape_drives;
	}
}

/*
 ***************************************************************************
 * Collect initial statistics for all existing tapes in the system.
 * This function should be called only once.
 ***************************************************************************
 */
void tape_gather_initial_stats(void)
{
	int new_max_tape_drives, i;
	FILE *fp;
	char filename[MAXPATHLEN + 1];

	/* Get number of tapes in the system */
	new_max_tape_drives = get_max_tape_drives();

	if (new_max_tape_drives == 0) {
		/* No tapes found */
		fprintf(stderr, _("No tape drives with statistics found\n"));
		exit(1);
	}
	else {
		/* Allocate structures */
		if (tape_old_stats == NULL) {
			tape_old_stats = (struct tape_stats *)
				malloc(sizeof(struct tape_stats) * new_max_tape_drives);
			tape_new_stats = (struct tape_stats *)
				malloc(sizeof(struct tape_stats) * new_max_tape_drives);
			for (i = 0; i < new_max_tape_drives; i++) {
				tape_old_stats[i].valid = TAPE_STATS_INVALID;
				tape_new_stats[i].valid = TAPE_STATS_INVALID;
			}
			max_tape_drives = new_max_tape_drives;
		} else
			/* This should only be called once */
			return;
	}

	/* Read stats for each tape */
	for (i = 0; i < max_tape_drives; i++) {
		/*
		 * Everything starts out valid but failing to open
		 * a file gets the tape drive marked invalid.
		 */
		tape_new_stats[i].valid = TAPE_STATS_VALID;
		tape_old_stats[i].valid = TAPE_STATS_VALID;

		__gettimeofday(&tape_old_stats[i].tv, NULL);

		tape_new_stats[i].tv.tv_sec = tape_old_stats[i].tv.tv_sec;
		tape_new_stats[i].tv.tv_usec = tape_old_stats[i].tv.tv_usec;

		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_ns", read_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_ns", write_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "io_ns", other_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_byte_cnt", read_bytes)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_byte_cnt", write_bytes)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_cnt", read_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_cnt", write_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "other_cnt", other_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "resid_cnt", resid_count)

		tape_old_stats[i].read_time = 0;
		tape_old_stats[i].write_time = 0;
		tape_old_stats[i].other_time = 0;
		tape_old_stats[i].read_bytes = 0;
		tape_old_stats[i].write_bytes = 0;
		tape_old_stats[i].read_count = 0;
		tape_old_stats[i].write_count = 0;
		tape_old_stats[i].other_count = 0;
		tape_old_stats[i].resid_count = 0;
	}
}

/*
 ***************************************************************************
 * Collect a new sample of statistics for all existing tapes in the system.
 ***************************************************************************
 */
void tape_get_updated_stats(void)
{
	int i;
	FILE *fp;
	char filename[MAXPATHLEN + 1] = { 0 };

	/* Check tapes and realloc structures if  needed */
	tape_check_tapes_and_realloc();

	for (i = 0; i < max_tape_drives; i++) {
		/*
		 * Everything starts out valid but failing
		 * to open a file gets the tape drive marked invalid.
		 */
		tape_new_stats[i].valid = TAPE_STATS_VALID;
		__gettimeofday(&tape_new_stats[i].tv, NULL);

		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_ns", read_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_ns", write_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "io_ns", other_time)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_byte_cnt", read_bytes)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_byte_cnt", write_bytes)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "read_cnt", read_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "write_cnt", write_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "other_cnt", other_count)
		TAPE_STAT_FILE_VAL(TAPE_STAT_PATH "resid_cnt", resid_count)

		if ((tape_new_stats[i].read_time < tape_old_stats[i].read_time) ||
		    (tape_new_stats[i].write_time < tape_old_stats[i].write_time) ||
		    (tape_new_stats[i].other_time < tape_old_stats[i].other_time)) {
			tape_new_stats[i].valid = TAPE_STATS_INVALID;
		}
	}
}

/*
 ***************************************************************************
 * Display tapes statistics headings.
 ***************************************************************************
 */
void tape_write_headings(void)
{
	printf("Tape:     r/s     w/s   ");
	if (DISPLAY_MEGABYTES(flags)) {
		printf("MB_read/s   MB_wrtn/s");
	} else {
		printf("kB_read/s   kB_wrtn/s");
	}
	printf("  %%Rd  %%Wr  %%Oa    Rs/s    Ot/s\n");
}

/*
 ***************************************************************************
 * Calculate statistics for current tape.
 *
 * IN:
 * @i		Index in array for current tape.
 *
 * OUT:
 * @stats	Statistics for current tape.
 ***************************************************************************
 */
void tape_calc_one_stats(struct calc_stats *stats, int i)
{
	uint64_t duration;
	double temp;
	FILE *fp;

	/* Duration in ms done in ms to prevent rounding issues with using seconds */
	duration = (tape_new_stats[i].tv.tv_sec -
		tape_old_stats[i].tv.tv_sec) * 1000;
	duration -= tape_old_stats[i].tv.tv_usec / 1000;
	duration += tape_new_stats[i].tv.tv_usec / 1000;

	/* If duration is zero we need to calculate the ms since boot time */
	if (duration == 0) {
		fp = fopen(UPTIME, "r");

		/*
		 * Get uptime from /proc/uptime and if we can't then just set duration to
		 * be 0 - it will mean that we don't calculate stats.
		 */
		if (fp == NULL) {
			duration = 0;
		} else {
			if (fscanf(fp, "%lf", &temp) != 1) {
				temp = 0;
			}
			duration = (uint64_t) (temp * 1000);
			fclose(fp);
		}
	}

	/* The second value passed into the macro is the thing being calculated */
	CALC_STAT_CNT(read_count, reads_per_second)
	CALC_STAT_CNT(write_count, writes_per_second)
	CALC_STAT_CNT(other_count, other_per_second)
	CALC_STAT_KB(read_bytes, kbytes_read_per_second)
	CALC_STAT_KB(write_bytes, kbytes_written_per_second)
	CALC_STAT_PCT(read_time, read_pct_wait)
	CALC_STAT_PCT(write_time, write_pct_wait)
	CALC_STAT_PCT(other_time, all_pct_wait)
	CALC_STAT_CNT(resid_count, resids_per_second)
}

/*
 ***************************************************************************
 * Display statistics for current tape.
 *
 * IN:
 * @tape	Statistics for current tape.
 * @i		Index in array for current tape.
 ***************************************************************************
 */
void tape_write_stats(struct calc_stats *tape, int i)
{
	char buffer[32];
	uint64_t divisor = 1;

	if (DISPLAY_MEGABYTES(flags))
		divisor = 1024;

	sprintf(buffer, "st%i        ", i);
	buffer[5] = 0;
	cprintf_in(IS_STR, "%s", buffer, 0);
	cprintf_u64(NO_UNIT, 2, 7,
		    tape->reads_per_second,
		    tape->writes_per_second);
	cprintf_u64(DISPLAY_UNIT(flags) ? UNIT_KILOBYTE : NO_UNIT, 2, 11,
		    DISPLAY_UNIT(flags) ? tape->kbytes_read_per_second
					: tape->kbytes_read_per_second / divisor,
		    DISPLAY_UNIT(flags) ? tape->kbytes_written_per_second
					: tape->kbytes_written_per_second / divisor);
	cprintf_pc(DISPLAY_UNIT(flags), 3, 4, 0,
		   (double) tape->read_pct_wait,
		   (double) tape->write_pct_wait,
		   (double) tape->all_pct_wait);
	cprintf_u64(NO_UNIT, 2, 7,
		    tape->resids_per_second,
		    tape->other_per_second);
	printf("\n");
}

/*
 ***************************************************************************
 * Print everything now (stats and uptime).
 *
 * IN:
 * @rectime	Current date and time.
 ***************************************************************************
 */
void write_stats(struct tm *rectime)
{
	int i;
	struct calc_stats tape;
	struct tape_stats *tmp;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	/* Print time stamp */
	if (DISPLAY_TIMESTAMP(flags)) {
		if (DISPLAY_ISO(flags)) {
			strftime(timestamp, sizeof(timestamp), "%FT%T%z", rectime);
		}
		else {
			strftime(timestamp, sizeof(timestamp), "%x %X", rectime);
		}
		printf("%s\n", timestamp);
	}

	/* Print the headings */
	tape_write_headings();

	/*
	 * If either new or old is invalid or the I/Os per second is 0 and
	 * zero omit is true then we print nothing.
	 */
	if (max_tape_drives > 0) {

		for (i = 0; i < max_tape_drives; i++) {
			if ((tape_new_stats[i].valid == TAPE_STATS_VALID) &&
				(tape_old_stats[i].valid == TAPE_STATS_VALID)) {
				tape_calc_one_stats(&tape, i);
				if (!(DISPLAY_ZERO_OMIT(flags)
					&& (tape.other_per_second == 0)
					&& (tape.reads_per_second == 0)
					&& (tape.writes_per_second == 0)
					&& (tape.kbytes_read_per_second == 0)
					&& (tape.kbytes_written_per_second == 0)
					&& (tape.read_pct_wait == 0)
					&& (tape.write_pct_wait == 0)
					&& (tape.all_pct_wait == 0)
					&& (tape.resids_per_second == 0))) {
					tape_write_stats(&tape, i);
				}
			}
		}
		/*
		 * Swap new and old so next time we compare against the new old stats.
		 * If a new tape drive appears it won't appear in the output until after
		 * the second time we gather information about it.
		 */
		tmp = tape_old_stats;
		tape_old_stats = tape_new_stats;
		tape_new_stats = tmp;
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Main loop: Read tape stats from the relevant sources and display them.
 *
 * IN:
 * @count	Number of lines of stats to print.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void rw_tape_stat_loop(long int count, struct tm *rectime)
{
	struct tape_stats *tmp;
	int skip = 0;

	/* Should we skip first report? */
	if (DISPLAY_OMIT_SINCE_BOOT(flags) && interval > 0) {
		skip = 1;
	}

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	do {

		if (tape_new_stats == NULL) {
			tape_gather_initial_stats();
		} else {
			tape_get_updated_stats();
		}

		/* Get time */
		get_localtime(rectime, 0);

		/* Check whether we should skip first report */
		if (!skip) {
			/* Print results */
			write_stats(rectime);

			if (count > 0) {
				count--;
			}
		}
		else {
			skip = 0;
			tmp = tape_old_stats;
			tape_old_stats = tape_new_stats;
			tape_new_stats = tmp;
		}

		if (count) {
			__pause();
		}
	}
	while (count);
}

/*
 ***************************************************************************
 * Main entry to the tapestat program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int it = 0;
	int opt = 1;
	int i;
	long count = 1;
	struct utsname header;
	struct tm rectime;

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	/* Init color strings */
	init_colors();

	/* Process args... */
	while (opt < argc) {

		if (!strcmp(argv[opt], "--human")) {
			flags |= T_D_UNIT;
			opt++;
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'k':
					if (DISPLAY_MEGABYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in kB/s */
					flags |= T_D_KILOBYTES;
					break;

				case 'm':
					if (DISPLAY_KILOBYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in MB/s */
					flags |= T_D_MEGABYTES;
					break;

				case 't':
					/* Display timestamp */
					flags |= T_D_TIMESTAMP;
					break;

				case 'y':
					/* Don't display stats since system restart */
					flags |= T_D_OMIT_SINCE_BOOT;
					break;

				case 'z':
					/* Omit output for devices with no activity */
					flags |= T_D_ZERO_OMIT;
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

	tape_initialise();

	get_localtime(&rectime, 0);

	/* Get system name, release number and hostname */
	__uname(&header);
	if (print_gal_header(&rectime, header.sysname, header.release,
			     header.nodename, header.machine, cpu_nr,
			     PLAIN_OUTPUT)) {
		flags |= T_D_ISO;
	}
	printf("\n");

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Main loop */
	rw_tape_stat_loop(count, &rectime);

	/* Free structures */
	tape_uninitialise();

	return 0;
}
