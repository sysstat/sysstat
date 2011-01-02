/*
 * sadf: system activity data formatter
 * (C) 1999-2011 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "version.h"
#include "sadf.h"
#include "sa.h"
#include "common.h"
#include "ioconf.h"
#include "rndr_stats.h"
#include "xml_stats.h"

#ifdef USE_NLS
# include <locale.h>
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

/* File header */
struct file_header file_hdr;

static char *seps[] =  {"\t", ";"};

/*
 * Activity sequence.
 * This array must always be entirely filled (even with trailing zeros).
 */
unsigned int id_seq[NR_ACT];

/* Current record header */
struct record_header record_hdr[3];

struct tm rectime, loctime;
/* Contain the date specified by -s and -e options */
struct tstamp tm_start, tm_end;
char *args[MAX_ARGV_NR];

extern struct activity *act[];

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
		_("Usage: %s [ options ] [ <interval> [ <count> ] ] [ <datafile> ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -d | -D | -H | -p | -x ] [ -C ] [ -h ] [ -t ] [ -V ]\n"
			  "[ -P { <cpu> [,...] | ALL } ] [ -s [ <hh:mm:ss> ] ] [ -e [ <hh:mm:ss> ] ]\n"
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
 * Fill the rectime structure with current record's date and time, based on
 * current record's "number of seconds since the epoch" saved in file.
 * The resulting timestamp is expressed in UTC or in local time, depending
 * on whether option -t has been used or not.
 * NB: Option -t is ignored when option -p is used, since option -p
 * displays its timestamp as a long integer. This is type 'time_t',
 * which is the number of seconds since 1970 _always_ expressed in UTC.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
*/
void sadf_get_record_timestamp_struct(int curr)
{
	struct tm *ltm;

	if ((ltm = localtime((const time_t *) &record_hdr[curr].ust_time)) != NULL) {
		loctime = *ltm;
	}

	if (!PRINT_TRUE_TIME(flags) ||
	    ((format != S_O_DB_OPTION) && (format != S_O_XML_OPTION))) {
		/* Option -t is valid only with options -d and -x */
		ltm = gmtime((const time_t *) &record_hdr[curr].ust_time);
	}

	if (ltm) {
		rectime = *ltm;
	}
}

/*
 ***************************************************************************
 * Set current record's timestamp string. This timestamp is expressed in
 * UTC or in local time, depending on whether option -t has been used or
 * not.
 * NB: If options -D or -p have been used, the timestamp in expressed in
 * seconds since 1970.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @len		Maximum length of timestamp string.
 *
 * OUT:
 * @cur_time	Timestamp string.
 ***************************************************************************
*/
void set_record_timestamp_string(int curr, char *cur_time, int len)
{
	/* Fill timestamp structure */
	sadf_get_record_timestamp_struct(curr);

	/* Set cur_time date value */
	if (format == S_O_DB_OPTION) {
		if (PRINT_TRUE_TIME(flags)) {
			strftime(cur_time, len, "%Y-%m-%d %H:%M:%S", &rectime);
		}
		else {
			strftime(cur_time, len, "%Y-%m-%d %H:%M:%S UTC", &rectime);
		}
	}
	else if ((format == S_O_PPC_OPTION) || (format == S_O_DBD_OPTION)) {
		sprintf(cur_time, "%ld", record_hdr[curr].ust_time);
	}
}

/*
 ***************************************************************************
 * Display the field list (when displaying stats in DB format)
 *
 * IN:
 * @act_d	Activity to display, or ~0 for all.
 ***************************************************************************
 */
void list_fields(unsigned int act_id)
{
	int i;
	unsigned int msk;
	char *hl;
	char hline[HEADER_LINE_LEN];
	
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
				strcpy(hline, act[i]->hdr_line);
				for (hl = strtok(hline, "|"); hl; hl = strtok(NULL, "|"), msk <<= 1) {
					if ((hl != NULL) && (act[i]->opt_flags & msk)) {
						printf(";%s", hl);
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
 * write_mech_stats() -
 * Replace the old write_stats_for_ppc() and write_stats_for_db(),
 * making it easier for them to remain in sync and print the same data.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dt		Interval of time in seconds.
 * @itv		Interval of time in jiffies.
 * @g_itv	Interval of time in jiffies multiplied by the number of
 * 		processors.
 * @cur_time	Current timestamp.
 * @act_id	Activity to display, or ~0 for all.
 ***************************************************************************
 */
void write_mech_stats(int curr, unsigned long dt, unsigned long long itv,
		      unsigned long long g_itv, char *cur_time, unsigned int act_id)
{
	int i;
	char pre[80];	/* Text at beginning of each line */
	int isdb = ((format == S_O_DB_OPTION) || (format == S_O_DBD_OPTION));
	
	/* This substring appears on every output line, preformat it here */
	snprintf(pre, 80, "%s%s%ld%s%s",
		 file_hdr.sa_nodename, seps[isdb], dt, seps[isdb], cur_time);
	pre[79] = '\0';

	if (DISPLAY_HORIZONTALLY(flags)) {
		printf("%s", pre);
	}

	for (i = 0; i < NR_ACT; i++) {
		
		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;
		
		if (IS_SELECTED(act[i]->options) && (act[i]->nr > 0)) {
			
			if (NEEDS_GLOBAL_ITV(act[i]->options)) {
				(*act[i]->f_render)(act[i], isdb, pre, curr, g_itv);
			}
			else {
				(*act[i]->f_render)(act[i], isdb, pre, curr, itv);
			}
		}
	}

	if (DISPLAY_HORIZONTALLY(flags)) {
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Write system statistics.
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @reset		Set to TRUE if last_uptime variable should be
 * 			reinitialized (used in next_slice() function).
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @act_id		Activities to display.
 *
 * OUT:
 * @cnt			Set to 0 to indicate that no other lines of stats
 * 			should be displayed.
 *
 * RETURNS:
 * 1 if a line of stats has been displayed, and 0 otherwise.
 ***************************************************************************
 */
int write_parsable_stats(int curr, int reset, long *cnt, int use_tm_start,
			 int use_tm_end, unsigned int act_id)
{
	unsigned long long dt, itv, g_itv;
	char cur_time[26];
	static int cross_day = FALSE;
	static __nr_t cpu_nr = -1;
	
	if (cpu_nr < 0) {
		cpu_nr = act[get_activity_position(act, A_CPU)]->nr;
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

	/* Set current timestamp */
	set_record_timestamp_string(curr, cur_time, 26);

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
		loctime.tm_hour += 24;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(&loctime, &tm_start) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval values */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr],
		      cpu_nr, &itv, &g_itv);

	/* Check time (3) */
	if (use_tm_end && (datecmp(&loctime, &tm_end) > 0)) {
		/* It's too late... */
		*cnt = 0;
		return 0;
	}

	dt = itv / HZ;
	/* Correct rounding error for dt */
	if ((itv % HZ) >= (HZ / 2)) {
		dt++;
	}

	write_mech_stats(curr, dt, itv, g_itv, cur_time, act_id);

	return 1;
}

/*
 ***************************************************************************
 * Display XML activity records
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @reset		Set to TRUE if last_uptime should be reinitialized
 *			(used in next_slice() function).
 * @tab			Number of tabulations to print.
 * @cpu_nr		Number of processors.
 *
 * OUT:
 * @cnt			Set to 0 to indicate that no other lines of stats
 * 			should be displayed.
 *
 * RETURNS:
 * 1 if stats have been successfully displayed.
 ***************************************************************************
 */
int write_xml_stats(int curr, int use_tm_start, int use_tm_end, int reset,
		    long *cnt, int tab, __nr_t cpu_nr)
{
	int i;
	unsigned long long dt, itv, g_itv;
	char cur_time[XML_TIMESTAMP_LEN];
	static int cross_day = FALSE;

	/* Fill timestamp structure (rectime) for current record */
	sadf_get_record_timestamp_struct(curr);

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
		loctime.tm_hour += 24;
	}

	/* Check time (2) */
	if (use_tm_start && (datecmp(&loctime, &tm_start) < 0))
		/* it's too soon... */
		return 0;

	/* Get interval values */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr],
		      cpu_nr, &itv, &g_itv);

	/* Check time (3) */
	if (use_tm_end && (datecmp(&loctime, &tm_end) > 0)) {
		/* It's too late... */
		*cnt = 0;
		return 0;
	}

	dt = itv / HZ;
	/* Correct rounding error for dt */
	if ((itv % HZ) >= (HZ / 2)) {
		dt++;
	}

	strftime(cur_time, XML_TIMESTAMP_LEN,
		 "date=\"%Y-%m-%d\" time=\"%H:%M:%S\"", &rectime);

        xprintf(tab, "<timestamp %s interval=\"%llu\">", cur_time, dt);
	tab++;

	/* Display XML statistics */
	for (i = 0; i < NR_ACT; i++) {
		
		if (CLOSE_MARKUP(act[i]->options) ||
		    (IS_SELECTED(act[i]->options) && (act[i]->nr > 0))) {
			
			if (NEEDS_GLOBAL_ITV(act[i]->options)) {
				(*act[i]->f_xml_print)(act[i], curr, tab, g_itv);
			}
			else {
				(*act[i]->f_xml_print)(act[i], curr, tab, itv);
			}
		}
	}

	xprintf(--tab, "</timestamp>");

	return 1;
}

/*
 ***************************************************************************
 * Display XML restart records
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @tab			Number of tabulations to print.
 ***************************************************************************
 */
void write_xml_restarts(int curr, int use_tm_start, int use_tm_end, int tab)
{
	char cur_time[64];

	/* Fill timestamp structure for current record */
	sadf_get_record_timestamp_struct(curr);
	
	/* The record must be in the interval specified by -s/-e options */
	if ((use_tm_start && (datecmp(&loctime, &tm_start) < 0)) ||
	    (use_tm_end && (datecmp(&loctime, &tm_end) > 0)))
		return;

	strftime(cur_time, 64, "date=\"%Y-%m-%d\" time=\"%H:%M:%S\"", &rectime);
	xprintf(tab, "<boot %s/>", cur_time);
}

/*
 ***************************************************************************
 * Display XML COMMENT records
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @tab			Number of tabulations to print.
 * @ifd			Input file descriptor.
 ***************************************************************************
 */
void write_xml_comments(int curr, int use_tm_start, int use_tm_end, int tab, int ifd)
{
	char cur_time[64];
	char file_comment[MAX_COMMENT_LEN];

	sa_fread(ifd, file_comment, MAX_COMMENT_LEN, HARD_SIZE);
	file_comment[MAX_COMMENT_LEN - 1] = '\0';

	/* Fill timestamp structure for current record */
	sadf_get_record_timestamp_struct(curr);

	/* The record must be in the interval specified by -s/-e options */
	if ((use_tm_start && (datecmp(&loctime, &tm_start) < 0)) ||
	    (use_tm_end && (datecmp(&loctime, &tm_end) > 0)))
		return;

	strftime(cur_time, 64, "date=\"%Y-%m-%d\" time=\"%H:%M:%S\"", &rectime);
	xprintf(tab, "<comment %s com=\"%s\"/>", cur_time, file_comment);
}

/*
 ***************************************************************************
 * Print contents of a special (RESTART or COMMENT) record
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @use_tm_start	Set to TRUE if option -s has been used.
 * @use_tm_end		Set to TRUE if option -e has been used.
 * @rtype		Record type (RESTART or COMMENT).
 * @ifd			Input file descriptor.
 ***************************************************************************
 */
void sadf_print_special(int curr, int use_tm_start, int use_tm_end, int rtype, int ifd)
{
	char cur_time[26];
	int dp = 1;

	set_record_timestamp_string(curr, cur_time, 26);

	/* The record must be in the interval specified by -s/-e options */
	if ((use_tm_start && (datecmp(&loctime, &tm_start) < 0)) ||
	    (use_tm_end && (datecmp(&loctime, &tm_end) > 0))) {
		dp = 0;
	}

	if (rtype == R_RESTART) {
		if (!dp)
			return;
		if (format == S_O_PPC_OPTION) {
			printf("%s\t-1\t%ld\tLINUX-RESTART\n",
			       file_hdr.sa_nodename, record_hdr[curr].ust_time);
		}
		else if ((format == S_O_DB_OPTION) || (format == S_O_DBD_OPTION)) {
			printf("%s;-1;%s;LINUX-RESTART\n",
			       file_hdr.sa_nodename, cur_time);
		}
	}
	else if (rtype == R_COMMENT) {
		char file_comment[MAX_COMMENT_LEN];

		sa_fread(ifd, file_comment, MAX_COMMENT_LEN, HARD_SIZE);
		file_comment[MAX_COMMENT_LEN - 1] = '\0';

		if (!dp || !DISPLAY_COMMENT(flags))
			return;
		
		if (format == S_O_PPC_OPTION) {
			printf("%s\t-1\t%ld\tCOM %s\n",
			       file_hdr.sa_nodename, record_hdr[curr].ust_time,
			       file_comment);
		}
		else if ((format == S_O_DB_OPTION) || (format == S_O_DBD_OPTION)) {
			printf("%s;-1;%s;COM %s\n",
			       file_hdr.sa_nodename, cur_time, file_comment);
		}
	}
}

/*
 ***************************************************************************
 * Display data file header.
 *
 * IN:
 * @dfile	Name of system activity data file.
 * @file_magic	System activity file magic header.
 * @file_hdr	System activity file standard header.
 ***************************************************************************
 */
void display_file_header(char *dfile, struct file_magic *file_magic,
			 struct file_header *file_hdr)
{
	int i, p;
	static __nr_t cpu_nr = -1;

	if (cpu_nr < 0) {
		cpu_nr = act[get_activity_position(act, A_CPU)]->nr;
	}
	
	printf(_("System activity data file: %s (%#x)\n"),
		dfile, file_magic->format_magic);

	display_sa_file_version(file_magic);

	if (file_magic->format_magic != FORMAT_MAGIC) {
		exit(0);
	}

	printf(_("Host: "));
	print_gal_header(localtime((const time_t *) &(file_hdr->sa_ust_time)),
			 file_hdr->sa_sysname, file_hdr->sa_release,
			 file_hdr->sa_nodename, file_hdr->sa_machine,
			 cpu_nr > 1 ? cpu_nr - 1 : 1);

	printf(_("Size of a long int: %d\n"), file_hdr->sa_sizeof_long);
	
	printf(_("List of activities:\n"));
	for (i = 0; i < NR_ACT; i++) {
		if (!id_seq[i])
			continue;
		if ((p = get_activity_position(act, id_seq[i])) < 0) {
			PANIC(id_seq[i]);
		}
		printf("%02d: %s\t(x%d)", act[p]->id, act[p]->name, act[p]->nr);
		if (act[p]->f_count2 || (act[p]->nr2 > 1)) {
			printf("\t(x%d)", act[p]->nr2);
		}
		if (act[p]->magic == ACTIVITY_MAGIC_UNKNOWN) {
			printf(_("\t[Unknown activity format]"));
		}
		printf("\n");
	}

	exit(0);
}

/*
 ***************************************************************************
 * Display XML header and host data
 *
 * IN:
 * @tab		Number of tabulations to print.
 * @cpu_nr	Number of processors.
 *
 * OUT:
 * @tab		Number of tabulations to print.
 ***************************************************************************
 */
void display_xml_header(int *tab, __nr_t cpu_nr)
{
	char cur_time[XML_TIMESTAMP_LEN];
	
	printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	printf("<!DOCTYPE Configure PUBLIC \"DTD v%s sysstat //EN\"\n", XML_DTD_VERSION);
	printf("\"http://pagesperso-orange.fr/sebastien.godard/sysstat.dtd\">\n");

	xprintf(*tab, "<sysstat>");
	xprintf(++(*tab), "<sysdata-version>%s</sysdata-version>", XML_DTD_VERSION);

	xprintf(*tab, "<host nodename=\"%s\">", file_hdr.sa_nodename);
	xprintf(++(*tab), "<sysname>%s</sysname>", file_hdr.sa_sysname);
	xprintf(*tab, "<release>%s</release>", file_hdr.sa_release);
		
	xprintf(*tab, "<machine>%s</machine>", file_hdr.sa_machine);
	xprintf(*tab, "<number-of-cpus>%d</number-of-cpus>", cpu_nr > 1 ? cpu_nr - 1 : 1);

	/* Fill file timestmap structure (rectime) */
	get_file_timestamp_struct(flags, &rectime, &file_hdr);
	strftime(cur_time, XML_TIMESTAMP_LEN, "%Y-%m-%d", &rectime);
	xprintf(*tab, "<file-date>%s</file-date>", cur_time);
}

/*
 ***************************************************************************
 * Read stats for current activity from file and write them.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @fpos	Position in file where reading must start.
 * @curr	Index in array for current sample statistics.
 * @file_actlst	List of (known or unknown) activities in file.
 *
 * OUT:
 * @curr	Index in array for next sample statistics.
 * @cnt		Number of lines of stats remaining to write.
 * @eosaf	Set to TRUE if EOF (end of file) has been reached.
 * @act_id	Activity to display, or ~0 for all.
 * @reset	Set to TRUE if last_uptime variable should be
 * 		reinitialized (used in next_slice() function).
 ***************************************************************************
 */
void rw_curr_act_stats(int ifd, off_t fpos, int *curr, long *cnt, int *eosaf,
		       unsigned int act_id, int *reset, struct file_activity *file_actlst)
{
	unsigned char rtype;
	int next;

	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}
	
	if ((format == S_O_DB_OPTION) || (format == S_O_DBD_OPTION)) {
		/* Print field list */
		list_fields(act_id);
	}

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	*cnt  = count;

	do {
		/* Display <count> lines of stats */
		*eosaf = sa_fread(ifd, &record_hdr[*curr], RECORD_HEADER_SIZE,
				  SOFT_SIZE);
		rtype = record_hdr[*curr].record_type;

		if (!*eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
			/* Read the extra fields since it's not a RESTART record */
			read_file_stat_bunch(act, *curr, ifd, file_hdr.sa_nr_act,
					     file_actlst);
		}

		if (!*eosaf && (rtype != R_RESTART)) {

			if (rtype == R_COMMENT) {
				sadf_print_special(*curr, tm_start.use, tm_end.use,
					      R_COMMENT, ifd);
				continue;
			}

			next = write_parsable_stats(*curr, *reset, cnt,
						    tm_start.use, tm_end.use, act_id);

			if (next) {
				/*
				 * next is set to 1 when we were close enough to desired interval.
				 * In this case, the call to write_parsable_stats() has actually
				 * displayed a line of stats.
				 */
				*curr ^=1;
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
 * Display activities for -x option.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file_actlst	List of (known or unknown) activities in file.
 ***************************************************************************
 */
void xml_display_loop(int ifd, struct file_activity *file_actlst)
{
	int curr, tab = 0, rtype;
	int eosaf = TRUE, next, reset = FALSE;
	long cnt = 1;
	off_t fpos;
	static __nr_t cpu_nr = -1;
	
	if (cpu_nr < 0) {
		cpu_nr = act[get_activity_position(act, A_CPU)]->nr;
	}

	/* Save current file position */
	if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
		perror("lseek");
		exit(2);
	}

	/* Print XML header */
	display_xml_header(&tab, cpu_nr);

	/* Process activities */
	xprintf(tab++, "<statistics>");
	do {
		/*
		 * If this record is a special (RESTART or COMMENT) one,
		 * skip it and try to read the next record in file.
		 */
		do {
			eosaf = sa_fread(ifd, &record_hdr[0], RECORD_HEADER_SIZE, SOFT_SIZE);
			rtype = record_hdr[0].record_type;

			if (!eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
				/*
				 * OK: Previous record was not a special one.
				 * So read now the extra fields.
				 */
				read_file_stat_bunch(act, 0, ifd, file_hdr.sa_nr_act,
						     file_actlst);
				sadf_get_record_timestamp_struct(0);
			}

			if (!eosaf && (rtype == R_COMMENT)) {
				/*
				 * Ignore COMMENT record.
				 * (Unlike RESTART records, COMMENT records have an additional
				 * comment field).
				 */
				if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
					perror("lseek");
				}
			}
		}
		while (!eosaf && ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
			(tm_start.use && (datecmp(&loctime, &tm_start) < 0)) ||
			(tm_end.use && (datecmp(&loctime, &tm_end) >=0))));

		/* Save the first stats collected. Used for example in next_slice() function */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		curr = 1;
		cnt = count;
		reset = TRUE;

		if (!eosaf) {
			do {
				eosaf = sa_fread(ifd, &record_hdr[curr], RECORD_HEADER_SIZE,
						 SOFT_SIZE);
				rtype = record_hdr[curr].record_type;

				if (!eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
					/* Read the extra fields since it's not a special record */
					read_file_stat_bunch(act, curr, ifd, file_hdr.sa_nr_act,
							     file_actlst);

					/* next is set to 1 when we were close enough to desired interval */
					next = write_xml_stats(curr, tm_start.use, tm_end.use, reset,
							&cnt, tab, cpu_nr);

					if (next) {
						curr ^= 1;
						if (cnt > 0) {
							cnt--;
						}
					}
					reset = FALSE;
				}

				if (!eosaf && (rtype == R_COMMENT)) {
					/* Ignore COMMENT record */
					if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
						perror("lseek");
					}
				}
			}
			while (cnt && !eosaf && (rtype != R_RESTART));

			if (!cnt) {
				/* Go to next Linux restart, if possible */
				do {
					eosaf = sa_fread(ifd, &record_hdr[curr], RECORD_HEADER_SIZE,
							 SOFT_SIZE);
					rtype = record_hdr[curr].record_type;
					if (!eosaf && (rtype != R_RESTART) && (rtype != R_COMMENT)) {
						read_file_stat_bunch(act, curr, ifd, file_hdr.sa_nr_act,
								     file_actlst);
					}
					else if (!eosaf && (rtype == R_COMMENT)) {
						/* Ignore COMMENT record */
						if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
							perror("lseek");
						}
					}
				}
				while (!eosaf && (rtype != R_RESTART));
			}
			reset = TRUE;
		}
	}
	while (!eosaf);
	xprintf(--tab, "</statistics>");

	/* Rewind file */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}

	/* Process now RESTART entries to display restart messages */
	xprintf(tab++, "<restarts>");
	do {
		if ((eosaf = sa_fread(ifd, &record_hdr[0], RECORD_HEADER_SIZE,
				      SOFT_SIZE)) == 0) {

			rtype = record_hdr[0].record_type;
			if ((rtype != R_RESTART) && (rtype != R_COMMENT)) {
				read_file_stat_bunch(act, 0, ifd, file_hdr.sa_nr_act,
						     file_actlst);
			}
			if (rtype == R_RESTART) {
				write_xml_restarts(0, tm_start.use, tm_end.use, tab);
			}
			else if (rtype == R_COMMENT) {
				/* Ignore COMMENT record */
				if (lseek(ifd, MAX_COMMENT_LEN, SEEK_CUR) < MAX_COMMENT_LEN) {
					perror("lseek");
				}
			}
		}
	}
	while (!eosaf);
	xprintf(--tab, "</restarts>");

	/* Rewind file */
	if (lseek(ifd, fpos, SEEK_SET) < fpos) {
		perror("lseek");
		exit(2);
	}

	/* Last, process COMMENT entries to display comments */
	if (DISPLAY_COMMENT(flags)) {
		xprintf(tab++, "<comments>");
		do {
			if ((eosaf = sa_fread(ifd, &record_hdr[0], RECORD_HEADER_SIZE,
				              SOFT_SIZE)) == 0) {

				rtype = record_hdr[0].record_type;
				if ((rtype != R_RESTART) && (rtype != R_COMMENT)) {
					read_file_stat_bunch(act, 0, ifd, file_hdr.sa_nr_act,
							     file_actlst);
				}
				if (rtype == R_COMMENT) {
					write_xml_comments(0, tm_start.use, tm_end.use,
							   tab, ifd);
				}
			}
		}
		while (!eosaf);
		xprintf(--tab, "</comments>");
	}
	
	xprintf(--tab, "</host>");
	xprintf(--tab, "</sysstat>");
}

