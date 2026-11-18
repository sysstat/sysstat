/*
 * pcp_local.c: Local PMDA metric collection for PCP archive output.
 * (C) 2026 Red Hat, Inc.
 * (C) 2025-2026 by Sebastien Godard (sysstat <at> orange.fr)
 *
 * Reads /etc/sysconfig/sysstat.pcpconf to discover which metrics to collect
 * from a PM_CONTEXT_LOCAL context (pmda_proc.so and optionally others),
 * then writes them into the sadc PCP archive at a configurable interval.
 */

#ifdef HAVE_PMI_APPEND

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <pcp/pmapi.h>
#include <pcp/import.h>

#include "common.h"
#include "sysconfig.h"
#include "pcp_local.h"

#ifndef HAVE_NLS
# define _(x) (x)
#endif

/* PM_CONTEXT_LOCAL handle — saved at init, restored after each fetch */
static int local_ctx = -1;

/* Return the local context handle for help-text lookups in pcp_def_metrics.c */
int pcp_local_get_ctx(void) { return local_ctx; }

/*
 ***************************************************************************
 * Config file parsing and metric group management
 ***************************************************************************
 */

/* Growing string list used during config parsing and PMNS traversal */
struct strlist {
	char   **items;
	size_t	 count;
	size_t	 capacity;
};

static void strlist_add(struct strlist *sl, const char *s)
{
	char **tmp;
	char *d;

	if (sl->count >= sl->capacity) {
		sl->capacity = sl->capacity ? sl->capacity * 2 : 16;
		tmp = realloc(sl->items, sl->capacity * sizeof(char *));
		if (!tmp) { perror("realloc"); exit(4); }
		sl->items = tmp;
	}
	d = strdup(s);
	if (!d) { perror("strdup"); exit(4); }
	sl->items[sl->count++] = d;
}

static void strlist_free(struct strlist *sl)
{
	size_t i;

	for (i = 0; i < sl->count; i++)
		free(sl->items[i]);
	free(sl->items);
	sl->items = NULL;
	sl->count = sl->capacity = 0;
}

/* Static state for pmTraversePMNS callback (single-threaded init only) */
static struct strlist g_traverse;

static void traverse_cb(const char *name)
{
	strlist_add(&g_traverse, name);
}

static char *strip(char *s)
{
	char *end;

	while (isspace((unsigned char)*s))
		s++;
	if (!*s)
		return s;
	end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		*end-- = '\0';
	return s;
}

static size_t parse_size(const char *val)
{
	char *end;
	size_t v = (size_t)strtoull(val, &end, 10);

	switch (tolower((unsigned char)*end)) {
	case 'g': v *= 1024; /* fall through */
	case 'm': v *= 1024; /* fall through */
	case 'k': v *= 1024; break;
	}
	return v;
}

static void strtoupper(char *s)
{
	for (; *s; s++)
		*s = toupper((unsigned char)*s);
}

static struct pcp_metric_group *
group_add(struct pcp_local_config *cfg, const char *name)
{
	struct pcp_metric_group *g;
	void *_t;

	_t = realloc(cfg->groups, (cfg->num_groups + 1) * sizeof(*cfg->groups));
	if (!_t) { perror("realloc"); exit(4); }
	cfg->groups = _t;
	g = &cfg->groups[cfg->num_groups++];
	memset(g, 0, sizeof(*g));
	pmstrncpy(g->name, sizeof(g->name), name);
	strtoupper(g->name);
	return g;
}

static void group_add_metric(struct pcp_metric_group *g, const char *name)
{
	void *_t;
	char *d;

	_t = realloc(g->raw_metrics, (g->num_raw + 1) * sizeof(char *));
	if (!_t) { perror("realloc"); exit(4); }
	g->raw_metrics = _t;
	d = strdup(name);
	if (!d) { perror("strdup"); exit(4); }
	g->raw_metrics[g->num_raw++] = d;
}

/*
 ***************************************************************************
 * Phase 1: parse pcpconf group sections (no PCP context needed).
 *
 * Key-value pairs before the first [heading] are global settings.
 * Each [HEADING] starts a named metric group (uppercased, disabled
 * by default — enabled via -S like built-in activity groups).
 * Lines within a group are metric names.
 *
 * RETURNS:
 * 0 on success, -1 if file not found.
 ***************************************************************************
 */
