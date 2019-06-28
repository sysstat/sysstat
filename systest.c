/*
 * sysstat test functions.
 * (C) 2019 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

#include "systest.h"

time_t __unix_time = 0;
extern long interval;
extern int sigint_caught;

/*
 ***************************************************************************
 * Test mode: Instead of reading system time, use time given on the command
 * line.
 *
 * OUT:
 * @t	Number of seconds since the epoch, as given on the command line.
 ***************************************************************************
 */
void get_unix_time(time_t *t)
{
	*t = __unix_time;
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

	p = (p + 1) & 0x11;

	return 0;
}

/*
 ***************************************************************************
 * Test mode: Ignore environment variable value.
 ***************************************************************************
 */
char *get_env_value(char *c)
{
	return NULL;
}

/*
 ***************************************************************************
 * Test mode: Go to next time period.
 ***************************************************************************
 */
void next_time_step(void)
{
	static int root_nr = 1;
	char rootf[64], testf[64];

	__unix_time += interval;

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
 *
 * IN:
 * @name	Pathname to file.
 *
 * OUT:
 * @statbuf	Structure containing device ID.
 *
 * RETURNS:
 * 0 if it actually was the virtual device, 1 otherwise.
 ***************************************************************************
 */
int virtual_stat(const char *name, struct stat *statbuf)
{
	if (!strcmp(name, VIRTUALHD)) {
		statbuf->st_rdev = (253 << 8) + 2;
		return 0;
	}

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

#endif	/* TEST */

