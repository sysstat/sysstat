/*
 * pcp_def_metrics.h: Include file used to define PCP metrics.
 * (C) 2019-2026 by Sebastien Godard (sysstat <at> orange.fr)
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_DEF_METRICS_H
#define _PCP_DEF_METRICS_H

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>

/*
 * Forward declaration so pcp_def_*_metrics function prototypes compile
 * cleanly when this header is included before sa.h.
 */
struct activity;

/*
 ***************************************************************************
 * Structure with PCP metrics information for each activity
 ***************************************************************************
 */

struct act_metrics {
	size_t count;		/* number of metrics in this group */
	int max_inst;		/* instance stride for handles[] (>=1 after pcp_alloc_handles) */
	int *handles;		/* [count * max_inst] write handles, NULL until pcp_def_* */
	int *inst_ids;		/* [max_inst] PCP instance ID per slot; NULL for scalar groups */
	pmDesc *descs;		/* metric descriptors array */
	const char **names;	/* array of count metric names */
	pmID *pmids;		/* array of count metric IDs */
};

/*
 * Access a write handle for (metric_index, slot).
 * Only valid after pcp_alloc_handles() has been called for this group.
 * For scalar metrics slot is always 0.
 */
#define ACT_HANDLE(m, metric, slot) \
	((m)->handles[(metric) * (m)->max_inst + (slot)])

/* Register a scalar metric and return its PMI write handle in one call */
int act_register_scalar_handle(struct act_metrics *m, size_t metric);

/*
 ***************************************************************************
 * Helper macros for building internal PCP identifiers and structures
 ***************************************************************************
 */

#define PMI_ID(domain, cluster, item)	\
	((((domain)&0x1ff)<<22)|(((cluster)&0xfff)<<10)|((item)&0x3ff))
#define PMI_INDOM(domain, serial)	\
	((((domain)&0x1ff)<<22)|(((serial)&0x3fffff)))
/*
 * Use designated initializers — correct regardless of bit-field layout.
 * extraUnit/extraScale (new PCP fields for temperature, voltage, power)
 * are left zero; set them explicitly when pmiUnitsEx() is available -
 * not in older versions.
 */
#define PMI_UNITS(dimspace, dimtime, dimcount, scalespace, scaletime, scalecount) \
    ((pmUnits){ .dimSpace   = (dimspace),  .dimTime    = (dimtime),   \
                .dimCount   = (dimcount),  .scaleSpace = (scalespace), \
                .scaleTime  = (scaletime), .scaleCount = (scalecount) })

/*
 ***************************************************************************
 * sysstat archive file header metric grouping
 ***************************************************************************
 */

enum {
	FILE_HEADER_CPU_COUNT,
	FILE_HEADER_KERNEL_HERTZ,
	FILE_HEADER_UNAME_SYSNAME,
	FILE_HEADER_UNAME_RELEASE,
	FILE_HEADER_UNAME_NODENAME,
	FILE_HEADER_UNAME_MACHINE,
	FILE_HEADER_UNAME_FULL,
	FILE_HEADER_BOOTTIME,
	FILE_HEADER_NDISK,
	FILE_HEADER_NINTERFACE,
	FILE_HEADER_PAGESIZE,
	FILE_HEADER_METRIC_COUNT /*end*/
};

#define PMID_FILE_HEADER_CPU_COUNT		PMI_ID(60, 0, 32)
#define PMID_FILE_HEADER_KERNEL_HERTZ		PMI_ID(60, 0, 48)
#define PMID_FILE_HEADER_UNAME_RELEASE		PMI_ID(60, 12, 0)
#define PMID_FILE_HEADER_UNAME_SYSNAME		PMI_ID(60, 12, 2)
#define PMID_FILE_HEADER_UNAME_MACHINE		PMI_ID(60, 12, 3)
#define PMID_FILE_HEADER_UNAME_NODENAME		PMI_ID(60, 12, 4)
#define PMID_FILE_HEADER_UNAME_FULL		PMI_ID(60, 12, 5)
#define PMID_FILE_HEADER_BOOTTIME		PMI_ID(60, 0, 17)
#define PMID_FILE_HEADER_NDISK			PMI_ID(60, 0, 33)
#define PMID_FILE_HEADER_NINTERFACE		PMI_ID(60, 3, 27)
#define PMID_FILE_HEADER_PAGESIZE		PMI_ID(60, 1, 11)

extern const char *file_header_metric_names[];
extern pmDesc file_header_metric_descs[];
extern struct act_metrics file_header_metrics;

/*
 ***************************************************************************
 * sysstat archive record header metric grouping
 ***************************************************************************
 */

enum {
	RECORD_HEADER_KERNEL_UPTIME,
	RECORD_HEADER_METRIC_COUNT /*end*/
};
#define PMID_RECORD_HEADER_KERNEL_UPTIME	PMI_ID(60, 26, 0)

extern pmDesc record_header_metric_descs[];
extern struct act_metrics record_header_metrics;

/*
 ***************************************************************************
 * sadc provenance metric grouping — sadc.* namespace
 ***************************************************************************
 */

enum {
	SADC_VERSION,		/* string: sysstat version that wrote the archive */
	SADC_ACTIVITIES,	/* string: comma-separated list of collected activities */
	SADC_INTERVAL,		/* u32:    nominal collection interval in seconds */
	SADC_COMMENT,		/* string: operator comment text */
	SADC_RESTARTS,		/* u32:    restart event flag (value=1 at restart timestamps) */
	SADC_ARCHIVE,		/* string: archive base path */
	SADC_HOSTNAME,		/* string: hostname of the monitored system */
	SADC_TIMEZONE,		/* string: timezone name (e.g. UTC+10) */
	SADC_ZONEINFO,		/* string: Olson timezone name (e.g. Australia/Sydney) */
	SADC_METRIC_COUNT /*end*/
};

/* pmimport PMDA domain 43; clusters 0-1 are standard, cluster 2 is sadc-specific */
#define PMID_SADC_VERSION	PMI_ID(43, 0, 1)	/* pmimport.version */
#define PMID_SADC_ACTIVITIES	PMI_ID(43, 0, 2)	/* pmimport.args */
#define PMID_SADC_INTERVAL	PMI_ID(43, 2, 0)	/* pmimport.sadc.interval */
#define PMID_SADC_COMMENT	PMI_ID(43, 2, 1)	/* pmimport.sadc.comment */
#define PMID_SADC_RESTARTS	PMI_ID(43, 2, 2)	/* pmimport.sadc.restarts */
#define PMID_SADC_ARCHIVE	PMI_ID(43, 0, 0)	/* pmimport.archive */
#define PMID_SADC_HOSTNAME	PMI_ID(43, 1, 0)	/* pmimport.hostname */
#define PMID_SADC_TIMEZONE	PMI_ID(43, 1, 1)	/* pmimport.timezone */
#define PMID_SADC_ZONEINFO	PMI_ID(43, 1, 2)	/* pmimport.zoneinfo */

extern const char *sadc_metric_names[];
extern pmDesc sadc_metric_descs[];
extern struct act_metrics sadc_metrics;

/*
 ***************************************************************************
 * CPU metric grouping
 ***************************************************************************
 */
void pcp_def_cpu_metrics(struct activity *);

enum {
	CPU_ALLCPU_USER,
	CPU_ALLCPU_SYS,
	CPU_ALLCPU_NICE,
	CPU_ALLCPU_IDLE,
	CPU_ALLCPU_WAITTOTAL,
	CPU_ALLCPU_IRQTOTAL,
	CPU_ALLCPU_IRQSOFT,
	CPU_ALLCPU_IRQHARD,
	CPU_ALLCPU_STEAL,
	CPU_ALLCPU_GUEST,
	CPU_ALLCPU_GUESTNICE,
	CPU_PERCPU_USER,
	CPU_PERCPU_NICE,
	CPU_PERCPU_SYS,
	CPU_PERCPU_IDLE,
	CPU_PERCPU_WAITTOTAL,
	CPU_PERCPU_CPU_INTR,
	CPU_PERCPU_IRQSOFT,
	CPU_PERCPU_IRQHARD,
	CPU_PERCPU_STEAL,
	CPU_PERCPU_GUEST,
	CPU_PERCPU_GUESTNICE,
	CPU_PERCPU_INTERRUPTS,
	CPU_METRIC_COUNT /*end*/
};

