/*
 * rd_stats.h: Include file used to read system statistics
 * (C) 1999-2017 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _RD_STATS_H
#define _RD_STATS_H


/*
 ***************************************************************************
 * Miscellaneous constants
 ***************************************************************************
 */

/* Get IFNAMSIZ */
#include <net/if.h>
#ifndef IFNAMSIZ
#define IFNAMSIZ	16
#endif

/* Maximum length of network interface name */
#define MAX_IFACE_LEN	IFNAMSIZ
/* Maximum length of USB manufacturer string */
#define MAX_MANUF_LEN	24
/* Maximum length of USB product string */
#define MAX_PROD_LEN	48
/* Maximum length of filesystem name */
#define MAX_FS_LEN	128
/* Maximum length of FC host name */
#define MAX_FCH_LEN	16

#define CNT_PART	1
#define CNT_ALL_DEV	0
#define CNT_USED_DEV	1

#define K_DUPLEX_HALF	"half"
#define K_DUPLEX_FULL	"full"

#define C_DUPLEX_HALF	1
#define C_DUPLEX_FULL	2

/*
 ***************************************************************************
 * System files containing statistics
 ***************************************************************************
 */

/* Files */
#define PROC		"/proc"
#define SERIAL		"/proc/tty/driver/serial"
#define FDENTRY_STATE	"/proc/sys/fs/dentry-state"
#define FFILE_NR	"/proc/sys/fs/file-nr"
#define FINODE_STATE	"/proc/sys/fs/inode-state"
#define PTY_NR		"/proc/sys/kernel/pty/nr"
#define NET_DEV		"/proc/net/dev"
#define NET_SOCKSTAT	"/proc/net/sockstat"
#define NET_SOCKSTAT6	"/proc/net/sockstat6"
#define NET_RPC_NFS	"/proc/net/rpc/nfs"
#define NET_RPC_NFSD	"/proc/net/rpc/nfsd"
#define NET_SOFTNET	"/proc/net/softnet_stat"
#define LOADAVG		"/proc/loadavg"
#define VMSTAT		"/proc/vmstat"
#define NET_SNMP	"/proc/net/snmp"
#define NET_SNMP6	"/proc/net/snmp6"
#define CPUINFO		"/proc/cpuinfo"
#define MTAB		"/etc/mtab"
#define IF_DUPLEX	"/sys/class/net/%s/duplex"
#define IF_SPEED	"/sys/class/net/%s/speed"
#define FC_RX_FRAMES	"%s/%s/statistics/rx_frames"
#define FC_TX_FRAMES	"%s/%s/statistics/tx_frames"
#define FC_RX_WORDS	"%s/%s/statistics/rx_words"
#define FC_TX_WORDS	"%s/%s/statistics/tx_words"

/*
 ***************************************************************************
 * Definitions of structures for system statistics
 ***************************************************************************
 */

#define SIZEOF_LONG_64BIT	8
#define ULL_ALIGNMENT_WIDTH	8
#define UL_ALIGNMENT_WIDTH	SIZEOF_LONG_64BIT
#define U_ALIGNMENT_WIDTH	4

#define MAP_SIZE(m)	((m[0] * ULL_ALIGNMENT_WIDTH) + \
			 (m[1] * UL_ALIGNMENT_WIDTH) +  \
			 (m[2] * U_ALIGNMENT_WIDTH))

/*
 * Structure for CPU statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_cpu {
	unsigned long long cpu_user;
	unsigned long long cpu_nice;
	unsigned long long cpu_sys;
	unsigned long long cpu_idle;
	unsigned long long cpu_iowait;
	unsigned long long cpu_steal;
	unsigned long long cpu_hardirq;
	unsigned long long cpu_softirq;
	unsigned long long cpu_guest;
	unsigned long long cpu_guest_nice;
};

#define STATS_CPU_SIZE	(sizeof(struct stats_cpu))

/*
 * Structure for task creation and context switch statistics.
 * The attribute (aligned(16)) is necessary so that sizeof(structure) has
 * the same value on 32 and 64-bit architectures.
 */
struct stats_pcsw {
	unsigned long long context_switch;
	unsigned long processes			__attribute__ ((aligned (8)));
};

#define STATS_PCSW_SIZE	(sizeof(struct stats_pcsw))

