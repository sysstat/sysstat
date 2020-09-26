/*
 * rd_stats.c: Read system statistics
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

/* Generic PSI structure */
struct stats_psi {
	unsigned long long total;
	unsigned long	   avg10;
	unsigned long	   avg60;
	unsigned long	   avg300;
};

/*
 ***************************************************************************
 * Read CPU statistics.
 * Remember that this function is used by several sysstat commands!
 *
 * IN:
 * @st_cpu	Buffer where structures containing stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_cpu	Buffer with statistics.
 *
 * RETURNS:
 * Highest CPU number(*) for which statistics have been read.
 * 1 means CPU "all", 2 means CPU 0, 3 means CPU 1, etc.
 * Or -1 if the buffer was too small and needs to be reallocated.
 *
 * (*)This doesn't account for all processors in the machine in the case
 * where some CPU are offline and located at the end of the list.
 ***************************************************************************
 */
__nr_t read_stat_cpu(struct stats_cpu *st_cpu, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_cpu *st_cpu_i;
	struct stats_cpu sc;
	char line[8192];
	int proc_nr;
	__nr_t cpu_read = 0;

	if ((fp = fopen(STAT, "r")) == NULL) {
		fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
		exit(2);
	}

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "cpu ", 4)) {

			/*
			 * All the fields don't necessarily exist,
			 * depending on the kernel version used.
			 */
			memset(st_cpu, 0, STATS_CPU_SIZE);

			/*
			 * Read the number of jiffies spent in the different modes
			 * (user, nice, etc.) among all proc. CPU usage is not reduced
			 * to one processor to avoid rounding problems.
			 */
			sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			       &st_cpu->cpu_user,
			       &st_cpu->cpu_nice,
			       &st_cpu->cpu_sys,
			       &st_cpu->cpu_idle,
			       &st_cpu->cpu_iowait,
			       &st_cpu->cpu_hardirq,
			       &st_cpu->cpu_softirq,
			       &st_cpu->cpu_steal,
			       &st_cpu->cpu_guest,
			       &st_cpu->cpu_guest_nice);

			if (!cpu_read) {
				cpu_read = 1;
			}

			if (nr_alloc == 1)
				/* We just want to read stats for CPU "all" */
				break;
		}

		else if (!strncmp(line, "cpu", 3)) {
			/* All the fields don't necessarily exist */
			memset(&sc, 0, STATS_CPU_SIZE);
			/*
			 * Read the number of jiffies spent in the different modes
			 * (user, nice, etc) for current proc.
			 * This is done only on SMP machines.
			 */
			sscanf(line + 3, "%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			       &proc_nr,
			       &sc.cpu_user,
			       &sc.cpu_nice,
			       &sc.cpu_sys,
			       &sc.cpu_idle,
			       &sc.cpu_iowait,
			       &sc.cpu_hardirq,
			       &sc.cpu_softirq,
			       &sc.cpu_steal,
			       &sc.cpu_guest,
			       &sc.cpu_guest_nice);

			if (proc_nr + 2 > nr_alloc) {
				cpu_read = -1;
				break;
			}

			st_cpu_i = st_cpu + proc_nr + 1;
			*st_cpu_i = sc;

			if (proc_nr + 2 > cpu_read) {
				cpu_read = proc_nr + 2;
			}
		}
	}

	fclose(fp);
	return cpu_read;
}

/*
 ***************************************************************************
 * Read interrupts statistics from /proc/stat.
 * Remember that this function is used by several sysstat commands!
 *
 * IN:
 * @st_irq	Structure where stats will be saved.
 * @nr_alloc	Number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_irq	Structure with statistics.
 *
 * RETURNS:
 * Number of interrupts read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_stat_irq(struct stats_irq *st_irq, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_irq *st_irq_i;
	char line[8192];
	int i, pos;
	unsigned long long irq_nr;
	__nr_t irq_read = 0;

	if ((fp = fopen(STAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "intr ", 5)) {
			/* Read total number of interrupts received since system boot */
			sscanf(line + 5, "%llu", &st_irq->irq_nr);
			pos = strcspn(line + 5, " ") + 5;

			irq_read++;
			if (nr_alloc == 1)
				/* We just want to read the total number of interrupts */
				break;

			do {
				i = sscanf(line + pos, " %llu", &irq_nr);
				if (i < 1)
					break;

				if (irq_read + 1 > nr_alloc) {
					irq_read = -1;
					break;
				}
				st_irq_i = st_irq + irq_read++;
				st_irq_i->irq_nr = irq_nr;

				i = strcspn(line + pos + 1, " ");
				pos += i + 1;
			}
			while ((i > 0) && (pos < (sizeof(line) - 1)));

			break;
		}
	}

	fclose(fp);
	return irq_read;
}

