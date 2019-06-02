/*
 * pidstat: Report statistics for Linux tasks
 * (C) 2007-2019 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <regex.h>
#include <linux/sched.h>

#include "version.h"
#include "pidstat.h"
#include "common.h"
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

unsigned long long tot_jiffies[3] = {0, 0, 0};
unsigned long long uptime_cs[3] = {0, 0, 0};
struct pid_stats *st_pid_list[3] = {NULL, NULL, NULL};
unsigned int *pid_array = NULL;
struct pid_stats st_pid_null;
struct tm ps_tstamp[3];
char commstr[MAX_COMM_LEN];
char userstr[MAX_USER_LEN];
char procstr[MAX_COMM_LEN];
int show_threads = FALSE;

unsigned int pid_nr = 0;	/* Nb of PID to display */
int cpu_nr = 0;			/* Nb of processors on the machine */
unsigned long tlmkb;		/* Total memory in kB */
long interval = -1;
long count = 0;
unsigned int pidflag = 0;	/* General flags */
unsigned int tskflag = 0;	/* TASK/CHILD stats */
unsigned int actflag = 0;	/* Activity flag */

struct sigaction alrm_act, int_act, chld_act;
int signal_caught = 0;

int dplaces_nr = -1;		/* Number of decimal places */

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
	fprintf(stderr, _("Usage: %s [ options ] [ <interval> [ <count> ] ] [ -e <program> <args> ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -d ] [ -H ] [ -h ] [ -I ] [ -l ] [ -R ] [ -r ] [ -s ] [ -t ] [ -U [ <username> ] ]\n"
			  "[ -u ] [ -V ] [ -v ] [ -w ] [ -C <command> ] [ -G <process_name> ]\n"
			  "[ -p { <pid> [,...] | SELF | ALL } ] [ -T { TASK | CHILD | ALL } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ --human ]\n"));
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
 * SIGINT and SIGCHLD signals handler.
 *
 * IN:
 * @sig	Signal number.
 ***************************************************************************
 */
void sig_handler(int sig)
{
	signal_caught = 1;
}

/*
 ***************************************************************************
 * Initialize uptime variables.
 ***************************************************************************
 */
void init_stats(void)
{
	memset(&st_pid_null, 0, PID_STATS_SIZE);
}

/*
 ***************************************************************************
 * Allocate structures for PIDs entered on command line.
 *
 * IN:
 * @len	Number of PIDs entered on the command line.
 ***************************************************************************
 */
void salloc_pid_array(unsigned int len)
{
	if ((pid_array = (unsigned int *) malloc(sizeof(unsigned int) * len)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(pid_array, 0, sizeof(int) * len);
}

/*
 ***************************************************************************
 * Allocate structures for PIDs to read.
 *
 * IN:
 * @len	Number of PIDs (and TIDs) on the system.
 ***************************************************************************
 */
void salloc_pid(unsigned int len)
{
	short i;

	for (i = 0; i < 3; i++) {
		if ((st_pid_list[i] = (struct pid_stats *) calloc(len, PID_STATS_SIZE)) == NULL) {
			perror("calloc");
			exit(4);
		}
	}
}

/*
 ***************************************************************************
 * Reallocate structures for PIDs to read.
 ***************************************************************************
 */
void realloc_pid(void)
{
	short i;
	unsigned int new_size = 2 * pid_nr;

	for (i = 0; i < 3; i++) {
		if ((st_pid_list[i] = (struct pid_stats *) realloc(st_pid_list[i], PID_STATS_SIZE * new_size)) == NULL) {
			perror("realloc");
			exit(4);
		}
		memset(st_pid_list[i] + pid_nr, 0, PID_STATS_SIZE * (new_size - pid_nr));
	}

	pid_nr = new_size;
}

/*
 ***************************************************************************
 * Free PID list structures.
 ***************************************************************************
 */
void sfree_pid(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		free(st_pid_list[i]);
	}
}

/*
 ***************************************************************************
 * Check flags and set default values.
 ***************************************************************************
 */
void check_flags(void)
{
	unsigned int act = 0;

	/* Display CPU usage for active tasks by default */
	if (!actflag) {
		actflag |= P_A_CPU;
	}

	if (!DISPLAY_PID(pidflag)) {
		pidflag |= P_D_ACTIVE_PID + P_D_PID + P_D_ALL_PID;
	}

	if (!tskflag) {
		tskflag |= P_TASK;
	}

	/* Check that requested activities are available */
	if (DISPLAY_TASK_STATS(tskflag)) {
		act |= P_A_CPU + P_A_MEM + P_A_IO + P_A_CTXSW
		     + P_A_STACK + P_A_KTAB + P_A_RT;
	}
	if (DISPLAY_CHILD_STATS(tskflag)) {
		act |= P_A_CPU + P_A_MEM;
	}

	actflag &= act;

	if (!actflag) {
		fprintf(stderr, _("Requested activities not available\n"));
		exit(1);
	}
}

/*
 ***************************************************************************
 * Look for the PID in the list of PIDs entered on the command line, and
 * store it if necessary.
 *
 * IN:
 * @pid_array_nr	Length of the PID list.
 * @pid			PID to search.
 *
 * OUT:
 * @pid_array_nr	New length of the PID list.
 *
 * RETURNS:
 * Returns the position of the PID in the list.
 ***************************************************************************
 */
int update_pid_array(unsigned int *pid_array_nr, unsigned int pid)
{
	unsigned int i;

	for (i = 0; i < *pid_array_nr; i++) {
		if (pid_array[i] == pid)
			break;
	}

	if (i == *pid_array_nr) {
		/* PID not found: Store it */
		(*pid_array_nr)++;
		pid_array[i] = pid;
	}

	return i;
}

/*
 ***************************************************************************
 * Get pointer on task's command string.
 * If this is a thread then return the short command name so that threads
 * can still be identified.
 *
 * IN:
 * @pst		Pointer on structure with process stats and command line.
 ***************************************************************************
 */
char *get_tcmd(struct pid_stats *pst)
{
	if (DISPLAY_CMDLINE(pidflag) && strlen(pst->cmdline) && !pst->tgid)
		/* Option "-l" used */
		return pst->cmdline;
	else
		return pst->comm;
}

/*
 ***************************************************************************
 * Display process command name or command line.
 *
 * IN:
 * @pst		Pointer on structure with process stats and command line.
 ***************************************************************************
 */
void print_comm(struct pid_stats *pst)
{
	char *p;

	/* Get pointer on task's command string */
	p = get_tcmd(pst);

	if (pst->tgid) {
		cprintf_s(IS_ZERO, "  |__%s\n", p);
	}
	else {
		cprintf_s(IS_STR, "  %s\n", p);
	}
}

/*
 ***************************************************************************
 * Read /proc/meminfo.
 ***************************************************************************
 */
void read_proc_meminfo(void)
{
	struct stats_memory st_mem;

	memset(&st_mem, 0, STATS_MEMORY_SIZE);
	read_meminfo(&st_mem);
	tlmkb = st_mem.tlmkb;
}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/stat.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 * @thread_nr	Number of threads of the process.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************
 */
int read_proc_pid_stat(unsigned int pid, struct pid_stats *pst,
		       unsigned int *thread_nr, unsigned int tgid)
{
	int fd, sz, rc, commsz;
	char filename[128];
	static char buffer[1024 + 1];
	char *start, *end;

	if (tgid) {
		sprintf(filename, TASK_STAT, tgid, pid);
	}
	else {
		sprintf(filename, PID_STAT, pid);
	}

	if ((fd = open(filename, O_RDONLY)) < 0)
		/* No such process */
		return 1;

	sz = read(fd, buffer, 1024);
	close(fd);
	if (sz <= 0)
		return 1;
	buffer[sz] = '\0';

	if ((start = strchr(buffer, '(')) == NULL)
		return 1;
	start += 1;
	if ((end = strrchr(start, ')')) == NULL)
		return 1;
	commsz = end - start;
	if (commsz >= MAX_COMM_LEN)
		return 1;
	memcpy(pst->comm, start, commsz);
	pst->comm[commsz] = '\0';
	start = end + 2;

	rc = sscanf(start,
		    "%*s %*d %*d %*d %*d %*d %*u %llu %llu"
		    " %llu %llu %llu %llu %lld %lld %*d %*d %u %*u %*d %llu %llu"
		    " %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u"
		    " %*u %u %u %u %llu %llu %lld\n",
		    &pst->minflt, &pst->cminflt, &pst->majflt, &pst->cmajflt,
		    &pst->utime,  &pst->stime, &pst->cutime, &pst->cstime,
		    thread_nr, &pst->vsz, &pst->rss, &pst->processor,
		    &pst->priority, &pst->policy,
		    &pst->blkio_swapin_delays, &pst->gtime, &pst->cgtime);

	if (rc < 15)
		return 1;

	if (rc < 17) {
		/* gtime and cgtime fields are unavailable in file */
		pst->gtime = pst->cgtime = 0;
	}

	/* Convert to kB */
	pst->vsz >>= 10;
	pst->rss = PG_TO_KB(pst->rss);

	pst->pid = pid;
	pst->tgid = tgid;
	return 0;
}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/schedstat.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If != 0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 * @thread_nr	Number of threads of the process.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************
 */
int read_proc_pid_sched(unsigned int pid, struct pid_stats *pst,
		       unsigned int *thread_nr, unsigned int tgid)
{
	int fd, sz, rc = 0;
	char filename[128];
	static char buffer[1024 + 1];
	unsigned long long wtime = 0;

	if (tgid) {
		sprintf(filename, TASK_SCHED, tgid, pid);
	}
	else {
		sprintf(filename, PID_SCHED, pid);
	}

	if ((fd = open(filename, O_RDONLY)) >= 0) {
		/* schedstat file found for process */
		sz = read(fd, buffer, 1024);
		close(fd);
		if (sz > 0) {
			buffer[sz] = '\0';

			rc = sscanf(buffer, "%*u %llu %*d\n", &wtime);
		}
	}

	/* Convert ns to jiffies */
	pst->wtime = wtime * HZ / 1000000000;

	pst->pid = pid;
	pst->tgid = tgid;

	if (rc < 1)
		return 1;

	return 0;
}

/*
 *****************************************************************************
 * Read stats from /proc/#[/task/##]/status.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 *****************************************************************************
 */
int read_proc_pid_status(unsigned int pid, struct pid_stats *pst,
			 unsigned int tgid)
{
	FILE *fp;
	char filename[128], line[256];

	if (tgid) {
		sprintf(filename, TASK_STATUS, tgid, pid);
	}
	else {
		sprintf(filename, PID_STATUS, pid);
	}

	if ((fp = fopen(filename, "r")) == NULL)
		/* No such process */
		return 1;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Uid:", 4)) {
			sscanf(line + 5, "%u", &pst->uid);
		}
		else if (!strncmp(line, "Threads:", 8)) {
			sscanf(line + 9, "%u", &pst->threads);
		}
		else if (!strncmp(line, "voluntary_ctxt_switches:", 24)) {
			sscanf(line + 25, "%lu", &pst->nvcsw);
		}
		else if (!strncmp(line, "nonvoluntary_ctxt_switches:", 27)) {
			sscanf(line + 28, "%lu", &pst->nivcsw);
		}
	}

	fclose(fp);

	pst->pid = pid;
	pst->tgid = tgid;
	return 0;
}

