/*
 * sa_conv.h: Include file for "sadf -c" command.
 * (C) 1999-2016 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SA_CONV_H
#define _SA_CONV_H

/*
 * Header structure for previous system activity data file format
 * (v9.1.6 up to v10.2.1).
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

/* Structure stats_io for ACTIVITY_MAGIC_BASE format */
struct stats_io_8a {
	unsigned int  dk_drive		__attribute__ ((aligned (4)));
	unsigned int  dk_drive_rio	__attribute__ ((packed));
	unsigned int  dk_drive_wio	__attribute__ ((packed));
	unsigned int  dk_drive_rblk	__attribute__ ((packed));
	unsigned int  dk_drive_wblk	__attribute__ ((packed));
};

/* Structure stats_queue for ACTIVITY_MAGIC_BASE format */
struct stats_queue_8a {
	unsigned long nr_running	__attribute__ ((aligned (8)));
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

#endif  /* _SA_CONV_H */
