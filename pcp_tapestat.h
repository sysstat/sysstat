/*
 * pcp_tapestat.h: Read tape statistics from a PCP archive for tapestat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_TAPESTAT_H
#define _PCP_TAPESTAT_H

#ifdef HAVE_PCP

int pcp_tapestat_run(const char *archive);

#else

#define pcp_tapestat_run(a) (fprintf(stderr, _("PCP archive support unavailable, cannot read %s\n"), archive), 1)

#endif
#endif /* _PCP_TAPESTAT_H */