/*
 * Structure for interrupts statistics.
 * In activity buffer: First structure is for total number of interrupts ("SUM").
 * Following structures are for each individual interrupt (0, 1, etc.)
 *
 * NOTE: The total number of interrupts is saved as a %llu by the kernel,
 * whereas individual interrupts are saved as %u.
 */
struct stats_irq {
	unsigned long long irq_nr;
};

#define STATS_IRQ_SIZE	(sizeof(struct stats_irq))

/* Structure for swapping statistics */
struct stats_swap {
	unsigned long pswpin	__attribute__ ((aligned (8)));
	unsigned long pswpout	__attribute__ ((aligned (8)));
};

#define STATS_SWAP_SIZE	(sizeof(struct stats_swap))

/* Structure for paging statistics */
struct stats_paging {
	unsigned long pgpgin		__attribute__ ((aligned (8)));
	unsigned long pgpgout		__attribute__ ((aligned (8)));
	unsigned long pgfault		__attribute__ ((aligned (8)));
	unsigned long pgmajfault	__attribute__ ((aligned (8)));
	unsigned long pgfree		__attribute__ ((aligned (8)));
	unsigned long pgscan_kswapd	__attribute__ ((aligned (8)));
	unsigned long pgscan_direct	__attribute__ ((aligned (8)));
	unsigned long pgsteal		__attribute__ ((aligned (8)));
};

#define STATS_PAGING_SIZE	(sizeof(struct stats_paging))

/* Structure for I/O and transfer rate statistics */
struct stats_io {
	unsigned long long dk_drive;
	unsigned long long dk_drive_rio;
	unsigned long long dk_drive_wio;
	unsigned long long dk_drive_rblk;
	unsigned long long dk_drive_wblk;
};

#define STATS_IO_SIZE	(sizeof(struct stats_io))

/* Structure for memory and swap space utilization statistics */
struct stats_memory {
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
	unsigned long dirtykb		__attribute__ ((aligned (8)));
	unsigned long anonpgkb		__attribute__ ((aligned (8)));
	unsigned long slabkb		__attribute__ ((aligned (8)));
	unsigned long kstackkb		__attribute__ ((aligned (8)));
	unsigned long pgtblkb		__attribute__ ((aligned (8)));
	unsigned long vmusedkb		__attribute__ ((aligned (8)));
	unsigned long availablekb	__attribute__ ((aligned (8)));
};

#define STATS_MEMORY_SIZE	(sizeof(struct stats_memory))

/* Structure for kernel tables statistics */
struct stats_ktables {
	unsigned int  file_used;
	unsigned int  inode_used;
	unsigned int  dentry_stat;
	unsigned int  pty_nr;
};

#define STATS_KTABLES_SIZE	(sizeof(struct stats_ktables))

/* Structure for queue and load statistics */
struct stats_queue {
	unsigned long nr_running	__attribute__ ((aligned (8)));
	unsigned long procs_blocked	__attribute__ ((aligned (8)));
	unsigned int  load_avg_1	__attribute__ ((aligned (8)));
	unsigned int  load_avg_5;
	unsigned int  load_avg_15;
	unsigned int  nr_threads;
};

#define STATS_QUEUE_SIZE	(sizeof(struct stats_queue))

/* Structure for serial statistics */
struct stats_serial {
	unsigned int rx;
	unsigned int tx;
	unsigned int frame;
	unsigned int parity;
	unsigned int brk;
	unsigned int overrun;
	/*
	 * A value of 0 means that the structure is unused.
	 * To avoid the confusion, the line number is saved as (line# + 1)
	 */
	unsigned int line;
};

#define STATS_SERIAL_SIZE	(sizeof(struct stats_serial))

/* Structure for block devices statistics */
struct stats_disk {
	unsigned long long nr_ios;
	unsigned long rd_sect		__attribute__ ((aligned (8)));
	unsigned long wr_sect		__attribute__ ((aligned (8)));
	unsigned int rd_ticks		__attribute__ ((aligned (8)));
	unsigned int wr_ticks;
	unsigned int tot_ticks;
	unsigned int rq_ticks;
	unsigned int major;
	unsigned int minor;
};

#define STATS_DISK_SIZE	(sizeof(struct stats_disk))

