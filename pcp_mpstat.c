/*
 * pcp_mpstat.c: Read CPU statistics from a PCP archive for mpstat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 *
 * Implements "mpstat -a <archive>".  Populates the same globals that
 * mpstat's live read path fills (st_cpu, uptime_cs, mp_tstamp, cpu_nr)
 * then calls the existing write_stats() display function unchanged.
 *
 * PCP delivers CPU times in milliseconds; we store them directly as ms and
 * use a millisecond-based interval so that percentages computed by
 * ll_sp_value() are correct without any unit conversion.
 */

#ifdef HAVE_PCP

#include <pcp/pmapi.h>

#include "common.h"
#include "rd_stats.h"
#include "mpstat.h"
#include "pcp_def_metrics.h"
#include "pcp_mpstat.h"

#include <locale.h>
#ifdef USE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) (s)
#endif

/* PMIDs — use values from pcp_def_metrics.h, already the correct PMIDs */
enum {
	PCP_MPSTAT_PERCPU_USER,
	PCP_MPSTAT_PERCPU_NICE,
	PCP_MPSTAT_PERCPU_SYS,
	PCP_MPSTAT_PERCPU_IDLE,
	PCP_MPSTAT_PERCPU_IOWAIT,
	PCP_MPSTAT_PERCPU_HARDIRQ,
	PCP_MPSTAT_PERCPU_SOFTIRQ,
	PCP_MPSTAT_PERCPU_STEAL,
	PCP_MPSTAT_PERCPU_GUEST,
	PCP_MPSTAT_PERCPU_GUESTNICE,
	PCP_MPSTAT_ALLCPU_USER,
	PCP_MPSTAT_ALLCPU_NICE,
	PCP_MPSTAT_ALLCPU_SYS,
	PCP_MPSTAT_ALLCPU_IDLE,
	PCP_MPSTAT_ALLCPU_IOWAIT,
	PCP_MPSTAT_ALLCPU_HARDIRQ,
	PCP_MPSTAT_ALLCPU_SOFTIRQ,
	PCP_MPSTAT_ALLCPU_STEAL,
	PCP_MPSTAT_ALLCPU_GUEST,
	PCP_MPSTAT_ALLCPU_GUESTNICE,
	PCP_MPSTAT_NR
};

static pmID pcp_mpstat_pmids[PCP_MPSTAT_NR] = {
	[PCP_MPSTAT_PERCPU_USER]      = PMID_CPU_PERCPU_USER,
	[PCP_MPSTAT_PERCPU_NICE]      = PMID_CPU_PERCPU_NICE,
	[PCP_MPSTAT_PERCPU_SYS]       = PMID_CPU_PERCPU_SYS,
	[PCP_MPSTAT_PERCPU_IDLE]      = PMID_CPU_PERCPU_IDLE,
	[PCP_MPSTAT_PERCPU_IOWAIT]    = PMID_CPU_PERCPU_WAITTOTAL,
	[PCP_MPSTAT_PERCPU_HARDIRQ]   = PMID_CPU_PERCPU_IRQHARD,
	[PCP_MPSTAT_PERCPU_SOFTIRQ]   = PMID_CPU_PERCPU_IRQSOFT,
	[PCP_MPSTAT_PERCPU_STEAL]     = PMID_CPU_PERCPU_STEAL,
	[PCP_MPSTAT_PERCPU_GUEST]     = PMID_CPU_PERCPU_GUEST,
	[PCP_MPSTAT_PERCPU_GUESTNICE] = PMID_CPU_PERCPU_GUESTNICE,
	[PCP_MPSTAT_ALLCPU_USER]      = PMID_CPU_ALLCPU_USER,
	[PCP_MPSTAT_ALLCPU_NICE]      = PMID_CPU_ALLCPU_NICE,
	[PCP_MPSTAT_ALLCPU_SYS]       = PMID_CPU_ALLCPU_SYS,
	[PCP_MPSTAT_ALLCPU_IDLE]      = PMID_CPU_ALLCPU_IDLE,
	[PCP_MPSTAT_ALLCPU_IOWAIT]    = PMID_CPU_ALLCPU_WAITTOTAL,
	[PCP_MPSTAT_ALLCPU_HARDIRQ]   = PMID_CPU_ALLCPU_IRQHARD,
	[PCP_MPSTAT_ALLCPU_SOFTIRQ]   = PMID_CPU_ALLCPU_IRQSOFT,
	[PCP_MPSTAT_ALLCPU_STEAL]     = PMID_CPU_ALLCPU_STEAL,
	[PCP_MPSTAT_ALLCPU_GUEST]     = PMID_CPU_ALLCPU_GUEST,
	[PCP_MPSTAT_ALLCPU_GUESTNICE] = PMID_CPU_ALLCPU_GUESTNICE,
};