/*
 ***************************************************************************
 * Display activities for -p and -d options.
 *
 * IN:
 * @ifd		File descriptor of input file.
 * @file_actlst	List of (known or unknown) activities in file.
 ***************************************************************************
 */
void main_display_loop(int ifd, struct file_activity *file_actlst)
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
			if (sa_fread(ifd, &record_hdr[0], RECORD_HEADER_SIZE, SOFT_SIZE))
				/* End of sa data file */
				return;

			rtype = record_hdr[0].record_type;
			if ((rtype == R_RESTART) || (rtype == R_COMMENT)) {
				sadf_print_special(0, tm_start.use, tm_end.use, rtype, ifd);
			}
			else {
				/*
				 * OK: Previous record was not a special one.
				 * So read now the extra fields.
				 */
				read_file_stat_bunch(act, 0, ifd, file_hdr.sa_nr_act,
						     file_actlst);
				sadf_get_record_timestamp_struct(0);
			}
		}
		while ((rtype == R_RESTART) || (rtype == R_COMMENT) ||
		       (tm_start.use && (datecmp(&loctime, &tm_start) < 0)) ||
		       (tm_end.use && (datecmp(&loctime, &tm_end) >=0)));

		/* Save the first stats collected. Will be used to compute the average */
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
					  ALL_ACTIVITIES, &reset, file_actlst);
		}
		else {
			/* For each requested activity... */
			for (i = 0; i < NR_ACT; i++) {
				
				if (!id_seq[i])
					continue;
				
				if ((p = get_activity_position(act, id_seq[i])) < 0) {
					/* Should never happen */
					PANIC(1);
				}
				if (!IS_SELECTED(act[p]->options))
					continue;

				if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
					rw_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
							  act[p]->id, &reset, file_actlst);
				}
				else {
					unsigned int optf, msk;
					
					optf = act[p]->opt_flags;
					
					for (msk = 1; msk < 0x10; msk <<= 1) {
						if (act[p]->opt_flags & msk) {
							act[p]->opt_flags &= msk;
							
							rw_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf,
									  act[p]->id, &reset, file_actlst);
							act[p]->opt_flags = optf;
						}
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
					read_file_stat_bunch(act, curr, ifd, file_hdr.sa_nr_act,
							     file_actlst);
				}
				else if (!eosaf && (rtype == R_COMMENT)) {
					/* This was a COMMENT record: print it */
					sadf_print_special(curr, tm_start.use, tm_end.use, R_COMMENT, ifd);
				}
			}
			while (!eosaf && (rtype != R_RESTART));
		}

		/* The last record we read was a RESTART one: Print it */
		if (!eosaf && (record_hdr[curr].record_type == R_RESTART)) {
			sadf_print_special(curr, tm_start.use, tm_end.use, R_RESTART, ifd);
		}
	}
	while (!eosaf);
}

