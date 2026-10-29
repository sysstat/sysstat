/*
 * pcp_pidstat.c: Read per-process statistics from a PCP archive.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 *
 * Implements "pidstat -f <archive>" by opening a PM_CONTEXT_ARCHIVE
 * context, fetching proc.* metrics across consecutive timestamps, and
 * displaying output in pidstat's existing format.
 *
 * Unit conventions:
 *   CPU times  — stored in ms       (PCP delivers ms directly)
 *   wtime      — stored in ms       (PCP delivers ns; divided by 1e6 on store)
 *   VSZ / RSS  — stored in kB       (PCP delivers kB)
 *   I/O bytes  — stored in bytes    (PCP delivers bytes)
 *   fault/ctx  — stored as counts
 *   itv        — centiseconds       (timestamp delta * 100)
 */

#ifdef HAVE_PCP

#include <pcp/pmapi.h>
#include "pcp_compat.h"

#include "common.h"
#include "pidstat.h"
#include "pcp_pidstat.h"

#include <locale.h>
#ifdef USE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) (s)
#endif

/* -------------------------------------------------------------------------
 * PMID table — every proc.* metric needed by pidstat's display functions.
 * ------------------------------------------------------------------------- */

enum {
	PCP_PID_UTIME,		/* proc.psinfo.utime       (ms, counter)    */
	PCP_PID_STIME,		/* proc.psinfo.stime       (ms, counter)    */
	PCP_PID_GTIME,		/* proc.psinfo.guest_time  (ms, counter)    */
	PCP_PID_WTIME,		/* proc.schedstat.run_delay(ns, counter)    */
	PCP_PID_MINFLT,		/* proc.psinfo.minflt      (count,counter)  */
	PCP_PID_MAJFLT,		/* proc.psinfo.maj_flt     (count,counter)  */
	PCP_PID_VSIZE,		/* proc.psinfo.vsize       (kB, instant)    */
	PCP_PID_RSS,		/* proc.psinfo.rss         (kB, instant)    */
	PCP_PID_VMSTACK,	/* proc.memory.vmstack     (kB, instant)    */
	PCP_PID_RD_BYTES,	/* proc.io.read_bytes      (B,  counter)    */
	PCP_PID_WR_BYTES,	/* proc.io.write_bytes     (B,  counter)    */
	PCP_PID_CCW_BYTES,	/* proc.io.cancelled_write_bytes            */
	PCP_PID_VCTXSW,	/* proc.psinfo.vctxsw      (count,counter)  */
	PCP_PID_NVCTXSW,	/* proc.psinfo.nvctxsw     (count,counter)  */
	PCP_PID_THREADS,	/* proc.psinfo.threads     (count,instant)  */
	PCP_PID_FD_COUNT,	/* proc.fd.count           (count,instant)  */
	PCP_PID_PROCESSOR,	/* proc.psinfo.processor   (int, instant)   */
	PCP_PID_PRIORITY,	/* proc.psinfo.rt_priority (int, instant)   */
	PCP_PID_POLICY,		/* proc.psinfo.policy      (int, instant)   */
	PCP_PID_UID,		/* proc.id.uid             (int, discrete)  */
	PCP_PID_UID_NM,		/* proc.id.uid_nm          (str, discrete)  */
	PCP_PID_CMD,		/* proc.psinfo.cmd         (str, discrete)  */
	PCP_PID_PSARGS,		/* proc.psinfo.psargs      (str, discrete)  */
	PCP_PID_NR		/* sentinel */
};

static const char *pcp_pid_metric_names[PCP_PID_NR] = {
	[PCP_PID_UTIME]    = "proc.psinfo.utime",
	[PCP_PID_STIME]    = "proc.psinfo.stime",
	[PCP_PID_GTIME]    = "proc.psinfo.guest_time",
	[PCP_PID_WTIME]    = "proc.schedstat.run_delay",
	[PCP_PID_MINFLT]   = "proc.psinfo.minflt",
	[PCP_PID_MAJFLT]   = "proc.psinfo.maj_flt",
	[PCP_PID_VSIZE]    = "proc.psinfo.vsize",
	[PCP_PID_RSS]      = "proc.psinfo.rss",
	[PCP_PID_VMSTACK]  = "proc.memory.vmstack",
	[PCP_PID_RD_BYTES] = "proc.io.read_bytes",
	[PCP_PID_WR_BYTES] = "proc.io.write_bytes",
	[PCP_PID_CCW_BYTES]= "proc.io.cancelled_write_bytes",
	[PCP_PID_VCTXSW]   = "proc.psinfo.vctxsw",
	[PCP_PID_NVCTXSW]  = "proc.psinfo.nvctxsw",
	[PCP_PID_THREADS]  = "proc.psinfo.threads",
	[PCP_PID_FD_COUNT] = "proc.fd.count",
	[PCP_PID_PROCESSOR]= "proc.psinfo.processor",
	[PCP_PID_PRIORITY] = "proc.psinfo.rt_priority",
	[PCP_PID_POLICY]   = "proc.psinfo.policy",
	[PCP_PID_UID]      = "proc.id.uid",
	[PCP_PID_UID_NM]   = "proc.id.uid_nm",
	[PCP_PID_CMD]      = "proc.psinfo.cmd",
	[PCP_PID_PSARGS]   = "proc.psinfo.psargs",
};

