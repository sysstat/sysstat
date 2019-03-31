/*
 * pcp_stats.h: Include file used to display system statistics in PCP format.
 * (C) 2019 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PCP_STATS_H
#define _PCP_STATS_H

/*
 ***************************************************************************
 * Prototypes for functions used to display system statistics in PCP format
 ***************************************************************************
 */

/* Functions used to display statistics in PCP format */
__print_funct_t pcp_print_cpu_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_pcsw_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_irq_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_swap_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_paging_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_io_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_memory_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_ktables_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_queue_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_serial_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_dev_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_edev_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_nfs_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_nfsd_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_ip_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_eip_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_icmp_stats
	(struct activity *, int, unsigned long long, struct record_header *);
__print_funct_t pcp_print_net_eicmp_stats
	(struct activity *, int, unsigned long long, struct record_header *);

#endif /* _PCP_STATS_H */