/*
  *****************************************************************************
  * Read information from /proc/#[/task/##}/smaps.
  *
  * @pid		Process whose stats are to be read.
  * @pst		Pointer on structure where stats will be saved.
  * @tgid		If !=0, thread whose stats are to be read.
  *
  * OUT:
  * @pst		Pointer on structure where stats have been saved.
  *
  * RETURNS:
  * 0 if stats have been successfully read, and 1 otherwise.
  *****************************************************************************
  */
int read_proc_pid_smap(unsigned int pid, struct pid_stats *pst, unsigned int tgid)
{
	FILE *fp;
	char filename[128], line[256];
	int state = 0;

	if (tgid) {
		sprintf(filename, TASK_SMAP, tgid, pid);
	}
	else {
		sprintf(filename, PID_SMAP, pid);
	}

	if ((fp = fopen(filename, "rt")) == NULL)
		/* No such process */
		return 1;

	while ((state < 3) && (fgets(line, sizeof(line), fp) != NULL)) {
		switch (state) {
			case 0:
				if (strstr(line, "[stack]")) {
					state = 1;
				}
				break;
			case 1:
				if (strstr(line, "Size:")) {
					sscanf(line + sizeof("Size:"), "%lu", &pst->stack_size);
					state = 2;
				}
				break;
			case 2:
				if (strstr(line, "Referenced:")) {
					sscanf(line + sizeof("Referenced:"), "%lu", &pst->stack_ref);
					state = 3;
				}
				break;
		}
	}

	fclose(fp);

	pst->pid = pid;
	pst->tgid = tgid;
	return 0;
}

/*
 *****************************************************************************
 * Read process command line from /proc/#[/task/##]/cmdline.
 *
 * IN:
 * @pid		Process whose command line is to be read.
 * @pst		Pointer on structure where command line will be saved.
 * @tgid	If !=0, thread whose command line is to be read.
 *
 * OUT:
 * @pst		Pointer on structure where command line has been saved.
 *
 * RETURNS:
 * 0 if command line has been successfully read (even if the /proc/.../cmdline
 * is just empty), and 1 otherwise (the process has terminated).
 *****************************************************************************
 */
int read_proc_pid_cmdline(unsigned int pid, struct pid_stats *pst,
			  unsigned int tgid)
{
	FILE *fp;
	char filename[128], line[MAX_CMDLINE_LEN];
	size_t len;
	int i;

	if (tgid) {
		sprintf(filename, TASK_CMDLINE, tgid, pid);
	}
	else {
		sprintf(filename, PID_CMDLINE, pid);
	}

	if ((fp = fopen(filename, "r")) == NULL)
		/* No such process */
		return 1;

	memset(line, 0, MAX_CMDLINE_LEN);

	len = fread(line, 1, MAX_CMDLINE_LEN - 1, fp);
	fclose(fp);

	for (i = 0; i < len; i++) {
		if (line[i] == '\0') {
			line[i] = ' ';
		}
	}

	if (len) {
		strncpy(pst->cmdline, line, MAX_CMDLINE_LEN - 1);
		pst->cmdline[MAX_CMDLINE_LEN - 1] = '\0';
	}
	else {
		/* proc/.../cmdline was empty */
		pst->cmdline[0] = '\0';
	}
	return 0;
}

/*
 ***************************************************************************
 * Read stats from /proc/#[/task/##]/io.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read.
 * Also returns 0 if current process has terminated or if its io file
 * doesn't exist, but in this case, set process' F_NO_PID_IO flag to
 * indicate that I/O stats should no longer be read for it.
 ***************************************************************************
 */
int read_proc_pid_io(unsigned int pid, struct pid_stats *pst,
		     unsigned int tgid)
{
	FILE *fp;
	char filename[128], line[256];

	if (tgid) {
		sprintf(filename, TASK_IO, tgid, pid);
	}
	else {
		sprintf(filename, PID_IO, pid);
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		/* No such process... or file non existent! */
		pst->flags |= F_NO_PID_IO;
		/*
		 * Also returns 0 since io stats file doesn't necessarily exist,
		 * depending on the kernel version used.
		 */
		return 0;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "read_bytes:", 11)) {
			sscanf(line + 12, "%llu", &pst->read_bytes);
		}
		else if (!strncmp(line, "write_bytes:", 12)) {
			sscanf(line + 13, "%llu", &pst->write_bytes);
		}
		else if (!strncmp(line, "cancelled_write_bytes:", 22)) {
			sscanf(line + 23, "%llu", &pst->cancelled_write_bytes);
		}
	}

	fclose(fp);

	pst->pid = pid;
	pst->tgid = tgid;
	pst->flags &= ~F_NO_PID_IO;
	return 0;
}

/*
 ***************************************************************************
 * Count number of file descriptors in /proc/#[/task/##]/fd directory.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read.
 * Also returns 0 if current process has terminated or if we cannot read its
 * fd directory, but in this case, set process' F_NO_PID_FD flag to
 * indicate that fd directory couldn't be read.
 ***************************************************************************
 */
int read_proc_pid_fd(unsigned int pid, struct pid_stats *pst,
		     unsigned int tgid)
{
	DIR *dir;
	struct dirent *drp;
	char filename[128];

	if (tgid) {
		sprintf(filename, TASK_FD, tgid, pid);
	}
	else {
		sprintf(filename, PID_FD, pid);
	}

	if ((dir = opendir(filename)) == NULL) {
		/* Cannot read fd directory */
		pst->flags |= F_NO_PID_FD;
		return 0;
	}

	pst->fd_nr = 0;

	/* Count number of entries if fd directory */
	while ((drp = readdir(dir)) != NULL) {
		if (isdigit(drp->d_name[0])) {
			(pst->fd_nr)++;
		}
	}

	closedir(dir);

	pst->pid = pid;
	pst->tgid = tgid;
	pst->flags &= ~F_NO_PID_FD;
	return 0;
}

