/*
 * sadf: system activity data formatter
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
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "version.h"
#include "sadf.h"

# include <locale.h>	/* For setlocale() */
#ifdef USE_NLS
# include <libintl.h>
# define _(string) gettext(string)
#else
# define _(string) (string)
#endif

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#endif

#ifdef USE_SCCSID
#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }
#endif

#ifdef TEST
extern int __env;
void int_handler(int n) { return; }
#endif

long interval = -1, count = 0;

/* TRUE if data read from file don't match current machine's endianness */
int endian_mismatch = FALSE;
/* TRUE if file's data come from a 64 bit machine */
int arch_64 = FALSE;
/* Number of decimal places */
int dplaces_nr = -1;
/* Color palette number */
int palette = SVG_DEFAULT_COL_PALETTE;

uint64_t flags = 0;
unsigned int dm_major;		/* Device-mapper major number */
unsigned int format = 0;	/* Output format */
unsigned int f_position = 0;	/* Output format position in array */
unsigned int canvas_height = 0; /* SVG canvas height value set with option -O */
unsigned int user_hz = 0;	/* HZ value set with option -O */

/* File header */
struct file_header file_hdr;

/*
 * Activity sequence.
 * This array must always be entirely filled (even with trailing zeros).
 */
unsigned int id_seq[NR_ACT];

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
			  "[ -C ] [ -c | -d | -g | -j | -l | -p | -r | -x ] [ -H ] [ -h ] [ -T | -t | -U ] [ -V ]\n"
			  "[ -O <opts> [,...] ] [ -P { <cpu> [,...] | ALL } ]\n"
			  "[ --dev=<dev_list> ] [ --fs=<fs_list> ] [ --iface=<iface_list> ]\n"
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
 * @oformat	Output format to look for.
 *
 * RETURNS:
 * Position of output format in array.
 ***************************************************************************
 */
int get_format_position(unsigned int oformat)
{
        int i;

        for (i = 0; i < NR_FMT; i++) {
                if (fmt[i]->id == oformat)
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
	f_position = get_format_position(format);

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
		/* Remove option -T */
		flags &= ~S_F_LOCAL_TIME;
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
 * @file_magic	System activity file magic header.
 * @file_actlst	List of (known or unknown) activities in file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @oneof	Set to UEOF_CONT if an unexpected end of file should not make
 *		sadf stop. Default behavior is to stop on unexpected EOF.
 *
 * OUT:
 * @rtype	Type of record read (R_RESTART, R_COMMENT, etc.)
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on options used) has been saved for current record.
 *		If current record was a special one (RESTART or COMMENT) and
 *		noted to be ignored, then the timestamp is saved only if
 *		explicitly told to do so with the SET_TIMESTAMPS action flag.
 *
 * RETURNS:
 * 1 if EOF has been reached,
 * 2 if an unexpected EOF has been reached,
 * 0 otherwise.
 ***************************************************************************
 */
int read_next_sample(int ifd, int action, int curr, char *file, int *rtype, int tab,
		     struct file_magic *file_magic, struct file_activity *file_actlst,
		     struct tm *rectime, int oneof)
{
	int rc;
	char rec_hdr_tmp[MAX_RECORD_HEADER_SIZE];

	/* Read current record */
	if ((rc = read_record_hdr(ifd, rec_hdr_tmp, &record_hdr[curr], &file_hdr,
			    arch_64, endian_mismatch, oneof, sizeof(rec_hdr_tmp), flags)) != 0)
		/* End of sa file */
		return rc;

	*rtype = record_hdr[curr].record_type;

	if (*rtype == R_COMMENT) {
		if (action & IGNORE_COMMENT) {
			/* Ignore COMMENT record */
			if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
				if (oneof == UEOF_CONT)
					return 2;
				close(ifd);
				exit(2);
			}

			/* Ignore unknown extra structures if present */
			if (record_hdr[curr].extra_next && (skip_extra_struct(ifd, endian_mismatch, arch_64) < 0))
				return 2;

			if (action & SET_TIMESTAMPS) {
				sa_get_record_timestamp_struct(flags, &record_hdr[curr],
							       rectime);
			}
		}
		else {
			/* Display COMMENT record */
			print_special_record(&record_hdr[curr], flags, &tm_start, &tm_end,
					     *rtype, ifd, rectime, file, tab,
					     file_magic, &file_hdr, act, fmt[f_position],
					     endian_mismatch, arch_64);
		}
	}
	else if (*rtype == R_RESTART) {
		if (action & IGNORE_RESTART) {
			/*
			 * Ignore RESTART record (don't display it)
			 * but anyway we have to read the CPU number that follows it
			 * (unless we don't want to do it now).
			 */
			if (!(action & DONT_READ_CPU_NR)) {
				file_hdr.sa_cpu_nr = read_nr_value(ifd, file, file_magic,
								   endian_mismatch, arch_64, TRUE);

				/* Ignore unknown extra structures if present */
				if (record_hdr[curr].extra_next && (skip_extra_struct(ifd, endian_mismatch, arch_64) < 0))
					return 2;
			}
			if (action & SET_TIMESTAMPS) {
				sa_get_record_timestamp_struct(flags, &record_hdr[curr], rectime);
			}
		}
		else {
			/* Display RESTART record */
			print_special_record(&record_hdr[curr], flags, &tm_start, &tm_end,
					     *rtype, ifd, rectime, file, tab,
					     file_magic, &file_hdr, act, fmt[f_position],
					     endian_mismatch, arch_64);
		}
	}
	else {
		/*
		 * OK: Previous record was not a special one.
		 * So read now the extra fields.
		 */
		if (read_file_stat_bunch(act, curr, ifd, file_hdr.sa_act_nr, file_actlst,
					 endian_mismatch, arch_64, file, file_magic, oneof) > 0)
			return 2;
		sa_get_record_timestamp_struct(flags, &record_hdr[curr], rectime);
	}

	return 0;
}

