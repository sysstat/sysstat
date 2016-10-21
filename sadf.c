/*
 * sadf: system activity data formatter
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
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "version.h"
#include "sadf.h"
#include "sa.h"
#include "common.h"
#include "ioconf.h"

# include <locale.h>	/* For setlocale() */
#ifdef USE_NLS
# include <libintl.h>
# define _(string) gettext(string)
#else
# define _(string) (string)
#endif

#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }

long interval = -1, count = 0;

unsigned int flags = 0;
unsigned int dm_major;		/* Device-mapper major number */
unsigned int format = 0;	/* Output format */
unsigned int f_position = 0;	/* Output format position in array */

/* File header */
struct file_header file_hdr;

/*
 * Activity sequence.
 * This array must always be entirely filled (even with trailing zeros).
 */
unsigned int id_seq[NR_ACT];
/* Total number of SVG graphs for each activity */
int id_g_nr[NR_ACT];

/* Current record header */
struct record_header record_hdr[3];

/* Contain the date specified by -s and -e options */
struct tstamp tm_start, tm_end;
char *args[MAX_ARGV_NR];

extern struct activity *act[];
extern struct report_format *fmt[];

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
	fprintf(stderr,
		_("Usage: %s [ options ] [ <interval> [ <count> ] ] [ <datafile> | -[0-9]+ ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -C ] [ -c | -d | -g | -j | -p | -x ] [ -H ] [ -h ] [ -T | -t | -U ] [ -V ]\n"
			  "[ -O <opts> [,...] ] [ -P { <cpu> [,...] | ALL } ]\n"
			  "[ -s [ <hh:mm[:ss]> ] ] [ -e [ <hh:mm[:ss]> ] ]\n"
			  "[ -- <sar_options> ]\n"));
	exit(1);
}

/*
 ***************************************************************************
 * Init structures.
 ***************************************************************************
 */
void init_structures(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		memset(&record_hdr[i], 0, RECORD_HEADER_SIZE);
	}
}

/*
 ***************************************************************************
 * Look for output format in array.
 *
 * IN:
 * @fmt		Array of output formats.
 * @format	Output format to look for.
 *
 * RETURNS:
 * Position of output format in array.
 ***************************************************************************
 */
int get_format_position(struct report_format *fmt[], unsigned int format)
{
        int i;

        for (i = 0; i < NR_FMT; i++) {
                if (fmt[i]->id == format)
                        break;
        }

        if (i == NR_FMT)
		/* Should never happen */
                return 0;

        return i;
}

/*
 ***************************************************************************
 * Check that options entered on the command line are consistent with
 * selected output format. If no output format has been explicitly entered,
 * then select a default one.
 ***************************************************************************
 */
void check_format_options(void)
{
	if (!format) {
		/* Select output format if none has been selected */
		if (DISPLAY_HDR_ONLY(flags)) {
			format = F_HEADER_OUTPUT;
		}
		else {
			format = F_PPC_OUTPUT;
		}
	}

	/* Get format position in array */
	f_position = get_format_position(fmt, format);

	/* Check options consistency wrt output format */
	if (!ACCEPT_HEADER_ONLY(fmt[f_position]->options)) {
		/* Remove option -H */
		flags &= ~S_F_HDR_ONLY;
	}
	if (!ACCEPT_HORIZONTALLY(fmt[f_position]->options)) {
		/* Remove option -h */
		flags &= ~S_F_HORIZONTALLY;
	}
	if (!ACCEPT_LOCAL_TIME(fmt[f_position]->options)) {
		/* Remove options -T and -t */
		flags &= ~(S_F_LOCAL_TIME + S_F_TRUE_TIME);
	}
	if (!ACCEPT_SEC_EPOCH(fmt[f_position]->options)) {
		/* Remove option -U */
		flags &= ~S_F_SEC_EPOCH;
	}
	if (REJECT_TRUE_TIME(fmt[f_position]->options)) {
		/* Remove option -t */
		flags &= ~S_F_TRUE_TIME;
	}
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
 * printf() function modified for logic #1 (XML-like) display. Don't print a
 * CR at the end of the line.
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
 * printf() function modified for logic #1 (XML-like) display. Print a CR
 * at the end of the line.
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
 * Save or restore number of items for all known activities.
 *
 * IN:
 * @save_act_nr	Array containing number of items to restore for each
 * 		activity.
 * @action	DO_SAVE to save number of items, or DO_RESTORE to restore.
 *
 * OUT:
 * @save_act_nr	Array containing number of items saved for each activity.
 ***************************************************************************
 */
void sr_act_nr(__nr_t save_act_nr[], int action)
{
	int i;

	if (action == DO_SAVE) {
		/* Save number of items for all activities */
		for (i = 0; i < NR_ACT; i++) {
			save_act_nr[i] = act[i]->nr;
		}
	}
	else if (action == DO_RESTORE) {
		/*
		 * Restore number of items for all activities
		 * and reallocate structures accordingly.
		 */
		for (i = 0; i < NR_ACT; i++) {
			if (save_act_nr[i] > 0) {
				reallocate_vol_act_structures(act, save_act_nr[i],
							      act[i]->id);
			}
		}
	}
}

/*
 ***************************************************************************
 * Read next sample statistics. If it's a special record (R_RESTART or
 * R_COMMENT) then display it if requested. Also fill timestamps structures.
 *
 * IN:
 * @ifd		File descriptor
 * @action	Flags indicating if special records should be displayed or
 * 		not.
 * @curr	Index in array for current sample statistics.
 * @file	System activity data file name (name of file being read).
 * @tab		Number of tabulations to print.
 * @file_actlst	List of (known or unknown) activities in file.
 * @file_magic	System activity file magic header.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 *
 * OUT:
 * @rtype	Type of record read (R_RESTART, R_COMMENT, etc.)
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on options used) has been saved for current record.
 *		If current record was a special one (RESTART or COMMENT) and
 *		noted to be ignored, then the timestamp is saved only if
 *		explicitly told to do so with the SET_TIMESTAMPS action flag.
 * @loctime	Structure where timestamp (expressed in local time) has been
 *		saved for current record.
 *		If current record was a special one (RESTART or COMMENT) and
 *		noted to be ignored, then the timestamp is saved only if
 *		explicitly told to do so with the SET_TIMESTAMPS action flag.
 *
 * RETURNS:
 * TRUE if end of file has been reached.
 ***************************************************************************
 */
int read_next_sample(int ifd, int action, int curr, char *file, int *rtype, int tab,
		     struct file_magic *file_magic, struct file_activity *file_actlst,
		     struct tm *rectime, struct tm *loctime)
{
	int eosaf;

	/* Read current record */
	eosaf = sa_fread(ifd, &record_hdr[curr], RECORD_HEADER_SIZE, SOFT_SIZE);
	*rtype = record_hdr[curr].record_type;

	if (!eosaf) {
		if (*rtype == R_COMMENT) {
			if (action & IGNORE_COMMENT) {
				/* Ignore COMMENT record */
				if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
					perror("lseek");
				}
				if (action & SET_TIMESTAMPS) {
					sa_get_record_timestamp_struct(flags, &record_hdr[curr],
								       rectime, loctime);
				}
			}
			else {
				/* Display COMMENT record */
				print_special_record(&record_hdr[curr], flags, &tm_start, &tm_end,
						     *rtype, ifd, rectime, loctime, file, tab,
						     file_magic, &file_hdr, act, fmt[f_position]);
			}
		}
		else if (*rtype == R_RESTART) {
			if (action & IGNORE_RESTART) {
				/*
				 * Ignore RESTART record (don't display it)
				 * but anyway we have to reallocate volatile
				 * activities structures (unless we don't want to
				 * do it now).
				 */
				if (!(action & DONT_READ_VOLATILE)) {
					read_vol_act_structures(ifd, act, file, file_magic,
								file_hdr.sa_vol_act_nr);
				}
				if (action & SET_TIMESTAMPS) {
					sa_get_record_timestamp_struct(flags, &record_hdr[curr],
								       rectime, loctime);
				}
			}
			else {
				/* Display RESTART record */
				print_special_record(&record_hdr[curr], flags, &tm_start, &tm_end,
						     *rtype, ifd, rectime, loctime, file, tab,
						     file_magic, &file_hdr, act, fmt[f_position]);
			}
		}
		else {
			/*
			 * OK: Previous record was not a special one.
			 * So read now the extra fields.
			 */
			read_file_stat_bunch(act, curr, ifd, file_hdr.sa_act_nr,
					     file_actlst);
			sa_get_record_timestamp_struct(flags, &record_hdr[curr], rectime, loctime);
		}
	}

	return eosaf;
}

