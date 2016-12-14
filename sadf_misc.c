/*
 * sadf_misc.c: Funtions used by sadf to display special records
 * (C) 2011-2016 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include "sadf.h"
#include "sa.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;
extern char *seps[];

/*
 ***************************************************************************
 * Display restart messages (database and ppc formats).
 *
 * IN:
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @sep		Character used as separator.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	CPU count associated with restart mark.
 ***************************************************************************
 */
void print_dbppc_restart(char *cur_date, char *cur_time, int utc, char sep,
			 struct file_header *file_hdr, unsigned int cpu_nr)
{
	printf("%s%c-1%c", file_hdr->sa_nodename, sep, sep);
	if (strlen(cur_date)) {
		printf("%s ", cur_date);
	}
	printf("%s", cur_time);
	if (strlen(cur_date) && utc) {
		printf(" UTC");
	}
	printf("%cLINUX-RESTART\t(%d CPU)\n",
	       sep, cpu_nr > 1 ? cpu_nr - 1 : 1);
}

/*
 ***************************************************************************
 * Display restart messages (ppc format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	CPU count associated with restart mark.
 ***************************************************************************
 */
__printf_funct_t print_db_restart(int *tab, int action, char *cur_date,
				  char *cur_time, int utc, struct file_header *file_hdr,
				  unsigned int cpu_nr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action == F_MAIN) {
		print_dbppc_restart(cur_date, cur_time, utc, ';', file_hdr, cpu_nr);
	}
}

/*
 ***************************************************************************
 * Display restart messages (database format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	CPU count associated with restart mark.
 ***************************************************************************
 */
__printf_funct_t print_ppc_restart(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, struct file_header *file_hdr,
				   unsigned int cpu_nr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action == F_MAIN) {
		print_dbppc_restart(cur_date, cur_time, utc, '\t', file_hdr, cpu_nr);
	}
}

/*
 ***************************************************************************
 * Display restart messages (XML format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @file_hdr	System activity file standard header (unused here).
 * @cpu_nr	CPU count associated with restart mark.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_restart(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, struct file_header *file_hdr,
				   unsigned int cpu_nr)
{
	if (action & F_BEGIN) {
		xprintf((*tab)++, "<restarts>");
	}
	if (action & F_MAIN) {
		xprintf(*tab, "<boot date=\"%s\" time=\"%s\" utc=\"%d\" cpu_count=\"%d\"/>",
			cur_date, cur_time, utc ? 1 : 0, cpu_nr > 1 ? cpu_nr - 1 : 1);
	}
	if (action & F_END) {
		xprintf(--(*tab), "</restarts>");
	}
}

/*
 ***************************************************************************
 * Display restart messages (JSON format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @file_hdr	System activity file standard header (unused here).
 * @cpu_nr	CPU count associated with restart mark.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_restart(int *tab, int action, char *cur_date,
				    char *cur_time, int utc, struct file_header *file_hdr,
				    unsigned int cpu_nr)
{
	static int sep = FALSE;

	if (action & F_BEGIN) {
		printf(",\n");
		xprintf((*tab)++, "\"restarts\": [");
	}
	if (action & F_MAIN) {
		if (sep) {
			printf(",\n");
		}
		xprintf((*tab)++, "{");
		xprintf(*tab, "\"boot\": {\"date\": \"%s\", \"time\": \"%s\", \"utc\": %d, \"cpu_count\": %d}",
			cur_date, cur_time, utc ? 1 : 0, cpu_nr > 1 ? cpu_nr - 1 : 1);
		xprintf0(--(*tab), "}");
		sep = TRUE;
	}
	if (action & F_END) {
		if (sep) {
			printf("\n");
			sep = FALSE;
		}
		xprintf0(--(*tab), "]");
	}
}

/*
 ***************************************************************************
 * Display comments (database and ppc formats).
 *
 * IN:
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @sep		Character used as separator.
 * @file_hdr	System activity file standard header.
 ***************************************************************************
 */