#define PMID_CPU_ALLCPU_USER			PMI_ID(60, 0, 20)
#define PMID_CPU_ALLCPU_NICE			PMI_ID(60, 0, 21)
#define PMID_CPU_ALLCPU_SYS			PMI_ID(60, 0, 22)
#define PMID_CPU_ALLCPU_IDLE			PMI_ID(60, 0, 23)
#define PMID_CPU_ALLCPU_WAITTOTAL		PMI_ID(60, 0, 35)
#define PMID_CPU_ALLCPU_IRQTOTAL		PMI_ID(60, 0, 34)
#define PMID_CPU_ALLCPU_IRQSOFT			PMI_ID(60, 0, 53)
#define PMID_CPU_ALLCPU_IRQHARD			PMI_ID(60, 0, 54)
#define PMID_CPU_ALLCPU_STEAL			PMI_ID(60, 0, 55)
#define PMID_CPU_ALLCPU_GUEST			PMI_ID(60, 0, 60)
#define PMID_CPU_ALLCPU_GUESTNICE		PMI_ID(60, 0, 81)
#define PMID_CPU_PERCPU_USER			PMI_ID(60, 0, 0)
#define PMID_CPU_PERCPU_NICE			PMI_ID(60, 0, 1)
#define PMID_CPU_PERCPU_SYS			PMI_ID(60, 0, 2)
#define PMID_CPU_PERCPU_IDLE			PMI_ID(60, 0, 3)
#define PMID_CPU_PERCPU_WAITTOTAL		PMI_ID(60, 0, 30)
#define PMID_CPU_PERCPU_CPU_INTR		PMI_ID(60, 0, 31)
#define PMID_CPU_PERCPU_IRQSOFT			PMI_ID(60, 0, 56)
#define PMID_CPU_PERCPU_IRQHARD			PMI_ID(60, 0, 57)
#define PMID_CPU_PERCPU_STEAL			PMI_ID(60, 0, 58)
#define PMID_CPU_PERCPU_GUEST			PMI_ID(60, 0, 61)
#define PMID_CPU_PERCPU_GUESTNICE		PMI_ID(60, 0, 83)
#define PMID_CPU_PERCPU_INTERRUPTS		PMI_ID(60, 4, 1)

extern pmDesc cpu_metric_descs[];
extern struct act_metrics cpu_metrics;
#define STATS_CPU_METRICS (&cpu_metrics)

enum {
	SOFTNET_ALLCPU_PROCESSED,
	SOFTNET_ALLCPU_DROPPED,
	SOFTNET_ALLCPU_TIMESQUEEZE,
	SOFTNET_ALLCPU_RECEIVEDRPS,
	SOFTNET_ALLCPU_FLOWLIMIT,
	SOFTNET_ALLCPU_BACKLOGLENGTH,
	SOFTNET_PERCPU_PROCESSED,
	SOFTNET_PERCPU_DROPPED,
	SOFTNET_PERCPU_TIMESQUEEZE,
	SOFTNET_PERCPU_RECEIVEDRPS,
	SOFTNET_PERCPU_FLOWLIMIT,
	SOFTNET_PERCPU_BACKLOGLENGTH,
	SOFTNET_METRIC_COUNT /*end*/
};

#define PMID_SOFTNET_ALLCPU_PROCESSED		PMI_ID(60, 57, 0)
#define PMID_SOFTNET_ALLCPU_DROPPED		PMI_ID(60, 57, 1)
#define PMID_SOFTNET_ALLCPU_TIMESQUEEZE		PMI_ID(60, 57, 2)
#define PMID_SOFTNET_ALLCPU_RECEIVEDRPS		PMI_ID(60, 57, 4)
#define PMID_SOFTNET_ALLCPU_FLOWLIMIT		PMI_ID(60, 57, 5)
#define PMID_SOFTNET_ALLCPU_BACKLOGLENGTH	PMI_ID(60, 57, 12)
#define PMID_SOFTNET_PERCPU_PROCESSED		PMI_ID(60, 57, 6)
#define PMID_SOFTNET_PERCPU_DROPPED		PMI_ID(60, 57, 7)
#define PMID_SOFTNET_PERCPU_TIMESQUEEZE		PMI_ID(60, 57, 8)
#define PMID_SOFTNET_PERCPU_RECEIVEDRPS		PMI_ID(60, 57, 10)
#define PMID_SOFTNET_PERCPU_FLOWLIMIT		PMI_ID(60, 57, 11)
#define PMID_SOFTNET_PERCPU_BACKLOGLENGTH	PMI_ID(60, 57, 13)

extern pmDesc softnet_metric_descs[];
extern struct act_metrics softnet_metrics;
#define STATS_SOFTNET_METRICS (&softnet_metrics)

enum {
	POWER_PERCPU_CLOCK,
	POWER_CPU_METRIC_COUNT /*end*/
};

#define PMID_POWER_PERCPU_CLOCK			PMI_ID(60, 18, 0)

extern pmDesc power_cpu_metric_descs[];
extern struct act_metrics power_cpu_metrics;
#define STATS_PWR_CPU_METRICS (&power_cpu_metrics)

/*
 ***************************************************************************
 * Process and context switch metric grouping
 ***************************************************************************
 */
void pcp_def_pcsw_metrics(struct activity *);

enum {
	PCSW_CONTEXT_SWITCH,
	PCSW_FORK_SYSCALLS,
	PCSW_METRIC_COUNT /*end*/
};

#define PMID_PCSW_CONTEXT_SWITCH		PMI_ID(60, 0, 13)
#define PMID_PCSW_FORK_SYSCALLS			PMI_ID(60, 0, 14)

extern pmDesc pcsw_metric_descs[];
extern struct act_metrics pcsw_metrics;
#define STATS_PCSW_METRICS (&pcsw_metrics)

/*
 ***************************************************************************
 * Interrupt request line metric grouping
 ***************************************************************************
 */
void pcp_def_irq_metrics(struct activity *);

enum {
	IRQ_ALLIRQ_TOTAL,
	IRQ_PERIRQ_TOTAL,
	IRQ_METRIC_COUNT /*end*/
};

#define PMID_IRQ_ALLIRQ_TOTAL			PMI_ID(60, 0, 12)
#define PMID_IRQ_PERIRQ_TOTAL			PMI_ID(60, 4, 0)

extern pmDesc irq_metric_descs[];
extern struct act_metrics irq_metrics;
#define STATS_IRQ_METRICS (&irq_metrics)

/*
 ***************************************************************************
 * Swap metric grouping
 ***************************************************************************
 */
void pcp_def_swap_metrics(struct activity *);

enum {
	SWAP_PAGESIN,
	SWAP_PAGESOUT,
	SWAP_METRIC_COUNT /*end*/
};

#define PMID_SWAP_PAGESIN			PMI_ID(60, 0, 8)
#define PMID_SWAP_PAGESOUT			PMI_ID(60, 0, 9)

extern pmDesc swap_metric_descs[];
extern struct act_metrics swap_metrics;
#define STATS_SWAP_METRICS (&swap_metrics)

/*
 ***************************************************************************
 * Paging metric grouping
 ***************************************************************************
 */
void pcp_def_paging_metrics(struct activity *);

enum {
	PAGING_PGPGIN,
	PAGING_PGPGOUT,
	PAGING_PGFAULT,
	PAGING_PGMAJFAULT,
	PAGING_PGFREE,
	PAGING_PGSCANDIRECT,
	PAGING_PGSCANKSWAPD,
	PAGING_PGSTEAL,
	PAGING_PGDEMOTE,
	PAGING_PGPROMOTE,
	PAGING_METRIC_COUNT /*end*/
};

#define PMID_PAGING_PGPGIN			PMI_ID(60, 28, 6)
#define PMID_PAGING_PGPGOUT			PMI_ID(60, 28, 7)
#define PMID_PAGING_PGFAULT			PMI_ID(60, 28, 16)
#define PMID_PAGING_PGMAJFAULT			PMI_ID(60, 28, 17)
#define PMID_PAGING_PGFREE			PMI_ID(60, 28, 13)
#define PMID_PAGING_PGSCANDIRECT		PMI_ID(60, 28, 176)
#define PMID_PAGING_PGSCANKSWAPD		PMI_ID(60, 28, 177)
#define PMID_PAGING_PGSTEAL			PMI_ID(60, 28, 178)
#define PMID_PAGING_PGDEMOTE			PMI_ID(60, 28, 185)
#define PMID_PAGING_PGPROMOTE			PMI_ID(60, 28, 187)

extern pmDesc paging_metric_descs[];
extern struct act_metrics paging_metrics;
#define STATS_PAGING_METRICS (&paging_metrics)

/*
 ***************************************************************************
 * I/O metric grouping
 ***************************************************************************
 */
void pcp_def_io_metrics(struct activity *);

enum {
	IO_ALLDEV_TOTAL,
	IO_ALLDEV_READ,
	IO_ALLDEV_WRITE,
	IO_ALLDEV_DISCARD,
	IO_ALLDEV_READBYTES,
	IO_ALLDEV_WRITEBYTES,
	IO_ALLDEV_DISCARDBYTES,
	IO_METRIC_COUNT /*end*/
};

#define PMID_IO_ALLDEV_TOTAL			PMI_ID(60, 0, 29)
#define PMID_IO_ALLDEV_READ			PMI_ID(60, 0, 24)
#define PMID_IO_ALLDEV_WRITE			PMI_ID(60, 0, 25)
#define PMID_IO_ALLDEV_DISCARD			PMI_ID(60, 0, 96)
#define PMID_IO_ALLDEV_READBYTES		PMI_ID(60, 0, 41)
#define PMID_IO_ALLDEV_WRITEBYTES		PMI_ID(60, 0, 42)
#define PMID_IO_ALLDEV_DISCARDBYTES		PMI_ID(60, 0, 98)

extern pmDesc io_metric_descs[];
extern struct act_metrics io_metrics;
#define STATS_IO_METRICS (&io_metrics)

/*
 ***************************************************************************
 * Memory metric grouping
 ***************************************************************************
 */