/*
 ***************************************************************************
 * Display the field list (used eg. in database format).
 *
 * IN:
 * @act_id	Activity to display, or ~0 for all.
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

		if (IS_SELECTED(act[i]->options) && (act[i]->nr_ini > 0)) {
			if (!HAS_MULTIPLE_OUTPUTS(act[i]->options)) {
				printf(";%s", act[i]->hdr_line);
				if ((act[i]->nr_ini > 1) && DISPLAY_HORIZONTALLY(flags)) {
					printf("[...]");
				}
			}
			else {
				msk = 1;
				strncpy(hline, act[i]->hdr_line, sizeof(hline) - 1);
				hline[sizeof(hline) - 1] = '\0';
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
						if ((act[i]->nr_ini > 1) && DISPLAY_HORIZONTALLY(flags)) {
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
	struct tm ltm;
	time_t t;

	if (DISPLAY_ONE_DAY(flags)) {
		localtime_r((time_t *) &(record_hdr[2].ust_time), &ltm);

		/* Move back to midnight */
		ltm.tm_sec = ltm.tm_min = ltm.tm_hour = 0;

		t = mktime(&ltm);
		if (t != -1)
			return t;
	}

	return (time_t) record_hdr[2].ust_time;
}

/*
 ***************************************************************************
 * Save or restore position in file.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @action	DO_SAVE to save position or DO_RESTORE to restore it.
 ***************************************************************************
 */
void seek_file_position(int ifd, int action)
{
	static off_t fpos = -1;
	static unsigned int save_cpu_nr = 0;

	if (action == DO_SAVE) {
		/* Save current file position */
		if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
			perror("lseek");
			exit(2);
		}
		save_cpu_nr = file_hdr.sa_cpu_nr;
	}
	else if (action == DO_RESTORE) {
		/* Rewind file */
		if ((fpos < 0) || (lseek(ifd, fpos, SEEK_SET) < fpos)) {
			perror("lseek");
			exit(2);
		}
		file_hdr.sa_cpu_nr = save_cpu_nr;
	}
}

/*
 ***************************************************************************
 * Count number of different items in file. Save these numbers in fields
 * @item_list_sz of structure activity, and create the corresponding list
 * in field @item_list.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 * @file_actlst	List of (known or unknown) activities in file.
 * @rectime	Structure where timestamp (expressed in local time or
 *		in UTC depending on whether options -T/-t have been
 *		used or not) can be saved for current record.
 *
 * RETURNS:
 * 0 if no records are concerned in file, and 1 otherwise.
 ***************************************************************************
 */
int count_file_items(int ifd, char *file, struct file_magic *file_magic,
		      struct file_activity *file_actlst, struct tm *rectime)
{
	int i, eosaf, rtype;

	/* Save current file position */
	seek_file_position(ifd, DO_SAVE);

	/* Init maximum number of items for each activity */
	for (i = 0; i < NR_ACT; i++) {
		if (!HAS_LIST_ON_CMDLINE(act[i]->options)) {
			act[i]->item_list_sz = 0;
		}
	}

	/* Look for the first record that will be displayed */
	do {
		eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
					 0, file, &rtype, 0, file_magic, file_actlst,
					 rectime, UEOF_CONT);
		if (eosaf)
			/* No record to display */
			return 0;
	}
	while ((tm_start.use && (datecmp(rectime, &tm_start, FALSE) < 0)) ||
	       (tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0)));

	/*
	 * Read all the file and determine the maximum number
	 * of items for each activity.
	 */
	do {
		for (i = 0; i < NR_ACT; i++) {
			if (!HAS_LIST_ON_CMDLINE(act[i]->options)) {
				if (act[i]->f_count_new) {
					act[i]->item_list_sz += (*act[i]->f_count_new)(act[i], 0);
				}
				else if (act[i]->nr[0] > act[i]->item_list_sz) {
					act[i]->item_list_sz = act[i]->nr[0];
				}
			}
		}

		do {
			eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
						 0, file, &rtype, 0, file_magic, file_actlst,
						 rectime, UEOF_CONT);
			if (eosaf ||
			    (tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0)))
				/* End of data file or end time exceeded */
				break;
		}
		while ((rtype == R_RESTART) || (rtype == R_COMMENT));
	}
	while (!eosaf && !(tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0)));

	/* Rewind file */
	seek_file_position(ifd, DO_RESTORE);

	return 1;
}

