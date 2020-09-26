/*
 * sadc: system activity data collector
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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "version.h"
#include "sa.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef HAVE_SENSORS
#include "sensors/sensors.h"
#include "sensors/error.h"
#endif

#ifdef USE_SCCSID
#define SCCSID "@(#)sysstat-" VERSION ": " __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }
#endif

#ifdef TEST
extern time_t __unix_time;
extern int __env;
#endif

extern char *tzname[2];

long interval = -1;
uint64_t flags = 0;

int optz = 0;
char timestamp[2][TIMESTAMP_LEN];

struct file_header file_hdr;
struct record_header record_hdr;

char comment[MAX_COMMENT_LEN];

unsigned int id_seq[NR_ACT];

extern unsigned int hdr_types_nr[];
extern unsigned int act_types_nr[];
extern unsigned int rec_types_nr[];

extern struct activity *act[];
extern __nr_t (*f_count[]) (struct activity *);

struct sigaction alrm_act, int_act;
int sigint_caught = 0;

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
	fprintf(stderr, _("Usage: %s [ options ] [ <interval> [ <count> ] ] [ <outfile> ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -C <comment> ] [ -D ] [ -F ] [ -f ] [ -L ] [ -V ]\n"
			  "[ -S { INT | DISK | IPV6 | POWER | SNMP | XDISK | ALL | XALL } ]\n"));
	exit(1);
}

/*
 ***************************************************************************
 * Collect all activities belonging to a group.
 *
 * IN:
 * @group_id	Group identification number.
 * @opt_f	Optionnal flag to set.
 ***************************************************************************
 */
void collect_group_activities(unsigned int group_id, unsigned int opt_f)
{
	int i;

	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->group & group_id) {
			act[i]->options |= AO_COLLECTED;
			if (opt_f) {
				act[i]->opt_flags |= opt_f;
			}
		}
	}
}

/*
 ***************************************************************************
 * Parse option -S, indicating which activities are to be collected.
 *
 * IN:
 * @argv	Arguments list.
 * @opt		Index in list of arguments.
 ***************************************************************************
 */
void parse_sadc_S_option(char *argv[], int opt)
{
	char *p;
	int i;

	for (p = strtok(argv[opt], ","); p; p = strtok(NULL, ",")) {
		if (!strcmp(p, K_INT)) {
			/* Select group of interrupt activities */
			collect_group_activities(G_INT, AO_F_NULL);
		}
		else if (!strcmp(p, K_DISK)) {
			/* Select group of disk activities */
			collect_group_activities(G_DISK, AO_F_NULL);
		}
		else if (!strcmp(p, K_XDISK)) {
			/* Select group of disk and partition/filesystem activities */
			collect_group_activities(G_DISK + G_XDISK, AO_F_DISK_PART);
		}
		else if (!strcmp(p, K_SNMP)) {
			/* Select group of SNMP activities */
			collect_group_activities(G_SNMP, AO_F_NULL);
		}
		else if (!strcmp(p, K_IPV6)) {
			/* Select group of IPv6 activities */
			collect_group_activities(G_IPV6, AO_F_NULL);
		}
		else if (!strcmp(p, K_POWER)) {
			/* Select group of activities related to power management */
			collect_group_activities(G_POWER, AO_F_NULL);
		}
		else if (!strcmp(p, K_ALL) || !strcmp(p, K_XALL)) {
			/* Select all activities */
			for (i = 0; i < NR_ACT; i++) {

				if (!strcmp(p, K_ALL) && (act[i]->group & G_XDISK))
					/*
					 * Don't select G_XDISK activities
					 * when option -S ALL is used.
					 */
					continue;

				act[i]->options |= AO_COLLECTED;
			}
			if (!strcmp(p, K_XALL)) {
				/* Tell sadc to also collect partition statistics */
				collect_group_activities(G_DISK + G_XDISK, AO_F_DISK_PART);
			}
		}
		else if (!strcmp(p, K_A_NULL)) {
			/* Unselect all activities */
			for (i = 0; i < NR_ACT; i++) {
				act[i]->options &= ~AO_COLLECTED;
			}
		}
		else if (!strncmp(p, "A_", 2)) {
			/* Select activity by name */
			for (i = 0; i < NR_ACT; i++) {
				if (!strcmp(p, act[i]->name)) {
					act[i]->options |= AO_COLLECTED;
					break;
				}
			}
			if (i == NR_ACT) {
				usage(argv[0]);
			}
		}
		else if (!strncmp(p, "-A_", 3)) {
			/* Unselect activity by name */
			for (i = 0; i < NR_ACT; i++) {
				if (!strcmp(p + 1, act[i]->name)) {
					act[i]->options &= ~AO_COLLECTED;
					break;
				}
			}
			if (i == NR_ACT) {
				usage(argv[0]);
			}
		}
		else {
			usage(argv[0]);
		}
	}
}

/*
 ***************************************************************************
 * SIGALRM signal handler. No need to reset handler here.
 *
 * IN:
 * @sig	Signal number.
 ***************************************************************************
 */
void alarm_handler(int sig)
{
	__alarm(interval);
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
	pid_t ppid = getppid();

	sigint_caught = 1;

	if (!optz || (ppid == 1)) {
		/* sadc hasn't been called by sar or sar process is already dead */
		exit(1);
	}

	/*
	 * When starting sar then pressing ctrl/c, SIGINT is received
	 * by sadc, not sar. So send SIGINT to sar so that average stats
	 * can be displayed.
	 */
	if (kill(ppid, SIGINT) < 0) {
		exit(1);
	}
}