void print_dbppc_comment(char *cur_date, char *cur_time, int utc,
			 char *comment, char sep, struct file_header *file_hdr)
{
	printf("%s%c-1%c", file_hdr->sa_nodename, sep, sep);
	if (strlen(cur_date)) {
		printf("%s ", cur_date);
	}
	printf("%s", cur_time);
	if (strlen(cur_date) && utc) {
		printf(" UTC");
	}
	printf("%cCOM %s\n", sep, comment);
}

/*
 ***************************************************************************
 * Display comments (database format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header.
 ***************************************************************************
 */
__printf_funct_t print_db_comment(int *tab, int action, char *cur_date,
				  char *cur_time, int utc, char *comment,
				  struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action & F_MAIN) {
		print_dbppc_comment(cur_date, cur_time, utc, comment,
				    ';', file_hdr);
	}
}

/*
 ***************************************************************************
 * Display comments (ppc format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header.
 ***************************************************************************
 */
__printf_funct_t print_ppc_comment(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, char *comment,
				   struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action & F_MAIN) {
		print_dbppc_comment(cur_date, cur_time, utc, comment,
				    '\t', file_hdr);
	}
}

/*
 ***************************************************************************
 * Display comments (XML format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current comment.
 * @cur_time	Time string of current comment.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header (unused here).
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_comment(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, char *comment,
				   struct file_header *file_hdr)
{
	if (action & F_BEGIN) {
		xprintf((*tab)++, "<comments>");
	}
	if (action & F_MAIN) {
		xprintf(*tab, "<comment date=\"%s\" time=\"%s\" utc=\"%d\" com=\"%s\"/>",
			cur_date, cur_time, utc ? 1 : 0, comment);
	}
	if (action & F_END) {
		xprintf(--(*tab), "</comments>");
	}
}

/*
 ***************************************************************************
 * Display comments (JSON format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current comment.
 * @cur_time	Time string of current comment.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header (unused here).
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_comment(int *tab, int action, char *cur_date,
				    char *cur_time, int utc, char *comment,
				    struct file_header *file_hdr)
{
	static int sep = FALSE;

	if (action & F_BEGIN) {
		printf(",\n");
		xprintf((*tab)++, "\"comments\": [");
	}
	if (action & F_MAIN) {
		if (sep) {
			printf(",\n");
		}
		xprintf((*tab)++, "{");
		xprintf(*tab,
			"\"comment\": {\"date\": \"%s\", \"time\": \"%s\", "
			"\"utc\": %d, \"com\": \"%s\"}",
			cur_date, cur_time, utc ? 1 : 0, comment);
		xprintf0(--(*tab), "}");
		sep = TRUE;
	}
	if (action & F_END) {
		if (sep) {
			printf("\n");
			sep = FALSE;
		}
		xprintf0(--(*tab), "]");
	}
}

/*
 ***************************************************************************
 * Display the "statistics" part of the report (XML format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_statistics(int *tab, int action)
{
	if (action & F_BEGIN) {
		xprintf((*tab)++, "<statistics>");
	}
	if (action & F_END) {
		xprintf(--(*tab), "</statistics>");
	}
}

/*
 ***************************************************************************
 * Display the "statistics" part of the report (JSON format).
 *
 * IN:
 * @tab		Number of tabulations.
 * @action	Action expected from current function.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_statistics(int *tab, int action)
{
	static int sep = FALSE;

	if (action & F_BEGIN) {
		printf(",\n");
		xprintf((*tab)++, "\"statistics\": [");
	}
	if (action & F_MAIN) {
		if (sep) {
			xprintf(--(*tab), "},");
		}
		xprintf((*tab)++, "{");
		sep = TRUE;
	}
	if (action & F_END) {
		if (sep) {
			xprintf(--(*tab), "}");
			sep = FALSE;
		}
		xprintf0(--(*tab), "]");
	}
}

/*
 ***************************************************************************
 * Display the "timestamp" part of the report (db and ppc format).
 *
 * IN:
 * @fmt		Output format (F_DB_OUTPUT or F_PPC_OUTPUT).
 * @file_hdr	System activity file standard header.
 * @cur_date	Date string of current record.
 * @cur_time	Time string of current record.
 * @utc		True if @cur_time is expressed in UTC.
 * @itv		Interval of time with preceding record.
 *
 * RETURNS:
 * Pointer on the "timestamp" string.
 ***************************************************************************
 */
