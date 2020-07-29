/*
 * cifsiostat: Report I/O statistics for CIFS filesystems.
 * Copyright (C) 2010 Red Hat, Inc. All Rights Reserved
 * Written by Ivana Varekova <varekova@redhat.com>
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
#include <signal.h>
#include <sys/utsname.h>
#include <ctype.h>

#include "version.h"
#include "cifsiostat.h"
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

unsigned long long uptime_cs[2] = {0, 0};
struct io_cifs *cifs_list = NULL;

int cpu_nr = 0;		/* Nb of processors on the machine */
int flags = 0;		/* Flag for common options and system state */
int dplaces_nr = -1;	/* Number of decimal places */

long interval = 0;
char timestamp[TIMESTAMP_LEN];

struct sigaction alrm_act;

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

#ifdef DEBUG
	fprintf(stderr, _("Options are:\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ --pretty ]\n"
			  "[ -h ] [ -k | -m ] [ -t ] [ -V ] [ --debuginfo ]\n"));
#else
	fprintf(stderr, _("Options are:\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ] [ --pretty ]\n"
			  "[ -h ] [ -k | -m ] [ -t ] [ -V ]\n"));
#endif
	exit(1);
}

/*
 ***************************************************************************
 * SIGALRM signal handler.
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
 * Set every cifs entry to nonexistent status.
 *
 * IN:
 * @clist	Pointer on the start of the linked list.
 ***************************************************************************
 */
void set_cifs_nonexistent(struct io_cifs *clist)
{
	while (clist != NULL) {
		clist->exist = FALSE;
		clist = clist->next;
	}
}

/*
 ***************************************************************************
 * Check if a cifs filesystem is present in the list, and add it if requested.
 *
 * IN:
 * @clist	Address of pointer on the start of the linked list.
 * @name	cifs name.
 *
 * RETURNS:
 * Pointer on the io_cifs structure in the list where the cifs is located
 * (whether it was already in the list or if it has been added).
 * NULL if the cifs name is too long or if the cifs doesn't exist and we
 * don't want to add it.
 ***************************************************************************
 */
struct io_cifs *add_list_cifs(struct io_cifs **clist, char *name)
{
	struct io_cifs *c, *cs;
	int i;

	if (strnlen(name, MAX_NAME_LEN) == MAX_NAME_LEN)
		/* cifs name is too long */
		return NULL;

	while (*clist != NULL) {
		c = *clist;
		if ((i = strcmp(c->name, name)) == 0) {
			/* cifs found in list */
			c->exist = TRUE;
			return c;
		}
		if (i > 0)
			/*
			 * If no group defined and we don't use /proc/diskstats,
			 * insert current device in alphabetical order.
			 * NB: Using /proc/diskstats ("iostat -p ALL") is a bit better than
			 * using alphabetical order because sda10 comes after sda9...
			 */
			break;

		clist = &(c->next);
	}

	/* cifs not found */
	cs = *clist;

