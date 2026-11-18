/*
 * pcp_pidstat.h: Read per-process statistics from a PCP archive for pidstat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_PIDSTAT_H
#define _PCP_PIDSTAT_H

#ifdef HAVE_PCP

int pcp_pidstat_run(const char *archive);

#else /* !HAVE_PCP */

#define pcp_pidstat_run(a) (fprintf(stderr, _("PCP archive support unavailable, cannot read %s\n"), archive), 1)

#endif /* HAVE_PCP */
#endif /* _PCP_PIDSTAT_H */