void pcp_def_memory_metrics(struct activity *);

enum {
	MEM_PHYS_MB,
	MEM_PHYS_KB,
	MEM_UTIL_FREE,
	MEM_UTIL_SHARED,
	MEM_UTIL_AVAIL,
	MEM_UTIL_USED,
	MEM_UTIL_BUFFER,
	MEM_UTIL_CACHED,
	MEM_UTIL_COMMITAS,
	MEM_UTIL_ACTIVE,
	MEM_UTIL_INACTIVE,
	MEM_UTIL_DIRTY,
	MEM_UTIL_ANON,
	MEM_UTIL_SLAB,
	MEM_UTIL_KSTACK,
	MEM_UTIL_PGTABLE,
	MEM_UTIL_VMALLOC,
	MEM_UTIL_SWAPFREE,
	MEM_UTIL_SWAPTOTAL,
	MEM_UTIL_SWAPCACHED,
	MEM_UTIL_SHMEM,
	MEM_UTIL_FREEMEM,
	SWAP_FREE,
	SWAP_LENGTH,
	MEM_METRIC_COUNT /*end*/
};

#define PMID_MEM_PHYS_MB			PMI_ID(60, 1, 9)
#define PMID_MEM_PHYS_KB			PMI_ID(60, 1, 0)
#define PMID_MEM_UTIL_FREE			PMI_ID(60, 1, 2)
#define PMID_MEM_UTIL_SHARED			PMI_ID(60, 1, 3)
#define PMID_MEM_UTIL_AVAIL			PMI_ID(60, 1, 58)
#define PMID_MEM_UTIL_USED			PMI_ID(60, 1, 1)
#define PMID_MEM_UTIL_BUFFER			PMI_ID(60, 1, 4)
#define PMID_MEM_UTIL_CACHED			PMI_ID(60, 1, 5)
#define PMID_MEM_UTIL_COMMITAS			PMI_ID(60, 1, 26)
#define PMID_MEM_UTIL_ACTIVE			PMI_ID(60, 1, 14)
#define PMID_MEM_UTIL_INACTIVE			PMI_ID(60, 1, 15)
#define PMID_MEM_UTIL_DIRTY			PMI_ID(60, 1, 22)
#define PMID_MEM_UTIL_ANON			PMI_ID(60, 1, 30)
#define PMID_MEM_UTIL_SLAB			PMI_ID(60, 1, 25)
#define PMID_MEM_UTIL_KSTACK			PMI_ID(60, 1, 43)
#define PMID_MEM_UTIL_PGTABLE			PMI_ID(60, 1, 27)
#define PMID_MEM_UTIL_VMALLOC			PMI_ID(60, 1, 51)
#define PMID_MEM_UTIL_SWAPFREE			PMI_ID(60, 1, 21)
#define PMID_MEM_UTIL_SWAPTOTAL			PMI_ID(60, 1, 20)
#define PMID_MEM_UTIL_SWAPCACHED		PMI_ID(60, 1, 13)
#define PMID_MEM_UTIL_SHMEM			PMI_ID(60, 1, 42)
#define PMID_MEM_UTIL_FREEMEM			PMI_ID(60, 1, 10)
#define PMID_SWAP_FREE				PMI_ID(60, 1, 8)
#define PMID_SWAP_LENGTH			PMI_ID(60, 1, 6)

extern pmDesc mem_metric_descs[];
extern struct act_metrics mem_metrics;
#define STATS_MEMORY_METRICS (&mem_metrics)

/*
 ***************************************************************************
 * Kernel tables metric grouping
 ***************************************************************************
 */
void pcp_def_ktables_metrics(struct activity *);

enum {
	KTABLE_DENTRYS,
	KTABLE_FILES,
	KTABLE_INODES,
	KTABLE_PTYS,
	KTABLE_METRIC_COUNT /*end*/
};

#define PMID_KTABLE_DENTRYS			PMI_ID(60, 27, 5)
#define PMID_KTABLE_FILES			PMI_ID(60, 27, 0)
#define PMID_KTABLE_INODES			PMI_ID(60, 27, 3)
#define PMID_KTABLE_PTYS			PMI_ID(60, 72, 3)

extern pmDesc ktable_metric_descs[];
extern struct act_metrics ktable_metrics;
#define STATS_KTABLES_METRICS (&ktable_metrics)

/*
 ***************************************************************************
 * Kernel queues metric grouping
 ***************************************************************************
 */
void pcp_def_queue_metrics(struct activity *);

enum {
	KQUEUE_RUNNABLE,
	KQUEUE_PROCESSES,
	KQUEUE_BLOCKED,
	KQUEUE_LOADAVG,
	KQUEUE_METRIC_COUNT /*end*/
};

#define PMID_KQUEUE_RUNNABLE			PMI_ID(60, 2, 2)
#define PMID_KQUEUE_PROCESSES			PMI_ID(60, 2, 3)
#define PMID_KQUEUE_BLOCKED			PMI_ID(60, 0, 16)
#define PMID_KQUEUE_LOADAVG			PMI_ID(60, 2, 0)

extern pmDesc kqueue_metric_descs[];
extern struct act_metrics kqueue_metrics;
#define STATS_QUEUE_METRICS (&kqueue_metrics)

/*
 ***************************************************************************
 * Disk device metric grouping
 ***************************************************************************
 */
void pcp_def_disk_metrics(struct activity *);
void pcp_probe_disk_instances(struct activity *);

enum {
	DISK_PERDEV_READ,
	DISK_PERDEV_WRITE,
	DISK_PERDEV_TOTAL,
	DISK_PERDEV_TOTALBYTES,
	DISK_PERDEV_READBYTES,
	DISK_PERDEV_WRITEBYTES,
	DISK_PERDEV_DISCARDBYTES,
	DISK_PERDEV_READACTIVE,
	DISK_PERDEV_WRITEACTIVE,
	DISK_PERDEV_TOTALACTIVE,
	DISK_PERDEV_DISCARDACTIVE,
	DISK_PERDEV_AVACTIVE,
	DISK_PERDEV_AVQUEUE,
	DISK_PERDEV_BLKREAD,
	DISK_PERDEV_BLKWRITE,
	DISK_PERDEV_DISCARD,
	DISK_METRIC_COUNT /*end*/
};

#define PMID_DISK_PERDEV_READ			PMI_ID(60, 0, 4)
#define PMID_DISK_PERDEV_WRITE			PMI_ID(60, 0, 5)
#define PMID_DISK_PERDEV_TOTAL			PMI_ID(60, 0, 28)
#define PMID_DISK_PERDEV_TOTALBYTES		PMI_ID(60, 0, 37)
#define PMID_DISK_PERDEV_READBYTES		PMI_ID(60, 0, 38)
#define PMID_DISK_PERDEV_WRITEBYTES		PMI_ID(60, 0, 39)
#define PMID_DISK_PERDEV_DISCARDBYTES		PMI_ID(60, 0, 90)
#define PMID_DISK_PERDEV_READACTIVE		PMI_ID(60, 0, 72)
#define PMID_DISK_PERDEV_WRITEACTIVE		PMI_ID(60, 0, 73)
#define PMID_DISK_PERDEV_TOTALACTIVE		PMI_ID(60, 0, 79)
#define PMID_DISK_PERDEV_DISCARDACTIVE		PMI_ID(60, 0, 92)
#define PMID_DISK_PERDEV_AVACTIVE		PMI_ID(60, 0, 46)
#define PMID_DISK_PERDEV_AVQUEUE		PMI_ID(60, 0, 47)
#define PMID_DISK_PERDEV_BLKREAD		PMI_ID(60, 0, 6)
#define PMID_DISK_PERDEV_BLKWRITE		PMI_ID(60, 0, 7)
#define PMID_DISK_PERDEV_DISCARD		PMI_ID(60, 0, 88)

extern pmDesc disk_metric_descs[];
extern struct act_metrics disk_metrics;
#define STATS_DISK_METRICS (&disk_metrics)

/*
 ***************************************************************************
 * Shared metric indices for secondary disk classes (dm/md/partitions/zram)
 ***************************************************************************
 */
enum {
	DCLASS_READ,		/* read operations */
	DCLASS_WRITE,		/* write operations */
	DCLASS_TOTAL,		/* total I/Os */
	DCLASS_READBYTES,	/* kB read */
	DCLASS_WRITEBYTES,	/* kB written */
	DCLASS_TOTALBYTES,	/* kB total */
	DCLASS_DISCBYTES,	/* kB discarded */
	DCLASS_READ_MERGE,	/* read merges */
	DCLASS_WRITE_MERGE,	/* write merges */
	DCLASS_AVACTIVE,	/* avactive ms */
	DCLASS_AVEQ,		/* aveq ms */
	DCLASS_RD_ACTIVE,	/* read rawactive ms */
	DCLASS_WR_ACTIVE,	/* write rawactive ms */
	DCLASS_TOTALACTIVE,	/* total rawactive ms */
	DCLASS_DISCARDACTIVE,	/* discard rawactive ms */
	DCLASS_BLKREAD,		/* sectors read */
	DCLASS_BLKWRITE,	/* sectors written */
	DCLASS_DISCARD,		/* discard operations */
	DCLASS_METRIC_COUNT	/* end */
};

