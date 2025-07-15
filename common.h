/*
 * sysstat: System performance tools for Linux
 * (C) 1999-2025 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _COMMON_H
#define _COMMON_H

/* Maximum length of sensors device name */
#define MAX_SENSORS_DEV_LEN	20

#include <stdio.h>
#include <time.h>
#include <sched.h>	/* For __CPU_SETSIZE */
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include "systest.h"

#ifdef HAVE_SYS_SYSMACROS_H
/* Needed on some non-glibc environments */
#include <sys/sysmacros.h>
#endif

/*
 ***************************************************************************
 * Various keywords and constants
 ***************************************************************************
 */

#define FALSE	0
#define TRUE	1

#define PLAIN_OUTPUT	0

#define DISP_HDR	1

/* Index in units array (see common.c) */
#define NO_UNIT		-1

enum {
	UNIT_SECTOR	= 0,
	UNIT_BYTE	= 1,
	UNIT_KILOBYTE	= 2
};

#define NR_UNITS	8

/* Timestamp buffer length */
#define TIMESTAMP_LEN	64

/* Number of seconds per day */
#define SEC_PER_DAY	3600 * 24

/* Maximum number of CPUs */
#if defined(__CPU_SETSIZE) && __CPU_SETSIZE > 8192
#define NR_CPUS		__CPU_SETSIZE
#else
#define NR_CPUS		8192
#endif

/* Maximum number of interrupts */
#define NR_IRQS		4096

/* Size of /proc/interrupts line, CPU data excluded */
#define INTERRUPTS_LINE	128

/* Keywords */
#define K_ISO		"ISO"
#define K_ALL		"ALL"
#define K_LOWERALL	"all"
#define K_LOWERSUM	"sum"
#define K_UTC		"UTC"
#define K_JSON		"JSON"

/* Flags used with multiple commands (iostat, cifsiostat...) */
#define X_D_DEBUG		0x01
#define X_D_ISO			0x02
#define X_D_JSON_OUTPUT		0x04
#define X_D_SEC_EPOCH		0x08

#define DISPLAY_DEBUG(m)	(((m) & X_D_DEBUG)       == X_D_DEBUG)
#define DISPLAY_ISO(m)		(((m) & X_D_ISO)         == X_D_ISO)
#define DISPLAY_JSON_OUTPUT(m)	(((m) & X_D_JSON_OUTPUT) == X_D_JSON_OUTPUT)
#define DISPLAY_SEC_EPOCH(m)	(((m) & X_D_SEC_EPOCH)   == X_D_SEC_EPOCH)

/* Files */
#define __DISKSTATS		"diskstats"
#define __BLOCK			"block"
#define __DEV_BLOCK		"dev/block"
#define SLASH_SYS		PRE "/sys"
#define SLASH_DEV		PRE "/dev/"
#define STAT			PRE "/proc/stat"
#define UPTIME			PRE "/proc/uptime"
#define DISKSTATS		PRE "/proc/" __DISKSTATS
#define INTERRUPTS		PRE "/proc/interrupts"
#define MEMINFO			PRE "/proc/meminfo"
#define SYSFS_BLOCK		SLASH_SYS "/" __BLOCK
#define SYSFS_DEV_BLOCK		SLASH_SYS "/" __DEV_BLOCK
#define SYSFS_DEVCPU		PRE "/sys/devices/system/cpu"
#define S_STAT			"stat"
#define DEVMAP_DIR		PRE "/dev/mapper"
#define DEVICES			PRE "/proc/devices"
#define DEV_DISK_BY		PRE "/dev/disk/by"
#define DEV_DISK_BY_ID		PRE "/dev/disk/by-id"

#define MAX_FILE_LEN		512
#define MAX_PF_NAME		1024
#define MAX_NAME_LEN		128

#define IGNORE_VIRTUAL_DEVICES	FALSE
#define ACCEPT_VIRTUAL_DEVICES	TRUE
#define LOCAL_TIME		FALSE