/*
 ***************************************************************************
 * Compute the number of rows that will contain SVG views. Usually only one
 * view is displayed on a row, unless the "packed" option has been entered.
 * Each activity selected may have several views. Moreover some activities
 * may have a number of items that varies within the file: In this case,
 * the number of views will depend on the highest number of items saved in
 * the file.
 *
 * IN:
 * @ifd			File descriptor of input file.
 * @file		Name of file being read.
 * @file_magic		file_magic structure filled with file magic header data.
 * @file_actlst		List of (known or unknown) activities in file.
 * @rectime		Structure where timestamp (expressed in local time or
 *			in UTC depending on whether options -T/-t have been
 *			used or not) can be saved for current record.
 * @views_per_row	Default number of views displayed on a single row.
 *
 * OUT:
 * @views_per_row	Maximum number of views that will be displayed on a
 *			single row (useful only if "packed" option entered).
 * @nr_act_dispd	Number of activities that will be displayed.
 *			May be 0.
 *
 * RETURNS:
 * Number of rows containing views, taking into account only activities
 * to be displayed, and selected period of time (options -s/-e).
 * Result may be 0.
 ***************************************************************************
 */
int get_svg_graph_nr(int ifd, char *file, struct file_magic *file_magic,
		     struct file_activity *file_actlst, struct tm *rectime,
		     int *views_per_row, int *nr_act_dispd)
{
	int i, n, p, tot_g_nr = 0;

	*nr_act_dispd = 0;

	/* Count items in file */
	if (!count_file_items(ifd, file, file_magic, file_actlst, rectime))
		/* No record to display => No graph */
		return 0;

	for (i = 0; i < NR_ACT; i++) {
		if (!id_seq[i])
			continue;

		p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
		if (!IS_SELECTED(act[p]->options) || !act[p]->g_nr)
			continue;

		(*nr_act_dispd)++;

		if (PACK_VIEWS(flags)) {
			/*
			 * One activity = one row with multiple views.
			 * Exception is A_MEMORY, for which one activity may be
			 * displayed in two rows if both memory *and* swap utilization
			 * have been selected.
			 */
			if ((act[p]->id == A_MEMORY) &&
			    (DISPLAY_MEMORY(act[p]->opt_flags) && DISPLAY_SWAP(act[p]->opt_flags))) {
				n = 2;
			}
			else {
				n = 1;
			}
		}
		else {
			/* One activity = multiple rows with only one view */
			n = act[p]->g_nr;
		}
		if (ONE_GRAPH_PER_ITEM(act[p]->options)) {
			 n = n * act[p]->item_list_sz;
		}
		if (act[p]->g_nr > *views_per_row) {
			*views_per_row = act[p]->g_nr;
		}

		tot_g_nr += n;
	}

	if (*views_per_row > MAX_VIEWS_ON_A_ROW) {
		*views_per_row = MAX_VIEWS_ON_A_ROW;
	}

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
 * @rectime		Structure where timestamp (expressed in local time
 *			or in UTC depending on whether options -T/-t have
 * 			been used or not) has been saved for current record.
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
			long *cnt, void *parm, struct tm *rectime,
			int reset_cd, unsigned int act_id)
{
	int i;
	unsigned long long dt, itv;
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
	if (!next_slice(record_hdr[2].uptime_cs, record_hdr[curr].uptime_cs,
			reset, interval))
		/* Not close enough to desired interval */
		return 0;

	/* Check if we are beginning a new day */
	if (use_tm_start && record_hdr[!curr].ust_time &&
	    (record_hdr[curr].ust_time > record_hdr[!curr].ust_time) &&
	    (record_hdr[curr].hour < record_hdr[!curr].hour)) {
		cross_day = TRUE;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(rectime, &tm_start, cross_day) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval values in 1/100th of a second */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr], &itv);

	/* Check time (3) */
	if (use_tm_end && (datecmp(rectime, &tm_end, cross_day) > 0)) {
		/* It's too late... */
		*cnt = 0;
		return 0;
	}

	dt = itv / 100;
	/* Correct rounding error for dt */
	if ((itv % 100) >= 50) {
		dt++;
	}

	/* Set date and time strings for current record */
	set_record_timestamp_string(flags, &record_hdr[curr],
				    cur_date, cur_time, TIMESTAMP_LEN, rectime);

	if (*fmt[f_position]->f_timestamp) {
		pre = (char *) (*fmt[f_position]->f_timestamp)(parm, F_BEGIN, cur_date, cur_time, dt,
							       &record_hdr[curr], &file_hdr, flags);
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
										dt, &record_hdr[curr],
										&file_hdr, flags);
					}
				}
				(*act[i]->f_json_print)(act[i], curr, *tab, itv);
			}

			else if (format == F_XML_OUTPUT) {
				/* XML output */
				int *tab = (int *) parm;

				(*act[i]->f_xml_print)(act[i], curr, *tab, itv);
			}

			else if (format == F_SVG_OUTPUT) {
				/* SVG output */
				struct svg_parm *svg_p = (struct svg_parm *) parm;

				svg_p->dt = (unsigned long) dt;
				(*act[i]->f_svg_print)(act[i], curr, F_MAIN, svg_p, itv, &record_hdr[curr]);
			}

			else if (format == F_RAW_OUTPUT) {
				/* Raw output */
				if (DISPLAY_DEBUG_MODE(flags)) {
					printf("# name; %s; nr_curr; %d; nr_alloc; %d; nr_ini; %d\n", act[i]->name,
					       act[i]->nr[curr], act[i]->nr_allocated, act[i]->nr_ini);
				}

				(*act[i]->f_raw_print)(act[i], pre, curr);
			}

			else if (format == F_PCP_OUTPUT) {
				/* PCP archive */
				if (*act[i]->f_pcp_print) {
					(*act[i]->f_pcp_print)(act[i], curr, itv, &record_hdr[curr]);
				}
			}

			else {
				/* Other output formats: db, ppc */
				(*act[i]->f_render)(act[i], (format == F_DB_OUTPUT), pre, curr, itv);
			}
		}
	}

	if (*fmt[f_position]->f_timestamp) {
		(*fmt[f_position]->f_timestamp)(parm, F_END, cur_date, cur_time, dt,
						&record_hdr[curr], &file_hdr, flags);
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
 * @curr	Index in array for current sample statistics.
 * @act_id	Activity to display, or ~0 for all.
 * @file_actlst	List of (known or unknown) activities in file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
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
void rw_curr_act_stats(int ifd, int *curr, long *cnt, int *eosaf,
		       unsigned int act_id, int *reset, struct file_activity *file_actlst,
		       struct tm *rectime, char *file,
		       struct file_magic *file_magic)
{
	int rtype;
	int next, reset_cd;

	/* Rewind file */
	seek_file_position(ifd, DO_RESTORE);

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
		*eosaf = read_next_sample(ifd, IGNORE_RESTART | DONT_READ_CPU_NR,
					  *curr, file, &rtype, 0, file_magic,
					  file_actlst, rectime, UEOF_STOP);

		if (!*eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
			next = generic_write_stats(*curr, tm_start.use, tm_end.use, *reset, cnt,
						   NULL, rectime, reset_cd, act_id);
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
 * @curr	Index in array for current sample statistics.
 * @p		Current activity position.
 * @file_actlst	List of (known or unknown) activities in file.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @file	Name of file being read.
 * @file_magic	file_magic structure filled with file magic header data.
 * @g_nr	Number of graphs already displayed (for all activities).
 * @nr_act_dispd
 *		Total number of activities that will be displayed.
 *
 * OUT:
 * @cnt		Number of lines of stats remaining to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @reset	Set to TRUE if last_uptime variable should be
 *		reinitialized (used in next_slice() function).
 * @g_nr	Total number of views displayed (including current activity).
 ***************************************************************************
 */
void display_curr_act_graphs(int ifd, int *curr, long *cnt, int *eosaf,
			     int p, int *reset, struct file_activity *file_actlst,
			     struct tm *rectime, char *file, struct file_magic *file_magic,
			     int *g_nr, int nr_act_dispd)
{
	struct svg_parm parm;
	int rtype;
	int next, reset_cd;

	/* Rewind file */
	seek_file_position(ifd, DO_RESTORE);

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	parm.graph_no = *g_nr;
	parm.ust_time_ref = (unsigned long long) get_time_ref();
	parm.ust_time_first = record_hdr[2].ust_time;
	parm.restart = TRUE;
	parm.file_hdr = &file_hdr;
	parm.nr_act_dispd = nr_act_dispd;

	*cnt  = count;
	reset_cd = 1;

	/* Allocate graphs arrays */
	(*act[p]->f_svg_print)(act[p], !*curr, F_BEGIN, &parm, 0, &record_hdr[!*curr]);

	do {
		*eosaf = read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT | SET_TIMESTAMPS,
					  *curr, file, &rtype, 0, file_magic,
					  file_actlst, rectime, UEOF_CONT);

		if (!*eosaf && (rtype != R_COMMENT) && (rtype != R_RESTART)) {

			next = generic_write_stats(*curr, tm_start.use, tm_end.use, *reset, cnt,
						   &parm, rectime, reset_cd, act[p]->id);
			reset_cd = 0;
			if (next) {
				/*
				 * next is set to 1 when we were close enough to desired interval.
				 * In this case, the call to generic_write_stats() has actually
				 * displayed a line of stats.
				 */
				parm.restart = FALSE;
				parm.ust_time_end = record_hdr[*curr].ust_time;
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
							  file_actlst, rectime, UEOF_CONT);
			}
			while (!*eosaf && ((rtype == R_RESTART) || (rtype == R_COMMENT)));

			*curr ^= 1;
		}
	}
	while (!*eosaf);

	*reset = TRUE;

	/* Determine X axis end value */
	if (DISPLAY_ONE_DAY(flags) &&
	    (parm.ust_time_ref + (3600 * 24) > parm.ust_time_end)) {
		parm.ust_time_end = parm.ust_time_ref + (3600 * 24);
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
 * Formats:	XML, JSON, PCP
 *
 * NB: all statistics data will be sorted by timestamp.
 * Example: If stats for activities A and B at time t and t' have been collected,
 * the output will be:
 * stats for activity A at t
 * stats for activity B at t
 * stats for activity A at t'
 * stats for activity B at t'
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file	System activity data file name (name of file being read).
 * @file_actlst	List of (known or unknown) activities in file.
 * @file_magic	System activity file magic header.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @dparm	PCP archive file name.
 ***************************************************************************
 */
void logic1_display_loop(int ifd, char *file, struct file_activity *file_actlst,
			 struct file_magic *file_magic, struct tm *rectime, void *dparm)
{
	int curr, rtype, tab = 0;
	int eosaf, next, reset = FALSE;
	int ign_flag = IGNORE_COMMENT + IGNORE_RESTART;
	long cnt = 1;
	char *pcparchive = (char *) dparm;

	if (CREATE_ITEM_LIST(fmt[f_position]->options)) {
		/* Count items in file (e.g. for PCP output) */
		if (!count_file_items(ifd, file, file_magic, file_actlst, rectime))
			/* No record to display */
			return;
	}
	/* Save current file position */
	seek_file_position(ifd, DO_SAVE);

	/* Print header (eg. XML file header) */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&tab, F_BEGIN, pcparchive, file_magic,
					     &file_hdr, act, id_seq, file_actlst);
	}

	if (ORDER_ALL_RECORDS(fmt[f_position]->options)) {
		ign_flag = IGNORE_NOTHING;

		/* RESTART and COMMENTS records will be immediately processed */
		if (*fmt[f_position]->f_restart) {
			(*fmt[f_position]->f_restart)(&tab, F_BEGIN, NULL, NULL, FALSE,
						      &file_hdr, NULL);
		}
		if (DISPLAY_COMMENT(flags) && (*fmt[f_position]->f_comment)) {
			(*fmt[f_position]->f_comment)(&tab, F_BEGIN, NULL, NULL, 0, NULL,
						      &file_hdr, NULL);
		}
	}

	/* Process activities */
	if (*fmt[f_position]->f_statistics) {
		(*fmt[f_position]->f_statistics)(&tab, F_BEGIN, act, id_seq);
	}

	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one,
		 * process it then try to read the next record in file.
		 */
		do {
			eosaf = read_next_sample(ifd, ign_flag, 0, file,
						 &rtype, tab, file_magic, file_actlst,
						 rectime, UEOF_STOP);
		}
		while (!eosaf && ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
			(tm_start.use && (datecmp(rectime, &tm_start, FALSE) < 0)) ||
			(tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0))));

		curr = 1;
		cnt = count;
		reset = TRUE;

		if (!eosaf) {

			/* Save the first stats collected. Used for example in next_slice() function */
			copy_structures(act, id_seq, record_hdr, 2, 0);

			do {
				eosaf = read_next_sample(ifd, ign_flag, curr, file,
							 &rtype, tab, file_magic, file_actlst,
							 rectime, UEOF_CONT);

				if (!eosaf && (rtype != R_COMMENT) && (rtype != R_RESTART)) {
					if (*fmt[f_position]->f_statistics) {
						(*fmt[f_position]->f_statistics)(&tab, F_MAIN, act, id_seq);
					}

					/* next is set to 1 when we were close enough to desired interval */
					next = generic_write_stats(curr, tm_start.use, tm_end.use, reset,
								  &cnt, &tab, rectime, FALSE, ALL_ACTIVITIES);

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
					eosaf = read_next_sample(ifd, ign_flag, curr, file,
								 &rtype, tab, file_magic, file_actlst,
								 rectime, UEOF_CONT);
				}
				while (!eosaf && (rtype != R_RESTART));
			}
			reset = TRUE;
		}
	}
	while (!eosaf);

	if (*fmt[f_position]->f_statistics) {
		(*fmt[f_position]->f_statistics)(&tab, F_END, act, id_seq);
	}

	if (ign_flag == IGNORE_NOTHING) {
		/*
		 * RESTART and COMMENT records have already been processed.
		 * Display possible trailing data then terminate.
		 */
		if (*fmt[f_position]->f_restart) {
			(*fmt[f_position]->f_restart)(&tab, F_END, NULL, NULL,
						      FALSE, &file_hdr, NULL);
		}
		if (DISPLAY_COMMENT(flags) && (*fmt[f_position]->f_comment)) {
			(*fmt[f_position]->f_comment)(&tab, F_END, NULL, NULL, 0, NULL,
						      &file_hdr, NULL);
		}
		goto terminate;
	}

	/* Rewind file */
	seek_file_position(ifd, DO_RESTORE);

	/* Process now RESTART entries to display restart messages */
	if (*fmt[f_position]->f_restart) {
		(*fmt[f_position]->f_restart)(&tab, F_BEGIN, NULL, NULL, FALSE,
					      &file_hdr, NULL);
	}

	do {
		eosaf = read_next_sample(ifd, IGNORE_COMMENT, 0,
					 file, &rtype, tab, file_magic, file_actlst,
					 rectime, UEOF_CONT);
	}
	while (!eosaf);

	if (*fmt[f_position]->f_restart) {
		(*fmt[f_position]->f_restart)(&tab, F_END, NULL, NULL, FALSE, &file_hdr, NULL);
	}

	/* Rewind file */
	seek_file_position(ifd, DO_RESTORE);

	/* Last, process COMMENT entries to display comments */
	if (DISPLAY_COMMENT(flags)) {
		if (*fmt[f_position]->f_comment) {
			(*fmt[f_position]->f_comment)(&tab, F_BEGIN, NULL, NULL, 0, NULL,
						      &file_hdr, NULL);
		}
		do {
			eosaf = read_next_sample(ifd, IGNORE_RESTART, 0,
						 file, &rtype, tab, file_magic, file_actlst,
						 rectime, UEOF_CONT);
		}
		while (!eosaf);

		if (*fmt[f_position]->f_comment) {
			(*fmt[f_position]->f_comment)(&tab, F_END, NULL, NULL, 0, NULL,
						      &file_hdr, NULL);
		}
	}

