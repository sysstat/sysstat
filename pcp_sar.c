/*
 * pcp_sar.c: PCP archive replay functions for sar.
 * (C) 2025-2026 Red Hat, Inc.
 * (C) 2025 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifdef HAVE_PCP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <pcp/pmapi.h>
#include "pcp_compat.h"

#include "sa.h"
#include "pcp_stats.h"
#include "pcp_def_metrics.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern long interval, count;
extern int dish, endian_mismatch, arch_64;
extern uint64_t flags;
extern char timestamp[2][TIMESTAMP_LEN];
extern unsigned long avg_count;
extern struct file_header file_hdr;
extern struct record_header record_hdr[3];
extern unsigned int id_seq[];
extern struct tstamp_ext rectime, tm_start, tm_end;
extern struct activity *act[];
extern struct report_format sar_fmt;
extern void write_stats_avg(int, int, unsigned int);

static pcp_log_label_t log_label;
static long pcp_archive_interval = 0;

/*
 ***************************************************************************
 * Print system statistics using the PCP PMAPI.
 * This is called when we read stats either from a PCP archive.
 *
 * IN:
 * @curr		Index in array for current sample statistics.
 * @reset		Set to TRUE if last_uptime variable should be
 * 			reinitialized (used in next_slice() function).
 * @act_id		Activity that can be displayed or ~0 for all.
 *			Remember that when reading stats from a file, only
 *			one activity can be displayed at a time.
 *
 * OUT:
 * @cnt			Number of remaining lines to display.
 *
 * RETURNS:
 * 1 if stats have been successfully displayed, and 0 otherwise.
 ***************************************************************************
 */
static int write_stats_pcp(int curr, long *cnt, int reset, unsigned int act_id)
{
	int i, rc = 0;
	unsigned long long itv;

	/* Get then set previous timestamp */
	if (sa_get_record_timestamp_struct(flags, &record_hdr[!curr], &rectime))
		return 0;
	set_record_timestamp_string(flags, NULL, timestamp[!curr], TIMESTAMP_LEN, &rectime);

	/* Get then set current timestamp */
	if (sa_get_record_timestamp_struct(flags, &record_hdr[curr], &rectime))
		return 0;
	set_record_timestamp_string(flags, NULL, timestamp[curr], TIMESTAMP_LEN, &rectime);

	/* Get interval value in 1/100th of a second */
	get_itv_value(&record_hdr[curr], &record_hdr[!curr], &itv);

	avg_count++;

	/* Test stdout */
	TEST_STDOUT(STDOUT_FILENO);

	for (i = 0; i < NR_ACT; i++) {
		if ((act_id != ALL_ACTIVITIES) && (act[i]->id != act_id))
			continue;

		if (IS_SELECTED(act[i]->options) && (act[i]->nr[curr] > 0)) {
			/* Display current activity statistics */
			(*act[i]->f_print)(act[i], !curr, curr, itv);
			rc = 1;
		}
	}

	return rc;
}

/*
 ***************************************************************************
 * Print report header.
 *
 * IN:
 * @ctxid	PMAPI context identifier.
 * @from_file	Input file name.
 ***************************************************************************
 */