/* Structure for network interfaces statistics */
struct stats_net_dev {
	unsigned long long rx_packets;
	unsigned long long tx_packets;
	unsigned long long rx_bytes;
	unsigned long long tx_bytes;
	unsigned long long rx_compressed;
	unsigned long long tx_compressed;
	unsigned long long multicast;
	unsigned int       speed;
	char 	 interface[MAX_IFACE_LEN];
	char	 duplex;
};

#define STATS_NET_DEV_SIZE	(sizeof(struct stats_net_dev))

/* Structure for network interface errors statistics */
struct stats_net_edev {
	unsigned long long collisions;
	unsigned long long rx_errors;
	unsigned long long tx_errors;
	unsigned long long rx_dropped;
	unsigned long long tx_dropped;
	unsigned long long rx_fifo_errors;
	unsigned long long tx_fifo_errors;
	unsigned long long rx_frame_errors;
	unsigned long long tx_carrier_errors;
	char	      interface[MAX_IFACE_LEN];
};

#define STATS_NET_EDEV_SIZE	(sizeof(struct stats_net_edev))

/* Structure for NFS client statistics */
struct stats_net_nfs {
	unsigned int  nfs_rpccnt;
	unsigned int  nfs_rpcretrans;
	unsigned int  nfs_readcnt;
	unsigned int  nfs_writecnt;
	unsigned int  nfs_accesscnt;
	unsigned int  nfs_getattcnt;
};

#define STATS_NET_NFS_SIZE	(sizeof(struct stats_net_nfs))

/* Structure for NFS server statistics */
struct stats_net_nfsd {
	unsigned int  nfsd_rpccnt;
	unsigned int  nfsd_rpcbad;
	unsigned int  nfsd_netcnt;
	unsigned int  nfsd_netudpcnt;
	unsigned int  nfsd_nettcpcnt;
	unsigned int  nfsd_rchits;
	unsigned int  nfsd_rcmisses;
	unsigned int  nfsd_readcnt;
	unsigned int  nfsd_writecnt;
	unsigned int  nfsd_accesscnt;
	unsigned int  nfsd_getattcnt;
};

#define STATS_NET_NFSD_SIZE	(sizeof(struct stats_net_nfsd))

/* Structure for IPv4 sockets statistics */
struct stats_net_sock {
	unsigned int  sock_inuse;
	unsigned int  tcp_inuse;
	unsigned int  tcp_tw;
	unsigned int  udp_inuse;
	unsigned int  raw_inuse;
	unsigned int  frag_inuse;
};

#define STATS_NET_SOCK_SIZE	(sizeof(struct stats_net_sock))

/* Structure for IP statistics */
struct stats_net_ip {
	unsigned long long InReceives;
	unsigned long long ForwDatagrams;
	unsigned long long InDelivers;
	unsigned long long OutRequests;
	unsigned long long ReasmReqds;
	unsigned long long ReasmOKs;
	unsigned long long FragOKs;
	unsigned long long FragCreates;
};

#define STATS_NET_IP_SIZE	(sizeof(struct stats_net_ip))

/* Structure for IP errors statistics */
struct stats_net_eip {
	unsigned long long InHdrErrors;
	unsigned long long InAddrErrors;
	unsigned long long InUnknownProtos;
	unsigned long long InDiscards;
	unsigned long long OutDiscards;
	unsigned long long OutNoRoutes;
	unsigned long long ReasmFails;
	unsigned long long FragFails;
};

#define STATS_NET_EIP_SIZE	(sizeof(struct stats_net_eip))

/* Structure for ICMP statistics */
struct stats_net_icmp {
	unsigned long InMsgs		__attribute__ ((aligned (8)));
	unsigned long OutMsgs		__attribute__ ((aligned (8)));
	unsigned long InEchos		__attribute__ ((aligned (8)));
	unsigned long InEchoReps	__attribute__ ((aligned (8)));
	unsigned long OutEchos		__attribute__ ((aligned (8)));
	unsigned long OutEchoReps	__attribute__ ((aligned (8)));
	unsigned long InTimestamps	__attribute__ ((aligned (8)));
	unsigned long InTimestampReps	__attribute__ ((aligned (8)));
	unsigned long OutTimestamps	__attribute__ ((aligned (8)));
	unsigned long OutTimestampReps	__attribute__ ((aligned (8)));
	unsigned long InAddrMasks	__attribute__ ((aligned (8)));
	unsigned long InAddrMaskReps	__attribute__ ((aligned (8)));
	unsigned long OutAddrMasks	__attribute__ ((aligned (8)));
	unsigned long OutAddrMaskReps	__attribute__ ((aligned (8)));
};

