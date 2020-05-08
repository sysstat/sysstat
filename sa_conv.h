/*
 * sa_conv.h: Include file for "sadf -c" command.
 * (C) 1999-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SA_CONV_H
#define _SA_CONV_H

/*
 * Header structure for previous system activity data file formats.
 * 2171: v9.1.6 -> 10.2.1
 * 2173: 10.3.1 -> 11.6.x
 */
struct file_header_2171 {
	unsigned long sa_ust_time	__attribute__ ((aligned (8)));
	unsigned int sa_act_nr		__attribute__ ((aligned (8)));
	unsigned char sa_day;
	unsigned char sa_month;
	unsigned char sa_year;
	char sa_sizeof_long;
	char sa_sysname[UTSNAME_LEN];
	char sa_nodename[UTSNAME_LEN];
	char sa_release[UTSNAME_LEN];
	char sa_machine[UTSNAME_LEN];
};

#define FILE_HEADER_SIZE_2171	(sizeof(struct file_header_2171))
#define FILE_HEADER_2171_ULL_NR	0
#define FILE_HEADER_2171_UL_NR	1
#define FILE_HEADER_2171_U_NR	1

struct file_header_2173 {
	unsigned long sa_ust_time	__attribute__ ((aligned (8)));
	unsigned int sa_last_cpu_nr	__attribute__ ((aligned (8)));
	unsigned int sa_act_nr;
	unsigned int sa_vol_act_nr;
	unsigned char sa_day;
	unsigned char sa_month;
	unsigned char sa_year;
	char sa_sizeof_long;
	char sa_sysname[UTSNAME_LEN];
	char sa_nodename[UTSNAME_LEN];
	char sa_release[UTSNAME_LEN];
	char sa_machine[UTSNAME_LEN];
};

#define FILE_HEADER_2173_ULL_NR	0
#define FILE_HEADER_2173_UL_NR	1
#define FILE_HEADER_2173_U_NR	3

/* file_activity structure for versions older than 11.7.1 */
struct old_file_activity {
	unsigned int id;
	unsigned int magic;
	int nr;
	int nr2;
	int size;
};

#define OLD_FILE_ACTIVITY_SIZE	(sizeof(struct old_file_activity))
#define OLD_FILE_ACTIVITY_ULL_NR	0
#define OLD_FILE_ACTIVITY_UL_NR		0
#define OLD_FILE_ACTIVITY_U_NR		5

/* File record header structure for versions older than 11.7.1 */
struct old_record_header {
	unsigned long long uptime	__attribute__ ((aligned (16)));
	unsigned long long uptime0	__attribute__ ((aligned (16)));
	unsigned long ust_time		__attribute__ ((aligned (16)));
	unsigned char record_type	__attribute__ ((aligned (8)));
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
};

#define OLD_RECORD_HEADER_SIZE	(sizeof(struct old_record_header))
#define OLD_RECORD_HEADER_ULL_NR	2
#define OLD_RECORD_HEADER_UL_NR		1
#define OLD_RECORD_HEADER_U_NR		0

/* Structure stats_cpu for ACTIVITY_MAGIC_BASE format */
struct stats_cpu_8a {
	unsigned long long cpu_user		__attribute__ ((aligned (16)));
	unsigned long long cpu_nice		__attribute__ ((aligned (16)));
	unsigned long long cpu_sys		__attribute__ ((aligned (16)));
	unsigned long long cpu_idle		__attribute__ ((aligned (16)));
	unsigned long long cpu_iowait		__attribute__ ((aligned (16)));
	unsigned long long cpu_steal		__attribute__ ((aligned (16)));
	unsigned long long cpu_hardirq		__attribute__ ((aligned (16)));
	unsigned long long cpu_softirq		__attribute__ ((aligned (16)));
	unsigned long long cpu_guest		__attribute__ ((aligned (16)));
	unsigned long long cpu_guest_nice	__attribute__ ((aligned (16)));
};

#define STATS_CPU_8A_SIZE	(sizeof(struct stats_cpu_8a))

/* Structure stats_pcsw for ACTIVITY_MAGIC_BASE format */
struct stats_pcsw_8a {
	unsigned long long context_switch	__attribute__ ((aligned (16)));
	unsigned long processes			__attribute__ ((aligned (16)));
};

