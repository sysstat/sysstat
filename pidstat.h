/*
 * pidstat: Display per-process statistics.
 * (C) 2007-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */
#ifndef _PIDSTAT_H
#define _PIDSTAT_H

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
/* sys/param.h defines HZ but needed for _POSIX_ARG_MAX and LOGIN_NAME_MAX */
#undef HZ
#endif
#include "common.h"

#define K_SELF		"SELF"

#define K_P_TASK	"TASK"
#define K_P_CHILD	"CHILD"
#define K_P_ALL		"ALL"

#ifdef _POSIX_ARG_MAX
#define MAX_COMM_LEN    _POSIX_ARG_MAX
#define MAX_CMDLINE_LEN _POSIX_ARG_MAX
#else
#define MAX_COMM_LEN    128
#define MAX_CMDLINE_LEN 128
#endif

#ifdef LOGIN_NAME_MAX
#define MAX_USER_LEN    LOGIN_NAME_MAX
#else
#define MAX_USER_LEN    32
#endif

/* Activities */
#define P_A_CPU		0x01
#define P_A_MEM		0x02
#define P_A_IO		0x04
#define P_A_CTXSW	0x08
#define P_A_STACK	0x10
#define P_A_KTAB	0x20
#define P_A_RT		0x40

#define DISPLAY_CPU(m)		(((m) & P_A_CPU) == P_A_CPU)
#define DISPLAY_MEM(m)		(((m) & P_A_MEM) == P_A_MEM)
#define DISPLAY_IO(m)		(((m) & P_A_IO) == P_A_IO)
#define DISPLAY_CTXSW(m)	(((m) & P_A_CTXSW) == P_A_CTXSW)
#define DISPLAY_STACK(m)	(((m) & P_A_STACK) == P_A_STACK)
#define DISPLAY_KTAB(m)		(((m) & P_A_KTAB) == P_A_KTAB)
#define DISPLAY_RT(m)		(((m) & P_A_RT) == P_A_RT)

/* TASK/CHILD */
#define P_NULL		0x00
#define P_TASK		0x01
#define P_CHILD		0x02

#define DISPLAY_TASK_STATS(m)	(((m) & P_TASK) == P_TASK)
#define DISPLAY_CHILD_STATS(m)	(((m) & P_CHILD) == P_CHILD)

#define P_D_PID		0x0001
#define P_D_ALL_PID	0x0002
#define P_F_IRIX_MODE	0x0004
#define P_F_COMMSTR	0x0008
#define P_D_ACTIVE_PID	0x0010
#define P_D_TID		0x0020
#define P_D_ONELINE	0x0040
#define P_D_CMDLINE	0x0080
#define P_D_USERNAME	0x0100
#define P_F_USERSTR	0x0200
#define P_F_PROCSTR	0x0400
#define P_D_UNIT	0x0800
#define P_D_SEC_EPOCH	0x1000

#define DISPLAY_PID(m)		(((m) & P_D_PID) == P_D_PID)
#define DISPLAY_ALL_PID(m)	(((m) & P_D_ALL_PID) == P_D_ALL_PID)
#define IRIX_MODE_OFF(m)	(((m) & P_F_IRIX_MODE) == P_F_IRIX_MODE)
#define COMMAND_STRING(m)	(((m) & P_F_COMMSTR) == P_F_COMMSTR)
#define DISPLAY_ACTIVE_PID(m)	(((m) & P_D_ACTIVE_PID) == P_D_ACTIVE_PID)
#define DISPLAY_TID(m)		(((m) & P_D_TID) == P_D_TID)
#define DISPLAY_ONELINE(m)	(((m) & P_D_ONELINE) == P_D_ONELINE)
#define DISPLAY_CMDLINE(m)	(((m) & P_D_CMDLINE) == P_D_CMDLINE)
#define DISPLAY_USERNAME(m)	(((m) & P_D_USERNAME) == P_D_USERNAME)
#define USER_STRING(m)		(((m) & P_F_USERSTR) == P_F_USERSTR)
#define PROCESS_STRING(m)	(((m) & P_F_PROCSTR) == P_F_PROCSTR)
#define DISPLAY_UNIT(m)		(((m) & P_D_UNIT) == P_D_UNIT)
#define PRINT_SEC_EPOCH(m)	(((m) & P_D_SEC_EPOCH) == P_D_SEC_EPOCH)

/* Per-process flags */
#define F_NO_PID_IO	0x01
#define F_NO_PID_FD	0x02
#define F_PID_DISPLAYED	0x04

#define NO_PID_IO(m)		(((m) & F_NO_PID_IO) == F_NO_PID_IO)
#define NO_PID_FD(m)		(((m) & F_NO_PID_FD) == F_NO_PID_FD)
#define IS_PID_DISPLAYED(m)	(((m) & F_PID_DISPLAYED) == F_PID_DISPLAYED)


#define PROC		PRE "/proc"