/*
 ***************************************************************************
 * Read statistics from a system activity data file.
 *
 * IN:
 * @dfile	System activity data file name.
 ***************************************************************************
 */
void read_stats_from_file(char dfile[])
{
	struct file_magic file_magic;
	struct file_activity *file_actlst = NULL;
	int ifd, ignore;

	/* Prepare file for reading */
	ignore = (format == S_O_HDR_OPTION);
	check_file_actlst(&ifd, dfile, act, &file_magic, &file_hdr,
			  &file_actlst, id_seq, ignore);
	
	if (format == S_O_HDR_OPTION) {
		/* Display data file header then exit */
		display_file_header(dfile, &file_magic, &file_hdr);
	}

	/* Perform required allocations */
	allocate_structures(act);

	if (format == S_O_XML_OPTION) {
		xml_display_loop(ifd, file_actlst);
	}
	else {
		main_display_loop(ifd, file_actlst);
	}

	close(ifd);
	
	if (file_actlst) {
		free(file_actlst);
	}
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
	int i;
	char dfile[MAX_FILE_LEN];

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
				if (parse_sar_opt(argv, &opt, act, &flags, C_SADF)) {
					usage(argv[0]);
				}
			}
			else {

				for (i = 1; *(argv[opt] + i); i++) {

					switch (*(argv[opt] + i)) {

					case 'C':
						flags |= S_F_COMMENT;
						break;
							
					case 'd':
						if (format && (format != S_O_DB_OPTION)) {
							usage(argv[0]);
						}
						format = S_O_DB_OPTION;
						break;

					case 'D':
						if (format && (format != S_O_DBD_OPTION)) {
							usage(argv[0]);
						}
						format = S_O_DBD_OPTION;
						break;

					case 'h':
						flags |= S_F_HORIZONTALLY;
						break;

					case 'H':
						if (format && (format != S_O_HDR_OPTION)) {
							usage(argv[0]);
						}
						format = S_O_HDR_OPTION;
						break;

					case 'p':
						if (format && (format != S_O_PPC_OPTION)) {
							usage(argv[0]);
						}
						format = S_O_PPC_OPTION;
						break;

					case 't':
						flags |= S_F_TRUE_TIME;
						break;

					case 'x':
						if (format && (format != S_O_XML_OPTION)) {
							usage(argv[0]);
						}
						format = S_O_XML_OPTION;
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
			if (!dfile[0]) {
				if (!strcmp(argv[opt], "-")) {
					/* File name set to '-' */
					set_default_file(&rectime, dfile);
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
				}
			}
			else {
				/* File already specified */
				usage(argv[0]);
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
		set_default_file(&rectime, dfile);
	}

	if (tm_start.use && tm_end.use && (tm_end.tm_hour < tm_start.tm_hour)) {
		tm_end.tm_hour += 24;
	}

	if (USE_PRETTY_OPTION(flags)) {
		dm_major = get_devmap_major();
	}

	/*
	 * Display all the contents of the daily data file if the count parameter
	 * was not set on the command line.
	 */
	if (!count) {
		count = -1;
	}

	/* Default is CPU activity and PPC display */
	select_default_activity(act);
	
	if (!format) {
		if (DISPLAY_HORIZONTALLY(flags)) {
			format = S_O_DB_OPTION;
		}
		else {
			format = S_O_PPC_OPTION;
		}
	}
	if (DISPLAY_HORIZONTALLY(flags) && (format != S_O_DB_OPTION)) {
		/* Remove option -h if not used with option -d */
		flags &= ~S_F_HORIZONTALLY;
	}

	if (interval < 0) {
		interval = 1;
	}

	/* Read stats from file */
	read_stats_from_file(dfile);

	/* Free bitmaps */
	free_bitmaps(act);

	return 0;
}