/*
 ***************************************************************************
 * Display an error message.
 ***************************************************************************
 */
void p_write_error(void)
{
	fprintf(stderr, _("Cannot write data to system activity file: %s\n"),
		strerror(errno));
	exit(2);
}

/*
 ***************************************************************************
 * Init structures. All of them are init'ed first when they are allocated
 * (done by SREALLOC() macro in sa_sys_init() function).
 * Then, they are init'ed again each time before reading the various system
 * stats to make sure that no stats from a previous reading will remain.
 * This is useful mainly for non sequential activities where some structures
 * may remain unchanged. Such an activity is A_CPU, for which statistics
 * for offline CPU won't be read and their corresponding stats structure
 * won't be overwritten, giving the idea they are still online if we don't
 * reset their structures to zero.
 * Other activities may also assume that structure's fields are initialized
 * when their stats are read.
 ***************************************************************************
 */
void reset_stats(void)
{
	int i;

        for (i = 0; i < NR_ACT; i++) {
		if ((act[i]->nr > 0) && act[i]->_buf0) {
			memset(act[i]->_buf0, 0,
			       (size_t) act[i]->msize * (size_t) act[i]->nr_allocated * (size_t) act[i]->nr2);
		}
	}
}

/*
 ***************************************************************************
 * Count activities items then allocate and init corresponding structures.
 * Activities such A_CPU with AO_ALWAYS_COUNTED flag set are always counted
 * (thus the number of CPU will always be counted even if CPU activity is
 * not collected), but ONLY those that will be collected have allocated
 * structures.
 * This function is called when sadc is started, and when a file is rotated.
 * If a file is rotated and structures are reallocated with a larger size,
 * additional space is not initialized: It doesn't matter as reset_stats()
 * will do it later.
 ***************************************************************************
 */
void sa_sys_init(void)
{
	int i, idx;
	__nr_t f_count_results[NR_F_COUNT];

	/* Init array. Means that no items have been counted yet */
	for (i = 0; i < NR_F_COUNT; i++) {
		f_count_results[i] = -1;
	}

	for (i = 0; i < NR_ACT; i++) {

		if ((HAS_COUNT_FUNCTION(act[i]->options) && IS_COLLECTED(act[i]->options)) ||
		    ALWAYS_COUNT_ITEMS(act[i]->options)) {
			idx = act[i]->f_count_index;

			/* Number of items is not a constant and should be calculated */
			if (f_count_results[idx] >= 0) {
				act[i]->nr_ini = f_count_results[idx];
			}
			else {
				act[i]->nr_ini = (f_count[idx])(act[i]);
				f_count_results[idx] = act[i]->nr_ini;
			}
		}

		if (act[i]->nr_ini > 0) {
			if (act[i]->f_count2) {
				act[i]->nr2 = (*act[i]->f_count2)(act[i]);
			}
			/* else act[i]->nr2 is a constant and doesn't need to be calculated */

			if (!act[i]->nr2) {
				act[i]->nr_ini = 0;
			}
		}

		if (IS_COLLECTED(act[i]->options) && (act[i]->nr_ini > 0)) {
			/* Allocate structures for current activity (using nr_ini and nr2 results) */
			SREALLOC(act[i]->_buf0, void,
				 (size_t) act[i]->msize * (size_t) act[i]->nr_ini * (size_t) act[i]->nr2);
			act[i]->nr_allocated = act[i]->nr_ini;
		}

		if (act[i]->nr_ini <= 0) {
			/* No items found: Invalidate current activity */
			act[i]->options &= ~AO_COLLECTED;
		}

		if (HAS_DETECT_FUNCTION(act[i]->options) && IS_COLLECTED(act[i]->options)) {
			idx = act[i]->f_count_index;

			/* Detect if files needed by activity exist */
			if (f_count_results[idx] < 0) {
				f_count_results[idx] = (f_count[idx])(act[i]);
			}
			if (f_count_results[idx] == 0) {
				/* Files not present */
				act[i]->options &= ~AO_COLLECTED;
			}
		}

		/* Set default activity list */
		id_seq[i] = act[i]->id;
	}
}

/*
 ***************************************************************************
 * Free structures.
 ***************************************************************************
 */
void sa_sys_free(void)
{
	int i;

	for (i = 0; i < NR_ACT; i++) {

		if (act[i]->nr_allocated > 0) {
			if (act[i]->_buf0) {
				free(act[i]->_buf0);
				act[i]->_buf0 = NULL;
				act[i]->nr_allocated = 0;
			}
		}
	}
}

/*
 ***************************************************************************
 * If -L option used, request a non-blocking, exclusive lock on the file.
 * If lock would block, then another process (possibly sadc) has already
 * opened that file => exit.
 *
 * IN:
 * @fd		Output file descriptor.
 * @fatal	Indicate if failing to lock file should be fatal or not.
 * 		If it's not fatal then we'll wait for next iteration and
 * 		try again.
 *
 * RETURNS:
 * 0 on success, or 1 if file couldn't be locked.
 ***************************************************************************
 */