/*
 ***************************************************************************
 * Display the field list (used eg. in database format).
 *
 * IN:
 * @act_d	Activity to display, or ~0 for all.
 ***************************************************************************
 */
void list_fields(unsigned int act_id)
{
	int i, j;
	unsigned int msk;
	char *hl;
	char hline[HEADER_LINE_LEN] = "";

	printf("# hostname;interval;timestamp");

	for (i = 0; i < NR_ACT; i++) {

		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {
			if (!HAS_MULTIPLE_OUTPUTS(act[i]->options)) {
				printf(";%s", act[i]->hdr_line);
				if ((act[i]->nr > 1) && DISPLAY_HORIZONTALLY(flags)) {
					printf("[...]");
				}
			}
			else {
				msk = 1;
				strncpy(hline, act[i]->hdr_line, HEADER_LINE_LEN - 1);
				hline[HEADER_LINE_LEN - 1] = '\0';
				for (hl = strtok(hline, "|"); hl; hl = strtok(NULL, "|"), msk <<= 1) {
					if ((hl != NULL) && ((act[i]->opt_flags & 0xff) & msk)) {
						if (strchr(hl, '&')) {
							j = strcspn(hl, "&");
							if ((act[i]->opt_flags & 0xff00) & (msk << 8)) {
								/* Display whole header line */
								*(hl + j) = ';';
								printf(";%s", hl);
							}
							else {
								/* Display only the first part of the header line */
								*(hl + j) = '\0';
								printf(";%s", hl);
							}
							*(hl + j) = '&';
						}
						else {
							printf(";%s", hl);
						}
						if ((act[i]->nr > 1) && DISPLAY_HORIZONTALLY(flags)) {
							printf("[...]");
						}
					}
				}
			}
		}
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Determine the time (expressed in seconds since the epoch) used as the
 * origin on X axis for SVG graphs. If S_F_SVG_ONE_DAY is set, then origin
 * will be the beginning of current day (00:00:00) else it will be the time
 * of the first sample collected.
 *
 * RETURNS:
 * Time origin on X axis (expressed in seconds since the epoch).
 ***************************************************************************
 */
time_t get_time_ref(void)
{
	struct tm *ltm;
	time_t t;

	if (DISPLAY_ONE_DAY(flags)) {
		ltm = localtime((time_t *) &(record_hdr[2].ust_time));

		/* Move back to midnight */
		ltm->tm_sec = ltm->tm_min = ltm->tm_hour = 0;

		t = mktime(ltm);
		if (t != -1)
			return t;
	}

	return record_hdr[2].ust_time;
}

/*
 ***************************************************************************
 * Compute the number of SVG graphs to display. Each activity selected may
 * have several graphs. Moreover we have to take into account volatile
 * activities (eg. CPU) for which the number of graphs will depend on the
 * highest number of items (eg. maximum number of CPU) saved in the file.
 * This number may be higher than the real number of graphs that will be
 * displayed since some items have a preallocation constant.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 * @file_actlst	List of (known or unknown) activities in file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 *
 * RETURNS:
 * Total number of graphs to display, taking into account only activities
 * to be displayed, and selected period of time (options -s/-e).
 ***************************************************************************
 */
int get_svg_graph_nr(int ifd, char *file, struct file_magic *file_magic,
		     struct file_activity *file_actlst, struct tm *rectime,
		     struct tm *loctime)
{
	int i, n, p, eosaf;
	int rtype, new_tot_g_nr, tot_g_nr = 0;
	off_t fpos;
	__nr_t save_act_nr[NR_ACT] = {0};

	/* Save current file position and items number */
	if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
		perror("lseek");
		exit(2);
	}
	sr_act_nr(save_act_nr, DO_SAVE);

	/* Init total number of graphs for each activity */
	for (i = 0; i < NR_ACT; i++) {
		id_g_nr[i] = 0;
	}

	/* Look for the first record that will be displayed */
	do {
		eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
					 0, file, &rtype, 0, file_magic, file_actlst,
					 rectime, loctime);
		if (eosaf)
			/* No record to display => no graph too */
			return 0;
	}
	while ((tm_start.use && (datecmp(loctime, &tm_start) < 0)) ||
	       (tm_end.use && (datecmp(loctime, &tm_end) >= 0)));

	do {
		new_tot_g_nr = 0;

		for (i = 0; i < NR_ACT; i++) {
			if (!id_seq[i])
				continue;

			p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
			if (!IS_SELECTED(act[p]->options))
				continue;

			if (ONE_GRAPH_PER_ITEM(act[p]->options)) {
				 n = act[p]->g_nr * act[p]->nr;
			}
			else {
				n = act[p]->g_nr;
			}

			if (n > id_g_nr[i]) {
				 id_g_nr[i] = n;
			 }
			new_tot_g_nr += n;
		}

		if (new_tot_g_nr > tot_g_nr) {
			tot_g_nr = new_tot_g_nr;
		}

		do {
			eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
						 0, file, &rtype, 0, file_magic, file_actlst,
						 rectime, loctime);
			if (eosaf ||
			    (tm_end.use && (datecmp(loctime, &tm_end) >= 0)))
				/* End of data file or end time exceeded */
				break;
		}
		while (rtype != R_RESTART);

		if (eosaf ||
		    (tm_end.use && (datecmp(loctime, &tm_end) >= 0)))
			/*
			 * End of file, or end time exceeded:
			 * Current number of graphs is up-to-date.
			 */
			break;

	/*
	 * If we have found a RESTART record then we have also read the list of volatile
	 * activities following it, reallocated the structures and changed the number of
	 * items (act[p]->nr) for those volatile activities. So loop again to compute
	 * the new total number of graphs.
	 */
	}
	while (rtype == R_RESTART);

	/* Rewind file and restore items number */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}
	sr_act_nr(save_act_nr, DO_RESTORE);

	return tot_g_nr;
}
/*
 ***************************************************************************
 * Display *one* sample of statistics for one or several activities,
 * checking that all conditions are met before printing (time start, time
 * end, interval). Current record should be a record of statistics (R_STATS),
 * not a special one (R_RESTART or R_COMMENT).
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @reset		Set to TRUE if last_uptime should be reinitialized
 *			(used in next_slice() function).
 * @parm		Pointer on parameters depending on output format
 * 			(eg.: number of tabulations to print).
 * @cpu_nr		Number of processors.
 * @rectime		Structure where timestamp (expressed in local time
 *			or in UTC depending on whether options -T/-t have
 * 			been used or not) has been saved for current record.
 * @loctime		Structure where timestamp (expressed in local time)
 *			has been saved for current record.
 * @reset_cd		TRUE if static cross_day variable should be reset.
 * @act_id		Activity to display (only for formats where
 * 			activities are displayed one at a time) or
 *			ALL_ACTIVITIES for all.
 *
 * OUT:
 * @cnt			Set to 0 to indicate that no other lines of stats
 * 			should be displayed.
 *
 * RETURNS:
 * 1 if stats have been successfully displayed.
 ***************************************************************************
 */
