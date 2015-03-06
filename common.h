/*
 * sysstat: System performance tools for Linux
 * (C) 1999-2014 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _COMMON_H
#define _COMMON_H

/* Maximum length of sensors device name */
#define MAX_SENSORS_DEV_LEN	20

#include <time.h>
#include <sched.h>	/* For __CPU_SETSIZE */
#include "rd_stats.h"

/*
 ***************************************************************************
 * Various keywords and constants
 ***************************************************************************
 */

#define FALSE	0
#define TRUE	1

#define DISP_HDR	1

/* Number of seconds per day */
#define SEC_PER_DAY	3600 * 24

/* Maximum number of CPUs */
#if defined(__CPU_SETSIZE) && __CPU_SETSIZE > 2048
#define NR_CPUS		__CPU_SETSIZE
#else
#define NR_CPUS		2048
#endif

/* Maximum number of interrupts */
#define NR_IRQS			1024

/* Size of /proc/interrupts line, CPU data excluded */
#define INTERRUPTS_LINE	128

/* Keywords */
#define K_ISO	"ISO"
#define K_ALL	"ALL"
#define K_UTC	"UTC"

/* Files */
#define STAT			"/proc/stat"
#define UPTIME			"/proc/uptime"
#define DISKSTATS		"/proc/diskstats"
#define INTERRUPTS		"/proc/interrupts"
#define MEMINFO			"/proc/meminfo"
#define SYSFS_BLOCK		"/sys/block"
#define SYSFS_DEV_BLOCK		"/sys/dev/block"
#define SYSFS_DEVCPU		"/sys/devices/system/cpu"
#define SYSFS_TIME_IN_STATE	"cpufreq/stats/time_in_state"
#define S_STAT			"stat"
#define DEVMAP_DIR		"/dev/mapper"
#define DEVICES			"/proc/devices"
#define SYSFS_USBDEV		"/sys/bus/usb/devices"
#define DEV_DISK_BY		"/dev/disk/by"
#define SYSFS_IDVENDOR		"idVendor"
#define SYSFS_IDPRODUCT		"idProduct"
#define SYSFS_BMAXPOWER		"bMaxPower"
#define SYSFS_MANUFACTURER	"manufacturer"
#define SYSFS_PRODUCT		"product"
#define SYSFS_HBA               "/sys/class/fc_host" 

#define MAX_FILE_LEN		256
#define MAX_PF_NAME		1024
#define DEFAULT_DEVMAP_MAJOR	253
#define MAX_NAME_LEN		72

#define IGNORE_VIRTUAL_DEVICES	FALSE
#define ACCEPT_VIRTUAL_DEVICES	TRUE

/* Environment variables */
#define ENV_TIME_FMT		"S_TIME_FORMAT"
#define ENV_TIME_DEFTM		"S_TIME_DEF_TIME"

#define DIGITS			"0123456789"

/*
 ***************************************************************************
 * Macro functions definitions.
 ***************************************************************************
 */

/* Allocate and init structure */
#define SREALLOC(S, TYPE, SIZE)	do {								 \
   					TYPE *_p_;						 \
				   	_p_ = S;						 \
   				   	if (SIZE) {						 \
   				      		if ((S = (TYPE *) realloc(S, (SIZE))) == NULL) { \
				         		perror("realloc");			 \
				         		exit(4);				 \
				      		}						 \
				      		/* If the ptr was null, then it's a malloc() */	 \
   				      		if (!_p_)					 \
      				         		memset(S, 0, (SIZE));			 \
				   	}							 \
				} while (0)

/*
 * Macros used to display statistics values.
 *
 * NB: Define SP_VALUE() to normalize to %;
 * HZ is 1024 on IA64 and % should be normalized to 100.
 */
#define S_VALUE(m,n,p)	(((double) ((n) - (m))) / (p) * HZ)
#define SP_VALUE(m,n,p)	(((double) ((n) - (m))) / (p) * 100)

/*
 * Under very special circumstances, STDOUT may become unavailable.
 * This is what we try to guess here.
 */
#define TEST_STDOUT(_fd_)	do {					\
					if (write(_fd_, "", 0) == -1) {	\
				        	perror("stdout");	\
				       		exit(6);		\
				 	}				\
				} while (0)

#define MINIMUM(a,b)	((a) < (b) ? (a) : (b))

#define PANIC(m)	sysstat_panic(__FUNCTION__, m)

/* Number of ticks per second */
#define HZ		hz
extern unsigned int hz;

/* Number of bit shifts to convert pages to kB */
extern unsigned int kb_shift;

/*
 * kB <-> number of pages.
 * Page size depends on machine architecture (4 kB, 8 kB, 16 kB, 64 kB...)
 */
#define KB_TO_PG(k)	((k) >> kb_shift)
#define PG_TO_KB(k)	((k) << kb_shift)

/* Type of persistent device names used in sar and iostat */
extern char persistent_name_type[MAX_FILE_LEN];

/*
 ***************************************************************************
 * Structures definitions
 ***************************************************************************
 */

/* Structure used for extended disk statistics */
struct ext_disk_stats {
	double util;
	double await;
	double svctm;
	double arqsz;
};

/*
 ***************************************************************************
 * Functions prototypes
 ***************************************************************************
 */

extern void
	compute_ext_disk_stats(struct stats_disk *, struct stats_disk *,
			       unsigned long long, struct ext_disk_stats *);
extern int
	count_bits(void *, int);
extern int
	count_csvalues(int, char **);
extern char *
	device_name(char *);
extern void
	get_HZ(void);
extern unsigned int
	get_devmap_major(void);
extern unsigned long long
	get_interval(unsigned long long, unsigned long long);
extern void
	get_kb_shift(void);
extern time_t
	get_localtime(struct tm *, int);
extern time_t
	get_time(struct tm *, int);
unsigned long long
	get_per_cpu_interval(struct stats_cpu *, struct stats_cpu *);
extern char *
	get_persistent_name_from_pretty(char *);
extern char *
	get_persistent_type_dir(char *);
extern char *
	get_pretty_name_from_persistent(char *);
extern int
	get_sysfs_dev_nr(int);
extern int
	get_win_height(void);
extern void
	init_nls(void);
extern int
	is_device(char *, int);
extern double
	ll_sp_value(unsigned long long, unsigned long long, unsigned long long);
extern int
	print_gal_header(struct tm *, char *, char *, char *, char *, int);
extern void
	print_version(void);
extern char *
	strtolower(char *);
extern void
	sysstat_panic(const char *, int);

#endif  /* _COMMON_H */