#define STATS_NET_ICMP_SIZE	(sizeof(struct stats_net_icmp))

/* Structure for ICMP error message statistics */
struct stats_net_eicmp {
	unsigned long InErrors		__attribute__ ((aligned (8)));
	unsigned long OutErrors		__attribute__ ((aligned (8)));
	unsigned long InDestUnreachs	__attribute__ ((aligned (8)));
	unsigned long OutDestUnreachs	__attribute__ ((aligned (8)));
	unsigned long InTimeExcds	__attribute__ ((aligned (8)));
	unsigned long OutTimeExcds	__attribute__ ((aligned (8)));
	unsigned long InParmProbs	__attribute__ ((aligned (8)));
	unsigned long OutParmProbs	__attribute__ ((aligned (8)));
	unsigned long InSrcQuenchs	__attribute__ ((aligned (8)));
	unsigned long OutSrcQuenchs	__attribute__ ((aligned (8)));
	unsigned long InRedirects	__attribute__ ((aligned (8)));
	unsigned long OutRedirects	__attribute__ ((aligned (8)));
};

#define STATS_NET_EICMP_SIZE	(sizeof(struct stats_net_eicmp))

/* Structure for TCP statistics */
struct stats_net_tcp {
	unsigned long ActiveOpens	__attribute__ ((aligned (8)));
	unsigned long PassiveOpens	__attribute__ ((aligned (8)));
	unsigned long InSegs		__attribute__ ((aligned (8)));
	unsigned long OutSegs		__attribute__ ((aligned (8)));
};

#define STATS_NET_TCP_SIZE	(sizeof(struct stats_net_tcp))

/* Structure for TCP errors statistics */
struct stats_net_etcp {
	unsigned long AttemptFails	__attribute__ ((aligned (8)));
	unsigned long EstabResets	__attribute__ ((aligned (8)));
	unsigned long RetransSegs	__attribute__ ((aligned (8)));
	unsigned long InErrs		__attribute__ ((aligned (8)));
	unsigned long OutRsts		__attribute__ ((aligned (8)));
};

#define STATS_NET_ETCP_SIZE	(sizeof(struct stats_net_etcp))

/* Structure for UDP statistics */
struct stats_net_udp {
	unsigned long InDatagrams	__attribute__ ((aligned (8)));
	unsigned long OutDatagrams	__attribute__ ((aligned (8)));
	unsigned long NoPorts		__attribute__ ((aligned (8)));
	unsigned long InErrors		__attribute__ ((aligned (8)));
};

#define STATS_NET_UDP_SIZE	(sizeof(struct stats_net_udp))

/* Structure for IPv6 statistics */
struct stats_net_ip6 {
	unsigned long long InReceives6;
	unsigned long long OutForwDatagrams6;
	unsigned long long InDelivers6;
	unsigned long long OutRequests6;
	unsigned long long ReasmReqds6;
	unsigned long long ReasmOKs6;
	unsigned long long InMcastPkts6;
	unsigned long long OutMcastPkts6;
	unsigned long long FragOKs6;
	unsigned long long FragCreates6;
};

#define STATS_NET_IP6_SIZE	(sizeof(struct stats_net_ip6))

/* Structure for IPv6 errors statistics */
struct stats_net_eip6 {
	unsigned long long InHdrErrors6;
	unsigned long long InAddrErrors6;
	unsigned long long InUnknownProtos6;
	unsigned long long InTooBigErrors6;
	unsigned long long InDiscards6;
	unsigned long long OutDiscards6;
	unsigned long long InNoRoutes6;
	unsigned long long OutNoRoutes6;
	unsigned long long ReasmFails6;
	unsigned long long FragFails6;
	unsigned long long InTruncatedPkts6;
};

#define STATS_NET_EIP6_SIZE	(sizeof(struct stats_net_eip6))