#define PID_STAT	PRE "/proc/%u/stat"
#define PID_STATUS	PRE "/proc/%u/status"
#define PID_IO		PRE "/proc/%u/io"
#define PID_CMDLINE	PRE "/proc/%u/cmdline"
#define PID_SMAP	PRE "/proc/%u/smaps"
#define PID_FD		PRE "/proc/%u/fd"
#define PID_SCHED	PRE "/proc/%u/schedstat"

#define PROC_TASK	PRE "/proc/%u/task"
#define TASK_STAT	PRE "/proc/%u/task/%u/stat"
#define TASK_SCHED	PRE "/proc/%u/task/%u/schedstat"
#define TASK_STATUS	PRE "/proc/%u/task/%u/status"
#define TASK_IO		PRE "/proc/%u/task/%u/io"
#define TASK_CMDLINE	PRE "/proc/%u/task/%u/cmdline"
#define TASK_SMAP	PRE "/proc/%u/task/%u/smaps"
#define TASK_FD		PRE "/proc/%u/task/%u/fd"

#define PRINT_ID_HDR(_timestamp_, _flag_)	do {						\
							printf("\n%-11s", _timestamp_);	\
							if (DISPLAY_USERNAME(_flag_)) {		\
								printf("     USER");		\
							}					\
							else {					\
								printf("   UID");		\
							}					\
   							if (DISPLAY_TID(_flag_)) {		\
								printf("      TGID       TID");	\
							}					\
							else {					\
								printf("       PID");		\
							}					\
						} while (0)

/* Normally defined in <linux/sched.h> */
#ifndef SCHED_NORMAL
#define SCHED_NORMAL	0
#endif
#ifndef SCHED_FIFO
#define SCHED_FIFO	1
#endif
#ifndef SCHED_RR
#define SCHED_RR	2
#endif
#ifndef SCHED_BATCH
#define SCHED_BATCH	3
#endif
/* SCHED_ISO not yet implemented */
#ifndef SCHED_IDLE
#define SCHED_IDLE	5
#endif
#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE	6
#endif

#define GET_POLICY(p) \
	(p == SCHED_NORMAL   ? "NORMAL" : \
	(p == SCHED_FIFO     ? "FIFO" : \
	(p == SCHED_RR       ? "RR" : \
	(p == SCHED_BATCH    ? "BATCH" : \
	(p == SCHED_IDLE     ? "IDLE" : \
	(p == SCHED_DEADLINE ? "DEADLN" : \
	"?"))))))

struct pid_stats {
	unsigned long long read_bytes			__attribute__ ((aligned (8)));
	unsigned long long write_bytes			__attribute__ ((packed));
	unsigned long long cancelled_write_bytes	__attribute__ ((packed));
	unsigned long long blkio_swapin_delays		__attribute__ ((packed));
	unsigned long long minflt			__attribute__ ((packed));
	unsigned long long cminflt			__attribute__ ((packed));
	unsigned long long majflt			__attribute__ ((packed));
	unsigned long long cmajflt			__attribute__ ((packed));
	unsigned long long utime			__attribute__ ((packed));
	long long          cutime			__attribute__ ((packed));
	unsigned long long stime			__attribute__ ((packed));
	long long          cstime			__attribute__ ((packed));
	unsigned long long gtime			__attribute__ ((packed));
	long long          cgtime			__attribute__ ((packed));
	unsigned long long wtime			__attribute__ ((packed));
	unsigned long long vsz				__attribute__ ((packed));
	unsigned long long rss				__attribute__ ((packed));
	unsigned long      nvcsw			__attribute__ ((packed));
	unsigned long      nivcsw			__attribute__ ((packed));
	unsigned long      stack_size			__attribute__ ((packed));
	unsigned long      stack_ref			__attribute__ ((packed));
	unsigned int       processor			__attribute__ ((packed));
	unsigned int       priority			__attribute__ ((packed));
	unsigned int       policy			__attribute__ ((packed));
	unsigned int       threads			__attribute__ ((packed));
	unsigned int       fd_nr			__attribute__ ((packed));
};

#define PID_STATS_SIZE	(sizeof(struct pid_stats))

struct st_pid {
	unsigned long long total_vsz;
	unsigned long long total_rss;
	unsigned long long total_stack_size;
	unsigned long long total_stack_ref;
	unsigned long long total_threads;
	unsigned long long total_fd_nr;
	pid_t		   pid;
	uid_t		   uid;
	int		   exist;	/* TRUE if PID exists */
	unsigned int	   flags;
	unsigned int	   rt_asum_count;
	unsigned int	   rc_asum_count;
	unsigned int	   uc_asum_count;
	unsigned int	   tf_asum_count;
	unsigned int	   sk_asum_count;
	unsigned int	   delay_asum_count;
	struct pid_stats  *pstats[3];
	struct st_pid	  *tgid;	/* If current task is a TID, pointer to its TGID. NULL otherwise. */
	struct st_pid	  *next;
	char		   comm[MAX_COMM_LEN];
	char		   cmdline[MAX_CMDLINE_LEN];
};

#endif  /* _PIDSTAT_H */