/* Shared item numbers within each secondary disk cluster */
#define DCLASS_ITEM_READ		0
#define DCLASS_ITEM_WRITE		1
#define DCLASS_ITEM_TOTAL		2
#define DCLASS_ITEM_BLKREAD		3
#define DCLASS_ITEM_BLKWRITE		4
#define DCLASS_ITEM_READBYTES		6
#define DCLASS_ITEM_WRITEBYTES		7
#define DCLASS_ITEM_TOTALBYTES		8
#define DCLASS_ITEM_READ_MERGE		9
#define DCLASS_ITEM_WRITE_MERGE		10
#define DCLASS_ITEM_AVACTIVE		11
#define DCLASS_ITEM_AVEQ		12
#define DCLASS_ITEM_RD_ACTIVE		13
#define DCLASS_ITEM_WR_ACTIVE		14
#define DCLASS_ITEM_TOTALACTIVE		16
#define DCLASS_ITEM_DISCARD		18
#define DCLASS_ITEM_DISCBYTES		20
#define DCLASS_ITEM_DISCARDACTIVE	22

#define DCLASS_CLUSTER_DM		54
#define DCLASS_CLUSTER_MD		59
#define DCLASS_CLUSTER_PART		10
#define DCLASS_CLUSTER_ZRAM		86

#define DCLASS_INDOM_DM			PMI_INDOM(60, 24)
#define DCLASS_INDOM_MD			PMI_INDOM(60, 25)
#define DCLASS_INDOM_PART		PMI_INDOM(60, 10)
#define DCLASS_INDOM_ZRAM		PMI_INDOM(60, 38)

/* Classification helpers — implemented in pcp_def_metrics.c */
int is_dm_device(const char *name);
int is_md_device(const char *name);
int is_zram_device(const char *name);
int is_part_device(const char *name);
void dm_persistent_name(const char *kernel_name, unsigned int dmajor,
			unsigned int dminor, char *out, size_t len);

/* Per-class item lists and metrics (extern for pcp_stats.c) */
extern struct sa_item *dm_item_list;
extern struct sa_item *md_item_list;
extern struct sa_item *part_item_list;
extern struct sa_item *zram_item_list;
extern struct act_metrics dm_metrics;
extern struct act_metrics md_metrics;
extern struct act_metrics part_metrics;
extern struct act_metrics zram_metrics;

void pcp_def_disk_class_metrics(struct act_metrics *m, struct sa_item *ilist,
				int cluster, pmInDom indom,
				const char *prefix);
void pcp_probe_dm_instances(struct activity *a);
void pcp_probe_md_instances(struct activity *a);
void pcp_probe_part_instances(struct activity *a);
void pcp_probe_zram_instances(struct activity *a);

/*
 ***************************************************************************
 * Network interface metric grouping
 ***************************************************************************
 */
void pcp_def_net_dev_metrics(struct activity *);
void pcp_probe_net_dev_instances(struct activity *);
void pcp_probe_net_edev_instances(struct activity *);

enum {
	NET_PERINTF_INPACKETS,
	NET_PERINTF_OUTPACKETS,
	NET_PERINTF_INBYTES,
	NET_PERINTF_OUTBYTES,
	NET_PERINTF_INCOMPRESS,
	NET_PERINTF_OUTCOMPRESS,
	NET_PERINTF_INMULTICAST,
	NET_PERINTF_SPEED,
	NET_PERINTF_BAUDRATE,
	NET_PERINTF_DUPLEX,
	NET_PERINTF_METRIC_COUNT /*end*/
};

#define PMID_NET_PERINTF_INPACKETS		PMI_ID(60, 3, 1)
#define PMID_NET_PERINTF_OUTPACKETS		PMI_ID(60, 3, 9)
#define PMID_NET_PERINTF_INBYTES		PMI_ID(60, 3, 0)
#define PMID_NET_PERINTF_OUTBYTES		PMI_ID(60, 3, 8)
#define PMID_NET_PERINTF_INCOMPRESS		PMI_ID(60, 3, 6)
#define PMID_NET_PERINTF_OUTCOMPRESS		PMI_ID(60, 3, 15)
#define PMID_NET_PERINTF_INMULTICAST		PMI_ID(60, 3, 7)
#define PMID_NET_PERINTF_SPEED			PMI_ID(60, 3, 22)
#define PMID_NET_PERINTF_BAUDRATE		PMI_ID(60, 3, 23)
#define PMID_NET_PERINTF_DUPLEX			PMI_ID(60, 3, 24)

extern pmDesc netdev_metric_descs[];
extern struct act_metrics netdev_metrics;
#define STATS_NET_DEV_METRICS (&netdev_metrics)

enum {
	NET_EPERINTF_INERRORS,
	NET_EPERINTF_OUTERRORS,
	NET_EPERINTF_COLLISIONS,
	NET_EPERINTF_INDROPS,
	NET_EPERINTF_OUTDROPS,
	NET_EPERINTF_OUTCARRIER,
	NET_EPERINTF_INFRAME,
	NET_EPERINTF_INFIFO,
	NET_EPERINTF_OUTFIFO,
	NET_EPERINTF_METRIC_COUNT /*end*/
};

#define PMID_NET_EPERINTF_INERRORS		PMI_ID(60, 3, 2)
#define PMID_NET_EPERINTF_OUTERRORS		PMI_ID(60, 3, 10)
#define PMID_NET_EPERINTF_COLLISIONS		PMI_ID(60, 3, 13)
#define PMID_NET_EPERINTF_INDROPS		PMI_ID(60, 3, 3)
#define PMID_NET_EPERINTF_OUTDROPS		PMI_ID(60, 3, 11)
#define PMID_NET_EPERINTF_OUTCARRIER		PMI_ID(60, 3, 14)
#define PMID_NET_EPERINTF_INFRAME		PMI_ID(60, 3, 5)
#define PMID_NET_EPERINTF_INFIFO		PMI_ID(60, 3, 4)
#define PMID_NET_EPERINTF_OUTFIFO		PMI_ID(60, 3, 12)

extern pmDesc netedev_metric_descs[];
extern struct act_metrics netedev_metrics;
#define STATS_NET_EDEV_METRICS (&netedev_metrics)

/*
 ***************************************************************************
 * Serial line metric grouping
 ***************************************************************************
 */
void pcp_def_serial_metrics(struct activity *);

enum {
	SERIAL_PERTTY_RX,
	SERIAL_PERTTY_TX,
	SERIAL_PERTTY_FRAME,
	SERIAL_PERTTY_PARITY,
	SERIAL_PERTTY_BRK,
	SERIAL_PERTTY_OVERRUN,
	SERIAL_METRIC_COUNT /*end*/
};

#define PMID_SERIAL_PERTTY_RX			PMI_ID(60, 74, 0)
#define PMID_SERIAL_PERTTY_TX			PMI_ID(60, 74, 1)
#define PMID_SERIAL_PERTTY_FRAME		PMI_ID(60, 74, 2)
#define PMID_SERIAL_PERTTY_PARITY		PMI_ID(60, 74, 3)
#define PMID_SERIAL_PERTTY_BRK			PMI_ID(60, 74, 4)
#define PMID_SERIAL_PERTTY_OVERRUN		PMI_ID(60, 74, 5)

extern pmDesc serial_metric_descs[];
extern struct act_metrics serial_metrics;
#define STATS_SERIAL_METRICS (&serial_metrics)

/*
 ***************************************************************************
 * Socket metric grouping
 ***************************************************************************
 */
void pcp_def_net_sock_metrics(struct activity *);

enum {
	SOCKET_TOTAL,
	SOCKET_TCPINUSE,
	SOCKET_UDPINUSE,
	SOCKET_RAWINUSE,
	SOCKET_FRAGINUSE,
	SOCKET_TCPTW,
	SOCKET_METRIC_COUNT /*end*/
};

#define PMID_SOCKET_TOTAL			PMI_ID(60, 11, 9)
#define PMID_SOCKET_TCPINUSE			PMI_ID(60, 11, 0)
#define PMID_SOCKET_UDPINUSE			PMI_ID(60, 11, 3)
#define PMID_SOCKET_RAWINUSE			PMI_ID(60, 11, 6)
#define PMID_SOCKET_FRAGINUSE			PMI_ID(60, 11, 15)
#define PMID_SOCKET_TCPTW			PMI_ID(60, 11, 11)

extern pmDesc socket_metric_descs[];
extern struct act_metrics socket_metrics;
#define STATS_NET_SOCK_METRICS (&socket_metrics)

/*
 ***************************************************************************
 * IP metric grouping
 ***************************************************************************
 */
void pcp_def_net_ip_metrics(struct activity *);
void pcp_def_net_eip_metrics(struct activity *);

enum {
	NET_IP_INRECEIVES,
	NET_IP_FORWDATAGRAMS,
	NET_IP_INDELIVERS,
	NET_IP_OUTREQUESTS,
	NET_IP_REASMREQDS,
	NET_IP_REASMOKS,
	NET_IP_FRAGOKS,
	NET_IP_FRAGCREATES,
	NET_IP_METRIC_COUNT /*end*/
};