int generic_write_stats(int curr, int use_tm_start, int use_tm_end, int reset,
			long *cnt, void *parm, __nr_t cpu_nr, struct tm *rectime,
			struct tm *loctime, int reset_cd, unsigned int act_id)
{
	int i;
	unsigned long long dt, itv, g_itv;
	char cur_date[TIMESTAMP_LEN], cur_time[TIMESTAMP_LEN], *pre = NULL;
	static int cross_day = FALSE;

	if (reset_cd) {
		/*
		 * See note in sar.c.
		 * NB: Reseting cross_day is needed only if datafile
		 * may be rewinded (eg. in db or ppc output formats).
		 */
		cross_day = 0;
	}

	/*
	 * Check time (1).
	 * For this first check, we use the time interval entered on
	 * the command line. This is equivalent to sar's option -i which
	 * selects records at seconds as close as possible to the number
	 * specified by the interval parameter.
	 */
	if (!next_slice(record_hdr[2].uptime0, record_hdr[curr].uptime0,
			reset, interval))
		/* Not close enough to desired interval */
		return 0;

	/* Check if we are beginning a new day */
	if (use_tm_start && record_hdr[!curr].ust_time &&
	    (record_hdr[curr].ust_time > record_hdr[!curr].ust_time) &&
	    (record_hdr[curr].hour < record_hdr[!curr].hour)) {
		cross_day = TRUE;
	}