/*
 ***************************************************************************
 * Read various stats for given PID.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 * @tgid	If !=0, thread whose stats are to be read.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 * @thread_nr	Number of threads of the process.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************
 */
int read_pid_stats(unsigned int pid, struct pid_stats *pst,
		   unsigned int *thread_nr, unsigned int tgid)
{
	if (read_proc_pid_stat(pid, pst, thread_nr, tgid))
		return 1;

	/*
	 * No need to test the return code here: Not finding
	 * the schedstat files shouldn't make pidstat stop.
	 */
	read_proc_pid_sched(pid, pst, thread_nr, tgid);

	if (DISPLAY_CMDLINE(pidflag)) {
		if (read_proc_pid_cmdline(pid, pst, tgid))
			return 1;
	}

	if (read_proc_pid_status(pid, pst, tgid))
		return 1;

	if (DISPLAY_STACK(actflag)) {
		if (read_proc_pid_smap(pid, pst, tgid))
			return 1;
	}

	if (DISPLAY_KTAB(actflag)) {
		if (read_proc_pid_fd(pid, pst, tgid))
			return 1;
	}

	if (DISPLAY_IO(actflag))
		/* Assume that /proc/#/task/#/io exists! */
		return (read_proc_pid_io(pid, pst, tgid));

	return 0;
}

/*
 ***************************************************************************
 * Count number of threads in /proc/#/task directory, including the leader
 * one.
 *
 * IN:
 * @pid	Process number for which the number of threads are to be counted.
 *
 * RETURNS:
 * Number of threads for the given process (min value is 1).
 * A value of 0 indicates that the process has terminated.
 ***************************************************************************
 */
unsigned int count_tid(unsigned int pid)
{
	struct pid_stats pst;
	unsigned int thread_nr;

	if (read_proc_pid_stat(pid, &pst, &thread_nr, 0) != 0)
		/* Task no longer exists */
		return 0;

	return thread_nr;
}

/*
 ***************************************************************************
 * Count number of processes (and threads).
 *
 * RETURNS:
 * Number of processes (and threads if requested).
 ***************************************************************************
 */
unsigned int count_pid(void)
{
	DIR *dir;
	struct dirent *drp;
	unsigned int pid = 0;

	/* Open /proc directory */
	if ((dir = opendir(PROC)) == NULL) {
		perror("opendir");
		exit(4);
	}

	/* Get directory entries */
	while ((drp = readdir(dir)) != NULL) {
		if (isdigit(drp->d_name[0])) {
			/* There is at least the TGID */
			pid++;
			if (DISPLAY_TID(pidflag)) {
				pid += count_tid(atoi(drp->d_name));
			}
		}
	}

	/* Close /proc directory */
	closedir(dir);

	return pid;
}

/*
 ***************************************************************************
 * Count number of threads associated with the tasks entered on the command
 * line.
 *
 * IN:
 * @pid_array_nr	Length of the PID list.
 *
 * RETURNS:
 * Number of threads (including the leading one) associated with every task
 * entered on the command line.
 ***************************************************************************
 */
unsigned int count_tid_in_list(unsigned int pid_array_nr)
{
	unsigned int p, tid, pid = 0;

	for (p = 0; p < pid_array_nr; p++) {

		tid = count_tid(pid_array[p]);

		if (!tid) {
			/* PID no longer exists */
			pid_array[p] = 0;
		}
		else {
			/* <tid_value> TIDs + 1 TGID */
			pid += tid + 1;
		}
	}

	return pid;
}

/*
 ***************************************************************************
 * Allocate and init structures according to system state.
 *
 * IN:
 * @pid_array_nr	Length of the PID list.
 ***************************************************************************
 */
void pid_sys_init(unsigned int pid_array_nr)
{
	/* Init stat common counters */
	init_stats();

	/* Count nb of proc */
	cpu_nr = get_cpu_nr(~0, FALSE);

	if (DISPLAY_ALL_PID(pidflag)) {
		/* Count PIDs and allocate structures */
		pid_nr = count_pid() + NR_PID_PREALLOC;
		salloc_pid(pid_nr);
	}
	else if (DISPLAY_TID(pidflag)) {
		/* Count total number of threads associated with tasks in list */
		pid_nr = count_tid_in_list(pid_array_nr) + NR_PID_PREALLOC;
		salloc_pid(pid_nr);
	}
	else {
		pid_nr = pid_array_nr;
		salloc_pid(pid_nr);
	}
}

/*
 ***************************************************************************
 * Read stats for threads in /proc/#/task directory.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @pid		Process number whose threads stats are to be read.
 * @index	Index in process list where stats will be saved.
 *
 * OUT:
 * @index	Index in process list where next stats will be saved.
 ***************************************************************************
 */
void read_task_stats(int curr, unsigned int pid, unsigned int *index)
{
	DIR *dir;
	struct dirent *drp;
	char filename[128];
	struct pid_stats *pst;
	unsigned int thr_nr;

	/* Open /proc/#/task directory */
	sprintf(filename, PROC_TASK, pid);
	if ((dir = opendir(filename)) == NULL)
		return;

	while ((drp = readdir(dir)) != NULL) {
		if (!isdigit(drp->d_name[0])) {
			continue;
		}

		pst = st_pid_list[curr] + (*index)++;
		if (read_pid_stats(atoi(drp->d_name), pst, &thr_nr, pid)) {
			/* Thread no longer exists */
			pst->pid = 0;
		}

		if (*index >= pid_nr) {
			realloc_pid();
		}
	}

	closedir(dir);
}

/*
 ***************************************************************************
 * Read various stats.
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @pid_array_nr	Length of the PID list.
 ***************************************************************************
 */