int
pcp_local_load_groups(struct pcp_local_config *cfg, const char *conffile)
{
	FILE *fp;
	char line[1024];
	struct pcp_metric_group *cur_group = NULL;

	memset(cfg, 0, sizeof(*cfg));

	if ((fp = fopen(conffile, "r")) == NULL)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		char *p = strip(line);

		if (!*p || *p == '#')
			continue;

		if (*p == '[') {
			char *end = strchr(p, ']');
			if (!end)
				continue;
			*end = '\0';
			cur_group = group_add(cfg, p + 1);
			continue;
		}

		if (cur_group == NULL) {
			char *eq = strchr(p, '=');
			char *key, *val;

			if (!eq)
				continue;
			*eq = '\0';
			key = strip(p);
			val = strip(eq + 1);

			if (!strcmp(key, "volume_size"))
				cfg->volume_size = parse_size(val);
		} else {
			group_add_metric(cur_group, p);
		}
	}

	fclose(fp);
	return 0;
}

/*
 ***************************************************************************
 * -S integration: enable/disable metric groups by name.
 * Names are compared case-insensitively (uppercased internally).
 * Groups that collide with built-in -S keywords are unreachable
 * here because the built-in dispatch runs first; such groups just
 * stay disabled.
 *
 * RETURNS:
 * 0 if group found, -1 if not.
 ***************************************************************************
 */
int
pcp_local_group_enable(const char *name, struct pcp_local_config *cfg)
{
	char upper[32];
	unsigned int i;

	pmstrncpy(upper, sizeof(upper), name);
	strtoupper(upper);

	for (i = 0; i < cfg->num_groups; i++) {
		if (!strcmp(upper, cfg->groups[i].name)) {
			cfg->groups[i].enabled = 1;
			return 0;
		}
	}
	return -1;
}

int
pcp_local_group_disable(const char *name, struct pcp_local_config *cfg)
{
	char upper[32];
	unsigned int i;

	pmstrncpy(upper, sizeof(upper), name);
	strtoupper(upper);

	for (i = 0; i < cfg->num_groups; i++) {
		if (!strcmp(upper, cfg->groups[i].name)) {
			cfg->groups[i].enabled = 0;
			return 0;
		}
	}
	return -1;
}

int
pcp_local_group_enable_all(struct pcp_local_config *cfg)
{
	unsigned int i;

	for (i = 0; i < cfg->num_groups; i++)
		cfg->groups[i].enabled = 1;
	return cfg->num_groups;
}

void
pcp_local_group_disable_all(struct pcp_local_config *cfg)
{
	unsigned int i;

	for (i = 0; i < cfg->num_groups; i++)
		cfg->groups[i].enabled = 0;
}

void
pcp_local_append_group_names(const struct pcp_local_config *cfg,
			     char *buf, size_t len)
{
	unsigned int i;
	int any = (buf[0] != '\0');

	for (i = 0; i < cfg->num_groups; i++) {
		if (!cfg->groups[i].enabled)
			continue;
		if (any)
			pmstrncat(buf, len, ",");
		pmstrncat(buf, len, cfg->groups[i].name);
		any = 1;
	}
}

/*
 ***************************************************************************
 * Instance domain management
 ***************************************************************************
 */

/*
 * Find the slot for inst_id in the sorted indom, or (size_t)-1 if absent.
 */
static size_t
local_indom_find(const struct pcp_local_indom *id, int inst_id)
{
	size_t lo = 0, hi = id->hwm;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;

		if (id->inst_ids[mid] == inst_id)
			return mid;
		if (id->inst_ids[mid] < inst_id)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (size_t)-1;
}

/*
 * Insert inst_id at sorted position, growing all metric handle arrays that
 * reference this indom.  Returns the new slot index.
 *
 * IN:
 * @cfg		Full local config (needed to grow metric handle arrays).
 * @id		Indom to insert into.
 * @inst_id	Instance ID to insert.
 * @inst_name	PMDA-provided instance name string.
 *
 * RETURNS:
 * Slot index of the inserted instance.
 ***************************************************************************
 */
