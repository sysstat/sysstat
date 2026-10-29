/*
 * pcp_compat.h: Compatibility layer for building against older PCP releases.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com>)
 *
 * PCP 7 bumped PMAPI to version 4 and switched the timestamps carried by
 * pmLogLabel, pmSetMode() and pmResult from struct timeval to struct
 * timespec.  It also changed the pmiWrite() prototype to take nanoseconds
 * and added the PMI_APPEND write mode plus a number of helper routines
 * (pmiSetZoneinfo(), pmiSetVolumeSize(), pmiSetImportProgram(),
 * pmiExtraUnits() and pmiPutAtomValueHandle()).
 *
 * Rather than converting between timeval and timespec everywhere, the
 * helpers below alias the high resolution PMAPI interfaces that have always
 * carried struct timespec timestamps.  On PCP 7 those interfaces were
 * renamed into the default ones, so the aliases collapse to the plain names.
 * Features that have no older equivalent remain conditional on
 * HAVE_PMI_APPEND.
 */

#ifndef _PCP_COMPAT_H
#define _PCP_COMPAT_H

#ifdef HAVE_PCP

#include <pcp/pmapi.h>
#include <pcp/import.h>

/*
 * Detect the header generation rather than the PMAPI_VERSION value: the
 * latter may be overridden on the command line, but the ABI exposed by the
 * installed headers is fixed.
 */
#if defined(PMAPI_VERSION_4)

typedef pmLogLabel		pcp_log_label_t;
typedef pmResult		pcp_result_t;

#define pcp_get_archive_label(l)	pmGetArchiveLabel(l)
#define pcp_set_mode(m, w, d)		pmSetMode((m), (w), (d))
#define pcp_fetch(n, p, r)		pmFetch((n), (p), (r))
#define pcp_free_result(r)		pmFreeResult(r)

#else /* !PMAPI_VERSION_4 */

/*
 * Use the high resolution variants: their labels and results carry
 * struct timespec timestamps, exactly like PMAPI version 4 does.
 */
typedef pmHighResLogLabel	pcp_log_label_t;
typedef pmHighResResult		pcp_result_t;

#define pcp_get_archive_label(l)	pmGetHighResArchiveLabel(l)
#define pcp_set_mode(m, w, d)		pmSetModeHighRes((m), (w), (d))
#define pcp_fetch(n, p, r)		pmFetchHighRes((n), (p), (r))
#define pcp_free_result(r)		pmFreeHighResResult(r)

#endif /* PMAPI_VERSION_4 */

/*
 * pmiWrite() has taken a nanosecond argument since PCP 7.2; older releases
 * take microseconds in a plain int.
 */
static inline int
pcp_pmi_write(unsigned long long sec, unsigned int nsec)
{
#ifdef HAVE_PMI_APPEND
	return pmiWrite(sec, nsec);
#else
	return pmiWrite((int)sec, (int)(nsec / 1000));
#endif
}

#ifndef HAVE_PMI_APPEND
/*
 * Older libpcp_import has no pmiPutAtomValueHandle().  Emulate it with
 * pmiPutValueHandle(), using the metric type recorded when the handle was
 * registered (see pcp_def_metrics.c).
 */
int pcp_pmi_put_atom(int handle, const pmAtomValue *atom);
#define pmiPutAtomValueHandle(handle, atom) \
	pcp_pmi_put_atom((handle), (atom))
#endif

#endif /* HAVE_PCP */

#endif /* _PCP_COMPAT_H */