	if (cross_day) {
		/*
		 * This is necessary if we want to properly handle something like:
		 * sar -s time_start -e time_end with
		 * time_start(day D) > time_end(day D+1)
		 */
		loctime->tm_hour += 24;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(loctime, &tm_start) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval values */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr],
		      cpu_nr, &itv, &g_itv);

	/* Check time (3) */
	if (use_tm_end && (datecmp(loctime, &tm_end) > 0)) {
		/* It's too late... */
		*cnt = 0;
		return 0;
	}

	dt = itv / HZ;
	/* Correct rounding error for dt */
	if ((itv % HZ) >= (HZ / 2)) {
		dt++;
	}

	/* Set date and time strings for current record */
	set_record_timestamp_string(flags, &record_hdr[curr],
				    cur_date, cur_time, TIMESTAMP_LEN, rectime);

	if (*fmt[f_position]->f_timestamp) {
		pre = (char *) (*fmt[f_position]->f_timestamp)(parm, F_BEGIN, cur_date, cur_time,
							       dt, &file_hdr, flags);
	}

	/* Display statistics */
	for (i = 0; i < NR_ACT; i++) {

		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if ((TEST_MARKUP(fmt[f_position]->options) && CLOSE_MARKUP(act[i]->options)) ||
		    (IS_SELECTED(act[i]->options) && (act[i]->nr > 0))) {

			if (format == F_JSON_OUTPUT) {
				/* JSON output */
				int *tab = (int *) parm;

				if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {

					if (*fmt[f_position]->f_timestamp) {
						(*fmt[f_position]->f_timestamp)(tab, F_MAIN, cur_date, cur_time,
										dt, &file_hdr, flags);
					}
				}
				(*act[i]->f_json_print)(act[i], curr, *tab, NEED_GLOBAL_ITV(act[i]->options) ?
							g_itv : itv);
			}

			else if (format == F_XML_OUTPUT) {
				/* XML output */
				int *tab = (int *) parm;

				(*act[i]->f_xml_print)(act[i], curr, *tab, NEED_GLOBAL_ITV(act[i]->options) ?
						       g_itv : itv);
			}

			else if (format == F_SVG_OUTPUT) {
				/* SVG output */
				struct svg_parm *svg_p = (struct svg_parm *) parm;

				svg_p->dt = (unsigned long) dt;
				(*act[i]->f_svg_print)(act[i], curr, F_MAIN, svg_p,
						       NEED_GLOBAL_ITV(act[i]->options) ? g_itv : itv,
						       &record_hdr[curr]);
			}

			else {
				/* Other output formats: db, ppc */
				(*act[i]->f_render)(act[i], (format == F_DB_OUTPUT), pre, curr,
						    NEED_GLOBAL_ITV(act[i]->options) ? g_itv : itv);
			}
		}
	}

	if (*fmt[f_position]->f_timestamp) {
		(*fmt[f_position]->f_timestamp)(parm, F_END, cur_date, cur_time,
						dt, &file_hdr, flags);
	}

	return 1;
}

/*
 ***************************************************************************
 * Read stats for current activity from file and print them.
 * Display at most <count> lines of stats (and possibly comments inserted
 * in file) located between two LINUX RESTART messages.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @fpos	Position in file where reading must start.
 * @curr	Index in array for current sample statistics.
 * @act_id	Activity to display, or ~0 for all.
 * @file_actlst	List of (known or unknown) activities in file.
 * @cpu_nr	Number of processors for current activity data file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 *
 * OUT:
 * @curr	Index in array for next sample statistics.
 * @cnt		Number of lines of stats remaining to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @reset	Set to TRUE if last_uptime variable should be
 * 		reinitialized (used in next_slice() function).
 ***************************************************************************
 */
void rw_curr_act_stats(int ifd, off_t fpos, int *curr, long *cnt, int *eosaf,
		       unsigned int act_id, int *reset, struct file_activity *file_actlst,
		        __nr_t cpu_nr, struct tm *rectime, struct tm *loctime,
			char *file, struct file_magic *file_magic)
{
	int rtype;
	int next, reset_cd;

	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}

	if (DISPLAY_FIELD_LIST(fmt[f_position]->options)) {
		/* Print field list */
		list_fields(act_id);
	}

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	*cnt  = count;
	reset_cd = 1;

	do {
		/* Display <count> lines of stats */
		*eosaf = read_next_sample(ifd, IGNORE_RESTART | DONT_READ_VOLATILE,
					  *curr, file, &rtype, 0, file_magic,
					  file_actlst, rectime, loctime);

		if (!*eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
			next = generic_write_stats(*curr, tm_start.use, tm_end.use, *reset, cnt,
						   NULL, cpu_nr, rectime, loctime, reset_cd, act_id);
			reset_cd = 0;

			if (next) {
				/*
				 * next is set to 1 when we were close enough to desired interval.
				 * In this case, the call to generic_write_stats() has actually
				 * displayed a line of stats.
				 */
				*curr ^= 1;
				if (*cnt > 0) {
					(*cnt)--;
				}
			}
			*reset = FALSE;
		}
	}
	while (*cnt && !*eosaf && (rtype != R_RESTART));

	*reset = TRUE;
}

/*
 ***************************************************************************
 * Read stats for current activity from file and display its SVG graphs.
 * At most <count> lines of stats are taken into account.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @fpos	Position in file where reading must start.
 * @curr	Index in array for current sample statistics.
 * @p		Current activity position.
 * @file_actlst	List of (known or unknown) activities in file.
 * @cpu_nr	Number of processors for current activity data file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 * @save_act_nr	Array where the number of volatile activities are saved
 *		for current position in file.
 * @g_nr	Number of graphs already displayed (for all activities).
 *
 * OUT:
 * @cnt		Number of lines of stats remaining to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @reset	Set to TRUE if last_uptime variable should be
 *		reinitialized (used in next_slice() function).
 * @g_nr	Total number of views displayed (including current activity).
 ***************************************************************************
 */