static size_t
local_indom_insert(struct pcp_local_config *cfg, struct pcp_local_indom *id,
		   int inst_id, const char *inst_name)
{
	size_t pos, i;
	void *_t;
	/* Find insertion point */
	for (pos = 0; pos < id->hwm; pos++) {
		if (id->inst_ids[pos] > inst_id)
			break;
	}

	/* Grow if needed */
	if (id->hwm >= id->capacity) {
		size_t new_cap = id->capacity ? id->capacity * 2 : 16;

		_t = realloc(id->inst_ids, new_cap * sizeof(int));
		if (!_t) { perror("realloc"); exit(4); }
		id->inst_ids   = _t;
		_t = realloc(id->inst_names, new_cap * sizeof(char *));
		if (!_t) { perror("realloc"); exit(4); }
		id->inst_names = _t;

		/* Grow handle arrays for every metric using this indom */
		for (i = 0; i < cfg->num_metrics; i++) {
			struct pcp_local_metric *m = &cfg->metrics[i];

			if (m->indom_idx < 0 || &cfg->indoms[m->indom_idx] != id)
				continue;
			_t = realloc(m->handles, new_cap * sizeof(int));
			if (!_t) { perror("realloc"); exit(4); }
			m->handles = _t;
			/* Initialise newly allocated slots to -1 */
			if (new_cap > m->handle_cap) {
				memset(m->handles + m->handle_cap, -1,
				       (new_cap - m->handle_cap) * sizeof(int));
			}
			m->handle_cap = new_cap;
		}
		id->capacity = new_cap;
	}

	/* Shift inst_ids and inst_names right to make room */
	if (pos < id->hwm) {
		memmove(&id->inst_ids[pos + 1],   &id->inst_ids[pos],
			(id->hwm - pos) * sizeof(int));
		memmove(&id->inst_names[pos + 1], &id->inst_names[pos],
			(id->hwm - pos) * sizeof(char *));

		/* Shift handle arrays right for every metric using this indom */
		for (i = 0; i < cfg->num_metrics; i++) {
			struct pcp_local_metric *m = &cfg->metrics[i];

			if (m->indom_idx < 0 || &cfg->indoms[m->indom_idx] != id)
				continue;
			memmove(&m->handles[pos + 1], &m->handles[pos],
				(id->hwm - pos) * sizeof(int));
		}
	}

	id->inst_ids[pos]   = inst_id;
	id->inst_names[pos] = strdup(inst_name);
	if (!id->inst_names[pos]) { perror("strdup"); exit(4); }

	/* Initialise handles at this slot for all referencing metrics */
	for (i = 0; i < cfg->num_metrics; i++) {
		if (cfg->metrics[i].indom_idx >= 0
		    && &cfg->indoms[cfg->metrics[i].indom_idx] == id)
			cfg->metrics[i].handles[pos] = -1;
	}

	id->hwm++;
	return pos;
}

/*
 * Find or create the indom entry for a given pmInDom value.
 * Returns the index into cfg->indoms (stable across future reallocs).
 */
static int
local_indom_get(struct pcp_local_config *cfg, pmInDom indom)
{
	size_t i;
	void *_t;

	for (i = 0; i < cfg->num_indoms; i++) {
		if (cfg->indoms[i].indom == indom)
			return (int)i;
	}

	_t = realloc(cfg->indoms, (cfg->num_indoms + 1) * sizeof(*cfg->indoms));
	if (!_t) { perror("realloc"); exit(4); }
	cfg->indoms = _t;
	memset(&cfg->indoms[cfg->num_indoms], 0, sizeof(*cfg->indoms));
	cfg->indoms[cfg->num_indoms].indom = indom;
	return (int)cfg->num_indoms++;
}

/*
 ***************************************************************************
 * Add a single leaf metric to the config.
 *
 * IN:
 * @cfg		Config to extend.
 * @name	Metric name (will be duped).
 *
 * RETURNS:
 * 0 on success, -1 on error (metric not found, or duplicate PMID).
 ***************************************************************************
 */
