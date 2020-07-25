/*
 * iostat: report CPU and I/O statistics
 * (C) 1999-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _IOSTAT_H
#define _IOSTAT_H

#include "common.h"

/* I_: iostat - D_: Display - F_: Flag */
#define I_D_CPU			0x000001
#define I_D_DISK		0x000002
#define I_D_TIMESTAMP		0x000004
#define I_D_EXTENDED		0x000008
#define I_D_EVERYTHING		0x000010
#define I_D_KILOBYTES		0x000020
#define I_D_ALL_DIR		0x000040
#define I_D_DEBUG		0x000080
#define I_D_UNFILTERED		0x000100
#define I_D_MEGABYTES		0x000200
#define I_D_ALL_DEVICES		0x000400
#define I_F_GROUP_DEFINED	0x000800
#define I_D_PRETTY		0x001000
#define I_D_PERSIST_NAME	0x002000
#define I_D_OMIT_SINCE_BOOT	0x004000
#define I_D_JSON_OUTPUT		0x008000
#define I_D_DEVMAP_NAME		0x010000
#define I_D_ISO			0x020000
#define I_D_GROUP_TOTAL_ONLY	0x040000
#define I_D_ZERO_OMIT		0x080000
#define I_D_UNIT		0x100000
#define I_D_SHORT_OUTPUT	0x200000

#define DISPLAY_CPU(m)			(((m) & I_D_CPU)              == I_D_CPU)
#define DISPLAY_DISK(m)			(((m) & I_D_DISK)             == I_D_DISK)
#define DISPLAY_TIMESTAMP(m)		(((m) & I_D_TIMESTAMP)        == I_D_TIMESTAMP)
#define DISPLAY_EXTENDED(m)		(((m) & I_D_EXTENDED)         == I_D_EXTENDED)
#define DISPLAY_EVERYTHING(m)		(((m) & I_D_EVERYTHING)       == I_D_EVERYTHING)
#define DISPLAY_KILOBYTES(m)		(((m) & I_D_KILOBYTES)        == I_D_KILOBYTES)
#define DISPLAY_MEGABYTES(m)		(((m) & I_D_MEGABYTES)        == I_D_MEGABYTES)
#define DISPLAY_DEBUG(m)		(((m) & I_D_DEBUG)            == I_D_DEBUG)
#define DISPLAY_UNFILTERED(m)		(((m) & I_D_UNFILTERED)       == I_D_UNFILTERED)
#define DISPLAY_ALL_DEVICES(m)		(((m) & I_D_ALL_DEVICES)      == I_D_ALL_DEVICES)
#define GROUP_DEFINED(m)		(((m) & I_F_GROUP_DEFINED)    == I_F_GROUP_DEFINED)
#define DISPLAY_PRETTY(m)		(((m) & I_D_PRETTY)           == I_D_PRETTY)
#define DISPLAY_PERSIST_NAME_I(m)	(((m) & I_D_PERSIST_NAME)     == I_D_PERSIST_NAME)
#define DISPLAY_OMIT_SINCE_BOOT(m)	(((m) & I_D_OMIT_SINCE_BOOT)  == I_D_OMIT_SINCE_BOOT)
#define DISPLAY_DEVMAP_NAME(m)		(((m) & I_D_DEVMAP_NAME)      == I_D_DEVMAP_NAME)
#define DISPLAY_ISO(m)			(((m) & I_D_ISO)              == I_D_ISO)
#define DISPLAY_GROUP_TOTAL_ONLY(m)	(((m) & I_D_GROUP_TOTAL_ONLY) == I_D_GROUP_TOTAL_ONLY)
#define DISPLAY_ZERO_OMIT(m)		(((m) & I_D_ZERO_OMIT)        == I_D_ZERO_OMIT)
#define DISPLAY_JSON_OUTPUT(m)		(((m) & I_D_JSON_OUTPUT)      == I_D_JSON_OUTPUT)
#define DISPLAY_UNIT(m)			(((m) & I_D_UNIT)	      == I_D_UNIT)
#define DISPLAY_SHORT_OUTPUT(m)		(((m) & I_D_SHORT_OUTPUT)     == I_D_SHORT_OUTPUT)
#define USE_ALL_DIR(m)			(((m) & I_D_ALL_DIR)          == I_D_ALL_DIR)

