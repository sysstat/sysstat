/*
 * pr_xstats.h: Include file used to display extended reports data.
 * (C) 2023 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PR_XSTATS_H
#define _PR_XSTATS_H

#include "common.h"


/*
 ***************************************************************************
 * Prototypes for functions used to display extended reports data
 ***************************************************************************
 */
void print_cpu_xstats
	(int, int, int, double *);
void print_genf_xstats
	(int, int, double *);
void print_genu64_xstats
	(int, int, double *);
void print_irq_xstats
	(int, struct activity *, int, int, char *, unsigned char [], double *);
void print_paging_xstats
	(int, double *);
void print_ram_memory_xstats
	(int, double *, int, int);
void print_swap_memory_xstats
	(int, double *, int, int);
void print_queue_xstats
	(int, double *);
void print_serial_xstats
	(int, char *, double *);
void print_disk_xstats
	(int, int, char *, double *);
void print_net_dev_xstats
	(int, int, char *, double *);
void print_net_edev_xstats
	(int, char *, double *);
void print_pwr_cpufreq_xstats
	(int, char *, double *);
void print_pwr_fan_xstats
	(int, int, char *, double *);
void print_pwr_sensor_xstats
	(int, int, char *, double *);
void print_huge_xstats
	(int, int, double *);
void print_pwr_wghfreq_xstats
	(int, int, double *);
void print_filesystem_xstats
	(int, int, char *, double *);
void print_fchost_xstats
	(int, char *, double *);
void print_softnet_xstats
	(int, int, double *);
void print_psi_xstats
	(int, int, double *);
void print_pwr_bat_xstats
	(int, char *, double *);

#endif /* _PR_XSTATS_H */