	/* Add cifs to the list */
	if ((*clist = (struct io_cifs *) malloc(IO_CIFS_SIZE)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(*clist, 0, IO_CIFS_SIZE);

	c = *clist;
	for (i = 0; i < 2; i++) {
		if ((c->cifs_stats[i] = (struct cifs_st *) malloc(CIFS_ST_SIZE)) == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(c->cifs_stats[i], 0, CIFS_ST_SIZE);
	}
	strncpy(c->name, name, sizeof(c->name));
	c->name[sizeof(c->name) - 1] = '\0';
	c->exist = TRUE;
	c->next = cs;

	return c;
}

/*
 ***************************************************************************
 * Read CIFS-mount directories stats from /proc/fs/cifs/Stats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void read_cifs_stat(int curr)
{
	FILE *fp;
	char line[256];
	char aux[32];
	int start = 0;
	long long unsigned aux_open;
	long long unsigned all_open = 0;
	char cifs_name[MAX_NAME_LEN];
	char name_tmp[MAX_NAME_LEN];
	struct cifs_st scifs;
	struct io_cifs *ci;

	if ((fp = fopen(CIFSSTATS, "r")) == NULL)
		return;

	sprintf(aux, "%%*d) %%%ds",
		MAX_NAME_LEN < 200 ? MAX_NAME_LEN - 1 : 200);

	memset(&scifs, 0, CIFS_ST_SIZE);

	while (fgets(line, sizeof(line), fp) != NULL) {

		/* Read CIFS directory name */
		if (isdigit((unsigned char) line[0]) && sscanf(line, aux , name_tmp) == 1) {
			if (start) {
				scifs.fopens = all_open;
				ci = add_list_cifs(&cifs_list, cifs_name);
				if (ci != NULL) {
					*ci->cifs_stats[curr] = scifs;
				}
				all_open = 0;
			}
			else {
				start = 1;
			}
			strncpy(cifs_name, name_tmp, sizeof(cifs_name));
			cifs_name[sizeof(cifs_name) - 1] = '\0';
			memset(&scifs, 0, CIFS_ST_SIZE);
		}
		else {
			if (!strncmp(line, "Reads:", 6)) {
				/*
				 * SMB1 format: Reads: %llu Bytes: %llu
				 * SMB2 format: Reads: %llu sent %llu failed
				 * If this is SMB2 format then only the first variable (rd_ops) will be set.
				 */
				sscanf(line, "Reads: %llu Bytes: %llu", &scifs.rd_ops, &scifs.rd_bytes);
			}
			else if (!strncmp(line, "Bytes read:", 11)) {
				sscanf(line, "Bytes read: %llu  Bytes written: %llu",
				       &scifs.rd_bytes, &scifs.wr_bytes);
			}
			else if (!strncmp(line, "Writes:", 7)) {
				/*
				 * SMB1 format: Writes: %llu Bytes: %llu
				 * SMB2 format: Writes: %llu sent %llu failed
				 * If this is SMB2 format then only the first variable (wr_ops) will be set.
				 */
				sscanf(line, "Writes: %llu Bytes: %llu", &scifs.wr_ops, &scifs.wr_bytes);
			}
			else if (!strncmp(line, "Opens:", 6)) {
				sscanf(line, "Opens: %llu Closes:%llu Deletes: %llu",
				       &aux_open, &scifs.fcloses, &scifs.fdeletes);
				all_open += aux_open;
			}
			else if (!strncmp(line, "Posix Opens:", 12)) {
				sscanf(line, "Posix Opens: %llu", &aux_open);
				all_open += aux_open;
			}
			else if (!strncmp(line, "Open files:", 11)) {
				sscanf(line, "Open files: %llu total (local), %llu",
				       &all_open, &aux_open);
				all_open += aux_open;
			}
			else if (!strncmp(line, "Closes:", 7)) {
				sscanf(line, "Closes: %llu", &scifs.fcloses);
			}
		}
	}

	if (start) {
		scifs.fopens = all_open;
		ci = add_list_cifs(&cifs_list, cifs_name);
		if (ci != NULL) {
			*ci->cifs_stats[curr] = scifs;
		}
	}

	fclose(fp);
}

/*
 ***************************************************************************
 * Display CIFS stats header.
 *
 * OUT:
 * @fctr	Conversion factor.
 ***************************************************************************
 */
void write_cifs_stat_header(int *fctr)
{
	if (!DISPLAY_PRETTY(flags)) {
		printf("Filesystem            ");
	}
	if (DISPLAY_KILOBYTES(flags)) {
		printf("        rkB/s        wkB/s");
		*fctr = 1024;
	}
	else if (DISPLAY_MEGABYTES(flags)) {
		printf("        rMB/s        wMB/s");
		*fctr = 1024 * 1024;
	}
	else {
		printf("         rB/s         wB/s");
		*fctr = 1;
	}
	printf("    rops/s    wops/s         fo/s         fc/s         fd/s");
	if (DISPLAY_PRETTY(flags)) {
		printf(" Filesystem");
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Write CIFS stats read from /proc/fs/cifs/Stats.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @itv		Interval of time (in 1/100th of a second).
 * @fctr	Conversion factor.
 * @clist	Pointer on the linked list where the cifs is saved.
 * @ioi		Current sample statistics.
 * @ioj		Previous sample statistics.
 ***************************************************************************
 */
void write_cifs_stat(int curr, unsigned long long itv, int fctr,
		     struct io_cifs *clist, struct cifs_st *ioni,
		     struct cifs_st *ionj)
{
	double rbytes, wbytes;

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, "%-22s", clist->name, 0);
	}

	/*       rB/s   wB/s   fo/s   fc/s   fd/s*/
	rbytes = S_VALUE(ionj->rd_bytes, ioni->rd_bytes, itv);
	wbytes = S_VALUE(ionj->wr_bytes, ioni->wr_bytes, itv);
	if (!DISPLAY_UNIT(flags)) {
		rbytes /= fctr;
		wbytes /= fctr;
	}
	cprintf_f(DISPLAY_UNIT(flags) ? UNIT_BYTE : NO_UNIT, 2, 12, 2,
		  rbytes, wbytes);
	cprintf_f(NO_UNIT, 2, 9, 2,
		  S_VALUE(ionj->rd_ops, ioni->rd_ops, itv),
		  S_VALUE(ionj->wr_ops, ioni->wr_ops, itv));
	cprintf_f(NO_UNIT, 3, 12, 2,
		  S_VALUE(ionj->fopens, ioni->fopens, itv),
		  S_VALUE(ionj->fcloses, ioni->fcloses, itv),
		  S_VALUE(ionj->fdeletes, ioni->fdeletes, itv));
	if (DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %s", clist->name, 0);
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Print everything now (stats and uptime).
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void write_stats(int curr, struct tm *rectime)
{
	int fctr = 1;
	unsigned long long itv;
	struct io_cifs *clist;
	struct cifs_st *ioni, *ionj;

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
#ifdef DEBUG
		if (DISPLAY_DEBUG(flags)) {
			fprintf(stderr, "%s\n", timestamp);
		}
#endif
	}

	/* Interval of time, reduced to one processor */
	itv = get_interval(uptime_cs[!curr], uptime_cs[curr]);

	/* Display CIFS stats header */
	write_cifs_stat_header(&fctr);

	for (clist = cifs_list; clist != NULL; clist = clist->next) {

		if (!clist->exist)
			/* Current cifs non existent */
			continue;

		ioni = clist->cifs_stats[curr];
		ionj = clist->cifs_stats[!curr];

#ifdef DEBUG
		if (DISPLAY_DEBUG(flags)) {
			/* Debug output */
			fprintf(stderr, "name=%s itv=%llu fctr=%d ioni{ rd_bytes=%llu "
					"wr_bytes=%llu rd_ops=%llu wr_ops=%llu fopens=%llu "
					"fcloses=%llu fdeletes=%llu}\n",
				clist->name, itv, fctr,
				ioni->rd_bytes, ioni->wr_bytes,
				ioni->rd_ops,   ioni->wr_ops,
				ioni->fopens,   ioni->fcloses,
				ioni->fdeletes);
		}
#endif
		write_cifs_stat(curr, itv, fctr, clist, ioni, ionj);
	}
	printf("\n");
}

/*
 ***************************************************************************
 * Main loop: Read stats from the relevant sources and display them.
 *
 * IN:
 * @count	Number of lines of stats to print.
 * @rectime	Current date and time.
 ***************************************************************************
 */
void rw_io_stat_loop(long int count, struct tm *rectime)
{
	int curr = 1;

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	do {
		/* Every device is potentially nonexistent */
		set_cifs_nonexistent(cifs_list);

		/* Read system uptime in 1/100th of a second */
		read_uptime(&(uptime_cs[curr]));

		/* Read CIFS stats */
		read_cifs_stat(curr);

		/* Get time */
		get_localtime(rectime, 0);

		/* Print results */
		write_stats(curr, rectime);

		if (count > 0) {
			count--;
		}

		if (count) {
			curr ^= 1;
			__pause();
		}
	}
	while (count);
}

/*
 ***************************************************************************
 * Main entry to the cifsiostat program.
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

#ifdef DEBUG
		if (!strcmp(argv[opt], "--debuginfo")) {
			flags |= I_D_DEBUG;
			opt++;
		} else
#endif

		if (!strcmp(argv[opt], "--human")) {
			flags |= I_D_UNIT;
			opt++;
		}

		else if (!strcmp(argv[opt], "--pretty")) {
			/* Display an easy-to-read CIFS report */
			flags |= I_D_PRETTY;
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

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'h':
					/* Option -h is equivalent to --pretty --human */
					flags |= I_D_PRETTY + I_D_UNIT;
					break;

				case 'k':
					if (DISPLAY_MEGABYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in kB/s */
					flags |= I_D_KILOBYTES;
					break;

				case 'm':
					if (DISPLAY_KILOBYTES(flags)) {
						usage(argv[0]);
					}
					/* Display stats in MB/s */
					flags |= I_D_MEGABYTES;
					break;

				case 't':
					/* Display timestamp */
					flags |= I_D_TIMESTAMP;
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

	/* How many processors on this machine? */
	cpu_nr = get_cpu_nr(~0, FALSE);

	get_localtime(&rectime, 0);

	/* Get system name, release number and hostname */
	__uname(&header);
	if (print_gal_header(&rectime, header.sysname, header.release,
			     header.nodename, header.machine, cpu_nr,
			     PLAIN_OUTPUT)) {
		flags |= I_D_ISO;
	}
	printf("\n");

	/* Main loop */
	rw_io_stat_loop(count, &rectime);

	return 0;
}