char *print_dbppc_timestamp(int fmt, struct file_header *file_hdr, char *cur_date,
			    char *cur_time, int utc, unsigned long long itv)
{
	int isdb = (fmt == F_DB_OUTPUT);
	static char pre[80];
	char temp[80];

	/* This substring appears on every output line, preformat it here */
	snprintf(pre, 80, "%s%s%lld%s",
		 file_hdr->sa_nodename, seps[isdb], itv, seps[isdb]);
	if (strlen(cur_date)) {
		snprintf(temp, 80, "%s%s ", pre, cur_date);
	}
	else {
		strcpy(temp, pre);
	}
	snprintf(pre, 80, "%s%s%s", temp, cur_time,
		 strlen(cur_date) && utc ? " UTC" : "");
	pre[79] = '\0';

	if (DISPLAY_HORIZONTALLY(flags)) {
		printf("%s", pre);
	}

	return pre;
}

/*
 ***************************************************************************
 * Display the "timestamp" part of the report (ppc format).
 *
 * IN:
 * @parm	Pointer on specific parameters (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current record.
 * @cur_time	Time string of current record.
 * @itv		Interval of time with preceding record.
 * @file_hdr	System activity file standard header.
 * @flags	Flags for common options.
 *
 * RETURNS:
 * Pointer on the "timestamp" string.
 ***************************************************************************
 */
__tm_funct_t print_ppc_timestamp(void *parm, int action, char *cur_date,
				 char *cur_time, unsigned long long itv,
				 struct file_header *file_hdr, unsigned int flags)
{
	int utc = !PRINT_LOCAL_TIME(flags) && !PRINT_TRUE_TIME(flags);

	if (action & F_BEGIN) {
		return print_dbppc_timestamp(F_PPC_OUTPUT, file_hdr, cur_date, cur_time, utc, itv);
	}
	if (action & F_END) {
		if (DISPLAY_HORIZONTALLY(flags)) {
			printf("\n");
		}
	}

	return NULL;
}

/*
 ***************************************************************************
 * Display the "timestamp" part of the report (db format).
 *
 * IN:
 * @parm	Pointer on specific parameters (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current record.
 * @cur_time	Time string of current record.
 * @itv		Interval of time with preceding record.
 * @file_hdr	System activity file standard header.
 * @flags	Flags for common options.
 *
 * RETURNS:
 * Pointer on the "timestamp" string.
 ***************************************************************************
 */
__tm_funct_t print_db_timestamp(void *parm, int action, char *cur_date,
				char *cur_time, unsigned long long itv,
				struct file_header *file_hdr, unsigned int flags)
{
	int utc = !PRINT_LOCAL_TIME(flags) && !PRINT_TRUE_TIME(flags);

	if (action & F_BEGIN) {
		return print_dbppc_timestamp(F_DB_OUTPUT, file_hdr, cur_date, cur_time, utc, itv);
	}
	if (action & F_END) {
		if (DISPLAY_HORIZONTALLY(flags)) {
			printf("\n");
		}
	}

	return NULL;
}

/*
 ***************************************************************************
 * Display the "timestamp" part of the report (XML format).
 *
 * IN:
 * @parm	Specific parameter. Here: number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current comment.
 * @cur_time	Time string of current comment.
 * @itv		Interval of time with preceding record.
 * @file_hdr	System activity file standard header (unused here).
 * @flags	Flags for common options.
 ***************************************************************************
 */
