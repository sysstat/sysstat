/*
 * mpstat: per-processor statistics
 * (C) 2000-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/utsname.h>

#include "version.h"
#include "mpstat.h"
#include "rd_stats.h"
#include "count.h"

#include <locale.h>	/* For setlocale() */
#ifdef USE_NLS
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef USE_SCCSID
#define SCCSID "@(#)sysstat-" VERSION ": "  __FILE__ " compiled " __DATE__ " " __TIME__
char *sccsid(void) { return (SCCSID); }
#endif

unsigned long long uptime_cs[3] = {0, 0, 0};

/* NOTE: Use array of _char_ for bitmaps to avoid endianness problems...*/
unsigned char *cpu_bitmap;	/* Bit 0: Global; Bit 1: 1st proc; etc. */
unsigned char *node_bitmap;	/* Bit 0: Global; Bit 1: 1st NUMA node; etc. */

/* Structures used to save CPU and NUMA nodes CPU stats */
struct stats_cpu *st_cpu[3];
struct stats_cpu *st_node[3];

/*
 * Structure used to save total number of interrupts received
 * among all CPU and for each CPU.
 */
struct stats_irq *st_irq[3];

/*
 * Structures used to save, for each interrupt, the number
 * received by each CPU.
 */
struct stats_irqcpu *st_irqcpu[3];
struct stats_irqcpu *st_softirqcpu[3];

/*
 * Number of CPU per node, e.g.:
 * cpu_per_node[0]: total nr of CPU (this is node "all")
 * cpu_per_node[1]: nr of CPU for node 0
 * etc.
 */
int *cpu_per_node;

/*
 * Node number the CPU belongs to, e.g.:
 * cpu2node[0]: node nr for CPU 0
 */
int *cpu2node;

/* CPU topology */
struct cpu_topology *st_cpu_topology;

struct tm mp_tstamp[3];

/* Activity flag */
unsigned int actflags = 0;

unsigned int flags = 0;

/* Interval and count parameters */
long interval = -1, count = 0;
/* Number of decimal places */
int dplaces_nr = -1;

/*
 * Nb of processors on the machine.
 * A value of 2 means there are 2 processors (0 and 1).
 */
int cpu_nr = 0;

/*
 * Highest NUMA node number found on the machine.
 * A value of 0 means node 0 (one node).
 * A value of -1 means no nodes found.
 * We have: node_nr < cpu_nr (see get_node_placement() function).
 */
int node_nr = -1;

/* Nb of interrupts per processor */
int irqcpu_nr = 0;
/* Nb of soft interrupts per processor */
int softirqcpu_nr = 0;

struct sigaction alrm_act, int_act;
int sigint_caught = 0;

/*
 ***************************************************************************
 * Print usage and exit
 *
 * IN:
 * @progname	Name of sysstat command
 ***************************************************************************
 */