/* Structure stats_irq for ACTIVITY_MAGIC_BASE format */
struct stats_irq_8a {
	unsigned long long irq_nr	__attribute__ ((aligned (16)));
};

/* Structure stats_io for ACTIVITY_MAGIC_BASE format */
struct stats_io_8a {
	unsigned int dk_drive		__attribute__ ((aligned (4)));
	unsigned int dk_drive_rio	__attribute__ ((packed));
	unsigned int dk_drive_wio	__attribute__ ((packed));
	unsigned int dk_drive_rblk	__attribute__ ((packed));
	unsigned int dk_drive_wblk	__attribute__ ((packed));
};

/* Structure stats_memory for ACTIVITY_MAGIC_BASE format */
struct stats_memory_8a {
	unsigned long frmkb		__attribute__ ((aligned (8)));
	unsigned long bufkb		__attribute__ ((aligned (8)));
	unsigned long camkb		__attribute__ ((aligned (8)));
	unsigned long tlmkb		__attribute__ ((aligned (8)));
	unsigned long frskb		__attribute__ ((aligned (8)));
	unsigned long tlskb		__attribute__ ((aligned (8)));
	unsigned long caskb		__attribute__ ((aligned (8)));
	unsigned long comkb		__attribute__ ((aligned (8)));
	unsigned long activekb		__attribute__ ((aligned (8)));
	unsigned long inactkb		__attribute__ ((aligned (8)));
#define STATS_MEMORY_8A_1_SIZE	80
	unsigned long dirtykb		__attribute__ ((aligned (8)));
#define STATS_MEMORY_8A_2_SIZE	88
	unsigned long anonpgkb		__attribute__ ((aligned (8)));
	unsigned long slabkb		__attribute__ ((aligned (8)));
	unsigned long kstackkb		__attribute__ ((aligned (8)));
	unsigned long pgtblkb		__attribute__ ((aligned (8)));
	unsigned long vmusedkb		__attribute__ ((aligned (8)));
#define STATS_MEMORY_8A_3_SIZE	128
	unsigned long availablekb	__attribute__ ((aligned (8)));
};

#define STATS_MEMORY_8A_SIZE	(sizeof(struct stats_memory_8a))

/* Structure stats_ktables for ACTIVITY_MAGIC_BASE */
struct stats_ktables_8a {
	unsigned int file_used		__attribute__ ((aligned (4)));
	unsigned int inode_used		__attribute__ ((packed));
	unsigned int dentry_stat	__attribute__ ((packed));
	unsigned int pty_nr		__attribute__ ((packed));
};

/* Structure stats_queue for ACTIVITY_MAGIC_BASE format */
struct stats_queue_8a {
	unsigned long nr_running	__attribute__ ((aligned (8)));
	unsigned int  load_avg_1	__attribute__ ((aligned (8)));
	unsigned int  load_avg_5	__attribute__ ((packed));
	unsigned int  load_avg_15	__attribute__ ((packed));
	unsigned int  nr_threads	__attribute__ ((packed));
};

/* Structure stats_queue for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_queue_8b {
	unsigned long nr_running	__attribute__ ((aligned (8)));
	unsigned long procs_blocked	__attribute__ ((aligned (8)));
	unsigned int  load_avg_1	__attribute__ ((aligned (8)));
	unsigned int  load_avg_5	__attribute__ ((packed));
	unsigned int  load_avg_15	__attribute__ ((packed));
	unsigned int  nr_threads	__attribute__ ((packed));
};

/* Structure stats_disk for ACTIVITY_MAGIC_BASE format */
struct stats_disk_8a {
	unsigned long long rd_sect	__attribute__ ((aligned (16)));
	unsigned long long wr_sect	__attribute__ ((aligned (16)));
	unsigned long rd_ticks		__attribute__ ((aligned (16)));
	unsigned long wr_ticks		__attribute__ ((aligned (8)));
	unsigned long tot_ticks		__attribute__ ((aligned (8)));
	unsigned long rq_ticks		__attribute__ ((aligned (8)));
	unsigned long nr_ios		__attribute__ ((aligned (8)));
	unsigned int  major		__attribute__ ((aligned (8)));
	unsigned int  minor		__attribute__ ((packed));
};