__tm_funct_t print_xml_timestamp(void *parm, int action, char *cur_date,
				 char *cur_time, unsigned long long itv,
				 struct file_header *file_hdr, unsigned int flags)
{
	int utc = !PRINT_LOCAL_TIME(flags) && !PRINT_TRUE_TIME(flags);
	int *tab = (int *) parm;

	if (action & F_BEGIN) {
		xprintf((*tab)++, "<timestamp date=\"%s\" time=\"%s\" utc=\"%d\" interval=\"%llu\">",
			cur_date, cur_time, utc ? 1 : 0, itv);
	}
	if (action & F_END) {
		xprintf(--(*tab), "</timestamp>");
	}

	return NULL;
}

/*
 ***************************************************************************
 * Display the "timestamp" part of the report (JSON format).
 *
 * IN:
 * @parm	Specific parameter. Here: number of tabulations.
 * @action	Action expected from current function.
 * @cur_date	Date string of current comment.
 * @cur_time	Time string of current comment.
 * @itv		Interval of time with preceding record.
 * @file_hdr	System activity file standard header (unused here).
 * @flags	Flags for common options.
 ***************************************************************************
 */
__tm_funct_t print_json_timestamp(void *parm, int action, char *cur_date,
				  char *cur_time, unsigned long long itv,
				  struct file_header *file_hdr, unsigned int flags)
{
	int utc = !PRINT_LOCAL_TIME(flags) && !PRINT_TRUE_TIME(flags);
	int *tab = (int *) parm;

	if (action & F_BEGIN) {
		xprintf0(*tab,
			 "\"timestamp\": {\"date\": \"%s\", \"time\": \"%s\", "
			 "\"utc\": %d, \"interval\": %llu}",
			 cur_date, cur_time, utc ? 1 : 0, itv);
	}
	if (action & F_MAIN) {
		printf(",\n");
	}
	if (action & F_END) {
		printf("\n");
	}

	return NULL;
}

/*
 ***************************************************************************
 * Display the header of the report (XML format).
 *
 * IN:
 * @parm	Specific parameter. Here: number of tabulations.
 * @action	Action expected from current function.
 * @dfile	Name of system activity data file.
 * @file_magic	System activity file magic header.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	Number of processors for current daily data file.
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 *
 * OUT:
 * @parm	Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr, __nr_t cpu_nr,
				  struct activity *act[], unsigned int id_seq[])
{
	struct tm rectime, *loc_t;
	char cur_time[TIMESTAMP_LEN];
	int *tab = (int *) parm;

	if (action & F_BEGIN) {
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		printf("<!DOCTYPE sysstat PUBLIC \"DTD v%s sysstat //EN\"\n",
		       XML_DTD_VERSION);
		printf("\"http://pagesperso-orange.fr/sebastien.godard/sysstat-%s.dtd\">\n",
		       XML_DTD_VERSION);

		xprintf(*tab, "<sysstat\n"
			      "xmlns=\"http://pagesperso-orange.fr/sebastien.godard/sysstat\"\n"
			      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
			      "xsi:schemaLocation=\"http://pagesperso-orange.fr/sebastien.godard sysstat.xsd\">");

		xprintf(++(*tab), "<sysdata-version>%s</sysdata-version>",
			XML_DTD_VERSION);

		xprintf(*tab, "<host nodename=\"%s\">", file_hdr->sa_nodename);
		xprintf(++(*tab), "<sysname>%s</sysname>", file_hdr->sa_sysname);
		xprintf(*tab, "<release>%s</release>", file_hdr->sa_release);

		xprintf(*tab, "<machine>%s</machine>", file_hdr->sa_machine);
		xprintf(*tab, "<number-of-cpus>%d</number-of-cpus>",
			cpu_nr > 1 ? cpu_nr - 1 : 1);

		/* Fill file timestmap structure (rectime) */
		get_file_timestamp_struct(flags, &rectime, file_hdr);
		strftime(cur_time, sizeof(cur_time), "%Y-%m-%d", &rectime);
		xprintf(*tab, "<file-date>%s</file-date>", cur_time);

		if ((loc_t = gmtime((const time_t *) &file_hdr->sa_ust_time)) != NULL) {
			strftime(cur_time, sizeof(cur_time), "%T", loc_t);
			xprintf(*tab, "<file-utc-time>%s</file-utc-time>", cur_time);
		}

	}
	if (action & F_END) {
		xprintf(--(*tab), "</host>");
		xprintf(--(*tab), "</sysstat>");
	}
}

