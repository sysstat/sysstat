/*
 * pcp_stats.h: Include file used to display system statistics in PCP format.
 * (C) 2019-2026 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PCP_STATS_H
#define _PCP_STATS_H

struct pcp_local_config;

/*
 ***************************************************************************
 * Functions used to display statistics in PCP format.
 * These use only sysstat types (__print_funct_t, struct activity) and are
 * always declared so activity.c can reference them unconditionally.
 * The implementations live in pcp_stats.o which is always linked.
 ***************************************************************************
 */

__print_funct_t pcp_print_cpu_stats
	(struct activity *, int);
__print_funct_t pcp_print_pcsw_stats
	(struct activity *, int);
__print_funct_t pcp_print_irq_stats
	(struct activity *, int);
__print_funct_t pcp_print_swap_stats
	(struct activity *, int);
__print_funct_t pcp_print_paging_stats
	(struct activity *, int);
__print_funct_t pcp_print_io_stats
	(struct activity *, int);
__print_funct_t pcp_print_memory_stats
	(struct activity *, int);
__print_funct_t pcp_print_ktables_stats
	(struct activity *, int);
__print_funct_t pcp_print_queue_stats
	(struct activity *, int);
__print_funct_t pcp_print_disk_stats
	(struct activity *, int);
__print_funct_t pcp_print_serial_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_dev_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_edev_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_nfs_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_nfsd_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_sock_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_ip_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_eip_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_icmp_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_eicmp_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_tcp_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_etcp_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_udp_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_sock6_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_ip6_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_eip6_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_icmp6_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_eicmp6_stats
	(struct activity *, int);
__print_funct_t pcp_print_net_udp6_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_cpufreq_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_fan_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_temp_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_in_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_bat_stats
	(struct activity *, int);
__print_funct_t pcp_print_huge_stats
	(struct activity *, int);
__print_funct_t pcp_print_pwr_usb_stats
	(struct activity *, int);
__print_funct_t pcp_print_filesystem_stats
	(struct activity *, int);
__print_funct_t pcp_print_fchost_stats
	(struct activity *, int);
__print_funct_t pcp_print_softnet_stats
	(struct activity *, int);
__print_funct_t pcp_print_psicpu_stats
	(struct activity *, int);
__print_funct_t pcp_print_psiio_stats
	(struct activity *, int);
__print_funct_t pcp_print_psimem_stats
	(struct activity *, int);

/*
 ***************************************************************************
 * Declarations that require PCP types (pmValueSet, pmResult, etc.).
 * Only visible when HAVE_PCP is defined; the header is self-contained.
 ***************************************************************************
 */

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>

/* Functions used to read from the PCP archive format */
unsigned long long pcp_read_u64(pmValueSet *, int, pmDesc *, int);
unsigned long pcp_read_u32(pmValueSet *, int, pmDesc *, int);
double pcp_read_double(pmValueSet *, int, pmDesc *, int);
float pcp_read_float(pmValueSet *, int, pmDesc *, int);
char *pcp_read_str(pmValueSet *, int, pmDesc *, int);

void pcp_read_stats(pmValueSet *, struct file_header *, int);

/* sadc self-description and event functions */
void pcp_register_sadc_metrics(void);
void pcp_write_import_metrics(const char *path, const struct file_header *hdr,
			      long interval_secs,
			      const struct pcp_local_config *local_cfg);
void pcp_register_import_program(const char *archive_path,
				 const struct pcp_local_config *local_cfg);
void pcp_write_sadc_special_record(const char *comment, unsigned int cpu_nr,
				   unsigned long long timestamp, unsigned int nsec,
				   const struct pcp_local_config *local_cfg);

/* Shared PCP archive reading helpers (used by sar and sadf) */
void check_pcpfile_actlist(const char *from_file, struct activity *act[], uint64_t flags);
int read_stats_from_result(pmResult *result, struct file_header *header, int curr);
void pcp_read_sadc_metrics(char **version, long *interval);

/* sadf->PCP write-path wrappers */
void pcp_write_activity_help(struct activity *act[], int nact);
void pcp_write_file_header_metrics(const struct file_header *hdr);
void pcp_write_inventory_metrics(__nr_t nr_disk, __nr_t nr_iface,
				 unsigned long long ust_time,
				 unsigned long long uptime_cs);
void pcp_write_sadf_sample(unsigned long long ust_time);
void pcp_open_sadf_archive(const char *dfile, const struct file_header *hdr);
void pcp_close_sadf_archive(unsigned long long ust_time);
void pcp_write_sadf_restart(const struct file_header *hdr, unsigned long long ust_time);
void pcp_write_sadf_comment(const char *comment, unsigned long long ust_time);
#endif /* HAVE_PCP */

/*
 ***************************************************************************
 * sadc direct-write wrappers.  Real implementations require PMI_APPEND;
 * stubs allow sadc.c to compile without #ifdef in the function bodies.
 ***************************************************************************
 */

#ifdef HAVE_PMI_APPEND
int  pcp_open_sadc_archive(const char *path, const struct file_header *hdr);
void pcp_write_uptime(unsigned long long uptime_cs);
int  pcp_write_sadc_sample(unsigned long long ust_time, unsigned int nsec,
			   uint64_t flags);
void pcp_sadc_set_volume_size(size_t volume_size);
void pcp_close_sadc_archive(void);
#else
static inline int  pcp_open_sadc_archive(const char *p __attribute__((unused)),
					 const struct file_header *h __attribute__((unused)))
	{ return -1; }
static inline int  pcp_write_sadc_sample(unsigned long long t __attribute__((unused)),
					 long n __attribute__((unused)),
					 uint64_t f __attribute__((unused)))
	{ return 0; }
static inline void pcp_sadc_set_volume_size(size_t s __attribute__((unused))) {}
static inline void pcp_close_sadc_archive(void) {}
#endif /* HAVE_PMI_APPEND */

#endif /* _PCP_STATS_H */