void usage(char *progname)
{
	fprintf(stderr, _("Usage: %s [ options ] [ <interval> [ <count> ] ]\n"),
		progname);

	fprintf(stderr, _("Options are:\n"
			  "[ -A ] [ -n ] [ -T ] [ -u ] [ -V ]\n"
			  "[ -I { SUM | CPU | SCPU | ALL } ] [ -N { <node_list> | ALL } ]\n"
			  "[ --dec={ 0 | 1 | 2 } ] [ -o JSON ] [ -P { <cpu_list> | ALL } ]\n"));
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
 * SIGINT signal handler.
 *
 * IN:
 * @sig	Signal number.
 **************************************************************************
 */
void int_handler(int sig)
{
	sigint_caught = 1;
}

/*
 ***************************************************************************
 * Allocate stats structures and cpu bitmap. Also do it for NUMA nodes
 * (although the machine may not be a NUMA one). Assume that the number of
 * nodes is lower or equal than that of CPU.
 *
 * IN:
 * @nr_cpus	Number of CPUs. This is the real number of available CPUs + 1
 * 		because we also have to allocate a structure for CPU 'all'.
 ***************************************************************************
 */
void salloc_mp_struct(int nr_cpus)
{
	int i;

	for (i = 0; i < 3; i++) {

		if ((st_cpu[i] = (struct stats_cpu *) malloc(STATS_CPU_SIZE * nr_cpus))
		    == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_cpu[i], 0, STATS_CPU_SIZE * nr_cpus);

		if ((st_node[i] = (struct stats_cpu *) malloc(STATS_CPU_SIZE * nr_cpus))
		    == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_node[i], 0, STATS_CPU_SIZE * nr_cpus);

		if ((st_irq[i] = (struct stats_irq *) malloc(STATS_IRQ_SIZE * nr_cpus))
		    == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_irq[i], 0, STATS_IRQ_SIZE * nr_cpus);

		if ((st_irqcpu[i] = (struct stats_irqcpu *) malloc(STATS_IRQCPU_SIZE * nr_cpus * irqcpu_nr))
		    == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_irqcpu[i], 0, STATS_IRQCPU_SIZE * nr_cpus * irqcpu_nr);

		if ((st_softirqcpu[i] = (struct stats_irqcpu *) malloc(STATS_IRQCPU_SIZE * nr_cpus * softirqcpu_nr))
		     == NULL) {
			perror("malloc");
			exit(4);
		}
		memset(st_softirqcpu[i], 0, STATS_IRQCPU_SIZE * nr_cpus * softirqcpu_nr);
	}

	if ((cpu_bitmap = (unsigned char *) malloc((nr_cpus >> 3) + 1)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(cpu_bitmap, 0, (nr_cpus >> 3) + 1);

	if ((node_bitmap = (unsigned char *) malloc((nr_cpus >> 3) + 1)) == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(node_bitmap, 0, (nr_cpus >> 3) + 1);

	if ((cpu_per_node = (int *) malloc(sizeof(int) * nr_cpus)) == NULL) {
		perror("malloc");
		exit(4);
	}

	if ((cpu2node = (int *) malloc(sizeof(int) * nr_cpus)) == NULL) {
		perror("malloc");
		exit(4);
	}

	if ((st_cpu_topology = (struct cpu_topology *) malloc(sizeof(struct cpu_topology) * nr_cpus)) == NULL) {
		perror("malloc");
		exit(4);
	}
}

/*
 ***************************************************************************
 * Free structures and bitmap.
 ***************************************************************************
 */
void sfree_mp_struct(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		free(st_cpu[i]);
		free(st_node[i]);
		free(st_irq[i]);
		free(st_irqcpu[i]);
		free(st_softirqcpu[i]);
	}

	free(cpu_bitmap);
	free(node_bitmap);
	free(cpu_per_node);
	free(cpu2node);
}

/*
 ***************************************************************************
 * Get node placement (which node each CPU belongs to, and total number of
 * CPU that each node has).
 *
 * IN:
 * @nr_cpus		Number of CPU on this machine.
 *
 * OUT:
 * @cpu_per_node	Number of CPU per node.
 * @cpu2node		The node the CPU belongs to.
 *
 * RETURNS:
 * Highest node number found (e.g., 0 means node 0).
 * A value of -1 means no nodes have been found.
 ***************************************************************************
 */
int get_node_placement(int nr_cpus, int cpu_per_node[], int cpu2node[])

{
	DIR *dir;
	struct dirent *drd;
	char line[MAX_PF_NAME];
	int cpu, node, hi_node_nr = -1;

	/* Init number of CPU per node */
	memset(cpu_per_node, 0, sizeof(int) * (nr_cpus + 1));
	/* CPU belongs to no node by default */
	memset(cpu2node, -1, sizeof(int) * nr_cpus);

	/* This is node "all" */
	cpu_per_node[0] = nr_cpus;

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		snprintf(line, sizeof(line), "%s/cpu%d", SYSFS_DEVCPU, cpu);
		line[sizeof(line) - 1] = '\0';

		/* Open relevant /sys directory */
		if ((dir = opendir(line)) == NULL)
			return -1;

		/* Get current file entry */
		while ((drd = readdir(dir)) != NULL) {

			if (!strncmp(drd->d_name, "node", 4) && isdigit(drd->d_name[4])) {
				node = atoi(drd->d_name + 4);
				if ((node >= nr_cpus) || (node < 0)) {
					/* Assume we cannot have more nodes than CPU */
					closedir(dir);
					return -1;
				}
				cpu_per_node[node + 1]++;
				cpu2node[cpu] = node;
				if (node > hi_node_nr) {
					hi_node_nr = node;
				}
				/* Node placement found for current CPU: Go to next CPU directory */
				break;
			}
		}

		/* Close directory */
		closedir(dir);
	}

	return hi_node_nr;
}

/*
 ***************************************************************************
 * Read system logical topology: Socket number for each logical core is read
 * from the /sys/devices/system/cpu/cpu{N}/topology/physical_package_id file,
 * and the logical core id number is the first number read from the
 * /sys/devices/system/cpu/cpu{N}/topology/thread_siblings_list file.
 * Don't use /sys/devices/system/cpu/cpu{N}/topology/core_id as this is the
 * physical core id (seems to be different from the number displayed by lscpu).
 *
 * IN:
 * @nr_cpus	Number of CPU on this machine.
 * @cpu_topo	Structures where socket and core id numbers will be saved.
 *
 * OUT:
 * @cpu_topo	Structures where socket and core id numbers have been saved.
 ***************************************************************************
 */
void read_topology(int nr_cpus, struct cpu_topology *cpu_topo)
{
	struct cpu_topology *cpu_topo_i;
	FILE *fp;
	char filename[MAX_PF_NAME];
	int cpu, rc;

	/* Init system topology */
	memset(st_cpu_topology, 0, sizeof(struct cpu_topology) * nr_cpus);

	for (cpu = 0; cpu < nr_cpus; cpu++) {

		cpu_topo_i = cpu_topo + cpu;

		/* Read current CPU's socket number */
		snprintf(filename, sizeof(filename), "%s/cpu%d/%s", SYSFS_DEVCPU, cpu, PHYS_PACK_ID);
		filename[sizeof(filename) - 1] = '\0';

		if ((fp = fopen(filename, "r")) != NULL) {
			rc = fscanf(fp, "%d", &cpu_topo_i->phys_package_id);
			fclose(fp);

			if (rc < 1) {
				cpu_topo_i->phys_package_id = -1;
			}
		}

		/* Read current CPU's logical core id number */
		snprintf(filename, sizeof(filename), "%s/cpu%d/%s", SYSFS_DEVCPU, cpu, THREAD_SBL_LST);
		filename[sizeof(filename) - 1] = '\0';

		if ((fp = fopen(filename, "r")) != NULL) {
			rc = fscanf(fp, "%d", &cpu_topo_i->logical_core_id);
			fclose(fp);

			if (rc < 1) {
				cpu_topo_i->logical_core_id = -1;
			}
		}
	}
}

/*
 ***************************************************************************
 * Compute node statistics: Split CPU statistics among nodes.
 *
 * IN:
 * @src		Structure containing CPU stats to add.
 *
 * OUT:
 * @dest	Structure containing global CPU stats.
 ***************************************************************************
 */
void add_cpu_stats(struct stats_cpu *dest, struct stats_cpu *src)
{
	dest->cpu_user       += src->cpu_user;
	dest->cpu_nice       += src->cpu_nice;
	dest->cpu_sys        += src->cpu_sys;
	dest->cpu_idle       += src->cpu_idle;
	dest->cpu_iowait     += src->cpu_iowait;
	dest->cpu_hardirq    += src->cpu_hardirq;
	dest->cpu_softirq    += src->cpu_softirq;
	dest->cpu_steal      += src->cpu_steal;
	dest->cpu_guest      += src->cpu_guest;
	dest->cpu_guest_nice += src->cpu_guest_nice;
}

/*
 ***************************************************************************
 * Compute node statistics: Split CPU statistics among nodes.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 *
 * OUT:
 * @st_node	Array where CPU stats for each node have been saved.
 ***************************************************************************
 */
void set_node_cpu_stats(int prev, int curr)
{
	int cpu;
	unsigned long long tot_jiffies_p;
	struct stats_cpu *scp, *scc, *snp, *snc;
	struct stats_cpu *scc_all = st_cpu[curr];
	struct stats_cpu *scp_all = st_cpu[prev];
	struct stats_cpu *snc_all = st_node[curr];
	struct stats_cpu *snp_all = st_node[prev];

	/* Reset structures */
	memset(st_node[prev], 0, STATS_CPU_SIZE * (cpu_nr + 1));
	memset(st_node[curr], 0, STATS_CPU_SIZE * (cpu_nr + 1));

	/* Node 'all' is the same as CPU 'all' */
	*snp_all = *scp_all;
	*snc_all = *scc_all;

	/* Individual nodes */
	for (cpu = 0; cpu < cpu_nr; cpu++) {
		scc = st_cpu[curr] + cpu + 1;
		scp = st_cpu[prev] + cpu + 1;
		snp = st_node[prev] + cpu2node[cpu] + 1;
		snc = st_node[curr] + cpu2node[cpu] + 1;


		tot_jiffies_p = scp->cpu_user + scp->cpu_nice +
				scp->cpu_sys + scp->cpu_idle +
				scp->cpu_iowait + scp->cpu_hardirq +
				scp->cpu_steal + scp->cpu_softirq;
		if ((tot_jiffies_p == 0) && (interval != 0))
			/*
			 * CPU has just come back online with no ref from
			 * previous iteration: Skip it.
			 */
			continue;

		add_cpu_stats(snp, scp);
		add_cpu_stats(snc, scc);
	}
}

/*
 ***************************************************************************
 * Compute global CPU statistics as the sum of individual CPU ones, and
 * calculate interval for global CPU.
 * Also identify offline CPU.
 *
 * IN:
 * @prev	Index in array where stats used as reference are.
 * @curr	Index in array for current sample statistics.
 * @offline_cpu_bitmap
 *		CPU bitmap for offline CPU.
 *
 * OUT:
 * @offline_cpu_bitmap
 *		CPU bitmap with offline CPU.
 *
 * RETURNS:
 * Interval for global CPU.
 ***************************************************************************
 */
unsigned long long get_global_cpu_mpstats(int prev, int curr,
					  unsigned char offline_cpu_bitmap[])
{
	int i;
	unsigned long long tot_jiffies_c, tot_jiffies_p;
	unsigned long long deltot_jiffies = 0;
	struct stats_cpu *scc, *scp;
	struct stats_cpu *scc_all = st_cpu[curr];
	struct stats_cpu *scp_all = st_cpu[prev];

	/*
	 * For UP machines we keep the values read from global CPU line in /proc/stat.
	 * Also look for offline CPU: They won't be displayed, and some of their values may
	 * have to be modified.
	 */
	if (cpu_nr > 1) {
		memset(scc_all, 0, sizeof(struct stats_cpu));
		memset(scp_all, 0, sizeof(struct stats_cpu));
	}
	else {
		/* This is a UP machine */
		return get_per_cpu_interval(st_cpu[curr], st_cpu[prev]);
	}

	for (i = 1; i <= cpu_nr; i++) {

		scc = st_cpu[curr] + i;
		scp = st_cpu[prev] + i;

		/*
		 * Compute the total number of jiffies spent by current processor.
		 * NB: Don't add cpu_guest/cpu_guest_nice because cpu_user/cpu_nice
		 * already include them.
		 */
		tot_jiffies_c = scc->cpu_user + scc->cpu_nice +
				scc->cpu_sys + scc->cpu_idle +
				scc->cpu_iowait + scc->cpu_hardirq +
				scc->cpu_steal + scc->cpu_softirq;
		tot_jiffies_p = scp->cpu_user + scp->cpu_nice +
				scp->cpu_sys + scp->cpu_idle +
				scp->cpu_iowait + scp->cpu_hardirq +
				scp->cpu_steal + scp->cpu_softirq;

		/*
		 * If the CPU is offline then it is omited from /proc/stat:
		 * All the fields couldn't have been read and the sum of them is zero.
		 */
		if (tot_jiffies_c == 0) {
			/*
			 * CPU is currently offline.
			 * Set current struct fields (which have been set to zero)
			 * to values from previous iteration. Hence their values won't
			 * jump from zero when the CPU comes back online.
			 * Note that this workaround no longer fully applies with recent kernels,
			 * as I have noticed that when a CPU comes back online, some fields
			 * restart from their previous value (e.g. user, nice, system)
			 * whereas others restart from zero (idle, iowait)! To deal with this,
			 * the get_per_cpu_interval() function will set these previous values
			 * to zero if necessary.
			 */
			*scc = *scp;

			/*
			 * Mark CPU as offline to not display it
			 * (and thus it will not be confused with a tickless CPU).
			 */
			offline_cpu_bitmap[i >> 3] |= 1 << (i & 0x07);
		}

		if ((tot_jiffies_p == 0) && (interval != 0)) {
			/*
			 * CPU has just come back online.
			 * Unfortunately, no reference values are available
			 * from a previous iteration, probably because it was
			 * already offline when the first sample has been taken.
			 * So don't display that CPU to prevent "jump-from-zero"
			 * output syndrome, and don't take it into account for CPU "all".
			 * NB: Test for interval != 0 to make sure we don't want stats
			 * since boot time.
			 */
			offline_cpu_bitmap[i >> 3] |= 1 << (i & 0x07);
			continue;
		}

		/*
		 * Get interval for current CPU and add it to global CPU.
		 * Note: Previous idle and iowait values (saved in scp) may be modified here.
		 */
		deltot_jiffies += get_per_cpu_interval(scc, scp);

		add_cpu_stats(scc_all, scc);
		add_cpu_stats(scp_all, scp);
	}

	return deltot_jiffies;
}

/*
 ***************************************************************************
 * Display CPU statistics in plain format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @offline_cpu_bitmap
 *		CPU bitmap for offline CPU.
 ***************************************************************************
 */
void write_plain_cpu_stats(int dis, unsigned long long deltot_jiffies, int prev, int curr,
			   char *prev_string, char *curr_string, unsigned char offline_cpu_bitmap[])
{
	int i;
	struct stats_cpu *scc, *scp;
	struct cpu_topology *cpu_topo_i;

	if (dis) {
		printf("\n%-11s  CPU", prev_string);
		if (DISPLAY_TOPOLOGY(flags)) {
			printf(" CORE SOCK NODE");
		}
		printf("    %%usr   %%nice    %%sys %%iowait    %%irq   "
		       "%%soft  %%steal  %%guest  %%gnice   %%idle\n");
	}

	/*
	 * Now display CPU statistics (including CPU "all"),
	 * except for offline CPU or CPU that the user doesn't want to see.
	 */
	for (i = 0; i <= cpu_nr; i++) {

		/* Check if we want stats about this proc */
		if (!(*(cpu_bitmap + (i >> 3)) & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			continue;

		scc = st_cpu[curr] + i;
		scp = st_cpu[prev] + i;

		printf("%-11s", curr_string);

		if (i == 0) {
			/* This is CPU "all" */
			cprintf_in(IS_STR, " %s", " all", 0);

			if (DISPLAY_TOPOLOGY(flags)) {
				printf("               ");
			}
		}
		else {
			cprintf_in(IS_INT, " %4d", "", i - 1);

			if (DISPLAY_TOPOLOGY(flags)) {
				cpu_topo_i = st_cpu_topology + i - 1;
				cprintf_in(IS_INT, " %4d", "", cpu_topo_i->logical_core_id);
				cprintf_in(IS_INT, " %4d", "", cpu_topo_i->phys_package_id);
				cprintf_in(IS_INT, " %4d", "", cpu2node[i - 1]);
			}

			/* Recalculate itv for current proc */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/*
				 * If the CPU is tickless then there is no change in CPU values
				 * but the sum of values is not zero.
				 */
				cprintf_pc(NO_UNIT, 10, 7, 2,
					   0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 100.0);
				printf("\n");

				continue;
			}
		}

		cprintf_pc(NO_UNIT, 10, 7, 2,
			   (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
			   0.0 :
			   ll_sp_value(scp->cpu_user - scp->cpu_guest,
				       scc->cpu_user - scc->cpu_guest, deltot_jiffies),
			   (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
			   0.0 :
			   ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
				       scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies),
			   ll_sp_value(scp->cpu_sys,
				       scc->cpu_sys, deltot_jiffies),
			   ll_sp_value(scp->cpu_iowait,
				       scc->cpu_iowait, deltot_jiffies),
			   ll_sp_value(scp->cpu_hardirq,
				       scc->cpu_hardirq, deltot_jiffies),
			   ll_sp_value(scp->cpu_softirq,
				       scc->cpu_softirq, deltot_jiffies),
			   ll_sp_value(scp->cpu_steal,
				       scc->cpu_steal, deltot_jiffies),
			   ll_sp_value(scp->cpu_guest,
				       scc->cpu_guest, deltot_jiffies),
			   ll_sp_value(scp->cpu_guest_nice,
				       scc->cpu_guest_nice, deltot_jiffies),
			   (scc->cpu_idle < scp->cpu_idle) ?
			   0.0 :
			   ll_sp_value(scp->cpu_idle,
				       scc->cpu_idle, deltot_jiffies));
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display CPU statistics in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @offline_cpu_bitmap
 *		CPU bitmap for offline CPU.
 ***************************************************************************
 */
void write_json_cpu_stats(int tab, unsigned long long deltot_jiffies, int prev, int curr,
			  unsigned char offline_cpu_bitmap[])
{
	int i, next = FALSE;
	char cpu_name[16], topology[1024] = "";
	struct stats_cpu *scc, *scp;
	struct cpu_topology *cpu_topo_i;

	xprintf(tab++, "\"cpu-load\": [");

	/*
	 * Now display CPU statistics (including CPU "all"),
	 * except for offline CPU or CPU that the user doesn't want to see.
	 */
	for (i = 0; i <= cpu_nr; i++) {

		/* Check if we want stats about this proc */
		if (!(*(cpu_bitmap + (i >> 3)) & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			continue;

		scc = st_cpu[curr] + i;
		scp = st_cpu[prev] + i;

		if (next) {
			printf(",\n");
		}
		next = TRUE;

		if (i == 0) {
			/* This is CPU "all" */
			strcpy(cpu_name, "all");

			if (DISPLAY_TOPOLOGY(flags)) {
				snprintf(topology, 1024,
					 ", \"core\": \"\", \"socket\": \"\", \"node\": \"\"");
			}

		}
		else {
			snprintf(cpu_name, 16, "%d", i - 1);
			cpu_name[15] = '\0';

			if (DISPLAY_TOPOLOGY(flags)) {
				cpu_topo_i = st_cpu_topology + i - 1;
				snprintf(topology, 1024,
					 ", \"core\": \"%d\", \"socket\": \"%d\", \"node\": \"%d\"",
					 cpu_topo_i->logical_core_id, cpu_topo_i->phys_package_id, cpu2node[i - 1]);
			}

			/* Recalculate itv for current proc */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/*
				 * If the CPU is tickless then there is no change in CPU values
				 * but the sum of values is not zero.
				 */
				xprintf0(tab, "{\"cpu\": \"%d\"%s, \"usr\": 0.00, \"nice\": 0.00, "
					 "\"sys\": 0.00, \"iowait\": 0.00, \"irq\": 0.00, "
					 "\"soft\": 0.00, \"steal\": 0.00, \"guest\": 0.00, "
					 "\"gnice\": 0.00, \"idle\": 100.00}", i - 1, topology);
				printf("\n");

				continue;
			}
		}

		xprintf0(tab, "{\"cpu\": \"%s\"%s, \"usr\": %.2f, \"nice\": %.2f, \"sys\": %.2f, "
			 "\"iowait\": %.2f, \"irq\": %.2f, \"soft\": %.2f, \"steal\": %.2f, "
			 "\"guest\": %.2f, \"gnice\": %.2f, \"idle\": %.2f}",
			 cpu_name, topology,
			 (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
			 0.0 :
			 ll_sp_value(scp->cpu_user - scp->cpu_guest,
				     scc->cpu_user - scc->cpu_guest, deltot_jiffies),
			 (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
			 0.0 :
			 ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
				     scc->cpu_nice - scc->cpu_guest_nice, deltot_jiffies),
			 ll_sp_value(scp->cpu_sys,
				     scc->cpu_sys, deltot_jiffies),
			 ll_sp_value(scp->cpu_iowait,
				     scc->cpu_iowait, deltot_jiffies),
			 ll_sp_value(scp->cpu_hardirq,
				     scc->cpu_hardirq, deltot_jiffies),
			 ll_sp_value(scp->cpu_softirq,
				     scc->cpu_softirq, deltot_jiffies),
			 ll_sp_value(scp->cpu_steal,
				     scc->cpu_steal, deltot_jiffies),
			 ll_sp_value(scp->cpu_guest,
				     scc->cpu_guest, deltot_jiffies),
			 ll_sp_value(scp->cpu_guest_nice,
				     scc->cpu_guest_nice, deltot_jiffies),
			 (scc->cpu_idle < scp->cpu_idle) ?
			 0.0 :
			 ll_sp_value(scp->cpu_idle,
				     scc->cpu_idle, deltot_jiffies));
	}

	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display CPU statistics in plain or JSON format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @tab		Number of tabs to print (JSON format only).
 * @next	TRUE is a previous activity has been displayed (JSON format
 * 		only).
 * @offline_cpu_bitmap
 *		CPU bitmap for offline CPU.
 ***************************************************************************
 */
void write_cpu_stats(int dis, unsigned long long deltot_jiffies, int prev, int curr,
		     char *prev_string, char *curr_string, int tab, int *next,
		     unsigned char offline_cpu_bitmap[])
{
	if (!deltot_jiffies) {
		/* CPU "all" cannot be tickless */
		deltot_jiffies = 1;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		if (*next) {
			printf(",\n");
		}
		*next = TRUE;
		write_json_cpu_stats(tab, deltot_jiffies, prev, curr,
				     offline_cpu_bitmap);
	}
	else {
		write_plain_cpu_stats(dis, deltot_jiffies, prev, curr,
				      prev_string, curr_string, offline_cpu_bitmap);
	}
}

/*
 ***************************************************************************
 * Display CPU statistics for NUMA nodes in plain format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 ***************************************************************************
 */
void write_plain_node_stats(int dis, unsigned long long deltot_jiffies,
			    int prev, int curr, char *prev_string, char *curr_string)
{
	struct stats_cpu *snc, *snp, *scc, *scp;
	int cpu, node;

	if (dis) {
		printf("\n%-11s NODE    %%usr   %%nice    %%sys %%iowait    %%irq   "
		       "%%soft  %%steal  %%guest  %%gnice   %%idle\n",
		       prev_string);
	}

	for (node = 0; node <= node_nr + 1; node++) {

		snc = st_node[curr] + node;
		snp = st_node[prev] + node;

		/* Check if we want stats about this node */
		if (!(*(node_bitmap + (node >> 3)) & (1 << (node & 0x07))))
			continue;

		if (!cpu_per_node[node])
			/* No CPU in this node */
			continue;

		printf("%-11s", curr_string);
		if (node == 0) {
			/* This is node "all", i.e. CPU "all" */
			cprintf_in(IS_STR, " %s", " all", 0);
		}
		else {
			cprintf_in(IS_INT, " %4d", "", node - 1);

			/* Recalculate interval for current node */
			deltot_jiffies = 0;
			for (cpu = 1; cpu <= cpu_nr; cpu++) {
				scc = st_cpu[curr] + cpu;
				scp = st_cpu[prev] + cpu;

				if ((scp->cpu_user + scp->cpu_nice + scp->cpu_sys +
				     scp->cpu_idle + scp->cpu_iowait + scp->cpu_hardirq +
				     scp->cpu_steal + scp->cpu_softirq == 0) && (interval != 0))
					continue;

				if (cpu2node[cpu - 1] == node - 1) {
					deltot_jiffies += get_per_cpu_interval(scc, scp);
				}
			}

			if (!deltot_jiffies) {
				/* All CPU in node are tickless and/or offline */
				cprintf_pc(NO_UNIT, 10, 7, 2,
					   0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 100.0);
				printf("\n");

				continue;
			}
		}

		cprintf_pc(NO_UNIT, 10, 7, 2,
			   (snc->cpu_user - snc->cpu_guest) < (snp->cpu_user - snp->cpu_guest) ?
			   0.0 :
			   ll_sp_value(snp->cpu_user - snp->cpu_guest,
				       snc->cpu_user - snc->cpu_guest, deltot_jiffies),
			   (snc->cpu_nice - snc->cpu_guest_nice) < (snp->cpu_nice - snp->cpu_guest_nice) ?
			   0.0 :
			   ll_sp_value(snp->cpu_nice - snp->cpu_guest_nice,
				       snc->cpu_nice - snc->cpu_guest_nice, deltot_jiffies),
			   ll_sp_value(snp->cpu_sys,
				       snc->cpu_sys, deltot_jiffies),
			   ll_sp_value(snp->cpu_iowait,
				       snc->cpu_iowait, deltot_jiffies),
			   ll_sp_value(snp->cpu_hardirq,
				       snc->cpu_hardirq, deltot_jiffies),
			   ll_sp_value(snp->cpu_softirq,
				       snc->cpu_softirq, deltot_jiffies),
			   ll_sp_value(snp->cpu_steal,
				       snc->cpu_steal, deltot_jiffies),
			   ll_sp_value(snp->cpu_guest,
				       snc->cpu_guest, deltot_jiffies),
			   ll_sp_value(snp->cpu_guest_nice,
				       snc->cpu_guest_nice, deltot_jiffies),
			   (snc->cpu_idle < snp->cpu_idle) ?
			   0.0 :
			   ll_sp_value(snp->cpu_idle,
				       snc->cpu_idle, deltot_jiffies));
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display CPU statistics for NUMA nodes in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 ***************************************************************************
 */
void write_json_node_stats(int tab, unsigned long long deltot_jiffies,
			   int prev, int curr)
{
	struct stats_cpu *snc, *snp, *scc, *scp;
	int cpu, node, next = FALSE;
	char node_name[16];

	xprintf(tab++, "\"node-load\": [");

	for (node = 0; node <= node_nr + 1; node++) {

		snc = st_node[curr] + node;
		snp = st_node[prev] + node;

		/* Check if we want stats about this node */
		if (!(*(node_bitmap + (node >> 3)) & (1 << (node & 0x07))))
			continue;

		if (!cpu_per_node[node])
			/* No CPU in this node */
			continue;

		if (next) {
			printf(",\n");
		}
		next = TRUE;

		if (node == 0) {
			/* This is node "all", i.e. CPU "all" */
			strcpy(node_name, "all");
		}
		else {
			snprintf(node_name, 16, "%d", node - 1);
			node_name[15] = '\0';

			/* Recalculate interval for current node */
			deltot_jiffies = 0;
			for (cpu = 1; cpu <= cpu_nr; cpu++) {
				scc = st_cpu[curr] + cpu;
				scp = st_cpu[prev] + cpu;

				if ((scp->cpu_user + scp->cpu_nice + scp->cpu_sys +
				     scp->cpu_idle + scp->cpu_iowait + scp->cpu_hardirq +
				     scp->cpu_steal + scp->cpu_softirq == 0) && (interval != 0))
					continue;

				if (cpu2node[cpu - 1] == node - 1) {
					deltot_jiffies += get_per_cpu_interval(scc, scp);
				}
			}

			if (!deltot_jiffies) {
				/* All CPU in node are tickless and/or offline */
				xprintf0(tab, "{\"node\": \"%d\", \"usr\": 0.00, \"nice\": 0.00, \"sys\": 0.00, "
			      "\"iowait\": 0.00, \"irq\": 0.00, \"soft\": 0.00, \"steal\": 0.00, "
			      "\"guest\": 0.00, \"gnice\": 0.00, \"idle\": 100.00}", node - 1);

				continue;
			}
		}

		xprintf0(tab, "{\"node\": \"%s\", \"usr\": %.2f, \"nice\": %.2f, \"sys\": %.2f, "
			      "\"iowait\": %.2f, \"irq\": %.2f, \"soft\": %.2f, \"steal\": %.2f, "
			      "\"guest\": %.2f, \"gnice\": %.2f, \"idle\": %.2f}", node_name,
			 (snc->cpu_user - snc->cpu_guest) < (snp->cpu_user - snp->cpu_guest) ?
			 0.0 :
			 ll_sp_value(snp->cpu_user - snp->cpu_guest,
				     snc->cpu_user - snc->cpu_guest, deltot_jiffies),
			 (snc->cpu_nice - snc->cpu_guest_nice) < (snp->cpu_nice - snp->cpu_guest_nice) ?
			 0.0 :
			 ll_sp_value(snp->cpu_nice - snp->cpu_guest_nice,
				     snc->cpu_nice - snc->cpu_guest_nice, deltot_jiffies),
			 ll_sp_value(snp->cpu_sys,
				     snc->cpu_sys, deltot_jiffies),
			 ll_sp_value(snp->cpu_iowait,
				     snc->cpu_iowait, deltot_jiffies),
			 ll_sp_value(snp->cpu_hardirq,
				     snc->cpu_hardirq, deltot_jiffies),
			 ll_sp_value(snp->cpu_softirq,
				     snc->cpu_softirq, deltot_jiffies),
			 ll_sp_value(snp->cpu_steal,
				     snc->cpu_steal, deltot_jiffies),
			 ll_sp_value(snp->cpu_guest,
				     snc->cpu_guest, deltot_jiffies),
			 ll_sp_value(snp->cpu_guest_nice,
				     snc->cpu_guest_nice, deltot_jiffies),
			 (snc->cpu_idle < snp->cpu_idle) ?
			 0.0 :
			 ll_sp_value(snp->cpu_idle,
				     snc->cpu_idle, deltot_jiffies));
	}
	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display nodes statistics in plain or JSON format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @deltot_jiffies
 *		Number of jiffies spent on the interval by all processors.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @tab		Number of tabs to print (JSON format only).
 * @next	TRUE is a previous activity has been displayed (JSON format
 * 		only).
 ***************************************************************************
 */
void write_node_stats(int dis, unsigned long long deltot_jiffies, int prev, int curr,
		      char *prev_string, char *curr_string, int tab, int *next)
{
	if (!deltot_jiffies) {
		/* CPU "all" cannot be tickless */
		deltot_jiffies = 1;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		if (*next) {
			printf(",\n");
		}
		*next = TRUE;
		write_json_node_stats(tab, deltot_jiffies, prev, curr);
	}
	else {
		write_plain_node_stats(dis, deltot_jiffies, prev, curr,
				       prev_string, curr_string);
	}
}

/*
 ***************************************************************************
 * Display total number of interrupts per CPU in plain format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 ***************************************************************************
 */
void write_plain_isumcpu_stats(int dis, unsigned long long itv, int prev, int curr,
			       char *prev_string, char *curr_string)
{
	struct stats_cpu *scc, *scp;
	struct stats_irq *sic, *sip;
	unsigned long long pc_itv;
	int cpu;

	if (dis) {
		printf("\n%-11s  CPU    intr/s\n", prev_string);
		}

	if (*cpu_bitmap & 1) {
		printf("%-11s", curr_string);
		cprintf_in(IS_STR, " %s", " all", 0);
		/* Print total number of interrupts among all cpu */
		cprintf_f(NO_UNIT, 1, 9, 2,
			  S_VALUE(st_irq[prev]->irq_nr, st_irq[curr]->irq_nr, itv));
		printf("\n");
	}

	for (cpu = 1; cpu <= cpu_nr; cpu++) {

		sic = st_irq[curr] + cpu;
		sip = st_irq[prev] + cpu;

		scc = st_cpu[curr] + cpu;
		scp = st_cpu[prev] + cpu;

		/* Check if we want stats about this CPU */
		if (!(*(cpu_bitmap + (cpu >> 3)) & (1 << (cpu & 0x07))))
			continue;

		if ((scc->cpu_user    + scc->cpu_nice + scc->cpu_sys   +
		     scc->cpu_iowait  + scc->cpu_idle + scc->cpu_steal +
		     scc->cpu_hardirq + scc->cpu_softirq) == 0) {

			/* This is an offline CPU */
			continue;
		}

		printf("%-11s", curr_string);
		cprintf_in(IS_INT, " %4d", "", cpu - 1);

		/* Recalculate itv for current proc */
		pc_itv = get_per_cpu_interval(scc, scp);

		if (!pc_itv) {
			/* This is a tickless CPU: Value displayed is 0.00 */
			cprintf_f(NO_UNIT, 1, 9, 2, 0.0);
			printf("\n");
		}
		else {
			/* Display total number of interrupts for current CPU */
			cprintf_f(NO_UNIT, 1, 9, 2,
				  S_VALUE(sip->irq_nr, sic->irq_nr, itv));
			printf("\n");
		}
	}
}

/*
 ***************************************************************************
 * Display total number of interrupts per CPU in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 ***************************************************************************
 */
void write_json_isumcpu_stats(int tab, unsigned long long itv, int prev, int curr)
{
	struct stats_cpu *scc, *scp;
	struct stats_irq *sic, *sip;
	unsigned long long pc_itv;
	int cpu, next = FALSE;

	xprintf(tab++, "\"sum-interrupts\": [");

	if (*cpu_bitmap & 1) {

		next = TRUE;
		/* Print total number of interrupts among all cpu */
		xprintf0(tab, "{\"cpu\": \"all\", \"intr\": %.2f}",
			 S_VALUE(st_irq[prev]->irq_nr, st_irq[curr]->irq_nr, itv));
	}

	for (cpu = 1; cpu <= cpu_nr; cpu++) {

		sic = st_irq[curr] + cpu;
		sip = st_irq[prev] + cpu;

		scc = st_cpu[curr] + cpu;
		scp = st_cpu[prev] + cpu;

		/* Check if we want stats about this CPU */
		if (!(*(cpu_bitmap + (cpu >> 3)) & (1 << (cpu & 0x07))))
			continue;

		if (next) {
			printf(",\n");
		}
		next = TRUE;

		if ((scc->cpu_user    + scc->cpu_nice + scc->cpu_sys   +
		     scc->cpu_iowait  + scc->cpu_idle + scc->cpu_steal +
		     scc->cpu_hardirq + scc->cpu_softirq) == 0) {

			/* This is an offline CPU */
			continue;
		}

		/* Recalculate itv for current proc */
		pc_itv = get_per_cpu_interval(scc, scp);

		if (!pc_itv) {
			/* This is a tickless CPU: Value displayed is 0.00 */
			xprintf0(tab, "{\"cpu\": \"%d\", \"intr\": 0.00}",
				 cpu - 1);
		}
		else {
			/* Display total number of interrupts for current CPU */
			xprintf0(tab, "{\"cpu\": \"%d\", \"intr\": %.2f}",
				 cpu - 1,
				 S_VALUE(sip->irq_nr, sic->irq_nr, itv));
		}
	}
	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display total number of interrupts per CPU in plain or JSON format.
 *
 * IN:
 * @dis		TRUE if a header line must be printed.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @tab		Number of tabs to print (JSON format only).
 * @next	TRUE is a previous activity has been displayed (JSON format
 * 		only).
 ***************************************************************************
 */
void write_isumcpu_stats(int dis, unsigned long long itv, int prev, int curr,
		     char *prev_string, char *curr_string, int tab, int *next)
{
	if (DISPLAY_JSON_OUTPUT(flags)) {
		if (*next) {
			printf(",\n");
		}
		*next = TRUE;
		write_json_isumcpu_stats(tab, itv, prev, curr);
	}
	else {
		write_plain_isumcpu_stats(dis, itv, prev, curr, prev_string, curr_string);
	}
}

/*
 ***************************************************************************
 * Display interrupts statistics for each CPU in plain format.
 *
 * IN:
 * @st_ic	Array for per-CPU statistics.
 * @ic_nr	Number of interrupts (hard or soft) per CPU.
 * @dis		TRUE if a header line must be printed.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 ***************************************************************************
 */
void write_plain_irqcpu_stats(struct stats_irqcpu *st_ic[], int ic_nr, int dis,
			      unsigned long long itv, int prev, int curr,
			      char *prev_string, char *curr_string)
{
	struct stats_cpu *scc;
	int j = ic_nr, offset, cpu, colwidth[NR_IRQS];
	struct stats_irqcpu *p, *q, *p0, *q0;

	/*
	 * Check if number of interrupts has changed.
	 * If this is the case, the header line will be printed again.
	 * NB: A zero interval value indicates that we are
	 * displaying statistics since system startup.
	 */
	if (!dis && interval) {
		for (j = 0; j < ic_nr; j++) {
			p0 = st_ic[curr] + j;
			q0 = st_ic[prev] + j;
			if (strcmp(p0->irq_name, q0->irq_name))
				/*
				 * These are two different interrupts: The header must be displayed
				 * (maybe an interrupt has disappeared, or a new one has just been registered).
				 * Note that we compare even empty strings for the case where
				 * a disappearing interrupt would be the last one in the list.
				 */
				break;
		}
	}

	if (dis || (j < ic_nr)) {
		/* Print header */
		printf("\n%-11s  CPU", prev_string);
		for (j = 0; j < ic_nr; j++) {
			p0 = st_ic[curr] + j;
			if (p0->irq_name[0] == '\0')
				/* End of the list of interrupts */
				break;
			printf(" %8s/s", p0->irq_name);
		}
		printf("\n");
	}

	/* Calculate column widths */
	for (j = 0; j < ic_nr; j++) {
		p0 = st_ic[curr] + j;
		/*
		 * Width is IRQ name + 2 for the trailing "/s".
		 * Width is calculated even for "undefined" interrupts (with
		 * an empty irq_name string) to quiet code analysis tools.
		 */
		colwidth[j] = strlen(p0->irq_name) + 2;
		/*
		 * Normal space for printing a number is 11 chars
		 * (space + 10 digits including the period).
		 */
		if (colwidth[j] < 10) {
			colwidth[j] = 10;
		}
	}

	for (cpu = 1; cpu <= cpu_nr; cpu++) {

		scc = st_cpu[curr] + cpu;

		/*
		 * Check if we want stats about this CPU.
		 * CPU must have been explicitly selected using option -P,
		 * else we display every CPU.
		 */
		if (!(*(cpu_bitmap + (cpu >> 3)) & (1 << (cpu & 0x07))) && USE_OPTION_P(flags))
			continue;

		if ((scc->cpu_user    + scc->cpu_nice + scc->cpu_sys   +
		     scc->cpu_iowait  + scc->cpu_idle + scc->cpu_steal +
		     scc->cpu_hardirq + scc->cpu_softirq) == 0)
			/* Offline CPU found */
			continue;

		printf("%-11s", curr_string);
		cprintf_in(IS_INT, "  %3d", "", cpu - 1);

		for (j = 0; j < ic_nr; j++) {
			p0 = st_ic[curr] + j;	/* irq_name set only for CPU#0 */
			/*
			 * An empty string for irq_name means it is a remaining interrupt
			 * which is no longer used, for example because the
			 * number of interrupts has decreased in /proc/interrupts.
			 */
			if (p0->irq_name[0] == '\0')
				/* End of the list of interrupts */
				break;
			q0 = st_ic[prev] + j;
			offset = j;

			/*
			 * If we want stats for the time since system startup,
			 * we have p0->irq_name != q0->irq_name, since q0 structure
			 * is completely set to zero.
			 */
			if (strcmp(p0->irq_name, q0->irq_name) && interval) {
				/* Check if interrupt exists elsewhere in list */
				for (offset = 0; offset < ic_nr; offset++) {
					q0 = st_ic[prev] + offset;
					if (!strcmp(p0->irq_name, q0->irq_name))
						/* Interrupt found at another position */
						break;
				}
			}

			p = st_ic[curr] + (cpu - 1) * ic_nr + j;

			if (!strcmp(p0->irq_name, q0->irq_name) || !interval) {
				q = st_ic[prev] + (cpu - 1) * ic_nr + offset;
				cprintf_f(NO_UNIT, 1, colwidth[j], 2,
					  S_VALUE(q->interrupt, p->interrupt, itv));
			}
			else {
				/*
				 * Instead of printing "N/A", assume that previous value
				 * for this new interrupt was zero.
				 */
				cprintf_f(NO_UNIT, 1, colwidth[j], 2,
					  S_VALUE(0, p->interrupt, itv));
			}
		}
		printf("\n");
	}
}

/*
 ***************************************************************************
 * Display interrupts statistics for each CPU in JSON format.
 *
 * IN:
 * @tab		Number of tabs to print.
 * @st_ic	Array for per-CPU statistics.
 * @ic_nr	Number of interrupts (hard or soft) per CPU.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @type	Activity (M_D_IRQ_CPU or M_D_SOFTIRQS).
 ***************************************************************************
 */
void write_json_irqcpu_stats(int tab, struct stats_irqcpu *st_ic[], int ic_nr,
			     unsigned long long itv, int prev, int curr, int type)
{
	struct stats_cpu *scc;
	int j = ic_nr, offset, cpu;
	struct stats_irqcpu *p, *q, *p0, *q0;
	int nextcpu = FALSE, nextirq;

	if (type == M_D_IRQ_CPU) {
		xprintf(tab++, "\"individual-interrupts\": [");
	}
	else {
		xprintf(tab++, "\"soft-interrupts\": [");
	}

	for (cpu = 1; cpu <= cpu_nr; cpu++) {

		scc = st_cpu[curr] + cpu;

		/*
		 * Check if we want stats about this CPU.
		 * CPU must have been explicitly selected using option -P,
		 * else we display every CPU.
		 */
		if (!(*(cpu_bitmap + (cpu >> 3)) & (1 << (cpu & 0x07))) && USE_OPTION_P(flags))
			continue;

		if ((scc->cpu_user    + scc->cpu_nice + scc->cpu_sys   +
		     scc->cpu_iowait  + scc->cpu_idle + scc->cpu_steal +
		     scc->cpu_hardirq + scc->cpu_softirq) == 0)
			/* Offline CPU found */
			continue;

		if (nextcpu) {
			printf(",\n");
		}
		nextcpu = TRUE;
		nextirq = FALSE;
		xprintf(tab++, "{\"cpu\": \"%d\", \"intr\": [", cpu - 1);

		for (j = 0; j < ic_nr; j++) {

			p0 = st_ic[curr] + j;	/* irq_name set only for CPU#0 */
			/*
			 * An empty string for irq_name means it is a remaining interrupt
			 * which is no longer used, for example because the
			 * number of interrupts has decreased in /proc/interrupts.
			 */
			if (p0->irq_name[0] == '\0')
				/* End of the list of interrupts */
				break;
			q0 = st_ic[prev] + j;
			offset = j;

			if (nextirq) {
				printf(",\n");
			}
			nextirq = TRUE;

			/*
			 * If we want stats for the time since system startup,
			 * we have p0->irq_name != q0->irq_name, since q0 structure
			 * is completely set to zero.
			 */
			if (strcmp(p0->irq_name, q0->irq_name) && interval) {
				/* Check if interrupt exists elsewhere in list */
				for (offset = 0; offset < ic_nr; offset++) {
					q0 = st_ic[prev] + offset;
					if (!strcmp(p0->irq_name, q0->irq_name))
						/* Interrupt found at another position */
						break;
				}
			}

			p = st_ic[curr] + (cpu - 1) * ic_nr + j;

			if (!strcmp(p0->irq_name, q0->irq_name) || !interval) {
				q = st_ic[prev] + (cpu - 1) * ic_nr + offset;
				xprintf0(tab, "{\"name\": \"%s\", \"value\": %.2f}",
					 p0->irq_name,
					 S_VALUE(q->interrupt, p->interrupt, itv));
			}
			else {
				/*
				 * Instead of printing "N/A", assume that previous value
				 * for this new interrupt was zero.
				 */
				xprintf0(tab, "{\"name\": \"%s\", \"value\": %.2f}",
					 p0->irq_name,
					 S_VALUE(0, p->interrupt, itv));
			}
		}
		printf("\n");
		xprintf0(--tab, "] }");
	}
	printf("\n");
	xprintf0(--tab, "]");
}

/*
 ***************************************************************************
 * Display interrupts statistics for each CPU in plain or JSON format.
 *
 * IN:
 * @st_ic	Array for per-CPU statistics.
 * @ic_nr	Number of interrupts (hard or soft) per CPU.
 * @dis		TRUE if a header line must be printed.
 * @itv		Interval value.
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where current statistics will be saved.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 * @tab		Number of tabs to print (JSON format only).
 * @next	TRUE is a previous activity has been displayed (JSON format
 * 		only).
 * @type	Activity (M_D_IRQ_CPU or M_D_SOFTIRQS).
 ***************************************************************************
 */
void write_irqcpu_stats(struct stats_irqcpu *st_ic[], int ic_nr, int dis,
			unsigned long long itv, int prev, int curr,
			char *prev_string, char *curr_string, int tab,
			int *next, int type)
{
	if (DISPLAY_JSON_OUTPUT(flags)) {
		if (*next) {
			printf(",\n");
		}
		*next = TRUE;
		write_json_irqcpu_stats(tab, st_ic, ic_nr, itv, prev, curr, type);
	}
	else {
		write_plain_irqcpu_stats(st_ic, ic_nr, dis, itv, prev, curr,
					 prev_string, curr_string);
	}
}

/*
 ***************************************************************************
 * Core function used to display statistics.
 *
 * IN:
 * @prev	Position in array where statistics used	as reference are.
 *		Stats used as reference may be the previous ones read, or
 *		the very first ones when calculating the average.
 * @curr	Position in array where statistics for current sample are.
 * @dis		TRUE if a header line must be printed.
 * @prev_string	String displayed at the beginning of a header line. This is
 * 		the timestamp of the previous sample, or "Average" when
 * 		displaying average stats.
 * @curr_string	String displayed at the beginning of current sample stats.
 * 		This is the timestamp of the current sample, or "Average"
 * 		when displaying average stats.
 ***************************************************************************
 */
void write_stats_core(int prev, int curr, int dis,
		      char *prev_string, char *curr_string)
{
	unsigned long long itv, deltot_jiffies = 1;
	int tab = 4, next = FALSE;
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	/*
	 * Compute CPU "all" as sum of all individual CPU (on SMP machines)
	 * and look for offline CPU.
	 */
	deltot_jiffies = get_global_cpu_mpstats(prev, curr, offline_cpu_bitmap);

	if (DISPLAY_JSON_OUTPUT(flags)) {
		xprintf(tab++, "{");
		xprintf(tab, "\"timestamp\": \"%s\",", curr_string);
	}

	/* Get time interval */
	itv = get_interval(uptime_cs[prev], uptime_cs[curr]);

	/* Print CPU stats */
	if (DISPLAY_CPU(actflags)) {
		write_cpu_stats(dis, deltot_jiffies, prev, curr,
				prev_string, curr_string, tab, &next, offline_cpu_bitmap);
	}

	/* Print node CPU stats */
	if (DISPLAY_NODE(actflags)) {
		set_node_cpu_stats(prev, curr);
		write_node_stats(dis, deltot_jiffies, prev, curr, prev_string,
				 curr_string, tab, &next);
	}

	/* Print total number of interrupts per processor */
	if (DISPLAY_IRQ_SUM(actflags)) {
		write_isumcpu_stats(dis, itv, prev, curr, prev_string, curr_string,
				    tab, &next);
	}

	/* Display each interrupt value for each CPU */
	if (DISPLAY_IRQ_CPU(actflags)) {
		write_irqcpu_stats(st_irqcpu, irqcpu_nr, dis, itv, prev, curr,
				   prev_string, curr_string, tab, &next, M_D_IRQ_CPU);
	}
	if (DISPLAY_SOFTIRQS(actflags)) {
		write_irqcpu_stats(st_softirqcpu, softirqcpu_nr, dis, itv, prev, curr,
				   prev_string, curr_string, tab, &next, M_D_SOFTIRQS);
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		printf("\n");
		xprintf0(--tab, "}");
	}
}

/*
 ***************************************************************************
 * Print statistics average.
 *
 * IN:
 * @curr	Position in array where statistics for current sample are.
 * @dis		TRUE if a header line must be printed.
 ***************************************************************************
 */
void write_stats_avg(int curr, int dis)
{
	char string[16];

	strncpy(string, _("Average:"), 16);
	string[15] = '\0';
	write_stats_core(2, curr, dis, string, string);
}

/*
 ***************************************************************************
 * Print statistics.
 *
 * IN:
 * @curr	Position in array where statistics for current sample are.
 * @dis		TRUE if a header line must be printed.
 ***************************************************************************
 */
void write_stats(int curr, int dis)
{
	char cur_time[2][TIMESTAMP_LEN];

	/* Get previous timestamp */
	if (is_iso_time_fmt()) {
		strftime(cur_time[!curr], sizeof(cur_time[!curr]), "%H:%M:%S", &mp_tstamp[!curr]);
	}
	else {
		strftime(cur_time[!curr], sizeof(cur_time[!curr]), "%X", &(mp_tstamp[!curr]));
	}

	/* Get current timestamp */
	if (is_iso_time_fmt()) {
		strftime(cur_time[curr], sizeof(cur_time[curr]), "%H:%M:%S", &mp_tstamp[curr]);
	}
	else {
		strftime(cur_time[curr], sizeof(cur_time[curr]), "%X", &(mp_tstamp[curr]));
	}

	write_stats_core(!curr, curr, dis, cur_time[!curr], cur_time[curr]);
}

/*
 ***************************************************************************
 * Read stats from /proc/interrupts or /proc/softirqs.
 *
 * IN:
 * @file	/proc file to read (interrupts or softirqs).
 * @ic_nr	Number of interrupts (hard or soft) per CPU.
 * @curr	Position in array where current statistics will be saved.
 *
 * OUT:
 * @st_ic	Array for per-CPU interrupts statistics.
 ***************************************************************************
 */
void read_interrupts_stat(char *file, struct stats_irqcpu *st_ic[], int ic_nr, int curr)
{
	FILE *fp;
	struct stats_irq *st_irq_i;
	struct stats_irqcpu *p;
	char *line = NULL, *li;
	unsigned long irq = 0;
	unsigned int cpu;
	int cpu_index[cpu_nr], index = 0, len;
	char *cp, *next;

	/* Reset total number of interrupts received by each CPU */
	for (cpu = 0; cpu < cpu_nr; cpu++) {
		st_irq_i = st_irq[curr] + cpu + 1;
		st_irq_i->irq_nr = 0;
	}

	if ((fp = fopen(file, "r")) != NULL) {

		SREALLOC(line, char, INTERRUPTS_LINE + 11 * cpu_nr);

		/*
		 * Parse header line to see which CPUs are online
		 */
		while (fgets(line, INTERRUPTS_LINE + 11 * cpu_nr, fp) != NULL) {
			next = line;
			while (((cp = strstr(next, "CPU")) != NULL) && (index < cpu_nr)) {
				cpu = strtol(cp + 3, &next, 10);
				cpu_index[index++] = cpu;
			}
			if (index)
				/* Header line found */
				break;
		}

		/* Parse each line of interrupts statistics data */
		while ((fgets(line, INTERRUPTS_LINE + 11 * cpu_nr, fp) != NULL) &&
		       (irq < ic_nr)) {

			/* Skip over "<irq>:" */
			if ((cp = strchr(line, ':')) == NULL)
				/* Chr ':' not found */
				continue;
			cp++;

			p = st_ic[curr] + irq;

			/* Remove possible heading spaces in interrupt's name... */
			li = line;
			while (*li == ' ')
				li++;

			len = strcspn(li, ":");
			if (len >= MAX_IRQ_LEN) {
				len = MAX_IRQ_LEN - 1;
			}
			/* ...then save its name */
			strncpy(p->irq_name, li, len);
			p->irq_name[len] = '\0';

			/* For each interrupt: Get number received by each CPU */
			for (cpu = 0; cpu < index; cpu++) {
				p = st_ic[curr] + cpu_index[cpu] * ic_nr + irq;
				st_irq_i = st_irq[curr] + cpu_index[cpu] + 1;
				/*
				 * No need to set (st_irqcpu + cpu * irqcpu_nr)->irq_name:
				 * This is the same as st_irqcpu->irq_name.
				 * Now save current interrupt value for current CPU (in stats_irqcpu structure)
				 * and total number of interrupts received by current CPU (in stats_irq structure).
				 */
				p->interrupt = strtoul(cp, &next, 10);
				st_irq_i->irq_nr += p->interrupt;
				cp = next;
			}
			irq++;
		}

		fclose(fp);

		free(line);
	}

	while (irq < ic_nr) {
		/* Nb of interrupts per processor has changed */
		p = st_ic[curr] + irq;
		p->irq_name[0] = '\0';	/* This value means this is a dummy interrupt */
		irq++;
	}
}

/*
 ***************************************************************************
 * Main loop: Read stats from the relevant sources, and display them.
 *
 * IN:
 * @dis_hdr	Set to TRUE if the header line must always be printed.
 * @rows	Number of rows of screen.
 ***************************************************************************
 */
void rw_mpstat_loop(int dis_hdr, int rows)
{
	struct stats_cpu *scc;
	int i;
	int curr = 1, dis = 1;
	unsigned long lines = rows;

	/* Dont buffer data if redirected to a pipe */
	setbuf(stdout, NULL);

	/* Read system uptime and CPU stats */
	read_uptime(&(uptime_cs[0]));
	read_stat_cpu(st_cpu[0], cpu_nr + 1);

	/*
	 * Calculate global CPU stats as the sum of individual ones.
	 * Done only on SMP machines. On UP machines, we keep the values
	 * read from /proc/stat for global CPU stats.
	 */
	if (cpu_nr > 1) {
		memset(st_cpu[0], 0, STATS_CPU_SIZE);

		for (i = 1; i <= cpu_nr; i++) {
			scc = st_cpu[0] + i;

			st_cpu[0]->cpu_user += scc->cpu_user;
			st_cpu[0]->cpu_nice += scc->cpu_nice;
			st_cpu[0]->cpu_sys += scc->cpu_sys;
			st_cpu[0]->cpu_idle += scc->cpu_idle;
			st_cpu[0]->cpu_iowait += scc->cpu_iowait;
			st_cpu[0]->cpu_hardirq += scc->cpu_hardirq;
			st_cpu[0]->cpu_steal += scc->cpu_steal;
			st_cpu[0]->cpu_softirq += scc->cpu_softirq;
			st_cpu[0]->cpu_guest += scc->cpu_guest;
			st_cpu[0]->cpu_guest_nice += scc->cpu_guest_nice;
		}
	}

	/* Read system topology */
	if (DISPLAY_CPU(actflags) && DISPLAY_TOPOLOGY(flags)) {
		read_topology(cpu_nr, st_cpu_topology);
	}

	/*
	 * Read total number of interrupts received among all CPU.
	 * (this is the first value on the line "intr:" in the /proc/stat file).
	 */
	if (DISPLAY_IRQ_SUM(actflags)) {
		read_stat_irq(st_irq[0], 1);
	}

	/*
	 * Read number of interrupts received by each CPU, for each interrupt,
	 * and compute the total number of interrupts received by each CPU.
	 */
	if (DISPLAY_IRQ_SUM(actflags) || DISPLAY_IRQ_CPU(actflags)) {
		/* Read this file to display int per CPU or total nr of int per CPU */
		read_interrupts_stat(INTERRUPTS, st_irqcpu, irqcpu_nr, 0);
	}
	if (DISPLAY_SOFTIRQS(actflags)) {
		read_interrupts_stat(SOFTIRQS, st_softirqcpu, softirqcpu_nr, 0);
	}

	if (!interval) {
		/* Display since boot time */
		mp_tstamp[1] = mp_tstamp[0];
		memset(st_cpu[1], 0, STATS_CPU_SIZE * (cpu_nr + 1));
		memset(st_node[1], 0, STATS_CPU_SIZE * (cpu_nr + 1));
		memset(st_irq[1], 0, STATS_IRQ_SIZE * (cpu_nr + 1));
		memset(st_irqcpu[1], 0, STATS_IRQCPU_SIZE * (cpu_nr + 1) * irqcpu_nr);
		if (DISPLAY_SOFTIRQS(actflags)) {
			memset(st_softirqcpu[1], 0, STATS_IRQCPU_SIZE * (cpu_nr + 1) * softirqcpu_nr);
		}
		write_stats(0, DISP_HDR);
		if (DISPLAY_JSON_OUTPUT(flags)) {
			printf("\n\t\t\t]\n\t\t}\n\t]\n}}\n");
		}
		exit(0);
	}

	/* Set a handler for SIGALRM */
	memset(&alrm_act, 0, sizeof(alrm_act));
	alrm_act.sa_handler = alarm_handler;
	sigaction(SIGALRM, &alrm_act, NULL);
	alarm(interval);

	/* Save the first stats collected. Will be used to compute the average */
	mp_tstamp[2] = mp_tstamp[0];
	uptime_cs[2] = uptime_cs[0];
	memcpy(st_cpu[2], st_cpu[0], STATS_CPU_SIZE * (cpu_nr + 1));
	memcpy(st_node[2], st_node[0], STATS_CPU_SIZE * (cpu_nr + 1));
	memcpy(st_irq[2], st_irq[0], STATS_IRQ_SIZE * (cpu_nr + 1));
	memcpy(st_irqcpu[2], st_irqcpu[0], STATS_IRQCPU_SIZE * (cpu_nr + 1) * irqcpu_nr);
	if (DISPLAY_SOFTIRQS(actflags)) {
		memcpy(st_softirqcpu[2], st_softirqcpu[0],
		       STATS_IRQCPU_SIZE * (cpu_nr + 1) * softirqcpu_nr);
	}

	/* Set a handler for SIGINT */
	memset(&int_act, 0, sizeof(int_act));
	int_act.sa_handler = int_handler;
	sigaction(SIGINT, &int_act, NULL);

	__pause();

	if (sigint_caught)
		/* SIGINT signal caught during first interval: Exit immediately */
		return;

	do {
		/*
		 * Resetting the structure not needed since every fields will be set.
		 * Exceptions are per-CPU structures: Some of them may not be filled
		 * if corresponding processor is disabled (offline). We set them to zero
		 * to be able to distinguish between offline and tickless CPUs.
		 */
		memset(st_cpu[curr], 0, STATS_CPU_SIZE * (cpu_nr + 1));

		/* Get time */
		get_localtime(&(mp_tstamp[curr]), 0);

		/* Read uptime and CPU stats */
		read_uptime(&(uptime_cs[curr]));
		read_stat_cpu(st_cpu[curr], cpu_nr + 1);

		/* Read system topology */
		if (DISPLAY_CPU(actflags) && DISPLAY_TOPOLOGY(flags)) {
			read_topology(cpu_nr, st_cpu_topology);
		}

		/* Read total number of interrupts received among all CPU */
		if (DISPLAY_IRQ_SUM(actflags)) {
			read_stat_irq(st_irq[curr], 1);
		}

		/*
		 * Read number of interrupts received by each CPU, for each interrupt,
		 * and compute the total number of interrupts received by each CPU.
		 */
		if (DISPLAY_IRQ_SUM(actflags) || DISPLAY_IRQ_CPU(actflags)) {
			read_interrupts_stat(INTERRUPTS, st_irqcpu, irqcpu_nr, curr);
		}
		if (DISPLAY_SOFTIRQS(actflags)) {
			read_interrupts_stat(SOFTIRQS, st_softirqcpu, softirqcpu_nr, curr);
		}

		/* Write stats */
		if (!dis_hdr) {
			dis = lines / rows;
			if (dis) {
				lines %= rows;
			}
			lines++;
		}
		write_stats(curr, dis);

		if (count > 0) {
			count--;
		}

		if (count) {

			__pause();

			if (sigint_caught) {
				/* SIGINT signal caught => Display average stats */
				count = 0;
			}
			else {
				if (DISPLAY_JSON_OUTPUT(flags)) {
					printf(",\n");
				}
				curr ^= 1;
			}
		}
	}
	while (count);

	/* Write stats average */
	if (DISPLAY_JSON_OUTPUT(flags)) {
		printf("\n\t\t\t]\n\t\t}\n\t]\n}}\n");
	}
	else {
		write_stats_avg(curr, dis_hdr);
	}
}

/*
 ***************************************************************************
 * Main entry to the program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
	int opt = 0, i, actset = FALSE;
	struct utsname header;
	int dis_hdr = -1;
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

	/* What is the highest processor number on this machine? */
	cpu_nr = get_cpu_nr(~0, TRUE);

	/* Calculate number of interrupts per processor */
	irqcpu_nr = get_irqcpu_nr(INTERRUPTS, NR_IRQS, cpu_nr) +
		    NR_IRQCPU_PREALLOC;
	/* Calculate number of soft interrupts per processor */
	softirqcpu_nr = get_irqcpu_nr(SOFTIRQS, NR_IRQS, cpu_nr) +
			NR_IRQCPU_PREALLOC;

	/*
	 * cpu_nr: a value of 2 means there are 2 processors (0 and 1).
	 * In this case, we have to allocate 3 structures: global, proc0 and proc1.
	 */
	salloc_mp_struct(cpu_nr + 1);

	/* Get NUMA node placement */
	node_nr = get_node_placement(cpu_nr, cpu_per_node, cpu2node);

	while (++opt < argc) {

		if (!strncmp(argv[opt], "--dec=", 6) && (strlen(argv[opt]) == 7)) {
			/* Get number of decimal places */
			dplaces_nr = atoi(argv[opt] + 6);
			if ((dplaces_nr < 0) || (dplaces_nr > 2)) {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-I")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			actset = TRUE;

			for (t = strtok(argv[opt], ","); t; t = strtok(NULL, ",")) {
				if (!strcmp(t, K_SUM)) {
					/* Display total number of interrupts per CPU */
					actflags |= M_D_IRQ_SUM;
				}
				else if (!strcmp(t, K_CPU)) {
					/* Display interrupts per CPU */
					actflags |= M_D_IRQ_CPU;
				}
				else if (!strcmp(t, K_SCPU)) {
					/* Display soft interrupts per CPU */
					actflags |= M_D_SOFTIRQS;
				}
				else if (!strcmp(t, K_ALL)) {
					actflags |= M_D_IRQ_SUM + M_D_IRQ_CPU + M_D_SOFTIRQS;
				}
				else {
					usage(argv[0]);
				}
			}
		}

		else if (!strcmp(argv[opt], "-o")) {
			/* Select output format */
			if (argv[++opt] && !strcmp(argv[opt], K_JSON)) {
				flags |= F_JSON_OUTPUT;
			}
			else {
				usage(argv[0]);
			}
		}

		else if (!strcmp(argv[opt], "-N")) {
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			if (node_nr >= 0) {
				flags |= F_OPTION_N;
				actflags |= M_D_NODE;
				actset = TRUE;
				dis_hdr = 9;
				if (parse_values(argv[opt], node_bitmap, node_nr + 1, K_LOWERALL)) {
					usage(argv[0]);
				}
			}
		}

		else if (!strcmp(argv[opt], "-P")) {
			/* '-P ALL' can be used on UP machines */
			if (!argv[++opt]) {
				usage(argv[0]);
			}
			flags |= F_OPTION_P;
			dis_hdr = 9;

			if (parse_values(argv[opt], cpu_bitmap, cpu_nr, K_LOWERALL)) {
				usage(argv[0]);
			}
		}

		else if (!strncmp(argv[opt], "-", 1)) {
			for (i = 1; *(argv[opt] + i); i++) {

				switch (*(argv[opt] + i)) {

				case 'A':
					flags |= F_OPTION_A;
					actflags |= M_D_CPU + M_D_IRQ_SUM + M_D_IRQ_CPU + M_D_SOFTIRQS;
					if (node_nr >= 0) {
						actflags |= M_D_NODE;
					}
					actset = TRUE;
					break;

				case 'n':
					/* Display CPU stats based on NUMA node placement */
					if (node_nr >= 0) {
						actflags |= M_D_NODE;
						actset = TRUE;
					}
					break;

				case 'T':
					/* Display logical topology */
					flags |= F_TOPOLOGY;
					break;

				case 'u':
					/* Display CPU */
					actflags |= M_D_CPU;
					break;

				case 'V':
					/* Print version number */
					print_version();
					break;

				default:
					usage(argv[0]);
				}
			}
		}

		else if (interval < 0) {
			/* Get interval */
			if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
				usage(argv[0]);
			}
			interval = atol(argv[opt]);
			if (interval < 0) {
				usage(argv[0]);
			}
			count = -1;
		}

		else if (count <= 0) {
			/* Get count value */
			if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
			    !interval) {
				usage(argv[0]);
			}
			count = atol(argv[opt]);
			if (count < 1) {
				usage(argv[0]);
			}
		}

		else {
			usage(argv[0]);
		}
	}

	/* Default: Display CPU (e.g., "mpstat", "mpstat -P 1", "mpstat -P 1 -n", "mpstat -P 1 -N 1"... */
	if (!actset ||
	    (USE_OPTION_P(flags) && !(actflags & ~M_D_NODE))) {
		actflags |= M_D_CPU;
	}

	if (count_bits(&actflags, sizeof(unsigned int)) > 1) {
		dis_hdr = 9;
	}

	if (USE_OPTION_A(flags)) {
		/*
		 * Set -P ALL -N ALL only if individual CPU and/or nodes
		 * have not been selected.
		 */
		if ((node_nr >= 0) && !USE_OPTION_N(flags)) {
			memset(node_bitmap, ~0, ((cpu_nr + 1) >> 3) + 1);
			flags += F_OPTION_N;
		}
		if (!USE_OPTION_P(flags)) {
			memset(cpu_bitmap, ~0, ((cpu_nr + 1) >> 3) + 1);
			flags += F_OPTION_P;
		}
	}

	if (!USE_OPTION_P(flags)) {
		/* Option -P not used: Set bit 0 (global stats among all proc) */
		*cpu_bitmap = 1;
	}
	if (!USE_OPTION_N(flags)) {
		/* Option -N not used: Set bit 0 (global stats among all nodes) */
		*node_bitmap = 1;
	}
	if (dis_hdr < 0) {
		dis_hdr = 0;
	}
	if (!dis_hdr) {
		/* Get window size */
		rows = get_win_height();
	}
	if (interval < 0) {
		/* Interval not set => display stats since boot time */
		interval = 0;
	}

	if (DISPLAY_JSON_OUTPUT(flags)) {
		/* Use a decimal point to make JSON code compliant with RFC7159 */
		setlocale(LC_NUMERIC, "C");
	}

	/* Get time */
	get_localtime(&(mp_tstamp[0]), 0);

	/* Get system name, release number and hostname */
	__uname(&header);
	print_gal_header(&(mp_tstamp[0]), header.sysname, header.release,
			 header.nodename, header.machine, get_cpu_nr(~0, FALSE),
			 DISPLAY_JSON_OUTPUT(flags));

	/* Main loop */
	rw_mpstat_loop(dis_hdr, rows);

	/* Free structures */
	sfree_mp_struct();

	return 0;
}
