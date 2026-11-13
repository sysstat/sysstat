/*
 * pcp_iostat.h: Read disk I/O statistics from a PCP archive for iostat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 */

#ifndef _PCP_IOSTAT_H
#define _PCP_IOSTAT_H

#ifdef HAVE_PCP

int pcp_iostat_run(const char *archive);

#else

#define pcp_iostat_run(a) (fprintf(stderr, _("PCP archive support unavailable, cannot read %s\n"), archive), 1)

#endif
#endif /* _PCP_IOSTAT_H */