#define PMID_NET_IP_INRECEIVES			PMI_ID(60, 14, 2)
#define PMID_NET_IP_FORWDATAGRAMS		PMI_ID(60, 14, 5)
#define PMID_NET_IP_INDELIVERS			PMI_ID(60, 14, 8)
#define PMID_NET_IP_OUTREQUESTS			PMI_ID(60, 14, 9)
#define PMID_NET_IP_REASMREQDS			PMI_ID(60, 14, 13)
#define PMID_NET_IP_REASMOKS			PMI_ID(60, 14, 14)
#define PMID_NET_IP_FRAGOKS			PMI_ID(60, 14, 16)
#define PMID_NET_IP_FRAGCREATES			PMI_ID(60, 14, 18)

extern pmDesc net_ip_metric_descs[];
extern struct act_metrics net_ip_metrics;
#define STATS_NET_IP_METRICS (&net_ip_metrics)

enum {
	NET_EIP_INHDRERRORS,
	NET_EIP_INADDRERRORS,
	NET_EIP_INUNKNOWNPROTOS,
	NET_EIP_INDISCARDS,
	NET_EIP_OUTDISCARDS,
	NET_EIP_OUTNOROUTES,
	NET_EIP_REASMFAILS,
	NET_EIP_FRAGFAILS,
	NET_EIP_METRIC_COUNT /*end*/
};

#define PMID_NET_EIP_INHDRERRORS		PMI_ID(60, 14, 3)
#define PMID_NET_EIP_INADDRERRORS		PMI_ID(60, 14, 4)
#define PMID_NET_EIP_INUNKNOWNPROTOS		PMI_ID(60, 14, 6)
#define PMID_NET_EIP_INDISCARDS			PMI_ID(60, 14, 7)
#define PMID_NET_EIP_OUTDISCARDS		PMI_ID(60, 14, 10)
#define PMID_NET_EIP_OUTNOROUTES		PMI_ID(60, 14, 11)
#define PMID_NET_EIP_REASMFAILS			PMI_ID(60, 14, 15)
#define PMID_NET_EIP_FRAGFAILS			PMI_ID(60, 14, 17)

extern pmDesc net_eip_metric_descs[];
extern struct act_metrics net_eip_metrics;
#define STATS_NET_EIP_METRICS (&net_eip_metrics)

/*
 ***************************************************************************
 * NFS request instance numbering
 ***************************************************************************
 */
enum {
	NFS_REQUEST_GETATTR = 4,
	NFS_REQUEST_READ = 6,
	NFS_REQUEST_WRITE = 8,
	NFS_REQUEST_ACCESS = 18,
};

/*
 ***************************************************************************
 * NFS client metric grouping
 ***************************************************************************
 */
void pcp_def_net_nfs_metrics(struct activity *);

enum {
	NFSCLIENT_RPCCCNT,
	NFSCLIENT_RPCRETRANS,
	NFSCLIENT_REQUESTS,
	NFSCLIENT_METRIC_COUNT /*end*/
};

#define PMID_NFSCLIENT_RPCCCNT			PMI_ID(60, 7, 20)
#define PMID_NFSCLIENT_RPCRETRANS		PMI_ID(60, 7, 21)
#define PMID_NFSCLIENT_REQUESTS			PMI_ID(60, 7, 4)

extern pmDesc nfsclient_metric_descs[];
extern struct act_metrics nfsclient_metrics;
#define STATS_NET_NFS_METRICS (&nfsclient_metrics)

/*
 ***************************************************************************
 * NFS server metric grouping
 ***************************************************************************
 */
void pcp_def_net_nfsd_metrics(struct activity *);

enum {
	NFSSERVER_RPCCNT,
	NFSSERVER_RPCBADCLNT,
	NFSSERVER_NETCNT,
	NFSSERVER_NETUDPCNT,
	NFSSERVER_NETTCPCNT,
	NFSSERVER_RCHITS,
	NFSSERVER_RCMISSES,
	NFSSERVER_REQUESTS,
	NFSSERVER_METRIC_COUNT /*end*/
};

#define PMID_NFSSERVER_RPCCNT			PMI_ID(60, 7, 30)
#define PMID_NFSSERVER_RPCBADCLNT		PMI_ID(60, 7, 34)
#define PMID_NFSSERVER_NETCNT			PMI_ID(60, 7, 44)
#define PMID_NFSSERVER_NETUDPCNT		PMI_ID(60, 7, 45)
#define PMID_NFSSERVER_NETTCPCNT		PMI_ID(60, 7, 46)
#define PMID_NFSSERVER_RCHITS			PMI_ID(60, 7, 35)
#define PMID_NFSSERVER_RCMISSES			PMI_ID(60, 7, 36)
#define PMID_NFSSERVER_REQUESTS			PMI_ID(60, 7, 12)

extern pmDesc nfsserver_metric_descs[];
extern struct act_metrics nfsserver_metrics;
#define STATS_NET_NFSD_METRICS (&nfsserver_metrics)

/*
 ***************************************************************************
 * Internet Control Message Protocol metric grouping
 ***************************************************************************
 */
void pcp_def_net_icmp_metrics(struct activity *);
void pcp_def_net_eicmp_metrics(struct activity *);

enum {
	NET_ICMP_INMSGS,
	NET_ICMP_OUTMSGS,
	NET_ICMP_INECHOS,
	NET_ICMP_INECHOREPS,
	NET_ICMP_OUTECHOS,
	NET_ICMP_OUTECHOREPS,
	NET_ICMP_INTIMESTAMPS,
	NET_ICMP_INTIMESTAMPREPS,
	NET_ICMP_OUTTIMESTAMPS,
	NET_ICMP_OUTTIMESTAMPREPS,
	NET_ICMP_INADDRMASKS,
	NET_ICMP_INADDRMASKREPS,
	NET_ICMP_OUTADDRMASKS,
	NET_ICMP_OUTADDRMASKREPS,
	NET_ICMP_METRIC_COUNT /*end*/
};

#define PMID_NET_ICMP_INMSGS			PMI_ID(60, 14, 20)
#define PMID_NET_ICMP_OUTMSGS			PMI_ID(60, 14, 33)
#define PMID_NET_ICMP_INECHOS			PMI_ID(60, 14, 27)
#define PMID_NET_ICMP_INECHOREPS		PMI_ID(60, 14, 28)
#define PMID_NET_ICMP_OUTECHOS			PMI_ID(60, 14, 40)
#define PMID_NET_ICMP_OUTECHOREPS		PMI_ID(60, 14, 41)
#define PMID_NET_ICMP_INTIMESTAMPS		PMI_ID(60, 14, 29)
#define PMID_NET_ICMP_INTIMESTAMPREPS		PMI_ID(60, 14, 30)
#define PMID_NET_ICMP_OUTTIMESTAMPS		PMI_ID(60, 14, 42)
#define PMID_NET_ICMP_OUTTIMESTAMPREPS		PMI_ID(60, 14, 43)
#define PMID_NET_ICMP_INADDRMASKS		PMI_ID(60, 14, 31)
#define PMID_NET_ICMP_INADDRMASKREPS		PMI_ID(60, 14, 32)
#define PMID_NET_ICMP_OUTADDRMASKS		PMI_ID(60, 14, 44)
#define PMID_NET_ICMP_OUTADDRMASKREPS		PMI_ID(60, 14, 45)

extern pmDesc net_icmp_metric_descs[];
extern struct act_metrics net_icmp_metrics;
#define STATS_NET_ICMP_METRICS (&net_icmp_metrics)

enum {
	NET_EICMP_INERRORS,
	NET_EICMP_OUTERRORS,
	NET_EICMP_INDESTUNREACHS,
	NET_EICMP_OUTDESTUNREACHS,
	NET_EICMP_INTIMEEXCDS,
	NET_EICMP_OUTTIMEEXCDS,
	NET_EICMP_INPARMPROBS,
	NET_EICMP_OUTPARMPROBS,
	NET_EICMP_INSRCQUENCHS,
	NET_EICMP_OUTSRCQUENCHS,
	NET_EICMP_INREDIRECTS,
	NET_EICMP_OUTREDIRECTS,
	NET_EICMP_METRIC_COUNT /*end*/
};

#define PMID_NET_EICMP_INERRORS			PMI_ID(60, 14, 21)
#define PMID_NET_EICMP_OUTERRORS		PMI_ID(60, 14, 34)
#define PMID_NET_EICMP_INDESTUNREACHS		PMI_ID(60, 14, 22)
#define PMID_NET_EICMP_OUTDESTUNREACHS		PMI_ID(60, 14, 35)
#define PMID_NET_EICMP_INTIMEEXCDS		PMI_ID(60, 14, 23)
#define PMID_NET_EICMP_OUTTIMEEXCDS		PMI_ID(60, 14, 36)
#define PMID_NET_EICMP_INPARMPROBS		PMI_ID(60, 14, 24)
#define PMID_NET_EICMP_OUTPARMPROBS		PMI_ID(60, 14, 37)
#define PMID_NET_EICMP_INSRCQUENCHS		PMI_ID(60, 14, 25)
#define PMID_NET_EICMP_OUTSRCQUENCHS		PMI_ID(60, 14, 38)
#define PMID_NET_EICMP_INREDIRECTS		PMI_ID(60, 14, 26)
#define PMID_NET_EICMP_OUTREDIRECTS		PMI_ID(60, 14, 39)