int ask_for_flock(int fd, int fatal)
{
	/* Option -L may be used only if an outfile was specified on the command line */
	if (LOCK_FILE(flags)) {
		/*
		 * Yes: Try to lock file. To make code portable, check for both EWOULDBLOCK
		 * and EAGAIN return codes, and treat them the same (glibc documentation).
		 * Indeed, some Linux ports (e.g. hppa-linux) do not equate EWOULDBLOCK and
		 * EAGAIN like every other Linux port.
		 */
		if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
			if ((((errno == EWOULDBLOCK) || (errno == EAGAIN)) && (fatal == FATAL)) ||
			    ((errno != EWOULDBLOCK) && (errno != EAGAIN))) {
				perror("flock");
				exit(1);
			}
			/* Was unable to lock file: Lock would have blocked... */
			return 1;
		}
		else {
			/* File successfully locked */
			flags |= S_F_FILE_LOCKED;
		}
	}
	return 0;
}

/*
 ***************************************************************************
 * Fill system activity file magic header.
 *
 * IN:
 * @file_magic	System activity file magic header.
 ***************************************************************************
 */
void fill_magic_header(struct file_magic *file_magic)
{
	int i;

	memset(file_magic, 0, FILE_MAGIC_SIZE);

	file_magic->sysstat_magic = SYSSTAT_MAGIC;
	file_magic->format_magic  = FORMAT_MAGIC;

	enum_version_nr(file_magic);

	file_magic->header_size = FILE_HEADER_SIZE;

	for (i = 0; i < 3; i++) {
		file_magic->hdr_types_nr[i] = hdr_types_nr[i];
	}
}

/*
 ***************************************************************************
 * Fill system activity file header, then write it (or print it if stdout).
 *
 * IN:
 * @fd	Output file descriptor. May be stdout.
 ***************************************************************************
 */
void setup_file_hdr(int fd)
{
	int i, j, p;
	struct tm rectime;
	struct utsname header;
	struct file_magic file_magic;
	struct file_activity file_act;

	/* Fill then write file magic header */
	fill_magic_header(&file_magic);

	if (write_all(fd, &file_magic, FILE_MAGIC_SIZE) != FILE_MAGIC_SIZE) {
		p_write_error();
	}

	/* First reset the structure */
	memset(&file_hdr, 0, FILE_HEADER_SIZE);

	/* Then get current date */
	file_hdr.sa_ust_time = (unsigned long long) get_time(&rectime, 0);

	/* OK, now fill the header */
	file_hdr.sa_act_nr      = get_activity_nr(act, AO_COLLECTED, COUNT_ACTIVITIES);
	file_hdr.sa_day         = rectime.tm_mday;
	file_hdr.sa_month       = rectime.tm_mon;
	file_hdr.sa_year        = rectime.tm_year;
	file_hdr.sa_sizeof_long = sizeof(long);
	file_hdr.sa_hz		= HZ;

	for (i = 0; i < 3; i++) {
		file_hdr.act_types_nr[i] = act_types_nr[i];
		file_hdr.rec_types_nr[i] = rec_types_nr[i];
	}
	file_hdr.act_size = FILE_ACTIVITY_SIZE;
	file_hdr.rec_size = RECORD_HEADER_SIZE;

	/*
	 * This is a new file (or stdout): Set sa_cpu_nr field to the number
	 * of CPU of the machine (1 .. CPU_NR + 1). This is the number of CPU, whether
	 * online or offline, when sadc was started.
	 * A_CPU activity is always counted in sa_sys_init(), even if it's not collected.
	 */
	file_hdr.sa_cpu_nr = act[get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND)]->nr_ini;

	/* Get system name, release number, hostname and machine architecture */
	__uname(&header);
	strncpy(file_hdr.sa_sysname, header.sysname, sizeof(file_hdr.sa_sysname));
	file_hdr.sa_sysname[sizeof(file_hdr.sa_sysname) - 1]  = '\0';
	strncpy(file_hdr.sa_nodename, header.nodename, sizeof(file_hdr.sa_nodename));
	file_hdr.sa_nodename[sizeof(file_hdr.sa_nodename) - 1] = '\0';
	strncpy(file_hdr.sa_release, header.release, sizeof(file_hdr.sa_release));
	file_hdr.sa_release[sizeof(file_hdr.sa_release) - 1]  = '\0';
	strncpy(file_hdr.sa_machine, header.machine, sizeof(file_hdr.sa_machine));
	file_hdr.sa_machine[sizeof(file_hdr.sa_machine) - 1]  = '\0';

	/* Get timezone value and save it */
	tzset();
	strncpy(file_hdr.sa_tzname, tzname[0], TZNAME_LEN);
	file_hdr.sa_tzname[TZNAME_LEN - 1] = '\0';

	/* Write file header */
	if (write_all(fd, &file_hdr, FILE_HEADER_SIZE) != FILE_HEADER_SIZE) {
		p_write_error();
	}

	/* Reset file_activity structure (in case some unknown extra fields exist) */
	memset(&file_act, 0, FILE_ACTIVITY_SIZE);

	/* Write activity list */
	for (i = 0; i < NR_ACT; i++) {

		/*
		 * Activity sequence given by id_seq array.
		 * Sequence must be the same for stdout as for output file.
		 */
		if (!id_seq[i])
			continue;
		if ((p = get_activity_position(act, id_seq[i], RESUME_IF_NOT_FOUND)) < 0)
			continue;

		if (IS_COLLECTED(act[p]->options)) {
			file_act.id    = act[p]->id;
			file_act.magic = act[p]->magic;
			file_act.nr    = act[p]->nr_ini;
			file_act.nr2   = act[p]->nr2;
			file_act.size  = act[p]->fsize;
			for (j = 0; j < 3; j++) {
				file_act.types_nr[j] = act[p]->gtypes_nr[j];
			}

			file_act.has_nr = HAS_COUNT_FUNCTION(act[p]->options);

			if (write_all(fd, &file_act, FILE_ACTIVITY_SIZE) != FILE_ACTIVITY_SIZE) {
				p_write_error();
			}
		}
	}

	return;
}