terminate:
	/* Print header trailer */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&tab, F_END, pcparchive, file_magic,
					     &file_hdr, act, id_seq, file_actlst);
	}
}

/*
 ***************************************************************************
 * Display file contents in selected format (logic #2).
 * Logic #2:	Grouped by activity. Sorted by timestamp. Stop on RESTART
 * 		records.
 * Formats:	ppc, CSV, raw
 *
 * NB: All statistics data for one activity will be displayed before
 * displaying stats for next activity. This is what sar does in its report.
 * Example: If stats for activities A and B at time t and t' have been collected,
 * the output will be:
 * stats for activity A at t
 * stats for activity A at t'
 * stats for activity B at t
 * stats for activity B at t'
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file	Name of file being read.
 * @file_actlst	List of (known or unknown) activities in file.
 * @file_magic	file_magic structure filled with file magic header data.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @dparm	Unused here.
 ***************************************************************************
 */
void logic2_display_loop(int ifd, char *file, struct file_activity *file_actlst,
			 struct file_magic *file_magic, struct tm *rectime, void *dparm)
{
	int i, p;
	int curr = 1, rtype;
	int eosaf = TRUE, reset = FALSE;
	long cnt = 1;

	/* Read system statistics from file */
	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one, print it and
		 * (try to) get another one.
		 */
		do {
			if (read_next_sample(ifd, IGNORE_NOTHING, 0,
					     file, &rtype, 0, file_magic, file_actlst,
					     rectime, UEOF_STOP))
				/* End of sa data file */
				return;
		}
		while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
		       (tm_start.use && (datecmp(rectime, &tm_start, FALSE) < 0)) ||
		       (tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0)));

		/* Save the first stats collected. Used for example in next_slice() function */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		/* Set flag to reset last_uptime variable. Should be done after a LINUX RESTART record */
		reset = TRUE;

		/* Save current file position */
		seek_file_position(ifd, DO_SAVE);

		/* Read and write stats located between two possible Linux restarts */

		if (DISPLAY_HORIZONTALLY(flags)) {
			/*
			 * If stats are displayed horizontally, then all activities
			 * are printed on the same line.
			 */
			rw_curr_act_stats(ifd, &curr, &cnt, &eosaf,
					  ALL_ACTIVITIES, &reset, file_actlst,
					  rectime, file, file_magic);
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
					rw_curr_act_stats(ifd, &curr, &cnt, &eosaf,
							  act[p]->id, &reset, file_actlst,
							  rectime, file, file_magic);
				}
				else {
					unsigned int optf, msk;

					optf = act[p]->opt_flags;

					for (msk = 1; msk < 0x100; msk <<= 1) {
						if ((act[p]->opt_flags & 0xff) & msk) {
							act[p]->opt_flags &= (0xffffff00 + msk);

							rw_curr_act_stats(ifd, &curr, &cnt, &eosaf,
									  act[p]->id, &reset, file_actlst,
									  rectime, file,
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
				eosaf = read_next_sample(ifd, IGNORE_RESTART | DONT_READ_CPU_NR,
							 curr, file, &rtype, 0, file_magic,
							 file_actlst, rectime, UEOF_STOP);
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
					     R_RESTART, ifd, rectime, file, 0,
					     file_magic, &file_hdr, act, fmt[f_position],
					     endian_mismatch, arch_64);
		}
	}
	while (!eosaf);
}