extern pmDesc net_eicmp_metric_descs[];
extern struct act_metrics net_eicmp_metrics;
#define STATS_NET_EICMP_METRICS (&net_eicmp_metrics)

/*
 ***************************************************************************
 * Transmission Control Protocol metric grouping
 ***************************************************************************
 */
void pcp_def_net_tcp_metrics(struct activity *);
void pcp_def_net_etcp_metrics(struct activity *);

enum {
	NET_TCP_ACTIVEOPENS,
	NET_TCP_PASSIVEOPENS,
	NET_TCP_INSEGS,
	NET_TCP_OUTSEGS,
	NET_TCP_METRIC_COUNT /*all*/
};

#define PMID_NET_TCP_ACTIVEOPENS		PMI_ID(60, 14, 54)
#define PMID_NET_TCP_PASSIVEOPENS		PMI_ID(60, 14, 55)
#define PMID_NET_TCP_INSEGS			PMI_ID(60, 14, 59)
#define PMID_NET_TCP_OUTSEGS			PMI_ID(60, 14, 60)

extern pmDesc net_tcp_metric_descs[];
extern struct act_metrics net_tcp_metrics;
#define STATS_NET_TCP_METRICS (&net_tcp_metrics)

enum {
	NET_ETCP_ATTEMPTFAILS,
	NET_ETCP_ESTABRESETS,
	NET_ETCP_RETRANSSEGS,
	NET_ETCP_INERRS,
	NET_ETCP_OUTRSTS,
	NET_ETCP_METRIC_COUNT /*end*/
};

#define PMID_NET_ETCP_ATTEMPTFAILS		PMI_ID(60, 14, 56)
#define PMID_NET_ETCP_ESTABRESETS		PMI_ID(60, 14, 57)
#define PMID_NET_ETCP_RETRANSSEGS		PMI_ID(60, 14, 61)
#define PMID_NET_ETCP_INERRS			PMI_ID(60, 14, 62)
#define PMID_NET_ETCP_OUTRSTS			PMI_ID(60, 14, 63)

extern pmDesc net_etcp_metric_descs[];
extern struct act_metrics net_etcp_metrics;
#define STATS_NET_ETCP_METRICS (&net_etcp_metrics)

/*
 ***************************************************************************
 * Unreliable Datagram Protocol metric grouping
 ***************************************************************************
 */
void pcp_def_net_udp_metrics(struct activity *);

enum {
	NET_UDP_INDATAGRAMS,
	NET_UDP_OUTDATAGRAMS,
	NET_UDP_NOPORTS,
	NET_UDP_INERRORS,
	NET_UDP_METRIC_COUNT /*end*/
};

#define PMID_NET_UDP_INDATAGRAMS		PMI_ID(60, 14, 70)
#define PMID_NET_UDP_OUTDATAGRAMS		PMI_ID(60, 14, 74)
#define PMID_NET_UDP_NOPORTS			PMI_ID(60, 14, 71)
#define PMID_NET_UDP_INERRORS			PMI_ID(60, 14, 72)

extern pmDesc net_udp_metric_descs[];
extern struct act_metrics net_udp_metrics;
#define STATS_NET_UDP_METRICS (&net_udp_metrics)

/*
 ***************************************************************************
 * Socket v6 metric grouping
 ***************************************************************************
 */
void pcp_def_net_sock6_metrics(struct activity *);

enum {
	NET_SOCK6_TCPINUSE,
	NET_SOCK6_UDPINUSE,
	NET_SOCK6_RAWINUSE,
	NET_SOCK6_FRAGINUSE,
	NET_SOCK6_METRIC_COUNT /*end*/
};

#define PMID_NET_SOCK6_TCPINUSE			PMI_ID(60, 73, 0)
#define PMID_NET_SOCK6_UDPINUSE			PMI_ID(60, 73, 1)
#define PMID_NET_SOCK6_RAWINUSE			PMI_ID(60, 73, 3)
#define PMID_NET_SOCK6_FRAGINUSE		PMI_ID(60, 73, 4)

extern pmDesc net_sock6_metric_descs[];
extern struct act_metrics net_sock6_metrics;
#define STATS_NET_SOCK6_METRICS (&net_sock6_metrics)

/*
 ***************************************************************************
 * IPv6 metric grouping
 ***************************************************************************
 */
void pcp_def_net_ip6_metrics(struct activity *);
void pcp_def_net_eip6_metrics(struct activity *);

enum {
	NET_IP6_INRECEIVES,
	NET_IP6_OUTFORWDATAGRAMS,
	NET_IP6_INDELIVERS,
	NET_IP6_OUTREQUESTS,
	NET_IP6_REASMREQDS,
	NET_IP6_REASMOKS,
	NET_IP6_INMCASTPKTS,
	NET_IP6_OUTMCASTPKTS,
	NET_IP6_FRAGOKS,
	NET_IP6_FRAGCREATES,
	NET_IP6_METRIC_COUNT /*all*/
};

#define PMID_NET_IP6_INRECEIVES			PMI_ID(60, 58, 0)
#define PMID_NET_IP6_OUTFORWDATAGRAMS		PMI_ID(60, 58, 9)
#define PMID_NET_IP6_INDELIVERS			PMI_ID(60, 58, 8)
#define PMID_NET_IP6_OUTREQUESTS		PMI_ID(60, 58, 10)
#define PMID_NET_IP6_REASMREQDS			PMI_ID(60, 58, 14)
#define PMID_NET_IP6_REASMOKS			PMI_ID(60, 58, 15)
#define PMID_NET_IP6_INMCASTPKTS		PMI_ID(60, 58, 20)
#define PMID_NET_IP6_OUTMCASTPKTS		PMI_ID(60, 58, 21)
#define PMID_NET_IP6_FRAGOKS			PMI_ID(60, 58, 17)
#define PMID_NET_IP6_FRAGCREATES		PMI_ID(60, 58, 19)

extern pmDesc net_ip6_metric_descs[];
extern struct act_metrics net_ip6_metrics;
#define STATS_NET_IP6_METRICS (&net_ip6_metrics)

enum {
	NET_EIP6_INHDRERRORS,
	NET_EIP6_INADDRERRORS,
	NET_EIP6_INUNKNOWNPROTOS,
	NET_EIP6_INTOOBIGERRORS,
	NET_EIP6_INDISCARDS,
	NET_EIP6_OUTDISCARDS,
	NET_EIP6_INNOROUTES,
	NET_EIP6_OUTNOROUTES,
	NET_EIP6_REASMFAILS,
	NET_EIP6_FRAGFAILS,
	NET_EIP6_INTRUNCATEDPKTS,
	NET_EIP6_METRIC_COUNT /*end*/
};

#define PMID_NET_EIP6_INHDRERRORS		PMI_ID(60, 58, 1)
#define PMID_NET_EIP6_INADDRERRORS		PMI_ID(60, 58, 4)
#define PMID_NET_EIP6_INUNKNOWNPROTOS		PMI_ID(60, 58, 5)
#define PMID_NET_EIP6_INTOOBIGERRORS		PMI_ID(60, 58, 2)
#define PMID_NET_EIP6_INDISCARDS		PMI_ID(60, 58, 7)
#define PMID_NET_EIP6_OUTDISCARDS		PMI_ID(60, 58, 11)
#define PMID_NET_EIP6_INNOROUTES		PMI_ID(60, 58, 3)
#define PMID_NET_EIP6_OUTNOROUTES		PMI_ID(60, 58, 12)
#define PMID_NET_EIP6_REASMFAILS		PMI_ID(60, 58, 16)
#define PMID_NET_EIP6_FRAGFAILS			PMI_ID(60, 58, 18)
#define PMID_NET_EIP6_INTRUNCATEDPKTS		PMI_ID(60, 58, 6)

extern pmDesc net_eip6_metric_descs[];
extern struct act_metrics net_eip6_metrics;
#define STATS_NET_EIP6_METRICS (&net_eip6_metrics)

/*
 ***************************************************************************
 * ICMPv6 metric grouping
 ***************************************************************************
 */
void pcp_def_net_icmp6_metrics(struct activity *);
void pcp_def_net_eicmp6_metrics(struct activity *);

enum {
	NET_ICMP6_INMSGS,
	NET_ICMP6_OUTMSGS,
	NET_ICMP6_INECHOS,
	NET_ICMP6_INECHOREPLIES,
	NET_ICMP6_OUTECHOREPLIES,
	NET_ICMP6_INGROUPMEMBQUERIES,
	NET_ICMP6_INGROUPMEMBRESPONSES,
	NET_ICMP6_OUTGROUPMEMBRESPONSES,
	NET_ICMP6_INGROUPMEMBREDUCTIONS,
	NET_ICMP6_OUTGROUPMEMBREDUCTIONS,
	NET_ICMP6_INROUTERSOLICITS,
	NET_ICMP6_OUTROUTERSOLICITS,
	NET_ICMP6_INROUTERADVERTISEMENTS,
	NET_ICMP6_INNEIGHBORSOLICITS,
	NET_ICMP6_OUTNEIGHBORSOLICITS,
	NET_ICMP6_INNEIGHBORADVERTISEMENTS,
	NET_ICMP6_OUTNEIGHBORADVERTISEMENTS,
	NET_ICMP6_METRIC_COUNT /*end*/
};

