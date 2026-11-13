/*
 * pcp_cifsiostat.h: Read CIFS statistics from a PCP archive for cifsiostat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_CIFSIOSTAT_H
#define _PCP_CIFSIOSTAT_H

#ifdef HAVE_PCP

int pcp_cifsiostat_run(const char *archive);

#else

#define pcp_cifsiostat_run(a) (fprintf(stderr, _("PCP archive support unavailable, cannot read %s\n"), archive), 1)

#endif
#endif /* _PCP_CIFSIOSTAT_H */
