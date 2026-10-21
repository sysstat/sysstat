/*
 * pcp_tapestat.c: Read tape statistics from a PCP archive for tapestat.
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 *
 * Implements "tapestat -a <archive>".  Populates the tape_old_stats[] and
 * tape_new_stats[] globals from tape.dev.* metrics in a PCP archive
 * (written by pmlogger or sadc with tape.dev.* in sysstat.pcpconf), then
 * calls tapestat's existing write_stats() display function unchanged.
 *
 * All tape.dev.* metrics are in the linux PMDA (domain 60, cluster 71).
 * Units match tapestat's internal struct directly (ns, bytes, counts).
 */

#ifdef HAVE_PCP

#include <pcp/pmapi.h>

#include "common.h"
#include "tapestat.h"
#include "pcp_tapestat.h"
#include "pcp_def_metrics.h"

#include <locale.h>
#ifdef USE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) (s)
#endif

/* All tape.dev.* metrics live in linux PMDA cluster 71 */
enum {
	PCP_TAPE_READ_CNT,	/* tape.dev.read_cnt       60.71.4 */
	PCP_TAPE_READ_BYTES,	/* tape.dev.read_byte_cnt  60.71.3 */
	PCP_TAPE_READ_NS,	/* tape.dev.read_ns        60.71.5 */
	PCP_TAPE_WRITE_CNT,	/* tape.dev.write_cnt      60.71.8 */
	PCP_TAPE_WRITE_BYTES,	/* tape.dev.write_byte_cnt 60.71.7 */
	PCP_TAPE_WRITE_NS,	/* tape.dev.write_ns       60.71.9 */
	PCP_TAPE_OTHER_CNT,	/* tape.dev.other_cnt      60.71.2 */
	PCP_TAPE_RESID_CNT,	/* tape.dev.resid_cnt      60.71.6 */
	PCP_TAPE_IO_NS,		/* tape.dev.io_ns          60.71.1 */
	PCP_TAPE_NR
};

static pmID pcp_tape_pmids[PCP_TAPE_NR] = {
	[PCP_TAPE_READ_CNT]   = PMI_ID(60, 71, 4),
	[PCP_TAPE_READ_BYTES] = PMI_ID(60, 71, 3),
	[PCP_TAPE_READ_NS]    = PMI_ID(60, 71, 5),
	[PCP_TAPE_WRITE_CNT]  = PMI_ID(60, 71, 8),
	[PCP_TAPE_WRITE_BYTES]= PMI_ID(60, 71, 7),
	[PCP_TAPE_WRITE_NS]   = PMI_ID(60, 71, 9),
	[PCP_TAPE_OTHER_CNT]  = PMI_ID(60, 71, 2),
	[PCP_TAPE_RESID_CNT]  = PMI_ID(60, 71, 6),
	[PCP_TAPE_IO_NS]      = PMI_ID(60, 71, 1),
};

/*
 * Extract the tape device index from instance name "stN" -> N.
 * Returns -1 if name doesn't match.
 */
static int
tape_index_from_name(const char *name)
{
	int idx;

	if (!name || (name[0] != 's' && name[0] != 'S'))
		return -1;
	if (name[1] != 't' && name[1] != 'T')
		return -1;
	if (sscanf(name + 2, "%d", &idx) != 1)
		return -1;
	return idx;
}

static void
build_tape_snap(struct tape_stats *tgt, pmResult *result,
		struct timespec *ts, pmInDom tape_indom)
{
	pmValueSet *vs[PCP_TAPE_NR];
	int m, i;

	memset(vs, 0, sizeof(vs));
	for (m = 0; m < result->numpmid; m++) {
		int idx;

		for (idx = 0; idx < PCP_TAPE_NR; idx++) {
			if (result->vset[m]->pmid == pcp_tape_pmids[idx]) {
				vs[idx] = result->vset[m];
				break;
			}
		}
	}

	if (!vs[PCP_TAPE_READ_CNT])
		return;