/* Environment variables */
#define ENV_TIME_FMT		"S_TIME_FORMAT"
#define ENV_TIME_DEFTM		"S_TIME_DEF_TIME"
#define ENV_COLORS		"S_COLORS"
#define ENV_COLORS_SGR		"S_COLORS_SGR"
#define ENV_REPEAT_HEADER	"S_REPEAT_HEADER"

#define C_NEVER			"never"
#define C_ALWAYS		"always"

#define DIGITS			"0123456789"
#define XDIGITS			"0123456789-"

/* Batteries status */
enum {
	BAT_STS_UNKNOWN		= 0,
	BAT_STS_CHARGING	= 1,
	BAT_STS_DISCHARGING	= 2,
	BAT_STS_NOTCHARGING	= 3,
	BAT_STS_FULL		= 4
};

/* Number of different statuses */
#define BAT_STS_NR	5

/*
 ***************************************************************************
 * Macro functions definitions.
 ***************************************************************************
 */

/*
 * Macro used to define activity bitmap size.
 * All those bitmaps have an additional bit used for global activity
 * (eg. CPU "all" or total number of interrupts). That's why we do "(m) + 1".
 */
#define BITMAP_SIZE(m)	((((m) + 1) >> 3) + 1)

/* Allocate and init structure */
#ifdef DEBUG
#define SREALLOC(S, TYPE, SIZE)	do {								   \
					size_t _sz_ = (SIZE);					   \
					if ((_sz_) == 0) {					   \
						 /* SIZE may be zero when an overflow happens */   \
						 /* when two non-zero values are multiplied   */   \
						 /* together.				      */   \
						 fprintf(stderr, "%s: SREALLOC: SIZE is zero!\n", __FUNCTION__); \
						 exit(4); 					   \
					}							   \
					__SREALLOC(S, TYPE, _sz_);				   \
				} while (0)
#else
#define SREALLOC(S, TYPE, SIZE)	do {								   \
					size_t _sz_ = (SIZE);					   \
					if ((_sz_) == 0) {					   \
						exit(4); 					   \
					}							   \
					__SREALLOC(S, TYPE, _sz_);				   \
				} while (0)
#endif

#define __SREALLOC(S, TYPE, SIZE)	do {							   \
						TYPE *_p_;					   \
						if ((_p_ = (TYPE *) realloc(S, (SIZE))) == NULL) { \
							perror("realloc");			   \
							if (S) {				   \
								free(S);			   \
							}					   \
							exit(4);				   \
						}						   \
						/* If the ptr was null, then it's a malloc() */	   \
						if (!S) {					   \
							memset(_p_, 0, (SIZE));			   \
						}						   \
						S = _p_;					   \
					} while (0)

/* Set CPU in given bitmap   */
#define SET_CPU_BITMAP(bitmap, cpu)	bitmap[(cpu) >> 3] |= 1 << ((cpu) & 0x07)
/* Check if CPU is set in bitmap */
#define IS_CPU_SET(bitmap, cpu)		(bitmap[(cpu) >> 3] & (1 << ((cpu) & 0x07)))
/* Mark CPU as offline in dedicated CPU bitmap */
#define MARK_CPU_OFFLINE(bitmap, cpu) 	SET_CPU_BITMAP(bitmap, cpu)
/* Check if given CPU is offline */
#define IS_CPU_OFFLINE(bitmap, cpu)	IS_CPU_SET(bitmap, cpu)
/* Check if given CPU has been selected */
#define IS_CPU_SELECTED(bitmap, cpu)	IS_CPU_SET(bitmap, cpu)

/*
 * Macros used to display statistics values.
 *
 */
/* With S_VALUE macro, the interval of time (@p) is given in 1/100th of a second */
#define S_VALUE(m,n,p)		(((double) ((n) - (m))) / (p) * 100)
/* Define SP_VALUE() to normalize to % */
#define SP_VALUE(m,n,p)		(((double) ((n) - (m))) / (p) * 100)

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
extern unsigned long hz;

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
 * Colors definitions
 ***************************************************************************
 */

