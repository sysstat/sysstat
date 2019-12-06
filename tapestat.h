/*
 * tapestat: report tape statistics
 * (C) 2015 Hewlett-Packard Development Company, L.P.
 * 
 * Initial revision by Shane M. SEYMOUR (shane.seymour <at> hpe.com)
 * Modified for sysstat by Sebastien GODARD (sysstat <at> orange.fr)
 */

#ifndef _TAPESTAT_H
#define _TAPESTAT_H

#include "common.h"

/* T_: tapestat - D_: Display - F_: Flag */
#define T_D_TIMESTAMP		0x00001
#define T_D_KILOBYTES		0x00002
#define T_D_MEGABYTES		0x00004
#define T_D_OMIT_SINCE_BOOT	0x00008
#define T_D_ISO			0x00010
#define T_D_ZERO_OMIT		0x00020
#define T_D_UNIT		0x00040

#define DISPLAY_TIMESTAMP(m)		(((m) & T_D_TIMESTAMP)       == T_D_TIMESTAMP)
#define DISPLAY_KILOBYTES(m)		(((m) & T_D_KILOBYTES)       == T_D_KILOBYTES)
#define DISPLAY_MEGABYTES(m)		(((m) & T_D_MEGABYTES)       == T_D_MEGABYTES)
#define DISPLAY_OMIT_SINCE_BOOT(m)	(((m) & T_D_OMIT_SINCE_BOOT) == T_D_OMIT_SINCE_BOOT)
#define DISPLAY_ISO(m)			(((m) & T_D_ISO)             == T_D_ISO)
#define DISPLAY_ZERO_OMIT(m)		(((m) & T_D_ZERO_OMIT)       == T_D_ZERO_OMIT)
#define DISPLAY_UNIT(m)			(((m) & T_D_UNIT)	     == T_D_UNIT)

#define TAPE_STATS_VALID 1
#define TAPE_STATS_INVALID 0

#define SYSFS_CLASS_TAPE_DIR 	PRE "/sys/class/scsi_tape"
#define TAPE_STAT_PATH		PRE "/sys/class/scsi_tape/st%i/stats/"

#define TAPE_STAT_FILE_VAL(A, B)					\
	snprintf(filename, MAXPATHLEN, A, i);				\
	if ((fp = fopen(filename, "r")) != NULL) {			\
		if (fscanf(fp, "%"PRId64, &tape_new_stats[i].B) != 1) {	\
			tape_new_stats[i].valid = TAPE_STATS_INVALID;	\
		}							\
		fclose(fp);						\
	} else {							\
		tape_new_stats[i].valid = TAPE_STATS_INVALID;		\
		continue;						\
	}


/*
 * A - tape_stats structure member name, e.g. read_count
 * B - calc_stats structure member name, e.g. reads_per_second
 *
 * These macros are not selfcontained they depend on some other
 * variables defined either as global or local to the function.
 */

#define CALC_STAT_CNT(A, B)					\
	if ((tape_new_stats[i].A == tape_old_stats[i].A) ||	\
		(duration <= 0)) {				\
		stats->B = 0;					\
	} else {						\
                temp = (double) (tape_new_stats[i].A -		\
			tape_old_stats[i].A)			\
			/ (((double) duration) / 1000);		\
		stats->B = (uint64_t) temp;			\
	}
#define CALC_STAT_KB(A, B)					\
        if ((tape_new_stats[i].A == tape_old_stats[i].A) ||	\
		(duration <= 0)) {				\
		stats->B = 0;					\
        } else {						\
		temp = (double) (tape_new_stats[i].A -		\
			tape_old_stats[i].A)			\
			/ (((double) duration) / 1000.0);	\
		stats->B = (uint64_t) (temp / 1024.0);		\
	}

#define TAPE_MAX_PCT 999

#define CALC_STAT_PCT(A, B)						\
	if ((tape_new_stats[i].A == tape_old_stats[i].A) ||		\
		(duration <= 0)) {					\
		stats->B = 0;						\
	} else {							\
		temp = (double) (tape_new_stats[i].A -			\
			tape_old_stats[i].A)				\
			/ (((double) duration));			\
		stats->B = (uint64_t) (100.0 * temp / 1000000.0);	\
		if (stats->B > TAPE_MAX_PCT)				\
			stats->B = TAPE_MAX_PCT;				\
	}

struct tape_stats {
        uint64_t read_time;
        uint64_t write_time;
        uint64_t other_time;
        uint64_t read_bytes;
        uint64_t write_bytes;
        uint64_t read_count;
        uint64_t write_count;
        uint64_t other_count;
        uint64_t resid_count;
        char valid;
        struct timeval tv;
};
struct calc_stats {
        uint64_t reads_per_second;
        uint64_t writes_per_second;
        uint64_t other_per_second;
        uint64_t kbytes_read_per_second;
        uint64_t kbytes_written_per_second;
        uint64_t read_pct_wait;
        uint64_t write_pct_wait;
        uint64_t all_pct_wait;
        uint64_t resids_per_second;
};

void tape_get_updated_stats(void);
void tape_gather_initial_stats(void);
void tape_check_tapes_and_realloc(void);
int get_max_tape_drives(void);
void tape_uninitialise(void);
void tape_initialise(void);
void tape_calc_one_stats(struct calc_stats *, int);
void tape_write_headings(void);
void tape_write_stats(struct calc_stats *, int);

#endif  /* _TAPESTAT_H */