static int
local_metric_add(struct pcp_local_config *cfg, const char *name)
{
	pmID   pmid;
	pmDesc desc;
	int    sts;
	const char *np = name;
	struct pcp_local_metric *m;
	void *_t;

	sts = pmLookupName(1, &np, &pmid);
	if (sts < 0) {
		fprintf(stderr, _("PCP local: skipping metric '%s': %s\n"),
			name, pmErrStr(sts));
		return -1;
	}

	sts = pmLookupDesc(pmid, &desc);
	if (sts < 0) {
		fprintf(stderr, _("PCP local: skipping metric '%s': %s\n"),
			name, pmErrStr(sts));
		return -1;
	}

	_t = realloc(cfg->metrics, (cfg->num_metrics + 1) * sizeof(*cfg->metrics));
	if (!_t) { perror("realloc"); exit(4); }
	cfg->metrics = _t;
	memset(&cfg->metrics[cfg->num_metrics], 0, sizeof(*cfg->metrics));

	m = &cfg->metrics[cfg->num_metrics];
	m->name      = strdup(name);
	if (!m->name) { perror("strdup"); exit(4); }
	m->pmid      = pmid;
	m->desc      = desc;
	m->handle    = -1;
	m->indom_idx = -1;

	if (desc.indom != PM_INDOM_NULL) {
		m->indom_idx  = local_indom_get(cfg, desc.indom);
		m->handles    = NULL;
		m->handle_cap = 0;
	}

	cfg->num_metrics++;
	return 0;
}

/*
 ***************************************************************************
 * Phase 2: open PM_CONTEXT_LOCAL, resolve metrics for enabled groups.
 *
 * Must be called AFTER pcp_local_load_groups() and after -S option
 * parsing so that group enabled flags are final.
 ***************************************************************************
 */
int
pcp_local_init(struct pcp_local_config *cfg)
{
	unsigned int g, m;
	size_t j;
	int sts;

	/*
	 * Point libpcp at the local DSO PMDA configuration and namespace so
	 * that PM_CONTEXT_LOCAL loads the correct set of DSO PMDAs and resolves
	 * metric names against the matching local PMNS.
	 */
	char path[MAXPATHLEN];
	pmsprintf(path, sizeof(path), "%s/local.conf", pmGetConfig("PCP_SYSCONF_DIR"));
	setenv("PCP_PMCDCONF_FILE", path, 0);
	pmsprintf(path, sizeof(path), "%s/pmns/local.root", pmGetConfig("PCP_VAR_DIR"));
	setenv("PMNS_DEFAULT", path, 0);

	sts = pmNewContext(PM_CONTEXT_LOCAL, NULL);
	if (sts < 0) {
		fprintf(stderr, _("PCP local: pmNewContext failed: %s\n"),
			pmErrStr(sts));
		return -1;
	}
	local_ctx = sts;

	/* Expand and resolve metrics from enabled groups only */
	for (g = 0; g < cfg->num_groups; g++) {
		struct pcp_metric_group *grp = &cfg->groups[g];

		if (!grp->enabled)
			continue;

		for (m = 0; m < grp->num_raw; m++) {
			pmID pmid;
			const char *np = grp->raw_metrics[m];

			sts = pmLookupName(1, &np, &pmid);
			if (sts >= 0) {
				local_metric_add(cfg, grp->raw_metrics[m]);
			} else if (sts == PM_ERR_NONLEAF) {
				memset(&g_traverse, 0, sizeof(g_traverse));
				pmTraversePMNS(grp->raw_metrics[m], traverse_cb);
				for (j = 0; j < g_traverse.count; j++)
					local_metric_add(cfg, g_traverse.items[j]);
				strlist_free(&g_traverse);
			} else {
				fprintf(stderr,
					_("PCP local: skipping '%s': %s\n"),
					grp->raw_metrics[m], pmErrStr(sts));
			}
		}
	}

	if (!cfg->num_metrics)
		return -1;

	/* Build flat PMID array for pmFetch */
	cfg->pmids = malloc(cfg->num_metrics * sizeof(pmID));
	if (!cfg->pmids) { perror("malloc"); exit(4); }
	for (j = 0; j < cfg->num_metrics; j++)
		cfg->pmids[j] = cfg->metrics[j].pmid;

	return 0;
}

/*
 ***************************************************************************
 * Register local metrics into the active PMI write context.
 * Must be called AFTER pmiStart().  Silently skips metrics whose PMID is
 * already registered (avoiding double-writes with built-in sadc metrics).
 ***************************************************************************
 */