#define PMID_NET_ICMP6_INMSGS			PMI_ID(60, 58, 32)
#define PMID_NET_ICMP6_OUTMSGS			PMI_ID(60, 58, 34)
#define PMID_NET_ICMP6_INECHOS			PMI_ID(60, 58, 41)
#define PMID_NET_ICMP6_INECHOREPLIES		PMI_ID(60, 58, 42)
#define PMID_NET_ICMP6_OUTECHOREPLIES		PMI_ID(60, 58, 57)
#define PMID_NET_ICMP6_INGROUPMEMBQUERIES	PMI_ID(60, 58, 43)
#define PMID_NET_ICMP6_INGROUPMEMBRESPONSES	PMI_ID(60, 58, 44)
#define PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES	PMI_ID(60, 58, 59)
#define PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS	PMI_ID(60, 58, 45)
#define PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS	PMI_ID(60, 58, 60)
#define PMID_NET_ICMP6_INROUTERSOLICITS		PMI_ID(60, 58, 46)
#define PMID_NET_ICMP6_OUTROUTERSOLICITS	PMI_ID(60, 58, 61)
#define PMID_NET_ICMP6_INROUTERADVERTISEMENTS	PMI_ID(60, 58, 47)
#define PMID_NET_ICMP6_INNEIGHBORSOLICITS	PMI_ID(60, 58, 48)
#define PMID_NET_ICMP6_OUTNEIGHBORSOLICITS	PMI_ID(60, 58, 63)
#define PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS	PMI_ID(60, 58, 49)
#define PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS PMI_ID(60, 58, 64)

extern pmDesc net_icmp6_metric_descs[];
extern struct act_metrics net_icmp6_metrics;
#define STATS_NET_ICMP6_METRICS (&net_icmp6_metrics)

enum {
	NET_EICMP6_INERRORS,
	NET_EICMP6_INDESTUNREACHS,
	NET_EICMP6_OUTDESTUNREACHS,
	NET_EICMP6_INTIMEEXCDS,
	NET_EICMP6_OUTTIMEEXCDS,
	NET_EICMP6_INPARMPROBLEMS,
	NET_EICMP6_OUTPARMPROBLEMS,
	NET_EICMP6_INREDIRECTS,
	NET_EICMP6_OUTREDIRECTS,
	NET_EICMP6_INPKTTOOBIGS,
	NET_EICMP6_OUTPKTTOOBIGS,
	NET_EICMP6_METRIC_COUNT /*end*/
};

#define PMID_NET_EICMP6_INERRORS		PMI_ID(60, 58, 33)
#define PMID_NET_EICMP6_INDESTUNREACHS		PMI_ID(60, 58, 37)
#define PMID_NET_EICMP6_OUTDESTUNREACHS		PMI_ID(60, 58, 52)
#define PMID_NET_EICMP6_INTIMEEXCDS		PMI_ID(60, 58, 39)
#define PMID_NET_EICMP6_OUTTIMEEXCDS		PMI_ID(60, 58, 54)
#define PMID_NET_EICMP6_INPARMPROBLEMS		PMI_ID(60, 58, 40)
#define PMID_NET_EICMP6_OUTPARMPROBLEMS		PMI_ID(60, 58, 55)
#define PMID_NET_EICMP6_INREDIRECTS		PMI_ID(60, 58, 50)
#define PMID_NET_EICMP6_OUTREDIRECTS		PMI_ID(60, 58, 65)
#define PMID_NET_EICMP6_INPKTTOOBIGS		PMI_ID(60, 58, 38)
#define PMID_NET_EICMP6_OUTPKTTOOBIGS		PMI_ID(60, 58, 53)

extern pmDesc net_eicmp6_metric_descs[];
extern struct act_metrics net_eicmp6_metrics;
#define STATS_NET_EICMP6_METRICS (&net_eicmp6_metrics)

/*
 ***************************************************************************
 * UDPv6 metric grouping
 ***************************************************************************
 */
void pcp_def_net_udp6_metrics(struct activity *);

enum {
	NET_UDP6_INDATAGRAMS,
	NET_UDP6_OUTDATAGRAMS,
	NET_UDP6_NOPORTS,
	NET_UDP6_INERRORS,
	NET_UDP6_METRIC_COUNT /*end*/
};

#define PMID_NET_UDP6_INDATAGRAMS		PMI_ID(60, 58, 67)
#define PMID_NET_UDP6_OUTDATAGRAMS		PMI_ID(60, 58, 70)
#define PMID_NET_UDP6_NOPORTS			PMI_ID(60, 58, 68)
#define PMID_NET_UDP6_INERRORS			PMI_ID(60, 58, 69)

extern pmDesc net_udp6_metric_descs[];
extern struct act_metrics net_udp6_metrics;
#define STATS_NET_UDP6_METRICS (&net_udp6_metrics)

/*
 ***************************************************************************
 * Hugepage metric grouping
 ***************************************************************************
 */
void pcp_def_huge_metrics(struct activity *);

enum {
	MEM_HUGE_TOTALBYTES,
	MEM_HUGE_FREEBYTES,
	MEM_HUGE_RSVDBYTES,
	MEM_HUGE_SURPBYTES,
	MEM_HUGE_METRIC_COUNT /*end*/
};

#define PMID_MEM_HUGE_TOTALBYTES		PMI_ID(60, 1, 60)
#define PMID_MEM_HUGE_FREEBYTES			PMI_ID(60, 1, 61)
#define PMID_MEM_HUGE_RSVDBYTES			PMI_ID(60, 1, 62)
#define PMID_MEM_HUGE_SURPBYTES			PMI_ID(60, 1, 63)

extern pmDesc mem_huge_metric_descs[];
extern struct act_metrics mem_huge_metrics;
#define STATS_HUGE_METRICS (&mem_huge_metrics)

/*
 ***************************************************************************
 * Fan metric grouping
 ***************************************************************************
 */
void pcp_def_pwr_fan_metrics(struct activity *);

enum {
	POWER_FAN_RPM,
	POWER_FAN_DRPM,
	POWER_FAN_DEVICE,
	POWER_FAN_METRIC_COUNT /*end*/
};

#define PMID_POWER_FAN_RPM			PMI_ID(34, 0, 0)
#define PMID_POWER_FAN_DRPM			PMI_ID(34, 0, 1)
#define PMID_POWER_FAN_DEVICE			PMI_ID(34, 0, 2)

extern pmDesc power_fan_metric_descs[];
extern struct act_metrics power_fan_metrics;
#define STATS_PWR_FAN_METRICS (&power_fan_metrics)

/*
 ***************************************************************************
 * Temperature metric grouping
 ***************************************************************************
 */
void pcp_def_pwr_temp_metrics(struct activity *);

enum {
	POWER_TEMP_CELSIUS,
	POWER_TEMP_PERCENT,
	POWER_TEMP_DEVICE,
	POWER_TEMP_METRIC_COUNT /*end*/
};

#define PMID_POWER_TEMP_CELSIUS			PMI_ID(34, 1, 0)
#define PMID_POWER_TEMP_PERCENT			PMI_ID(34, 1, 1)
#define PMID_POWER_TEMP_DEVICE			PMI_ID(34, 1, 2)

extern pmDesc power_temp_metric_descs[];
extern struct act_metrics power_temp_metrics;
#define STATS_PWR_TEMP_METRICS (&power_temp_metrics)

/*
 ***************************************************************************
 * Voltage metric grouping
 ***************************************************************************
 */
void pcp_def_pwr_in_metrics(struct activity *);

enum {
	POWER_IN_VOLTAGE,
	POWER_IN_PERCENT,
	POWER_IN_DEVICE,
	POWER_IN_METRIC_COUNT /*end*/
};

#define PMID_POWER_IN_VOLTAGE			PMI_ID(34, 2, 0)
#define PMID_POWER_IN_PERCENT			PMI_ID(34, 2, 1)
#define PMID_POWER_IN_DEVICE			PMI_ID(34, 2, 2)

extern pmDesc power_in_metric_descs[];
extern struct act_metrics power_in_metrics;
#define STATS_PWR_IN_METRICS (&power_in_metrics)

/*
 ***************************************************************************
 * Battery metric grouping
 ***************************************************************************
 */
void pcp_def_pwr_bat_metrics(struct activity *);

enum {
	POWER_BAT_CAPACITY,
	POWER_BAT_STATUS,
	POWER_BAT_METRIC_COUNT /*end*/
};

#define PMID_POWER_BAT_CAPACITY			PMI_ID(34, 4, 0)
#define PMID_POWER_BAT_STATUS			PMI_ID(34, 4, 1)

extern pmDesc power_bat_metric_descs[];
extern struct act_metrics power_bat_metrics;
#define STATS_PWR_BAT_METRICS (&power_bat_metrics)

/*
 ***************************************************************************
 * USB metric grouping
 ***************************************************************************
 */
void pcp_def_pwr_usb_metrics(struct activity *);