/* Structure for ICMPv6 statistics */
struct stats_net_icmp6 {
	unsigned long InMsgs6				__attribute__ ((aligned (8)));
	unsigned long OutMsgs6				__attribute__ ((aligned (8)));
	unsigned long InEchos6				__attribute__ ((aligned (8)));
	unsigned long InEchoReplies6			__attribute__ ((aligned (8)));
	unsigned long OutEchoReplies6			__attribute__ ((aligned (8)));
	unsigned long InGroupMembQueries6		__attribute__ ((aligned (8)));
	unsigned long InGroupMembResponses6		__attribute__ ((aligned (8)));
	unsigned long OutGroupMembResponses6		__attribute__ ((aligned (8)));
	unsigned long InGroupMembReductions6		__attribute__ ((aligned (8)));
	unsigned long OutGroupMembReductions6		__attribute__ ((aligned (8)));
	unsigned long InRouterSolicits6			__attribute__ ((aligned (8)));
	unsigned long OutRouterSolicits6		__attribute__ ((aligned (8)));
	unsigned long InRouterAdvertisements6		__attribute__ ((aligned (8)));
	unsigned long InNeighborSolicits6		__attribute__ ((aligned (8)));
	unsigned long OutNeighborSolicits6		__attribute__ ((aligned (8)));
	unsigned long InNeighborAdvertisements6		__attribute__ ((aligned (8)));
	unsigned long OutNeighborAdvertisements6	__attribute__ ((aligned (8)));
};

#define STATS_NET_ICMP6_SIZE	(sizeof(struct stats_net_icmp6))

/* Structure for ICMPv6 error message statistics */
struct stats_net_eicmp6 {
	unsigned long InErrors6		__attribute__ ((aligned (8)));
	unsigned long InDestUnreachs6	__attribute__ ((aligned (8)));
	unsigned long OutDestUnreachs6	__attribute__ ((aligned (8)));
	unsigned long InTimeExcds6	__attribute__ ((aligned (8)));
	unsigned long OutTimeExcds6	__attribute__ ((aligned (8)));
	unsigned long InParmProblems6	__attribute__ ((aligned (8)));
	unsigned long OutParmProblems6	__attribute__ ((aligned (8)));
	unsigned long InRedirects6	__attribute__ ((aligned (8)));
	unsigned long OutRedirects6	__attribute__ ((aligned (8)));
	unsigned long InPktTooBigs6	__attribute__ ((aligned (8)));
	unsigned long OutPktTooBigs6	__attribute__ ((aligned (8)));
};

#define STATS_NET_EICMP6_SIZE	(sizeof(struct stats_net_eicmp6))

/* Structure for UDPv6 statistics */
struct stats_net_udp6 {
	unsigned long InDatagrams6	__attribute__ ((aligned (8)));
	unsigned long OutDatagrams6	__attribute__ ((aligned (8)));
	unsigned long NoPorts6		__attribute__ ((aligned (8)));
	unsigned long InErrors6		__attribute__ ((aligned (8)));
};

#define STATS_NET_UDP6_SIZE	(sizeof(struct stats_net_udp6))

/* Structure for IPv6 sockets statistics */
struct stats_net_sock6 {
	unsigned int  tcp6_inuse;
	unsigned int  udp6_inuse;
	unsigned int  raw6_inuse;
	unsigned int  frag6_inuse;
};

#define STATS_NET_SOCK6_SIZE	(sizeof(struct stats_net_sock6))

/*
 * Structure for CPU frequency statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_pwr_cpufreq {
	unsigned long cpufreq	__attribute__ ((aligned (8)));
};

#define STATS_PWR_CPUFREQ_SIZE	(sizeof(struct stats_pwr_cpufreq))

/* Structure for hugepages statistics */
struct stats_huge {
	unsigned long frhkb			__attribute__ ((aligned (8)));
	unsigned long tlhkb			__attribute__ ((aligned (8)));
};

#define STATS_HUGE_SIZE	(sizeof(struct stats_memory))

/*
 * Structure for weighted CPU frequency statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_pwr_wghfreq {
	unsigned long long 	time_in_state;
	unsigned long 		freq		__attribute__ ((aligned (8)));
};

#define STATS_PWR_WGHFREQ_SIZE	(sizeof(struct stats_pwr_wghfreq))

/*
 * Structure for USB devices plugged into the system.
 */
