/*
 * sysstat test functions.
 * (C) 2019-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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

#ifdef TEST

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>

#include "systest.h"

time_t __unix_time = 0;
int __env = 0;

extern long interval;
extern int sigint_caught;

/*
 ***************************************************************************
 * Test mode: Instead of reading system time, use time given on the command
 * line.
 *
 * RETURNS:
 * Number of seconds since the epoch, as given on the command line.
 ***************************************************************************
 */
time_t get_unix_time(time_t *t)
{
	return __unix_time;
}

/*
 ***************************************************************************
 * Test mode: Get time of the day using __unix_time variable contents.
 *
 * OUT:
 * @tv	Number of seconds since the Epoch.
 ***************************************************************************
 */
void get_day_time(struct timeval *tv)
{
	__unix_time += interval;
	tv->tv_sec = __unix_time;
	tv->tv_usec = 0;
}

/*
 ***************************************************************************
 * Test mode: Send bogus information about current kernel.
 *
 * OUT:
 * @h	Structure with kernel information.
 ***************************************************************************
 */
void get_uname(struct utsname *h)
{
	strcpy(h->sysname, "Linux");
	strcpy(h->nodename, "SYSSTAT.TEST");
	strcpy(h->release, "1.2.3-TEST");
	strcpy(h->machine, "x86_64");
}

/*
 ***************************************************************************
 * Test mode: Send bogus information about current filesystem.
 *
 * OUT:
 * @buf	Structure with filesystem information.
 ***************************************************************************
 */
int get_fs_stat(char *c, struct statvfs *buf)
{
	static int p = 0;
	unsigned long long bfree[4] = {89739427840, 293286670336, 11696156672, 292616732672};
	unsigned long long blocks[4] = {97891291136, 309502345216, 30829043712, 309502345216};
	unsigned long long bavail[4] = {84722675712, 277541253120, 10106515456, 276871315456};
	unsigned long long files[4] = {6111232, 19202048, 1921360, 19202048};
	unsigned long long ffree[4] = {6008414, 19201593, 1621550, 19051710};

	buf->f_bfree = bfree[p];
	buf->f_blocks = blocks[p];
	buf->f_bavail = bavail[p];
	buf->f_frsize = 1;
	buf->f_files = files[p];
	buf->f_ffree = ffree[p];

	p = (p + 1) & 0x3;

	return 0;
}

/*
 ***************************************************************************
 * Test mode: Ignore environment variable value.
 ***************************************************************************
 */
char *get_env_value(const char *c)
{
	if (!__env)
		return NULL;

	fprintf(stderr, "Reading contents of %s\n", c);
	return getenv(c);
}

/*
 ***************************************************************************
 * Test mode: Go to next time period.
 ***************************************************************************
 */
void next_time_step(void)
{
	int root_nr = 1;
	char rootf[64], testf[64];
	char *resolved_name;

	__unix_time += interval;

	if ((resolved_name = realpath(ROOTDIR, NULL)) != NULL) {
		if (strlen(resolved_name) > 4) {
			root_nr = atoi(resolved_name + strlen(resolved_name) - 1);
		}
		free(resolved_name);
	}
	if ((unlink(ROOTDIR) < 0) && (errno != ENOENT)) {
		perror("unlink");
		exit(1);
	}

	sprintf(rootf, "%s%d", ROOTFILE, ++root_nr);
	sprintf(testf, "%s/%s", TESTDIR, rootf);
	if (access(testf, F_OK) < 0) {
		if (errno == ENOENT) {
			/* No more kernel directories: Simulate a Ctrl/C */
			int_handler(0);
			return ;
		}
	}

	if (symlink(rootf, ROOTDIR) < 0) {
		perror("link");
		exit(1);
	}
}

/*
 ***************************************************************************
 * If current file is considered as a virtual one ("virtualhd"), then set
 * its device ID (major 253, minor 2, corresponding here to dm-2) in the
 * stat structure normally filled by the stat() system call.
 * Otherwise, open file and read its major and minor numbers.
 *
 * IN:
 * @name	Pathname to file.
 *
 * OUT:
 * @statbuf	Structure containing device ID.
 *
 * RETURNS:
 * 0 if it actually was the virtual device, 1 otherwise, and -1 on failure.
 ***************************************************************************
 */