/* Structure stats_disk for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_disk_8b {
	unsigned long long nr_ios	__attribute__ ((aligned (16)));
	unsigned long rd_sect		__attribute__ ((aligned (16)));
	unsigned long wr_sect		__attribute__ ((aligned (8)));
	unsigned int rd_ticks		__attribute__ ((aligned (8)));
	unsigned int wr_ticks		__attribute__ ((packed));
	unsigned int tot_ticks		__attribute__ ((packed));
	unsigned int rq_ticks		__attribute__ ((packed));
	unsigned int major		__attribute__ ((packed));
	unsigned int minor		__attribute__ ((packed));
};

/* Structure stats_net_dev for ACTIVITY_MAGIC_BASE format */
struct stats_net_dev_8a {
	unsigned long rx_packets		__attribute__ ((aligned (8)));
	unsigned long tx_packets		__attribute__ ((aligned (8)));
	unsigned long rx_bytes			__attribute__ ((aligned (8)));
	unsigned long tx_bytes			__attribute__ ((aligned (8)));
	unsigned long rx_compressed		__attribute__ ((aligned (8)));
	unsigned long tx_compressed		__attribute__ ((aligned (8)));
	unsigned long multicast			__attribute__ ((aligned (8)));
	char          interface[MAX_IFACE_LEN]	__attribute__ ((aligned (8)));
};

/* Structure stats_net_dev for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_dev_8b {
	unsigned long long rx_packets		__attribute__ ((aligned (16)));
	unsigned long long tx_packets		__attribute__ ((aligned (16)));
	unsigned long long rx_bytes		__attribute__ ((aligned (16)));
	unsigned long long tx_bytes		__attribute__ ((aligned (16)));
	unsigned long long rx_compressed	__attribute__ ((aligned (16)));
	unsigned long long tx_compressed	__attribute__ ((aligned (16)));
	unsigned long long multicast		__attribute__ ((aligned (16)));
	char          interface[MAX_IFACE_LEN]	__attribute__ ((aligned (16)));
};

/* Structure stats_net_dev for ACTIVITY_MAGIC_BASE + 2 format */
struct stats_net_dev_8c {
	unsigned long long rx_packets		__attribute__ ((aligned (16)));
	unsigned long long tx_packets		__attribute__ ((aligned (16)));
	unsigned long long rx_bytes		__attribute__ ((aligned (16)));
	unsigned long long tx_bytes		__attribute__ ((aligned (16)));
	unsigned long long rx_compressed	__attribute__ ((aligned (16)));
	unsigned long long tx_compressed	__attribute__ ((aligned (16)));
	unsigned long long multicast		__attribute__ ((aligned (16)));
	unsigned int       speed		__attribute__ ((aligned (16)));
	char 	 interface[MAX_IFACE_LEN]	__attribute__ ((aligned (4)));
	char	 duplex;
};

/* Structure stats_net_edev for ACTIVITY_MAGIC_BASE format */
struct stats_net_edev_8a {
	unsigned long collisions		__attribute__ ((aligned (8)));
	unsigned long rx_errors			__attribute__ ((aligned (8)));
	unsigned long tx_errors			__attribute__ ((aligned (8)));
	unsigned long rx_dropped		__attribute__ ((aligned (8)));
	unsigned long tx_dropped		__attribute__ ((aligned (8)));
	unsigned long rx_fifo_errors		__attribute__ ((aligned (8)));
	unsigned long tx_fifo_errors		__attribute__ ((aligned (8)));
	unsigned long rx_frame_errors		__attribute__ ((aligned (8)));
	unsigned long tx_carrier_errors		__attribute__ ((aligned (8)));
	char          interface[MAX_IFACE_LEN]	__attribute__ ((aligned (8)));
};