/*
 ***************************************************************************
 * Display file contents in SVG format.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file	Name of file being read.
 * @file_actlst	List of (known or unknown) activities in file.
 * @file_magic	file_magic structure filled with file magic header data.
 * @rectime	Structure where timestamp (expressed in local time or in UTC
 *		depending on whether options -T/-t have been used or not) can
 *		be saved for current record.
 * @dparm	Unused here.
 ***************************************************************************
 */
void svg_display_loop(int ifd, char *file, struct file_activity *file_actlst,
		      struct file_magic *file_magic, struct tm *rectime, void *dparm)
{
	struct svg_hdr_parm parm;
	int i, p;
	int curr = 1, rtype, g_nr = 0, views_per_row = 1, nr_act_dispd;
	int eosaf = TRUE, reset = TRUE;
	long cnt = 1;
	int graph_nr = 0;

	/* Init custom colors palette */
	init_custom_color_palette();

	/*
	 * Calculate the number of rows and the max number of views per row to display.
	 * Result may be 0. In this case, "No data" will be displayed instead of the graphs.
	 */
	graph_nr = get_svg_graph_nr(ifd, file, file_magic,
				    file_actlst, rectime, &views_per_row, &nr_act_dispd);

	if (SET_CANVAS_HEIGHT(flags)) {
		/*
		 * Option "-O height=..." used: @graph_nr is NO LONGER a number
		 * of graphs but the SVG canvas height set on the command line.
		 */
		graph_nr = canvas_height;
	}