enum {
	POWER_USB_BUS,
	POWER_USB_VENDORID,
	POWER_USB_PRODUCTID,
	POWER_USB_MAXPOWER,
	POWER_USB_MANUFACTURER,
	POWER_USB_PRODUCTNAME,
	POWER_USB_METRIC_COUNT /*end*/
};

#define PMID_POWER_USB_BUS			PMI_ID(34, 3, 0)
#define PMID_POWER_USB_VENDORID			PMI_ID(34, 3, 1)
#define PMID_POWER_USB_PRODUCTID		PMI_ID(34, 3, 2)
#define PMID_POWER_USB_MAXPOWER			PMI_ID(34, 3, 3)
#define PMID_POWER_USB_MANUFACTURER		PMI_ID(34, 3, 4)
#define PMID_POWER_USB_PRODUCTNAME		PMI_ID(34, 3, 5)

extern pmDesc power_usb_metric_descs[];
extern struct act_metrics power_usb_metrics;
#define STATS_PWR_USB_METRICS (&power_usb_metrics)

/*
 ***************************************************************************
 * Filesystem metric grouping
 ***************************************************************************
 */
void pcp_def_filesystem_metrics(struct activity *);
void pcp_probe_filesystem_instances(struct activity *);

enum {
	FILESYS_CAPACITY,
	FILESYS_FREE,
	FILESYS_USED,
	FILESYS_FULL,
	FILESYS_MAXFILES,
	FILESYS_FREEFILES,
	FILESYS_USEDFILES,
	FILESYS_AVAIL,
	FILESYS_METRIC_COUNT /*end*/
};

#define PMID_FILESYS_CAPACITY			PMI_ID(60, 5, 1)
#define PMID_FILESYS_FREE			PMI_ID(60, 5, 3)
#define PMID_FILESYS_USED			PMI_ID(60, 5, 2)
#define PMID_FILESYS_FULL			PMI_ID(60, 5, 8)
#define PMID_FILESYS_MAXFILES			PMI_ID(60, 5, 4)
#define PMID_FILESYS_FREEFILES			PMI_ID(60, 5, 6)
#define PMID_FILESYS_USEDFILES			PMI_ID(60, 5, 5)
#define PMID_FILESYS_AVAIL			PMI_ID(60, 5, 10)

extern pmDesc filesys_metric_descs[];
extern struct act_metrics filesys_metrics;
#define STATS_FILESYSTEM_METRICS (&filesys_metrics)

/*
 ***************************************************************************
 * Fibre Channel Host Bus Adapter metric grouping
 ***************************************************************************
 */
void pcp_def_fchost_metrics(struct activity *);

enum {
	FCHOST_INFRAMES,
	FCHOST_OUTFRAMES,
	FCHOST_INBYTES,
	FCHOST_OUTBYTES,
	FCHOST_METRIC_COUNT /*end*/
};

#define PMID_FCHOST_INFRAMES			PMI_ID(60, 91, 0)
#define PMID_FCHOST_OUTFRAMES			PMI_ID(60, 91, 1)
#define PMID_FCHOST_INBYTES			PMI_ID(60, 91, 2)
#define PMID_FCHOST_OUTBYTES			PMI_ID(60, 91, 3)

extern pmDesc fchost_metric_descs[];
extern struct act_metrics fchost_metrics;
#define STATS_FCHOST_METRICS (&fchost_metrics)

/*
 ***************************************************************************
 * Pressure Stall Information metric grouping
 ***************************************************************************
 */
void pcp_def_psi_metrics(struct activity *);

enum {
	PSI_CPU_SOMETOTAL,
	PSI_CPU_SOMEAVG,
	PSI_CPU_METRIC_COUNT /*end*/
};

#define PMID_PSI_CPU_SOMETOTAL			PMI_ID(60, 83, 1)
#define PMID_PSI_CPU_SOMEAVG			PMI_ID(60, 83, 0)
		
extern pmDesc psi_cpu_metric_descs[];
extern struct act_metrics psi_cpu_metrics;
#define STATS_PSI_CPU_METRICS (&psi_cpu_metrics)

enum {
	PSI_IO_SOMETOTAL,
	PSI_IO_SOMEAVG,
	PSI_IO_FULLTOTAL,
	PSI_IO_FULLAVG,
	PSI_IO_METRIC_COUNT /*end*/
};

#define PMID_PSI_IO_SOMETOTAL			PMI_ID(60, 85, 1)
#define PMID_PSI_IO_SOMEAVG			PMI_ID(60, 85, 0)
#define PMID_PSI_IO_FULLTOTAL			PMI_ID(60, 85, 3)
#define PMID_PSI_IO_FULLAVG			PMI_ID(60, 85, 2)

extern pmDesc psi_io_metric_descs[];
extern struct act_metrics psi_io_metrics;
#define STATS_PSI_IO_METRICS (&psi_io_metrics)

enum {
	PSI_MEM_SOMETOTAL,
	PSI_MEM_SOMEAVG,
	PSI_MEM_FULLTOTAL,
	PSI_MEM_FULLAVG,
	PSI_MEM_METRIC_COUNT /*end*/
};

#define PMID_PSI_MEM_SOMETOTAL			PMI_ID(60, 84, 1)
#define PMID_PSI_MEM_SOMEAVG			PMI_ID(60, 84, 0)
#define PMID_PSI_MEM_FULLTOTAL			PMI_ID(60, 84, 3)
#define PMID_PSI_MEM_FULLAVG			PMI_ID(60, 84, 2)

extern pmDesc psi_mem_metric_descs[];
extern struct act_metrics psi_mem_metrics;
#define STATS_PSI_MEM_METRICS (&psi_mem_metrics)

/* Handle allocation helpers (implemented in pcp_def_metrics.c) */
void pcp_alloc_handles(struct act_metrics *m, size_t num_inst);
void pcp_alloc_handle(struct act_metrics *m, size_t metric,
		      size_t slot, int inst_id,
		      const char *inst_name);
size_t pcp_find_slot(const struct act_metrics *m, int inst_id);

/*
 * Shared fetch helper: extract a u64 value for a specific instance from a
 * pmValueSet.  Returns 0 if the instance is not present or extraction fails.
 * Used by pcp_iostat, pcp_mpstat, pcp_cifsiostat and pcp_tapestat replay code.
 */
static inline unsigned long long
pcp_inst_u64(pmValueSet *vset, int inst_id)
{
	int i;
	pmAtomValue atom;

	if (!vset)
		return 0;
	for (i = 0; i < vset->numval; i++) {
		if (vset->vlist[i].inst != inst_id)
			continue;
		if (pmExtractValue(vset->valfmt, &vset->vlist[i],
				   PM_TYPE_U64, &atom, PM_TYPE_U64) < 0)
			return 0;
		return atom.ull;
	}
	return 0;
}

#else
/*
 ***************************************************************************
 * Define no-op macros and functions for generic code without PCP support
 ***************************************************************************
 */

#define STATS_CPU_METRICS		NULL
#define STATS_PWR_CPU_METRICS		NULL
#define STATS_SOFTNET_METRICS		NULL
#define STATS_PCSW_METRICS		NULL
#define STATS_IRQ_METRICS		NULL
#define STATS_SWAP_METRICS		NULL
#define STATS_PAGING_METRICS		NULL
#define STATS_IO_METRICS		NULL
#define STATS_MEMORY_METRICS		NULL
#define STATS_KTABLES_METRICS		NULL
#define STATS_QUEUE_METRICS		NULL
#define STATS_DISK_METRICS		NULL
#define STATS_NET_DEV_METRICS		NULL
#define STATS_NET_EDEV_METRICS		NULL
#define STATS_SERIAL_METRICS		NULL
#define STATS_NET_SOCK_METRICS		NULL
#define STATS_NET_IP_METRICS		NULL
#define STATS_NET_EIP_METRICS		NULL
#define STATS_NET_NFS_METRICS		NULL
#define STATS_NET_NFSD_METRICS		NULL
#define STATS_NET_ICMP_METRICS		NULL
#define STATS_NET_EICMP_METRICS		NULL
#define STATS_NET_TCP_METRICS		NULL
#define STATS_NET_ETCP_METRICS		NULL
#define STATS_NET_UDP_METRICS		NULL
#define STATS_NET_SOCK6_METRICS		NULL
#define STATS_NET_IP6_METRICS 		NULL
#define STATS_NET_EIP6_METRICS 		NULL
#define STATS_NET_ICMP6_METRICS 	NULL
#define STATS_NET_EICMP6_METRICS 	NULL
#define STATS_NET_UDP6_METRICS 		NULL
#define STATS_HUGE_METRICS 		NULL
#define STATS_PWR_FAN_METRICS 		NULL
#define STATS_PWR_TEMP_METRICS 		NULL
#define STATS_PWR_IN_METRICS 		NULL
#define STATS_PWR_BAT_METRICS 		NULL
#define STATS_PWR_USB_METRICS 		NULL
#define STATS_FILESYSTEM_METRICS	NULL
#define STATS_FCHOST_METRICS 		NULL
#define STATS_PSI_CPU_METRICS 		NULL
#define STATS_PSI_IO_METRICS 		NULL
#define STATS_PSI_MEM_METRICS		NULL

#endif /* HAVE_PCP undefined */
#endif /* _PCP_DEF_METRICS_H */