static int print_report_hdr_pcpfile(int ctxid, const char from_file[])
{
	pmValueSet		*values;
	pcp_result_t		*result;
	struct act_metrics	*metrics;
	unsigned int		cpu_count = 0;
	struct tm		tm_time;
	char			*sysname, *release, *nodename, *machine;
	char			host[MAXHOSTNAMELEN] = {0};
	int			i, sts;

	if ((sts = pcp_get_archive_label(&log_label)) < 0) {
		fprintf(stderr, 
			_("Cannot read archive label from file %s: %s\n"),
			from_file, pmErrStr(sts));
		return 0;
	}
	pmLocaltime(&log_label.start.tv_sec, &tm_time);

	if ((sts = pcp_set_mode(PM_MODE_FORW, &log_label.start, NULL)) < 0) {
		fprintf(stderr, _("Cannot set sample mode of PCP archive %s\n"),
			from_file);
		return 0;
	}

	sysname = release = nodename = machine = NULL;
	metrics = &file_header_metrics;

	for (i = 0; i < FILE_HEADER_METRIC_COUNT; i++)
		metrics->pmids[i] = metrics->descs[i].pmid;

	if ((sts = pcp_fetch(metrics->count, metrics->pmids, &result)) < 0) {
		fprintf(stderr, 
			_("Cannot read header metrics from archive %s: %s\n"),
			from_file, pmErrStr(sts));
		return 0;
	}

	if (result->numpmid != metrics->count) {
		pcp_free_result(result);
		fprintf(stderr, 
			_("Missing mandatory header metrics from archive %s\n"),
			from_file);
		return 0;
	}

	for (i = 0; i < FILE_HEADER_METRIC_COUNT; i++) {
		values = result->vset[i];
		if (values->numval != 1)
			continue;

		switch (values->pmid) {

			case PMID_FILE_HEADER_CPU_COUNT:
				cpu_count = pcp_read_u32(values, 0, metrics->descs,
							FILE_HEADER_CPU_COUNT);
				break;

			case PMID_FILE_HEADER_UNAME_SYSNAME:
				sysname = pcp_read_str(values, 0, metrics->descs,
							FILE_HEADER_UNAME_SYSNAME);
				break;

			case PMID_FILE_HEADER_UNAME_RELEASE:
				release = pcp_read_str(values, 0, metrics->descs,
							FILE_HEADER_UNAME_RELEASE);
				break;

			case PMID_FILE_HEADER_UNAME_NODENAME:
				nodename = pcp_read_str(values, 0, metrics->descs,
							FILE_HEADER_UNAME_NODENAME);
				break;

			case PMID_FILE_HEADER_UNAME_MACHINE:
				machine = pcp_read_str(values, 0, metrics->descs,
							FILE_HEADER_UNAME_MACHINE);
				break;
		}
	}

	pcp_free_result(result);

	if (sysname == NULL || release == NULL || machine == NULL) {
		fprintf(stderr,
			_("Missing host information values in archive %s\n"),
			from_file);
	}

	if (cpu_count > 0) {
		if (nodename == NULL)
			pmGetContextHostName_r(ctxid, host, sizeof(host));

		print_gal_header(&tm_time, sysname, release,
				 nodename ? nodename : host,
				 machine, cpu_count, PLAIN_OUTPUT);
	}

	free(sysname);
	free(release);
	free(nodename);
	free(machine);

	if (cpu_count == 0) {
		fprintf(stderr,
			_("Missing processor count metric in archive %s\n"),
			from_file);
	}

	/* Display sadc self-description if present (sadc -O pcp archives) */
	{
		char	*sadc_version = NULL;
		long	sadc_interval = 0;

		pcp_read_sadc_metrics(&sadc_version, &sadc_interval);
		if (sadc_version) {
			printf(_("Collected by sysstat %s"), sadc_version);
			if (sadc_interval > 0)
				printf(_(", %ld-second interval"), sadc_interval);
			printf("\n");
			free(sadc_version);
		}
		if (sadc_interval > 0)
			pcp_archive_interval = sadc_interval;
	}

	/*
	 * If interval is still unknown (no sadc.interval metric — e.g. archives
	 * created by "sadf -l"), derive it from the gap between the first two
	 * consecutive samples in the archive.
	 */
	if (pcp_archive_interval == 0) {
		/* Derive interval from two consecutive samples using kernel.all.cpu.user
		 * (PMID known; no name lookup needed for an archive context fetch) */
		pcp_result_t *r1 = NULL, *r2 = NULL;
		pmID cpu_id = PMID_CPU_ALLCPU_USER;

		pcp_set_mode(PM_MODE_FORW, &log_label.start, NULL);
		if (pcp_fetch(1, &cpu_id, &r1) >= 0) {
			if (pcp_fetch(1, &cpu_id, &r2) >= 0) {
				long gap = r2->timestamp.tv_sec - r1->timestamp.tv_sec;
				if (gap > 0)
					pcp_archive_interval = gap;
				pcp_free_result(r2);
			}
			pcp_free_result(r1);
		}
		pcp_set_mode(PM_MODE_FORW, &log_label.start, NULL);
	}

	return 1;	/* success */
}

/*
 ***************************************************************************
 * Read current activity's statistics from PCP archive and display them.
 *
 * IN:
 * @now		Timestamp archive position where sampling must start.
 * @end		Timestamp of archive end when sampling must stop.
 * @curr	Index in array for current sample statistics.
 * @rows	Number of rows of screen.
 * @act_id	Activity to display.
 * @file	Name of file being read.
 *
 * OUT:
 * @curr	Index in array for next sample statistics.
 * @cnt		Number of remaining lines of stats to write.
 * @reset	Set to TRUE if last_uptime variable should be reinitialized
 *		(used in next_slice() function).
 ***************************************************************************
 */