void display_curr_act_graphs(int ifd, off_t fpos, int *curr, long *cnt, int *eosaf,
			     int p, int *reset, struct file_activity *file_actlst,
			     __nr_t cpu_nr, struct tm *rectime, struct tm *loctime,
			     char *file, struct file_magic *file_magic,
			     __nr_t save_act_nr[], int *g_nr)
{
	struct svg_parm parm;
	int rtype;
	int next, reset_cd;

	/* Rewind file... */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}
	/*
	 * ... and restore number of items for volatile activities
	 * for this position in file.
	 */
	sr_act_nr(save_act_nr, DO_RESTORE);

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	parm.graph_no = *g_nr;
	parm.ust_time_ref = get_time_ref();
	parm.ust_time_first = record_hdr[2].ust_time;
	parm.restart = TRUE;

	*cnt  = count;
	reset_cd = 1;

	/* Allocate graphs arrays */
	(*act[p]->f_svg_print)(act[p], !*curr, F_BEGIN, &parm, 0, &record_hdr[!*curr]);

	do {
		*eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
					  *curr, file, &rtype, 0, file_magic,
					  file_actlst, rectime, loctime);

		if (!*eosaf && (rtype != R_COMMENT) && (rtype != R_RESTART)) {

			next = generic_write_stats(*curr, tm_start.use, tm_end.use, *reset, cnt,
						   &parm, cpu_nr, rectime, loctime, reset_cd, act[p]->id);
			reset_cd = 0;
			if (next) {
				/*
				 * next is set to 1 when we were close enough to desired interval.
				 * In this case, the call to generic_write_stats() has actually
				 * displayed a line of stats.
				 */
				parm.restart = FALSE;
				*curr ^= 1;
				if (*cnt > 0) {
					(*cnt)--;
				}
			}
			*reset = FALSE;
		}
		if (!*eosaf && (rtype == R_RESTART)) {
			parm.restart = TRUE;
			*reset = TRUE;
			/* Go to next statistics record, if possible */
			do {
				*eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
							  *curr, file, &rtype, 0, file_magic,
							  file_actlst, rectime, loctime);
			}
			while (!*eosaf && ((rtype == R_RESTART) || (rtype == R_COMMENT)));
			*curr ^= 1;
		}
		
	}
	while (!*eosaf);

	*reset = TRUE;

	/* Determine X axis end value */
	if (DISPLAY_ONE_DAY(flags) &&
	    (parm.ust_time_ref + (3600 * 24) > record_hdr[!*curr].ust_time)) {
		parm.ust_time_end = parm.ust_time_ref + (3600 * 24);
	}
	else {
		parm.ust_time_end = record_hdr[!*curr].ust_time;
	}

	/* Actually display graphs for current activity */
	(*act[p]->f_svg_print)(act[p], *curr, F_END, &parm, 0, &record_hdr[!*curr]);

	/* Update total number of graphs already displayed */
	*g_nr = parm.graph_no;
}

/*
 ***************************************************************************
 * Display file contents in selected format (logic #1).
 * Logic #1:	Grouped by record type. Sorted by timestamp.
 * Formats:	XML, JSON
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file_actlst	List of (known or unknown) activities in file.
 * @file	System activity data file name (name of file being read).
 * @file_magic	System activity file magic header.
 * @cpu_nr	Number of processors for current activity data file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 ***************************************************************************
 */
void logic1_display_loop(int ifd, struct file_activity *file_actlst, char *file,
			 struct file_magic *file_magic, __nr_t cpu_nr,
			 struct tm *rectime, struct tm *loctime)
{
	int curr, tab = 0, rtype;
	int eosaf, next, reset = FALSE;
	__nr_t save_act_nr[NR_ACT] = {0};
	long cnt = 1;
	off_t fpos;

	if (format == F_JSON_OUTPUT) {
		/* Use a decimal point to make JSON code compliant with RFC7159 */
		setlocale(LC_NUMERIC, "C");
	}

	/* Save current file position */
	if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
		perror("lseek");
		exit(2);
	}
	/* Save number of activities items for current file position */
	sr_act_nr(save_act_nr, DO_SAVE);

	/* Print header (eg. XML file header) */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&tab, F_BEGIN, file, file_magic,
					     &file_hdr, cpu_nr, act, id_seq);
	}

	/* Process activities */
	if (*fmt[f_position]->f_statistics) {
		(*fmt[f_position]->f_statistics)(&tab, F_BEGIN);
	}

	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one,
		 * skip it and try to read the next record in file.
		 */
		do {
			eosaf = read_next_sample(ifd, IGNORE_COMMENT | IGNORE_RESTART, 0,
						 file, &rtype, tab, file_magic, file_actlst,
						 rectime, loctime);
		}
		while (!eosaf && ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
			(tm_start.use && (datecmp(loctime, &tm_start) < 0)) ||
			(tm_end.use && (datecmp(loctime, &tm_end) >= 0))));

		/* Save the first stats collected. Used for example in next_slice() function */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		curr = 1;
		cnt = count;
		reset = TRUE;

		if (!eosaf) {
			do {
				eosaf = read_next_sample(ifd, IGNORE_COMMENT | IGNORE_RESTART, curr,
							 file, &rtype, tab, file_magic, file_actlst,
							 rectime, loctime);

				if (!eosaf && (rtype != R_COMMENT) && (rtype != R_RESTART)) {
					if (*fmt[f_position]->f_statistics) {
						(*fmt[f_position]->f_statistics)(&tab, F_MAIN);
					}

					/* next is set to 1 when we were close enough to desired interval */
					next = generic_write_stats(curr, tm_start.use, tm_end.use, reset,
								  &cnt, &tab, cpu_nr, rectime, loctime,
								  FALSE, ALL_ACTIVITIES);

					if (next) {
						curr ^= 1;
						if (cnt > 0) {
							cnt--;
						}
					}
					reset = FALSE;
				}
			}
			while (cnt && !eosaf && (rtype != R_RESTART));

			if (!cnt) {
				/* Go to next Linux restart, if possible */
				do {
					eosaf = read_next_sample(ifd, IGNORE_COMMENT | IGNORE_RESTART, curr,
								 file, &rtype, tab, file_magic, file_actlst,
								 rectime, loctime);
				}
				while (!eosaf && (rtype != R_RESTART));
			}
			reset = TRUE;
		}
	}
	while (!eosaf);

	if (*fmt[f_position]->f_statistics) {
		(*fmt[f_position]->f_statistics)(&tab, F_END);
	}

	/* Rewind file... */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}
	/*
	 * ... and restore number of items for volatile activities
	 * for this position in file.
	 */
	sr_act_nr(save_act_nr, DO_RESTORE);

	/* Process now RESTART entries to display restart messages */
	if (*fmt[f_position]->f_restart) {
		(*fmt[f_position]->f_restart)(&tab, F_BEGIN, NULL, NULL, FALSE,
					      &file_hdr, 0);
	}

	do {
		eosaf = read_next_sample(ifd, IGNORE_COMMENT, 0,
					 file, &rtype, tab, file_magic, file_actlst,
					 rectime, loctime);
	}
	while (!eosaf);

	if (*fmt[f_position]->f_restart) {
		(*fmt[f_position]->f_restart)(&tab, F_END, NULL, NULL, FALSE, &file_hdr, 0);
	}

	/* Rewind file... */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}
	/*
	 * ... and restore number of items for volatile activities
	 * for this position in file.
	 */
	sr_act_nr(save_act_nr, DO_RESTORE);

	/* Last, process COMMENT entries to display comments */
	if (DISPLAY_COMMENT(flags)) {
		if (*fmt[f_position]->f_comment) {
			(*fmt[f_position]->f_comment)(&tab, F_BEGIN, NULL, NULL, 0, NULL,
						      &file_hdr);
		}
		do {
			eosaf = read_next_sample(ifd, IGNORE_RESTART, 0,
						 file, &rtype, tab, file_magic, file_actlst,
						 rectime, loctime);
		}
		while (!eosaf);

		if (*fmt[f_position]->f_comment) {
			(*fmt[f_position]->f_comment)(&tab, F_END, NULL, NULL, 0, NULL,
						      &file_hdr);
		}
	}

	/* Print header trailer */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&tab, F_END, file, file_magic,
					     &file_hdr, cpu_nr, act, id_seq);
	}
}