/*
 ***************************************************************************
 * Read memory statistics from /proc/meminfo.
 *
 * IN:
 * @st_memory	Structure where stats will be saved.
 *
 * OUT:
 * @st_memory	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_meminfo(struct stats_memory *st_memory)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(MEMINFO, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "MemTotal:", 9)) {
			/* Read the total amount of memory in kB */
			sscanf(line + 9, "%llu", &st_memory->tlmkb);
		}
		else if (!strncmp(line, "MemFree:", 8)) {
			/* Read the amount of free memory in kB */
			sscanf(line + 8, "%llu", &st_memory->frmkb);
		}
		else if (!strncmp(line, "MemAvailable:", 13)) {
			/* Read the amount of available memory in kB */
			sscanf(line + 13, "%llu", &st_memory->availablekb);
		}
		else if (!strncmp(line, "Buffers:", 8)) {
			/* Read the amount of buffered memory in kB */
			sscanf(line + 8, "%llu", &st_memory->bufkb);
		}
		else if (!strncmp(line, "Cached:", 7)) {
			/* Read the amount of cached memory in kB */
			sscanf(line + 7, "%llu", &st_memory->camkb);
		}
		else if (!strncmp(line, "SwapCached:", 11)) {
			/* Read the amount of cached swap in kB */
			sscanf(line + 11, "%llu", &st_memory->caskb);
		}
		else if (!strncmp(line, "Active:", 7)) {
			/* Read the amount of active memory in kB */
			sscanf(line + 7, "%llu", &st_memory->activekb);
		}
		else if (!strncmp(line, "Inactive:", 9)) {
			/* Read the amount of inactive memory in kB */
			sscanf(line + 9, "%llu", &st_memory->inactkb);
		}
		else if (!strncmp(line, "SwapTotal:", 10)) {
			/* Read the total amount of swap memory in kB */
			sscanf(line + 10, "%llu", &st_memory->tlskb);
		}
		else if (!strncmp(line, "SwapFree:", 9)) {
			/* Read the amount of free swap memory in kB */
			sscanf(line + 9, "%llu", &st_memory->frskb);
		}
		else if (!strncmp(line, "Dirty:", 6)) {
			/* Read the amount of dirty memory in kB */
			sscanf(line + 6, "%llu", &st_memory->dirtykb);
		}
		else if (!strncmp(line, "Committed_AS:", 13)) {
			/* Read the amount of commited memory in kB */
			sscanf(line + 13, "%llu", &st_memory->comkb);
		}
		else if (!strncmp(line, "AnonPages:", 10)) {
			/* Read the amount of pages mapped into userspace page tables in kB */
			sscanf(line + 10, "%llu", &st_memory->anonpgkb);
		}
		else if (!strncmp(line, "Slab:", 5)) {
			/* Read the amount of in-kernel data structures cache in kB */
			sscanf(line + 5, "%llu", &st_memory->slabkb);
		}
		else if (!strncmp(line, "KernelStack:", 12)) {
			/* Read the kernel stack utilization in kB */
			sscanf(line + 12, "%llu", &st_memory->kstackkb);
		}
		else if (!strncmp(line, "PageTables:", 11)) {
			/* Read the amount of memory dedicated to the lowest level of page tables in kB */
			sscanf(line + 11, "%llu", &st_memory->pgtblkb);
		}
		else if (!strncmp(line, "VmallocUsed:", 12)) {
			/* Read the amount of vmalloc area which is used in kB */
			sscanf(line + 12, "%llu", &st_memory->vmusedkb);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read machine uptime, independently of the number of processors.
 *
 * OUT:
 * @uptime	Uptime value in hundredths of a second.
 ***************************************************************************
 */
void read_uptime(unsigned long long *uptime)
{
	FILE *fp = NULL;
	char line[128];
	unsigned long up_sec, up_cent;
	int err = FALSE;

	if ((fp = fopen(UPTIME, "r")) == NULL) {
		err = TRUE;
	}
	else if (fgets(line, sizeof(line), fp) == NULL) {
		err = TRUE;
	}
	else if (sscanf(line, "%lu.%lu", &up_sec, &up_cent) == 2) {
		*uptime = (unsigned long long) up_sec * 100 +
			  (unsigned long long) up_cent;
	}
	else {
		err = TRUE;
	}

	if (fp != NULL) {
		fclose(fp);
	}
	if (err) {
		fprintf(stderr, _("Cannot read %s\n"), UPTIME);
		exit(2);
	}
}

/*
 ***************************************************************************
 * Compute "extended" device statistics (service time, etc.).
 *
 * IN:
 * @sdc		Structure with current device statistics.
 * @sdp		Structure with previous device statistics.
 * @itv		Interval of time in 1/100th of a second.
 *
 * OUT:
 * @xds		Structure with extended statistics.
 ***************************************************************************
*/
void compute_ext_disk_stats(struct stats_disk *sdc, struct stats_disk *sdp,
			    unsigned long long itv, struct ext_disk_stats *xds)
{
	xds->util  = S_VALUE(sdp->tot_ticks, sdc->tot_ticks, itv);
	/*
	 * Kernel gives ticks already in milliseconds for all platforms
	 * => no need for further scaling.
	 * Origin (unmerged) flush operations are counted as writes.
	 */
	xds->await = (sdc->nr_ios - sdp->nr_ios) ?
		((sdc->rd_ticks - sdp->rd_ticks) + (sdc->wr_ticks - sdp->wr_ticks) + (sdc->dc_ticks - sdp->dc_ticks)) /
		((double) (sdc->nr_ios - sdp->nr_ios)) : 0.0;
	xds->arqsz = (sdc->nr_ios - sdp->nr_ios) ?
		((sdc->rd_sect - sdp->rd_sect) + (sdc->wr_sect - sdp->wr_sect) + (sdc->dc_sect - sdp->dc_sect)) /
		((double) (sdc->nr_ios - sdp->nr_ios)) : 0.0;
}

/*
 ***************************************************************************
 * Since ticks may vary slightly from CPU to CPU, we'll want
 * to recalculate itv based on this CPU's tick count, rather
 * than that reported by the "cpu" line. Otherwise we
 * occasionally end up with slightly skewed figures, with
 * the skew being greater as the time interval grows shorter.
 *
 * IN:
 * @scc	Current sample statistics for current CPU.
 * @scp	Previous sample statistics for current CPU.
 *
 * RETURNS:
 * Interval of time based on current CPU, expressed in jiffies.
 ***************************************************************************
 */
unsigned long long get_per_cpu_interval(struct stats_cpu *scc,
					struct stats_cpu *scp)
{
	unsigned long long ishift = 0LL;

	if ((scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest)) {
		/*
		 * Sometimes the nr of jiffies spent in guest mode given by the guest
		 * counter in /proc/stat is slightly higher than that included in
		 * the user counter. Update the interval value accordingly.
		 */
		ishift += (scp->cpu_user - scp->cpu_guest) -
		          (scc->cpu_user - scc->cpu_guest);
	}
	if ((scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice)) {
		/*
		 * Idem for nr of jiffies spent in guest_nice mode.
		 */
		ishift += (scp->cpu_nice - scp->cpu_guest_nice) -
		          (scc->cpu_nice - scc->cpu_guest_nice);
	}

	/*
	 * Workaround for CPU coming back online: With recent kernels
	 * some fields (user, nice, system) restart from their previous value,
	 * whereas others (idle, iowait) restart from zero.
	 * For the latter we need to set their previous value to zero to
	 * avoid getting an interval value < 0.
	 * (I don't know how the other fields like hardirq, steal... behave).
	 * Don't assume the CPU has come back from offline state if previous
	 * value was greater than ULLONG_MAX - 0x7ffff (the counter probably
	 * overflew).
	 */
	if ((scc->cpu_iowait < scp->cpu_iowait) && (scp->cpu_iowait < (ULLONG_MAX - 0x7ffff))) {
		/*
		 * The iowait value reported by the kernel can also decrement as
		 * a result of inaccurate iowait tracking. Waiting on IO can be
		 * first accounted as iowait but then instead as idle.
		 * Therefore if the idle value during the same period did not
		 * decrease then consider this is a problem with the iowait
		 * reporting and correct the previous value according to the new
		 * reading. Otherwise, treat this as CPU coming back online.
		 */
		if ((scc->cpu_idle > scp->cpu_idle) || (scp->cpu_idle >= (ULLONG_MAX - 0x7ffff))) {
			scp->cpu_iowait = scc->cpu_iowait;
		}
		else {
			scp->cpu_iowait = 0;
		}
	}
	if ((scc->cpu_idle < scp->cpu_idle) && (scp->cpu_idle < (ULLONG_MAX - 0x7ffff))) {
		scp->cpu_idle = 0;
	}

	/*
	 * Don't take cpu_guest and cpu_guest_nice into account
	 * because cpu_user and cpu_nice already include them.
	 */
	return ((scc->cpu_user    + scc->cpu_nice   +
		 scc->cpu_sys     + scc->cpu_iowait +
		 scc->cpu_idle    + scc->cpu_steal  +
		 scc->cpu_hardirq + scc->cpu_softirq) -
		(scp->cpu_user    + scp->cpu_nice   +
		 scp->cpu_sys     + scp->cpu_iowait +
		 scp->cpu_idle    + scp->cpu_steal  +
		 scp->cpu_hardirq + scp->cpu_softirq) +
		 ishift);
}

#ifdef SOURCE_SADC
/*---------------- BEGIN: FUNCTIONS USED BY SADC ONLY ---------------------*/

/*
 ***************************************************************************
 * Replace octal codes in string with their corresponding characters.
 *
 * IN:
 * @str		String to parse.
 *
 * OUT:
 * @str		String with octal codes replaced with characters.
 ***************************************************************************
 */
void oct2chr(char *str)
{
	int i = 0;
	int j, len;

	len = strlen(str);

	while (i < len - 3) {
		if ((str[i] == '\\') &&
		    (str[i + 1] >= '0') && (str[i + 1] <= '3') &&
		    (str[i + 2] >= '0') && (str[i + 2] <= '7') &&
		    (str[i + 3] >= '0') && (str[i + 3] <= '7')) {
			/* Octal code found */
			str[i] = (str[i + 1] - 48) * 64 +
			         (str[i + 2] - 48) * 8  +
			         (str[i + 3] - 48);
			for (j = i + 4; j <= len; j++) {
				str[j - 3] = str[j];
			}
			len -= 3;
		}
		i++;
	}
}

/*
 ***************************************************************************
 * Read processes (tasks) creation and context switches statistics
 * from /proc/stat.
 *
 * IN:
 * @st_pcsw	Structure where stats will be saved.
 *
 * OUT:
 * @st_pcsw	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_stat_pcsw(struct stats_pcsw *st_pcsw)
{
	FILE *fp;
	char line[8192];

	if ((fp = fopen(STAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "ctxt ", 5)) {
			/* Read number of context switches */
			sscanf(line + 5, "%llu", &st_pcsw->context_switch);
		}

		else if (!strncmp(line, "processes ", 10)) {
			/* Read number of processes created since system boot */
			sscanf(line + 10, "%lu", &st_pcsw->processes);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read queue and load statistics from /proc/loadavg and /proc/stat.
 *
 * IN:
 * @st_queue	Structure where stats will be saved.
 *
 * OUT:
 * @st_queue	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_loadavg(struct stats_queue *st_queue)
{
	FILE *fp;
	char line[8192];
	unsigned int load_tmp[3];
	int rc;

	if ((fp = fopen(LOADAVG, "r")) == NULL)
		return 0;

	/* Read load averages and queue length */
	rc = fscanf(fp, "%u.%u %u.%u %u.%u %llu/%llu %*d\n",
		    &load_tmp[0], &st_queue->load_avg_1,
		    &load_tmp[1], &st_queue->load_avg_5,
		    &load_tmp[2], &st_queue->load_avg_15,
		    &st_queue->nr_running,
		    &st_queue->nr_threads);

	fclose(fp);

	if (rc < 8)
		return 0;

	st_queue->load_avg_1  += load_tmp[0] * 100;
	st_queue->load_avg_5  += load_tmp[1] * 100;
	st_queue->load_avg_15 += load_tmp[2] * 100;

	if (st_queue->nr_running) {
		/* Do not take current process into account */
		st_queue->nr_running--;
	}

	/* Read nr of tasks blocked from /proc/stat */
	if ((fp = fopen(STAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "procs_blocked ", 14)) {
			/* Read number of processes blocked */
			sscanf(line + 14, "%llu", &st_queue->procs_blocked);
			break;
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read swapping statistics from /proc/vmstat.
 *
 * IN:
 * @st_swap	Structure where stats will be saved.
 *
 * OUT:
 * @st_swap	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_vmstat_swap(struct stats_swap *st_swap)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(VMSTAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "pswpin ", 7)) {
			/* Read number of swap pages brought in */
			sscanf(line + 7, "%lu", &st_swap->pswpin);
		}
		else if (!strncmp(line, "pswpout ", 8)) {
			/* Read number of swap pages brought out */
			sscanf(line + 8, "%lu", &st_swap->pswpout);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read paging statistics from /proc/vmstat.
 *
 * IN:
 * @st_paging	Structure where stats will be saved.
 *
 * OUT:
 * @st_paging	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_vmstat_paging(struct stats_paging *st_paging)
{
	FILE *fp;
	char line[128];
	unsigned long pgtmp;

	if ((fp = fopen(VMSTAT, "r")) == NULL)
		return 0;

	st_paging->pgsteal = 0;
	st_paging->pgscan_kswapd = st_paging->pgscan_direct = 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "pgpgin ", 7)) {
			/* Read number of pages the system paged in */
			sscanf(line + 7, "%lu", &st_paging->pgpgin);
		}
		else if (!strncmp(line, "pgpgout ", 8)) {
			/* Read number of pages the system paged out */
			sscanf(line + 8, "%lu", &st_paging->pgpgout);
		}
		else if (!strncmp(line, "pgfault ", 8)) {
			/* Read number of faults (major+minor) made by the system */
			sscanf(line + 8, "%lu", &st_paging->pgfault);
		}
		else if (!strncmp(line, "pgmajfault ", 11)) {
			/* Read number of faults (major only) made by the system */
			sscanf(line + 11, "%lu", &st_paging->pgmajfault);
		}
		else if (!strncmp(line, "pgfree ", 7)) {
			/* Read number of pages freed by the system */
			sscanf(line + 7, "%lu", &st_paging->pgfree);
		}
		else if (!strncmp(line, "pgsteal_", 8)) {
			/* Read number of pages stolen by the system */
			sscanf(strchr(line, ' '), "%lu", &pgtmp);
			st_paging->pgsteal += pgtmp;
		}
		else if (!strncmp(line, "pgscan_kswapd", 13)) {
			/* Read number of pages scanned by the kswapd daemon */
			sscanf(strchr(line, ' '), "%lu", &pgtmp);
			st_paging->pgscan_kswapd += pgtmp;
		}
		else if (!strncmp(line, "pgscan_direct", 13)) {
			/* Read number of pages scanned directly */
			sscanf(strchr(line, ' '), "%lu", &pgtmp);
			st_paging->pgscan_direct += pgtmp;
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read I/O and transfer rates statistics from /proc/diskstats.
 *
 * IN:
 * @st_io	Structure where stats will be saved.
 *
 * OUT:
 * @st_io	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_diskstats_io(struct stats_io *st_io)
{
	FILE *fp;
	char line[1024];
	char dev_name[MAX_NAME_LEN];
	unsigned int major, minor;
	unsigned long rd_ios, wr_ios, dc_ios;
	unsigned long rd_sec, wr_sec, dc_sec;

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		/* Discard I/O stats may be not available */
		dc_ios = dc_sec = 0;

		if (sscanf(line,
			   "%u %u %s "
			   "%lu %*u %lu %*u "
			   "%lu %*u %lu %*u "
			   "%*u %*u %*u "
			   "%lu %*u %lu",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_sec,
			   &wr_ios, &wr_sec,
			   &dc_ios, &dc_sec) >= 7) {

			if (is_device(SLASH_SYS, dev_name, IGNORE_VIRTUAL_DEVICES)) {
				/*
				 * OK: It's a (real) device and not a partition.
				 * Note: Structure should have been initialized first!
				 */
				st_io->dk_drive      += (unsigned long long) rd_ios +
							(unsigned long long) wr_ios +
							(unsigned long long) dc_ios;
				st_io->dk_drive_rio  += rd_ios;
				st_io->dk_drive_rblk += rd_sec;
				st_io->dk_drive_wio  += wr_ios;
				st_io->dk_drive_wblk += wr_sec;
				st_io->dk_drive_dio  += dc_ios;
				st_io->dk_drive_dblk += dc_sec;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read block devices statistics from /proc/diskstats.
 *
 * IN:
 * @st_disk	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 1.
 * @read_part	True if disks *and* partitions should be read; False if only
 * 		disks are read.
 *
 * OUT:
 * @st_disk	Structure with statistics.
 *
 * RETURNS:
 * Number of block devices read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_diskstats_disk(struct stats_disk *st_disk, __nr_t nr_alloc,
			   int read_part)
{
	FILE *fp;
	char line[1024];
	char dev_name[MAX_NAME_LEN];
	struct stats_disk *st_disk_i;
	unsigned int major, minor, rd_ticks, wr_ticks, dc_ticks, tot_ticks, rq_ticks, part_nr;
	unsigned long rd_ios, wr_ios, dc_ios, rd_sec, wr_sec, dc_sec;
	unsigned long long wwn[2];
	__nr_t dsk_read = 0;

	if ((fp = fopen(DISKSTATS, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		/* Discard I/O stats may be not available */
		dc_ios = dc_sec = dc_ticks = 0;

		if (sscanf(line,
			   "%u %u %s "
			   "%lu %*u %lu %u "
			   "%lu %*u %lu %u "
			   "%*u %u %u "
			   "%lu %*u %lu %u",
			   &major, &minor, dev_name,
			   &rd_ios, &rd_sec, &rd_ticks,
			   &wr_ios, &wr_sec, &wr_ticks,
			   &tot_ticks, &rq_ticks,
			   &dc_ios, &dc_sec, &dc_ticks) >= 11) {

			if (!rd_ios && !wr_ios && !dc_ios)
				/* Unused device: Ignore it */
				continue;
			if (read_part || is_device(SLASH_SYS, dev_name, ACCEPT_VIRTUAL_DEVICES)) {

				if (dsk_read + 1 > nr_alloc) {
					dsk_read = -1;
					break;
				}

				st_disk_i = st_disk + dsk_read++;
				st_disk_i->major     = major;
				st_disk_i->minor     = minor;
				st_disk_i->nr_ios    = (unsigned long long) rd_ios +
						       (unsigned long long) wr_ios +
						       (unsigned long long) dc_ios;
				st_disk_i->rd_sect   = rd_sec;
				st_disk_i->wr_sect   = wr_sec;
				st_disk_i->dc_sect   = dc_sec;
				st_disk_i->rd_ticks  = rd_ticks;
				st_disk_i->wr_ticks  = wr_ticks;
				st_disk_i->dc_ticks  = dc_ticks;
				st_disk_i->tot_ticks = tot_ticks;
				st_disk_i->rq_ticks  = rq_ticks;

				if (get_wwnid_from_pretty(dev_name, wwn, &part_nr) < 0) {
					st_disk_i->wwn[0] = 0ULL;
				}
				else {
					st_disk_i->wwn[0] = wwn[0];
					st_disk_i->wwn[1] = wwn[1];
					st_disk_i->part_nr = part_nr;
				}
			}
		}
	}

	fclose(fp);
	return dsk_read;
}

/*
 ***************************************************************************
 * Read serial lines statistics from /proc/tty/driver/serial.
 *
 * IN:
 * @st_serial	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_serial	Structure with statistics.
 *
 * RETURNS:
 * Number of serial lines read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_tty_driver_serial(struct stats_serial *st_serial, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_serial *st_serial_i;
	char line[256];
	char *p;
	__nr_t sl_read = 0;

	if ((fp = fopen(SERIAL, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL ) {

		if ((p = strstr(line, "tx:")) != NULL) {

			if (sl_read + 1 > nr_alloc) {
				sl_read = -1;
				break;
			}

			st_serial_i = st_serial + sl_read++;
			/* Read serial line number */
			sscanf(line, "%u", &st_serial_i->line);
			/*
			 * Read the number of chars transmitted and received by
			 * current serial line.
			 */
			sscanf(p + 3, "%u", &st_serial_i->tx);
			if ((p = strstr(line, "rx:")) != NULL) {
				sscanf(p + 3, "%u", &st_serial_i->rx);
			}
			if ((p = strstr(line, "fe:")) != NULL) {
				sscanf(p + 3, "%u", &st_serial_i->frame);
			}
			if ((p = strstr(line, "pe:")) != NULL) {
				sscanf(p + 3, "%u", &st_serial_i->parity);
			}
			if ((p = strstr(line, "brk:")) != NULL) {
				sscanf(p + 4, "%u", &st_serial_i->brk);
			}
			if ((p = strstr(line, "oe:")) != NULL) {
				sscanf(p + 3, "%u", &st_serial_i->overrun);
			}
		}
	}

	fclose(fp);
	return sl_read;
}

/*
 ***************************************************************************
 * Read kernel tables statistics from various system files.
 *
 * IN:
 * @st_ktables	Structure where stats will be saved.
 *
 * OUT:
 * @st_ktables	Structure with statistics.
 *
 * RETURNS:
 * 1 (always success).
 ***************************************************************************
 */
__nr_t read_kernel_tables(struct stats_ktables *st_ktables)
{
	FILE *fp;
	unsigned long long parm;
	int rc = 0;

	/* Open /proc/sys/fs/dentry-state file */
	if ((fp = fopen(FDENTRY_STATE, "r")) != NULL) {
		rc = fscanf(fp, "%*d %llu",
			    &st_ktables->dentry_stat);
		fclose(fp);
		if (rc == 0) {
			st_ktables->dentry_stat = 0;
		}
	}

	/* Open /proc/sys/fs/file-nr file */
	if ((fp = fopen(FFILE_NR, "r")) != NULL) {
		rc = fscanf(fp, "%llu %llu",
			    &st_ktables->file_used, &parm);
		fclose(fp);
		/*
		 * The number of used handles is the number of allocated ones
		 * minus the number of free ones.
		 */
		if (rc == 2) {
			st_ktables->file_used -= parm;
		}
		else {
			st_ktables->file_used = 0;
		}
	}

	/* Open /proc/sys/fs/inode-state file */
	if ((fp = fopen(FINODE_STATE, "r")) != NULL) {
		rc = fscanf(fp, "%llu %llu",
			    &st_ktables->inode_used, &parm);
		fclose(fp);
		/*
		 * The number of inuse inodes is the number of allocated ones
		 * minus the number of free ones.
		 */
		if (rc == 2) {
			st_ktables->inode_used -= parm;
		}
		else {
			st_ktables->inode_used = 0;
		}
	}

	/* Open /proc/sys/kernel/pty/nr file */
	if ((fp = fopen(PTY_NR, "r")) != NULL) {
		rc = fscanf(fp, "%llu",
			    &st_ktables->pty_nr);
		fclose(fp);
		if (rc == 0) {
			st_ktables->pty_nr = 0;
		}
	}

	return 1;
}

/*
 ***************************************************************************
 * Read network interfaces statistics from /proc/net/dev.
 *
 * IN:
 * @st_net_dev	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_net_dev	Structure with statistics.
 *
 * RETURNS:
 * Number of interfaces read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_net_dev(struct stats_net_dev *st_net_dev, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_net_dev *st_net_dev_i;
	char line[256];
	char iface[MAX_IFACE_LEN];
	__nr_t dev_read = 0;
	int pos;

	if ((fp = fopen(NET_DEV, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		pos = strcspn(line, ":");
		if (pos < strlen(line)) {

			if (dev_read + 1 > nr_alloc) {
				dev_read = -1;
				break;
			}

			st_net_dev_i = st_net_dev + dev_read++;
			strncpy(iface, line, MINIMUM(pos, sizeof(iface) - 1));
			iface[MINIMUM(pos, sizeof(iface) - 1)] = '\0';
			sscanf(iface, "%s", st_net_dev_i->interface); /* Skip heading spaces */
			sscanf(line + pos + 1, "%llu %llu %*u %*u %*u %*u %llu %llu %llu %llu "
			       "%*u %*u %*u %*u %*u %llu",
			       &st_net_dev_i->rx_bytes,
			       &st_net_dev_i->rx_packets,
			       &st_net_dev_i->rx_compressed,
			       &st_net_dev_i->multicast,
			       &st_net_dev_i->tx_bytes,
			       &st_net_dev_i->tx_packets,
			       &st_net_dev_i->tx_compressed);
		}
	}

	fclose(fp);
	return dev_read;
}

/*
 ***************************************************************************
 * Read duplex and speed data for network interface cards.
 *
 * IN:
 * @st_net_dev	Structure where stats will be saved.
 * @nbr		Number of network interfaces to read.
 *
 * OUT:
 * @st_net_dev	Structure with statistics.
 ***************************************************************************
 */
void read_if_info(struct stats_net_dev *st_net_dev, int nbr)
{
	FILE *fp;
	struct stats_net_dev *st_net_dev_i;
	char filename[128], duplex[32];
	int dev, n;

	for (dev = 0; dev < nbr; dev++) {

		st_net_dev_i = st_net_dev + dev;

		/* Read speed info */
		sprintf(filename, IF_DUPLEX, st_net_dev_i->interface);

		if ((fp = fopen(filename, "r")) == NULL)
			/* Cannot read NIC duplex */
			continue;

		n = fscanf(fp, "%31s", duplex);

		fclose(fp);

		if (n != 1)
			/* Cannot read NIC duplex */
			continue;

		if (!strcmp(duplex, K_DUPLEX_FULL)) {
			st_net_dev_i->duplex = C_DUPLEX_FULL;
		}
		else if (!strcmp(duplex, K_DUPLEX_HALF)) {
			st_net_dev_i->duplex = C_DUPLEX_HALF;
		}
		else
			continue;

		/* Read speed info */
		sprintf(filename, IF_SPEED, st_net_dev_i->interface);

		if ((fp = fopen(filename, "r")) == NULL)
			/* Cannot read NIC speed */
			continue;

		n = fscanf(fp, "%u", &st_net_dev_i->speed);

		fclose(fp);

		if (n != 1) {
			st_net_dev_i->speed = 0;
		}
	}
}


/*
 ***************************************************************************
 * Read network interfaces errors statistics from /proc/net/dev.
 *
 * IN:
 * @st_net_edev	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_net_edev	Structure with statistics.
 *
 * RETURNS:
 * Number of interfaces read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_net_edev(struct stats_net_edev *st_net_edev, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_net_edev *st_net_edev_i;
	static char line[256];
	char iface[MAX_IFACE_LEN];
	__nr_t dev_read = 0;
	int pos;

	if ((fp = fopen(NET_DEV, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		pos = strcspn(line, ":");
		if (pos < strlen(line)) {

			if (dev_read + 1 > nr_alloc) {
				dev_read = -1;
				break;
			}

			st_net_edev_i = st_net_edev + dev_read++;
			strncpy(iface, line, MINIMUM(pos, sizeof(iface) - 1));
			iface[MINIMUM(pos, sizeof(iface) - 1)] = '\0';
			sscanf(iface, "%s", st_net_edev_i->interface); /* Skip heading spaces */
			sscanf(line + pos + 1, "%*u %*u %llu %llu %llu %llu %*u %*u %*u %*u "
			       "%llu %llu %llu %llu %llu",
			       &st_net_edev_i->rx_errors,
			       &st_net_edev_i->rx_dropped,
			       &st_net_edev_i->rx_fifo_errors,
			       &st_net_edev_i->rx_frame_errors,
			       &st_net_edev_i->tx_errors,
			       &st_net_edev_i->tx_dropped,
			       &st_net_edev_i->tx_fifo_errors,
			       &st_net_edev_i->collisions,
			       &st_net_edev_i->tx_carrier_errors);
		}
	}

	fclose(fp);
	return dev_read;
}

