/*
 * pcp_mpstat.h: Read CPU statistics from a PCP archive for mpstat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_MPSTAT_H
#define _PCP_MPSTAT_H

#ifdef HAVE_PCP

int pcp_mpstat_run(const char *archive);

#else

#define pcp_mpstat_run(a) (fprintf(stderr, _("PCP archive support unavailable, cannot read %s\n"), archive), 1)

#endif
#endif /* _PCP_MPSTAT_H */
