/*
 * count.c: Count items for which statistics will be collected.
 * (C) 1999-2018 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "common.h"
#include "rd_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


/*
 ***************************************************************************
 * Count number of processors in /sys.
 *
 * IN:
 * @highest	If set to TRUE, then look for the highest processor number.
 * 		This is used when eg. the machine has 4 CPU numbered 0, 1, 4
 *		and 5. In this case, this procedure will return 6.
 *
 * RETURNS:
 * Number of processors (online and offline).
 * A value of 0 means that /sys was not mounted.
 * A value of N (!=0) means N processor(s) (cpu0 .. cpu(N-1)).
 ***************************************************************************
 */
int get_sys_cpu_nr(int highest)
{
	DIR *dir;
	struct dirent *drd;
	struct stat buf;
	char line[MAX_PF_NAME];
	int num_proc, proc_nr = -1;

	/* Open relevant /sys directory */
	if ((dir = opendir(SYSFS_DEVCPU)) == NULL)
		return 0;

	/* Get current file entry */
	while ((drd = readdir(dir)) != NULL) {

		if (!strncmp(drd->d_name, "cpu", 3) && isdigit(drd->d_name[3])) {
			snprintf(line, MAX_PF_NAME, "%s/%s", SYSFS_DEVCPU, drd->d_name);
			line[MAX_PF_NAME - 1] = '\0';
			if (stat(line, &buf) < 0)
				continue;
			if (S_ISDIR(buf.st_mode)) {
				if (highest) {
					sscanf(drd->d_name + 3, "%d", &num_proc);
					if (num_proc > proc_nr) {
						proc_nr = num_proc;
					}
				}
				else {
					proc_nr++;
				}
			}
		}
	}

	/* Close directory */
	closedir(dir);

	return (proc_nr + 1);
}

/*
 ***************************************************************************
 * Count number of processors in /proc/stat.
 *
 * RETURNS:
 * Number of processors. The returned value is greater than or equal to the
 * number of online processors.
 * A value of 0 means one processor and non SMP kernel.
 * A value of N (!=0) means N processor(s) (0 .. N-1) with SMP kernel.
 ***************************************************************************
 */
int get_proc_cpu_nr(void)
{
	FILE *fp;
	char line[16];
	int num_proc, proc_nr = -1;

	if ((fp = fopen(STAT, "r")) == NULL) {
		fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
		exit(1);
	}

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (strncmp(line, "cpu ", 4) && !strncmp(line, "cpu", 3)) {
			sscanf(line + 3, "%d", &num_proc);
			if (num_proc > proc_nr) {
				proc_nr = num_proc;
			}
		}
	}

	fclose(fp);

	proc_nr++;
	return proc_nr;
}

/*
 ***************************************************************************
 * Count the number of processors on the machine, or look for the
 * highest processor number.
 * Try to use /sys for that, or /proc/stat if /sys doesn't exist.
 *
 * IN:
 * @max_nr_cpus	Maximum number of proc that sysstat can handle.
 * @highest	If set to TRUE, then look for the highest processor number.
 * 		This is used when eg. the machine has 4 CPU numbered 0, 1, 4
 *		and 5. In this case, this procedure will return 6.
 *
 * RETURNS:
 * Number of processors.
 * 0: one proc and non SMP kernel.
 * 1: one proc and SMP kernel (NB: On SMP machines where all the CPUs but
 *    one have been disabled, we get the total number of proc since we use
 *    /sys to count them).
 * 2: two proc...
 ***************************************************************************
 */
__nr_t get_cpu_nr(unsigned int max_nr_cpus, int highest)
{
	__nr_t cpu_nr;

	if ((cpu_nr = get_sys_cpu_nr(highest)) == 0) {
		/* /sys may be not mounted. Use /proc/stat instead */
		cpu_nr = get_proc_cpu_nr();
	}

	if (cpu_nr > max_nr_cpus) {
		fprintf(stderr, _("Cannot handle so many processors!\n"));
		exit(1);
	}

	return cpu_nr;
}

/*
 ***************************************************************************
 * Find number of interrupts available per processor (use
 * /proc/interrupts file or /proc/softirqs).
 *
 * IN:
 * @file		/proc file to read (interrupts or softirqs).
 * @max_nr_irqcpu       Maximum number of interrupts per processor that
 *                      sadc can handle.
 * @cpu_nr		Number of processors.
 *
 * RETURNS:
 * Number of interrupts per processor.
 ***************************************************************************
 */
__nr_t get_irqcpu_nr(char *file, int max_nr_irqcpu, int cpu_nr)
{
	FILE *fp;
	char *line = NULL;
	__nr_t irq = 0;
	int p;

	if ((fp = fopen(file, "r")) == NULL)
		return 0;       /* No interrupts file */

	SREALLOC(line, char, INTERRUPTS_LINE + 11 * cpu_nr);

	while ((fgets(line, INTERRUPTS_LINE + 11 * cpu_nr , fp) != NULL) &&
	       (irq < max_nr_irqcpu)) {
		p = strcspn(line, ":");
		if ((p > 0) && (p < 16)) {
			irq++;
		}
	}

	fclose(fp);

	free(line);

	return irq;
}

/*
 ***************************************************************************
 * Find number of devices and partitions available in /proc/diskstats.
 *
 * IN:
 * @count_part		Set to TRUE if devices _and_ partitions are to be
 *			counted.
 * @only_used_dev	When counting devices, set to TRUE if only devices
 *			with non zero stats must be counted.
 *
 * RETURNS:
 * Number of devices (and partitions).
 ***************************************************************************
 */
__nr_t get_diskstats_dev_nr(int count_part, int only_used_dev)
{
	FILE *fp;
	char line[256];
	char dev_name[MAX_NAME_LEN];
	__nr_t dev = 0;
	int i;
	unsigned long rd_ios, wr_ios;

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		/* File non-existent */
		return 0;

	/*
	 * Counting devices and partitions is simply a matter of counting
	 * the number of lines...
	 */
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (!count_part) {
			i = sscanf(line, "%*d %*d %s %lu %*u %*u %*u %lu",
				   dev_name, &rd_ios, &wr_ios);
			if ((i == 2) || !is_device(dev_name, ACCEPT_VIRTUAL_DEVICES))
				/* It was a partition and not a device */
				continue;
			if (only_used_dev && !rd_ios && !wr_ios)
				/* Unused device */
				continue;
		}
		dev++;
	}

	fclose(fp);

	return dev;
}