/* Structure stats_net_edev for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_edev_8b {
	unsigned long long collisions		__attribute__ ((aligned (16)));
	unsigned long long rx_errors		__attribute__ ((aligned (16)));
	unsigned long long tx_errors		__attribute__ ((aligned (16)));
	unsigned long long rx_dropped		__attribute__ ((aligned (16)));
	unsigned long long tx_dropped		__attribute__ ((aligned (16)));
	unsigned long long rx_fifo_errors	__attribute__ ((aligned (16)));
	unsigned long long tx_fifo_errors	__attribute__ ((aligned (16)));
	unsigned long long rx_frame_errors	__attribute__ ((aligned (16)));
	unsigned long long tx_carrier_errors	__attribute__ ((aligned (16)));
	char	      interface[MAX_IFACE_LEN]	__attribute__ ((aligned (16)));
};

/* Structure stats_net_ip for ACTIVITY_MAGIC_BASE format */
struct stats_net_ip_8a {
	unsigned long InReceives	__attribute__ ((aligned (8)));
	unsigned long ForwDatagrams	__attribute__ ((aligned (8)));
	unsigned long InDelivers	__attribute__ ((aligned (8)));
	unsigned long OutRequests	__attribute__ ((aligned (8)));
	unsigned long ReasmReqds	__attribute__ ((aligned (8)));
	unsigned long ReasmOKs		__attribute__ ((aligned (8)));
	unsigned long FragOKs		__attribute__ ((aligned (8)));
	unsigned long FragCreates	__attribute__ ((aligned (8)));
};

/* Structure stats_net_ip for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_ip_8b {
	unsigned long long InReceives		__attribute__ ((aligned (16)));
	unsigned long long ForwDatagrams	__attribute__ ((aligned (16)));
	unsigned long long InDelivers		__attribute__ ((aligned (16)));
	unsigned long long OutRequests		__attribute__ ((aligned (16)));
	unsigned long long ReasmReqds		__attribute__ ((aligned (16)));
	unsigned long long ReasmOKs		__attribute__ ((aligned (16)));
	unsigned long long FragOKs		__attribute__ ((aligned (16)));
	unsigned long long FragCreates		__attribute__ ((aligned (16)));
};

/* Structure stats_net_eip for ACTIVITY_MAGIC_BASE format */
struct stats_net_eip_8a {
	unsigned long InHdrErrors	__attribute__ ((aligned (8)));
	unsigned long InAddrErrors	__attribute__ ((aligned (8)));
	unsigned long InUnknownProtos	__attribute__ ((aligned (8)));
	unsigned long InDiscards	__attribute__ ((aligned (8)));
	unsigned long OutDiscards	__attribute__ ((aligned (8)));
	unsigned long OutNoRoutes	__attribute__ ((aligned (8)));
	unsigned long ReasmFails	__attribute__ ((aligned (8)));
	unsigned long FragFails		__attribute__ ((aligned (8)));
};

/* Structure stats_net_eip for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_eip_8b {
	unsigned long long InHdrErrors		__attribute__ ((aligned (16)));
	unsigned long long InAddrErrors		__attribute__ ((aligned (16)));
	unsigned long long InUnknownProtos	__attribute__ ((aligned (16)));
	unsigned long long InDiscards		__attribute__ ((aligned (16)));
	unsigned long long OutDiscards		__attribute__ ((aligned (16)));
	unsigned long long OutNoRoutes		__attribute__ ((aligned (16)));
	unsigned long long ReasmFails		__attribute__ ((aligned (16)));
	unsigned long long FragFails		__attribute__ ((aligned (16)));
};

/* Structure stats_net_ip6 for ACTIVITY_MAGIC_BASE format */
struct stats_net_ip6_8a {
	unsigned long InReceives6	__attribute__ ((aligned (8)));
	unsigned long OutForwDatagrams6	__attribute__ ((aligned (8)));
	unsigned long InDelivers6	__attribute__ ((aligned (8)));
	unsigned long OutRequests6	__attribute__ ((aligned (8)));
	unsigned long ReasmReqds6	__attribute__ ((aligned (8)));
	unsigned long ReasmOKs6		__attribute__ ((aligned (8)));
	unsigned long InMcastPkts6	__attribute__ ((aligned (8)));
	unsigned long OutMcastPkts6	__attribute__ ((aligned (8)));
	unsigned long FragOKs6		__attribute__ ((aligned (8)));
	unsigned long FragCreates6	__attribute__ ((aligned (8)));
};

