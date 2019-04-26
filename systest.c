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

#include <string.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

#include "systest.h"

time_t __unix_time;

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

#endif	/* TEST */