static pmID  pcp_pid_pmids[PCP_PID_NR];
static pmDesc pcp_pid_descs[PCP_PID_NR];

/* -------------------------------------------------------------------------
 * Per-sample per-process snapshot — values in pidstat display units.
 * rss and vsz are stored in kB (PCP delivers kB, no PG_TO_KB needed).
 * CPU times are in ms (PCP delivers ms directly).
 * ------------------------------------------------------------------------- */

struct pcp_pid_snap {
	int		 inst_id;	/* PID (= PCP instance ID) */
	char		 cmd[64];
	char		 psargs[256];
	char		 uid_nm[32];
	unsigned int	 uid;
	unsigned int	 processor;
	unsigned int	 priority;
	unsigned int	 policy;
	unsigned int	 threads;
	unsigned int	 fd_count;
	unsigned long long utime;	/* ms */
	unsigned long long stime;	/* ms */
	unsigned long long gtime;	/* ms */
	unsigned long long wtime;	/* ms */
	unsigned long long minflt;
	unsigned long long majflt;
	unsigned long long vsz;		/* kB */
	unsigned long long rss;		/* kB */
	unsigned long long stack_size;	/* kB */
	unsigned long long read_bytes;
	unsigned long long write_bytes;
	unsigned long long ccwr_bytes;
	unsigned long      nvcsw;
	unsigned long      nivcsw;
};

/* Two-sample ring: [0] = prev, [1] = curr */
static struct pcp_pid_snap *snap[2]  = { NULL, NULL };
static int		    snap_nr[2] = { 0, 0 };
static int		    snap_cap[2]= { 0, 0 };

static void snap_clear(int s)
{
	snap_nr[s] = 0;
}

static struct pcp_pid_snap *snap_alloc(int s, int inst_id)
{
	if (snap_nr[s] >= snap_cap[s]) {
		size_t new_cap = snap_cap[s] ? snap_cap[s] * 2 : 256;
		struct pcp_pid_snap *tmp;

		tmp = realloc(snap[s], new_cap * sizeof(struct pcp_pid_snap));
		if (!tmp)
			return NULL;
		snap[s]    = tmp;
		snap_cap[s] = new_cap;
	}
	memset(&snap[s][snap_nr[s]], 0, sizeof(struct pcp_pid_snap));
	snap[s][snap_nr[s]].inst_id = inst_id;
	return &snap[s][snap_nr[s]++];
}

static int cmp_snap_by_id(const void *a, const void *b)
{
	return ((const struct pcp_pid_snap *)a)->inst_id
	     - ((const struct pcp_pid_snap *)b)->inst_id;
}

/*
 * Sort snap[s] by inst_id after build_snap so binary search is possible.
 * Called once per sample after all instances have been appended.
 */
static void snap_sort(int s)
{
	if (snap_nr[s] > 1)
		qsort(snap[s], snap_nr[s], sizeof(*snap[s]), cmp_snap_by_id);
}

/* Binary search by instance ID — O(log n) vs the former O(n). */
static struct pcp_pid_snap *snap_find(int s, int inst_id)
{
	int lo = 0, hi = (int)snap_nr[s] - 1;

	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;