int virtual_stat(const char *name, struct stat *statbuf)
{
	FILE *fp;
	char line[128];
	int major, minor;

	if (!strcmp(name, VIRTUALHD)) {
		statbuf->st_rdev = (253 << MINORBITS) + 2;
		return 0;
	}

	statbuf->st_rdev = 0;

	if ((fp = fopen(name, "r")) == NULL)
		return -1;

	if (fgets(line, sizeof(line), fp) != NULL) {
		sscanf(line, "%d %d", &major, &minor);
		statbuf->st_rdev = (major << MINORBITS) + minor;
	}

	fclose(fp);

	return 1;
}

/*
 ***************************************************************************
 * Open a "_list" file containing a list of files enumerated in a known
 * order contained in current directory.
 *
 * IN:
 * @name	Pathname to directory containing the "_list" file.
 *
 * RETURNS:
 * A pointer on current "_list" file.
 ***************************************************************************
 */
DIR *open_list(const char *name)
{
	FILE *fp;
	char filename[1024];

	snprintf(filename, sizeof(filename), "%s/%s", name, _LIST);
	filename[sizeof(filename) - 1] = '\0';

	if ((fp = fopen(filename, "r")) == NULL)
		return NULL;

	return (DIR *) fp;
}

/*
 ***************************************************************************
 * Read next file name contained in a "_list" file.
 *
 * IN:
 * @dir	Pointer on current "_list" file.
 *
 * RETURNS:
 * A structure containing the name of the next file to read.
 ***************************************************************************
 */
struct dirent *read_list(DIR *dir)
{
	FILE *fp = (FILE *) dir;
	static struct dirent drd;
	char line[1024];


	if ((fgets(line, sizeof(line), fp) != NULL) && (strlen(line) > 1) &&
		(strlen(line) < sizeof(drd.d_name))) {
		strcpy(drd.d_name, line);
		drd.d_name[strlen(line) - 1] = '\0';
		return &drd;
	}

	return NULL;
}

/*
 ***************************************************************************
 * Close a "_list" file.
 *
 * IN:
 * @dir	Pointer on "_list" file to close.
 ***************************************************************************
 */
void close_list(DIR *dir)
{
	FILE *fp = (FILE *) dir;

	fclose(fp);
}

/*
 ***************************************************************************
 * Replacement function for realpath() system call. Do nothing here.
 *
 * IN:
 * @name	Pathname to process.
 * @c		Unused here.
 *
 * RETURNS:
 * Pathname (unchanged).
 ***************************************************************************
 */
char *get_realname(char *name, char *c)
{
	char *resolved_name;

	if ((resolved_name = (char *) malloc(1024)) == NULL) {
		perror("malloc");
		exit(4);
	}
	strncpy(resolved_name, name, 1024);
	resolved_name[1023] = '\0';

	return resolved_name;
}

/*
 ***************************************************************************
 * Replacement function for getpwuid() system call. Fill a dummy passwd
 * structure containing the name of a user.
 *
 * IN:
 * @uid		UID of the user
 *
 * RETURNS:
 * Pointer on the passwd structure.
 ***************************************************************************
 */
struct passwd *get_usrname(uid_t uid)
{
	static struct passwd pwd_ent;
	static char pw_name[16];

	pwd_ent.pw_name = pw_name;
	if (!uid) {
		strcpy(pwd_ent.pw_name, "root");
	}
	else {
		strcpy(pwd_ent.pw_name, "testusr");
	}

	return &pwd_ent;
}

/*
 ***************************************************************************
 * Replacement function for fork() system call. Don't fork really but return
 * a known PID number.
 *
 * RETURNS:
 * Known PID number.
 ***************************************************************************
 */
pid_t get_known_pid(void)
{
	return 8741;
}

#endif	/* TEST */

