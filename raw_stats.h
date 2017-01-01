/*
 * raw_stats.h: Include file used to display statistics in raw format.
 * (C) 1999-2017 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _RAW_STATS_H
#define _RAW_STATS_H

#include "common.h"

/*
 ***************************************************************************
 * Prototypes for functions used to display statistics in raw format.
 ***************************************************************************
 */

__print_funct_t raw_print_cpu_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_pcsw_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_irq_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_swap_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_paging_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_io_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_memory_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_ktables_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_queue_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_serial_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_disk_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_dev_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_edev_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_nfs_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_nfsd_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_sock_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_ip_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_eip_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_net_icmp_stats
	(struct activity *, char *, int);

#endif /* _RAW_STATS_H */