		if (snap[s][mid].inst_id == inst_id)
			return &snap[s][mid];
		if (snap[s][mid].inst_id < inst_id)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/* -------------------------------------------------------------------------
 * Extract a scalar u64/u32/string value from a pmValueSet for a given
 * instance ID.  Returns 0 (or empty string) if instance not present.
 * ------------------------------------------------------------------------- */

static unsigned long long
vset_u64(pmValueSet *vset, pmDesc *desc, int inst_id)
{
	int i;

	for (i = 0; i < vset->numval; i++) {
		if (vset->vlist[i].inst != inst_id)
			continue;
		pmAtomValue atom;

		if (pmExtractValue(vset->valfmt, &vset->vlist[i],
				   desc->type, &atom, PM_TYPE_U64) < 0)
			return 0;
		return atom.ull;
	}
	return 0;
}

static unsigned int
vset_u32(pmValueSet *vset, pmDesc *desc, int inst_id)
{
	int i;

	for (i = 0; i < vset->numval; i++) {
		if (vset->vlist[i].inst != inst_id)
			continue;
		pmAtomValue atom;

		if (pmExtractValue(vset->valfmt, &vset->vlist[i],
				   desc->type, &atom, PM_TYPE_U32) < 0)
			return 0;
		return atom.ul;
	}
	return 0;
}

static void
vset_str(pmValueSet *vset, pmDesc *desc, int inst_id, char *buf, size_t bufsz)
{
	int i;
	pmAtomValue atom;

	buf[0] = '\0';
	for (i = 0; i < vset->numval; i++) {
		if (vset->vlist[i].inst != inst_id)
			continue;
		if (pmExtractValue(vset->valfmt, &vset->vlist[i],
				   desc->type, &atom, PM_TYPE_STRING) < 0)
			return;
		snprintf(buf, bufsz, "%s", atom.cp);
		free(atom.cp);
		return;
	}
}

/* -------------------------------------------------------------------------
 * Build snap[s] from a pcp_result_t.  Iterates instance IDs from utime's vset
 * (which has one entry per active process) and fills all fields.
 * ------------------------------------------------------------------------- */

static void
build_snap(int s, pcp_result_t *result)
{
	pmValueSet *utime_vset = NULL;
	pmValueSet *vs[PCP_PID_NR];
	struct pcp_pid_snap *p;
	int m, i, idx, inst;

	snap_clear(s);

	/* Find the utime vset to enumerate instances */
	for (m = 0; m < result->numpmid; m++) {
		if (result->vset[m]->pmid == pcp_pid_pmids[PCP_PID_UTIME]) {
			utime_vset = result->vset[m];
			break;
		}
	}
	if (!utime_vset || utime_vset->numval <= 0)
		return;

	/* Build a vset index keyed by our enum for fast access */
	memset(vs, 0, sizeof(vs));
	for (m = 0; m < result->numpmid; m++) {
		for (idx = 0; idx < PCP_PID_NR; idx++) {
			if (result->vset[m]->pmid == pcp_pid_pmids[idx]) {
				vs[idx] = result->vset[m];
				break;
			}
		}
	}

	for (i = 0; i < utime_vset->numval; i++) {
		inst = utime_vset->vlist[i].inst;
		p = snap_alloc(s, inst);
		if (!p)
			continue;

		/* CPU times stored in ms; PCP delivers utime/stime/gtime in ms,
		 * wtime (blkio delay) in ns — convert ns to ms on store. */
		if (vs[PCP_PID_UTIME])
			p->utime = vset_u64(vs[PCP_PID_UTIME],
					    &pcp_pid_descs[PCP_PID_UTIME], inst);
		if (vs[PCP_PID_STIME])
			p->stime = vset_u64(vs[PCP_PID_STIME],
					    &pcp_pid_descs[PCP_PID_STIME], inst);
		if (vs[PCP_PID_GTIME])
			p->gtime = vset_u64(vs[PCP_PID_GTIME],
					    &pcp_pid_descs[PCP_PID_GTIME], inst);
		if (vs[PCP_PID_WTIME])
			p->wtime = vset_u64(vs[PCP_PID_WTIME],
					    &pcp_pid_descs[PCP_PID_WTIME], inst)
				   / 1000000ULL;

		/* Fault counters: raw counts */
		if (vs[PCP_PID_MINFLT])
			p->minflt = vset_u64(vs[PCP_PID_MINFLT],
					     &pcp_pid_descs[PCP_PID_MINFLT], inst);
		if (vs[PCP_PID_MAJFLT])
			p->majflt = vset_u64(vs[PCP_PID_MAJFLT],
					     &pcp_pid_descs[PCP_PID_MAJFLT], inst);

		/* Memory: PCP delivers kB directly — no page conversion */
		if (vs[PCP_PID_VSIZE])
			p->vsz = vset_u64(vs[PCP_PID_VSIZE],
					  &pcp_pid_descs[PCP_PID_VSIZE], inst);
		if (vs[PCP_PID_RSS])
			p->rss = vset_u64(vs[PCP_PID_RSS],
					  &pcp_pid_descs[PCP_PID_RSS], inst);
		if (vs[PCP_PID_VMSTACK])
			p->stack_size = vset_u64(vs[PCP_PID_VMSTACK],
					  &pcp_pid_descs[PCP_PID_VMSTACK], inst);

		/* I/O: bytes */
		if (vs[PCP_PID_RD_BYTES])
			p->read_bytes = vset_u64(vs[PCP_PID_RD_BYTES],
					  &pcp_pid_descs[PCP_PID_RD_BYTES], inst);
		if (vs[PCP_PID_WR_BYTES])
			p->write_bytes = vset_u64(vs[PCP_PID_WR_BYTES],
					  &pcp_pid_descs[PCP_PID_WR_BYTES], inst);
		if (vs[PCP_PID_CCW_BYTES])
			p->ccwr_bytes = vset_u64(vs[PCP_PID_CCW_BYTES],
					  &pcp_pid_descs[PCP_PID_CCW_BYTES], inst);

		/* Context switches */
		if (vs[PCP_PID_VCTXSW])
			p->nvcsw = (unsigned long)vset_u64(vs[PCP_PID_VCTXSW],
					  &pcp_pid_descs[PCP_PID_VCTXSW], inst);
		if (vs[PCP_PID_NVCTXSW])
			p->nivcsw = (unsigned long)vset_u64(vs[PCP_PID_NVCTXSW],
					  &pcp_pid_descs[PCP_PID_NVCTXSW], inst);

		/* Scalar instantaneous fields */
		if (vs[PCP_PID_THREADS])
			p->threads = vset_u32(vs[PCP_PID_THREADS],
					  &pcp_pid_descs[PCP_PID_THREADS], inst);
		if (vs[PCP_PID_FD_COUNT])
			p->fd_count = vset_u32(vs[PCP_PID_FD_COUNT],
					  &pcp_pid_descs[PCP_PID_FD_COUNT], inst);
		if (vs[PCP_PID_PROCESSOR])
			p->processor = vset_u32(vs[PCP_PID_PROCESSOR],
					  &pcp_pid_descs[PCP_PID_PROCESSOR], inst);
		if (vs[PCP_PID_PRIORITY])
			p->priority = vset_u32(vs[PCP_PID_PRIORITY],
					  &pcp_pid_descs[PCP_PID_PRIORITY], inst);
		if (vs[PCP_PID_POLICY])
			p->policy = vset_u32(vs[PCP_PID_POLICY],
					  &pcp_pid_descs[PCP_PID_POLICY], inst);
		if (vs[PCP_PID_UID])
			p->uid = vset_u32(vs[PCP_PID_UID],
					  &pcp_pid_descs[PCP_PID_UID], inst);

		/* String fields */
		if (vs[PCP_PID_CMD])
			vset_str(vs[PCP_PID_CMD],
				 &pcp_pid_descs[PCP_PID_CMD], inst,
				 p->cmd, sizeof(p->cmd));
		if (vs[PCP_PID_PSARGS])
			vset_str(vs[PCP_PID_PSARGS],
				 &pcp_pid_descs[PCP_PID_PSARGS], inst,
				 p->psargs, sizeof(p->psargs));
		if (vs[PCP_PID_UID_NM])
			vset_str(vs[PCP_PID_UID_NM],
				 &pcp_pid_descs[PCP_PID_UID_NM], inst,
				 p->uid_nm, sizeof(p->uid_nm));
	}
}

/* -------------------------------------------------------------------------
 * Display one interval's worth of data for a pair of snapshots.
 * itv: interval in centiseconds (matches pidstat's native itv unit).
 * deltot_jiffies: total CPU jiffies in interval across all CPUs (for %CPU).
 * timestamp: human-readable current sample time string.
 * ------------------------------------------------------------------------- */

static void
display_interval(unsigned long long itv, const char *timestamp)
{
	unsigned long long itv_ms;
	struct pcp_pid_snap *c, *p;
	const char *cmd;
	int i, dis = 1;

	/* itv is in centiseconds; CPU times in snap are in ms (1 cs = 10 ms) */
	itv_ms = itv * 10;
	if (itv_ms == 0)
		itv_ms = 1;

	for (i = 0; i < snap_nr[1]; i++) {
		c = &snap[1][i];
		p = snap_find(0, c->inst_id);
		cmd = DISPLAY_CMDLINE(pidflag) && c->psargs[0]
		      ? c->psargs : c->cmd;

		if (!p)
			continue;

		/* -u: CPU */
		if (DISPLAY_CPU(actflag)) {
			unsigned long long dlt_utime = c->utime - p->utime;
			unsigned long long dlt_stime = c->stime - p->stime;
			unsigned long long dlt_gtime = c->gtime - p->gtime;
			unsigned long long dlt_wtime = c->wtime - p->wtime;

			if (dis) {
				PRINT_ID_HDR(timestamp, pidflag);
				printf("    %%usr %%system  %%guest   %%wait    %%CPU"
				       "   CPU  Command\n");
				dis = 0;
			}

			printf("%-11s", timestamp);
			if (DISPLAY_USERNAME(pidflag))
				printf(" %8s", c->uid_nm[0] ? c->uid_nm : "?");
			else
				printf(" %7u", c->uid);
			printf(" %7d", c->inst_id);

			printf(" %7.2f %7.2f %7.2f %7.2f %7.2f %5u  %s\n",
			       /* %usr */
			       (double)(dlt_utime - (dlt_utime < dlt_gtime ? 0
						     : dlt_gtime)) / itv_ms * 100,
			       /* %system */
			       (double)dlt_stime / itv_ms * 100,
			       /* %guest */
			       (double)dlt_gtime / itv_ms * 100,
			       /* %wait */
			       (double)dlt_wtime / itv_ms * 100,
			       /* %CPU */
			       (double)(dlt_utime + dlt_stime) / itv_ms * 100,
			       /* CPU# */
			       c->processor,
			       /* Command */
			       cmd[0] ? cmd : "?");
		}

		/* -r: memory */
			dis = 1;
		if (DISPLAY_MEM(actflag)) {
			if (dis) {
				PRINT_ID_HDR(timestamp, pidflag);
				printf(" minflt/s  majflt/s     VSZ     RSS"
				       "   %%MEM  Command\n");
				dis = 0;
			}
			printf("%-11s", timestamp);
			if (DISPLAY_USERNAME(pidflag))
				printf(" %8s", c->uid_nm[0] ? c->uid_nm : "?");
			else
				printf(" %7u", c->uid);
			printf(" %7d", c->inst_id);

			printf(" %9.2f %9.2f %7llu %7llu %6.2f  %s\n",
			       S_VALUE(p->minflt, c->minflt, itv),
			       S_VALUE(p->majflt, c->majflt, itv),
			       c->vsz,
			       c->rss,
			       tlmkb ? SP_VALUE(0, c->rss, tlmkb) : 0.0,
			       cmd[0] ? cmd : "?");
		}

		/* -d: I/O */
			dis = 1;
		if (DISPLAY_IO(actflag)) {
			if (dis) {
				PRINT_ID_HDR(timestamp, pidflag);
				printf("   kB_rd/s   kB_wr/s kB_ccwr/s iodelay"
				       "  Command\n");
				dis = 0;
			}
			printf("%-11s", timestamp);
			if (DISPLAY_USERNAME(pidflag))
				printf(" %8s", c->uid_nm[0] ? c->uid_nm : "?");
			else
				printf(" %7u", c->uid);
			printf(" %7d", c->inst_id);

			printf(" %9.2f %9.2f %9.2f %7llu  %s\n",
			       S_VALUE(p->read_bytes,  c->read_bytes,  itv) / 1024,
			       S_VALUE(p->write_bytes, c->write_bytes, itv) / 1024,
			       S_VALUE(p->ccwr_bytes,  c->ccwr_bytes,  itv) / 1024,
			       /* iodelay: delta of blkio delays — not in PCP, show 0 */
			       0ULL,
			       cmd[0] ? cmd : "?");
		}

		/* -w: context switches */
			dis = 1;
		if (DISPLAY_CTXSW(actflag)) {
			if (dis) {
				PRINT_ID_HDR(timestamp, pidflag);
				printf("   cswch/s nvcswch/s  Command\n");
				dis = 0;
			}
			printf("%-11s", timestamp);
			if (DISPLAY_USERNAME(pidflag))
				printf(" %8s", c->uid_nm[0] ? c->uid_nm : "?");
			else
				printf(" %7u", c->uid);
			printf(" %7d", c->inst_id);

			printf(" %9.2f %9.2f  %s\n",
			       S_VALUE(p->nvcsw,  c->nvcsw,  itv),
			       S_VALUE(p->nivcsw, c->nivcsw, itv),
			       cmd[0] ? cmd : "?");
		}

		/* -v: kernel tables (threads, fd) */
		if (DISPLAY_KTAB(actflag)) {
			if (dis) {
				PRINT_ID_HDR(timestamp, pidflag);
				printf(" threads   fd-nr  Command\n");
				dis = 0;
			}
			printf("%-11s", timestamp);
			if (DISPLAY_USERNAME(pidflag))
				printf(" %8s", c->uid_nm[0] ? c->uid_nm : "?");
			else
				printf(" %7u", c->uid);
			printf(" %7d", c->inst_id);

			printf(" %7u %7u  %s\n",
			       c->threads, c->fd_count,
			       cmd[0] ? cmd : "?");
		}
	}
}

/* -------------------------------------------------------------------------
 * Main entry point: open archive, fetch, display.
 * ------------------------------------------------------------------------- */

int
pcp_pidstat_run(const char *archive)
{
	struct pcp_pid_snap *tmp_snap;
	pmID fetch_pmids[PCP_PID_NR];
	pcp_result_t *result = NULL, *prev_result = NULL;
	struct timespec prev_tv = {0, 0};
	char timestamp[32];
	int ctx, sts, m, fetch_nr = 0;
	int tmp_nr, tmp_cap, first = 1;

	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive);
	if (ctx < 0) {
		fprintf(stderr, _("Cannot open PCP archive %s: %s\n"),
			archive, pmErrStr(ctx));
		return 1;
	}