/* Structure stats_net_ip6 for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_ip6_8b {
	unsigned long long InReceives6		__attribute__ ((aligned (16)));
	unsigned long long OutForwDatagrams6	__attribute__ ((aligned (16)));
	unsigned long long InDelivers6		__attribute__ ((aligned (16)));
	unsigned long long OutRequests6		__attribute__ ((aligned (16)));
	unsigned long long ReasmReqds6		__attribute__ ((aligned (16)));
	unsigned long long ReasmOKs6		__attribute__ ((aligned (16)));
	unsigned long long InMcastPkts6		__attribute__ ((aligned (16)));
	unsigned long long OutMcastPkts6	__attribute__ ((aligned (16)));
	unsigned long long FragOKs6		__attribute__ ((aligned (16)));
	unsigned long long FragCreates6		__attribute__ ((aligned (16)));
};

/* Structure stats_net_eip6 for ACTIVITY_MAGIC_BASE format */
struct stats_net_eip6_8a {
        unsigned long InHdrErrors6      __attribute__ ((aligned (8)));
        unsigned long InAddrErrors6     __attribute__ ((aligned (8)));
        unsigned long InUnknownProtos6  __attribute__ ((aligned (8)));
        unsigned long InTooBigErrors6   __attribute__ ((aligned (8)));
        unsigned long InDiscards6       __attribute__ ((aligned (8)));
        unsigned long OutDiscards6      __attribute__ ((aligned (8)));
        unsigned long InNoRoutes6       __attribute__ ((aligned (8)));
        unsigned long OutNoRoutes6      __attribute__ ((aligned (8)));
        unsigned long ReasmFails6       __attribute__ ((aligned (8)));
        unsigned long FragFails6        __attribute__ ((aligned (8)));
        unsigned long InTruncatedPkts6  __attribute__ ((aligned (8)));
};

/* Structure stats_net_eip6 for ACTIVITY_MAGIC_BASE + 1 format */
struct stats_net_eip6_8b {
	unsigned long long InHdrErrors6		__attribute__ ((aligned (16)));
	unsigned long long InAddrErrors6	__attribute__ ((aligned (16)));
	unsigned long long InUnknownProtos6	__attribute__ ((aligned (16)));
	unsigned long long InTooBigErrors6	__attribute__ ((aligned (16)));
	unsigned long long InDiscards6		__attribute__ ((aligned (16)));
	unsigned long long OutDiscards6		__attribute__ ((aligned (16)));
	unsigned long long InNoRoutes6		__attribute__ ((aligned (16)));
	unsigned long long OutNoRoutes6		__attribute__ ((aligned (16)));
	unsigned long long ReasmFails6		__attribute__ ((aligned (16)));
	unsigned long long FragFails6		__attribute__ ((aligned (16)));
	unsigned long long InTruncatedPkts6	__attribute__ ((aligned (16)));
};

/* Structure stats_huge for ACTIVITY_MAGIC_BASE */
struct stats_huge_8a {
	unsigned long frhkb	__attribute__ ((aligned (8)));
	unsigned long tlhkb	__attribute__ ((aligned (8)));
};

/* Structure stats_pwr_wghfreq for ACTIVITY_MAGIC_BASE */
struct stats_pwr_wghfreq_8a {
	unsigned long long time_in_state	__attribute__ ((aligned (16)));
	unsigned long      freq			__attribute__ ((aligned (16)));
};

/* Structure stats_filesystem for ACTIVITY_MAGIC_BASE */
struct stats_filesystem_8a {
	unsigned long long f_blocks		__attribute__ ((aligned (16)));
	unsigned long long f_bfree		__attribute__ ((aligned (16)));
	unsigned long long f_bavail		__attribute__ ((aligned (16)));
	unsigned long long f_files		__attribute__ ((aligned (16)));
	unsigned long long f_ffree		__attribute__ ((aligned (16)));
	char 		   fs_name[MAX_FS_LEN]	__attribute__ ((aligned (16)));
#define STATS_FILESYSTEM_8A_1_SIZE	160
	char 		   mountp[MAX_FS_LEN];
};

#endif  /* _SA_CONV_H */