/*
 ***************************************************************************
 * Display the header of the report (JSON format).
 *
 * IN:
 * @parm	Specific parameter. Here: number of tabulations.
 * @action	Action expected from current function.
 * @dfile	Name of system activity data file.
 * @file_magic	System activity file magic header.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	Number of processors for current daily data file.
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 *
 * OUT:
 * @parm	Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_header(void *parm, int action, char *dfile,
				   struct file_magic *file_magic,
				   struct file_header *file_hdr, __nr_t cpu_nr,
				   struct activity *act[], unsigned int id_seq[])
{
	struct tm rectime, *loc_t;
	char cur_time[TIMESTAMP_LEN];
	int *tab = (int *) parm;

	if (action & F_BEGIN) {
		xprintf(*tab, "{\"sysstat\": {");

		xprintf(++(*tab), "\"sysdata-version\": %s,",
			XML_DTD_VERSION);

		xprintf(*tab, "\"hosts\": [");
		xprintf(++(*tab), "{");
		xprintf(++(*tab), "\"nodename\": \"%s\",", file_hdr->sa_nodename);
		xprintf(*tab, "\"sysname\": \"%s\",", file_hdr->sa_sysname);
		xprintf(*tab, "\"release\": \"%s\",", file_hdr->sa_release);

		xprintf(*tab, "\"machine\": \"%s\",", file_hdr->sa_machine);
		xprintf(*tab, "\"number-of-cpus\": %d,",
			cpu_nr > 1 ? cpu_nr - 1 : 1);

		/* Fill file timestmap structure (rectime) */
		get_file_timestamp_struct(flags, &rectime, file_hdr);
		strftime(cur_time, sizeof(cur_time), "%Y-%m-%d", &rectime);
		xprintf0(*tab, "\"file-date\": \"%s\"", cur_time);

		if ((loc_t = gmtime((const time_t *) &file_hdr->sa_ust_time)) != NULL) {
			strftime(cur_time, sizeof(cur_time), "%T", loc_t);
			printf(",\n");
			xprintf0(*tab, "\"file-utc-time\": \"%s\"", cur_time);
		}

	}
	if (action & F_END) {
		printf("\n");
		xprintf(--(*tab), "}");
		xprintf(--(*tab), "]");
		xprintf(--(*tab), "}}");
	}
}

/*
 ***************************************************************************
 * Display data file header.
 *
 * IN:
 * @parm	Specific parameter (unused here).
 * @action	Action expected from current function.
 * @dfile	Name of system activity data file.
 * @file_magic	System activity file magic header.
 * @file_hdr	System activity file standard header.
 * @cpu_nr	Number of processors for current daily data file.
 * @act		Array of activities.
 * @id_seq	Activity sequence.
 ***************************************************************************
 */
__printf_funct_t print_hdr_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr, __nr_t cpu_nr,
				  struct activity *act[], unsigned int id_seq[])
{
	int i, p;
	struct tm rectime, *loc_t;
	char cur_time[TIMESTAMP_LEN];

