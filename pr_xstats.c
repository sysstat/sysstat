/*
 * pr_xstats.c: Functions used by sar to display extended reports
 * (e.g. minimum and maximum values).
 * (C) 2023 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include "sa.h"

extern uint64_t flags;

/*
 * **************************************************************************
 * Display min or max values for CPU statistics.
 *
 * IN:
 * @display_cpu_def	TRUE if only main CPU metrics should be displayed.
 * @cpu			CPU number.
 * @ismax		TRUE: Display max header - FALSE: Display min header.
 * @spextr		Pointer on array with min or max values.
 ***************************************************************************
 */
void print_cpu_xstats(int display_cpu_def, int cpu, int ismax, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (cpu == 0) {
		/* This is CPU "all" */
		cprintf_in(IS_STR, " %s", "    all", 0);
	}
	else {
		cprintf_in(IS_INT, " %7d", "", cpu - 1);
	}

	if (display_cpu_def) {
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 5, 9, 2,
			    *spextr, *(spextr + 1), *(spextr + 2),
			    *(spextr + 3), *(spextr + 4));
		cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 2,
			    *(spextr + 9));
	}
	else {
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 9, 9, 2,
			    *spextr, *(spextr + 1), *(spextr + 2),
			    *(spextr + 3), *(spextr + 4), *(spextr + 5),
			    *(spextr + 6), *(spextr + 7), *(spextr + 8));
		cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 2,
			    *(spextr + 9));
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values (float values).
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @nr		Number of values to display.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_genf_xstats(int ismax, int nr, double *spextr)
{
	int i;

	/* Print min / max header */
	print_minmax(ismax);

	for (i = 0; i < nr; i++) {
		cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *(spextr + i));
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values (integer values).
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @nr		Number of values to display.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_genu64_xstats(int ismax, int nr, double *spextr)
{
	int i;

	/* Print min / max header */
	print_minmax(ismax);

	for (i = 0; i < nr; i++) {
		cprintf_u64(NO_UNIT, 1, 9, (unsigned long long) *(spextr + i));
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for interrupts statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @irq		Current interrupt number.
 * @name	Name of current interrupt number.
 * @masked_cpu_bitmap
 *		CPU bitmap for offline and unselected CPU.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_irq_xstats(int ismax, struct activity *a, int curr, int irq, char *name,
		      unsigned char masked_cpu_bitmap[], double *spextr)
{
	int cpu;

	/* Print min / max header */
	print_minmax(ismax);

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %9s", name, 0);
	}

	for (cpu = 0; (cpu < a->nr[curr]) && (cpu < a->bitmap->b_size + 1); cpu++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (masked_cpu_bitmap[cpu >> 3] & (1 << (cpu & 0x07)))
			/* No */
			continue;

		cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *(spextr + (cpu * a->nr2 + irq) * a->xnr));
	}

	if (DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %s", name, 0);
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for RAM memory utilization.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @spextr	Pointer on array with min or max values.
 * @unit	Default values unit.
 * @dispall	TRUE if all memory fields should be displayed.
 ***************************************************************************
 */
void print_ram_memory_xstats(int ismax, double *spextr, int unit, int dispall)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_u64(unit, 3, 9,
		    (unsigned long long) *spextr,
		    (unsigned long long) *(spextr + 1),
		    (unsigned long long) *(spextr + 2));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 3));
	cprintf_u64(unit, 3, 9,
		    (unsigned long long) *(spextr + 4),
		    (unsigned long long) *(spextr + 5),
		    (unsigned long long) *(spextr + 6));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 7));
	cprintf_u64(unit, 3, 9,
		    (unsigned long long) *(spextr + 8),
		    (unsigned long long) *(spextr + 9),
		    (unsigned long long) *(spextr + 10));

	if (dispall) {
		/* Display extended memory statistics */
		cprintf_u64(unit, 5, 9,
			    (unsigned long long) *(spextr + 11),
			    (unsigned long long) *(spextr + 12),
			    (unsigned long long) *(spextr + 13),
			    (unsigned long long) *(spextr + 14),
			    (unsigned long long) *(spextr + 15));
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for swap memory utilization.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @spextr	Pointer on array with min or max values.
 * @unit	Default values unit.
 * @dispall	TRUE if all memory fields should be displayed.
 ***************************************************************************
 */
void print_swap_memory_xstats(int ismax, double *spextr, int unit, int dispall)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_u64(unit, 2, 9,
		    (unsigned long long) *(spextr + 16),
		    (unsigned long long) *(spextr + 17));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 18));
	cprintf_u64(unit, 1, 9,
		    (unsigned long long) *(spextr + 19));
	cprintf_xpc(DISPLAY_UNIT(flags), FALSE, 1, 9, 2, *(spextr + 20));

	printf("\n");
}

