/*
 * pcp_def_metrics.h: Include file used to define PCP metrics.
 * (C) 2019 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _PCP_DEF_METRICS_H
#define _PCP_DEF_METRICS_H

/*
 ***************************************************************************
 * Prototypes for functions used to define PCP metrics.
 ***************************************************************************
 */

void pcp_def_cpu_metrics(struct activity *);
void pcp_def_pcsw_metrics(void);
void pcp_def_irq_metrics(struct activity *);
void pcp_def_memory_metrics(struct activity *);
void pcp_def_queue_metrics(void);

#endif /* _PCP_DEF_METRICS_H */