/*
 ***************************************************************************
 * Display file contents in selected format (logic #2).
 * Logic #2:	Grouped by activity. Sorted by timestamp. Stop on RESTART
 * 		records.
 * Formats:	ppc, CSV
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file_actlst	List of (known or unknown) activities in file.
 * @cpu_nr	Number of processors for current activity data file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 ***************************************************************************
 */
void logic2_display_loop(int ifd, struct file_activity *file_actlst, __nr_t cpu_nr,
			 struct tm *rectime, struct tm *loctime, char *file,
			 struct file_magic *file_magic)
{
	int i, p;
	int curr = 1, rtype;
	int eosaf = TRUE, reset = FALSE;
	long cnt = 1;
	off_t fpos;

	/* Read system statistics from file */
	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one, print it and
		 * (try to) get another one.
		 */
		do {
			if (read_next_sample(ifd, IGNORE_NOTHING, 0,
					     file, &rtype, 0, file_magic, file_actlst,
					     rectime, loctime))
				/* End of sa data file */
				return;
		}
		while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
		       (tm_start.use && (datecmp(loctime, &tm_start) < 0)) ||
		       (tm_end.use && (datecmp(loctime, &tm_end) >= 0)));

		/* Save the first stats collected. Used for example in next_slice() function */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		/* Set flag to reset last_uptime variable. Should be done after a LINUX RESTART record */
		reset = TRUE;

		/* Save current file position */
		if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
			perror("lseek");
			exit(2);
		}

		/* Read and write stats located between two possible Linux restarts */

		if (DISPLAY_HORIZONTALLY(flags)) {
			/*
			 * If stats are displayed horizontally, then all activities
			 * are printed on the same line.
			 */
			rw_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
					  ALL_ACTIVITIES, &reset, file_actlst,
					  cpu_nr, rectime, loctime, file, file_magic);
		}
		else {
			/* For each requested activity... */
			for (i = 0; i < NR_ACT; i++) {

				if (!id_seq[i])
					continue;

				p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
				if (!IS_SELECTED(act[p]->options))
					continue;

				if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
					rw_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
							  act[p]->id, &reset, file_actlst,
							  cpu_nr, rectime, loctime, file,
							  file_magic);
				}
				else {
					unsigned int optf, msk;

					optf = act[p]->opt_flags;

					for (msk = 1; msk < 0x100; msk <<= 1) {
						if ((act[p]->opt_flags & 0xff) & msk) {
							act[p]->opt_flags &= (0xffffff00 + msk);

							rw_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
									  act[p]->id, &reset, file_actlst,
									  cpu_nr, rectime, loctime, file,
									  file_magic);
							act[p]->opt_flags = optf;
						}
					}
				}
			}
		}

		if (!cnt) {
			/* Go to next Linux restart, if possible */
			do {
				eosaf = read_next_sample(ifd, IGNORE_RESTART | DONT_READ_VOLATILE,
							 curr, file, &rtype, 0, file_magic,
							 file_actlst, rectime, loctime);
			}
			while (!eosaf && (rtype != R_RESTART));
		}

		/*
		 * The last record we read was a RESTART one: Print it.
		 * NB: Unlike COMMENTS records (which are displayed for each
		 * activity), RESTART ones are only displayed once.
		 */
		if (!eosaf && (record_hdr[curr].record_type == R_RESTART)) {
			print_special_record(&record_hdr[curr], flags, &tm_start, &tm_end,
					     R_RESTART, ifd, rectime, loctime, file, 0,
					     file_magic, &file_hdr, act, fmt[f_position]);
		}
	}
	while (!eosaf);
}