void read_stats(int curr, unsigned int pid_array_nr)
{
	DIR *dir;
	struct dirent *drp;
	unsigned int p = 0, q, pid, thr_nr;
	struct pid_stats *pst;
	struct stats_cpu *st_cpu;

	/*
	 * Allocate two structures for CPU statistics.
	 * No need to init them (done by read_stat_cpu() function).
	 */
	if ((st_cpu = (struct stats_cpu *) malloc(STATS_CPU_SIZE * 2)) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Read statistics for CPUs "all" */
	read_stat_cpu(st_cpu, 1);

	/*
	 * Compute the total number of jiffies spent by all processors.
	 * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
	 * already include them.
	 */
	tot_jiffies[curr] = st_cpu->cpu_user + st_cpu->cpu_nice +
			    st_cpu->cpu_sys + st_cpu->cpu_idle +
			    st_cpu->cpu_iowait + st_cpu->cpu_hardirq +
			    st_cpu->cpu_steal + st_cpu->cpu_softirq;
	free(st_cpu);

	if (DISPLAY_ALL_PID(pidflag)) {

		/* Open /proc directory */
		if ((dir = opendir(PROC)) == NULL) {
			perror("opendir");
			exit(4);
		}

		/* Get directory entries */
		while ((drp = readdir(dir)) != NULL) {
			if (!isdigit(drp->d_name[0])) {
				continue;
			}

			pst = st_pid_list[curr] + p++;
			pid = atoi(drp->d_name);

			if (read_pid_stats(pid, pst, &thr_nr, 0)) {
				/* Process has terminated */
				pst->pid = 0;
			} else if (DISPLAY_TID(pidflag)) {
				/* Read stats for threads in task subdirectory */
				read_task_stats(curr, pid, &p);
			}

			if (p >= pid_nr) {
				realloc_pid();
			}
		}

		for (q = p; q < pid_nr; q++) {
			pst = st_pid_list[curr] + q;
			pst->pid = 0;
		}

		/* Close /proc directory */
		closedir(dir);
	}

	else if (DISPLAY_PID(pidflag)) {
		unsigned int op;

		/* Read stats for each PID in the list */
		for (op = 0; op < pid_array_nr; op++) {

			if (p >= pid_nr)
				break;
			pst = st_pid_list[curr] + p++;

			if (pid_array[op]) {
				/* PID should still exist. So read its stats */
				if (read_pid_stats(pid_array[op], pst, &thr_nr, 0)) {
					/* PID has terminated */
					pst->pid = 0;
					pid_array[op] = 0;
				}
				else if (DISPLAY_TID(pidflag)) {
					read_task_stats(curr, pid_array[op], &p);
				}
			}
		}
		/* Reset remaining structures */
		for (q = p; q < pid_nr; q++) {
			pst = st_pid_list[curr] + q;
			pst->pid = 0;
		}

	}
	/* else unknown command */
}

/*
 ***************************************************************************
 * Get current PID to display.
 * First, check that PID exists. *Then* check that it's an active process
 * and/or that the string (entered on the command line with option -C)
 * is found in command name, or that the process string (entered on the
 * command line with option -G) is found either in its command name (in case
 * PID is a process) or in command name of its thread leader (in case
 * PID is a thread).
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @p		Index in process list.
 * @activity	Current activity to display (CPU, memory...).
 * 		Can be more than one if stats are displayed on one line.
 * @pflag	Flag indicating whether stats are to be displayed for
 * 		individual tasks or for all their children.
 *
 * OUT:
 * @pstc	Structure with PID statistics for current sample.
 * @pstp	Structure with PID statistics for previous sample.
 *
 * RETURNS:
 *  0 if PID no longer exists.
 * -1 if PID exists but should not be displayed.
 *  1 if PID can be displayed.
 ***************************************************************************
 */
int get_pid_to_display(int prev, int curr, int p, unsigned int activity,
		       unsigned int pflag,
		       struct pid_stats **pstc, struct pid_stats **pstp)
{
	int q, rc;
	regex_t regex;
	struct passwd *pwdent;
	char *pc;

	*pstc = st_pid_list[curr] + p;

	if (!(*pstc)->pid)
		/* PID no longer exists */
		return 0;

	if (DISPLAY_ALL_PID(pidflag) || DISPLAY_TID(pidflag)) {

		/* Look for previous stats for same PID */
		q = p;

		do {
			*pstp = st_pid_list[prev] + q;
			if (((*pstp)->pid == (*pstc)->pid) &&
			    ((*pstp)->tgid == (*pstc)->tgid))
				break;
			q++;
			if (q >= pid_nr) {
				q = 0;
			}
		}
		while (q != p);

		if (((*pstp)->pid != (*pstc)->pid) ||
		    ((*pstp)->tgid != (*pstc)->tgid)) {
			/* PID not found (no data previously read) */
			*pstp = &st_pid_null;
		}

		if (DISPLAY_ACTIVE_PID(pidflag)) {
			int isActive = FALSE;

			/* Check that it's an "active" process */
			if (DISPLAY_CPU(activity)) {
				/* User time already includes guest time */
				if (((*pstc)->utime != (*pstp)->utime) ||
				    ((*pstc)->stime != (*pstp)->stime)) {
					isActive = TRUE;
				}
				else {
					/*
					 * Process is not active but if we are showing
					 * child stats then we need to look there.
					 */
					if (DISPLAY_CHILD_STATS(pflag)) {
						/* User time already includes guest time */
						if (((*pstc)->cutime != (*pstp)->cutime) ||
						    ((*pstc)->cstime != (*pstp)->cstime)) {
							isActive = TRUE;
						}
					}
				}
			}

			if (DISPLAY_MEM(activity) && (!isActive)) {
				if (((*pstc)->minflt != (*pstp)->minflt) ||
				    ((*pstc)->majflt != (*pstp)->majflt)) {
					isActive = TRUE;
				}
				else {
					if (DISPLAY_TASK_STATS(pflag)) {
						if (((*pstc)->vsz != (*pstp)->vsz) ||
						    ((*pstc)->rss != (*pstp)->rss)) {
							isActive = TRUE;
						}
					}
					else if (DISPLAY_CHILD_STATS(pflag)) {
						if (((*pstc)->cminflt != (*pstp)->cminflt) ||
						    ((*pstc)->cmajflt != (*pstp)->cmajflt)) {
							isActive = TRUE;
						}
					}
				}
			}


			if (DISPLAY_STACK(activity) && (!isActive)) {
				if (((*pstc)->stack_size  != (*pstp)->stack_size) ||
				    ((*pstc)->stack_ref != (*pstp)->stack_ref)) {
					isActive = TRUE;
				}
			}

			if (DISPLAY_IO(activity) && (!isActive)) {
				if ((*pstc)->blkio_swapin_delays !=
				     (*pstp)->blkio_swapin_delays) {
					isActive = TRUE;
				}
				if (!(NO_PID_IO((*pstc)->flags)) && (!isActive)) {
					/* /proc/#/io file should exist to check I/O stats */
					if (((*pstc)->read_bytes  != (*pstp)->read_bytes)  ||
					    ((*pstc)->write_bytes != (*pstp)->write_bytes) ||
					    ((*pstc)->cancelled_write_bytes !=
					     (*pstp)->cancelled_write_bytes)) {
						isActive = TRUE;
					}
				}
			}

			if (DISPLAY_CTXSW(activity) && (!isActive)) {
				if (((*pstc)->nvcsw  != (*pstp)->nvcsw) ||
				    ((*pstc)->nivcsw != (*pstp)->nivcsw)) {
					isActive = TRUE;
				}
			}

			if (DISPLAY_RT(activity) && (!isActive)) {
				if (((*pstc)->priority != (*pstp)->priority) ||
				    ((*pstc)->policy != (*pstp)->policy)) {
					isActive = TRUE;
				}
			}

			if (DISPLAY_KTAB(activity) && (!isActive) &&
				/* /proc/#/fd directory should be readable */
				!(NO_PID_FD((*pstc)->flags))) {
				if (((*pstc)->threads != (*pstp)->threads) ||
				    ((*pstc)->fd_nr != (*pstp)->fd_nr)) {
					isActive = TRUE;
				}
			}

			/* If PID isn't active for any of the activities then return */
			if (!isActive)
				return -1;
		}
	}

	else if (DISPLAY_PID(pidflag)) {
		*pstp = st_pid_list[prev] + p;
		if (!(*pstp)->pid) {
			if (interval)
				/* PID no longer exists */
				return 0;
			else {
				/*
				 * If interval is zero, then we are trying to
				 * display stats for a given process since boot time.
				 */
				*pstp = &st_pid_null;
			}
		}
	}

	if (COMMAND_STRING(pidflag)) {
		if (regcomp(&regex, commstr, REG_EXTENDED | REG_NOSUB) != 0)
			/* Error in preparing regex structure */
			return -1;

		pc = get_tcmd(*pstc);	/* Get pointer on task's command string */
		rc = regexec(&regex, pc, 0, NULL, 0);
		regfree(&regex);

		if (rc)
			/* regex pattern not found in command name */
			return -1;
	}

	if (PROCESS_STRING(pidflag)) {
		if (!(*pstc)->tgid) {
			/* This PID is a process ("thread group leader") */
			if (regcomp(&regex, procstr, REG_EXTENDED | REG_NOSUB) != 0) {
				/* Error in preparing regex structure */
				show_threads = FALSE;
				return -1;
			}

			pc = get_tcmd(*pstc);	/* Get pointer on task's command string */
			rc = regexec(&regex, pc, 0, NULL, 0);
			regfree(&regex);

			if (rc) {
				/* regex pattern not found in command name */
				show_threads = FALSE;
				return -1;
			}

			/*
			 * This process and all its threads will be displayed.
			 * No need to save PID value: For every process read by pidstat,
			 * pidstat then immediately reads all its threads.
			 */
			show_threads = TRUE;
		}
		else if (!show_threads) {
			/* This pid is a thread and is not part of a process to display */
			return -1;
		}
	}

	if (USER_STRING(pidflag)) {
		if ((pwdent = getpwuid((*pstc)->uid)) != NULL) {
			if (strcmp(pwdent->pw_name, userstr))
				/* This PID doesn't belong to user */
				return -1;
		}
	}

	return 1;
}

/*
 ***************************************************************************
 * Display UID/username, PID and TID.
 *
 * IN:
 * @pst		Current process statistics.
 * @c		No-op character.
 ***************************************************************************
 */
void __print_line_id(struct pid_stats *pst, char c)
{
	char format[32];
	struct passwd *pwdent;

	if (DISPLAY_USERNAME(pidflag) && ((pwdent = getpwuid(pst->uid)) != NULL)) {
		cprintf_in(IS_STR, " %8s", pwdent->pw_name, 0);
	}
	else {
		cprintf_in(IS_INT, " %5d", "", pst->uid);
	}

	if (DISPLAY_TID(pidflag)) {

		if (pst->tgid) {
			/* This is a TID */
			sprintf(format, "         %c %%9u", c);
		}
		else {
			/* This is a PID (TGID) */
			sprintf(format, " %%9u         %c", c);
		}
	}
	else {
		strcpy(format, " %9u");
	}

	cprintf_in(IS_INT, format, "", pst->pid);
}

/*
 ***************************************************************************
 * Display timestamp, PID and TID.
 *
 * IN:
 * @timestamp	Current timestamp.
 * @pst		Current process statistics.
 ***************************************************************************
 */
void print_line_id(char *timestamp, struct pid_stats *pst)
{
	printf("%-11s", timestamp);

	__print_line_id(pst, '-');
}

/*
 ***************************************************************************
 * Display all statistics for tasks in one line format.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample.
 * @itv		Interval of time in jiffies.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_task_all_stats(int prev, int curr, int dis,
			     char *prev_string, char *curr_string,
			     unsigned long long itv,
			     unsigned long long deltot_jiffies)
{
	struct pid_stats *pstc, *pstp;
	char dstr[32];
	unsigned int p;
	int again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		if (DISPLAY_CPU(actflag)) {
			printf("    %%usr %%system  %%guest   %%wait    %%CPU   CPU");
		}
		if (DISPLAY_MEM(actflag)) {
			printf("  minflt/s  majflt/s     VSZ     RSS   %%MEM");
		}
		if (DISPLAY_STACK(actflag)) {
			printf(" StkSize  StkRef");
		}
		if (DISPLAY_IO(actflag)) {
			printf("   kB_rd/s   kB_wr/s kB_ccwr/s iodelay");
		}
		if (DISPLAY_CTXSW(actflag)) {
			printf("   cswch/s nvcswch/s");
		}
		if (DISPLAY_KTAB(actflag)) {
			printf(" threads   fd-nr");
		}
		if (DISPLAY_RT(actflag)) {
			printf(" prio policy");
		}
		printf("  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if (get_pid_to_display(prev, curr, p, actflag, P_TASK,
				       &pstc, &pstp) <= 0)
			continue;

		print_line_id(curr_string, pstc);

		if (DISPLAY_CPU(actflag)) {
			cprintf_pc(DISPLAY_UNIT(pidflag), 5, 7, 2,
				   (pstc->utime - pstc->gtime) < (pstp->utime - pstp->gtime) ?
				   0.0 :
				   SP_VALUE(pstp->utime - pstp->gtime,
					    pstc->utime - pstc->gtime, itv * HZ / 100),
				   SP_VALUE(pstp->stime,  pstc->stime, itv * HZ / 100),
				   SP_VALUE(pstp->gtime,  pstc->gtime, itv * HZ / 100),
				   SP_VALUE(pstp->wtime, pstc->wtime, itv * HZ / 100),
				   /* User time already includes guest time */
				   IRIX_MODE_OFF(pidflag) ?
				   SP_VALUE(pstp->utime + pstp->stime,
					    pstc->utime + pstc->stime, deltot_jiffies) :
				   SP_VALUE(pstp->utime + pstp->stime,
					    pstc->utime + pstc->stime, itv * HZ / 100));

			cprintf_in(IS_INT, "   %3d", "", pstc->processor);
		}

		if (DISPLAY_MEM(actflag)) {
			cprintf_f(NO_UNIT, 2, 9, 2,
				  S_VALUE(pstp->minflt, pstc->minflt, itv),
				  S_VALUE(pstp->majflt, pstc->majflt, itv));
			cprintf_u64(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7,
				    (unsigned long long) pstc->vsz,
				    (unsigned long long) pstc->rss);
			cprintf_pc(DISPLAY_UNIT(pidflag), 1, 6, 2,
				   tlmkb ? SP_VALUE(0, pstc->rss, tlmkb) : 0.0);
		}

		if (DISPLAY_STACK(actflag)) {
			cprintf_u64(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7,
				    (unsigned long long) pstc->stack_size,
				    (unsigned long long) pstc->stack_ref);
		}

		if (DISPLAY_IO(actflag)) {
			if (!NO_PID_IO(pstc->flags))
			{
				double rbytes, wbytes, cbytes;

				rbytes = S_VALUE(pstp->read_bytes,  pstc->read_bytes, itv);
				wbytes = S_VALUE(pstp->write_bytes, pstc->write_bytes, itv);
				cbytes = S_VALUE(pstp->cancelled_write_bytes,
						 pstc->cancelled_write_bytes, itv);
				if (!DISPLAY_UNIT(pidflag)) {
					rbytes /= 1024;
					wbytes /= 1024;
					cbytes /= 1024;
				}
				cprintf_f(DISPLAY_UNIT(pidflag) ? UNIT_BYTE : NO_UNIT, 3, 9, 2,
					  rbytes, wbytes, cbytes);
			}
			else {
				/*
				 * Keep the layout even though this task has no I/O
				 * typically threads with no I/O measurements.
				 */
				sprintf(dstr, " %9.2f %9.2f %9.2f", -1.0, -1.0, -1.0);
				cprintf_s(IS_ZERO, "%s", dstr);
			}
			/* I/O delays come from another file (/proc/#/stat) */
			cprintf_u64(NO_UNIT, 1, 7,
				    (unsigned long long) (pstc->blkio_swapin_delays - pstp->blkio_swapin_delays));
		}

		if (DISPLAY_CTXSW(actflag)) {
			cprintf_f(NO_UNIT, 2, 9, 2,
				  S_VALUE(pstp->nvcsw, pstc->nvcsw, itv),
				  S_VALUE(pstp->nivcsw, pstc->nivcsw, itv));
		}

		if (DISPLAY_KTAB(actflag)) {
			cprintf_u64(NO_UNIT, 1, 7,
				    (unsigned long long) pstc->threads);
			if (NO_PID_FD(pstc->flags)) {
				/* /proc/#/fd directory not readable */
				cprintf_s(IS_ZERO, " %7s", "-1");
			}
			else {
				cprintf_u64(NO_UNIT, 1, 7, (unsigned long long) pstc->fd_nr);
			}
		}

		if (DISPLAY_RT(actflag)) {
			cprintf_u64(NO_UNIT, 1, 4,
				    (unsigned long long) pstc->priority);
			cprintf_s(IS_STR, " %6s",
				  GET_POLICY(pstc->policy));
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display all statistics for tasks' children in one line format.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_child_all_stats(int prev, int curr, int dis,
			      char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		if (DISPLAY_CPU(actflag))
			printf("    usr-ms system-ms  guest-ms");
		if (DISPLAY_MEM(actflag))
			printf(" minflt-nr majflt-nr");
		printf("  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if (get_pid_to_display(prev, curr, p, actflag, P_CHILD,
				       &pstc, &pstp) <= 0)
			continue;

		print_line_id(curr_string, pstc);

		if (DISPLAY_CPU(actflag)) {
			cprintf_f(NO_UNIT, 3, 9, 0,
				  (pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) <
				  (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime) ?
				  0.0 :
				  (double) ((pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) -
					    (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime)) /
					    HZ * 1000,
				  (double) ((pstc->stime + pstc->cstime) -
					    (pstp->stime + pstp->cstime)) / HZ * 1000,
				  (double) ((pstc->gtime + pstc->cgtime) -
					    (pstp->gtime + pstp->cgtime)) / HZ * 1000);
		}

		if (DISPLAY_MEM(actflag)) {
			cprintf_u64(NO_UNIT, 2, 9,
				    (unsigned long long) ((pstc->minflt + pstc->cminflt) - (pstp->minflt + pstp->cminflt)),
				    (unsigned long long) ((pstc->majflt + pstc->cmajflt) - (pstp->majflt + pstp->cmajflt)));
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display CPU statistics for tasks.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @itv		Interval of time in 1/100th of a second.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_task_cpu_stats(int prev, int curr, int dis, int disp_avg,
			     char *prev_string, char *curr_string,
			     unsigned long long itv,
			     unsigned long long deltot_jiffies)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf("    %%usr %%system  %%guest   %%wait    %%CPU   CPU  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if (get_pid_to_display(prev, curr, p, P_A_CPU, P_TASK,
				       &pstc, &pstp) <= 0)
			continue;

		print_line_id(curr_string, pstc);
		cprintf_pc(DISPLAY_UNIT(pidflag), 5, 7, 2,
			   (pstc->utime - pstc->gtime) < (pstp->utime - pstp->gtime) ?
			   0.0 :
			   SP_VALUE(pstp->utime - pstp->gtime,
				    pstc->utime - pstc->gtime, itv * HZ / 100),
			   SP_VALUE(pstp->stime, pstc->stime, itv * HZ / 100),
			   SP_VALUE(pstp->gtime, pstc->gtime, itv * HZ / 100),
			   SP_VALUE(pstp->wtime, pstc->wtime, itv * HZ / 100),
			   /* User time already includes guest time */
			   IRIX_MODE_OFF(pidflag) ?
			   SP_VALUE(pstp->utime + pstp->stime,
				    pstc->utime + pstc->stime, deltot_jiffies) :
			   SP_VALUE(pstp->utime + pstp->stime,
				    pstc->utime + pstc->stime, itv * HZ / 100));

		if (!disp_avg) {
			cprintf_in(IS_INT, "   %3d", "", pstc->processor);
		}
		else {
			cprintf_in(IS_STR, "%s", "     -", 0);
		}
		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display CPU statistics for tasks' children.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_child_cpu_stats(int prev, int curr, int dis, int disp_avg,
			      char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int rc, again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf("    usr-ms system-ms  guest-ms  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_CPU, P_CHILD,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average */
		if (!disp_avg) {
			pstc->uc_asum_count = pstp->uc_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);
		if (disp_avg) {
			cprintf_f(NO_UNIT, 3, 9, 0,
				  (pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) <
				  (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime) ?
				  0.0 :
				  (double) ((pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) -
					    (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime)) /
					    (HZ * pstc->uc_asum_count) * 1000,
				  (double) ((pstc->stime + pstc->cstime) -
					    (pstp->stime + pstp->cstime)) /
					    (HZ * pstc->uc_asum_count) * 1000,
				  (double) ((pstc->gtime + pstc->cgtime) -
					    (pstp->gtime + pstp->cgtime)) /
					    (HZ * pstc->uc_asum_count) * 1000);
		}
		else {
			cprintf_f(NO_UNIT, 3, 9, 0,
				  (pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) <
				  (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime) ?
				  0.0 :
				  (double) ((pstc->utime + pstc->cutime - pstc->gtime - pstc->cgtime) -
					    (pstp->utime + pstp->cutime - pstp->gtime - pstp->cgtime)) /
					    HZ * 1000,
				  (double) ((pstc->stime + pstc->cstime) -
					    (pstp->stime + pstp->cstime)) / HZ * 1000,
				  (double) ((pstc->gtime + pstc->cgtime) -
					    (pstp->gtime + pstp->cgtime)) / HZ * 1000);
		}
		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display memory statistics for tasks.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @itv		Interval of time in 1/100th of a second.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_task_memory_stats(int prev, int curr, int dis, int disp_avg,
				char *prev_string, char *curr_string,
				unsigned long long itv)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int rc, again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf("  minflt/s  majflt/s     VSZ     RSS   %%MEM  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_MEM, P_TASK,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average */
		if (!disp_avg) {
			pstc->total_vsz = pstp->total_vsz + pstc->vsz;
			pstc->total_rss = pstp->total_rss + pstc->rss;
			pstc->rt_asum_count = pstp->rt_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);

		cprintf_f(NO_UNIT, 2, 9, 2,
			  S_VALUE(pstp->minflt, pstc->minflt, itv),
			  S_VALUE(pstp->majflt, pstc->majflt, itv));

		if (disp_avg) {
			cprintf_f(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7, 0,
				  (double) pstc->total_vsz / pstc->rt_asum_count,
				  (double) pstc->total_rss / pstc->rt_asum_count);

			cprintf_pc(DISPLAY_UNIT(pidflag), 1, 6, 2,
				   tlmkb ?
				   SP_VALUE(0, pstc->total_rss / pstc->rt_asum_count, tlmkb)
				   : 0.0);
		}
		else {
			cprintf_u64(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7,
				    (unsigned long long) pstc->vsz,
				    (unsigned long long) pstc->rss);

			cprintf_pc(DISPLAY_UNIT(pidflag), 1, 6, 2,
				   tlmkb ? SP_VALUE(0, pstc->rss, tlmkb) : 0.0);
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display memory statistics for tasks' children.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_child_memory_stats(int prev, int curr, int dis, int disp_avg,
				 char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int rc, again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf(" minflt-nr majflt-nr  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_MEM, P_CHILD,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average */
		if (!disp_avg) {
			pstc->rc_asum_count = pstp->rc_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);
		if (disp_avg) {
			cprintf_f(NO_UNIT, 2, 9, 0,
				  (double) ((pstc->minflt + pstc->cminflt) -
					    (pstp->minflt + pstp->cminflt)) / pstc->rc_asum_count,
				  (double) ((pstc->majflt + pstc->cmajflt) -
					    (pstp->majflt + pstp->cmajflt)) / pstc->rc_asum_count);
		}
		else {
			cprintf_u64(NO_UNIT, 2, 9,
				    (unsigned long long) ((pstc->minflt + pstc->cminflt) - (pstp->minflt + pstp->cminflt)),
                    (unsigned long long) ((pstc->majflt + pstc->cmajflt) - (pstp->majflt + pstp->cmajflt)));
		}
		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display stack size statistics for tasks.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_stack_stats(int prev, int curr, int dis, int disp_avg,
			  char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int rc, again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf(" StkSize  StkRef  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_STACK, P_NULL,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average */
		if (!disp_avg) {
			pstc->total_stack_size = pstp->total_stack_size + pstc->stack_size;
			pstc->total_stack_ref  = pstp->total_stack_ref  + pstc->stack_ref;
			pstc->sk_asum_count = pstp->sk_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);

		if (disp_avg) {
			cprintf_f(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7, 0,
				  (double) pstc->total_stack_size / pstc->sk_asum_count,
				  (double) pstc->total_stack_ref  / pstc->sk_asum_count);
		}
		else {
			cprintf_u64(DISPLAY_UNIT(pidflag) ? UNIT_KILOBYTE : NO_UNIT, 2, 7,
				    (unsigned long long) pstc->stack_size,
				    (unsigned long long) pstc->stack_ref);
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display I/O statistics.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @itv		Interval of time in 1/100th of a second.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_io_stats(int prev, int curr, int dis, int disp_avg,
		       char *prev_string, char *curr_string,
		       unsigned long long itv)
{
	struct pid_stats *pstc, *pstp;
	char dstr[32];
	unsigned int p;
	int rc, again = 0;
	double rbytes, wbytes, cbytes;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf("   kB_rd/s   kB_wr/s kB_ccwr/s iodelay  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_IO, P_NULL,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average delays */
		if (!disp_avg) {
			pstc->delay_asum_count = pstp->delay_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);
		if (!NO_PID_IO(pstc->flags)) {
			rbytes = S_VALUE(pstp->read_bytes,  pstc->read_bytes, itv);
			wbytes = S_VALUE(pstp->write_bytes, pstc->write_bytes, itv);
			cbytes = S_VALUE(pstp->cancelled_write_bytes,
					 pstc->cancelled_write_bytes, itv);
			if (!DISPLAY_UNIT(pidflag)) {
				rbytes /= 1024;
				wbytes /= 1024;
				cbytes /= 1024;
			}
			cprintf_f(DISPLAY_UNIT(pidflag) ? UNIT_BYTE : NO_UNIT, 3, 9, 2,
				  rbytes, wbytes, cbytes);
		}
		else {
			/* I/O file not readable (permission denied or file non existent) */
			sprintf(dstr, " %9.2f %9.2f %9.2f", -1.0, -1.0, -1.0);
			cprintf_s(IS_ZERO, "%s", dstr);
		}
		/* I/O delays come from another file (/proc/#/stat) */
		if (disp_avg) {
			cprintf_f(NO_UNIT, 1, 7, 0,
				  (double) (pstc->blkio_swapin_delays - pstp->blkio_swapin_delays) /
					    pstc->delay_asum_count);
		}
		else {
			cprintf_u64(NO_UNIT, 1, 7,
				    (unsigned long long) (pstc->blkio_swapin_delays - pstp->blkio_swapin_delays));
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display context switches statistics.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @itv		Interval of time in 1/100th of a second.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_ctxswitch_stats(int prev, int curr, int dis,
			      char *prev_string, char *curr_string,
			      unsigned long long itv)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf("   cswch/s nvcswch/s  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if (get_pid_to_display(prev, curr, p, P_A_CTXSW, P_NULL,
				       &pstc, &pstp) <= 0)
			continue;

		print_line_id(curr_string, pstc);
		cprintf_f(NO_UNIT, 2, 9, 2,
			  S_VALUE(pstp->nvcsw, pstc->nvcsw, itv),
			  S_VALUE(pstp->nivcsw, pstc->nivcsw, itv));
		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display scheduling priority and policy information.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_rt_stats(int prev, int curr, int dis,
		       char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf(" prio policy  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if (get_pid_to_display(prev, curr, p, P_A_RT, P_NULL,
				       &pstc, &pstp) <= 0)
			continue;

		print_line_id(curr_string, pstc);
		cprintf_u64(NO_UNIT, 1, 4,
			    (unsigned long long) pstc->priority);
		cprintf_s(IS_STR, " %6s", GET_POLICY(pstc->policy));
		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display some kernel tables values for tasks.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_pid_ktab_stats(int prev, int curr, int dis, int disp_avg,
			 char *prev_string, char *curr_string)
{
	struct pid_stats *pstc, *pstp;
	unsigned int p;
	int rc, again = 0;

	if (dis) {
		PRINT_ID_HDR(prev_string, pidflag);
		printf(" threads   fd-nr");
		printf("  Command\n");
	}

	for (p = 0; p < pid_nr; p++) {

		if ((rc = get_pid_to_display(prev, curr, p, P_A_KTAB, P_NULL,
					     &pstc, &pstp)) == 0)
			/* PID no longer exists */
			continue;

		/* This will be used to compute average */
		if (!disp_avg) {
			pstc->total_threads = pstp->total_threads + pstc->threads;
			pstc->total_fd_nr = pstp->total_fd_nr + pstc->fd_nr;
			pstc->tf_asum_count = pstp->tf_asum_count + 1;
		}

		if (rc < 0)
			/* PID should not be displayed */
			continue;

		print_line_id(curr_string, pstc);

		if (disp_avg) {
			cprintf_f(NO_UNIT, 2, 7, 0,
				  (double) pstc->total_threads / pstc->tf_asum_count,
				  NO_PID_FD(pstc->flags) ?
				  -1.0 :
				  (double) pstc->total_fd_nr / pstc->tf_asum_count);
		}
		else {
			cprintf_u64(NO_UNIT, 1, 7,
				    (unsigned long long) pstc->threads);
			if (NO_PID_FD(pstc->flags)) {
				cprintf_s(IS_ZERO, " %7s", "-1");
			}
			else {
				cprintf_u64(NO_UNIT, 1, 7,
					    (unsigned long long) pstc->fd_nr);
			}
		}

		print_comm(pstc);
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Display statistics.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 * @disp_avg	TRUE if average stats are displayed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_stats_core(int prev, int curr, int dis, int disp_avg,
		     char *prev_string, char *curr_string)
{
	unsigned long long itv, deltot_jiffies;
	int again = 0;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	/* Total number of jiffies spent on the interval */
	deltot_jiffies = get_interval(tot_jiffies[prev], tot_jiffies[curr]);

	itv = get_interval(uptime_cs[prev], uptime_cs[curr]);

	if (PROCESS_STRING(pidflag)) {
		/* Reset "show threads" flag */
		show_threads = FALSE;
	}

	if (DISPLAY_ONELINE(pidflag)) {
		if (DISPLAY_TASK_STATS(tskflag)) {
			again += write_pid_task_all_stats(prev, curr, dis, prev_string, curr_string,
							  itv, deltot_jiffies);
		}
		if (DISPLAY_CHILD_STATS(tskflag)) {
			again += write_pid_child_all_stats(prev, curr, dis, prev_string, curr_string);
		}
	}
	else {
		/* Display CPU stats */
		if (DISPLAY_CPU(actflag)) {

			if (DISPLAY_TASK_STATS(tskflag)) {
				again += write_pid_task_cpu_stats(prev, curr, dis, disp_avg,
								  prev_string, curr_string,
								  itv, deltot_jiffies);
			}
			if (DISPLAY_CHILD_STATS(tskflag)) {
				again += write_pid_child_cpu_stats(prev, curr, dis, disp_avg,
								   prev_string, curr_string);
			}
		}

		/* Display memory stats */
		if (DISPLAY_MEM(actflag)) {

			if (DISPLAY_TASK_STATS(tskflag)) {
				again += write_pid_task_memory_stats(prev, curr, dis, disp_avg,
								     prev_string, curr_string, itv);
			}
			if (DISPLAY_CHILD_STATS(tskflag) && DISPLAY_MEM(actflag)) {
				again += write_pid_child_memory_stats(prev, curr, dis, disp_avg,
								      prev_string, curr_string);
			}
		}

		/* Display stack stats */
		if (DISPLAY_STACK(actflag)) {
			again += write_pid_stack_stats(prev, curr, dis, disp_avg,
						       prev_string, curr_string);
		}

		/* Display I/O stats */
		if (DISPLAY_IO(actflag)) {
			again += write_pid_io_stats(prev, curr, dis, disp_avg, prev_string,
						    curr_string, itv);
		}

		/* Display context switches stats */
		if (DISPLAY_CTXSW(actflag)) {
			again += write_pid_ctxswitch_stats(prev, curr, dis, prev_string,
							   curr_string, itv);
		}

		/* Display kernel table stats */
		if (DISPLAY_KTAB(actflag)) {
			again += write_pid_ktab_stats(prev, curr, dis, disp_avg,
						      prev_string, curr_string);
		}

		/* Display scheduling priority and policy information */
		if (DISPLAY_RT(actflag)) {
			again += write_pid_rt_stats(prev, curr, dis, prev_string, curr_string);
		}
	}

	if (DISPLAY_ALL_PID(pidflag)) {
		again = 1;
	}

	return again;
}

/*
 ***************************************************************************
 * Print statistics average.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 ***************************************************************************
 */
void write_stats_avg(int curr, int dis)
{
	char string[16];

	strncpy(string, _("Average:"), 16);
	string[15] = '\0';
	write_stats_core(2, curr, dis, TRUE, string, string);
}

/*
 ***************************************************************************
 * Get previous and current timestamps, then display statistics.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 * @dis		TRUE if a header line must be printed.
 *
 * RETURNS:
 * 0 if all the processes to display have terminated.
 * <> 0 if there are still some processes left to display.
 ***************************************************************************
 */
int write_stats(int curr, int dis)
{
	char cur_time[2][TIMESTAMP_LEN];

	/* Get previous timestamp */
	if (DISPLAY_ONELINE(pidflag)) {
		strcpy(cur_time[!curr], "# Time     ");
	}
	else if (PRINT_SEC_EPOCH(pidflag)) {
		snprintf(cur_time[!curr], TIMESTAMP_LEN, "%-11ld", mktime(&ps_tstamp[!curr]));
		cur_time[!curr][TIMESTAMP_LEN - 1] = '\0';
	}
	else if (is_iso_time_fmt()) {
		strftime(cur_time[!curr], sizeof(cur_time[!curr]), "%H:%M:%S", &ps_tstamp[!curr]);
	}
	else {
		strftime(cur_time[!curr], sizeof(cur_time[!curr]), "%X", &ps_tstamp[!curr]);
	}

	/* Get current timestamp */
	if (PRINT_SEC_EPOCH(pidflag)) {
		snprintf(cur_time[curr], TIMESTAMP_LEN, "%-11ld", mktime(&ps_tstamp[curr]));
		cur_time[curr][TIMESTAMP_LEN - 1] = '\0';
	}
	else if (is_iso_time_fmt()) {
		strftime(cur_time[curr], sizeof(cur_time[curr]), "%H:%M:%S", &ps_tstamp[curr]);
	}
	else {
		strftime(cur_time[curr], sizeof(cur_time[curr]), "%X", &ps_tstamp[curr]);
	}

	return (write_stats_core(!curr, curr, dis, FALSE,
				 cur_time[!curr], cur_time[curr]));
}

/*
 ***************************************************************************
 * Main loop: Read and display PID stats.
 *
 * IN:
 * @dis_hdr		Set to TRUE if the header line must always be printed.
 * @rows		Number of rows of screen.
 * @pid_array_nr	Length of the PID list.
 ***************************************************************************
 */
void rw_pidstat_loop(int dis_hdr, int rows, unsigned int pid_array_nr)
{
	int curr = 1, dis = 1;
	int again;
	unsigned long lines = rows;

	/* Don't buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	/* Read system uptime */
	read_uptime(&uptime_cs[0]);
	read_stats(0, pid_array_nr);

	if (DISPLAY_MEM(actflag)) {
		/* Get total memory */
		read_proc_meminfo();
	}

	if (!interval) {
		/* Display since boot time */
		ps_tstamp[1] = ps_tstamp[0];
		memset(st_pid_list[1], 0, PID_STATS_SIZE * pid_nr);
		write_stats(0, DISP_HDR);
		exit(0);
	}

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Save the first stats collected. Will be used to compute the average */
	ps_tstamp[2] = ps_tstamp[0];
	tot_jiffies[2] = tot_jiffies[0];
	uptime_cs[2] = uptime_cs[0];
	memcpy(st_pid_list[2], st_pid_list[0], PID_STATS_SIZE * pid_nr);

	/* Set a handler for SIGINT */
	memset(&int_act, 0, sizeof(int_act));
	int_act.sa_handler = sig_handler;
	sigaction(SIGINT, &int_act, NULL);

	/* Wait for SIGALRM (or possibly SIGINT) signal */
	pause();

	if (signal_caught)
		/* SIGINT/SIGCHLD signals caught during first interval: Exit immediately */
		return;

	do {
		/* Get time */
		get_localtime(&ps_tstamp[curr], 0);

		/* Read system uptime (in 1/100th of a second) */
		read_uptime(&(uptime_cs[curr]));

		/* Read stats */
		read_stats(curr, pid_array_nr);

		if (!dis_hdr) {
			dis = lines / rows;
			if (dis) {
				lines %= rows;
			}
			lines++;
		}

		/* Print results */
		again = write_stats(curr, dis);

		if (!again)
			return;

		if (count > 0) {
			count--;
		}

		if (count) {

			pause();

			if (signal_caught) {
				/* SIGINT/SIGCHLD signals caught => Display average stats */
				count = 0;
				printf("\n");	/* Skip "^C" displayed on screen */
			}
			else {
				curr ^= 1;
			}
		}
	}
	while (count);

	/*
	 * The one line format uses a raw time value rather than time strings
	 * so the average doesn't really fit.
	 */
	if (!DISPLAY_ONELINE(pidflag))
	{
		/* Write stats average */
		write_stats_avg(curr, dis_hdr);
	}
}

/*
 ***************************************************************************
 * Start a program that will be monitored by pidstat.
 *
 * IN:
 * @argc	Number of arguments.
 * @argv	Arguments values.
 *
 * RETURNS:
 * The PID of the program executed.
 ***************************************************************************
 */
pid_t exec_pgm(int argc, char **argv)
{
	pid_t child;
	char *args[argc + 1];
	int i;

	child = fork();

	switch(child) {

		case -1:
			perror("fork");
			exit(4);
			break;

		case 0:
			/* Child */
			for (i = 0; i < argc; i++) {
				args[i] = argv[i];
			}
			args[argc] = NULL;

			execvp(args[0], args);
			perror("exec");
			exit(4);
			break;

		default:
			/*
			 * Parent.
			 * Set a handler for SIGCHLD (signal that will be received
			 * by pidstat when the child program terminates).
			 * The handler is the same as for SIGINT: Stop and display
			 * average statistics.
			 */
			memset(&chld_act, 0, sizeof(chld_act));
			chld_act.sa_handler = sig_handler;
			sigaction(SIGCHLD, &chld_act, NULL);

			return child;
	}
}

/*
 ***************************************************************************
 * Main entry to the pidstat program.
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int opt = 1, dis_hdr = -1;
	int i;
	unsigned int pid, pid_array_nr = 0;
	struct utsname header;
	int rows = 23;
	char *t;

#ifdef USE_NLS
	/* Init National Language Support */
	init_nls();
#endif

	/* Init color strings */
	init_colors();

	/* Get HZ */
	get_HZ();

	/* Compute page shift in kB */
	get_kb_shift();

	/* Allocate structures for device list */
	if (argc > 1) {
		salloc_pid_array((argc / 2) + count_csvalues(argc, argv));
	}

	/* Process args... */
	while (opt < argc) {

		if (!strcmp(argv[opt], "-e")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			pidflag |= P_D_PID;
			update_pid_array(&pid_array_nr, exec_pgm(argc - opt, argv + opt));
			break;
		}

		else if (!strcmp(argv[opt], "-p")) {
			pidflag |= P_D_PID;
			if (!argv[++opt]) {
				usage(argv[0]);
			}

			for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
				if (!strcmp(t, K_ALL)) {
					pidflag |= P_D_ALL_PID;
				}
				else if (!strcmp(t, K_SELF)) {
					update_pid_array(&pid_array_nr, getpid());
				}
				else {
					if (strspn(t, DIGITS) != strlen(t)) {
						usage(argv[0]);
					}
					pid = atoi(t);
					if (pid < 1) {
						usage(argv[0]);
					}
					update_pid_array(&pid_array_nr, pid);
				}
			}
			opt++;
		}

		else if (!strcmp(argv[opt], "-C")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			strncpy(commstr, argv[opt++], MAX_COMM_LEN);
			commstr[MAX_COMM_LEN - 1] = '\0';
			pidflag |= P_F_COMMSTR;
			if (!strlen(commstr)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-G")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			strncpy(procstr, argv[opt++], MAX_COMM_LEN);
			procstr[MAX_COMM_LEN - 1] = '\0';
			pidflag |= P_F_PROCSTR;
			if (!strlen(procstr)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "--human")) {
			pidflag |= P_D_UNIT;
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

		else if (!strcmp(argv[opt], "-T")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			if (tskflag) {
				dis_hdr++;
			}
			if (!strcmp(argv[opt], K_P_TASK)) {
				tskflag |= P_TASK;
			}
			else if (!strcmp(argv[opt], K_P_CHILD)) {
				tskflag |= P_CHILD;
			}
			else if (!strcmp(argv[opt], K_P_ALL)) {
				tskflag |= P_TASK + P_CHILD;
				dis_hdr++;
			}
			else {
				usage(argv[0]);
			}
			opt++;
		}

		/* Option used individually. See below for grouped option */
		else if (!strcmp(argv[opt], "-U")) {
			/* Display username instead of UID */
			pidflag |= P_D_USERNAME;
			if (argv[++opt] && (argv[opt][0] != '-') &&
			    (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
				strncpy(userstr, argv[opt++], MAX_USER_LEN);
				userstr[MAX_USER_LEN - 1] = '\0';
				pidflag |= P_F_USERSTR;
				if (!strlen(userstr)) {
					usage(argv[0]);
				}
			}
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'd':
					/* Display I/O usage */
					actflag |= P_A_IO;
					dis_hdr++;
					break;

				case 'H':
					/* Display timestamps in sec since the epoch */
					pidflag |= P_D_SEC_EPOCH;
					break;

				case 'h':
					/* Display stats on one line */
					pidflag |= P_D_ONELINE;
					break;

				case 'I':
					/* IRIX mode off */
					pidflag |= P_F_IRIX_MODE;
					break;

				case 'l':
					/* Display whole command line */
					pidflag |= P_D_CMDLINE;
					break;

				case 'R':
					/* Display priority and policy info */
					actflag |= P_A_RT;
					dis_hdr++;
					break;

				case 'r':
					/* Display memory usage */
					actflag |= P_A_MEM;
					dis_hdr++;
					break;

				case 's':
					/* Display stack sizes */
					actflag |= P_A_STACK;
					dis_hdr++;
					break;

				case 't':
					/* Display stats for threads */
					pidflag |= P_D_TID;
					break;

				case 'U':
					/* When option is grouped, it cannot take an arg */
					pidflag |= P_D_USERNAME;
					break;

				case 'u':
					/* Display CPU usage */
					actflag |= P_A_CPU;
					dis_hdr++;
					break;

				case 'V':
					/* Print version number and exit */
					print_version();
					break;

				case 'v':
					/* Display some kernel tables values */
					actflag |= P_A_KTAB;
					dis_hdr++;
					break;

				case 'w':
					/* Display context switches */
					actflag |= P_A_CTXSW;
					dis_hdr++;
					break;

				default:
					usage(argv[0]);
				}
			}
			opt++;
		}

		else if (interval < 0) {	/* Get interval */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			interval = atol(argv[opt++]);
			if (interval < 0) {
				usage(argv[0]);
			}
			count = -1;
		}

		else if (count <= 0) {	/* Get count value */
			if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
			    !interval) {
				usage(argv[0]);
			}
			count = atol(argv[opt++]);
			if (count < 1) {
				usage(argv[0]);
			}
		}
		else {
			usage(argv[0]);
		}
	}

	if (interval < 0) {
		/* Interval not set => display stats since boot time */
		interval = 0;
	}

	/* Check flags and set default values */
	check_flags();

	/* Init structures */
	pid_sys_init(pid_array_nr);

	if (dis_hdr < 0) {
		dis_hdr = 0;
	}
	if (!dis_hdr) {
		if (pid_nr > 1) {
			dis_hdr = 1;
		}
		else {
			rows = get_win_height();
		}
	}

	/* Get time */
	get_localtime(&(ps_tstamp[0]), 0);

	/* Get system name, release number and hostname */
	uname(&header);
	print_gal_header(&(ps_tstamp[0]), header.sysname, header.release,
			 header.nodename, header.machine, cpu_nr,
			 PLAIN_OUTPUT);

	/* Main loop */
	rw_pidstat_loop(dis_hdr, rows, pid_array_nr);

	/* Free structures */
	free(pid_array);
	sfree_pid();

	return 0;
}
