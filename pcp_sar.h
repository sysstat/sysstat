/*
 * pcp_sar.h: PCP archive replay for sar.
 * (C) 2025-2026 Red Hat, Inc.
 */

#ifndef _PCP_SAR_H
#define _PCP_SAR_H

#include <stdint.h>

#ifdef HAVE_PCP
int try_read_stats_from_pcpfile(const char *from_file, uint64_t flags);
#else
static inline int
try_read_stats_from_pcpfile(const char *f __attribute__((unused)),
			    uint64_t fl __attribute__((unused)))
	{ return 0; }
#endif

#endif /* _PCP_SAR_H */