static unsigned long long
scalar_u64(pmValueSet *vset)
{
	pmAtomValue atom;

	if (!vset || vset->numval <= 0)
		return 0;
	if (pmExtractValue(vset->valfmt, &vset->vlist[0],
			   PM_TYPE_U64, &atom, PM_TYPE_U64) < 0)
		return 0;
	return atom.ull;
}

static void
build_cpu_sample(int s, pmResult *result)
{
	pmValueSet *vs[PCP_MPSTAT_NR];
	struct stats_cpu *scc;
	int m, i, idx;

	memset(vs, 0, sizeof(vs));
	for (m = 0; m < result->numpmid; m++) {
		for (idx = 0; idx < PCP_MPSTAT_NR; idx++) {
			if (result->vset[m]->pmid == pcp_mpstat_pmids[idx]) {
				vs[idx] = result->vset[m];
				break;
			}
		}
	}

	/* Slot 0: all-CPU aggregate (raw ms values — interval is also in ms) */
	scc = &st_cpu[s][0];
	scc->cpu_user       = scalar_u64(vs[PCP_MPSTAT_ALLCPU_USER]);
	scc->cpu_nice       = scalar_u64(vs[PCP_MPSTAT_ALLCPU_NICE]);
	scc->cpu_sys        = scalar_u64(vs[PCP_MPSTAT_ALLCPU_SYS]);
	scc->cpu_idle       = scalar_u64(vs[PCP_MPSTAT_ALLCPU_IDLE]);
	scc->cpu_iowait     = scalar_u64(vs[PCP_MPSTAT_ALLCPU_IOWAIT]);
	scc->cpu_hardirq    = scalar_u64(vs[PCP_MPSTAT_ALLCPU_HARDIRQ]);
	scc->cpu_softirq    = scalar_u64(vs[PCP_MPSTAT_ALLCPU_SOFTIRQ]);
	scc->cpu_steal      = scalar_u64(vs[PCP_MPSTAT_ALLCPU_STEAL]);
	scc->cpu_guest      = scalar_u64(vs[PCP_MPSTAT_ALLCPU_GUEST]);
	scc->cpu_guest_nice = scalar_u64(vs[PCP_MPSTAT_ALLCPU_GUESTNICE]);

	/* Slots 1..cpu_nr: per-CPU (instance ID = CPU number) */
	if (!vs[PCP_MPSTAT_PERCPU_USER]) return;

	for (i = 0; i < vs[PCP_MPSTAT_PERCPU_USER]->numval; i++) {
		int cpu_id = vs[PCP_MPSTAT_PERCPU_USER]->vlist[i].inst;

		if (cpu_id < 0 || cpu_id >= cpu_nr) continue;

		scc = &st_cpu[s][cpu_id + 1];
		scc->cpu_user       = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_USER],      cpu_id);
		scc->cpu_nice       = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_NICE],      cpu_id);
		scc->cpu_sys        = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_SYS],       cpu_id);
		scc->cpu_idle       = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_IDLE],      cpu_id);
		scc->cpu_iowait     = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_IOWAIT],    cpu_id);
		scc->cpu_hardirq    = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_HARDIRQ],   cpu_id);
		scc->cpu_softirq    = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_SOFTIRQ],   cpu_id);
		scc->cpu_steal      = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_STEAL],     cpu_id);
		scc->cpu_guest      = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_GUEST],     cpu_id);
		scc->cpu_guest_nice = pcp_inst_u64(vs[PCP_MPSTAT_PERCPU_GUESTNICE], cpu_id);
	}
}