	/* Actions F_MAIN and F_END ignored */
	if (action & F_BEGIN) {
		printf(_("System activity data file: %s (%#x)\n"),
		       dfile, file_magic->format_magic);

		display_sa_file_version(stdout, file_magic);

		if (file_magic->format_magic != FORMAT_MAGIC) {
			return;
		}

		printf(_("Genuine sa datafile: %s (%x)\n"),
		       file_magic->upgraded ? _("no") : _("yes"),
		       file_magic->upgraded);

		printf(_("Host: "));
		print_gal_header(localtime((const time_t *) &(file_hdr->sa_ust_time)),
				 file_hdr->sa_sysname, file_hdr->sa_release,
				 file_hdr->sa_nodename, file_hdr->sa_machine,
				 cpu_nr > 1 ? cpu_nr - 1 : 1);

		printf(_("Number of CPU for last samples in file: %u\n"),
		       file_hdr->sa_last_cpu_nr > 1 ? file_hdr->sa_last_cpu_nr - 1 : 1);

		/* Fill file timestmap structure (rectime) */
		get_file_timestamp_struct(flags, &rectime, file_hdr);
		strftime(cur_time, sizeof(cur_time), "%Y-%m-%d", &rectime);
		printf(_("File date: %s\n"), cur_time);

		if ((loc_t = gmtime((const time_t *) &file_hdr->sa_ust_time)) != NULL) {
			printf(_("File time: "));
			strftime(cur_time, sizeof(cur_time), "%T", loc_t);
			printf("%s UTC\n", cur_time);
		}

		printf(_("Size of a long int: %d\n"), file_hdr->sa_sizeof_long);

		/* Number of activities (number of volatile activities) in file */
		printf("sa_act_nr (sa_vol_act_nr): %u (%u)\n",
		       file_hdr->sa_act_nr, file_hdr->sa_vol_act_nr);

		printf(_("List of activities:\n"));

		for (i = 0; i < NR_ACT; i++) {
			if (!id_seq[i])
				continue;

			p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);

			printf("%02d: %s\t(x%d)", act[p]->id, act[p]->name, act[p]->nr);
			if (act[p]->f_count2 || (act[p]->nr2 > 1)) {
				printf("\t(x%d)", act[p]->nr2);
			}
			if (act[p]->magic == ACTIVITY_MAGIC_UNKNOWN) {
				printf(_("\t[Unknown activity format]"));
			}
			printf("\n");
		}
	}
}

/*
 ***************************************************************************
 * Display the header of the report (SVG format).
 *
 * IN:
 * @parm	Specific parameter. Here: number of graphs to display.
 * @action	Action expected from current function.
 * @dfile	Name of system activity data file (unused here).
 * @file_magic	System activity file magic header (unused here).
 * @file_hdr	System activity file standard header.
 * @cpu_nr	Number of processors for current daily data file.
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 ***************************************************************************
 */
__printf_funct_t print_svg_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr, __nr_t cpu_nr,
				  struct activity *act[], unsigned int id_seq[])
{
	int *graph_nr = (int *) parm;

	if (action & F_BEGIN) {
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		printf("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" ");
		printf("\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
		printf("<svg xmlns=\"http://www.w3.org/2000/svg\"");
		if (action & F_END) {
			printf(">\n");
		}
	}

	if (action & F_MAIN) {
		printf(" width=\"%d\" height=\"%d\""
		       " fill=\"black\" stroke=\"gray\" stroke-width=\"1\">\n",
		       SVG_V_XSIZE, SVG_H_YSIZE + SVG_T_YSIZE * (*graph_nr));
		printf("<text x= \"0\" y=\"30\" text-anchor=\"start\" stroke=\"brown\">");
		print_gal_header(localtime((const time_t *) &(file_hdr->sa_ust_time)),
				 file_hdr->sa_sysname, file_hdr->sa_release,
				 file_hdr->sa_nodename, file_hdr->sa_machine,
				 cpu_nr > 1 ? cpu_nr - 1 : 1);
		printf("</text>\n");
	}

	if (action & F_END) {
		printf("</svg>\n");
	}
}
