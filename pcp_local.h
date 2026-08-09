/*
 * pcp_local.h: Local PMDA metric collection for PCP archive output.
 * (C) 2026 Red Hat, Inc.
 */

#ifndef _PCP_LOCAL_H
#define _PCP_LOCAL_H

#ifdef HAVE_PMI_APPEND
#include <pcp/pmapi.h>
#include <pcp/import.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Named metric group from a pcpconf [HEADING] section.
 * Groups are selectively enabled/disabled via sadc -S.
 */
struct pcp_metric_group {
	char		 name[32];	/* uppercased section name */
	int		 enabled;	/* default TRUE, toggled by -S */
	unsigned int	 num_raw;	/* number of metric names */
	char	       **raw_metrics;	/* metric names from config */
};

/*
 * Per-instance domain state for local-context metrics.
 * inst_ids[] is kept sorted ascending so binary search gives O(log n) lookup.
 * inst_names[] runs parallel to inst_ids[].
 * Metric handle arrays (pcp_local_metric.handles[]) are indexed by slot,
 * where slot = sorted position in inst_ids[].
 */
struct pcp_local_indom {
	pmInDom  indom;
	size_t   hwm;		/* active instance count */
	size_t   capacity;	/* allocated size */
	int	*inst_ids;	/* sorted ascending by instance ID */
	char   **inst_names;	/* parallel PMDA-provided name strings */
};

/*
 * Per-metric descriptor with associated handle storage.
 */
struct pcp_local_metric {
	char			*name;
	pmID			 pmid;
	pmDesc			 desc;
	int			 indom_idx;	/* index into cfg->indoms, -1 for singleton */
	int			 handle;	/* singleton handle */
	int			*handles;	/* instanced: handles[slot] */
	size_t			 handle_cap;	/* mirrors indom->capacity */
};

/*
 * Parsed sysstat.pcpconf configuration.
 */
struct pcp_local_config {
	size_t			 volume_size;	/* data volume rotation size (bytes) */
	unsigned int		 num_metrics;
	unsigned int		 num_indoms;
	unsigned int		 num_groups;
	struct pcp_local_metric	*metrics;
	struct pcp_local_indom	*indoms;
	struct pcp_metric_group	*groups;	/* pcpconf metric groups */
	pmID			*pmids;		/* flat array for pmFetch */
};

/* Phase 1: parse pcpconf group sections (no PCP context needed) */
int  pcp_local_load_groups(struct pcp_local_config *cfg, const char *conffile);

/* Phase 2: open PM_CONTEXT_LOCAL, discover metrics for enabled groups */
int  pcp_local_init(struct pcp_local_config *cfg);

/* Register discovered metrics into the active PMI write context */
void pcp_local_register(struct pcp_local_config *cfg);

/* Fetch and write local-context metrics at this timestamp */
void pcp_local_write(struct pcp_local_config *cfg);

/* Return local context handle for help-text lookups */
int  pcp_local_get_ctx(void);

/* Write help text for local metrics into active PMI archive */
void pcp_local_write_help(const struct pcp_local_config *cfg);

/* Write help text for arbitrary PMIDs via local DSO PMDA context */
void pcp_local_write_pmid_help(const pmID *pmids, int n);

/* Release all resources */
void pcp_local_free(struct pcp_local_config *cfg);

/* -S integration: enable/disable pcpconf metric groups by name */
int  pcp_local_group_enable(const char *name, struct pcp_local_config *cfg);
int  pcp_local_group_disable(const char *name, struct pcp_local_config *cfg);
int  pcp_local_group_enable_all(struct pcp_local_config *cfg);
void pcp_local_group_disable_all(struct pcp_local_config *cfg);

/* Append enabled group names to a comma-separated activities string */
void pcp_local_append_group_names(const struct pcp_local_config *cfg,
				  char *buf, size_t len);

#else /* !HAVE_PMI_APPEND */

struct pcp_local_config { int dummy; };

static inline int
pcp_local_load_groups(struct pcp_local_config *c __attribute__((unused)),
		      const char *f __attribute__((unused)))
	{ return 0; }
static inline int
pcp_local_init(struct pcp_local_config *c __attribute__((unused)))
	{ return 0; }
static inline void
pcp_local_register(struct pcp_local_config *c __attribute__((unused))) {}
static inline void
pcp_local_write(struct pcp_local_config *c __attribute__((unused))) {}
static inline void
pcp_local_free(struct pcp_local_config *c __attribute__((unused))) {}
static inline int
pcp_local_group_enable(const char *n __attribute__((unused)),
		       struct pcp_local_config *c __attribute__((unused)))
	{ return -1; }
static inline int
pcp_local_group_disable(const char *n __attribute__((unused)),
			struct pcp_local_config *c __attribute__((unused)))
	{ return -1; }
static inline int
pcp_local_group_enable_all(struct pcp_local_config *c __attribute__((unused)))
	{ return 0; }
static inline void
pcp_local_group_disable_all(struct pcp_local_config *c __attribute__((unused))) {}
static inline void
pcp_local_append_group_names(const struct pcp_local_config *c __attribute__((unused)),
			     char *b __attribute__((unused)),
			     size_t l __attribute__((unused))) {}

#endif /* HAVE_PMI_APPEND */
#endif /* _PCP_LOCAL_H */