	parm.graph_nr = graph_nr;
	parm.views_per_row = PACK_VIEWS(flags) ? views_per_row : 1;
	parm.nr_act_dispd = nr_act_dispd;

	/* Print SVG header */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&parm, F_BEGIN + F_MAIN, file, file_magic,
					     &file_hdr, act, id_seq, file_actlst);
	}

	/*
	* If this record is a special (RESTART or COMMENT) one, ignore it and
	* (try to) get another one.
	*/
	do {
		if (read_next_sample(ifd, IGNORE_RESTART | IGNORE_COMMENT, 0,
				     file, &rtype, 0, file_magic, file_actlst,
				     rectime, UEOF_CONT))
		{
			/* End of sa data file: No views displayed */
			parm.graph_nr = 0;
			goto close_svg;
		}
	}
	while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
	       (tm_start.use && (datecmp(rectime, &tm_start, FALSE) < 0)) ||
	       (tm_end.use && (datecmp(rectime, &tm_end, FALSE) >= 0)));

	/* Save the first stats collected. Used for example in next_slice() function */
	copy_structures(act, id_seq, record_hdr, 2, 0);

	/* Save current file position */
	seek_file_position(ifd, DO_SAVE);

	/* For each requested activity, display graphs */
	for (i = 0; i < NR_ACT; i++) {

		if (!id_seq[i])
			continue;

		p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
		if (!IS_SELECTED(act[p]->options) || !act[p]->g_nr)
			continue;

		if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
			display_curr_act_graphs(ifd, &curr, &cnt, &eosaf,
						p, &reset, file_actlst,
						rectime, file,
						file_magic, &g_nr, nr_act_dispd);
		}
		else {
			unsigned int optf, msk;

			optf = act[p]->opt_flags;

			for (msk = 1; msk < 0x100; msk <<= 1) {
				if ((act[p]->opt_flags & 0xff) & msk) {
					act[p]->opt_flags &= (0xffffff00 + msk);
					display_curr_act_graphs(ifd, &curr, &cnt, &eosaf,
								p, &reset, file_actlst,
								rectime, file,
								file_magic, &g_nr, nr_act_dispd);
					act[p]->opt_flags = optf;
				}
			}
		}
	}

	/* Real number of graphs that have been displayed */
	parm.graph_nr = g_nr;