/*
 ***************************************************************************
 * Display file contents in selected format (logic #3).
 * Logic #3:	Special logic for SVG output format.
 * Formats:	SVG
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file_actlst	List of (known or unknown) activities in file.
 * @cpu_nr	Number of processors for current activity data file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @loctime	Structure where timestamp (expressed in local time) can be
 *		saved for current record.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 ***************************************************************************
 */
void logic3_display_loop(int ifd, struct file_activity *file_actlst, __nr_t cpu_nr,
			 struct tm *rectime, struct tm *loctime, char *file,
			 struct file_magic *file_magic)
{
	int i, p;
	int curr = 1, rtype, g_nr = 0;
	int eosaf = TRUE, reset = TRUE;
	long cnt = 1;
	off_t fpos;
	int graph_nr = 0;
	__nr_t save_act_nr[NR_ACT] = {0};

	/* Use a decimal point to make SVG code locale independent */
	setlocale(LC_NUMERIC, "C");

	/* Calculate the number of graphs to display */
	graph_nr = get_svg_graph_nr(ifd, file, file_magic,
				    file_actlst, rectime, loctime);
	if (!graph_nr)
		/* No graph to display */
		return;

	/* Print SVG header */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&graph_nr, F_BEGIN + F_MAIN, file, file_magic,
					     &file_hdr, cpu_nr, act, id_seq);
	}

	/*
	* If this record is a special (RESTART or COMMENT) one, ignore it and
	* (try to) get another one.
	*/
	do {
		if (read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT, 0,
				     file, &rtype, 0, file_magic, file_actlst,
				     rectime, loctime))
			/* End of sa data file */
			return;
	}
	while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
	       (tm_start.use && (datecmp(loctime, &tm_start) < 0)) ||
	       (tm_end.use && (datecmp(loctime, &tm_end) >= 0)));

	/* Save the first stats collected. Used for example in next_slice() function */
	copy_structures(act, id_seq, record_hdr, 2, 0);

	/* Save current file position */
	if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
		perror("lseek");
		exit(2);
	}
	/* Save number of activities items for current file position */
	sr_act_nr(save_act_nr, DO_SAVE);

	/* For each requested activity, display graphs */
	for (i = 0; i < NR_ACT; i++) {

		if (!id_seq[i])
			continue;

		p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
		if (!IS_SELECTED(act[p]->options) || !act[p]->g_nr)
			continue;

		if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
			display_curr_act_graphs(ifd, fpos, &curr, &cnt, &eosaf,
						p, &reset, file_actlst,
						cpu_nr, rectime, loctime, file,
						file_magic, save_act_nr, &g_nr);
		}
		else {
			unsigned int optf, msk;

			optf = act[p]->opt_flags;

			for (msk = 1; msk < 0x100; msk <<= 1) {
				if ((act[p]->opt_flags & 0xff) & msk) {
					act[p]->opt_flags &= (0xffffff00 + msk);
					display_curr_act_graphs(ifd, fpos, &curr, &cnt, &eosaf,
								p, &reset, file_actlst,
								cpu_nr, rectime, loctime, file,
								file_magic, save_act_nr, &g_nr);
					act[p]->opt_flags = optf;
				}
			}
		}
	}

	/* Print SVG trailer */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&graph_nr, F_END, file, file_magic,
					     &file_hdr, cpu_nr, act, id_seq);
	}
}

/*
 ***************************************************************************
 * Check system activity datafile contents before displaying stats.
 * Display file header if option -H has been entered, else call function
 * corresponding to selected output format.
 *
 * IN:
 * @dfile	System activity data file name.
 ***************************************************************************
 */
void read_stats_from_file(char dfile[])
{
	struct file_magic file_magic;
	struct file_activity *file_actlst = NULL;
	struct tm rectime, loctime;
	int ifd, ignore, tab = 0;
	__nr_t cpu_nr;

	/* Prepare file for reading and read its headers */
	ignore = ACCEPT_BAD_FILE_FORMAT(fmt[f_position]->options);
	check_file_actlst(&ifd, dfile, act, &file_magic, &file_hdr,
			  &file_actlst, id_seq, ignore);

	/* Now pick up number of proc for this file */
	cpu_nr = act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr;

	if (DISPLAY_HDR_ONLY(flags)) {
		if (*fmt[f_position]->f_header) {
			/* Display only data file header then exit */
			(*fmt[f_position]->f_header)(&tab, F_BEGIN + F_END, dfile, &file_magic,
						     &file_hdr, cpu_nr, act, id_seq);
		}
		exit(0);
	}

	/* Perform required allocations */
	allocate_structures(act);

	/* Call function corresponding to selected output format */
	if (format == F_SVG_OUTPUT) {
		logic3_display_loop(ifd, file_actlst, cpu_nr,
				    &rectime, &loctime, dfile, &file_magic);
	}
	else if (DISPLAY_GROUPED_STATS(fmt[f_position]->options)) {
		logic2_display_loop(ifd, file_actlst, cpu_nr,
				    &rectime, &loctime, dfile, &file_magic);
	}
	else {
		logic1_display_loop(ifd, file_actlst, dfile,
				    &file_magic, cpu_nr, &rectime, &loctime);
	}

	close(ifd);

	free(file_actlst);
	free_structures(act);
}