void
pcp_local_register(struct pcp_local_config *cfg)
{
	size_t i;

	for (i = 0; i < cfg->num_metrics; i++) {
		struct pcp_local_metric *m = &cfg->metrics[i];
		int sts;

		/* Use PMID/desc exactly as provided by the PMDA */
		sts = pmiAddMetric(m->name, m->pmid,
				   m->desc.type, m->desc.indom,
				   m->desc.sem,  m->desc.units);
		if (sts < 0) {
			/*
			 * PM_ERR_DUPPMID means the metric is already registered
			 * by sadc's built-in write path — skip silently.
			 */
			if (sts != PMI_ERR_DUPMETRICID) {
				fprintf(stderr,
					_("PCP local: pmiAddMetric '%s': %s\n"),
					m->name, pmiErrStr(sts));
			}
			/* Mark metric inactive by zeroing its pmid */
			m->pmid = PM_ID_NULL;
		}
	}
}

/*
 ***************************************************************************
 * Fetch and write local-context metrics into the PMI pending buffer.
 * Called from the sadc main loop when the local interval has elapsed.
 * Does NOT call pmiWrite — that is done by the main loop.
 *
 * IN:
 * @cfg		Local metric configuration.
 ***************************************************************************
 */
void
pcp_local_write(struct pcp_local_config *cfg)
{
	pmResult *result = NULL;
	int saved_ctx, sts;
	size_t i;

	if (!cfg->num_metrics || local_ctx < 0)
		return;

	/* Switch to the local fetch context */
	saved_ctx = pmWhichContext();
	pmUseContext(local_ctx);

	sts = pmFetch((int)cfg->num_metrics, cfg->pmids, &result);
	if (sts < 0) {
		fprintf(stderr, _("PCP local: pmFetch failed: %s\n"),
			pmErrStr(sts));
		pmUseContext(saved_ctx);
		return;
	}

	/* Restore write context before calling PMI functions */
	pmUseContext(saved_ctx);

	for (i = 0; i < (size_t)result->numpmid; i++) {
		pmValueSet *vset = result->vset[i];
		struct pcp_local_metric *m = NULL;
		size_t j;

		if (vset->numval <= 0)
			continue;

		/* Find matching metric by PMID */
		for (j = 0; j < cfg->num_metrics; j++) {
			if (cfg->metrics[j].pmid == vset->pmid) {
				m = &cfg->metrics[j];
				break;
			}
		}
		if (!m || m->pmid == PM_ID_NULL)
			continue;

		if (m->indom_idx < 0) {
			/* Singleton metric */
			pmAtomValue atom;

			if (pmExtractValue(vset->valfmt, &vset->vlist[0],
					   m->desc.type, &atom,
					   m->desc.type) < 0)
				continue;

			if (m->handle < 0)
				m->handle = pmiGetHandle(m->name, NULL);
			if (m->handle >= 0)
				pmiPutAtomValueHandle(m->handle, &atom);
		} else {
			/* Instanced metric — look up indom by stable index */
			struct pcp_local_indom *id = &cfg->indoms[m->indom_idx];
			int vi;

			for (vi = 0; vi < vset->numval; vi++) {
				int inst_id = vset->vlist[vi].inst;
				size_t slot;
				pmAtomValue atom;
				char *inst_name;

				slot = local_indom_find(id, inst_id);
				if (slot == (size_t)-1) {
					/*
					 * New instance: look up name from PMDA,
					 * register with PMI, insert into indom.
					 */
					pmUseContext(local_ctx);
					sts = pmNameInDom(m->desc.indom,
							  inst_id, &inst_name);
					pmUseContext(saved_ctx);
					if (sts < 0)
						continue;

					pmiAddInstance(m->desc.indom,
						       inst_name, inst_id);
					slot = local_indom_insert(cfg, id,
								  inst_id,
								  inst_name);
					free(inst_name);
					/* Reacquire pointer: insert may have realloc'd cfg->indoms */
					id = &cfg->indoms[m->indom_idx];
				}

				if (pmExtractValue(vset->valfmt,
						   &vset->vlist[vi],
						   m->desc.type, &atom,
						   m->desc.type) < 0)
					continue;

				if (m->handles[slot] < 0) {
					m->handles[slot] =
						pmiGetHandle(m->name,
							     id->inst_names[slot]);
				}
				if (m->handles[slot] >= 0)
					pmiPutAtomValueHandle(m->handles[slot],
							      &atom);
			}
		}
	}

	pmFreeResult(result);
}