close_svg:
	/* Print SVG trailer */
	if (*fmt[f_position]->f_header) {
		(*fmt[f_position]->f_header)(&parm, F_END, file, file_magic,
					     &file_hdr, act, id_seq, file_actlst);
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
 * @pcparchive	PCP archive file name.
 ***************************************************************************
 */
void read_stats_from_file(char dfile[], char pcparchive[])
{
	struct file_magic file_magic;
	struct file_activity *file_actlst = NULL;
	struct tm rectime;
	int ifd, tab = 0;

	/* Prepare file for reading and read its headers */
	check_file_actlst(&ifd, dfile, act, flags, &file_magic, &file_hdr,
			  &file_actlst, id_seq, &endian_mismatch, &arch_64);

	if (DISPLAY_HDR_ONLY(flags)) {
		if (*fmt[f_position]->f_header) {
			if (format == F_PCP_OUTPUT) {
				dfile = pcparchive;
			}
			/* Display only data file header then exit */
			(*fmt[f_position]->f_header)(&tab, F_BEGIN + F_END, dfile, &file_magic,
						     &file_hdr, act, id_seq, file_actlst);
		}
		exit(0);
	}

	/* Perform required allocations */
	allocate_structures(act);

	if (SET_LC_NUMERIC_C(fmt[f_position]->options)) {
		/* Use a decimal point */
		setlocale(LC_NUMERIC, "C");
	}

	/* Call function corresponding to selected output format */
	if (*fmt[f_position]->f_display) {
		(*fmt[f_position]->f_display)(ifd, dfile, file_actlst, &file_magic,
					      &rectime, pcparchive);
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
	int i, rc, p, q;
	char dfile[MAX_FILE_LEN], pcparchive[MAX_FILE_LEN];
	char *t, *v;

	/* Compute page shift in kB */
	get_kb_shift();

	dfile[0] = pcparchive[0] = '\0';

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
			if (!sar_options) {
				usage(argv[0]);
			}
			if (parse_sar_I_opt(argv, &opt, &flags, act)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-P")) {
			if (parse_sa_P_opt(argv, &opt, &flags, act)) {
				usage(argv[0]);
			}
		}

		else if (!strncmp(argv[opt], "--dev=", 6)) {
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

#ifdef TEST
		else if (!strncmp(argv[opt], "--getenv", 8)) {
			__env = TRUE;
			opt++;
		}
#endif

		else if (!strcmp(argv[opt], "-O")) {
			/* Parse output options */
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
				else if (!strcmp(t, K_SHOWIDLE)) {
					flags |= S_F_SVG_SHOW_IDLE;
				}
				else if (!strcmp(t, K_SHOWINFO)) {
					flags |= S_F_SVG_SHOW_INFO;
				}
				else if (!strcmp(t, K_DEBUG)) {
					flags |= S_F_RAW_DEBUG_MODE;
				}
				else if (!strncmp(t, K_HEIGHT, strlen(K_HEIGHT))) {
					v = t + strlen(K_HEIGHT);
					if (!strlen(v) || (strspn(v, DIGITS) != strlen(v))) {
						usage(argv[0]);
					}
					canvas_height = atoi(v);
					flags |= S_F_SVG_HEIGHT;
				}
				else if (!strcmp(t, K_PACKED)) {
					flags |= S_F_SVG_PACKED;
				}
				else if (!strcmp(t, K_SHOWTOC)) {
					flags |= S_F_SVG_SHOW_TOC;
				}
				else if (!strcmp(t, K_CUSTOMCOL)) {
					palette = SVG_CUSTOM_COL_PALETTE;
				}
				else if (!strcmp(t, K_BWCOL)) {
					palette = SVG_BW_COL_PALETTE;
				}
				else if (!strncmp(t, K_PCPARCHIVE, strlen(K_PCPARCHIVE))) {
					v = t + strlen(K_PCPARCHIVE);
					strncpy(pcparchive, v, sizeof(pcparchive));
					pcparchive[sizeof(pcparchive) - 1] = '\0';
				}
				else if (!strncmp(t, K_HZ, strlen(K_HZ))) {
					v = t + strlen(K_HZ);
					if (!strlen(v) || (strspn(v, DIGITS) != strlen(v))) {
						usage(argv[0]);
					}
					user_hz = atoi(v);
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
			if (!argv[++opt] || !sar_options) {
				usage(argv[0]);
			}
			/* Parse sar's option -m */
			if (parse_sar_m_opt(argv, &opt, act)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-n")) {
			if (!argv[++opt] || !sar_options) {
				usage(argv[0]);
			}
			/* Parse sar's option -n */
			if (parse_sar_n_opt(argv, &opt, act)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-q")) {
			if (!sar_options) {
				usage(argv[0]);
			}
			else if (!argv[++opt]) {
				SELECT_ACTIVITY(A_QUEUE);
			}
			/* Parse option -q */
			else if (parse_sar_q_opt(argv, &opt, act)) {
				SELECT_ACTIVITY(A_QUEUE);
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

					case 'l':
						if (format) {
							usage(argv[0]);
						}
						format = F_PCP_OUTPUT;
						break;

					case 'p':
						if (format) {
							usage(argv[0]);
						}
						format = F_PPC_OUTPUT;
						break;

					case 'r':
						if (format) {
							usage(argv[0]);
						}
						format = F_RAW_OUTPUT;
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
			/* Write data to file */
			strncpy(dfile, argv[opt++], sizeof(dfile));
			dfile[sizeof(dfile) - 1] = '\0';
			/* Check if this is an alternate directory for sa files */
			check_alt_sa_dir(dfile, 0, -1);
		}

		else if (interval < 0) {
			/* Get interval */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			interval = atol(argv[opt++]);
			if (interval < 1) {
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

	if (USE_OPTION_A(flags)) {
		/* Set -P ALL -I ALL if needed */
		set_bitmaps(act, &flags);
	}

	/* sadf reads current daily data file by default */
	if (!dfile[0]) {
		set_default_file(dfile, day_offset, -1);
	}

	/* PCP mode: If no archive file specified then use the name of the daily data file */
	if (!pcparchive[0] && (format == F_PCP_OUTPUT)) {
		strcpy(pcparchive, dfile);
	}

#ifndef HAVE_PCP
	if (format == F_PCP_OUTPUT) {
		fprintf(stderr, _("PCP support not compiled in\n"));
		exit(1);
	}
#endif

	if (tm_start.use && tm_end.use && (tm_end.tm_hour < tm_start.tm_hour)) {
		tm_end.tm_hour += 24;
	}

	if (DISPLAY_PRETTY(flags)) {
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
		read_stats_from_file(dfile, pcparchive);
	}

	/* Free bitmaps */
	free_bitmaps(act);

	return 0;
}