/*
 ***************************************************************************
 * Write the new number of CPU after the RESTART record in file.
 *
 * IN:
 * @ofd		Output file descriptor.
 ***************************************************************************
 */
void write_new_cpu_nr(int ofd)
{
	int p;

	p = get_activity_position(act, A_CPU, EXIT_IF_NOT_FOUND);

	if (write_all(ofd, &(act[p]->nr_ini), sizeof(__nr_t)) != sizeof(__nr_t)) {
		p_write_error();
	}
}

/*
 ***************************************************************************
 * sadc called with interval and count parameters not set:
 * Write a dummy record notifying a system restart, or insert a comment in
 * binary data file if option -C has been used.
 * Writing a dummy record should typically be done at boot time,
 * before the cron daemon is started to avoid conflict with sa1/sa2 scripts.
 *
 * IN:
 * @ofd		Output file descriptor.
 * @rtype	Record type to write (restart or comment).
 ***************************************************************************
 */
void write_special_record(int ofd, int rtype)
{
	struct tm rectime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL};

	/* Check if file is locked */
	if (!FILE_LOCKED(flags)) {
		ask_for_flock(ofd, FATAL);
	}

	/* Reset the structure (sane to do it, as other fields may be added in the future) */
	memset(&record_hdr, 0, RECORD_HEADER_SIZE);

	/* Set record type */
	record_hdr.record_type = rtype;

	/* Save time */
	record_hdr.ust_time = (unsigned long long) get_time(&rectime, 0);

	record_hdr.hour   = rectime.tm_hour;
	record_hdr.minute = rectime.tm_min;
	record_hdr.second = rectime.tm_sec;

	/* Write record now */
	if (write_all(ofd, &record_hdr, RECORD_HEADER_SIZE) != RECORD_HEADER_SIZE) {
		p_write_error();
	}

	if (rtype == R_RESTART) {
		/* Also write the new number of CPU */
		write_new_cpu_nr(ofd);
	}
	else if (rtype == R_COMMENT) {
		/* Also write the comment */
		if (write_all(ofd, comment, MAX_COMMENT_LEN) != MAX_COMMENT_LEN) {
			p_write_error();
		}
	}
}

/*
 ***************************************************************************
 * Write stats (or print them if stdout).
 *
 * IN:
 * @ofd		Output file descriptor. May be stdout.
 ***************************************************************************
 */
void write_stats(int ofd)
{
	int i, p;

	/* Try to lock file */
	if (!FILE_LOCKED(flags)) {
		if (ask_for_flock(ofd, NON_FATAL))
			/*
			 * Unable to lock file:
			 * Wait for next iteration to try again to save data.
			 */
			return;
	}

	/* Write record header */
	if (write_all(ofd, &record_hdr, RECORD_HEADER_SIZE) != RECORD_HEADER_SIZE) {
		p_write_error();
	}

	/* Then write all statistics */
	for (i = 0; i < NR_ACT; i++) {

		if (!id_seq[i])
			continue;
		if ((p = get_activity_position(act, id_seq[i], RESUME_IF_NOT_FOUND)) < 0)
			continue;

		if (IS_COLLECTED(act[p]->options)) {
			if (HAS_COUNT_FUNCTION(act[p]->options) && (act[p]->f_count_index >= 0)) {
				if (write_all(ofd, &(act[p]->_nr0), sizeof(__nr_t)) != sizeof(__nr_t)) {
					p_write_error();
				}
			}
			if (write_all(ofd, act[p]->_buf0, act[p]->fsize * act[p]->_nr0 * act[p]->nr2) !=
			    (act[p]->fsize * act[p]->_nr0 * act[p]->nr2)) {
				p_write_error();
			}
		}
	}
}

/*
 ***************************************************************************
 * Create a system activity daily data file.
 *
 * IN:
 * @ofile	Name of output file.
 *
 * OUT:
 * @ofd		Output file descriptor.
 ***************************************************************************
 */
