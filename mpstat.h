/*
 * mpstat: per-processor statistics
 * (C) 2000-2019 by Sebastien Godard (sysstat <at> orange.fr)
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
#define F_P_OPTION	0x01
/* 0x02: unused */
/* JSON output */
#define F_JSON_OUTPUT	0x04
/* Indicate that option -N has been used */
#define F_N_OPTION	0x08

#define USE_P_OPTION(m)		(((m) & F_P_OPTION) == F_P_OPTION)
#define DISPLAY_JSON_OUTPUT(m)	(((m) & F_JSON_OUTPUT) == F_JSON_OUTPUT)
#define USE_N_OPTION(m)		(((m) & F_N_OPTION) == F_N_OPTION)

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

#define STATS_IRQCPU_SIZE      (sizeof(struct stats_irqcpu))

#endif