static int handle_curr_act_pcpstats(struct timespec *now, struct timespec *end,
		int *curr, long *cnt, int rows, int p, int *reset, const char *file)
{
	pmID *tp, *pmids = NULL;
	pcp_result_t *result;
	/*
	 * PM_MODE_FORW returns raw cumulative counter values as stored in the
	 * archive, which is what sar needs to compute its own rates.
	 * PM_MODE_INTERP would convert counters to rates, breaking sar's math.
	 * The delta is unused in PM_MODE_FORW but kept for the pcp_set_mode call.
	 */
	struct timespec delta = {0, 0};
	struct act_metrics *metrics = act[p]->metrics;
	unsigned int act_id = act[p]->id;
	unsigned long lines = 0;
	int numpmids, mode = PM_MODE_FORW;
	int sts, i, j, davg = 0, next, inc = 0;

	if (metrics == NULL)
		return PM_ERR_EOL;

	if ((sts = pcp_set_mode(mode, now, &delta)) < 0) {
		fprintf(stderr, _("Cannot set sample mode of archive %s\n"), file);
		return 0;
	}

	/* fetch 'record header' metrics every time plus current activity metrics */
	numpmids = RECORD_HEADER_METRIC_COUNT + metrics->count;
	if ((tp = calloc(numpmids, sizeof(pmID))) == NULL) {
		fprintf(stderr, _("Cannot allocate metric memory for %s\n"), file);
		return 0;
	}
	/* append IDs for each record header metric */
	for (j = 0; j < RECORD_HEADER_METRIC_COUNT; j++)
		tp[j] = record_header_metric_descs[j].pmid;
	for (i = 0; i < metrics->count; i++)
		tp[j+i] = metrics->descs[i].pmid;
	pmids = tp;

	/*
	 * Restore the first stats collected.
	 * Used to compute the rate displayed on the first line.
	 */
	copy_structures(act, id_seq, record_hdr, !*curr, 2);

	*cnt = count;

	/* Assess number of lines printed when a bitmap is used */
	if (act[p]->bitmap) {
		inc = count_bits(act[p]->bitmap->b_array,
				 BITMAP_SIZE(act[p]->bitmap->b_size));
	}

	do {
		if ((sts = pcp_fetch(numpmids, pmids, &result)) < 0) {
			if (sts != PM_ERR_EOL)
				fprintf(stderr, "%s: %s\n", "sar", pmErrStr(sts));
			break;
		}

		*now = result->timestamp;
		rectime.use = USE_EPOCH_T;
		rectime.epoch_time = now->tv_sec;

		/*
		 * Display <count> lines of stats.
		 */
		sts = read_stats_from_result(result, &file_hdr, *curr);
		pcp_free_result(result);

		if ((lines >= rows) || !lines) {
			lines = 0;
			dish = TRUE;
		}
		else
			dish = FALSE;

		if (sts == R_RESTART) {
			/* This is a mark record: Stop now */
			*reset = TRUE;
			break;
		}

		/* next is set to 1 when we were close enough to desired interval */
		next = write_stats_pcp(*curr, cnt, *reset, act_id);
		if (next && (*cnt > 0)) {
			(*cnt)--;
		}

		if (next) {
			davg++;
			*curr ^= 1;

			if (inc) {
				lines += inc;
			}
			else {
				lines += act[p]->nr[*curr];
			}
		}
		*reset = FALSE;

		if (*cnt == 0) {
			sts = PM_ERR_EOL;
			break;
		}
	}
	while (now->tv_sec <= end->tv_sec);

	free(pmids);

	/*
	 * At this moment, if we had a R_RESTART record, we still haven't read
	 * the number of CPU following it (nor the possible extra structures).
	 * But in this case, we always have @cnt != 0.
	 */

	if (davg) {
		write_stats_avg(!*curr, USE_SA_FILE, act_id);
	}

	return sts;
}

/*
 * Read statistics from a PCP system activity data file.
 *
 * IN:
 * @from_file	Input file name.
 ***************************************************************************
 */