/*
 * **************************************************************************
 * Display min and max values for queue and load statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_queue_xstats(int ismax, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_u64(NO_UNIT, 2, 9,
		    (unsigned long long) *spextr,
		    (unsigned long long) *(spextr + 1));
	cprintf_f(NO_UNIT, FALSE, 3, 9, 2,
		  (double) *(spextr + 2) / 100,
		  (double) *(spextr + 3) / 100,
		  (double) *(spextr + 4) / 100);
	cprintf_u64(NO_UNIT, 1, 9,
		    (unsigned long long) *(spextr + 5));

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for serial lines statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @name	Serial line name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_serial_xstats(int ismax, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_in(IS_INT, "       %3d", "", atoi(name));

	cprintf_f(NO_UNIT, FALSE, 6, 9, 2,
		  *spextr, *(spextr + 1), *(spextr + 2),
		  *(spextr + 3), *(spextr + 4), *(spextr + 5));

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for disks statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @unit	Unit used to display values.
 * @name	Disk name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_disk_xstats(int ismax, int unit, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %9s", name, 0);
	}

	cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *spextr);
	cprintf_f(unit, FALSE, 4, 9, 2,
		  *(spextr + 1), *(spextr + 2), *(spextr + 3), *(spextr + 4));
	cprintf_f(NO_UNIT, FALSE, 2, 9, 2, *(spextr + 5), *(spextr + 6));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 7));

	if (DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %s", name, 0);
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for network interfaces.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @unit	Unit used to display values.
 * @name	Network interface name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_net_dev_xstats(int ismax, int unit, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %9s", name, 0);
	}

	cprintf_f(NO_UNIT, FALSE, 2, 9, 2, *spextr, *(spextr + 1));
	cprintf_f(unit, FALSE, 2, 9, 2,
		  unit < 0 ? *(spextr + 2) / 1024 : *(spextr + 2),
		  unit < 0 ? *(spextr + 3) / 1024 : *(spextr + 3));
	cprintf_f(NO_UNIT, FALSE, 3, 9, 2,
		  *(spextr + 4), *(spextr + 5), *(spextr + 6));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 7));

	if (DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %s", name, 0);
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for network interfaces errors statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @name	Network interface name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_net_edev_xstats(int ismax, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (!DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %9s", name, 0);
	}

	cprintf_f(NO_UNIT, FALSE, 9, 9, 2,
		  *spextr, *(spextr + 1), *(spextr + 2),
		  *(spextr + 3), *(spextr + 4), *(spextr + 5),
		  *(spextr + 6), *(spextr + 7), *(spextr + 8));

	if (DISPLAY_PRETTY(flags)) {
		cprintf_in(IS_STR, " %s", name, 0);
	}

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for CPU frequency statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @name	CPU number
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_pwr_cpufreq_xstats(int ismax, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);
	cprintf_in(IS_STR, "%s", name, 0);

	cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *spextr);

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for fan statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @fan		Fan number.
 * @name	Device (fan) name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_pwr_fan_xstats(int ismax, int fan, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_in(IS_INT, "     %5d", "", fan + 1);
	cprintf_f(NO_UNIT, FALSE, 2, 9, 2, *spextr, *(spextr + 1));
	cprintf_in(IS_STR, " %s\n", name, 0);
}

/*
 * **************************************************************************
 * Display min or max values for device temperature statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @sensorid	Sensor id number.
 * @name	Device name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_pwr_sensor_xstats(int ismax, int sensorid, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_in(IS_INT, "     %5d", "", sensorid);
	cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *spextr);
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 1));
	cprintf_in(IS_STR, " %s\n", name, 0);
}

/*
 * **************************************************************************
 * Display min or max values for huge pages statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @unit	Unit used to display values.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_huge_xstats(int ismax, int unit, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_u64(unit, 2, 9,
		    (unsigned long long) *spextr,
		    (unsigned long long) *(spextr + 1));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 2));
	cprintf_u64(unit, 2, 9,
		    (unsigned long long) *(spextr + 3),
		    (unsigned long long) *(spextr + 4));

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for CPU weighted frequency statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @cpu		Current CPU number.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_pwr_wghfreq_xstats(int ismax, int cpu, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (!cpu) {
		/* This is CPU "all" */
		cprintf_in(IS_STR, "%s", "     all", 0);
	}
	else {
		cprintf_in(IS_INT, "     %3d", "", cpu - 1);
	}
	cprintf_f(NO_UNIT, FALSE, 1, 9, 2, *spextr);

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for filesystems statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @unit	Unit used to display values.
 * @name	Filesystem name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_filesystem_xstats(int ismax, int unit, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_f(unit, FALSE, 2, 9, 0, *spextr, *(spextr + 1));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 2, 9, 2,
		    *(spextr + 2), *(spextr + 3));
	cprintf_u64(NO_UNIT, 2, 9,
		    (unsigned long long) *(spextr + 4),
		    (unsigned long long) *(spextr + 5));
	cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + 6));

	cprintf_in(IS_STR, " %s\n", name, 0);
}