	/* Batch name:PMID and PMID:descriptor lookups in two round-trips */
	pmLookupName(PCP_PID_NR, pcp_pid_metric_names, pcp_pid_pmids);
	pmLookupDescs(PCP_PID_NR, pcp_pid_pmids, pcp_pid_descs);

	/* Filter out any absent metrics from the fetch list */
	for (m = 0; m < PCP_PID_NR; m++) {
		if (pcp_pid_pmids[m] != PM_ID_NULL)
			fetch_pmids[fetch_nr++] = pcp_pid_pmids[m];
	}
	if (fetch_nr == 0) {
		fprintf(stderr,
			_("No proc.* metrics found in archive %s\n"
			  "Ensure sadc was run with -O pcp and sysstat.pcpconf"
			  " lists proc.* metrics.\n"), archive);
		pmDestroyContext(ctx);
		return 1;
	}

	pcp_set_mode(PM_MODE_FORW, NULL, 0);

	/* Read total memory for %MEM calculation */
	tlmkb = sysconf(_SC_PHYS_PAGES) * (sysconf(_SC_PAGE_SIZE) / 1024);

	while ((sts = pcp_fetch(fetch_nr, fetch_pmids, &result)) >= 0) {
		struct timespec curr_tv;
		curr_tv = result->timestamp;

		if (!first) {
			/* centiseconds between samples */
			unsigned long long itv =
				(unsigned long long)(curr_tv.tv_sec  - prev_tv.tv_sec)  * 100
				+ (curr_tv.tv_nsec - prev_tv.tv_nsec) / 10000000;

			/* Format timestamp as HH:MM:SS */
			time_t t = (time_t)curr_tv.tv_sec;
			struct tm *tm = localtime(&t);

			strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm);

			build_snap(1, result);
			snap_sort(1);

			/* deltot_jiffies: not available from proc.* alone —
			 * use 0 to fall back to per-process %CPU calculation. */
			display_interval(itv, timestamp);

			/* Roll curr -> prev */
			if (prev_result)
				pcp_free_result(prev_result);
			prev_result = result;
			result = NULL;

			/* Swap snap buffers */
			tmp_snap = snap[0]; tmp_nr = snap_nr[0]; tmp_cap = snap_cap[0];
			snap[0] = snap[1]; snap_nr[0] = snap_nr[1]; snap_cap[0] = snap_cap[1];
			snap[1] = tmp_snap; snap_nr[1] = tmp_nr; snap_cap[1] = tmp_cap;
		} else {
			build_snap(0, result);
			snap_sort(0);

			if (prev_result)
				pcp_free_result(prev_result);
			prev_result = result;
			result = NULL;
			first = 0;
		}

		prev_tv = curr_tv;
	}

	if (result)
		pcp_free_result(result);
	if (prev_result)
		pcp_free_result(prev_result);
	free(snap[0]); free(snap[1]);

	pmDestroyContext(ctx);
	return 0;
}

#endif /* HAVE_PCP */
