/*
 * sadf_misc.c: Funtions used by sadf to display special records
 * (C) 2011-2018 by Sebastien GODARD (sysstat <at> orange.fr)
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

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;
extern char *seps[];
extern unsigned int id_seq[];


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
 ***************************************************************************
 */
void print_dbppc_restart(char *cur_date, char *cur_time, int utc, char sep,
			 struct file_header *file_hdr)
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
	       sep, file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);
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
 ***************************************************************************
 */
__printf_funct_t print_db_restart(int *tab, int action, char *cur_date,
				  char *cur_time, int utc, struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action == F_MAIN) {
		print_dbppc_restart(cur_date, cur_time, utc, ';', file_hdr);
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
 ***************************************************************************
 */
__printf_funct_t print_ppc_restart(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action == F_MAIN) {
		print_dbppc_restart(cur_date, cur_time, utc, '\t', file_hdr);
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
 * @file_hdr	System activity file standard header.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_restart(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, struct file_header *file_hdr)
{
	if (action & F_BEGIN) {
		xprintf((*tab)++, "<restarts>");
	}
	if (action & F_MAIN) {
		xprintf(*tab, "<boot date=\"%s\" time=\"%s\" utc=\"%d\" cpu_count=\"%d\"/>",
			cur_date, cur_time, utc ? 1 : 0,
			file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);
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
 * @file_hdr	System activity file standard header.
 *
 * OUT:
 * @tab		Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_restart(int *tab, int action, char *cur_date,
				    char *cur_time, int utc, struct file_header *file_hdr)
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
			cur_date, cur_time, utc ? 1 : 0,
			file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);
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
 * Display restart messages (raw format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @file_hdr	System activity file standard header.
 ***************************************************************************
 */
__printf_funct_t print_raw_restart(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action == F_MAIN) {
		printf("%s", cur_time);
		if (strlen(cur_date) && utc) {
			printf(" UTC");
		}
		printf("; LINUX-RESTART (%d CPU)\n",
		       file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);
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
 * Display comments (raw format).
 *
 * IN:
 * @tab		Number of tabulations (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current restart message.
 * @cur_time	Time string of current restart message.
 * @utc		True if @cur_time is expressed in UTC.
 * @comment	Comment to display.
 * @file_hdr	System activity file standard header (unused here).
 ***************************************************************************
 */
__printf_funct_t print_raw_comment(int *tab, int action, char *cur_date,
				   char *cur_time, int utc, char *comment,
				   struct file_header *file_hdr)
{
	/* Actions F_BEGIN and F_END ignored */
	if (action & F_MAIN) {
		printf("%s", cur_time);
		if (strlen(cur_date) && utc) {
			printf(" UTC");
		}
		printf("; COM %s\n", comment);
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
	static char pre[512];
	char temp1[128], temp2[256];

	/* This substring appears on every output line, preformat it here */
	snprintf(temp1, sizeof(temp1), "%s%s%lld%s",
		 file_hdr->sa_nodename, seps[isdb], itv, seps[isdb]);
	if (strlen(cur_date)) {
		snprintf(temp2, sizeof(temp2), "%s%s ", temp1, cur_date);
	}
	else {
		strcpy(temp2, temp1);
	}
	snprintf(pre, sizeof(pre), "%s%s%s", temp2, cur_time,
		 strlen(cur_date) && utc ? " UTC" : "");
	pre[sizeof(pre) - 1] = '\0';

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
 * Display the "timestamp" part of the report (raw format).
 *
 * IN:
 * @parm	Pointer on specific parameters (unused here).
 * @action	Action expected from current function.
 * @cur_date	Date string of current record.
 * @cur_time	Time string of current record.
 * @itv		Interval of time with preceding record (unused here).
 * @file_hdr	System activity file standard header (unused here).
 * @flags	Flags for common options.
 *
 * RETURNS:
 * Pointer on the "timestamp" string.
 ***************************************************************************
 */
__tm_funct_t print_raw_timestamp(void *parm, int action, char *cur_date,
				char *cur_time, unsigned long long itv,
				struct file_header *file_hdr, unsigned int flags)
{
	int utc = !PRINT_LOCAL_TIME(flags) && !PRINT_TRUE_TIME(flags);
	static char pre[80];

	if (action & F_BEGIN) {
		snprintf(pre, 80, "%s%s", cur_time, strlen(cur_date) && utc ? " UTC" : "");
		pre[79] = '\0';
		return pre;
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
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 * @file_actlst	List of (known or unknown) activities in file (unused here).
 *
 * OUT:
 * @parm	Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_xml_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr,
				  struct activity *act[], unsigned int id_seq[],
				  struct file_activity *file_actlst)
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
			file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);

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
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 * @file_actlst	List of (known or unknown) activities in file (unused here).
 *
 * OUT:
 * @parm	Number of tabulations.
 ***************************************************************************
 */
__printf_funct_t print_json_header(void *parm, int action, char *dfile,
				   struct file_magic *file_magic,
				   struct file_header *file_hdr,
				   struct activity *act[], unsigned int id_seq[],
				   struct file_activity *file_actlst)
{
	struct tm rectime, *loc_t;
	char cur_time[TIMESTAMP_LEN];
	int *tab = (int *) parm;

	if (action & F_BEGIN) {
		xprintf(*tab, "{\"sysstat\": {");

		xprintf(++(*tab), "\"hosts\": [");
		xprintf(++(*tab), "{");
		xprintf(++(*tab), "\"nodename\": \"%s\",", file_hdr->sa_nodename);
		xprintf(*tab, "\"sysname\": \"%s\",", file_hdr->sa_sysname);
		xprintf(*tab, "\"release\": \"%s\",", file_hdr->sa_release);

		xprintf(*tab, "\"machine\": \"%s\",", file_hdr->sa_machine);
		xprintf(*tab, "\"number-of-cpus\": %d,",
			file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1);

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
 * @act		Array of activities.
 * @id_seq	Activity sequence.
 * @file_actlst	List of (known or unknown) activities in file.
 ***************************************************************************
 */
__printf_funct_t print_hdr_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr,
				  struct activity *act[], unsigned int id_seq[],
				  struct file_activity *file_actlst)
{
	int i, p;
	struct tm rectime, *loc_t;
	struct file_activity *fal;
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
				 file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1,
				 PLAIN_OUTPUT);

		/* Fill file timestmap structure (rectime) */
		get_file_timestamp_struct(flags, &rectime, file_hdr);
		strftime(cur_time, sizeof(cur_time), "%Y-%m-%d", &rectime);
		printf(_("File date: %s\n"), cur_time);

		if ((loc_t = gmtime((const time_t *) &file_hdr->sa_ust_time)) != NULL) {
			printf(_("File time: "));
			strftime(cur_time, sizeof(cur_time), "%T", loc_t);
			printf("%s UTC\n", cur_time);
		}

		/* File composition: file_header, file_activity, record_header */
		printf(_("File composition: (%d,%d,%d),(%d,%d,%d),(%d,%d,%d)\n"),
		       file_magic->hdr_types_nr[0], file_magic->hdr_types_nr[1], file_magic->hdr_types_nr[2],
		       file_hdr->act_types_nr[0], file_hdr->act_types_nr[1], file_hdr->act_types_nr[2],
		       file_hdr->rec_types_nr[0], file_hdr->rec_types_nr[1], file_hdr->rec_types_nr[2]);

		printf(_("Size of a long int: %d\n"), file_hdr->sa_sizeof_long);
		printf("HZ = %lu\n", file_hdr->sa_hz);
		printf(_("Number of activities in file: %u\n"),
		       file_hdr->sa_act_nr);

		printf(_("List of activities:\n"));
		fal = file_actlst;
		for (i = 0; i < file_hdr->sa_act_nr; i++, fal++) {

			p = get_activity_position(act, fal->id, RESUME_IF_NOT_FOUND);

			printf("%02d: [%02x] ", fal->id, fal->magic);
			if (p >= 0) {
				printf("%-20s", act[p]->name);
			}
			else {
				printf("%-20s", _("Unknown activity"));
			}
			printf(" %c:%4d", fal->has_nr ? 'Y' : 'N', fal->nr);
			if (fal->nr2 > 1) {
				printf("x%d", fal->nr2);
			}
			printf("\t(%d,%d,%d)", fal->types_nr[0], fal->types_nr[1], fal->types_nr[2]);
			if ((p >= 0) && (act[p]->magic == ACTIVITY_MAGIC_UNKNOWN)) {
				printf(_(" \t[Unknown format]"));
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
 * @parm	Specific parameters. Here: number of rows of views to display
 *		or canvas height entered on the command line (@graph_nr), and
 *		max number of views on a single row (@views_per_row).
 * @action	Action expected from current function.
 * @dfile	Name of system activity data file (unused here).
 * @file_magic	System activity file magic header (unused here).
 * @file_hdr	System activity file standard header.
 * @act		Array of activities (unused here).
 * @id_seq	Activity sequence (unused here).
 * @file_actlst	List of (known or unknown) activities in file (unused here).
 ***************************************************************************
 */
__printf_funct_t print_svg_header(void *parm, int action, char *dfile,
				  struct file_magic *file_magic,
				  struct file_header *file_hdr,
				  struct activity *act[], unsigned int id_seq[],
				  struct file_activity *file_actlst)
{
	struct svg_hdr_parm *hdr_parm = (struct svg_hdr_parm *) parm;
	unsigned int height = 0, ht = 0;
	int i, p;

	if (action & F_BEGIN) {
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		printf("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" ");
		printf("\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
		printf("<svg xmlns=\"http://www.w3.org/2000/svg\"");
		if (DISPLAY_TOC(flags)) {
			printf(" xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
		}
		if (action & F_END) {
			printf(">\n");
		}
	}

	if (action & F_MAIN) {
		if (SET_CANVAS_HEIGHT(flags)) {
			/*
			 * Option "-O height=..." used: @graph_nr is
			 * the SVG canvas height set on the command line.
			 */
			height = hdr_parm->graph_nr;
		}
		else {
			height = SVG_H_YSIZE +
				 SVG_C_YSIZE * (DISPLAY_TOC(flags) ? hdr_parm->nr_act_dispd : 0) +
				 SVG_T_YSIZE * hdr_parm->graph_nr;
		}
		if (height < 100) {
			/* Min canvas height is 100 (at least to display "No data") */
			height = 100;
		}
		printf(" width=\"%d\" height=\"%d\""
		       " fill=\"black\" stroke=\"gray\" stroke-width=\"1\">\n",
		       SVG_T_XSIZE * (hdr_parm->views_per_row), height);
		printf("<text x=\"0\" y=\"30\" text-anchor=\"start\" stroke=\"brown\">");
		print_gal_header(localtime((const time_t *) &(file_hdr->sa_ust_time)),
				 file_hdr->sa_sysname, file_hdr->sa_release,
				 file_hdr->sa_nodename, file_hdr->sa_machine,
				 file_hdr->sa_cpu_nr > 1 ? file_hdr->sa_cpu_nr - 1 : 1,
				 PLAIN_OUTPUT);
		printf("</text>\n");
		if (DISPLAY_TOC(flags)) {
			for (i = 0; i < NR_ACT; i++) {
				if (!id_seq[i])
					continue;	/* Activity not in file */

				p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
				if (!IS_SELECTED(act[p]->options) || !act[p]->g_nr)
					continue;	/* Activity not selected or no graph available */

				printf("<a xlink:href=\"#g%d-0\" xlink:title=\"%s\">\n",
				       act[p]->id, act[p]->name);
				printf("<text x=\"10\" y=\"%d\">%s</text></a>\n",
				       SVG_H_YSIZE + ht, act[p]->desc);
				ht += SVG_C_YSIZE;
			}
		}
	}

	if (action & F_END) {
		if (!(action & F_BEGIN)) {
			if (!hdr_parm->graph_nr) {
				/* No views displayed */
				printf("<text x= \"0\" y=\"%d\" text-anchor=\"start\" stroke=\"red\">",
				       SVG_H_YSIZE +
				       SVG_C_YSIZE * (DISPLAY_TOC(flags) ? hdr_parm->nr_act_dispd : 0));
				printf("No data!</text>\n");
			}
			/* Give actual SVG height */
			printf("<!-- Actual canvas height: %d -->\n",
			       SVG_H_YSIZE +
			       SVG_C_YSIZE * (DISPLAY_TOC(flags) ? hdr_parm->nr_act_dispd : 0) +
			       SVG_T_YSIZE * hdr_parm->graph_nr);
		}
		printf("</svg>\n");
	}
}

/*
 ***************************************************************************
 * Count the number of new network interfaces in current sample. If a new
 * interface is found then add it to the linked list starting at
 * @a->item_list.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * Number of new interfaces identified in current sample that were not
 * previously in the list.
 ***************************************************************************
 */
__nr_t count_new_net_dev(struct activity *a, int curr)
{
	int i, nr = 0;
	struct stats_net_dev *sndc;

	for (i = 0; i < a->nr[curr]; i++) {
		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		nr += add_list_item(&(a->item_list), sndc->interface, MAX_IFACE_LEN);
	}

	return nr;
}

/*
 ***************************************************************************
 * Count the number of new network interfaces in current sample. If a new
 * interface is found then add it to the linked list starting at
 * @a->item_list.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * Number of new interfaces identified in current sample that were not
 * previously in the list.
 ***************************************************************************
 */
__nr_t count_new_net_edev(struct activity *a, int curr)
{
	int i, nr = 0;
	struct stats_net_edev *snedc;

	for (i = 0; i < a->nr[curr]; i++) {
		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		nr += add_list_item(&(a->item_list), snedc->interface, MAX_IFACE_LEN);
	}

	return nr;
}

/*
 ***************************************************************************
 * Count the number of new filesystems in current sample. If a new
 * filesystem is found then add it to the linked list starting at
 * @a->item_list.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * Number of new filesystems identified in current sample that were not
 * previously in the list.
 ***************************************************************************
 */
__nr_t count_new_filesystem(struct activity *a, int curr)
{
	int i, nr = 0;
	struct stats_filesystem *sfc;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		nr += add_list_item(&(a->item_list), sfc->fs_name, MAX_FS_LEN);
	}

	return nr;
}

/*
 ***************************************************************************
 * Count the number of new fchosts in current sample. If a new
 * fchost is found then add it to the linked list starting at
 * @a->item_list.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * Number of new fchosts identified in current sample that were not
 * previously in the list.
 ***************************************************************************
 */
__nr_t count_new_fchost(struct activity *a, int curr)
{
	int i, nr = 0;
	struct stats_fchost *sfcc;

	for (i = 0; i < a->nr[curr]; i++) {
		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		nr += add_list_item(&(a->item_list), sfcc->fchost_name, MAX_FCH_LEN);
	}

	return nr;
}

/*
 ***************************************************************************
 * Count the number of new block devices in current sample. If a new
 * block device is found then add it to the linked list starting at
 * @a->item_list.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 *
 * RETURNS:
 * Number of new block devices identified in current sample that were not
 * previously in the list.
 ***************************************************************************
 */
__nr_t count_new_disk(struct activity *a, int curr)
{
	int i, nr = 0;
	struct stats_disk *sdc;

	for (i = 0; i < a->nr[curr]; i++) {
		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		nr += add_list_item(&(a->item_list),
				    get_sa_devname(sdc->major, sdc->minor, flags),
				    MAX_DEV_LEN);
	}

	return nr;
}