static void read_stats_from_pcpfile(int ctxid, const char from_file[])
{
	struct timespec start = {0}, end = {PM_MAX_TIME_T, 0};
	long cnt = 1;
	int rows = get_win_height();
	unsigned int id;
	int i, j, p, sts, done = 0, curr = 1, reset = FALSE;

	j = 0;
	for (i = 0; i < NR_ACT; i++) {
		id = act[i]->id;
		if ((p = get_activity_position(act, id, RESUME_IF_NOT_FOUND)) < 0) {
			continue;	/* Unknown activity */
		}

                if (DISPLAY_HDR_ONLY(flags)) {
			id_seq[j++] = 0;
			continue;
		}

		id_seq[j++] = id;
	}

	/* Print report header */
	if (print_report_hdr_pcpfile(ctxid, from_file) == 0)
		return;

	if (tm_start.use != NO_TIME) {
		if ((sts = get_timespec_from_timestamp_struct(
				&log_label.start, &tm_start, &start)) != 0) {
			fprintf(stderr, _("Cannot decode requested start time\n"));
			return;
		}
	} else {
		start = log_label.start;
	}
	if (tm_end.use != NO_TIME &&
		(sts = get_timespec_from_timestamp_struct(
				&log_label.start, &tm_end, &end)) != 0) {
		fprintf(stderr, _("Cannot decode requested end time\n"));
		return;
	}

	/*
	 * Use PMIDs from our descriptor tables directly — no pmLookupName.
	 * Set nr_ini = nr2 = 1 for all PCP activities so allocate_structures
	 * makes a minimal initial allocation; pcp_read_* functions grow
	 * buffers via reallocate_buffers as actual instance counts are
	 * discovered from pcp_fetch results.
	 */
	for (i = 0; i < NR_ACT; i++) {
		if (act[i]->metrics) {
			act[i]->nr_ini = 1;
			if (act[i]->nr2 <= 0)
				act[i]->nr2 = 1;
		}
	}

	/* Perform required allocations */
	allocate_structures(act, flags);

	/* Read system statistics from PCP archive */
	do {
		/* Save the first stats collected. Will be used to compute the average */
		copy_structures(act, id_seq, record_hdr, 2, 0);

		reset = TRUE;	/* Set flag to reset last_uptime variable */

		/*
		 * Read and write stats located between two possible mark records.
		 * Activities that should be displayed are saved in id_seq[] array.
		 */
		for (i = 0; i < NR_ACT; i++) {

			if (!id_seq[i])
				continue;

			p = get_activity_position(act, id_seq[i], EXIT_IF_NOT_FOUND);
			if (!IS_SELECTED(act[p]->options))
				continue;

			if (!HAS_MULTIPLE_OUTPUTS(act[p]->options)) {
				if (handle_curr_act_pcpstats(&start, &end,
						&curr, &cnt, rows, p, &reset,
						from_file) == PM_ERR_EOL)
					done = 1;
			}
			else {
				unsigned int optf, msk;

				optf = act[p]->opt_flags;

				for (msk = 1; msk < 0x100; msk <<= 1) {
					if (!((act[p]->opt_flags & 0xff) & msk))
						continue;
					act[p]->opt_flags &= (0xffffff00 + msk);

					if (handle_curr_act_pcpstats(&start, &end,
							&curr, &cnt, rows, p, &reset,
							from_file) == PM_ERR_EOL)
						done = 1;

					act[p]->opt_flags = optf;
				}
			}
		}
	}
	while (!done);
}

/*
 ***************************************************************************
 * Try to open and read statistics from a PCP archive.
 *
 * IN:
 * @from_file	Input file name (may be a directory for PCP daily archives).
 * @flags	sadc/sar flags.
 *
 * RETURNS:
 * 1 if the file was a PCP archive and was successfully read, 0 otherwise.
 ***************************************************************************
 */
int try_read_stats_from_pcpfile(const char *from_file, uint64_t flags)
{
	int ctx;
	struct stat st;
	char pcp_base[MAX_FILE_LEN];
	const char *archive_path = from_file;

	if (stat(from_file, &st) == 0 && S_ISDIR(st.st_mode)) {
		const char *bn = strrchr(from_file, '/');
		bn = bn ? bn + 1 : from_file;
		snprintf(pcp_base, sizeof(pcp_base), "%s/%s", from_file, bn);
		archive_path = pcp_base;
	}

	if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive_path)) >= 0) {
		read_stats_from_pcpfile(ctx, archive_path);
		pmDestroyContext(ctx);
		return 1;
	}

	if (flags & S_F_PCP_INPUT) {
		fprintf(stderr,
			_("Cannot open PCP archive %s: %s\n"),
			archive_path, pmErrStr(ctx));
		exit(1);
	}

	return 0;
}

#endif /* HAVE_PCP */