#define C_LIGHT_RED	"\e[31;22m"
#define C_BOLD_RED	"\e[31;1m"
#define C_LIGHT_GREEN	"\e[32;22m"
#define C_BOLD_GREEN	"\e[32;1m"
#define C_LIGHT_YELLOW	"\e[33;22m"
#define C_BOLD_MAGENTA	"\e[35;1m"
#define C_BOLD_BLUE	"\e[34;1m"
#define C_LIGHT_BLUE	"\e[34;22m"
#define C_NORMAL	"\e[0m"

#define PERCENT_LIMIT_XHIGH	90.0
#define PERCENT_LIMIT_HIGH	75.0
#define PERCENT_LIMIT_LOW	25.0
#define PERCENT_LIMIT_XLOW	10.0

enum {
	XHIGH	= 1,
	XLOW	= 2,
	XLOW0	= 3
};

#define MAX_SGR_LEN	16

enum {
	IS_INT		= 0,
	IS_STR		= 1,
	IS_RESTART	= 2,
	IS_COMMENT	= 3,
	IS_ZERO		= 4,
	IS_DEBUG	= IS_RESTART
};

/*
 ***************************************************************************
 * Structures definitions
 ***************************************************************************
 */

/* Structure used for extended disk statistics */
struct ext_disk_stats {
	double util;
	double await;
	double arqsz;
};

/*
 ***************************************************************************
 * Functions prototypes
 ***************************************************************************
 */
void print_version
	(char *[], int);
void get_HZ
	(void);
void get_kb_shift
	(void);
time_t get_xtime
	(struct tm *, int, int);
time_t get_time
	(struct tm *, int);
void init_nls
	(void);
int is_device
	(char *, char *, int);
void sysstat_panic
	(const char *, int);
int extract_wwnid
	(char *, unsigned long long *, unsigned int *);
int get_wwnid_from_pretty
	(char *, unsigned long long *, unsigned int *);
int check_dir
	(char *);
size_t mul_check_overflow3
	(size_t, size_t, size_t);
size_t mul_check_overflow4
	(size_t, size_t, size_t, size_t);

#ifndef SOURCE_SADC
int count_bits
	(void *, int);
int count_csvalues
	(int, char **);
void cprintf_f
	(int, int, int, int, int, ...);
void cprintf_in
	(int, char *, char *, int);
void cprintf_xpc
	(int, int, int, int, int, ...);
void cprintf_s
	(int, char *, char *);
void cprintf_u64
	(int, int, int, ...);
void cprintf_x
	(int, int, ...);
void cprintf_tr
	(int, char *, char *);
char *device_name
	(char *);
char *escape_bs_char
	(char *);
char *get_device_name
	(unsigned int, unsigned int, unsigned long long [],
	 unsigned int, unsigned int, unsigned int, unsigned int, char *);
unsigned int get_devmap_major
	(void);
unsigned long long get_interval
	(unsigned long long, unsigned long long);
char *get_persistent_name_from_pretty
	(char *);
char *get_persistent_type_dir
	(char *);
char *get_pretty_name_from_persistent
	(char *);
int get_sysfs_dev_nr
	(int);
int get_win_height
	(void);
void init_colors
	(void);
double ll_sp_value
	(unsigned long long, unsigned long long, unsigned long long);
int is_iso_time_fmt
	(void);
int parse_range_values
	(char *t, int, int *, int *);
int parse_values
	(char *, unsigned char[], int, const char *);
int print_gal_header
	(struct tm *, char *, char *, char *, char *, int, int);
int set_report_date
	(struct tm *, char[], int);
char *strtolower
	(char *);
void write_sample_timestamp
	(int, struct tm *, uint64_t);
void xprintf
	(int, const char *, ...);
void xprintf0
	(int, const char *, ...);

#endif /* SOURCE_SADC undefined */
#endif  /* _COMMON_H */