/*
 ***************************************************************************
 * Write help text for all local-context metrics into the active PMI
 * write context.  Switches to the local context to call pmLookupText(),
 * then back to the PMI context to call pmiPutText().  Silently skips
 * any metric for which help text is unavailable.
 *
 * IN:
 * @cfg		Local metric configuration (metrics whose PMIDs to look up).
 ***************************************************************************
 */
void
pcp_local_write_help(const struct pcp_local_config *cfg)
{
	int saved_ctx, i;
	char *text;

	if (!cfg->num_metrics || local_ctx < 0)
		return;

	saved_ctx = pmWhichContext();
	pmUseContext(local_ctx);

	for (i = 0; i < (int)cfg->num_metrics; i++) {
		pmID pmid = cfg->metrics[i].pmid;

		if (pmLookupText(pmid, PM_TEXT_ONELINE, &text) >= 0) {
			pmUseContext(saved_ctx);
			pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmid, text);
			pmUseContext(local_ctx);
			free(text);
		}
		if (pmLookupText(pmid, PM_TEXT_HELP, &text) >= 0) {
			pmUseContext(saved_ctx);
			pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, pmid, text);
			pmUseContext(local_ctx);
			free(text);
		}
	}

	pmUseContext(saved_ctx);
}

/*
 ***************************************************************************
 * Write help text for an arbitrary set of PMIDs using the local DSO PMDA
 * context.  Used by sadc to populate archive help text for standard PCP
 * metrics (kernel.*, disk.*, mem.*, network.*, etc.) after all activities
 * have been registered.  Silently skips PMIDs with no text available.
 *
 * IN:
 * @pmids	Array of PMIDs to look up.
 * @n		Length of @pmids.
 ***************************************************************************
 */
void
pcp_local_write_pmid_help(const pmID *pmids, int n)
{
	int saved_ctx, i;
	char *text;

	if (local_ctx < 0 || n <= 0)
		return;

	saved_ctx = pmWhichContext();
	pmUseContext(local_ctx);

	for (i = 0; i < n; i++) {
		if (pmids[i] == PM_ID_NULL)
			continue;
		if (pmLookupText(pmids[i], PM_TEXT_ONELINE, &text) >= 0) {
			pmUseContext(saved_ctx);
			pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmids[i], text);
			pmUseContext(local_ctx);
			free(text);
		}
		if (pmLookupText(pmids[i], PM_TEXT_HELP, &text) >= 0) {
			pmUseContext(saved_ctx);
			pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, pmids[i], text);
			pmUseContext(local_ctx);
			free(text);
		}
	}

	pmUseContext(saved_ctx);
}

/*
 ***************************************************************************
 * Free all resources associated with a local metric configuration.
 ***************************************************************************
 */
void
pcp_local_free(struct pcp_local_config *cfg)
{
	size_t i;

	for (i = 0; i < cfg->num_metrics; i++) {
		free(cfg->metrics[i].name);
		free(cfg->metrics[i].handles);
	}
	free(cfg->metrics);

	for (i = 0; i < cfg->num_indoms; i++) {
		size_t j;

		for (j = 0; j < cfg->indoms[i].hwm; j++)
			free(cfg->indoms[i].inst_names[j]);
		free(cfg->indoms[i].inst_ids);
		free(cfg->indoms[i].inst_names);
	}
	free(cfg->indoms);
	free(cfg->pmids);

	for (i = 0; i < cfg->num_groups; i++) {
		unsigned int k;

		for (k = 0; k < cfg->groups[i].num_raw; k++)
			free(cfg->groups[i].raw_metrics[k]);
		free(cfg->groups[i].raw_metrics);
	}
	free(cfg->groups);

	if (local_ctx >= 0) {
		pmDestroyContext(local_ctx);
		local_ctx = -1;
	}

	memset(cfg, 0, sizeof(*cfg));
}

#endif /* HAVE_PMI_APPEND */
