/*
 * raw_stats.h: Include file used to display statistics in raw format.
 * (C) 1999-2017 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _RAW_STATS_H
#define _RAW_STATS_H

#include "common.h"

/*
 ***************************************************************************
 * Prototypes for functions used to display statistics in raw format.
 ***************************************************************************
 */

__print_funct_t raw_print_cpu_stats
	(struct activity *, char *, int);
__print_funct_t raw_print_pcsw_stats
	(struct activity *, char *, int);

#endif /* _RAW_STATS_H */