/*
 ***************************************************************************
 * Main entry to the sadf program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int opt = 1, sar_options = 0;
	int day_offset = 0;
	int i, rc;
	char dfile[MAX_FILE_LEN];
	char *t;

	/* Get HZ */
	get_HZ();

	/* Compute page shift in kB */
	get_kb_shift();

	dfile[0] = '\0';

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	tm_start.use = tm_end.use = FALSE;

	/* Allocate and init activity bitmaps */
	allocate_bitmaps(act);

	/* Init some structures */
	init_structures();

	/* Process options */
	while (opt < argc) {

		if (!strcmp(argv[opt], "-I")) {
			if (argv[++opt] && sar_options) {
				if (parse_sar_I_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-P")) {
			if (parse_sa_P_opt(argv, &opt, &flags, act)) {
				usage(argv[0]);
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

		else if (!strcmp(argv[opt], "-O")) {
			/* Parse SVG options */
			if (!argv[++opt] || sar_options) {
				usage(argv[0]);
			}
			for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
				if (!strcmp(t, K_SKIP_EMPTY)) {
					flags |= S_F_SVG_SKIP;
				}
				else if (!strcmp(t, K_AUTOSCALE)) {
					flags |= S_F_SVG_AUTOSCALE;
				}
				else if (!strcmp(t, K_ONEDAY)) {
					flags |= S_F_SVG_ONE_DAY;
				}
				else {
					usage(argv[0]);
				}
			}
			opt++;
		}

		else if ((strlen(argv[opt]) > 1) &&
			 (strlen(argv[opt]) < 4) &&
			 !strncmp(argv[opt], "-", 1) &&
			 (strspn(argv[opt] + 1, DIGITS) == (strlen(argv[opt]) - 1))) {
			if (dfile[0] || day_offset) {
				/* File already specified */
				usage(argv[0]);
			}
			day_offset = atoi(argv[opt++] + 1);
		}

		else if (!strcmp(argv[opt], "--")) {
			sar_options = 1;
			opt++;
		}

		else if (!strcmp(argv[opt], "-m")) {
			if (argv[++opt] && sar_options) {
				/* Parse sar's option -m */
				if (parse_sar_m_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-n")) {
			if (argv[++opt] && sar_options) {
				/* Parse sar's option -n */
				if (parse_sar_n_opt(argv, &opt, act)) {
					usage(argv[0]);
				}
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			/* Other options not previously tested */
			if (sar_options) {
				if ((rc = parse_sar_opt(argv, &opt, act, &flags, C_SADF)) != 0) {
					if (rc == 1) {
						usage(argv[0]);
					}
					exit(1);
				}
			}
			else {

				for (i = 1; *(argv[opt] + i); i++) {

					switch (*(argv[opt] + i)) {

					case 'C':
						flags |= S_F_COMMENT;
						break;

					case 'c':
						if (format) {
							usage(argv[0]);
						}
						format = F_CONV_OUTPUT;
						break;

					case 'd':
						if (format) {
							usage(argv[0]);
						}
						format = F_DB_OUTPUT;
						break;

					case 'g':
						if (format) {
							usage(argv[0]);
						}
						format = F_SVG_OUTPUT;
						break;

					case 'h':
						flags |= S_F_HORIZONTALLY;
						break;

					case 'H':
						flags |= S_F_HDR_ONLY;
						break;

					case 'j':
						if (format) {
							usage(argv[0]);
						}
						format = F_JSON_OUTPUT;
						break;

					case 'p':
						if (format) {
							usage(argv[0]);
						}
						format = F_PPC_OUTPUT;
						break;

					case 'T':
						flags |= S_F_LOCAL_TIME;
						break;

					case 't':
						flags |= S_F_TRUE_TIME;
						break;

					case 'U':
						flags |= S_F_SEC_EPOCH;
						break;

					case 'x':
						if (format) {
							usage(argv[0]);
						}
						format = F_XML_OUTPUT;
						break;

					case 'V':
						print_version();
						break;

					default:
						usage(argv[0]);
					}
				}
			}
			opt++;
		}

		/* Get data file name */
		else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
			if (dfile[0] || day_offset) {
				/* File already specified */
				usage(argv[0]);
			}
			if (!strcmp(argv[opt], "-")) {
				/* File name set to '-' */
				set_default_file(dfile, 0, -1);
				opt++;
			}
			else if (!strncmp(argv[opt], "-", 1)) {
				/* Bad option */
				usage(argv[0]);
			}
			else {
				/* Write data to file */
				strncpy(dfile, argv[opt++], MAX_FILE_LEN);
				dfile[MAX_FILE_LEN - 1] = '\0';
				/* Check if this is an alternate directory for sa files */
				check_alt_sa_dir(dfile, 0, -1);
			}
		}

		else if (interval < 0) {
			/* Get interval */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			interval = atol(argv[opt++]);
			if (interval <= 0) {
				usage(argv[0]);
			}
		}

		else {
			/* Get count value */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			if (count) {
				/* Count parameter already set */
				usage(argv[0]);
			}
			count = atol(argv[opt++]);
			if (count < 0) {
				usage(argv[0]);
			}
			else if (!count) {
				count = -1;	/* To generate a report continuously */
			}
		}
	}

	/* sadf reads current daily data file by default */
	if (!dfile[0]) {
		set_default_file(dfile, day_offset, -1);
	}

	if (tm_start.use && tm_end.use && (tm_end.tm_hour < tm_start.tm_hour)) {
		tm_end.tm_hour += 24;
	}

	if (USE_PRETTY_OPTION(flags)) {
		dm_major = get_devmap_major();
	}

	/* Options -T, -t and -U are mutually exclusive */
	if ((PRINT_LOCAL_TIME(flags) + PRINT_TRUE_TIME(flags) +
	    PRINT_SEC_EPOCH(flags)) > 1) {
		usage(argv[0]);
	}

	/*
	 * Display all the contents of the daily data file if the count parameter
	 * was not set on the command line.
	 */
	if (!count) {
		count = -1;
	}

	/* Default is CPU activity */
	select_default_activity(act);

	/* Check options consistency with selected output format. Default is PPC display */
	check_format_options();

	if (interval < 0) {
		interval = 1;
	}

	if (format == F_CONV_OUTPUT) {
		/* Convert file to current format */
		convert_file(dfile, act);
	}
	else {
		/* Read stats from file */
		read_stats_from_file(dfile);
	}

	/* Free bitmaps */
	free_bitmaps(act);

	return 0;
}