	for (i = 0; i < vs[PCP_TAPE_READ_CNT]->numval; i++) {
		int inst_id = vs[PCP_TAPE_READ_CNT]->vlist[i].inst;
		char *inst_name = NULL;
		int tape_idx;

		if (pmNameInDom(tape_indom, inst_id, &inst_name) < 0)
			continue;

		tape_idx = tape_index_from_name(inst_name);
		free(inst_name);

		if (tape_idx < 0 || tape_idx >= max_tape_drives)
			continue;

		tgt[tape_idx].read_count  = pcp_inst_u64(vs[PCP_TAPE_READ_CNT],   inst_id);
		tgt[tape_idx].read_bytes  = pcp_inst_u64(vs[PCP_TAPE_READ_BYTES],  inst_id);
		tgt[tape_idx].read_time   = pcp_inst_u64(vs[PCP_TAPE_READ_NS],    inst_id);
		tgt[tape_idx].write_count = pcp_inst_u64(vs[PCP_TAPE_WRITE_CNT],  inst_id);
		tgt[tape_idx].write_bytes = pcp_inst_u64(vs[PCP_TAPE_WRITE_BYTES], inst_id);
		tgt[tape_idx].write_time  = pcp_inst_u64(vs[PCP_TAPE_WRITE_NS],   inst_id);
		tgt[tape_idx].other_count = pcp_inst_u64(vs[PCP_TAPE_OTHER_CNT],  inst_id);
		tgt[tape_idx].resid_count = pcp_inst_u64(vs[PCP_TAPE_RESID_CNT],  inst_id);
		tgt[tape_idx].other_time  = pcp_inst_u64(vs[PCP_TAPE_IO_NS],      inst_id);
		tgt[tape_idx].tv.tv_sec   = (long)ts->tv_sec;
		tgt[tape_idx].tv.tv_usec  = (long)(ts->tv_nsec / 1000);
		tgt[tape_idx].valid       = TAPE_STATS_VALID;
	}
}

int
pcp_tapestat_run(const char *archive)
{
	int ctx, sts, i;
	pmResult *result = NULL, *prev_result = NULL;
	int first = 1;
	struct tm rectime;
	pmDesc tape_desc;
	pmInDom tape_indom = PM_INDOM_NULL;

	ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive);
	if (ctx < 0) {
		fprintf(stderr, _("Cannot open PCP archive %s: %s\n"),
			archive, pmErrStr(ctx));
		return 1;
	}

	/* Get tape device indom for correct pmNameInDom calls */
	if (pmLookupDesc(pcp_tape_pmids[PCP_TAPE_READ_CNT], &tape_desc) >= 0)
		tape_indom = tape_desc.indom;

	pmSetMode(PM_MODE_FORW, NULL, 0);

	while ((sts = pmFetch(PCP_TAPE_NR, pcp_tape_pmids, &result)) >= 0) {
		struct timespec curr_tv;
		time_t t;
		int nr_tapes;

		curr_tv = result->timestamp;
		t = (time_t)curr_tv.tv_sec;
		localtime_r(&t, &rectime);

		if (first) {
			/*
			 * First fetch: discover tape count and allocate arrays,
			 * then use this result directly as the initial snapshot.
			 * This avoids a separate probe that would discard the
			 * first sample's data.
			 */
			nr_tapes = result->vset[0] ? result->vset[0]->numval : 0;
			if (nr_tapes <= 0) {
				fprintf(stderr,
					_("No tape metrics in archive %s\n"), archive);
				pmFreeResult(result);
				break;
			}
			max_tape_drives = nr_tapes;
			tape_old_stats = calloc(nr_tapes, sizeof(struct tape_stats));
			tape_new_stats = calloc(nr_tapes, sizeof(struct tape_stats));
			if (!tape_old_stats || !tape_new_stats) {
				perror("calloc");
				pmFreeResult(result);
				break;
			}
			build_tape_snap(tape_old_stats, result, &curr_tv, tape_indom);
			first = 0;
		} else {
			build_tape_snap(tape_new_stats, result, &curr_tv, tape_indom);
			write_stats(&rectime);

			/* Swap old/new for next interval */
			struct tape_stats *tmp = tape_old_stats;
			tape_old_stats = tape_new_stats;
			tape_new_stats = tmp;
		}

		if (prev_result) pmFreeResult(prev_result);
		prev_result = result;
		result = NULL;
	}

	if (result) pmFreeResult(result);
	if (prev_result) pmFreeResult(prev_result);

	for (i = 0; i < max_tape_drives; i++)
		tape_new_stats[i].valid = TAPE_STATS_INVALID;
	free(tape_old_stats); tape_old_stats = NULL;
	free(tape_new_stats); tape_new_stats = NULL;
	max_tape_drives = 0;

	pmDestroyContext(ctx);
	return 0;
}

#endif /* HAVE_PCP */
