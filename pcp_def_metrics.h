/*
 * pcp_def_metrics.h: Include file used to define PCP metrics.
 * (C) 2019-2022 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PCP_DEF_METRICS_H
#define _PCP_DEF_METRICS_H

/*
 ***************************************************************************
 * Prototypes for functions used to define PCP metrics.
 ***************************************************************************
 */

void pcp_def_cpu_metrics(struct activity *);
void pcp_def_pcsw_metrics(void);
void pcp_def_irq_metrics(struct activity *);
void pcp_def_swap_metrics(void);
void pcp_def_paging_metrics(void);
void pcp_def_io_metrics(void);
void pcp_def_memory_metrics(struct activity *);
void pcp_def_ktables_metrics(void);
void pcp_def_queue_metrics(void);
void pcp_def_serial_metrics(struct activity *);
void pcp_def_disk_metrics(struct activity *);
void pcp_def_net_dev_metrics(struct activity *);
void pcp_def_net_nfs_metrics(void);
void pcp_def_net_nfsd_metrics(void);
void pcp_def_net_sock_metrics(void);
void pcp_def_net_ip_metrics(void);
void pcp_def_net_eip_metrics(void);
void pcp_def_net_icmp_metrics(void);
void pcp_def_net_eicmp_metrics(void);
void pcp_def_net_tcp_metrics(void);
void pcp_def_net_etcp_metrics(void);
void pcp_def_net_udp_metrics(void);
void pcp_def_net_sock6_metrics(void);
void pcp_def_net_ip6_metrics(void);
void pcp_def_net_eip6_metrics(void);
void pcp_def_net_icmp6_metrics(void);
void pcp_def_net_eicmp6_metrics(void);
void pcp_def_net_udp6_metrics(void);
void pcp_def_huge_metrics(void);
void pcp_def_pwr_fan_metrics(struct activity *);
void pcp_def_pwr_temp_metrics(struct activity *);
void pcp_def_pwr_in_metrics(struct activity *);
void pcp_def_pwr_usb_metrics(struct activity *);
void pcp_def_filesystem_metrics(struct activity *);
void pcp_def_fchost_metrics(struct activity *);
void pcp_def_psi_metrics(struct activity *);

#endif /* _PCP_DEF_METRICS_H */
