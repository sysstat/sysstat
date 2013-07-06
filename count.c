/*
 * count.c: Count items for which statistics will be collected.
 * (C) 1999-2013 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
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
 * RETURNS:
 * Number of processors (online and offline).
 * A value of 0 means that /sys was not mounted.
 * A value of N (!=0) means N processor(s) (cpu0 .. cpu(N-1)).
 ***************************************************************************
 */
int get_sys_cpu_nr(void)
{
	DIR *dir;
	struct dirent *drd;
	struct stat buf;
	char line[MAX_PF_NAME];
	int proc_nr = 0;

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
				proc_nr++;
			}
		}
	}

	/* Close directory */
	closedir(dir);

	return proc_nr;
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

	while (fgets(line, 16, fp) != NULL) {

		if (strncmp(line, "cpu ", 4) && !strncmp(line, "cpu", 3)) {
			sscanf(line + 3, "%d", &num_proc);
			if (num_proc > proc_nr) {
				proc_nr = num_proc;
			}
		}
	}

	fclose(fp);

	return (proc_nr + 1);
}

/*
 ***************************************************************************
 * Count the number of processors on the machine.
 * Try to use /sys for that, or /proc/stat if /sys doesn't exist.
 *
 * IN:
 * @max_nr_cpus	Maximum number of proc that sysstat can handle.
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
int get_cpu_nr(unsigned int max_nr_cpus)
{
	int cpu_nr;

	if ((cpu_nr = get_sys_cpu_nr()) == 0) {
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
 * Number of interrupts per processor + a pre-allocation constant.
 ***************************************************************************
 */
int get_irqcpu_nr(char *file, int max_nr_irqcpu, int cpu_nr)
{
	FILE *fp;
	char *line = NULL;
	unsigned int irq = 0;
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
int get_diskstats_dev_nr(int count_part, int only_used_dev)
{
	FILE *fp;
	char line[256];
	char dev_name[MAX_NAME_LEN];
	int dev = 0, i;
	unsigned long rd_ios, wr_ios;

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		/* File non-existent */
		return 0;

	/*
	 * Counting devices and partitions is simply a matter of counting
	 * the number of lines...
	 */
	while (fgets(line, 256, fp) != NULL) {
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

#ifdef SOURCE_SADC
/*---------------- BEGIN: FUNCTIONS USED BY SADC ONLY ---------------------*/

/*
 ***************************************************************************
 * Count number of interrupts that are in /proc/stat file.
 *
 * RETURNS:
 * Number of interrupts, including total number of interrupts.
 ***************************************************************************
 */
int get_irq_nr(void)
{
	FILE *fp;
	char line[8192];
	int in = 0;
	int pos = 4;

	if ((fp = fopen(STAT, "r")) == NULL)
		return 0;

	while (fgets(line, 8192, fp) != NULL) {

		if (!strncmp(line, "intr ", 5)) {

			while (pos < strlen(line)) {
				in++;
				pos += strcspn(line + pos + 1, " ") + 1;
			}
		}
	}

	fclose(fp);

	return in;
}

/*
 ***************************************************************************
 * Find number of serial lines that support tx/rx accounting
 * in /proc/tty/driver/serial file.
 *
 * RETURNS:
 * Number of serial lines supporting tx/rx accouting.
 ***************************************************************************
 */
int get_serial_nr(void)
{
	FILE *fp;
	char line[256];
	int sl = 0;

	if ((fp = fopen(SERIAL, "r")) == NULL)
		return 0;	/* No SERIAL file */

	while (fgets(line, 256, fp) != NULL) {
		/*
		 * tx/rx statistics are always present,
		 * except when serial line is unknown.
		 */
		if (strstr(line, "tx:") != NULL) {
			sl++;
		}
	}

	fclose(fp);

	return sl;
}

/*
 ***************************************************************************
 * Find number of interfaces (network devices) that are in /proc/net/dev
 * file.
 *
 * RETURNS:
 * Number of network interfaces.
 ***************************************************************************
 */
int get_iface_nr(void)
{
	FILE *fp;
	char line[128];
	int iface = 0;

	if ((fp = fopen(NET_DEV, "r")) == NULL)
		return 0;	/* No network device file */

	while (fgets(line, 128, fp) != NULL) {
		if (strchr(line, ':')) {
			iface++;
		}
	}

	fclose(fp);

	return iface;
}

/*
 ***************************************************************************
 * Get number of devices in /proc/diskstats.
 *
 * IN:
 * @f	Non zero (true) if disks *and* partitions should be counted, and
 *	zero (false) if only disks must be counted.
 *
 * RETURNS:
 * Number of devices.
 ***************************************************************************
 */
int get_disk_nr(unsigned int f)
{
	int disk_nr;

	/*
	 * Partitions are taken into account by sar -d only with
	 * kernels 2.6.25 and later.
	 */
	disk_nr = get_diskstats_dev_nr(f, CNT_USED_DEV);

	return disk_nr;
}

/*
 ***************************************************************************
 * Count number of possible frequencies for CPU#0.
 *
 * RETURNS:
 * Number of frequencies.
 ***************************************************************************
 */
int get_freq_nr(void)
{
	FILE *fp;
	char filename[MAX_PF_NAME];
	char line[128];
	int freq = 0;

	snprintf(filename, MAX_PF_NAME, "%s/cpu0/%s",
		 SYSFS_DEVCPU, SYSFS_TIME_IN_STATE);
	if ((fp = fopen(filename, "r")) == NULL)
		return 0;	/* No time_in_state file for CPU#0 */

	while (fgets(line, 128, fp) != NULL) {
		freq++;
	}

	fclose(fp);

	return freq;
}

/*
 ***************************************************************************
 * Count number of USB devices in /sys/bus/usb/devices.
 *
 * RETURNS:
 * Number of USB devices plugged into the system.
 * Don't count USB root hubs.
 * Return -1 if directory doesn't exist in sysfs.
 ***************************************************************************
 */
int get_usb_nr(void)
{
	DIR *dir;
	struct dirent *drd;
	int usb = 0;

	/* Open relevant /sys directory */
	if ((dir = opendir(SYSFS_USBDEV)) == NULL)
		return -1;

	/* Get current file entry */
	while ((drd = readdir(dir)) != NULL) {

		if (isdigit(drd->d_name[0]) && !strchr(drd->d_name, ':')) {
			usb++;
		}
	}

	/* Close directory */
	closedir(dir);

	return usb;
}

/*
 ***************************************************************************
 * Find number of filesystems in /etc/mtab. Pseudo-filesystems are ignored.
 *
 * RETURNS:
 * Number of filesystems.
 ***************************************************************************
 */
int get_filesystem_nr(void)
{
	FILE *fp;
	char line[256], fs_name[MAX_FS_LEN], mountp[128];
	int fs = 0;
	struct statfs buf;

	if ((fp = fopen(MTAB, "r")) == NULL)
		/* File non-existent */
		return 0;

	/* Get current filesystem */
	while (fgets(line, 256, fp) != NULL) {
		if (line[0] == '/') {
			
			/* Read filesystem name and mount point */
			sscanf(line, "%71s %127s", fs_name, mountp);
			
			/* Replace octal codes */
			oct2chr(mountp);
			
			/* Check that total size is not null */
			if (statfs(mountp, &buf) < 0)
				continue;
			
			if (buf.f_blocks) {
				fs++;
			}
		}
	}

	fclose(fp);

	return fs;
}

/*------------------ END: FUNCTIONS USED BY SADC ONLY ---------------------*/
#endif /* SOURCE_SADC */