void create_sa_file(int *ofd, char *ofile)
{
	if ((*ofd = open(ofile, O_CREAT | O_WRONLY,
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
		goto create_error;

	/* Try to lock file */
	ask_for_flock(*ofd, FATAL);

	/* Truncate file */
	if (ftruncate(*ofd, 0) >= 0) {

		/* Write file header */
		setup_file_hdr(*ofd);

		return;
	}

create_error:
	fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
	exit(2);
}

/*
 ***************************************************************************
 * Get descriptor for stdout.
 *
 * IN:
 * @stdfd	A value >= 0 indicates that stats data should also
 *		be written to stdout.
 *
 * OUT:
 * @stdfd	Stdout file descriptor.
 ***************************************************************************
 */
void open_stdout(int *stdfd)
{
	if (*stdfd >= 0) {
		if ((*stdfd = dup(STDOUT_FILENO)) < 0) {
			perror("dup");
			exit(4);
		}
		/* Write file header on STDOUT */
		setup_file_hdr(*stdfd);
	}
}

/*
 ***************************************************************************
 * Get descriptor for output file and write its header.
 * We may enter this function several times (when we rotate a file).
 * NB: If data are appended to an existing file then the format must be
 * strictly that expected by current version.
 *
 * IN:
 * @ofile		Name of output file.
 * @restart_mark	TRUE if sadc called with interval (and count) not
 * 			set, and no comment given (so we are going to insert
 * 			a restart mark into the file).
 *
 * OUT:
 * @ofd			Output file descriptor.
 ***************************************************************************
 */
void open_ofile(int *ofd, char ofile[], int restart_mark)
{
	struct file_magic file_magic;
	struct file_activity file_act[NR_ACT];
	struct tm rectime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL};
	ssize_t sz;
	int i, p;

	if (!ofile[0])
		return;

	/* Try to open file and check that data can be appended to it */
	if ((*ofd = open(ofile, O_APPEND | O_RDWR)) < 0) {
		if (errno == ENOENT) {
			/* File doesn't exist: Create it */
			create_sa_file(ofd, ofile);
			return;
		}
		fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
		exit(2);
	}

	/* Read file magic header */
	sz = read(*ofd, &file_magic, FILE_MAGIC_SIZE);
	if (!sz) {
		close(*ofd);
		/* This is an empty file: Create it again */
		create_sa_file(ofd, ofile);
		return;
	}

	/* Test various values ("strict writing" rule) */
	if ((sz != FILE_MAGIC_SIZE) ||
	    (file_magic.sysstat_magic != SYSSTAT_MAGIC) ||
	    (file_magic.format_magic != FORMAT_MAGIC) ||
	    (file_magic.header_size != FILE_HEADER_SIZE) ||
	    (file_magic.hdr_types_nr[0] != FILE_HEADER_ULL_NR) ||
	    (file_magic.hdr_types_nr[1] != FILE_HEADER_UL_NR) ||
	    (file_magic.hdr_types_nr[2] != FILE_HEADER_U_NR)) {
		if (FORCE_FILE(flags)) {
			close(*ofd);
			/* -F option used: Truncate file */
			create_sa_file(ofd, ofile);
			return;
		}
#ifdef DEBUG
		fprintf(stderr, "%s: Size read=%ld sysstat_magic=%x format_magic=%x header_size=%u header=%d,%d,%d\n",
			__FUNCTION__, sz, file_magic.sysstat_magic, file_magic.format_magic, file_magic.header_size,
			file_magic.hdr_types_nr[0], file_magic.hdr_types_nr[1], file_magic.hdr_types_nr[2]);
#endif
		/* Display error message and exit */
		handle_invalid_sa_file(*ofd, &file_magic, ofile, sz);
	}

	/* Read file standard header */
	if ((sz = read(*ofd, &file_hdr, FILE_HEADER_SIZE)) != FILE_HEADER_SIZE) {
#ifdef DEBUG
		fprintf(stderr, "%s: Size read=%ld\n",
			__FUNCTION__, sz);
#endif
		goto append_error;
	}

	/*
	 * If we are using the standard daily data file (file specified
	 * as "-" on the command line) and it is from a past month,
	 * then overwrite (truncate) it.
	 */
	get_time(&rectime, 0);

	if (((file_hdr.sa_month != rectime.tm_mon) ||
	    (file_hdr.sa_year != rectime.tm_year)) &&
	    WANT_SA_ROTAT(flags)) {
		close(*ofd);
		create_sa_file(ofd, ofile);
		return;
	}

	/* OK: It's a true system activity file */
	if (!file_hdr.sa_act_nr || (file_hdr.sa_act_nr > NR_ACT)) {
#ifdef DEBUG
		fprintf(stderr, "%s: sa_act_nr=%d\n",
			__FUNCTION__, file_hdr.sa_act_nr);
#endif
		/*
		 * No activities at all or at least one unknown activity:
		 * Cannot append data to such a file.
		 */
		goto append_error;
	}

	/* Other sanity checks ("strict writing" rule) */
	if ((file_hdr.act_size != FILE_ACTIVITY_SIZE) ||
	    (file_hdr.act_types_nr[0] != FILE_ACTIVITY_ULL_NR) ||
	    (file_hdr.act_types_nr[1] != FILE_ACTIVITY_UL_NR) ||
	    (file_hdr.act_types_nr[2] != FILE_ACTIVITY_U_NR) ||
	    (file_hdr.rec_size != RECORD_HEADER_SIZE) ||
	    (file_hdr.rec_types_nr[0] != RECORD_HEADER_ULL_NR) ||
	    (file_hdr.rec_types_nr[1] != RECORD_HEADER_UL_NR) ||
	    (file_hdr.rec_types_nr[2] != RECORD_HEADER_U_NR)) {
#ifdef DEBUG
		fprintf(stderr, "%s: act_size=%u act=%d,%d,%d rec_size=%u rec=%d,%d,%d\n",
			__FUNCTION__, file_hdr.act_size,
			file_hdr.act_types_nr[0], file_hdr.act_types_nr[1], file_hdr.act_types_nr[2],
			file_hdr.rec_size,
			file_hdr.rec_types_nr[0], file_hdr.rec_types_nr[1], file_hdr.rec_types_nr[2]);
#endif
		goto append_error;
	}

	for (i = 0; i < file_hdr.sa_act_nr; i++) {

		/* Read current activity in list */
		if (read(*ofd, &file_act[i], FILE_ACTIVITY_SIZE) != FILE_ACTIVITY_SIZE) {
#ifdef DEBUG
			fprintf(stderr, "%s: Wrong size for file_activity\n",
				__FUNCTION__);
#endif
			handle_invalid_sa_file(*ofd, &file_magic, ofile, 0);
		}

		p = get_activity_position(act, file_act[i].id, RESUME_IF_NOT_FOUND);

		if ((p < 0) || (act[p]->fsize != file_act[i].size) ||
		    (act[p]->magic != file_act[i].magic)) {
#ifdef DEBUG
			if (p < 0) {
				fprintf(stderr, "%s: p=%d\n", __FUNCTION__, p);
			}
			else {
				fprintf(stderr, "%s: %s: size=%d/%d magic=%x/%x\n",
					__FUNCTION__, act[p]->name, act[p]->fsize, file_act[i].size,
					act[p]->magic, file_act[i].magic);
			}
#endif
			/*
			 * Unknown activity in list or item size has changed or
			 * unknown activity format: Cannot append data to such a file
			 * ("strict writing" rule).
			 */
			goto append_error;
		}

		if ((file_act[i].nr <= 0) || (file_act[i].nr2 <= 0) ||
		    (file_act[i].nr > act[p]->nr_max) ||
		    (file_act[i].nr2 > NR2_MAX)) {
#ifdef DEBUG
			fprintf(stderr, "%s: %s: nr=%d nr_max=%d nr2=%d\n",
				__FUNCTION__, act[p]->name, file_act[i].nr,
				act[p]->nr_max, file_act[i].nr2);
#endif
			/*
			 * Number of items and subitems should never be zero (or negative)
			 * or greater than their upper limit.
			 */
			goto append_error;
		}

		if ((file_act[i].types_nr[0] != act[p]->gtypes_nr[0]) ||
		    (file_act[i].types_nr[1] != act[p]->gtypes_nr[1]) ||
		    (file_act[i].types_nr[2] != act[p]->gtypes_nr[2])) {
#ifdef DEBUG
			fprintf(stderr, "%s: %s: types=%d,%d,%d/%d,%d,%d\n",
				__FUNCTION__, act[p]->name,
				file_act[i].types_nr[0], file_act[i].types_nr[1], file_act[i].types_nr[2],
				act[p]->gtypes_nr[0], act[p]->gtypes_nr[1], act[p]->gtypes_nr[2]);
#endif
			/*
			 * Composition of structure containing statsitics cannot
			 * be different from that known by current version.
			 */
			goto append_error;
		}

		if ((file_act[i].has_nr && (act[p]->f_count_index < 0)) ||
		    (!file_act[i].has_nr && (act[p]->f_count_index >= 0) && HAS_COUNT_FUNCTION(act[p]->options))) {
#ifdef DEBUG
			fprintf(stderr, "%s: %s: has_nr=%d count_index=%d\n",
				__FUNCTION__, act[p]->name, file_act[i].has_nr, act[p]->f_count_index);
#endif
			/*
			 * For every activity whose number of items is not a constant,
			 * a value giving the number of structures to read should exist.
			 */
			goto append_error;
		}
	}

	/*
	 * OK: (Almost) all tests successfully passed.
	 * List of activities from the file prevails over that of the user.
	 * So unselect all of them. And reset activity sequence.
	 */
	for (i = 0; i < NR_ACT; i++) {
		act[i]->options &= ~AO_COLLECTED;
		id_seq[i] = 0;
	}

	for (i = 0; i < file_hdr.sa_act_nr; i++) {

		p = get_activity_position(act, file_act[i].id, EXIT_IF_NOT_FOUND);

		/*
		 * sar doesn't expect a number of items equal to 0.
		 * Yet @nr_ini may be 0 if no items have been found on current machine.
		 * Since we are appending data to a file, set @nr_ini to the value of the file.
		 * Stats saved in file will all be 0 for that activity if no items exist on
		 * the machine.
		 * NB: We must preserve the value read for A_CPU when a LINUX RESTART is inserted.
		 */
		if (!ALWAYS_COUNT_ITEMS(act[p]->options) || !act[p]->nr_ini) {
			act[p]->nr_ini = file_act[i].nr;
		}

		/*
		 * Force number of sub-items to that of the file, and reallocate structures.
		 * Note: Structures have been allocated in sa_sys_init() only for activities
		 * that are collected. But since activities from file now prevail over them,
		 * we need to reallocate.
		 */
		act[p]->nr2 = file_act[i].nr2;
		if (act[p]->nr_ini > act[p]->nr_allocated) {
			act[p]->nr_allocated = act[p]->nr_ini;
		}
		SREALLOC(act[p]->_buf0, void,
			 (size_t) act[p]->msize * (size_t) act[p]->nr_allocated * (size_t) act[p]->nr2);

		/* Save activity sequence */
		id_seq[i] = file_act[i].id;
		act[p]->options |= AO_COLLECTED;
	}

	return;

append_error:

	close(*ofd);
	if (FORCE_FILE(flags)) {
		/* Truncate file */
		create_sa_file(ofd, ofile);
	}
	else {
		fprintf(stderr, _("Cannot append data to that file (%s)\n"), ofile);
		exit(1);
	}
}

/*
 ***************************************************************************
 * Read statistics from various system files.
 ***************************************************************************
 */
void read_stats(void)
{
	int i;

	/* Read system uptime in 1/100th of a second */
	read_uptime(&(record_hdr.uptime_cs));

	for (i = 0; i < NR_ACT; i++) {
		if (IS_COLLECTED(act[i]->options)) {
			/* Read statistics for current activity */
			(*act[i]->f_read)(act[i]);
		}
	}
}

/*
 ***************************************************************************
 * Main loop: Read stats from the relevant sources and display them.
 *
 * IN:
 * @count	Number of lines of stats to display.
 * @stdfd	Stdout file descriptor.
 * @ofd		Output file descriptor.
 * @ofile	Name of output file.
 * @sa_dir	If not an empty string, contains the alternate location of
 * 		daily data files.
 ***************************************************************************
 */
void rw_sa_stat_loop(long count, int stdfd, int ofd, char ofile[],
		     char sa_dir[])
{
	int do_sa_rotat = 0;
	uint64_t save_flags;
	char new_ofile[MAX_FILE_LEN] = "";
	struct tm rectime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL};

	/* Set a handler for SIGINT */
	memset(&int_act, 0, sizeof(int_act));
	int_act.sa_handler = int_handler;
	sigaction(SIGINT, &int_act, NULL);

	/* Main loop */
	do {
		/* Init all structures */
		reset_stats();
		memset(&record_hdr, 0, RECORD_HEADER_SIZE);

		/* Save time */
		record_hdr.ust_time = (unsigned long long) get_time(&rectime, 0);
		record_hdr.hour     = rectime.tm_hour;
		record_hdr.minute   = rectime.tm_min;
		record_hdr.second   = rectime.tm_sec;

		/* Set record type */
		if (do_sa_rotat) {
			record_hdr.record_type = R_LAST_STATS;
		}
		else {
			record_hdr.record_type = R_STATS;
		}

		/* Read then write stats */
		read_stats();

		if (stdfd >= 0) {
			save_flags = flags;
			flags &= ~S_F_LOCK_FILE;
			write_stats(stdfd);
			flags = save_flags;
		}

		/* If the record type was R_LAST_STATS, tag it R_STATS before writing it */
		record_hdr.record_type = R_STATS;
		if (ofile[0]) {
			write_stats(ofd);
		}

		if (do_sa_rotat) {
			/*
			 * Stats are written at the end of previous file *and* at the
			 * beginning of the new one (outfile must have been specified
			 * as '-' on the command line).
			 */
			do_sa_rotat = FALSE;

			if (fdatasync(ofd) < 0) {
				/* Flush previous file */
				perror("fdatasync");
				exit(4);
			}
			close(ofd);
			strcpy(ofile, new_ofile);

			/* Recalculate number of system items and reallocate structures */
			sa_sys_init();

			/*
			 * Open and init new file.
			 * This is also used to set activity sequence to that of the file
			 * if the file already exists.
			 */
			open_ofile(&ofd, ofile, FALSE);

			/*
			 * Rewrite header and activity sequence to stdout since
			 * number of items may have changed.
			 */
			if (stdfd >= 0) {
				setup_file_hdr(stdfd);
			}

			/* Write stats to file again */
			write_stats(ofd);
		}

		/* Flush data */
		fflush(stdout);
		if (FDATASYNC(flags)) {
			/* If indicated, sync the data to media */
			if (fdatasync(ofd) < 0) {
				perror("fdatasync");
				exit(4);
			}
		}

		if (count > 0) {
			count--;
		}

		if (count) {
			/* Wait for a signal (probably SIGALRM or SIGINT) */
			__pause();
		}

		if (sigint_caught)
			/* SIGINT caught: Stop now */
			break;

		/* Rotate activity file if necessary */
		if (WANT_SA_ROTAT(flags)) {
			/* The user specified '-' as the filename to use */
			strncpy(new_ofile, sa_dir, sizeof(new_ofile) - 1);
			new_ofile[sizeof(new_ofile) - 1] = '\0';
			set_default_file(new_ofile, 0, USE_SA_YYYYMMDD(flags));

			if (strcmp(ofile, new_ofile)) {
				do_sa_rotat = TRUE;
			}
		}
	}
	while (count);

	/* Close file descriptors if they have actually been used */
	CLOSE(stdfd);
	CLOSE(ofd);
}

/*
 ***************************************************************************
 * Main entry to the program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int opt = 0;
	char ofile[MAX_FILE_LEN], sa_dir[MAX_FILE_LEN];
	int stdfd = 0, ofd = -1;
	int restart_mark;
	long count = 0;

#ifdef TEST
	fprintf(stderr, "TEST MODE\n");
#endif

	/* Get HZ */
	get_HZ();

	/* Compute page shift in kB */
	get_kb_shift();

	ofile[0] = sa_dir[0] = comment[0] = '\0';

#ifdef HAVE_SENSORS
	/* Initialize sensors, let it use the default cfg file */
	int err = sensors_init(NULL);
	if (err) {
		fprintf(stderr, "sensors_init: %s\n", sensors_strerror(err));
	}
#endif /* HAVE_SENSORS */

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	while (++opt < argc) {

		if (!strcmp(argv[opt], "-S")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			parse_sadc_S_option(argv, opt);
		}

		else if (!strcmp(argv[opt], "-D")) {
			flags |= S_F_SA_YYYYMMDD;
		}

		else if (!strcmp(argv[opt], "-F")) {
			flags |= S_F_FORCE_FILE;
		}

		else if (!strcmp(argv[opt], "-L")) {
			flags |= S_F_LOCK_FILE;
		}

		else if (!strcmp(argv[opt], "-V")) {
			print_version();
		}

		else if (!strcmp(argv[opt], "-Z")) {
			/* Set by sar command */
			optz = 1;
		}

		else if (!strcmp(argv[opt], "-f")) {
			flags |= S_F_FDATASYNC;
		}

		else if (!strcmp(argv[opt], "-C")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			strncpy(comment, argv[opt], sizeof(comment));
			comment[sizeof(comment) - 1] = '\0';
			if (!strlen(comment)) {
				usage(argv[0]);
			}
		}

#ifdef TEST
		else if (!strncmp(argv[opt], "--getenv", 8)) {
			__env = TRUE;
		}

		else if (!strncmp(argv[opt], "--unix_time=", 12)) {
			if (strspn(argv[opt] + 12, DIGITS) != strlen(argv[opt] + 12)) {
				usage(argv[0]);
			}
			__unix_time = atoll(argv[opt] + 12);
		}
#endif

		else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
			if (ofile[0] || WANT_SA_ROTAT(flags)) {
				/* Outfile already specified */
				usage(argv[0]);
			}
			stdfd = -1;	/* Don't write to STDOUT */
			if (!strcmp(argv[opt], "-")) {
				/* File name set to '-' */
				flags |= S_F_SA_ROTAT;
			}
			else if (!strncmp(argv[opt], "-", 1)) {
				/* Bad option */
				usage(argv[0]);
			}
			else {
				/* Write data to file */
				strncpy(ofile, argv[opt], sizeof(ofile));
				ofile[sizeof(ofile) - 1] = '\0';
			}
		}

		else if (interval < 0) {
			/* Get interval */
			interval = atol(argv[opt]);
			if (interval < 1) {
				usage(argv[0]);
			}
			count = -1;
		}

		else if (count <= 0) {
			/* Get count value */
			count = atol(argv[opt]);
			if (count < 1) {
				usage(argv[0]);
			}
		}

		else {
			usage(argv[0]);
		}
	}

	/* Process file entered on the command line */
	if (WANT_SA_ROTAT(flags)) {
		/* File name set to '-' */
		set_default_file(ofile, 0, USE_SA_YYYYMMDD(flags));
	}
	else if (ofile[0]) {
		/*
		 * A file (or directory) has been explicitly entered
		 * on the command line.
		 * Should ofile be a directory, it will be the alternate
		 * location for sa files. So save it.
		 */
		strcpy(sa_dir, ofile);
		/* Check if this is an alternate directory for sa files */
		if (check_alt_sa_dir(ofile, 0, USE_SA_YYYYMMDD(flags))) {
			/*
			 * Yes, it was a directory.
			 * ofile now contains the full path to current
			 * standard daily data file.
			 */
			flags |= S_F_SA_ROTAT;
		}
		else {
			/* No: So we can clear sa_dir */
			sa_dir[0] = '\0';
		}
	}

	/*
	 * If option -Z used, write to STDOUT even if a filename
	 * has been entered on the command line.
	 */
	if (optz) {
		stdfd = 0;
	}

	if (!ofile[0]) {
		/* -L option ignored when writing to STDOUT */
		flags &= ~S_F_LOCK_FILE;
	}

	/* Init structures according to machine architecture */
	sa_sys_init();

	/* At least one activity must be collected */
	if (!get_activity_nr(act, AO_COLLECTED, COUNT_ACTIVITIES)) {
		/* Requested activities not available: Exit */
		print_collect_error();
	}

	if ((interval < 0) && !comment[0]) {
		/*
		 * Interval (and count) not set, and no comment given
		 * => We are going to insert a restart mark.
		 */
		restart_mark = TRUE;
	}
	else {
		restart_mark = FALSE;
	}

	/*
	 * Open output file then STDOUT. Write header for each of them.
	 * NB: Output file must be opened first, because we may change
	 * the activities collected AND the activity sequence to that
	 * of the file, and the activities collected and activity sequence
	 * written on STDOUT must be consistent to those of the file.
	 */
	open_ofile(&ofd, ofile, restart_mark);
	open_stdout(&stdfd);

	if (interval < 0) {
		if (ofd >= 0) {
			/*
			 * Interval (and count) not set:
			 * Write a dummy record, or insert a comment, then exit.
			 * NB: Never write such a dummy record on stdout since
			 * sar never expects it.
			 */
			if (comment[0]) {
				write_special_record(ofd, R_COMMENT);
			}
			else {
				write_special_record(ofd, R_RESTART);
			}

			/* Close file descriptor */
			CLOSE(ofd);
		}

		/* Free structures */
		sa_sys_free();
		exit(0);
	}

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	__alarm(interval);

	/* Main loop */
	rw_sa_stat_loop(count, stdfd, ofd, ofile, sa_dir);

#ifdef HAVE_SENSORS
	/* Cleanup sensors */
	sensors_cleanup();
#endif /* HAVE_SENSORS */

	/* Free structures */
	sa_sys_free();

	return 0;
}