/*
 ***************************************************************************
 * Read NFS client statistics from /proc/net/rpc/nfs.
 *
 * IN:
 * @st_net_nfs	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_nfs	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_nfs(struct stats_net_nfs *st_net_nfs)
{
	FILE *fp;
	char line[256];
	unsigned int getattcnt = 0, accesscnt = 0, readcnt = 0, writecnt = 0;

	if ((fp = fopen(NET_RPC_NFS, "r")) == NULL)
		return 0;

	memset(st_net_nfs, 0, STATS_NET_NFS_SIZE);

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "rpc ", 4)) {
			sscanf(line + 4, "%u %u",
			       &st_net_nfs->nfs_rpccnt, &st_net_nfs->nfs_rpcretrans);
		}
		else if (!strncmp(line, "proc3 ", 6)) {
			sscanf(line + 6, "%*u %*u %u %*u %*u %u %*u %u %u",
			       &getattcnt, &accesscnt, &readcnt, &writecnt);

			st_net_nfs->nfs_getattcnt += getattcnt;
			st_net_nfs->nfs_accesscnt += accesscnt;
			st_net_nfs->nfs_readcnt   += readcnt;
			st_net_nfs->nfs_writecnt  += writecnt;
		}
		else if (!strncmp(line, "proc4 ", 6)) {
			sscanf(line + 6, "%*u %*u %u %u "
			       "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %u %u",
			       &readcnt, &writecnt, &accesscnt, &getattcnt);

			st_net_nfs->nfs_getattcnt += getattcnt;
			st_net_nfs->nfs_accesscnt += accesscnt;
			st_net_nfs->nfs_readcnt   += readcnt;
			st_net_nfs->nfs_writecnt  += writecnt;
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read NFS server statistics from /proc/net/rpc/nfsd.
 *
 * IN:
 * @st_net_nfsd	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_nfsd	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_nfsd(struct stats_net_nfsd *st_net_nfsd)
{
	FILE *fp;
	char line[256];
	unsigned int getattcnt = 0, accesscnt = 0, readcnt = 0, writecnt = 0;

	if ((fp = fopen(NET_RPC_NFSD, "r")) == NULL)
		return 0;

	memset(st_net_nfsd, 0, STATS_NET_NFSD_SIZE);

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "rc ", 3)) {
			sscanf(line + 3, "%u %u",
			       &st_net_nfsd->nfsd_rchits, &st_net_nfsd->nfsd_rcmisses);
		}
		else if (!strncmp(line, "net ", 4)) {
			sscanf(line + 4, "%u %u %u",
			       &st_net_nfsd->nfsd_netcnt, &st_net_nfsd->nfsd_netudpcnt,
			       &st_net_nfsd->nfsd_nettcpcnt);
		}
		else if (!strncmp(line, "rpc ", 4)) {
			sscanf(line + 4, "%u %u",
			       &st_net_nfsd->nfsd_rpccnt, &st_net_nfsd->nfsd_rpcbad);
		}
		else if (!strncmp(line, "proc3 ", 6)) {
			sscanf(line + 6, "%*u %*u %u %*u %*u %u %*u %u %u",
			       &getattcnt, &accesscnt, &readcnt, &writecnt);

			st_net_nfsd->nfsd_getattcnt += getattcnt;
			st_net_nfsd->nfsd_accesscnt += accesscnt;
			st_net_nfsd->nfsd_readcnt   += readcnt;
			st_net_nfsd->nfsd_writecnt  += writecnt;

		}
		else if (!strncmp(line, "proc4ops ", 9)) {
			sscanf(line + 9, "%*u %*u %*u %*u %u "
			       "%*u %*u %*u %*u %*u %u "
			       "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %u "
			       "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %u",
			       &accesscnt, &getattcnt, &readcnt, &writecnt);

			st_net_nfsd->nfsd_getattcnt += getattcnt;
			st_net_nfsd->nfsd_accesscnt += accesscnt;
			st_net_nfsd->nfsd_readcnt   += readcnt;
			st_net_nfsd->nfsd_writecnt  += writecnt;
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read network sockets statistics from /proc/net/sockstat.
 *
 * IN:
 * @st_net_sock	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_sock	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_sock(struct stats_net_sock *st_net_sock)
{
	FILE *fp;
	char line[96];
	char *p;

	if ((fp = fopen(NET_SOCKSTAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "sockets:", 8)) {
			/* Sockets */
			sscanf(line + 14, "%u", &st_net_sock->sock_inuse);
		}
		else if (!strncmp(line, "TCP:", 4)) {
			/* TCP sockets */
			sscanf(line + 11, "%u", &st_net_sock->tcp_inuse);
			if ((p = strstr(line, "tw")) != NULL) {
				sscanf(p + 2, "%u", &st_net_sock->tcp_tw);
			}
		}
		else if (!strncmp(line, "UDP:", 4)) {
			/* UDP sockets */
			sscanf(line + 11, "%u", &st_net_sock->udp_inuse);
		}
		else if (!strncmp(line, "RAW:", 4)) {
			/* RAW sockets */
			sscanf(line + 11, "%u", &st_net_sock->raw_inuse);
		}
		else if (!strncmp(line, "FRAG:", 5)) {
			/* FRAGments */
			sscanf(line + 12, "%u", &st_net_sock->frag_inuse);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read IP network traffic statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_ip	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_ip	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_ip(struct stats_net_ip *st_net_ip)
{
	FILE *fp;
	char line[1024];
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Ip:", 3)) {
			if (sw) {
				sscanf(line + 3, "%*u %*u %llu %*u %*u %llu %*u %*u "
				       "%llu %llu %*u %*u %*u %llu %llu %*u %llu %*u %llu",
				       &st_net_ip->InReceives,
				       &st_net_ip->ForwDatagrams,
				       &st_net_ip->InDelivers,
				       &st_net_ip->OutRequests,
				       &st_net_ip->ReasmReqds,
				       &st_net_ip->ReasmOKs,
				       &st_net_ip->FragOKs,
				       &st_net_ip->FragCreates);

				break;
			}
			else {
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read IP network errors statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_eip	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_eip	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_eip(struct stats_net_eip *st_net_eip)
{
	FILE *fp;
	char line[1024];
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Ip:", 3)) {
			if (sw) {
				sscanf(line + 3, "%*u %*u %*u %llu %llu %*u %llu %llu "
				       "%*u %*u %llu %llu %*u %*u %*u %llu %*u %llu",
				       &st_net_eip->InHdrErrors,
				       &st_net_eip->InAddrErrors,
				       &st_net_eip->InUnknownProtos,
				       &st_net_eip->InDiscards,
				       &st_net_eip->OutDiscards,
				       &st_net_eip->OutNoRoutes,
				       &st_net_eip->ReasmFails,
				       &st_net_eip->FragFails);

				break;
			}
			else {
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read ICMP network traffic statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_icmp	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_icmp	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_icmp(struct stats_net_icmp *st_net_icmp)
{
	FILE *fp;
	char line[1024];
	static char format[256] = "";
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Icmp:", 5)) {
			if (sw) {
				sscanf(line + 5, format,
				       &st_net_icmp->InMsgs,
				       &st_net_icmp->InEchos,
				       &st_net_icmp->InEchoReps,
				       &st_net_icmp->InTimestamps,
				       &st_net_icmp->InTimestampReps,
				       &st_net_icmp->InAddrMasks,
				       &st_net_icmp->InAddrMaskReps,
				       &st_net_icmp->OutMsgs,
				       &st_net_icmp->OutEchos,
				       &st_net_icmp->OutEchoReps,
				       &st_net_icmp->OutTimestamps,
				       &st_net_icmp->OutTimestampReps,
				       &st_net_icmp->OutAddrMasks,
				       &st_net_icmp->OutAddrMaskReps);

				break;
			}
			else {
				if (!strlen(format)) {
					if (strstr(line, "InCsumErrors")) {
						/*
						 * New format: InCsumErrors field exists at position #3.
						 * Capture: 1,9,10,11,12,13,14,15,22,23,24,25,26,27.
						 */
						strcpy(format, "%lu %*u %*u %*u %*u %*u %*u %*u "
							       "%lu %lu %lu %lu %lu %lu %lu %*u %*u %*u %*u "
							       "%*u %*u %lu %lu %lu %lu %lu %lu");
					}
					else {
						/*
						 * Old format: InCsumErrors field doesn't exist.
						 * Capture: 1,8,9,10,11,12,13,14,21,22,23,24,25,26.
						 */
						strcpy(format, "%lu %*u %*u %*u %*u %*u %*u "
							       "%lu %lu %lu %lu %lu %lu %lu %*u %*u %*u %*u "
							       "%*u %*u %lu %lu %lu %lu %lu %lu");
					}
				}
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read ICMP network errors statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_eicmp	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_eicmp	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_eicmp(struct stats_net_eicmp *st_net_eicmp)
{
	FILE *fp;
	char line[1024];
	static char format[256] = "";
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Icmp:", 5)) {
			if (sw) {
				sscanf(line + 5, format,
				       &st_net_eicmp->InErrors,
				       &st_net_eicmp->InDestUnreachs,
				       &st_net_eicmp->InTimeExcds,
				       &st_net_eicmp->InParmProbs,
				       &st_net_eicmp->InSrcQuenchs,
				       &st_net_eicmp->InRedirects,
				       &st_net_eicmp->OutErrors,
				       &st_net_eicmp->OutDestUnreachs,
				       &st_net_eicmp->OutTimeExcds,
				       &st_net_eicmp->OutParmProbs,
				       &st_net_eicmp->OutSrcQuenchs,
				       &st_net_eicmp->OutRedirects);

				break;
			}
			else {
				if (!strlen(format)) {
					if (strstr(line, "InCsumErrors")) {
						/*
						 * New format: InCsumErrors field exists at position #3.
						 * Capture: 2,4,5,6,7,8,16,17,18,19,20,21
						 */
						strcpy(format, "%*u %lu %*u %lu %lu %lu %lu %lu %*u %*u "
							       "%*u %*u %*u %*u %*u %lu %lu %lu %lu %lu %lu");
					}
					else {
						/*
						 * Old format: InCsumErrors field doesn't exist.
						 * Capture: 2,3,4,5,6,7,15,16,17,18,19,20
						 */
						strcpy(format, "%*u %lu %lu %lu %lu %lu %lu %*u %*u "
							       "%*u %*u %*u %*u %*u %lu %lu %lu %lu %lu %lu");

					}
				}
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read TCP network traffic statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_tcp	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_tcp	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_tcp(struct stats_net_tcp *st_net_tcp)
{
	FILE *fp;
	char line[1024];
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Tcp:", 4)) {
			if (sw) {
				sscanf(line + 4, "%*u %*u %*u %*d %lu %lu "
				       "%*u %*u %*u %lu %lu",
				       &st_net_tcp->ActiveOpens,
				       &st_net_tcp->PassiveOpens,
				       &st_net_tcp->InSegs,
				       &st_net_tcp->OutSegs);

				break;
			}
			else {
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read TCP network errors statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_etcp	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_etcp	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_etcp(struct stats_net_etcp *st_net_etcp)
{
	FILE *fp;
	char line[1024];
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Tcp:", 4)) {
			if (sw) {
				sscanf(line + 4, "%*u %*u %*u %*d %*u %*u "
				       "%lu %lu %*u %*u %*u %lu %lu %lu",
				       &st_net_etcp->AttemptFails,
				       &st_net_etcp->EstabResets,
				       &st_net_etcp->RetransSegs,
				       &st_net_etcp->InErrs,
				       &st_net_etcp->OutRsts);

				break;
			}
			else {
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read UDP network traffic statistics from /proc/net/snmp.
 *
 * IN:
 * @st_net_udp	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_udp	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_udp(struct stats_net_udp *st_net_udp)
{
	FILE *fp;
	char line[1024];
	int sw = FALSE;

	if ((fp = fopen(NET_SNMP, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Udp:", 4)) {
			if (sw) {
				sscanf(line + 4, "%lu %lu %lu %lu",
				       &st_net_udp->InDatagrams,
				       &st_net_udp->NoPorts,
				       &st_net_udp->InErrors,
				       &st_net_udp->OutDatagrams);

				break;
			}
			else {
				sw = TRUE;
			}
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read IPv6 network sockets statistics from /proc/net/sockstat6.
 *
 * IN:
 * @st_net_sock6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_sock6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_sock6(struct stats_net_sock6 *st_net_sock6)
{
	FILE *fp;
	char line[96];

	if ((fp = fopen(NET_SOCKSTAT6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "TCP6:", 5)) {
			/* TCPv6 sockets */
			sscanf(line + 12, "%u", &st_net_sock6->tcp6_inuse);
		}
		else if (!strncmp(line, "UDP6:", 5)) {
			/* UDPv6 sockets */
			sscanf(line + 12, "%u", &st_net_sock6->udp6_inuse);
		}
		else if (!strncmp(line, "RAW6:", 5)) {
			/* IPv6 RAW sockets */
			sscanf(line + 12, "%u", &st_net_sock6->raw6_inuse);
		}
		else if (!strncmp(line, "FRAG6:", 6)) {
			/* IPv6 FRAGments */
			sscanf(line + 13, "%u", &st_net_sock6->frag6_inuse);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read IPv6 network traffic statistics from /proc/net/snmp6.
 *
 * IN:
 * @st_net_ip6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_ip6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_ip6(struct stats_net_ip6 *st_net_ip6)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(NET_SNMP6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Ip6InReceives ", 14)) {
			sscanf(line + 14, "%llu", &st_net_ip6->InReceives6);
		}
		else if (!strncmp(line, "Ip6OutForwDatagrams ", 20)) {
			sscanf(line + 20, "%llu", &st_net_ip6->OutForwDatagrams6);
		}
		else if (!strncmp(line, "Ip6InDelivers ", 14)) {
			sscanf(line + 14, "%llu", &st_net_ip6->InDelivers6);
		}
		else if (!strncmp(line, "Ip6OutRequests ", 15)) {
			sscanf(line + 15, "%llu", &st_net_ip6->OutRequests6);
		}
		else if (!strncmp(line, "Ip6ReasmReqds ", 14)) {
			sscanf(line + 14, "%llu", &st_net_ip6->ReasmReqds6);
		}
		else if (!strncmp(line, "Ip6ReasmOKs ", 12)) {
			sscanf(line + 12, "%llu", &st_net_ip6->ReasmOKs6);
		}
		else if (!strncmp(line, "Ip6InMcastPkts ", 15)) {
			sscanf(line + 15, "%llu", &st_net_ip6->InMcastPkts6);
		}
		else if (!strncmp(line, "Ip6OutMcastPkts ", 16)) {
			sscanf(line + 16, "%llu", &st_net_ip6->OutMcastPkts6);
		}
		else if (!strncmp(line, "Ip6FragOKs ", 11)) {
			sscanf(line + 11, "%llu", &st_net_ip6->FragOKs6);
		}
		else if (!strncmp(line, "Ip6FragCreates ", 15)) {
			sscanf(line + 15, "%llu", &st_net_ip6->FragCreates6);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read IPv6 network errors statistics from /proc/net/snmp6.
 *
 * IN:
 * @st_net_eip6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_eip6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_eip6(struct stats_net_eip6 *st_net_eip6)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(NET_SNMP6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Ip6InHdrErrors ", 15)) {
			sscanf(line + 15, "%llu", &st_net_eip6->InHdrErrors6);
		}
		else if (!strncmp(line, "Ip6InAddrErrors ", 16)) {
			sscanf(line + 16, "%llu", &st_net_eip6->InAddrErrors6);
		}
		else if (!strncmp(line, "Ip6InUnknownProtos ", 19)) {
			sscanf(line + 19, "%llu", &st_net_eip6->InUnknownProtos6);
		}
		else if (!strncmp(line, "Ip6InTooBigErrors ", 18)) {
			sscanf(line + 18, "%llu", &st_net_eip6->InTooBigErrors6);
		}
		else if (!strncmp(line, "Ip6InDiscards ", 14)) {
			sscanf(line + 14, "%llu", &st_net_eip6->InDiscards6);
		}
		else if (!strncmp(line, "Ip6OutDiscards ", 15)) {
			sscanf(line + 15, "%llu", &st_net_eip6->OutDiscards6);
		}
		else if (!strncmp(line, "Ip6InNoRoutes ", 14)) {
			sscanf(line + 14, "%llu", &st_net_eip6->InNoRoutes6);
		}
		else if (!strncmp(line, "Ip6OutNoRoutes ", 15)) {
			sscanf(line + 15, "%llu", &st_net_eip6->OutNoRoutes6);
		}
		else if (!strncmp(line, "Ip6ReasmFails ", 14)) {
			sscanf(line + 14, "%llu", &st_net_eip6->ReasmFails6);
		}
		else if (!strncmp(line, "Ip6FragFails ", 13)) {
			sscanf(line + 13, "%llu", &st_net_eip6->FragFails6);
		}
		else if (!strncmp(line, "Ip6InTruncatedPkts ", 19)) {
			sscanf(line + 19, "%llu", &st_net_eip6->InTruncatedPkts6);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read ICMPv6 network traffic statistics from /proc/net/snmp6.
 *
 * IN:
 * @st_net_icmp6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_icmp6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_icmp6(struct stats_net_icmp6 *st_net_icmp6)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(NET_SNMP6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Icmp6InMsgs ", 12)) {
			sscanf(line + 12, "%lu", &st_net_icmp6->InMsgs6);
		}
		else if (!strncmp(line, "Icmp6OutMsgs ", 13)) {
			sscanf(line + 13, "%lu", &st_net_icmp6->OutMsgs6);
		}
		else if (!strncmp(line, "Icmp6InEchos ", 13)) {
			sscanf(line + 13, "%lu", &st_net_icmp6->InEchos6);
		}
		else if (!strncmp(line, "Icmp6InEchoReplies ", 19)) {
			sscanf(line + 19, "%lu", &st_net_icmp6->InEchoReplies6);
		}
		else if (!strncmp(line, "Icmp6OutEchoReplies ", 20)) {
			sscanf(line + 20, "%lu", &st_net_icmp6->OutEchoReplies6);
		}
		else if (!strncmp(line, "Icmp6InGroupMembQueries ", 24)) {
			sscanf(line + 24, "%lu", &st_net_icmp6->InGroupMembQueries6);
		}
		else if (!strncmp(line, "Icmp6InGroupMembResponses ", 26)) {
			sscanf(line + 26, "%lu", &st_net_icmp6->InGroupMembResponses6);
		}
		else if (!strncmp(line, "Icmp6OutGroupMembResponses ", 27)) {
			sscanf(line + 27, "%lu", &st_net_icmp6->OutGroupMembResponses6);
		}
		else if (!strncmp(line, "Icmp6InGroupMembReductions ", 27)) {
			sscanf(line + 27, "%lu", &st_net_icmp6->InGroupMembReductions6);
		}
		else if (!strncmp(line, "Icmp6OutGroupMembReductions ", 28)) {
			sscanf(line + 28, "%lu", &st_net_icmp6->OutGroupMembReductions6);
		}
		else if (!strncmp(line, "Icmp6InRouterSolicits ", 22)) {
			sscanf(line + 22, "%lu", &st_net_icmp6->InRouterSolicits6);
		}
		else if (!strncmp(line, "Icmp6OutRouterSolicits ", 23)) {
			sscanf(line + 23, "%lu", &st_net_icmp6->OutRouterSolicits6);
		}
		else if (!strncmp(line, "Icmp6InRouterAdvertisements ", 28)) {
			sscanf(line + 28, "%lu", &st_net_icmp6->InRouterAdvertisements6);
		}
		else if (!strncmp(line, "Icmp6InNeighborSolicits ", 24)) {
			sscanf(line + 24, "%lu", &st_net_icmp6->InNeighborSolicits6);
		}
		else if (!strncmp(line, "Icmp6OutNeighborSolicits ", 25)) {
			sscanf(line + 25, "%lu", &st_net_icmp6->OutNeighborSolicits6);
		}
		else if (!strncmp(line, "Icmp6InNeighborAdvertisements ", 30)) {
			sscanf(line + 30, "%lu", &st_net_icmp6->InNeighborAdvertisements6);
		}
		else if (!strncmp(line, "Icmp6OutNeighborAdvertisements ", 31)) {
			sscanf(line + 31, "%lu", &st_net_icmp6->OutNeighborAdvertisements6);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read ICMPv6 network errors statistics from /proc/net/snmp6.
 *
 * IN:
 * @st_net_eicmp6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_eicmp6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_eicmp6(struct stats_net_eicmp6 *st_net_eicmp6)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(NET_SNMP6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Icmp6InErrors ", 14)) {
			sscanf(line + 14, "%lu", &st_net_eicmp6->InErrors6);
		}
		else if (!strncmp(line, "Icmp6InDestUnreachs ", 20)) {
			sscanf(line + 20, "%lu", &st_net_eicmp6->InDestUnreachs6);
		}
		else if (!strncmp(line, "Icmp6OutDestUnreachs ", 21)) {
			sscanf(line + 21, "%lu", &st_net_eicmp6->OutDestUnreachs6);
		}
		else if (!strncmp(line, "Icmp6InTimeExcds ", 17)) {
			sscanf(line + 17, "%lu", &st_net_eicmp6->InTimeExcds6);
		}
		else if (!strncmp(line, "Icmp6OutTimeExcds ", 18)) {
			sscanf(line + 18, "%lu", &st_net_eicmp6->OutTimeExcds6);
		}
		else if (!strncmp(line, "Icmp6InParmProblems ", 20)) {
			sscanf(line + 20, "%lu", &st_net_eicmp6->InParmProblems6);
		}
		else if (!strncmp(line, "Icmp6OutParmProblems ", 21)) {
			sscanf(line + 21, "%lu", &st_net_eicmp6->OutParmProblems6);
		}
		else if (!strncmp(line, "Icmp6InRedirects ", 17)) {
			sscanf(line + 17, "%lu", &st_net_eicmp6->InRedirects6);
		}
		else if (!strncmp(line, "Icmp6OutRedirects ", 18)) {
			sscanf(line + 18, "%lu", &st_net_eicmp6->OutRedirects6);
		}
		else if (!strncmp(line, "Icmp6InPktTooBigs ", 18)) {
			sscanf(line + 18, "%lu", &st_net_eicmp6->InPktTooBigs6);
		}
		else if (!strncmp(line, "Icmp6OutPktTooBigs ", 19)) {
			sscanf(line + 19, "%lu", &st_net_eicmp6->OutPktTooBigs6);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read UDPv6 network traffic statistics from /proc/net/snmp6.
 *
 * IN:
 * @st_net_udp6	Structure where stats will be saved.
 *
 * OUT:
 * @st_net_udp6	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_net_udp6(struct stats_net_udp6 *st_net_udp6)
{
	FILE *fp;
	char line[128];

	if ((fp = fopen(NET_SNMP6, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "Udp6InDatagrams ", 16)) {
			sscanf(line + 16, "%lu", &st_net_udp6->InDatagrams6);
		}
		else if (!strncmp(line, "Udp6OutDatagrams ", 17)) {
			sscanf(line + 17, "%lu", &st_net_udp6->OutDatagrams6);
		}
		else if (!strncmp(line, "Udp6NoPorts ", 12)) {
			sscanf(line + 12, "%lu", &st_net_udp6->NoPorts6);
		}
		else if (!strncmp(line, "Udp6InErrors ", 13)) {
			sscanf(line + 13, "%lu", &st_net_udp6->InErrors6);
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read CPU frequency statistics.
 *
 * IN:
 * @st_pwr_cpufreq	Structure where stats will be saved.
 * @nr_alloc		Total number of structures allocated. Value is >= 1.
 *
 * OUT:
 * @st_pwr_cpufreq	Structure with statistics.
 *
 * RETURNS:
 * Highest CPU number for which statistics have been read.
 * 1 means CPU "all", 2 means CPU 0, 3 means CPU 1, etc.
 * Or -1 if the buffer was too small and needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_cpuinfo(struct stats_pwr_cpufreq *st_pwr_cpufreq, __nr_t nr_alloc)
{
	FILE *fp;
	struct stats_pwr_cpufreq *st_pwr_cpufreq_i;
	char line[1024];
	int nr = 0;
	__nr_t cpu_read = 1;	/* For CPU "all" */
	unsigned int proc_nr = 0, ifreq, dfreq;

	if ((fp = fopen(CPUINFO, "r")) == NULL)
		return 0;

	st_pwr_cpufreq->cpufreq = 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "processor\t", 10)) {
			sscanf(strchr(line, ':') + 1, "%u", &proc_nr);

			if (proc_nr + 2 > nr_alloc) {
				cpu_read = -1;
				break;
			}
		}

		/* Entry in /proc/cpuinfo is different between Intel and Power architectures */
		else if (!strncmp(line, "cpu MHz\t", 8) ||
			 !strncmp(line, "clock\t", 6)) {
			sscanf(strchr(line, ':') + 1, "%u.%u", &ifreq, &dfreq);

			/* Save current CPU frequency */
			st_pwr_cpufreq_i = st_pwr_cpufreq + proc_nr + 1;
			st_pwr_cpufreq_i->cpufreq = ifreq * 100 + dfreq / 10;

			/* Also save it to compute an average CPU frequency */
			st_pwr_cpufreq->cpufreq += st_pwr_cpufreq_i->cpufreq;
			nr++;

			if (proc_nr + 2 > cpu_read) {
				cpu_read = proc_nr + 2;
			}
		}
	}

	fclose(fp);

	if (nr) {
		/* Compute average CPU frequency for this machine */
		st_pwr_cpufreq->cpufreq /= nr;
	}
	return cpu_read;
}

/*
 ***************************************************************************
 * Read hugepages statistics from /proc/meminfo.
 *
 * IN:
 * @st_huge	Structure where stats will be saved.
 *
 * OUT:
 * @st_huge	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_meminfo_huge(struct stats_huge *st_huge)
{
	FILE *fp;
	char line[128];
	unsigned long szhkb = 0;

	if ((fp = fopen(MEMINFO, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "HugePages_Total:", 16)) {
			/* Read the total number of huge pages */
			sscanf(line + 16, "%llu", &st_huge->tlhkb);
		}
		else if (!strncmp(line, "HugePages_Free:", 15)) {
			/* Read the number of free huge pages */
			sscanf(line + 15, "%llu", &st_huge->frhkb);
		}
		else if (!strncmp(line, "HugePages_Rsvd:", 15)) {
			/* Read the number of reserved huge pages */
			sscanf(line + 15, "%llu", &st_huge->rsvdhkb);
		}
		else if (!strncmp(line, "HugePages_Surp:", 15)) {
			/* Read the number of surplus huge pages */
			sscanf(line + 15, "%llu", &st_huge->surphkb);
		}
		else if (!strncmp(line, "Hugepagesize:", 13)) {
			/* Read the default size of a huge page in kB */
			sscanf(line + 13, "%lu", &szhkb);
		}
	}

	fclose(fp);

	/* We want huge pages stats in kB and not expressed in a number of pages */
	st_huge->tlhkb *= szhkb;
	st_huge->frhkb *= szhkb;
	st_huge->rsvdhkb *= szhkb;
	st_huge->surphkb *= szhkb;

	return 1;
}

/*
 ***************************************************************************
 * Read CPU average frequencies statistics.
 *
 * IN:
 * @st_pwr_wghfreq	Structure where stats will be saved.
 * @cpu_nr		CPU number for which time_in_state date will be read.
 * @nbr			Total number of states (frequencies).
 *
 * OUT:
 * @st_pwr_wghfreq	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
int read_time_in_state(struct stats_pwr_wghfreq *st_pwr_wghfreq, int cpu_nr, int nbr)
{
	FILE *fp;
	struct stats_pwr_wghfreq *st_pwr_wghfreq_j;
	char filename[MAX_PF_NAME];
	char line[128];
	int j = 0;
	unsigned long freq;
	unsigned long long time_in_state;

	snprintf(filename, MAX_PF_NAME, "%s/cpu%d/%s",
		 SYSFS_DEVCPU, cpu_nr, SYSFS_TIME_IN_STATE);
	if ((fp = fopen(filename, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		sscanf(line, "%lu %llu", &freq, &time_in_state);

		if (j < nbr) {
			/* Save current frequency and time */
			st_pwr_wghfreq_j = st_pwr_wghfreq + j;
			st_pwr_wghfreq_j->freq = freq;
			st_pwr_wghfreq_j->time_in_state = time_in_state;
			j++;
		}
	}

	fclose(fp);
	return 1;
}

/*
 ***************************************************************************
 * Read weighted CPU frequency statistics.
 *
 * IN:
 * @st_pwr_wghfreq	Structure where stats will be saved.
 * @nr_alloc		Total number of structures allocated. Value is >= 0.
 * @nr2			Number of sub-items allocated per structure.
 *
 * OUT:
 * @st_pwr_wghfreq	Structure with statistics.
 *
 * RETURNS:
 * Number of CPU for which statistics have been read.
 * 1 means CPU "all", 2 means CPU "all" and 0, etc.
 * Or -1 if the buffer was to small and needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_cpu_wghfreq(struct stats_pwr_wghfreq *st_pwr_wghfreq, __nr_t nr_alloc,
			__nr_t nr2)
{
	__nr_t cpu_read = 0;
	int j;
	struct stats_pwr_wghfreq *st_pwr_wghfreq_i, *st_pwr_wghfreq_j, *st_pwr_wghfreq_all_j;

	do {
		if (cpu_read + 2 > nr_alloc)
			return -1;

		/* Read current CPU time-in-state data */
		st_pwr_wghfreq_i = st_pwr_wghfreq + (cpu_read + 1) * nr2;
		if (!read_time_in_state(st_pwr_wghfreq_i, cpu_read, nr2))
			break;

		/* Also save data for CPU 'all' */
		for (j = 0; j < nr2; j++) {
			st_pwr_wghfreq_j     = st_pwr_wghfreq_i + j;	/* CPU #cpu, state #j */
			st_pwr_wghfreq_all_j = st_pwr_wghfreq   + j;	/* CPU #all, state #j */
			if (!cpu_read) {
				/* Assume that possible frequencies are the same for all CPUs */
				st_pwr_wghfreq_all_j->freq = st_pwr_wghfreq_j->freq;
			}
			st_pwr_wghfreq_all_j->time_in_state += st_pwr_wghfreq_j->time_in_state;
		}
		cpu_read++;
	}
	while (1);

	if (cpu_read > 0) {
		for (j = 0; j < nr2; j++) {
			st_pwr_wghfreq_all_j = st_pwr_wghfreq + j;	/* CPU #all, state #j */
			st_pwr_wghfreq_all_j->time_in_state /= cpu_read;
		}

		return cpu_read + 1; /* For CPU "all" */
	}

	return 0;
}

/*
 ***************************************************************************
 * Read current USB device data.
 *
 * IN:
 * @st_pwr_usb		Structure where stats will be saved.
 * @usb_device		File name for current USB device.
 *
 * OUT:
 * @st_pwr_usb		Structure with statistics.
 ***************************************************************************
 */
void read_usb_stats(struct stats_pwr_usb *st_pwr_usb, char *usb_device)
{
	int l, rc;
	FILE *fp;
	char * rs;
	char filename[MAX_PF_NAME];

	/* Get USB device bus number */
	sscanf(usb_device, "%u", &st_pwr_usb->bus_nr);

	/* Read USB device vendor ID */
	snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
		 SYSFS_USBDEV, usb_device, SYSFS_IDVENDOR);
	if ((fp = fopen(filename, "r")) != NULL) {
		rc = fscanf(fp, "%x",
			    &st_pwr_usb->vendor_id);
		fclose(fp);
		if (rc == 0) {
			st_pwr_usb->vendor_id = 0;
		}
	}

	/* Read USB device product ID */
	snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
		 SYSFS_USBDEV, usb_device, SYSFS_IDPRODUCT);
	if ((fp = fopen(filename, "r")) != NULL) {
		rc = fscanf(fp, "%x",
			    &st_pwr_usb->product_id);
		fclose(fp);
		if (rc == 0) {
			st_pwr_usb->product_id = 0;
		}
	}

	/* Read USB device max power consumption */
	snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
		 SYSFS_USBDEV, usb_device, SYSFS_BMAXPOWER);
	if ((fp = fopen(filename, "r")) != NULL) {
		rc = fscanf(fp, "%u",
			    &st_pwr_usb->bmaxpower);
		fclose(fp);
		if (rc == 0) {
			st_pwr_usb->bmaxpower = 0;
		}
	}

	/* Read USB device manufacturer */
	snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
		 SYSFS_USBDEV, usb_device, SYSFS_MANUFACTURER);
	if ((fp = fopen(filename, "r")) != NULL) {
		rs = fgets(st_pwr_usb->manufacturer,
			   MAX_MANUF_LEN - 1, fp);
		fclose(fp);
		if ((rs != NULL) &&
		    (l = strlen(st_pwr_usb->manufacturer)) > 0) {
			/* Remove trailing CR */
			st_pwr_usb->manufacturer[l - 1] = '\0';
		}
	}

	/* Read USB device product */
	snprintf(filename, MAX_PF_NAME, "%s/%s/%s",
		 SYSFS_USBDEV, usb_device, SYSFS_PRODUCT);
	if ((fp = fopen(filename, "r")) != NULL) {
		rs = fgets(st_pwr_usb->product,
			   MAX_PROD_LEN - 1, fp);
		fclose(fp);
		if ((rs != NULL) &&
		    (l = strlen(st_pwr_usb->product)) > 0) {
			/* Remove trailing CR */
			st_pwr_usb->product[l - 1] = '\0';
		}
	}
}

/*
 ***************************************************************************
 * Read USB devices statistics.
 *
 * IN:
 * @st_pwr_usb		Structure where stats will be saved.
 * @nr_alloc		Total number of structures allocated. Value is >= 0.
 *
 * OUT:
 * @st_pwr_usb		Structure with statistics.
 *
 * RETURNS:
 * Number of USB devices read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_bus_usb_dev(struct stats_pwr_usb *st_pwr_usb, __nr_t nr_alloc)
{
	DIR *dir;
	struct dirent *drd;
	struct stats_pwr_usb *st_pwr_usb_i;
	__nr_t usb_read = 0;

	/* Open relevant /sys directory */
	if ((dir = opendir(SYSFS_USBDEV)) == NULL)
		return 0;

	/* Get current file entry */
	while ((drd = readdir(dir)) != NULL) {

		if (isdigit(drd->d_name[0]) && !strchr(drd->d_name, ':')) {

			if (usb_read + 1 > nr_alloc) {
				usb_read = -1;
				break;
			}

			/* Read current USB device data */
			st_pwr_usb_i = st_pwr_usb + usb_read++;
			read_usb_stats(st_pwr_usb_i, drd->d_name);
		}
	}

	/* Close directory */
	closedir(dir);
	return usb_read;
}

/*
 ***************************************************************************
 * Read filesystems statistics.
 *
 * IN:
 * @st_filesystem	Structure where stats will be saved.
 * @nr_alloc		Total number of structures allocated. Value is >= 0.
 *
 * OUT:
 * @st_filesystem	Structure with statistics.
 *
 * RETURNS:
 * Number of filesystems read, or -1 if the buffer was too small and
 * needs to be reallocated.
 ***************************************************************************
 */
__nr_t read_filesystem(struct stats_filesystem *st_filesystem, __nr_t nr_alloc)
{
	FILE *fp;
	char line[512], fs_name[MAX_FS_LEN], mountp[256], type[128];
	int skip = 0, skip_next = 0, fs;
	char *pos = 0, *pos2 = 0;
	__nr_t fs_read = 0;
	struct stats_filesystem *st_filesystem_i;
	struct statvfs buf;

	if ((fp = fopen(MTAB, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		/*
		 * Ignore line if the preceding line did not contain '\n'.
		 * (Some very long lines may be found for instance when
		 * overlay2 filesystem with docker is used).
		 */
		skip = skip_next;
		skip_next = (strchr(line, '\n') == NULL);
		if (skip)
			continue;

		if (line[0] == '/') {
			/* Find field separator position */
			pos = strchr(line, ' ');
			if (pos == NULL)
				continue;

			/*
			 * Find second field separator position,
			 * read filesystem type,
			 * if filesystem type is autofs, skip it
			*/
			pos2 = strchr(pos + 1, ' ');
			if (pos2 == NULL)
				continue;

			sscanf(pos2 + 1, "%127s", type);
			if(strcmp(type, "autofs") == 0)
				continue;

			/* Read current filesystem name */
			sscanf(line, "%127s", fs_name);
			/*
			 * And now read the corresponding mount point.
			 * Read fs name and mount point in two distinct operations,
			 * using '@pos + 1' position value for the mount point.
			 * Indeed, if fs name length is greater than 127 chars,
			 * previous scanf() would read only the first 127 chars, and
			 * mount point name would be read using the remaining chars
			 * from the fs name. This would result in a bogus name
			 * and following statvfs() function would always fail.
			 */
			sscanf(pos + 1, "%255s", mountp);

			/* Replace octal codes */
			oct2chr(mountp);

			/*
			 * It's important to have read the whole mount point name
			 * for statvfs() to work properly (see above).
			 */
			if ((__statvfs(mountp, &buf) < 0) || (!buf.f_blocks))
				continue;

			/* Check if it's a duplicate entry */
			fs = fs_read - 1;
			while (fs >= 0) {
				st_filesystem_i = st_filesystem + fs;
				if (!strcmp(st_filesystem_i->fs_name, fs_name))
					break;
				fs--;
			}
			if (fs >= 0)
				/* Duplicate entry found! Ignore current entry */
				continue;

			if (fs_read + 1 > nr_alloc) {
				fs_read = -1;
				break;
			}

			st_filesystem_i = st_filesystem + fs_read++;
			st_filesystem_i->f_blocks = (unsigned long long) buf.f_blocks * (unsigned long long) buf.f_frsize;
			st_filesystem_i->f_bfree  = (unsigned long long) buf.f_bfree * (unsigned long long) buf.f_frsize;
			st_filesystem_i->f_bavail = (unsigned long long) buf.f_bavail * (unsigned long long) buf.f_frsize;
			st_filesystem_i->f_files  = (unsigned long long) buf.f_files;
			st_filesystem_i->f_ffree  = (unsigned long long) buf.f_ffree;
			strncpy(st_filesystem_i->fs_name, fs_name, sizeof(st_filesystem_i->fs_name));
			st_filesystem_i->fs_name[sizeof(st_filesystem_i->fs_name) - 1] = '\0';
			strncpy(st_filesystem_i->mountp, mountp, sizeof(st_filesystem_i->mountp));
			st_filesystem_i->mountp[sizeof(st_filesystem_i->mountp) - 1] = '\0';
		}
	}

	fclose(fp);
	return fs_read;
}

/*
 ***************************************************************************
 * Read Fibre Channel HBA statistics.
 *
 * IN:
 * @st_fc	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 0.
 *
 * OUT:
 * @st_fc	Structure with statistics.
 *
 * RETURNS:
 * Number of FC hosts read, or -1 if the buffer was too small and needs to
 * be reallocated.
 ***************************************************************************
 */
__nr_t read_fchost(struct stats_fchost *st_fc, __nr_t nr_alloc)
{
	DIR *dir;
	FILE *fp;
	struct dirent *drd;
	struct stats_fchost *st_fc_i;
	__nr_t fch_read = 0;
	char fcstat_filename[MAX_PF_NAME];
	char line[256];
	unsigned long rx_frames, tx_frames, rx_words, tx_words;

	/* Each host, if present, will have its own hostX entry within SYSFS_FCHOST */
	if ((dir = __opendir(SYSFS_FCHOST)) == NULL)
		return 0; /* No FC hosts */

	/*
	 * Read each of the counters via sysfs, where they are
	 * returned as hex values (e.g. 0x72400).
	 */
	while ((drd = __readdir(dir)) != NULL) {
		rx_frames = tx_frames = rx_words = tx_words = 0;

		if (!strncmp(drd->d_name, "host", 4)) {

			if (fch_read + 1 > nr_alloc) {
				fch_read = -1;
				break;
			}

			snprintf(fcstat_filename, MAX_PF_NAME, FC_RX_FRAMES,
				 SYSFS_FCHOST, drd->d_name);
			if ((fp = fopen(fcstat_filename, "r"))) {
				if (fgets(line, sizeof(line), fp)) {
					sscanf(line, "%lx", &rx_frames);
				}
				fclose(fp);
			}

			snprintf(fcstat_filename, MAX_PF_NAME, FC_TX_FRAMES,
				 SYSFS_FCHOST, drd->d_name);
			if ((fp = fopen(fcstat_filename, "r"))) {
				if (fgets(line, sizeof(line), fp)) {
					sscanf(line, "%lx", &tx_frames);
				}
				fclose(fp);
			}

			snprintf(fcstat_filename, MAX_PF_NAME, FC_RX_WORDS,
				 SYSFS_FCHOST, drd->d_name);
			if ((fp = fopen(fcstat_filename, "r"))) {
				if (fgets(line, sizeof(line), fp)) {
					sscanf(line, "%lx", &rx_words);
				}
				fclose(fp);
			}

			snprintf(fcstat_filename, MAX_PF_NAME, FC_TX_WORDS,
				 SYSFS_FCHOST, drd->d_name);
			if ((fp = fopen(fcstat_filename, "r"))) {
				if (fgets(line, sizeof(line), fp)) {
					sscanf(line, "%lx", &tx_words);
				}
				fclose(fp);
			}

			st_fc_i = st_fc + fch_read++;
			st_fc_i->f_rxframes = rx_frames;
			st_fc_i->f_txframes = tx_frames;
			st_fc_i->f_rxwords  = rx_words;
			st_fc_i->f_txwords  = tx_words;
			memcpy(st_fc_i->fchost_name, drd->d_name, sizeof(st_fc_i->fchost_name));
			st_fc_i->fchost_name[sizeof(st_fc_i->fchost_name) - 1] = '\0';
		}
	}

	__closedir(dir);
	return fch_read;
}

/*
 ***************************************************************************
 * Read softnet statistics.
 *
 * IN:
 * @st_softnet	Structure where stats will be saved.
 * @nr_alloc	Total number of structures allocated. Value is >= 0.
 * @online_cpu_bitmap
 *		Bitmap listing online CPU.
 *
 * OUT:
 * @st_softnet	Structure with statistics.
 *
 * RETURNS:
 * 1 if stats have been sucessfully read, or 0 otherwise.
 * Returns -1 if the buffer was too small and needs to be reallocated.
 ***************************************************************************
 */
int read_softnet(struct stats_softnet *st_softnet, __nr_t nr_alloc,
		  unsigned char online_cpu_bitmap[])
{
	FILE *fp;
	struct stats_softnet *st_softnet_i;
	char line[1024];
	int cpu = 1, rc = 1;

	/* Open /proc/net/softnet_stat file */
	if ((fp = fopen(NET_SOFTNET, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		while ((!(online_cpu_bitmap[(cpu - 1) >> 3] & (1 << ((cpu - 1) & 0x07)))) && (cpu <= NR_CPUS + 1)) {
			cpu++;
		}
		if (cpu > NR_CPUS + 1)
			/* Should never happen */
			return 0;

		if (cpu + 1 > nr_alloc) {
			rc = -1;
			break;
		}

		st_softnet_i = st_softnet + cpu++;
		sscanf(line, "%x %x %x %*x %*x %*x %*x %*x %*x %x %x",
		       &st_softnet_i->processed,
		       &st_softnet_i->dropped,
		       &st_softnet_i->time_squeeze,
		       &st_softnet_i->received_rps,
		       &st_softnet_i->flow_limit);
	}

	fclose(fp);
	return rc;
}

/*
 ***************************************************************************
 * Read pressure-stall information from a file located in /proc/pressure
 * directory.
 *
 * IN:
 * @st_psi	Structure where stats will be saved.
 * @filename	File located in /proc/pressure directory to read.
 * @token	"some" or "full". Indicate which line shall be read in file.
 *
 * OUT:
 * @st_psi	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
int read_psi_stub(struct stats_psi *st_psi, char *filename, char *token)
{
	FILE *fp;
	char line[8192];
	unsigned long psi_tmp[3];
	int rc = 0, len;

	if ((fp = fopen(filename, "r")) == NULL)
		return 0;

	len = strlen(token);
	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, token, len)) {
			/* Read stats */
			rc = sscanf(line + len + 1, "avg10=%lu.%lu avg60=%lu.%lu avg300=%lu.%lu total=%llu",
				    &psi_tmp[0], &st_psi->avg10,
				    &psi_tmp[1], &st_psi->avg60,
				    &psi_tmp[2], &st_psi->avg300,
				    &st_psi->total);
		}
	}

	fclose(fp);

	if (rc < 7)
		return 0;

	st_psi->avg10  += psi_tmp[0] * 100;
	st_psi->avg60  += psi_tmp[1] * 100;
	st_psi->avg300 += psi_tmp[2] * 100;

	return 1;
}

/*
 ***************************************************************************
 * Read pressure-stall CPU information.
 *
 * IN:
 * @st_psi_cpu	Structure where stats will be saved.
 *
 * OUT:
 * @st_psi_cpu	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_psicpu(struct stats_psi_cpu *st_psi_cpu)
{
	struct stats_psi st_psi;

	/* Read CPU stats */
	if (!read_psi_stub(&st_psi, PSI_CPU, "some"))
		return 0;

	st_psi_cpu->some_acpu_10   = st_psi.avg10;
	st_psi_cpu->some_acpu_60   = st_psi.avg60;
	st_psi_cpu->some_acpu_300  = st_psi.avg300;
	st_psi_cpu->some_cpu_total = st_psi.total;

	return 1;
}

/*
 ***************************************************************************
 * Read pressure-stall I/O information.
 *
 * IN:
 * @st_psi_io	Structure where stats will be saved.
 *
 * OUT:
 * @st_psi_io	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_psiio(struct stats_psi_io *st_psi_io)
{
	struct stats_psi st_psi;

	/* Read I/O "some" stats */
	if (!read_psi_stub(&st_psi, PSI_IO, "some"))
		return 0;

	st_psi_io->some_aio_10   = st_psi.avg10;
	st_psi_io->some_aio_60   = st_psi.avg60;
	st_psi_io->some_aio_300  = st_psi.avg300;
	st_psi_io->some_io_total = st_psi.total;

	/* Read I/O "full" stats */
	if (!read_psi_stub(&st_psi, PSI_IO, "full"))
		return 0;

	st_psi_io->full_aio_10   = st_psi.avg10;
	st_psi_io->full_aio_60   = st_psi.avg60;
	st_psi_io->full_aio_300  = st_psi.avg300;
	st_psi_io->full_io_total = st_psi.total;

	return 1;
}

/*
 ***************************************************************************
 * Read pressure-stall memory information.
 *
 * IN:
 * @st_psi_mem	Structure where stats will be saved.
 *
 * OUT:
 * @st_psi_mem	Structure with statistics.
 *
 * RETURNS:
 * 1 on success, 0 otherwise.
 ***************************************************************************
 */
__nr_t read_psimem(struct stats_psi_mem *st_psi_mem)
{
	struct stats_psi st_psi;

	/* Read memory "some" stats */
	if (!read_psi_stub(&st_psi, PSI_MEM, "some"))
		return 0;

	st_psi_mem->some_amem_10   = st_psi.avg10;
	st_psi_mem->some_amem_60   = st_psi.avg60;
	st_psi_mem->some_amem_300  = st_psi.avg300;
	st_psi_mem->some_mem_total = st_psi.total;

	/* Read memory "full" stats */
	if (!read_psi_stub(&st_psi, PSI_MEM, "full"))
		return 0;

	st_psi_mem->full_amem_10   = st_psi.avg10;
	st_psi_mem->full_amem_60   = st_psi.avg60;
	st_psi_mem->full_amem_300  = st_psi.avg300;
	st_psi_mem->full_mem_total = st_psi.total;

	return 1;
}

/*------------------ END: FUNCTIONS USED BY SADC ONLY ---------------------*/
#endif /* SOURCE_SADC */