#define T_PART		0
#define T_DEV		1
#define T_PART_DEV	2
#define T_GROUP		3

/* Environment variable */
#define ENV_POSIXLY_CORRECT	"POSIXLY_CORRECT"

/*
 * Structures for I/O stats.
 * These are now dynamically allocated.
 */
struct io_stats {
	/* # of sectors read */
	unsigned long rd_sectors	__attribute__ ((aligned (8)));
	/* # of sectors written */
	unsigned long wr_sectors	__attribute__ ((packed));
	/* # of sectors discarded */
	unsigned long dc_sectors	__attribute__ ((packed));
	/* # of read operations issued to the device */
	unsigned long rd_ios		__attribute__ ((packed));
	/* # of read requests merged */
	unsigned long rd_merges		__attribute__ ((packed));
	/* # of write operations issued to the device */
	unsigned long wr_ios		__attribute__ ((packed));
	/* # of write requests merged */
	unsigned long wr_merges		__attribute__ ((packed));
	/* # of discard operations issued to the device */
	unsigned long dc_ios		__attribute__ ((packed));
	/* # of discard requests merged */
	unsigned long dc_merges		__attribute__ ((packed));
	/* # of flush requests issued to the device */
	unsigned long fl_ios		__attribute__ ((packed));
	/* Time of read requests in queue */
	unsigned int  rd_ticks		__attribute__ ((packed));
	/* Time of write requests in queue */
	unsigned int  wr_ticks		__attribute__ ((packed));
	/* Time of discard requests in queue */
	unsigned int  dc_ticks		__attribute__ ((packed));
	/* Time of flush requests in queue */
	unsigned int  fl_ticks		__attribute__ ((packed));
	/* # of I/Os in progress */
	unsigned int  ios_pgr		__attribute__ ((packed));
	/* # of ticks total (for this device) for I/O */
	unsigned int  tot_ticks		__attribute__ ((packed));
	/* # of ticks requests spent in queue */
	unsigned int  rq_ticks		__attribute__ ((packed));
};

#define IO_STATS_SIZE	(sizeof(struct io_stats))

struct io_device {
	char name[MAX_NAME_LEN];
	/*
	 * 0: Not a whole device (T_PART)
	 * 1: whole device (T_DEV)
	 * 2: whole device and all its partitions to be read (T_PART_DEV)
	 * 3+: group name (T_GROUP) (4 means 1 device in the group, 5 means 2 devices in the group, etc.)
	 */
	int dev_tp;
	/* TRUE if device exists in /proc/diskstats or /sys. Don't apply for groups. */
	int exist;
	/* major and minor numbers are set only for partitions (T_PART), not whole devices */
	int major;
	int minor;
	struct io_stats *dev_stats[2];
	struct io_device *next;
};

struct ext_io_stats {
	/* r_await */
	double r_await;
	/* w_await */
	double w_await;
	/* d_await */
	double d_await;
	/* f_await */
	double f_await;
	/* rsec/s */
	double rsectors;
	/* wsec/s */
	double wsectors;
	/* dsec/s */
	double dsectors;
	/* sec/s */
	double sectors;
	/* %rrqm */
	double rrqm_pc;
	/* %wrqm */
	double wrqm_pc;
	/* %drqm */
	double drqm_pc;
	/* rareq-sz */
	double rarqsz;
	/* wareq-sz */
	double warqsz;
	/* dareq-sz */
	double darqsz;
};

#endif  /* _IOSTAT_H */