int
pcp_mpstat_run(const char *archive)
{
	pmValueSet *percpu_vset;
	pmResult *result = NULL, *prev_result = NULL;
	int ctx, sts, i, m;
	int first = 1, curr = 1;

	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive);
	if (ctx < 0) {
		fprintf(stderr, _("Cannot open PCP archive %s: %s\n"),
			archive, pmErrStr(ctx));
		return 1;
	}

	pmSetMode(PM_MODE_FORW, NULL, 0);

	while ((sts = pmFetch(PCP_MPSTAT_NR, pcp_mpstat_pmids, &result)) >= 0) {
		struct timespec curr_tv;
		time_t t;
		unsigned long long curr_ts_ms;

		curr_tv = result->timestamp;
		t = (time_t)curr_tv.tv_sec;
		curr_ts_ms = (unsigned long long)curr_tv.tv_sec * 1000
			     + curr_tv.tv_nsec / 1000000;

		if (first) {
			/*
			 * First fetch: discover CPU count, allocate st_cpu
			 * arrays, then use this result as the initial sample
			 * — no separate probe needed (a second pmSetMode(NULL)
			 * does not rewind).
			 */
			percpu_vset = NULL;
			for (m = 0; m < result->numpmid; m++) {
				if (result->vset[m]->pmid ==
				    pcp_mpstat_pmids[PCP_MPSTAT_PERCPU_USER]) {
					percpu_vset = result->vset[m];
					break;
				}
			}
			if (!percpu_vset || percpu_vset->numval <= 0) {
				fprintf(stderr,
					_("No per-CPU metrics in archive %s\n"), archive);
				pmFreeResult(result);
				break;
			}
			cpu_nr = percpu_vset->numval;

			for (i = 0; i < 2; i++) {
				free(st_cpu[i]);
				st_cpu[i] = calloc(cpu_nr + 1, sizeof(struct stats_cpu));
				if (!st_cpu[i]) {
					perror("calloc");
					pmFreeResult(result);
					goto out;
				}
			}
			free(cpu_bitmap);
			cpu_bitmap = calloc(1, ((cpu_nr + 1) >> 3) + 1);
			if (!cpu_bitmap) {
				perror("calloc");
				pmFreeResult(result);
				goto out;
			}
		}

		/* Set mpstat's timestamp and uptime_cs globals (stored as ms here) */
		localtime_r(&t, &mp_tstamp[curr]);
		uptime_cs[curr] = curr_ts_ms;

		build_cpu_sample(curr, result);

		if (!first) {
			/* actflags drives what write_stats displays; ensure -u is set */
			if (!actflags)
				actflags |= M_D_CPU;
			/*
			 * cpu_bitmap is zeroed by salloc_mp_struct and only set
			 * by main after the option loop — which we bypass via exit().
			 * Select all CPUs (bit 0 = aggregate, bits 1..cpu_nr = each CPU).
			 */
			if (!*cpu_bitmap)
				memset(cpu_bitmap, ~0, ((cpu_nr + 1) >> 3) + 1);
			write_stats(curr, TRUE);
		}

		if (prev_result) pmFreeResult(prev_result);
		prev_result = result;
		result = NULL;

		curr ^= 1;
		first = 0;
	}
out:

	if (result) pmFreeResult(result);
	if (prev_result) pmFreeResult(prev_result);
	for (i = 0; i < 2; i++) { free(st_cpu[i]); st_cpu[i] = NULL; }

	pmDestroyContext(ctx);
	return 0;
}

#endif /* HAVE_PCP */
