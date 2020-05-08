/*
 * mpstat: per-processor statistics
 * (C) 2000-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _MPSTAT_H
#define _MPSTAT_H

#include "common.h"

/*
 ***************************************************************************
 * mpstat's specific system files.
 ***************************************************************************
 */

#define SOFTIRQS	PRE "/proc/softirqs"
#define PHYS_PACK_ID	"topology/pysical_package_id"
#define THREAD_SBL_LST	"topology/thread_siblings_list"

/*
 ***************************************************************************
 * Activities definitions.
 ***************************************************************************
 */

#define M_D_CPU		0x0001
#define M_D_IRQ_SUM	0x0002
#define M_D_IRQ_CPU	0x0004
#define M_D_SOFTIRQS	0x0008
#define M_D_NODE	0x0010

#define DISPLAY_CPU(m)		(((m) & M_D_CPU) == M_D_CPU)
#define DISPLAY_IRQ_SUM(m)	(((m) & M_D_IRQ_SUM) == M_D_IRQ_SUM)
#define DISPLAY_IRQ_CPU(m)	(((m) & M_D_IRQ_CPU) == M_D_IRQ_CPU)
#define DISPLAY_SOFTIRQS(m)	(((m) & M_D_SOFTIRQS) == M_D_SOFTIRQS)
#define DISPLAY_NODE(m)		(((m) & M_D_NODE) == M_D_NODE)

/*
 ***************************************************************************
 * Keywords and constants.
 ***************************************************************************
 */

/* Indicate that option -P has been used */
#define F_OPTION_P	0x01
/* Indicate that option -A has been used */
#define F_OPTION_A	0x02
/* JSON output */
#define F_JSON_OUTPUT	0x04
/* Indicate that option -N has been used */
#define F_OPTION_N	0x08
/* Display topology */
#define F_TOPOLOGY	0x10

#define USE_OPTION_P(m)		(((m) & F_OPTION_P) == F_OPTION_P)
#define USE_OPTION_A(m)		(((m) & F_OPTION_A) == F_OPTION_A)
#define DISPLAY_JSON_OUTPUT(m)	(((m) & F_JSON_OUTPUT) == F_JSON_OUTPUT)
#define USE_OPTION_N(m)		(((m) & F_OPTION_N) == F_OPTION_N)
#define DISPLAY_TOPOLOGY(m)	(((m) & F_TOPOLOGY) == F_TOPOLOGY)

#define K_SUM	"SUM"
#define K_CPU	"CPU"
#define K_SCPU	"SCPU"

#define NR_IRQCPU_PREALLOC	3

#define MAX_IRQ_LEN		16

/*
 ***************************************************************************
 * Structures used to store statistics.
 ***************************************************************************
 */

/*
 * stats_irqcpu->irq_name:  IRQ#-A
 * stats_irqcpu->interrupt: number of IRQ#-A for proc 0
 * stats_irqcpu->irq_name:  IRQ#-B
 * stats_irqcpu->interrupt: number of IRQ#-B for proc 0
 * ...
 * stats_irqcpu->irq_name:  (undef'd)
 * stats_irqcpu->interrupt: number of IRQ#-A for proc 1
 * stats_irqcpu->irq_name:  (undef'd)
 * stats_irqcpu->interrupt: number of IRQ#-B for proc 1
 * ...
 */
struct stats_irqcpu {
	unsigned int interrupt        __attribute__ ((aligned (4)));
	char         irq_name[MAX_IRQ_LEN];
};

struct cpu_topology {
	int phys_package_id;
	int logical_core_id;
};

#define STATS_IRQCPU_SIZE      (sizeof(struct stats_irqcpu))

#endif