/*
 * **************************************************************************
 * Display min or max values for Fibre Channel HBA statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @name	FC name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_fchost_xstats(int ismax, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_f(NO_UNIT, FALSE, 4, 9, 2,
		  *spextr, *(spextr + 1), *(spextr + 2), *(spextr + 3));
	cprintf_in(IS_STR, " %s\n", name, 0);
}

/*
 * **************************************************************************
 * Display min or max values for softnet statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @cpu		CPU number.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_softnet_xstats(int ismax, int cpu, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	if (!cpu) {
		/* This is CPU "all" */
		cprintf_in(IS_STR, " %s", "    all", 0);
	}
	else {
		cprintf_in(IS_INT, " %7d", "", cpu - 1);
	}

	cprintf_f(NO_UNIT, FALSE, 5, 9, 2,
		  *spextr, *(spextr + 1), *(spextr + 2),
		  *(spextr + 3), *(spextr + 4));
	cprintf_u64(NO_UNIT, 1, 9,
		    (unsigned long long) *(spextr + 5));

	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for pressure-stall statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @nr		Number of values to display.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_psi_xstats(int ismax, int nr, double *spextr)
{
	int i;

	/* Print min / max header */
	print_minmax(ismax);

	for (i = 0; i < nr; i++) {
		cprintf_xpc(DISPLAY_UNIT(flags), XHIGH, 1, 9, 2, *(spextr + i));
	}
	printf("\n");
}

/*
 * **************************************************************************
 * Display min or max values for battery statistics.
 *
 * IN:
 * @ismax	TRUE: Display max header - FALSE: Display min header.
 * @name	Battery id name.
 * @spextr	Pointer on array with min or max values.
 ***************************************************************************
 */
void print_pwr_bat_xstats(int ismax, char *name, double *spextr)
{
	/* Print min / max header */
	print_minmax(ismax);

	cprintf_in(IS_INT, "     %5d", "", atoi(name));

	cprintf_xpc(DISPLAY_UNIT(flags), XLOW, 1, 9, 0, *spextr);
	cprintf_f(NO_UNIT, TRUE, 1, 9, 2, *(spextr + 1));

	printf("\n");
}