struct stats_pwr_usb {
	unsigned int  bus_nr;
	unsigned int  vendor_id;
	unsigned int  product_id;
	unsigned int  bmaxpower;
	char	      manufacturer[MAX_MANUF_LEN];
	char	      product[MAX_PROD_LEN];
};

#define STATS_PWR_USB_SIZE	(sizeof(struct stats_pwr_usb))

/* Structure for filesystems statistics */
struct stats_filesystem {
	unsigned long long f_blocks;
	unsigned long long f_bfree;
	unsigned long long f_bavail;
	unsigned long long f_files;
	unsigned long long f_ffree;
	char 		   fs_name[MAX_FS_LEN];
	char 		   mountp[MAX_FS_LEN];
};

#define STATS_FILESYSTEM_SIZE	(sizeof(struct stats_filesystem))

/* Structure for Fibre Channel HBA statistics */
struct stats_fchost {
	unsigned long f_rxframes		__attribute__ ((aligned (8)));
	unsigned long f_txframes		__attribute__ ((aligned (8)));
	unsigned long f_rxwords			__attribute__ ((aligned (8)));
	unsigned long f_txwords			__attribute__ ((aligned (8)));
	char	      fchost_name[MAX_FCH_LEN]	__attribute__ ((aligned (8)));
};

#define STATS_FCHOST_SIZE	(sizeof(struct stats_fchost))

/* Structure for softnet statistics */
struct stats_softnet {
	unsigned int processed;
	unsigned int dropped;
	unsigned int time_squeeze;
	unsigned int received_rps;
	unsigned int flow_limit;
};

#define STATS_SOFTNET_SIZE	(sizeof(struct stats_softnet))

/*
 ***************************************************************************
 * Prototypes for functions used to read system statistics
 ***************************************************************************
 */

void oct2chr
	(char *);
void read_stat_cpu
	(struct stats_cpu *, int);
void read_stat_irq
	(struct stats_irq *, int);
void read_meminfo
	(struct stats_memory *);
void read_uptime
	(unsigned long long *);
void read_stat_pcsw
	(struct stats_pcsw *);
void read_loadavg
	(struct stats_queue *);
void read_vmstat_swap
	(struct stats_swap *);
void read_vmstat_paging
	(struct stats_paging *);
void read_diskstats_io
	(struct stats_io *);
void read_diskstats_disk
	(struct stats_disk *, int, int);
void read_tty_driver_serial
	(struct stats_serial *, int);
void read_kernel_tables
	(struct stats_ktables *);
int read_net_dev
	(struct stats_net_dev *, int);
void read_if_info
	(struct stats_net_dev *, int);
void read_net_edev
	(struct stats_net_edev *, int);
void read_net_nfs
	(struct stats_net_nfs *);
void read_net_nfsd
	(struct stats_net_nfsd *);
void read_net_sock
	(struct stats_net_sock *);
void read_net_ip
	(struct stats_net_ip *);
void read_net_eip
	(struct stats_net_eip *);
void read_net_icmp
	(struct stats_net_icmp *);
void read_net_eicmp
	(struct stats_net_eicmp *);
void read_net_tcp
	(struct stats_net_tcp *);
void read_net_etcp
	(struct stats_net_etcp *);
void read_net_udp
	(struct stats_net_udp *);
void read_net_sock6
	(struct stats_net_sock6 *);
void read_net_ip6
	(struct stats_net_ip6 *);
void read_net_eip6
	(struct stats_net_eip6 *);
void read_net_icmp6
	(struct stats_net_icmp6 *);
void read_net_eicmp6
	(struct stats_net_eicmp6 *);
void read_net_udp6
	(struct stats_net_udp6 *);
void read_cpuinfo
	(struct stats_pwr_cpufreq *, int);
void read_meminfo_huge
	(struct stats_huge *);
void read_time_in_state
	(struct stats_pwr_wghfreq *, int, int);
void read_bus_usb_dev
	(struct stats_pwr_usb *, int);
void read_filesystem
	(struct stats_filesystem *, int);
void read_fchost
	(struct stats_fchost *, int);
void read_softnet
	(struct stats_softnet *, int);

#endif /* _RD_STATS_H */
