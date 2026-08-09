/*
 * pcp_def_metrics.c: Functions used by sadf to define PCP metrics
 * (C) 2019-2026 by Sebastien GODARD (sysstat <at> orange.fr)
 * (C) 2026 Red Hat, Inc.  (Nathan Scott <nathans@redhat.com)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sa.h"

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#ifdef HAVE_PCP_IMPL_H
#include <pcp/impl.h>
#endif
#include "pcp_def_metrics.h"
#include "pcp_local.h"

extern struct activity *act[];
extern uint64_t flags;

/*
 ***************************************************************************
 * Register a scalar metric from an act_metrics group and return its write
 * handle.  Combines pmiAddMetric + pmiGetHandle into one call, avoiding the
 * repetitive .pmid/.type/.indom/.sem/.units expansion at every call site.
 *
 * IN:
 * @m		Metric group containing descriptor and name arrays.
 * @metric	Index within the group (e.g. RECORD_HEADER_KERNEL_UPTIME).
 *
 * RETURNS:
 * PMI write handle on success, -1 on failure.
 ***************************************************************************
 */
int act_register_scalar_handle(struct act_metrics *m, size_t metric)
{
	const char *name;
	pmDesc *desc;

	if (!m || metric >= m->count || !m->names || !m->descs)
		return -1;
	name = m->names[metric];
	desc = &m->descs[metric];
	pmiAddMetric(name, desc->pmid, desc->type, desc->indom, desc->sem, desc->units);
	return pmiGetHandle(name, NULL);
}

/*
 ***************************************************************************
 * Device classification helpers for secondary disk classes.
 *
 * IN:
 * @name	Kernel device name (as returned by get_device_name).
 *
 * RETURNS:
 * Non-zero if the device belongs to the indicated class.
 ***************************************************************************
 */
int is_dm_device(const char *name)
{
	return strncmp(name, "dm-", 3) == 0;
}

int is_md_device(const char *name)
{
	return strncmp(name, "md", 2) == 0 && isdigit((unsigned char)name[2]);
}

int is_zram_device(const char *name)
{
	return strncmp(name, "zram", 4) == 0;
}

int is_part_device(const char *name)
{
	size_t n = strlen(name);

	return n > 0 && isdigit((unsigned char)name[n - 1]) &&
	       !is_dm_device(name) && !is_md_device(name) &&
	       !is_zram_device(name);
}

/*
 ***************************************************************************
 * Resolve the persistent (human-readable) name for a dm device.
 *
 * Tries /sys/block/<kernel_name>/dm/name first, then walks /dev/mapper
 * matching by major:minor, and falls back to the kernel name.
 *
 * IN:
 * @kernel_name	Kernel block device name (e.g. "dm-0").
 * @major	Device major number.
 * @minor	Device minor number.
 * @out		Output buffer.
 * @len		Size of output buffer.
 ***************************************************************************
 */
void dm_persistent_name(const char *kernel_name, unsigned int dmajor,
			unsigned int dminor, char *out, size_t len)
{
	char path[512];
	int fd;
	ssize_t n;
	DIR *dp;
	struct dirent *de;
	struct stat sb;

	/* Try /sys/block/dm-X/dm/name first */
	pmsprintf(path, sizeof(path), "/sys/block/%s/dm/name", kernel_name);
	fd = open(path, O_RDONLY);
	if (fd >= 0) {
		n = read(fd, out, len - 1);
		close(fd);
		if (n > 0) {
			out[n] = '\0';
			/* strip trailing newline */
			if (n > 0 && out[n - 1] == '\n')
				out[n - 1] = '\0';
			return;
		}
	}

	/* Fallback: walk /dev/mapper matching by major:minor */
	pmsprintf(path, sizeof(path), "/dev/mapper");
	dp = opendir(path);
	if (dp) {
		while ((de = readdir(dp)) != NULL) {
			char fpath[512];

			pmsprintf(fpath, sizeof(fpath), "/dev/mapper/%s", de->d_name);
			if (stat(fpath, &sb) == 0 &&
			    (unsigned int)major(sb.st_rdev) == dmajor &&
			    (unsigned int)minor(sb.st_rdev) == dminor) {
				pmstrncpy(out, len - 1, de->d_name);
				closedir(dp);
				return;
			}
		}
		closedir(dp);
	}

	/* Last resort: use kernel name */
	pmstrncpy(out, len - 1, kernel_name);
}

/*
 * Per-class item lists and their sizes.
 * Populated by pcp_probe_*_instances(); used by pcp_def_disk_class_metrics()
 * and pcp_print_disk_stats().
 */
struct sa_item *dm_item_list;
size_t dm_item_list_sz;
struct sa_item *md_item_list;
size_t md_item_list_sz;
struct sa_item *part_item_list;
size_t part_item_list_sz;
struct sa_item *zram_item_list;
size_t zram_item_list_sz;

/*
 ***************************************************************************
 * Insert PCP metrics definition metadata into an archive.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @metric	Metric ID (from pcp_common.h enumerations).
 ***************************************************************************
 */
void act_add_metric(struct activity *a, int metric)
{
	struct act_metrics *metrics = a->metrics;
	const char *name;
	pmDesc *desc;
	int local, saved;
	char *text;

	if (!metrics || (size_t)metric >= metrics->count)
		PANIC(EINVAL);

	name = metrics->names[metric];
	desc = &metrics->descs[metric];
	pmiAddMetric(name, desc->pmid, desc->type, desc->indom, desc->sem, desc->units);

	/* Write help text from the local DSO PMDA into the archive if available */
	local = pcp_local_get_ctx();
	if (local < 0)
		return;
	saved = pmWhichContext();
	pmUseContext(local);
	if (pmLookupText(desc->pmid, PM_TEXT_ONELINE, &text) >= 0) {
		pmUseContext(saved);
		pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, desc->pmid, text);
		pmUseContext(local);
		free(text);
	}
	if (pmLookupText(desc->pmid, PM_TEXT_HELP, &text) >= 0) {
		pmUseContext(saved);
		pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, desc->pmid, text);
		pmUseContext(local);
		free(text);
	}
	pmUseContext(saved);
}

#ifdef HAVE_PMI_APPEND
/*
 ***************************************************************************
 * Register a metric using pmiExtraUnits() for physical dimensions that
 * the classic 6-component unit system cannot express (temperature, voltage,
 * current, power).  The static descriptor's classic fields are taken as-is;
 * extra_unit/extra_scale are appended via pmiExtraUnits().
 *
 * IN:
 * @a		Activity structure.
 * @metric	Metric index within the group.
 * @extra_unit	PM_UNIT_TEMPERATURE, PM_UNIT_VOLTAGE, etc.
 * @extra_scale	PM_TEMPERATURE_C, PM_VOLTAGE_V, etc.
 ***************************************************************************
 */
static void act_add_metric_ex(struct activity *a, int metric,
			      int extra_unit, unsigned int extra_scale)
{
	struct act_metrics *metrics = a->metrics;
	const char *name;
	pmDesc *desc;
	pmUnits units;

	if (!metrics || (size_t)metric >= metrics->count)
		PANIC(EINVAL);

	name = metrics->names[metric];
	desc = &metrics->descs[metric];
	units = pmiExtraUnits(desc->units.dimSpace, desc->units.dimTime,
			      desc->units.dimCount, desc->units.scaleSpace,
			      desc->units.scaleTime, desc->units.scaleCount,
			      extra_unit, extra_scale);
	pmiAddMetric(name, desc->pmid, desc->type, desc->indom, desc->sem, units);
}
#else
/* Fall back to registering with zero extra units on older PCP */
#define act_add_metric_ex(a, metric, extra_unit, extra_scale) \
	act_add_metric((a), (metric))
#endif /* HAVE_PMI_APPEND */

/*
 ***************************************************************************
 * Insert PCP instance metadata into an archive.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @metric	Metric ID (from pcp_common.h enumerations).
 * @name	External instance name.
 * @inst	Internal instance identifier.
 ***************************************************************************
 */
void act_add_instance(struct activity *a, int metric, char *name, int inst)
{
	struct act_metrics *metrics = a->metrics;
	pmDesc *desc;

	if (!metrics || (size_t)metric >= metrics->count)
		PANIC(EINVAL);

	desc = &metrics->descs[metric];
	pmiAddInstance(desc->indom, name, inst);
}

/*
 ***************************************************************************
 * Ensure the write-handle array for a metric group can accommodate
 * num_inst instances per metric.  Allocates on the first call; grows
 * (realloc) only when num_inst exceeds the current capacity.  New slots
 * are initialised to -1 (invalid handle).  No-op when num_inst is within
 * existing capacity.
 *
 * IN/OUT:
 * @m		Activity metrics group whose handle array to (re)allocate.
 * @num_inst	Number of instances required (>= 1).
 ***************************************************************************
 */
void pcp_alloc_handles(struct act_metrics *m, size_t num_inst)
{
	size_t old_slots, old_total, new_total;
	int *p, *q;

	if (num_inst < 1)
		num_inst = 1;
	if (m->handles && (int)num_inst <= m->max_inst)
		return;

	old_slots = (size_t)m->max_inst;
	old_total = m->count * old_slots;
	new_total = m->count * num_inst;

	p = realloc(m->handles, new_total * sizeof(int));
	if (!p)
		PANIC(ENOMEM);
	memset(p + old_total, -1, (new_total - old_total) * sizeof(int));
	m->handles = p;

	/*
	 * inst_ids is allocated only for dynamic (instanced) groups — those
	 * whose handles array was not pre-set to a static array.  Scalar
	 * groups are always PM_IN_NULL, so no lookup table is needed.
	 */
	if (m->inst_ids || old_slots == 0) {
		q = realloc(m->inst_ids, num_inst * sizeof(int));
		if (!q)
			PANIC(ENOMEM);
		/* Initialise new slots to PM_IN_NULL */
		for (size_t i = old_slots; i < num_inst; i++)
			q[i] = PM_IN_NULL;
		m->inst_ids = q;
	}

	m->max_inst = (int)num_inst;
}

/*
 ***************************************************************************
 * Obtain and store the write handle for one (metric, instance) pair.
 *
 * @metric	Metric index within the group (0-based).
 * @slot	Sequential array slot (0-based); use 0 for scalar metrics.
 * @inst_id	PCP instance identifier (PM_IN_NULL for scalar metrics).
 * @inst_name	Instance name string passed to pmiGetHandle; NULL for scalars.
 *
 * The handle array is grown automatically when slot exceeds current capacity.
 ***************************************************************************
 */
void pcp_alloc_handle(struct act_metrics *m, size_t metric,
		      size_t slot, int inst_id,
		      const char *inst_name)
{
	int h;

	pcp_alloc_handles(m, slot + 1);
	h = pmiGetHandle(m->names[metric], inst_name);
	/*
	 * pmiGetHandle returns a negative error code when there is no active
	 * PMI write context (e.g. when pcp_def_* is called from a read path
	 * like sadf -j reading a PCP archive).  Leave the slot at -1 in that
	 * case; pmiPutValueHandle will not be called on it.
	 */
	if (h >= 0)
		ACT_HANDLE(m, metric, slot) = h;
	if (m->inst_ids)
		m->inst_ids[slot] = inst_id;
}

/*
 ***************************************************************************
 * Find the handle-array slot for a given PCP instance identifier.
 * Returns (size_t)-1 if not found (e.g. on scalar-only groups where
 * inst_ids is NULL).
 *
 * IN:
 * @m		Metric group to search.
 * @inst_id	PCP instance identifier to look up.
 ***************************************************************************
 */
size_t pcp_find_slot(const struct act_metrics *m, int inst_id)
{
	int i;

	if (!m->inst_ids)
		return (size_t)-1;
	for (i = 0; i < m->max_inst; i++) {
		if (m->inst_ids[i] == inst_id)
			return (size_t)i;
	}
	return (size_t)-1;
}

/*
 ***************************************************************************
 * Wire write handles for a purely instanced metric group whose instances
 * are derived from a->item_list (disks, NICs, serial lines, sensors…).
 * Must be called after all pmiAddMetric and pmiAddInstance calls are done.
 *
 * IN:
 * @a		Activity whose metrics and item_list describe the group.
 ***************************************************************************
 */
static void
pcp_alloc_item_list_handles(struct activity *a)
{
	struct act_metrics *m = a->metrics;
	struct sa_item *list;
	size_t metric, slot;
	int n_inst = 0, inst_id = 0;

	for (list = a->item_list; list != NULL; list = list->next)
		n_inst++;
	pcp_alloc_handles(m, (size_t)(n_inst > 0 ? n_inst : 1));
	for (slot = 0, list = a->item_list;
	     list != NULL;
	     list = list->next, slot++, inst_id++) {
		for (metric = 0; metric < m->count; metric++)
			pcp_alloc_handle(m, metric, slot, inst_id, list->item_name);
	}
}

/*
 ***************************************************************************
 * Define PCP host metrics for an individual archive (file header).
 ***************************************************************************
 */
const char *file_header_metric_names[] = {
	[FILE_HEADER_CPU_COUNT] = "hinv.ncpu",
	[FILE_HEADER_KERNEL_HERTZ] = "kernel.all.hz",
	[FILE_HEADER_UNAME_RELEASE] = "kernel.uname.release",
	[FILE_HEADER_UNAME_SYSNAME] = "kernel.uname.sysname",
	[FILE_HEADER_UNAME_MACHINE] = "kernel.uname.machine",
	[FILE_HEADER_UNAME_NODENAME] = "kernel.uname.nodename",
	[FILE_HEADER_UNAME_FULL]     = "pmda.uname",
	[FILE_HEADER_BOOTTIME]       = "kernel.all.boottime",
	[FILE_HEADER_NDISK]          = "hinv.ndisk",
	[FILE_HEADER_NINTERFACE]     = "hinv.ninterface",
	[FILE_HEADER_PAGESIZE]       = "hinv.pagesize",
};
pmDesc file_header_metric_descs[] = {
	[FILE_HEADER_CPU_COUNT] = {
		.pmid = PMID_FILE_HEADER_CPU_COUNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_KERNEL_HERTZ] = {
		.pmid = PMID_FILE_HEADER_KERNEL_HERTZ,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_RELEASE] = {
		.pmid = PMID_FILE_HEADER_UNAME_RELEASE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_SYSNAME] = {
		.pmid = PMID_FILE_HEADER_UNAME_SYSNAME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_MACHINE] = {
		.pmid = PMID_FILE_HEADER_UNAME_MACHINE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_NODENAME] = {
		.pmid = PMID_FILE_HEADER_UNAME_NODENAME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_FULL] = {
		.pmid = PMID_FILE_HEADER_UNAME_FULL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_BOOTTIME] = {
		.pmid = PMID_FILE_HEADER_BOOTTIME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_SEC, 0),
		.type = PM_TYPE_64,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_NDISK] = {
		.pmid = PMID_FILE_HEADER_NDISK,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_NINTERFACE] = {
		.pmid = PMID_FILE_HEADER_NINTERFACE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_PAGESIZE] = {
		.pmid = PMID_FILE_HEADER_PAGESIZE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID file_header_metric_pmids[FILE_HEADER_METRIC_COUNT];

struct act_metrics file_header_metrics = {
	.count = FILE_HEADER_METRIC_COUNT,
	.descs = file_header_metric_descs,
	.names = file_header_metric_names,
	.pmids = file_header_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP host metrics for an individual sample (record header).
 ***************************************************************************
 */
const char *record_header_metric_names[] = {
	[RECORD_HEADER_KERNEL_UPTIME] = "kernel.all.uptime",
};
pmDesc record_header_metric_descs[] = {
	[RECORD_HEADER_KERNEL_UPTIME] = {
		.pmid = PMID_RECORD_HEADER_KERNEL_UPTIME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_SEC, 0),
		.type = PM_TYPE_DOUBLE,
		.sem = PM_SEM_INSTANT,
	},
};
pmID record_header_metric_pmids[RECORD_HEADER_METRIC_COUNT];

struct act_metrics record_header_metrics = {
	.count = RECORD_HEADER_METRIC_COUNT,
	.descs = record_header_metric_descs,
	.names = record_header_metric_names,
	.pmids = record_header_metric_pmids,
};

/*
 * sadc provenance metrics — written once per archive/event.
 * PMIDs are assigned dynamically by pmiAddMetric (PM_IN_NULL placeholders).
 * All metrics use scalar static handles (one slot, no instance domain).
 */
const char *sadc_metric_names[] = {
	[SADC_VERSION]    = "pmimport.version",
	[SADC_ACTIVITIES] = "pmimport.args",
	[SADC_INTERVAL]   = "pmimport.sadc.interval",
	[SADC_COMMENT]    = "pmimport.sadc.comment",
	[SADC_RESTARTS]   = "pmimport.sadc.restarts",
	[SADC_ARCHIVE]    = "pmimport.archive",
	[SADC_HOSTNAME]   = "pmimport.hostname",
	[SADC_TIMEZONE]   = "pmimport.timezone",
	[SADC_ZONEINFO]   = "pmimport.zoneinfo",
};
pmDesc sadc_metric_descs[] = {
	[SADC_VERSION] = {
		.pmid  = PMID_SADC_VERSION,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_ACTIVITIES] = {
		.pmid  = PMID_SADC_ACTIVITIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_INTERVAL] = {
		.pmid  = PMID_SADC_INTERVAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_SEC, 0),
		.type  = PM_TYPE_U32,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_COMMENT] = {
		.pmid  = PMID_SADC_COMMENT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_INSTANT,
	},
	[SADC_RESTARTS] = {
		.pmid  = PMID_SADC_RESTARTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type  = PM_TYPE_U32,
		.sem   = PM_SEM_INSTANT,
	},
	[SADC_ARCHIVE] = {
		.pmid  = PMID_SADC_ARCHIVE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_HOSTNAME] = {
		.pmid  = PMID_SADC_HOSTNAME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_TIMEZONE] = {
		.pmid  = PMID_SADC_TIMEZONE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
	[SADC_ZONEINFO] = {
		.pmid  = PMID_SADC_ZONEINFO,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type  = PM_TYPE_STRING,
		.sem   = PM_SEM_DISCRETE,
	},
};
pmID sadc_metric_pmids[SADC_METRIC_COUNT];
static int sadc_handles[SADC_METRIC_COUNT];

struct act_metrics sadc_metrics = {
	.count    = SADC_METRIC_COUNT,
	.max_inst = 1,
	.handles  = sadc_handles,
	.descs    = sadc_metric_descs,
	.names    = sadc_metric_names,
	.pmids    = sadc_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP instance for per-CPU interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @cpu		CPU number (0 is cpu0, 1 is cpu1, etc.)
 ***************************************************************************
 */
void pcp_def_percpu_intr_instances(struct activity *a, int cpu)
{
	int inst = 0;
	char buf[64];
	struct sa_item *list;

	/*
	 * kernel.percpu.interrupts lives in the cpu_metrics group (PMI_INDOM(60,0)),
	 * not irq_metrics, so go directly to pmiAddInstance rather than routing
	 * through act_add_instance which would index into the wrong metrics table.
	 */
	for (list = a->item_list; list != NULL; list = list->next) {
		pmsprintf(buf, sizeof(buf), "%s::cpu%d", list->item_name, cpu);
		pmiAddInstance(PMI_INDOM(60, 0), buf, inst++);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for per-CPU interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @cpu		CPU number (0 is cpu0, 1 is cpu1, etc.)
 ***************************************************************************
 */
void pcp_def_percpu_intr_metrics(struct activity *a, int cpu)
{
	/*
	 * kernel.percpu.interrupts is part of the CPU metric group, not the IRQ
	 * group.  Call pmiAddMetric directly rather than via act_add_metric which
	 * would incorrectly index into the calling activity's (IRQ) metrics table.
	 */
	pmiAddMetric("kernel.percpu.interrupts",
		     PMID_CPU_PERCPU_INTERRUPTS, PM_TYPE_U32,
		     PMI_INDOM(60, 0), PM_SEM_COUNTER,
		     pmiUnits(0, 0, 1, 0, 0, PM_COUNT_ONE));
}

/*
 ***************************************************************************
 * Define PCP metrics for global CPU statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */

void pcp_def_global_cpu_metrics(struct activity *a)
{
	act_add_metric(a, CPU_ALLCPU_USER);
	act_add_metric(a, CPU_ALLCPU_NICE);
	act_add_metric(a, CPU_ALLCPU_SYS);
	act_add_metric(a, CPU_ALLCPU_IDLE);
	act_add_metric(a, CPU_ALLCPU_WAITTOTAL);
	act_add_metric(a, CPU_ALLCPU_IRQSOFT);
	act_add_metric(a, CPU_ALLCPU_IRQHARD);
	act_add_metric(a, CPU_ALLCPU_STEAL);
	act_add_metric(a, CPU_ALLCPU_GUEST);
	act_add_metric(a, CPU_ALLCPU_GUESTNICE);
}

/*
 ***************************************************************************
 * Define PCP metrics for per-CPU statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_percpu_metrics(struct activity *a)
{
	act_add_metric(a, CPU_PERCPU_USER);
	act_add_metric(a, CPU_PERCPU_NICE);
	act_add_metric(a, CPU_PERCPU_SYS);
	act_add_metric(a, CPU_PERCPU_IDLE);
	act_add_metric(a, CPU_PERCPU_WAITTOTAL);
	act_add_metric(a, CPU_PERCPU_CPU_INTR);
	act_add_metric(a, CPU_PERCPU_IRQSOFT);
	act_add_metric(a, CPU_PERCPU_IRQHARD);
	act_add_metric(a, CPU_PERCPU_STEAL);
	act_add_metric(a, CPU_PERCPU_GUEST);
	act_add_metric(a, CPU_PERCPU_GUESTNICE);
}

/*
 ***************************************************************************
 * Define PCP instance for per-CPU instance domain.
 *
 * IN
 * @a		Activity structure with statistics.
 * @cpu		CPU number (0 is cpu0, 1 is cpu1, etc.)
 ***************************************************************************
 */
void pcp_def_percpu_instance(struct activity *a, int cpu)
{
	char buf[32];

	/*
	 * All per-CPU sysstat activities use the standard Linux per-CPU
	 * instance domain PMI_INDOM(60, 0).  Add the instance directly
	 * rather than routing through act_add_instance, which requires
	 * a valid per-CPU metric index in the calling activity's table.
	 */
	pmsprintf(buf, sizeof(buf), "cpu%d", cpu);
	pmiAddInstance(PMI_INDOM(60, 0), buf, cpu);
}

/*
 ***************************************************************************
 * Define PCP metrics for global CPU softnet statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_global_softnet_metrics(struct activity *a)
{
	act_add_metric(a, SOFTNET_ALLCPU_PROCESSED);
	act_add_metric(a, SOFTNET_ALLCPU_DROPPED);
	act_add_metric(a, SOFTNET_ALLCPU_TIMESQUEEZE);
	act_add_metric(a, SOFTNET_ALLCPU_RECEIVEDRPS);
	act_add_metric(a, SOFTNET_ALLCPU_FLOWLIMIT);
	act_add_metric(a, SOFTNET_ALLCPU_BACKLOGLENGTH);
}

/*
 ***************************************************************************
 * Define PCP metrics for per-CPU softnet statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_percpu_softnet_metrics(struct activity *a)
{
	act_add_metric(a, SOFTNET_PERCPU_PROCESSED);
	act_add_metric(a, SOFTNET_PERCPU_DROPPED);
	act_add_metric(a, SOFTNET_PERCPU_TIMESQUEEZE);
	act_add_metric(a, SOFTNET_PERCPU_RECEIVEDRPS);
	act_add_metric(a, SOFTNET_PERCPU_FLOWLIMIT);
	act_add_metric(a, SOFTNET_PERCPU_BACKLOGLENGTH);
}

/*
 ***************************************************************************
 * Define PCP metrics for CPU frequency statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_cpufreq_metrics(struct activity *a)
{
	act_add_metric(a, POWER_PERCPU_CLOCK);
}

/*
 ***************************************************************************
 * Define PCP metrics for CPU related statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_cpu_metrics(struct activity *a)
{
	int i, first = TRUE;

	for (i = 0; (i < a->nr_ini) &&
		    (a->bitmap == NULL || a->bitmap->b_array == NULL ||
		     i < a->bitmap->b_size + 1); i++) {

		/*
		 * Should current CPU (including CPU "all") be displayed?
		 * When bitmap or b_array is NULL (e.g. called from sadc to
		 * define archive metrics before allocate_bitmaps() runs),
		 * include all CPUs without filtering.
		 * NB: Offline not tested (they may be turned off and on within
		 * the same file).
		 */
		if (a->bitmap != NULL && a->bitmap->b_array != NULL &&
		    !IS_CPU_SELECTED(a->bitmap->b_array, i))
			/* CPU not selected */
			continue;

		if (!i) {
			if (a->id == A_CPU) {
				/* This is CPU "all" */
				pcp_def_global_cpu_metrics(a);
			}

			else if (a->id == A_NET_SOFT) {
				/* Create metrics for A_NET_SOFT */
				pcp_def_global_softnet_metrics(a);
			}
		}
		else {
			/* This is not CPU "all". */

			if (a->id == A_IRQ) {
				/* Create per-CPU interrupts metrics */
				pcp_def_percpu_intr_metrics(a, i - 1);
				pcp_def_percpu_intr_instances(a, i - 1);
			}

			else if (first) {
				/* First individual CPU: create instance AND metrics. */
				pcp_def_percpu_instance(a, i - 1);

				if (a->id == A_CPU) {
					/* Create metrics for A_CPU */
					pcp_def_percpu_metrics(a);
				}

				else if (a->id == A_PWR_CPU) {
					/* Create metric for A_PWR_CPU */
					pcp_def_pwr_cpufreq_metrics(a);
				}

				else if (a->id == A_NET_SOFT) {
					/* Create metrics for A_NET_SOFT */
					pcp_def_percpu_softnet_metrics(a);
				}

				first = FALSE;
			}

			else {
				/* Subsequent CPUs: instance only (metrics already registered). */
				pcp_def_percpu_instance(a, i - 1);
			}
		}
	}

	/*
	 * Wire write handles after all metrics and instances are registered.
	 * Per-CPU slot indices are sequential (0, 1, 2…) while inst_ids hold
	 * the actual CPU number so pcp_find_slot() can resolve cpu -> slot at
	 * write time when bitmap filtering produces non-contiguous CPU numbers.
	 */
	if (a->id == A_CPU || a->id == A_NET_SOFT) {
		size_t slot = 0;
		char cpuno[64];
		/*
		 * Pre-allocate with the final max_inst (= number of CPUs) so that
		 * scalar (all-CPU) handles are placed at the correct stride from the
		 * start.  Without this, the first per-CPU pcp_alloc_handle call that
		 * grows max_inst would leave scalar handles at the wrong positions.
		 */
		{
			int n_cpu = (a->nr_ini > 1) ? (a->nr_ini - 1) : 1;
			pcp_alloc_handles(a->metrics, (size_t)n_cpu);
		}

		if (a->id == A_CPU) {
			/* All-CPU scalar metrics */
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_USER,      0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_NICE,      0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_SYS,       0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_IDLE,      0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_WAITTOTAL, 0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_IRQSOFT,   0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_IRQHARD,   0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_STEAL,     0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_GUEST,     0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, CPU_ALLCPU_GUESTNICE, 0, PM_IN_NULL, NULL);
		} else {
			/* A_NET_SOFT all-CPU scalars */
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_PROCESSED,    0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_DROPPED,      0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_TIMESQUEEZE,  0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_RECEIVEDRPS,  0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_FLOWLIMIT,    0, PM_IN_NULL, NULL);
			pcp_alloc_handle(a->metrics, SOFTNET_ALLCPU_BACKLOGLENGTH,0, PM_IN_NULL, NULL);
		}

		/* Per-CPU instanced handles */
		for (i = 1; i < a->nr_ini; i++) {
			if (a->bitmap != NULL && a->bitmap->b_array != NULL &&
			    !IS_CPU_SELECTED(a->bitmap->b_array, i))
				continue;
			pmsprintf(cpuno, sizeof(cpuno), "cpu%d", i - 1);
			if (a->id == A_CPU) {
				pcp_alloc_handle(a->metrics, CPU_PERCPU_USER,      slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_NICE,      slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_SYS,       slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_IDLE,      slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_WAITTOTAL, slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_CPU_INTR,  slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_IRQSOFT,   slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_IRQHARD,   slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_STEAL,     slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_GUEST,     slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_GUESTNICE, slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, CPU_PERCPU_INTERRUPTS,slot, i-1, cpuno);
			} else {
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_PROCESSED,    slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_DROPPED,      slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_TIMESQUEEZE,  slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_RECEIVEDRPS,  slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_FLOWLIMIT,    slot, i-1, cpuno);
				pcp_alloc_handle(a->metrics, SOFTNET_PERCPU_BACKLOGLENGTH,slot, i-1, cpuno);
			}
			slot++;
		}
	}
	else if (a->id == A_PWR_CPU) {
		size_t slot = 0;
		char cpuno[64];

		for (i = 1; i < a->nr_ini; i++) {
			if (a->bitmap != NULL && a->bitmap->b_array != NULL &&
			    !IS_CPU_SELECTED(a->bitmap->b_array, i))
				continue;
			pmsprintf(cpuno, sizeof(cpuno), "cpu%d", i - 1);
			pcp_alloc_handle(a->metrics, POWER_PERCPU_CLOCK, slot, i-1, cpuno);
			slot++;
		}
	}
}

const char *cpu_metric_names[] = {
	[CPU_ALLCPU_USER] = "kernel.all.cpu.user",
	[CPU_ALLCPU_SYS] = "kernel.all.cpu.sys",
	[CPU_ALLCPU_NICE] = "kernel.all.cpu.nice",
	[CPU_ALLCPU_IDLE] = "kernel.all.cpu.idle",
	[CPU_ALLCPU_WAITTOTAL] = "kernel.all.cpu.wait.total",
	[CPU_ALLCPU_IRQTOTAL] = "kernel.all.cpu.irq.total",
	[CPU_ALLCPU_IRQSOFT] = "kernel.all.cpu.irq.soft",
	[CPU_ALLCPU_IRQHARD] = "kernel.all.cpu.irq.hard",
	[CPU_ALLCPU_STEAL] = "kernel.all.cpu.steal",
	[CPU_ALLCPU_GUEST] = "kernel.all.cpu.guest",
	[CPU_ALLCPU_GUESTNICE] = "kernel.all.cpu.guest_nice",
	[CPU_PERCPU_USER] = "kernel.percpu.cpu.user",
	[CPU_PERCPU_NICE] = "kernel.percpu.cpu.nice",
	[CPU_PERCPU_SYS] = "kernel.percpu.cpu.sys",
	[CPU_PERCPU_IDLE] = "kernel.percpu.cpu.idle",
	[CPU_PERCPU_WAITTOTAL] = "kernel.percpu.cpu.wait.total",
	[CPU_PERCPU_CPU_INTR] = "kernel.percpu.cpu.intr",
	[CPU_PERCPU_IRQSOFT] = "kernel.percpu.cpu.irq.soft",
	[CPU_PERCPU_IRQHARD] = "kernel.percpu.cpu.irq.hard",
	[CPU_PERCPU_STEAL] = "kernel.percpu.cpu.steal",
	[CPU_PERCPU_GUEST] = "kernel.percpu.cpu.guest",
	[CPU_PERCPU_GUESTNICE] = "kernel.percpu.cpu.guest_nice",
	[CPU_PERCPU_INTERRUPTS] = "kernel.percpu.interrupts",
};
pmDesc cpu_metric_descs[] = {
	[CPU_ALLCPU_USER] = {
		.pmid = PMID_CPU_ALLCPU_USER,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_NICE] = {
		.pmid = PMID_CPU_ALLCPU_NICE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_SYS] = {
		.pmid = PMID_CPU_ALLCPU_SYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IDLE] = {
		.pmid = PMID_CPU_ALLCPU_IDLE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_WAITTOTAL] = {
		.pmid = PMID_CPU_ALLCPU_WAITTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQTOTAL] = {
		.pmid = PMID_CPU_ALLCPU_IRQTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQSOFT] = {
		.pmid = PMID_CPU_ALLCPU_IRQSOFT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQHARD] = {
		.pmid = PMID_CPU_ALLCPU_IRQHARD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_STEAL] = {
		.pmid = PMID_CPU_ALLCPU_STEAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_GUEST] = {
		.pmid = PMID_CPU_ALLCPU_GUEST,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_GUESTNICE] = {
		.pmid = PMID_CPU_ALLCPU_GUESTNICE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_USER] = {
		.pmid = PMID_CPU_PERCPU_USER,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_NICE] = {
		.pmid = PMID_CPU_PERCPU_NICE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_SYS] = {
		.pmid = PMID_CPU_PERCPU_SYS,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IDLE] = {
		.pmid = PMID_CPU_PERCPU_IDLE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_WAITTOTAL] = {
		.pmid = PMID_CPU_PERCPU_WAITTOTAL,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_CPU_INTR] = {
		.pmid = PMID_CPU_PERCPU_CPU_INTR,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IRQSOFT] = {
		.pmid = PMID_CPU_PERCPU_IRQSOFT,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IRQHARD] = {
		.pmid = PMID_CPU_PERCPU_IRQHARD,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_STEAL] = {
		.pmid = PMID_CPU_PERCPU_STEAL,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_GUEST] = {
		.pmid = PMID_CPU_PERCPU_GUEST,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_GUESTNICE] = {
		.pmid = PMID_CPU_PERCPU_GUESTNICE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_INTERRUPTS] = {
		/*
		 * Written per-CPU using the standard per-CPU indom (60,0) with
		 * "cpuN" instances.  Represents the total interrupt count for
		 * each CPU; the per-interrupt-line breakdown is in pcp_print_irq_stats.
		 */
		.pmid = PMID_CPU_PERCPU_INTERRUPTS,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};
pmID cpu_metric_pmids[CPU_METRIC_COUNT];

struct act_metrics cpu_metrics = {
	.count = CPU_METRIC_COUNT,
	.descs = cpu_metric_descs,
	.names = cpu_metric_names,
	.pmids = cpu_metric_pmids,
};

const char *softnet_metric_names[] = {
	[SOFTNET_ALLCPU_PROCESSED] = "network.softnet.processed",
	[SOFTNET_ALLCPU_DROPPED] = "network.softnet.dropped",
	[SOFTNET_ALLCPU_TIMESQUEEZE] = "network.softnet.time_squeeze",
	[SOFTNET_ALLCPU_RECEIVEDRPS] = "network.softnet.received_rps",
	[SOFTNET_ALLCPU_FLOWLIMIT] = "network.softnet.flow_limit",
	[SOFTNET_ALLCPU_BACKLOGLENGTH] = "network.softnet.backlog_length",
	[SOFTNET_PERCPU_PROCESSED] = "network.softnet.percpu.processed",
	[SOFTNET_PERCPU_DROPPED] = "network.softnet.percpu.dropped",
	[SOFTNET_PERCPU_TIMESQUEEZE] = "network.softnet.percpu.time_squeeze",
	[SOFTNET_PERCPU_RECEIVEDRPS] = "network.softnet.percpu.received_rps",
	[SOFTNET_PERCPU_FLOWLIMIT] = "network.softnet.percpu.flow_limit",
	[SOFTNET_PERCPU_BACKLOGLENGTH] = "network.softnet.percpu.backlog_length",
};
pmDesc softnet_metric_descs[] = {
	[SOFTNET_ALLCPU_PROCESSED] = {
		.pmid = PMID_SOFTNET_ALLCPU_PROCESSED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_DROPPED] = {
		.pmid = PMID_SOFTNET_ALLCPU_DROPPED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_TIMESQUEEZE] = {
		.pmid = PMID_SOFTNET_ALLCPU_TIMESQUEEZE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_RECEIVEDRPS] = {
		.pmid = PMID_SOFTNET_ALLCPU_RECEIVEDRPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_FLOWLIMIT] = {
		.pmid = PMID_SOFTNET_ALLCPU_FLOWLIMIT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_BACKLOGLENGTH] = {
		.pmid = PMID_SOFTNET_ALLCPU_BACKLOGLENGTH,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_PROCESSED] = {
		.pmid = PMID_SOFTNET_PERCPU_PROCESSED,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_DROPPED] = {
		.pmid = PMID_SOFTNET_PERCPU_DROPPED,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_TIMESQUEEZE] = {
		.pmid = PMID_SOFTNET_PERCPU_TIMESQUEEZE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_RECEIVEDRPS] = {
		.pmid = PMID_SOFTNET_PERCPU_RECEIVEDRPS,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_FLOWLIMIT] = {
		.pmid = PMID_SOFTNET_PERCPU_FLOWLIMIT,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_BACKLOGLENGTH] = {
		.pmid = PMID_SOFTNET_PERCPU_BACKLOGLENGTH,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID softnet_metric_pmids[SOFTNET_METRIC_COUNT];

struct act_metrics softnet_metrics = {
	.count = SOFTNET_METRIC_COUNT,
	.descs = softnet_metric_descs,
	.names = softnet_metric_names,
	.pmids = softnet_metric_pmids,
};

const char *power_cpu_metric_names[] = {
	[POWER_PERCPU_CLOCK] = "hinv.cpu.clock",
};
pmDesc power_cpu_metric_descs[] = {
	[POWER_PERCPU_CLOCK] = {
		.pmid = PMID_POWER_PERCPU_CLOCK,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, -1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID power_cpu_metric_pmids[POWER_CPU_METRIC_COUNT];

struct act_metrics power_cpu_metrics = {
	.count = POWER_CPU_METRIC_COUNT,
	.descs = power_cpu_metric_descs,
	.names = power_cpu_metric_names,
	.pmids = power_cpu_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for task creation and context switch statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pcsw_metrics(struct activity *a)
{
	act_add_metric(a, PCSW_CONTEXT_SWITCH);
	act_add_metric(a, PCSW_FORK_SYSCALLS);

	for (size_t _i = 0; _i < PCSW_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *pcsw_metric_names[] = {
	[PCSW_CONTEXT_SWITCH] = "kernel.all.pswitch",
	[PCSW_FORK_SYSCALLS] = "kernel.all.sysfork",
};
pmDesc pcsw_metric_descs[] = {
	[PCSW_CONTEXT_SWITCH] = {
		.pmid = PMID_PCSW_CONTEXT_SWITCH,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PCSW_FORK_SYSCALLS] = {
		.pmid = PMID_PCSW_FORK_SYSCALLS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID pcsw_metric_pmids[PCSW_METRIC_COUNT];

static int pcsw_handles[PCSW_METRIC_COUNT];

struct act_metrics pcsw_metrics = {
	.count = PCSW_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= pcsw_handles,
	.descs = pcsw_metric_descs,
	.names = pcsw_metric_names,
	.pmids = pcsw_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_irq_metrics(struct activity *a)
{
	int first = TRUE, inst = 0;
	struct sa_item *list;

	if (a->bitmap != NULL && a->bitmap->b_array != NULL &&
	    !(a->bitmap->b_array[0] & 1))
		/* CPU "all" not selected: Nothing to do here */
		return;

	/*
	 * Always register the total interrupt count (kernel.all.intr): sadc
	 * collects it from /proc/stat regardless of whether a per-interrupt
	 * filter has been requested on the command line.
	 */
	act_add_metric(a, IRQ_ALLIRQ_TOTAL);

	/* Create instances and metrics for each interrupts for CPU "all" */
	for (list = a->item_list; list != NULL; list = list->next) {

		if (!strcmp(list->item_name, K_LOWERSUM)) {
			/* "sum" already registered above; skip duplicate */
		}
		else {
			if (first) {
				/* Create metric for a common interrupt for CPU "all" if not already done */
				act_add_metric(a, IRQ_PERIRQ_TOTAL);
				first = FALSE;
			}
			/* Create instance */
			act_add_instance(a, IRQ_PERIRQ_TOTAL, list->item_name, inst++);
		}
	}

	/* Wire handles after all metrics and instances are registered */
	pcp_alloc_handle(a->metrics, IRQ_ALLIRQ_TOTAL, 0, PM_IN_NULL, NULL);
	inst = 0;
	for (list = a->item_list; list != NULL; list = list->next) {
		if (!strcmp(list->item_name, K_LOWERSUM))
			continue;
		pcp_alloc_handle(a->metrics, IRQ_PERIRQ_TOTAL,
				 (size_t)inst, inst, list->item_name);
		inst++;
	}
}

const char *irq_metric_names[] = {
	[IRQ_ALLIRQ_TOTAL] = "kernel.all.intr",
	[IRQ_PERIRQ_TOTAL] = "kernel.all.interrupts.total",
};
pmDesc irq_metric_descs[] = {
	[IRQ_ALLIRQ_TOTAL] = {
		.pmid = PMID_IRQ_ALLIRQ_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IRQ_PERIRQ_TOTAL] = {
		.pmid = PMID_IRQ_PERIRQ_TOTAL,
		.indom = PMI_INDOM(60, 4),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID irq_metric_pmids[IRQ_METRIC_COUNT];

struct act_metrics irq_metrics = {
	.count = IRQ_METRIC_COUNT,
	.descs = irq_metric_descs,
	.names = irq_metric_names,
	.pmids = irq_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for swapping statistics.
 ***************************************************************************
 */
void pcp_def_swap_metrics(struct activity *a)
{
	act_add_metric(a, SWAP_PAGESIN);
	act_add_metric(a, SWAP_PAGESOUT);

	for (size_t _i = 0; _i < SWAP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *swap_metric_names[] = {
	[SWAP_PAGESIN] = "swap.pagesin",
	[SWAP_PAGESOUT] = "swap.pagesout",
};
pmDesc swap_metric_descs[] = {
	[SWAP_PAGESIN] = {
		.pmid = PMID_SWAP_PAGESIN,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SWAP_PAGESOUT] = {
		.pmid = PMID_SWAP_PAGESOUT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID swap_metric_pmids[SWAP_METRIC_COUNT];

static int swap_handles[SWAP_METRIC_COUNT];

struct act_metrics swap_metrics = {
	.count = SWAP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= swap_handles,
	.descs = swap_metric_descs,
	.names = swap_metric_names,
	.pmids = swap_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for paging statistics.
 ***************************************************************************
 */
void pcp_def_paging_metrics(struct activity *a)
{
	act_add_metric(a, PAGING_PGPGIN);
	act_add_metric(a, PAGING_PGPGOUT);
	act_add_metric(a, PAGING_PGFAULT);
	act_add_metric(a, PAGING_PGMAJFAULT);
	act_add_metric(a, PAGING_PGFREE);
	act_add_metric(a, PAGING_PGSCANDIRECT);
	act_add_metric(a, PAGING_PGSCANKSWAPD);
	act_add_metric(a, PAGING_PGSTEAL);
	act_add_metric(a, PAGING_PGPROMOTE);
	act_add_metric(a, PAGING_PGDEMOTE);

	for (size_t _i = 0; _i < PAGING_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *paging_metric_names[] = {
	[PAGING_PGPGIN] = "mem.vmstat.pgpgin",
	[PAGING_PGPGOUT] = "mem.vmstat.pgpgout",
	[PAGING_PGFAULT] = "mem.vmstat.pgfault",
	[PAGING_PGMAJFAULT] = "mem.vmstat.pgmajfault",
	[PAGING_PGFREE] = "mem.vmstat.pgfree",
	[PAGING_PGSCANDIRECT] = "mem.vmstat.pgscan_direct_total",
	[PAGING_PGSCANKSWAPD] = "mem.vmstat.pgscan_kswapd_total",
	[PAGING_PGSTEAL] = "mem.vmstat.pgsteal_total",
	[PAGING_PGDEMOTE] = "mem.vmstat.pgdemote_total",
	[PAGING_PGPROMOTE] = "mem.vmstat.pgpromote_success",
};
pmDesc paging_metric_descs[] = {
	[PAGING_PGPGIN] = {
		.pmid = PMID_PAGING_PGPGIN,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGPGOUT] = {
		.pmid = PMID_PAGING_PGPGOUT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGFAULT] = {
		.pmid = PMID_PAGING_PGFAULT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGMAJFAULT] = {
		.pmid = PMID_PAGING_PGMAJFAULT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGFREE] = {
		.pmid = PMID_PAGING_PGFREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSCANDIRECT] = {
		.pmid = PMID_PAGING_PGSCANDIRECT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSCANKSWAPD] = {
		.pmid = PMID_PAGING_PGSCANKSWAPD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSTEAL] = {
		.pmid = PMID_PAGING_PGSTEAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGDEMOTE] = {
		.pmid = PMID_PAGING_PGDEMOTE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGPROMOTE] = {
		.pmid = PMID_PAGING_PGPROMOTE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID paging_metric_pmids[PAGING_METRIC_COUNT];

static int paging_handles[PAGING_METRIC_COUNT];

struct act_metrics paging_metrics = {
	.count = PAGING_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= paging_handles,
	.descs = paging_metric_descs,
	.names = paging_metric_names,
	.pmids = paging_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for I/O and transfer rate statistics.
 ***************************************************************************
 */
void pcp_def_io_metrics(struct activity *a)
{
	act_add_metric(a, IO_ALLDEV_TOTAL);
	act_add_metric(a, IO_ALLDEV_READ);
	act_add_metric(a, IO_ALLDEV_WRITE);
	act_add_metric(a, IO_ALLDEV_DISCARD);
	act_add_metric(a, IO_ALLDEV_READBYTES);
	act_add_metric(a, IO_ALLDEV_WRITEBYTES);
	act_add_metric(a, IO_ALLDEV_DISCARDBYTES);

	for (size_t _i = 0; _i < IO_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *io_metric_names[] = {
	[IO_ALLDEV_TOTAL] = "disk.all.total",
	[IO_ALLDEV_READ] = "disk.all.read",
	[IO_ALLDEV_WRITE] = "disk.all.write",
	[IO_ALLDEV_DISCARD] = "disk.all.discard",
	[IO_ALLDEV_READBYTES] = "disk.all.read_bytes",
	[IO_ALLDEV_WRITEBYTES] = "disk.all.write_bytes",
	[IO_ALLDEV_DISCARDBYTES] = "disk.all.discard_bytes",
};
pmDesc io_metric_descs[] = {
	[IO_ALLDEV_TOTAL] = {
		.pmid = PMID_IO_ALLDEV_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_READ] = {
		.pmid = PMID_IO_ALLDEV_READ,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_WRITE] = {
		.pmid = PMID_IO_ALLDEV_WRITE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_DISCARD] = {
		.pmid = PMID_IO_ALLDEV_DISCARD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_READBYTES] = {
		.pmid = PMID_IO_ALLDEV_READBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_WRITEBYTES] = {
		.pmid = PMID_IO_ALLDEV_WRITEBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_DISCARDBYTES] = {
		.pmid = PMID_IO_ALLDEV_DISCARDBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID io_metric_pmids[IO_METRIC_COUNT];

static int io_handles[IO_METRIC_COUNT];

struct act_metrics io_metrics = {
	.count = IO_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= io_handles,
	.descs = io_metric_descs,
	.names = io_metric_names,
	.pmids = io_metric_pmids,
};

/*
 * **************************************************************************
 * Define PCP metrics for main memory utilization.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_ram_memory_metrics(struct activity *a)
{
	act_add_metric(a, MEM_PHYS_MB);
	act_add_metric(a, MEM_PHYS_KB);
	act_add_metric(a, MEM_UTIL_FREE);
	act_add_metric(a, MEM_UTIL_SHARED);
	act_add_metric(a, MEM_UTIL_AVAIL);
	act_add_metric(a, MEM_UTIL_USED);
	act_add_metric(a, MEM_UTIL_BUFFER);
	act_add_metric(a, MEM_UTIL_CACHED);
	act_add_metric(a, MEM_UTIL_COMMITAS);
	act_add_metric(a, MEM_UTIL_ACTIVE);
	act_add_metric(a, MEM_UTIL_INACTIVE);
	act_add_metric(a, MEM_UTIL_DIRTY);
}

/*
 * **************************************************************************
 * Define PCP metrics for all memory utilization.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_all_memory_metrics(struct activity *a)
{
	act_add_metric(a, MEM_UTIL_ANON);
	act_add_metric(a, MEM_UTIL_SLAB);
	act_add_metric(a, MEM_UTIL_KSTACK);
	act_add_metric(a, MEM_UTIL_PGTABLE);
	act_add_metric(a, MEM_UTIL_VMALLOC);
}

/*
 * **************************************************************************
 * Define PCP metrics for swap memory utilization.
 ***************************************************************************
 */
void pcp_def_swap_memory_metrics(struct activity *a)
{
	act_add_metric(a, MEM_UTIL_SWAPFREE);
	act_add_metric(a, MEM_UTIL_SWAPTOTAL);
	act_add_metric(a, MEM_UTIL_SWAPCACHED);
	act_add_metric(a, MEM_UTIL_SHMEM);
	act_add_metric(a, MEM_UTIL_FREEMEM);
	act_add_metric(a, SWAP_FREE);
	act_add_metric(a, SWAP_LENGTH);
}

/*
 ***************************************************************************
 * Define PCP metrics for memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_memory_metrics(struct activity *a)
{
	pcp_def_ram_memory_metrics(a);
	pcp_def_all_memory_metrics(a);
	pcp_def_swap_memory_metrics(a);

	for (size_t _i = 0; _i < MEM_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *mem_metric_names[] = {
	[MEM_PHYS_MB] = "hinv.physmem",
	[MEM_PHYS_KB] = "mem.physmem",
	[MEM_UTIL_FREE] = "mem.util.free",
	[MEM_UTIL_SHARED] = "mem.util.shared",
	[MEM_UTIL_AVAIL] = "mem.util.available",
	[MEM_UTIL_USED] = "mem.util.used",
	[MEM_UTIL_BUFFER] = "mem.util.bufmem",
	[MEM_UTIL_CACHED] = "mem.util.cached",
	[MEM_UTIL_COMMITAS] = "mem.util.committed_AS",
	[MEM_UTIL_ACTIVE] = "mem.util.active",
	[MEM_UTIL_INACTIVE] = "mem.util.inactive",
	[MEM_UTIL_DIRTY] = "mem.util.dirty",
	[MEM_UTIL_ANON] = "mem.util.anonpages",
	[MEM_UTIL_SLAB] = "mem.util.slab",
	[MEM_UTIL_KSTACK] = "mem.util.kernelStack",
	[MEM_UTIL_PGTABLE] = "mem.util.pageTables",
	[MEM_UTIL_VMALLOC] = "mem.util.vmallocUsed",
	[MEM_UTIL_SWAPFREE] = "mem.util.swapFree",
	[MEM_UTIL_SWAPTOTAL] = "mem.util.swapTotal",
	[MEM_UTIL_SWAPCACHED] = "mem.util.swapCached",
	[MEM_UTIL_SHMEM] = "mem.util.shmem",
	[MEM_UTIL_FREEMEM] = "mem.freemem",
	[SWAP_FREE] = "swap.free",
	[SWAP_LENGTH] = "swap.length",
};
pmDesc mem_metric_descs[] = {
	[MEM_PHYS_MB] = {
		.pmid = PMID_MEM_PHYS_MB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[MEM_PHYS_KB] = {
		.pmid = PMID_MEM_PHYS_KB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_DISCRETE,
	},
	[MEM_UTIL_FREE] = {
		.pmid = PMID_MEM_UTIL_FREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SHARED] = {
		.pmid = PMID_MEM_UTIL_SHARED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_AVAIL] = {
		.pmid = PMID_MEM_UTIL_AVAIL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_USED] = {
		.pmid = PMID_MEM_UTIL_USED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_BUFFER] = {
		.pmid = PMID_MEM_UTIL_BUFFER,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_CACHED] = {
		.pmid = PMID_MEM_UTIL_CACHED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_COMMITAS] = {
		.pmid = PMID_MEM_UTIL_COMMITAS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_ACTIVE] = {
		.pmid = PMID_MEM_UTIL_ACTIVE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_INACTIVE] = {
		.pmid = PMID_MEM_UTIL_INACTIVE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_DIRTY] = {
		.pmid = PMID_MEM_UTIL_DIRTY,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_ANON] = {
		.pmid = PMID_MEM_UTIL_ANON,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SLAB] = {
		.pmid = PMID_MEM_UTIL_SLAB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_KSTACK] = {
		.pmid = PMID_MEM_UTIL_KSTACK,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_PGTABLE] = {
		.pmid = PMID_MEM_UTIL_PGTABLE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_VMALLOC] = {
		.pmid = PMID_MEM_UTIL_VMALLOC,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPFREE] = {
		.pmid = PMID_MEM_UTIL_SWAPFREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPTOTAL] = {
		.pmid = PMID_MEM_UTIL_SWAPTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPCACHED] = {
		.pmid = PMID_MEM_UTIL_SWAPCACHED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SHMEM] = {
		.pmid = PMID_MEM_UTIL_SHMEM,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_FREEMEM] = {
		.pmid = PMID_MEM_UTIL_FREEMEM,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[SWAP_FREE] = {
		.pmid = PMID_SWAP_FREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[SWAP_LENGTH] = {
		.pmid = PMID_SWAP_LENGTH,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};
pmID mem_metric_pmids[MEM_METRIC_COUNT];

static int mem_handles[MEM_METRIC_COUNT];

struct act_metrics mem_metrics = {
	.count = MEM_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= mem_handles,
	.descs = mem_metric_descs,
	.names = mem_metric_names,
	.pmids = mem_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for kernel tables statistics.
 ***************************************************************************
 */
void pcp_def_ktables_metrics(struct activity *a)
{
	act_add_metric(a, KTABLE_DENTRYS);
	act_add_metric(a, KTABLE_FILES);
	act_add_metric(a, KTABLE_INODES);
	act_add_metric(a, KTABLE_PTYS);

	for (size_t _i = 0; _i < KTABLE_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *ktable_metric_names[] = {
	[KTABLE_DENTRYS] = "vfs.dentry.count",
	[KTABLE_FILES] = "vfs.files.count",
	[KTABLE_INODES] = "vfs.inodes.count",
	[KTABLE_PTYS] = "kernel.all.nptys",
};
pmDesc ktable_metric_descs[] = {
	[KTABLE_DENTRYS] = {
		.pmid = PMID_KTABLE_DENTRYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_FILES] = {
		.pmid = PMID_KTABLE_FILES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_INODES] = {
		.pmid = PMID_KTABLE_INODES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_PTYS] = {
		.pmid = PMID_KTABLE_PTYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};
pmID ktable_metric_pmids[KTABLE_METRIC_COUNT];

static int ktable_handles[KTABLE_METRIC_COUNT];

struct act_metrics ktable_metrics = {
	.count = KTABLE_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= ktable_handles,
	.descs = ktable_metric_descs,
	.names = ktable_metric_names,
	.pmids = ktable_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for queue and load statistics.
 ***************************************************************************
 */
void pcp_def_queue_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 2);

	pmiAddInstance(indom, "1 minute", 1);
	pmiAddInstance(indom, "5 minute", 5);
	pmiAddInstance(indom, "15 minute", 15);

	act_add_metric(a, KQUEUE_RUNNABLE);
	act_add_metric(a, KQUEUE_PROCESSES);
	act_add_metric(a, KQUEUE_BLOCKED);
	act_add_metric(a, KQUEUE_LOADAVG);

	/*
	 * Pre-allocate to the final max_inst (3 load-average periods) before
	 * any scalar handles so that a later pcp_alloc_handles() realloc does
	 * not corrupt the stride of already-allocated scalar handles.
	 */
	pcp_alloc_handles(a->metrics, 3);

	/* Scalar metrics use slot 0 */
	pcp_alloc_handle(a->metrics, KQUEUE_RUNNABLE,  0, PM_IN_NULL, NULL);
	pcp_alloc_handle(a->metrics, KQUEUE_PROCESSES, 0, PM_IN_NULL, NULL);
	pcp_alloc_handle(a->metrics, KQUEUE_BLOCKED,   0, PM_IN_NULL, NULL);
	/* kernel.all.load: inst IDs 1/5/15 -> slots 0/1/2 */
	pcp_alloc_handle(a->metrics, KQUEUE_LOADAVG,   0, 1,  "1 minute");
	pcp_alloc_handle(a->metrics, KQUEUE_LOADAVG,   1, 5,  "5 minute");
	pcp_alloc_handle(a->metrics, KQUEUE_LOADAVG,   2, 15, "15 minute");
}

const char *kqueue_metric_names[] = {
	[KQUEUE_RUNNABLE] = "kernel.all.runnable",
	[KQUEUE_PROCESSES] = "kernel.all.nprocs",
	[KQUEUE_BLOCKED] = "kernel.all.blocked",
	[KQUEUE_LOADAVG] = "kernel.all.load",
};
pmDesc kqueue_metric_descs[] = {
	[KQUEUE_RUNNABLE] = {
		.pmid = PMI_ID(60, 2, 2),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_PROCESSES] = {
		.pmid = PMI_ID(60, 2, 3),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_BLOCKED] = {
		.pmid = PMI_ID(60, 0, 16),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_LOADAVG] = {
		.pmid = PMI_ID(60, 2, 0),
		.indom = PMI_INDOM(60, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};
pmID kqueue_metric_pmids[KQUEUE_METRIC_COUNT];

struct act_metrics kqueue_metrics = {
	.count = KQUEUE_METRIC_COUNT,
	.descs = kqueue_metric_descs,
	.names = kqueue_metric_names,
	.pmids = kqueue_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metric instances for disks statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_perdisk_instances(struct activity *a)
{
	int inst = 0;
	struct sa_item *list;

	for (list = a->item_list; list != NULL; list = list->next) {
		act_add_instance(a, DISK_PERDEV_READ, list->item_name, inst++);
	}
}

/*
 ***************************************************************************
 * Probe for disk device names from the first read of buf[0] and populate
 * item_list so that pcp_def_perdisk_instances() can register per-device
 * PCP instances.  Called in the sadc direct-write path where item_list is
 * otherwise NULL (no command-line device filter).
 *
 * Must be called after read_stats() has filled a->buf[0].
 *
 * IN:
 * @a		Activity structure with disk statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_disk_instances(struct activity *a)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;

	if (a->item_list != NULL)
		return;	/* already populated (command-line filter or prior probe) */

	for (i = 0; i < a->_nr0; i++) {
		sdc = (struct stats_disk *)((char *)a->_buf0 + i * a->msize);
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn,
					   sdc->part_nr,
					   DISPLAY_PRETTY(flags),
					   DISPLAY_PERSIST_NAME_S(flags),
					   USE_STABLE_ID(flags), NULL);
		if (!dev_name || !dev_name[0])
			continue;
		/* Skip secondary device classes; they have their own item_lists */
		if (is_dm_device(dev_name) || is_md_device(dev_name) ||
		    is_zram_device(dev_name) || is_part_device(dev_name))
			continue;
		a->item_list_sz += add_list_item(&a->item_list,
						 dev_name,
						 MAX_DEV_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for disks statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_disk_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		/* Create instances for disk.dev (primary class only) */
		pcp_def_perdisk_instances(a);
	}

	act_add_metric(a, DISK_PERDEV_READ);
	act_add_metric(a, DISK_PERDEV_WRITE);
	act_add_metric(a, DISK_PERDEV_TOTAL);
	act_add_metric(a, DISK_PERDEV_TOTALBYTES);
	act_add_metric(a, DISK_PERDEV_READBYTES);
	act_add_metric(a, DISK_PERDEV_WRITEBYTES);
	act_add_metric(a, DISK_PERDEV_DISCARDBYTES);
	act_add_metric(a, DISK_PERDEV_READACTIVE);
	act_add_metric(a, DISK_PERDEV_WRITEACTIVE);
	act_add_metric(a, DISK_PERDEV_TOTALACTIVE);
	act_add_metric(a, DISK_PERDEV_DISCARDACTIVE);
	act_add_metric(a, DISK_PERDEV_AVACTIVE);
	act_add_metric(a, DISK_PERDEV_AVQUEUE);
	act_add_metric(a, DISK_PERDEV_BLKREAD);
	act_add_metric(a, DISK_PERDEV_BLKWRITE);
	act_add_metric(a, DISK_PERDEV_DISCARD);

	pcp_alloc_item_list_handles(a);

	/* Register secondary disk device class metrics */
	pcp_def_disk_class_metrics(&dm_metrics, dm_item_list,
				   DCLASS_CLUSTER_DM, DCLASS_INDOM_DM,
				   "disk.dm");
	pcp_def_disk_class_metrics(&md_metrics, md_item_list,
				   DCLASS_CLUSTER_MD, DCLASS_INDOM_MD,
				   "disk.md");
	pcp_def_disk_class_metrics(&part_metrics, part_item_list,
				   DCLASS_CLUSTER_PART, DCLASS_INDOM_PART,
				   "disk.partitions");
	pcp_def_disk_class_metrics(&zram_metrics, zram_item_list,
				   DCLASS_CLUSTER_ZRAM, DCLASS_INDOM_ZRAM,
				   "zram");
}

const char *disk_metric_names[] = {
	[DISK_PERDEV_READ] = "disk.dev.read",
	[DISK_PERDEV_WRITE] = "disk.dev.write",
	[DISK_PERDEV_TOTAL] = "disk.dev.total",
	[DISK_PERDEV_TOTALBYTES] = "disk.dev.total_bytes",
	[DISK_PERDEV_READBYTES] = "disk.dev.read_bytes",
	[DISK_PERDEV_WRITEBYTES] = "disk.dev.write_bytes",
	[DISK_PERDEV_DISCARDBYTES] = "disk.dev.discard_bytes",
	[DISK_PERDEV_READACTIVE] = "disk.dev.read_rawactive",
	[DISK_PERDEV_WRITEACTIVE] = "disk.dev.write_rawactive",
	[DISK_PERDEV_TOTALACTIVE] = "disk.dev.total_rawactive",
	[DISK_PERDEV_DISCARDACTIVE] = "disk.dev.discard_rawactive",
	[DISK_PERDEV_AVACTIVE] = "disk.dev.avactive",
	[DISK_PERDEV_AVQUEUE] = "disk.dev.aveq",
	[DISK_PERDEV_BLKREAD] = "disk.dev.blkread",
	[DISK_PERDEV_BLKWRITE] = "disk.dev.blkwrite",
	[DISK_PERDEV_DISCARD] = "disk.dev.discard",
};
pmDesc disk_metric_descs[] = {
	[DISK_PERDEV_READ] = {
		.pmid = PMID_DISK_PERDEV_READ,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITE] = {
		.pmid = PMID_DISK_PERDEV_WRITE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTAL] = {
		.pmid = PMID_DISK_PERDEV_TOTAL,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTALBYTES] = {
		.pmid = PMID_DISK_PERDEV_TOTALBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_READBYTES] = {
		.pmid = PMID_DISK_PERDEV_READBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITEBYTES] = {
		.pmid = PMID_DISK_PERDEV_WRITEBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_DISCARDBYTES] = {
		.pmid = PMID_DISK_PERDEV_DISCARDBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_READACTIVE] = {
		.pmid = PMID_DISK_PERDEV_READACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITEACTIVE] = {
		.pmid = PMID_DISK_PERDEV_WRITEACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTALACTIVE] = {
		.pmid = PMID_DISK_PERDEV_TOTALACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_DISCARDACTIVE] = {
		.pmid = PMID_DISK_PERDEV_DISCARDACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_AVACTIVE] = {
		.pmid = PMID_DISK_PERDEV_AVACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_AVQUEUE] = {
		.pmid = PMID_DISK_PERDEV_AVQUEUE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_BLKREAD] = {
		.pmid = PMID_DISK_PERDEV_BLKREAD,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_BLKWRITE] = {
		.pmid = PMID_DISK_PERDEV_BLKWRITE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_DISCARD] = {
		.pmid = PMID_DISK_PERDEV_DISCARD,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID disk_metric_pmids[DISK_METRIC_COUNT];

struct act_metrics disk_metrics = {
	.count = DISK_METRIC_COUNT,
	.descs = disk_metric_descs,
	.names = disk_metric_names,
	.pmids = disk_metric_pmids,
};

/*
 * Static metric name/desc/pmid arrays for secondary disk device classes.
 * Each class (dm, md, partitions, zram) has DCLASS_METRIC_COUNT metrics.
 * The metric name strings are built into per-class char arrays by
 * pcp_def_disk_class_metrics().
 */
static char dm_metric_name_bufs[DCLASS_METRIC_COUNT][64];
static const char *dm_metric_names[DCLASS_METRIC_COUNT];
static pmDesc dm_metric_descs[DCLASS_METRIC_COUNT];
static pmID dm_metric_pmids[DCLASS_METRIC_COUNT];
struct act_metrics dm_metrics = {
	.count = DCLASS_METRIC_COUNT,
	.descs = dm_metric_descs,
	.names = dm_metric_names,
	.pmids = dm_metric_pmids,
};

static char md_metric_name_bufs[DCLASS_METRIC_COUNT][64];
static const char *md_metric_names[DCLASS_METRIC_COUNT];
static pmDesc md_metric_descs[DCLASS_METRIC_COUNT];
static pmID md_metric_pmids[DCLASS_METRIC_COUNT];
struct act_metrics md_metrics = {
	.count = DCLASS_METRIC_COUNT,
	.descs = md_metric_descs,
	.names = md_metric_names,
	.pmids = md_metric_pmids,
};

static char part_metric_name_bufs[DCLASS_METRIC_COUNT][64];
static const char *part_metric_names[DCLASS_METRIC_COUNT];
static pmDesc part_metric_descs[DCLASS_METRIC_COUNT];
static pmID part_metric_pmids[DCLASS_METRIC_COUNT];
struct act_metrics part_metrics = {
	.count = DCLASS_METRIC_COUNT,
	.descs = part_metric_descs,
	.names = part_metric_names,
	.pmids = part_metric_pmids,
};

static char zram_metric_name_bufs[DCLASS_METRIC_COUNT][64];
static const char *zram_metric_names[DCLASS_METRIC_COUNT];
static pmDesc zram_metric_descs[DCLASS_METRIC_COUNT];
static pmID zram_metric_pmids[DCLASS_METRIC_COUNT];
struct act_metrics zram_metrics = {
	.count = DCLASS_METRIC_COUNT,
	.descs = zram_metric_descs,
	.names = zram_metric_names,
	.pmids = zram_metric_pmids,
};

/*
 * Mapping from DCLASS_* enum indices to PCP item numbers within a cluster.
 * These are the same for all four secondary disk clusters.
 */
static const int dclass_items[DCLASS_METRIC_COUNT] = {
	[DCLASS_READ]		= DCLASS_ITEM_READ,
	[DCLASS_WRITE]		= DCLASS_ITEM_WRITE,
	[DCLASS_TOTAL]		= DCLASS_ITEM_TOTAL,
	[DCLASS_READBYTES]	= DCLASS_ITEM_READBYTES,
	[DCLASS_WRITEBYTES]	= DCLASS_ITEM_WRITEBYTES,
	[DCLASS_TOTALBYTES]	= DCLASS_ITEM_TOTALBYTES,
	[DCLASS_DISCBYTES]	= DCLASS_ITEM_DISCBYTES,
	[DCLASS_READ_MERGE]	= DCLASS_ITEM_READ_MERGE,
	[DCLASS_WRITE_MERGE]	= DCLASS_ITEM_WRITE_MERGE,
	[DCLASS_AVACTIVE]	= DCLASS_ITEM_AVACTIVE,
	[DCLASS_AVEQ]		= DCLASS_ITEM_AVEQ,
	[DCLASS_RD_ACTIVE]	= DCLASS_ITEM_RD_ACTIVE,
	[DCLASS_WR_ACTIVE]	= DCLASS_ITEM_WR_ACTIVE,
	[DCLASS_TOTALACTIVE]	= DCLASS_ITEM_TOTALACTIVE,
	[DCLASS_DISCARDACTIVE]	= DCLASS_ITEM_DISCARDACTIVE,
	[DCLASS_BLKREAD]	= DCLASS_ITEM_BLKREAD,
	[DCLASS_BLKWRITE]	= DCLASS_ITEM_BLKWRITE,
	[DCLASS_DISCARD]	= DCLASS_ITEM_DISCARD,
};

/*
 * Human-readable metric name suffixes shared by all secondary disk classes.
 * The full metric name is "<prefix>.<suffix>" (e.g. "disk.dm.read").
 */
static const char * const dclass_suffixes[DCLASS_METRIC_COUNT] = {
	[DCLASS_READ]		= "read",
	[DCLASS_WRITE]		= "write",
	[DCLASS_TOTAL]		= "total",
	[DCLASS_READBYTES]	= "read_bytes",
	[DCLASS_WRITEBYTES]	= "write_bytes",
	[DCLASS_TOTALBYTES]	= "total_bytes",
	[DCLASS_DISCBYTES]	= "discard_bytes",
	[DCLASS_READ_MERGE]	= "read_merge",
	[DCLASS_WRITE_MERGE]	= "write_merge",
	[DCLASS_AVACTIVE]	= "avactive",
	[DCLASS_AVEQ]		= "aveq",
	[DCLASS_RD_ACTIVE]	= "read_rawactive",
	[DCLASS_WR_ACTIVE]	= "write_rawactive",
	[DCLASS_TOTALACTIVE]	= "total_rawactive",
	[DCLASS_DISCARDACTIVE]	= "discard_rawactive",
	[DCLASS_BLKREAD]	= "blkread",
	[DCLASS_BLKWRITE]	= "blkwrite",
	[DCLASS_DISCARD]	= "discard",
};

/*
 ***************************************************************************
 * Probe for dm device instances from buf[0] and populate dm_item_list.
 *
 * IN:
 * @a		Activity structure with disk statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_dm_instances(struct activity *a)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;
	char name[MAX_NAME_LEN];

	if (dm_item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sdc = (struct stats_disk *)((char *)a->_buf0 + i * a->msize);
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn,
					   sdc->part_nr, FALSE, FALSE, FALSE, NULL);
		if (!dev_name || !is_dm_device(dev_name))
			continue;
		dm_persistent_name(dev_name, sdc->major, sdc->minor,
				   name, sizeof(name));
		dm_item_list_sz += add_list_item(&dm_item_list, name,
						 MAX_NAME_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Probe for md device instances from buf[0] and populate md_item_list.
 *
 * IN:
 * @a		Activity structure with disk statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_md_instances(struct activity *a)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;

	if (md_item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sdc = (struct stats_disk *)((char *)a->_buf0 + i * a->msize);
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn,
					   sdc->part_nr, FALSE, FALSE, FALSE, NULL);
		if (!dev_name || !is_md_device(dev_name))
			continue;
		md_item_list_sz += add_list_item(&md_item_list, dev_name,
						 MAX_NAME_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Probe for partition device instances from buf[0] and populate
 * part_item_list.
 *
 * IN:
 * @a		Activity structure with disk statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_part_instances(struct activity *a)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;

	if (part_item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sdc = (struct stats_disk *)((char *)a->_buf0 + i * a->msize);
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn,
					   sdc->part_nr, FALSE, FALSE, FALSE, NULL);
		if (!dev_name || !is_part_device(dev_name))
			continue;
		part_item_list_sz += add_list_item(&part_item_list, dev_name,
						   MAX_NAME_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Probe for zram device instances from buf[0] and populate zram_item_list.
 *
 * IN:
 * @a		Activity structure with disk statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_zram_instances(struct activity *a)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;

	if (zram_item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sdc = (struct stats_disk *)((char *)a->_buf0 + i * a->msize);
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn,
					   sdc->part_nr, FALSE, FALSE, FALSE, NULL);
		if (!dev_name || !is_zram_device(dev_name))
			continue;
		zram_item_list_sz += add_list_item(&zram_item_list, dev_name,
						   MAX_NAME_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Register PCP metric descriptors and instance names for one secondary
 * disk device class (dm, md, partitions, or zram).  Fills in the names[],
 * descs[], and handles[] of the supplied act_metrics struct.
 *
 * Must be called after the class probe (pcp_probe_*_instances) has run
 * and after the PCP archive write context is open.
 *
 * IN:
 * @m		act_metrics struct for this class (dm_metrics, md_metrics, …).
 * @ilist	item_list for this class (dm_item_list, md_item_list, …).
 * @cluster	PCP cluster number for this class.
 * @indom	PCP instance domain for this class.
 * @prefix	Metric name prefix (e.g. "disk.dm", "disk.md").
 ***************************************************************************
 */
void pcp_def_disk_class_metrics(struct act_metrics *m, struct sa_item *ilist,
				int cluster, pmInDom indom,
				const char *prefix)
{
	struct sa_item *list;
	char (*namebuf)[64];
	size_t metric, slot;
	int inst, n_inst;
	int local, saved;
	char *text;

	/*
	 * Point namebuf at the per-class static storage embedded in the
	 * act_metrics names array.  Each class has its own DCLASS_METRIC_COUNT
	 * x 64 char buffer so successive calls do not overwrite each other.
	 * The mapping is: dm -> dm_metric_name_bufs, md -> md_metric_name_bufs,
	 * part -> part_metric_name_bufs, zram -> zram_metric_name_bufs.
	 * We identify the class by which pmids array is in use.
	 */
	if (m->pmids == dm_metric_pmids)
		namebuf = dm_metric_name_bufs;
	else if (m->pmids == md_metric_pmids)
		namebuf = md_metric_name_bufs;
	else if (m->pmids == part_metric_pmids)
		namebuf = part_metric_name_bufs;
	else
		namebuf = zram_metric_name_bufs;

	/* Build metric name strings and descriptors */
	for (metric = 0; metric < DCLASS_METRIC_COUNT; metric++) {
		pmsprintf(namebuf[metric], 64, "%s.%s", prefix, dclass_suffixes[metric]);
		m->names[metric] = namebuf[metric];

		m->descs[metric].pmid  = PMI_ID(60, cluster, dclass_items[metric]);
		m->descs[metric].indom = indom;
		m->descs[metric].sem   = PM_SEM_COUNTER;

		/* time-dimensioned metrics */
		if (metric == DCLASS_AVACTIVE || metric == DCLASS_AVEQ ||
		    metric == DCLASS_RD_ACTIVE || metric == DCLASS_WR_ACTIVE ||
		    metric == DCLASS_TOTALACTIVE || metric == DCLASS_DISCARDACTIVE) {
			m->descs[metric].type  = PM_TYPE_U32;
			m->descs[metric].units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0);
		}
		/* byte-dimensioned metrics */
		else if (metric == DCLASS_READBYTES || metric == DCLASS_WRITEBYTES ||
			 metric == DCLASS_TOTALBYTES || metric == DCLASS_DISCBYTES) {
			m->descs[metric].type  = PM_TYPE_U64;
			m->descs[metric].units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0);
		}
		/* count-dimensioned metrics (default) */
		else {
			m->descs[metric].type  = PM_TYPE_U64;
			m->descs[metric].units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE);
		}

		/* Register the metric in the archive */
		pmiAddMetric(m->names[metric],
			     m->descs[metric].pmid,
			     m->descs[metric].type,
			     m->descs[metric].indom,
			     m->descs[metric].sem,
			     m->descs[metric].units);

		/* Copy help text from the local DSO PMDA if available */
		local = pcp_local_get_ctx();
		if (local >= 0) {
			saved = pmWhichContext();
			pmUseContext(local);
			if (pmLookupText(m->descs[metric].pmid,
					 PM_TEXT_ONELINE, &text) >= 0) {
				pmUseContext(saved);
				pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE,
					   m->descs[metric].pmid, text);
				pmUseContext(local);
				free(text);
			}
			if (pmLookupText(m->descs[metric].pmid,
					 PM_TEXT_HELP, &text) >= 0) {
				pmUseContext(saved);
				pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP,
					   m->descs[metric].pmid, text);
				pmUseContext(local);
				free(text);
			}
			pmUseContext(saved);
		}
	}

	/* Register instances */
	inst = 0;
	for (list = ilist; list != NULL; list = list->next)
		pmiAddInstance(indom, list->item_name, inst++);

	/* Allocate write handles */
	n_inst = 0;
	for (list = ilist; list != NULL; list = list->next)
		n_inst++;
	pcp_alloc_handles(m, (size_t)(n_inst > 0 ? n_inst : 1));
	for (slot = 0, list = ilist; list != NULL; list = list->next, slot++) {
		for (metric = 0; metric < DCLASS_METRIC_COUNT; metric++)
			pcp_alloc_handle(m, metric, slot, (int)slot,
					 list->item_name);
	}
}

/*
 ***************************************************************************
 * Define PCP instances for network interface statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_dev_instances(struct activity *a)
{
	int inst = 0;
	struct sa_item *list;

	for (list = a->item_list; list != NULL; list = list->next) {
		act_add_instance(a, NET_PERINTF_INBYTES, list->item_name, inst++);
	}
}

/*
 ***************************************************************************
 * Probe for network interface names from the first read of buf[0] and
 * populate item_list so that pcp_def_net_dev_instances() can register
 * per-interface PCP instances.  Mirrors pcp_probe_disk_instances().
 *
 * Must be called after read_stats() has filled a->buf[0].
 *
 * IN:
 * @a		Activity structure with net_dev statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_net_dev_instances(struct activity *a)
{
	int i;
	struct stats_net_dev *sndc;

	if (a->item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sndc = (struct stats_net_dev *)((char *)a->_buf0 + i * a->msize);
		if (sndc->interface[0])
			a->item_list_sz += add_list_item(&a->item_list,
							 sndc->interface,
							 MAX_IFACE_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Probe for network interface names from A_NET_EDEV buf[0] and populate
 * item_list.  Uses stats_net_edev which has the interface field at the
 * END of the struct, unlike stats_net_dev.  Mirrors pcp_probe_net_dev_instances().
 *
 * Must be called after read_stats() has filled a->buf[0].
 *
 * IN:
 * @a		Activity structure with net_edev statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_net_edev_instances(struct activity *a)
{
	int i;
	struct stats_net_edev *snedc;

	if (a->item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		snedc = (struct stats_net_edev *)((char *)a->_buf0 + i * a->msize);
		if (snedc->interface[0])
			a->item_list_sz += add_list_item(&a->item_list,
							 snedc->interface,
							 MAX_IFACE_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Probe for filesystem names from buf[0] and populate item_list so that
 * pcp_def_filesystem_instances() can register per-filesystem PCP instances.
 * Mirrors pcp_probe_disk_instances().
 *
 * Must be called after read_stats() has filled a->buf[0].
 *
 * IN:
 * @a		Activity structure with filesystem statistics in buf[0].
 ***************************************************************************
 */
void pcp_probe_filesystem_instances(struct activity *a)
{
	int i;
	struct stats_filesystem *sfc;
	const char *name;

	if (a->item_list != NULL)
		return;

	for (i = 0; i < a->_nr0; i++) {
		sfc = (struct stats_filesystem *)((char *)a->_buf0 + i * a->msize);
		/* Use the persistent name if available, otherwise the block device name */
		name = sfc->fs_name[0] ? sfc->fs_name : sfc->mountp;
		if (name && name[0])
			a->item_list_sz += add_list_item(&a->item_list,
							 (char *)name,
							 MAX_FS_LEN, NULL);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for network interfaces (errors) statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_dev_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		/* Create instances */
		pcp_def_net_dev_instances(a);
	}

	if (a->id == A_NET_DEV) {
		/* Create metrics for A_NET_DEV */
		act_add_metric(a, NET_PERINTF_INPACKETS);
		act_add_metric(a, NET_PERINTF_OUTPACKETS);
		act_add_metric(a, NET_PERINTF_INBYTES);
		act_add_metric(a, NET_PERINTF_OUTBYTES);
		act_add_metric(a, NET_PERINTF_INCOMPRESS);
		act_add_metric(a, NET_PERINTF_OUTCOMPRESS);
		act_add_metric(a, NET_PERINTF_INMULTICAST);
		act_add_metric(a, NET_PERINTF_SPEED);
		act_add_metric(a, NET_PERINTF_BAUDRATE);
		act_add_metric(a, NET_PERINTF_DUPLEX);
	}
	else {
		/* Create metrics for A_NET_EDEV */
		act_add_metric(a, NET_EPERINTF_INERRORS);
		act_add_metric(a, NET_EPERINTF_OUTERRORS);
		act_add_metric(a, NET_EPERINTF_COLLISIONS);
		act_add_metric(a, NET_EPERINTF_INDROPS);
		act_add_metric(a, NET_EPERINTF_OUTDROPS);
		act_add_metric(a, NET_EPERINTF_OUTCARRIER);
		act_add_metric(a, NET_EPERINTF_INFRAME);
		act_add_metric(a, NET_EPERINTF_INFIFO);
		act_add_metric(a, NET_EPERINTF_OUTFIFO);
	}

	pcp_alloc_item_list_handles(a);
}

const char *netdev_metric_names[] = {
	[NET_PERINTF_INPACKETS] = "network.interface.in.packets",
	[NET_PERINTF_OUTPACKETS] = "network.interface.out.packets",
	[NET_PERINTF_INBYTES] = "network.interface.in.bytes",
	[NET_PERINTF_OUTBYTES] = "network.interface.out.bytes",
	[NET_PERINTF_INCOMPRESS] = "network.interface.in.compressed",
	[NET_PERINTF_OUTCOMPRESS] = "network.interface.out.compressed",
	[NET_PERINTF_INMULTICAST] = "network.interface.in.mcasts",
	[NET_PERINTF_SPEED]       = "network.interface.speed",
	[NET_PERINTF_BAUDRATE]    = "network.interface.baudrate",
	[NET_PERINTF_DUPLEX]      = "network.interface.duplex",
};
pmDesc netdev_metric_descs[] = {
	[NET_PERINTF_INPACKETS] = {
		.pmid = PMID_NET_PERINTF_INPACKETS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTPACKETS] = {
		.pmid = PMID_NET_PERINTF_OUTPACKETS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INBYTES] = {
		.pmid = PMID_NET_PERINTF_INBYTES,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTBYTES] = {
		.pmid = PMID_NET_PERINTF_OUTBYTES,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INCOMPRESS] = {
		.pmid = PMID_NET_PERINTF_INCOMPRESS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTCOMPRESS] = {
		.pmid = PMID_NET_PERINTF_OUTCOMPRESS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INMULTICAST] = {
		.pmid = PMID_NET_PERINTF_INMULTICAST,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_SPEED] = {
		.pmid = PMID_NET_PERINTF_SPEED,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, -1, 0, PM_SPACE_MBYTE, PM_TIME_SEC, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_DISCRETE,
	},
	[NET_PERINTF_BAUDRATE] = {
		.pmid = PMID_NET_PERINTF_BAUDRATE,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, -1, 0, PM_SPACE_BYTE, PM_TIME_SEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_DISCRETE,
	},
	[NET_PERINTF_DUPLEX] = {
		.pmid = PMID_NET_PERINTF_DUPLEX,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID netdev_metric_pmids[NET_PERINTF_METRIC_COUNT];

struct act_metrics netdev_metrics = {
	.count = NET_PERINTF_METRIC_COUNT,
	.descs = netdev_metric_descs,
	.names = netdev_metric_names,
	.pmids = netdev_metric_pmids,
};

const char *netedev_metric_names[] = {
	[NET_EPERINTF_INERRORS] = "network.interface.in.errors",
	[NET_EPERINTF_OUTERRORS] = "network.interface.out.errors",
	[NET_EPERINTF_COLLISIONS] = "network.interface.collisions",
	[NET_EPERINTF_INDROPS] = "network.interface.in.drops",
	[NET_EPERINTF_OUTDROPS] = "network.interface.out.drops",
	[NET_EPERINTF_OUTCARRIER] = "network.interface.out.carrier",
	[NET_EPERINTF_INFRAME] = "network.interface.in.frame",
	[NET_EPERINTF_INFIFO] = "network.interface.in.fifo",
	[NET_EPERINTF_OUTFIFO] = "network.interface.out.fifo",
};
pmDesc netedev_metric_descs[] = {
	[NET_EPERINTF_INERRORS] = {
		.pmid = PMID_NET_EPERINTF_INERRORS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTERRORS] = {
		.pmid = PMID_NET_EPERINTF_OUTERRORS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_COLLISIONS] = {
		.pmid = PMID_NET_EPERINTF_COLLISIONS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INDROPS] = {
		.pmid = PMID_NET_EPERINTF_INDROPS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTDROPS] = {
		.pmid = PMID_NET_EPERINTF_OUTDROPS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTCARRIER] = {
		.pmid = PMID_NET_EPERINTF_OUTCARRIER,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INFRAME] = {
		.pmid = PMID_NET_EPERINTF_INFRAME,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INFIFO] = {
		.pmid = PMID_NET_EPERINTF_INFIFO,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTFIFO] = {
		.pmid = PMID_NET_EPERINTF_OUTFIFO,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID netedev_metric_pmids[NET_EPERINTF_METRIC_COUNT];

struct act_metrics netedev_metrics = {
	.count = NET_EPERINTF_METRIC_COUNT,
	.descs = netedev_metric_descs,
	.names = netedev_metric_names,
	.pmids = netedev_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for serial lines statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_serial_metrics(struct activity *a)
{
	int i;
	char buf[64];

	/* Create serial metrics */
	act_add_metric(a, SERIAL_PERTTY_RX);
	act_add_metric(a, SERIAL_PERTTY_TX);
	act_add_metric(a, SERIAL_PERTTY_FRAME);
	act_add_metric(a, SERIAL_PERTTY_PARITY);
	act_add_metric(a, SERIAL_PERTTY_BRK);
	act_add_metric(a, SERIAL_PERTTY_OVERRUN);

	for (i = 0; i < a->nr_ini; i++) {
		/* Create instances */
		pmsprintf(buf, sizeof(buf), "serial%d", i);
		pmiAddInstance(PMI_INDOM(60, 35), buf, i);
	}

	pcp_alloc_item_list_handles(a);
}

const char *serial_metric_names[] = {
	[SERIAL_PERTTY_RX] = "tty.serial.rx",
	[SERIAL_PERTTY_TX] = "tty.serial.tx",
	[SERIAL_PERTTY_FRAME] = "tty.serial.frame",
	[SERIAL_PERTTY_PARITY] = "tty.serial.parity",
	[SERIAL_PERTTY_BRK] = "tty.serial.brk",
	[SERIAL_PERTTY_OVERRUN] = "tty.serial.overrun",
};
pmDesc serial_metric_descs[] = {
	[SERIAL_PERTTY_RX] = {
		.pmid = SERIAL_PERTTY_RX,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_TX] = {
		.pmid = SERIAL_PERTTY_TX,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_FRAME] = {
		.pmid = SERIAL_PERTTY_FRAME,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_PARITY] = {
		.pmid = SERIAL_PERTTY_PARITY,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_BRK] = {
		.pmid = SERIAL_PERTTY_BRK,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_OVERRUN] = {
		.pmid = SERIAL_PERTTY_OVERRUN,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};
pmID serial_metric_pmids[SERIAL_METRIC_COUNT];

struct act_metrics serial_metrics = {
	.count = SERIAL_METRIC_COUNT,
	.descs = serial_metric_descs,
	.names = serial_metric_names,
	.pmids = serial_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for NFS client statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_nfs_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 7);

	pmiAddInstance(indom, "getattr", NFS_REQUEST_GETATTR);
	pmiAddInstance(indom, "read", NFS_REQUEST_READ);
	pmiAddInstance(indom, "write", NFS_REQUEST_WRITE);
	pmiAddInstance(indom, "access", NFS_REQUEST_ACCESS);

	act_add_metric(a, NFSCLIENT_RPCCCNT);
	act_add_metric(a, NFSCLIENT_RPCRETRANS);
	act_add_metric(a, NFSCLIENT_REQUESTS);

	for (size_t _i = 0; _i < NFSCLIENT_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *nfsclient_metric_names[] = {
	[NFSCLIENT_RPCCCNT] = "rpc.client.rpccnt",
	[NFSCLIENT_RPCRETRANS] = "rpc.client.rpcretrans",
	[NFSCLIENT_REQUESTS] = "nfs.client.reqs",
};
pmDesc nfsclient_metric_descs[] = {
	[NFSCLIENT_RPCCCNT] = {
		.pmid = PMID_NFSCLIENT_RPCCCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[NFSCLIENT_RPCRETRANS] = {
		.pmid = PMID_NFSCLIENT_RPCRETRANS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[NFSCLIENT_REQUESTS] = {
		.pmid = PMID_NFSCLIENT_REQUESTS,
		.indom = PMI_INDOM(60, 7),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};
pmID nfsclient_metric_pmids[NFSCLIENT_METRIC_COUNT];

static int nfsclient_handles[NFSCLIENT_METRIC_COUNT];

struct act_metrics nfsclient_metrics = {
	.count = NFSCLIENT_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= nfsclient_handles,
	.descs = nfsclient_metric_descs,
	.names = nfsclient_metric_names,
	.pmids = nfsclient_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for NFS server statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_nfsd_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 7);

	pmiAddInstance(indom, "getattr", NFS_REQUEST_GETATTR);
	pmiAddInstance(indom, "read", NFS_REQUEST_READ);
	pmiAddInstance(indom, "write", NFS_REQUEST_WRITE);
	pmiAddInstance(indom, "access", NFS_REQUEST_ACCESS);

	act_add_metric(a, NFSSERVER_RPCCNT);
	act_add_metric(a, NFSSERVER_RPCBADCLNT);
	act_add_metric(a, NFSSERVER_NETCNT);
	act_add_metric(a, NFSSERVER_NETUDPCNT);
	act_add_metric(a, NFSSERVER_NETTCPCNT);
	act_add_metric(a, NFSSERVER_RCHITS);
	act_add_metric(a, NFSSERVER_RCMISSES);
	act_add_metric(a, NFSSERVER_REQUESTS);

	for (size_t _i = 0; _i < NFSSERVER_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *nfsserver_metric_names[] = {
	[NFSSERVER_RPCCNT] = "rpc.server.rpccnt",
	[NFSSERVER_RPCBADCLNT] = "rpc.server.rpcbadclnt",
	[NFSSERVER_NETCNT] = "rpc.server.netcnt",
	[NFSSERVER_NETUDPCNT] = "rpc.server.netudpcnt",
	[NFSSERVER_NETTCPCNT] = "rpc.server.nettcpcnt",
	[NFSSERVER_RCHITS] = "rpc.server.rchits",
	[NFSSERVER_RCMISSES] = "rpc.server.rcmisses",
	[NFSSERVER_REQUESTS] = "nfs.server.reqs",
};
pmDesc nfsserver_metric_descs[] = {
	[NFSSERVER_RPCCNT] = {
		.pmid = PMID_NFSSERVER_RPCCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RPCBADCLNT] = {
		.pmid = PMID_NFSSERVER_RPCBADCLNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETCNT] = {
		.pmid = PMID_NFSSERVER_NETCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETUDPCNT] = {
		.pmid = PMID_NFSSERVER_NETUDPCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETTCPCNT] = {
		.pmid = PMID_NFSSERVER_NETTCPCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RCHITS] = {
		.pmid = PMID_NFSSERVER_RCHITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RCMISSES] = {
		.pmid = PMID_NFSSERVER_RCMISSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_REQUESTS] = {
		.pmid = PMID_NFSSERVER_REQUESTS,
		.indom = PMI_INDOM(60, 7),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID nfsserver_metric_pmids[NFSSERVER_METRIC_COUNT];

static int nfsserver_handles[NFSSERVER_METRIC_COUNT];

struct act_metrics nfsserver_metrics = {
	.count = NFSSERVER_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= nfsserver_handles,
	.descs = nfsserver_metric_descs,
	.names = nfsserver_metric_names,
	.pmids = nfsserver_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_sock_metrics(struct activity *a)
{
	act_add_metric(a, SOCKET_TOTAL);
	act_add_metric(a, SOCKET_TCPINUSE);
	act_add_metric(a, SOCKET_UDPINUSE);
	act_add_metric(a, SOCKET_RAWINUSE);
	act_add_metric(a, SOCKET_FRAGINUSE);
	act_add_metric(a, SOCKET_TCPTW);

	for (size_t _i = 0; _i < SOCKET_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *socket_metric_names[] = {
	[SOCKET_TOTAL] = "network.sockstat.total",
	[SOCKET_TCPINUSE] = "network.sockstat.tcp.inuse",
	[SOCKET_UDPINUSE] = "network.sockstat.udp.inuse",
	[SOCKET_RAWINUSE] = "network.sockstat.raw.inuse",
	[SOCKET_FRAGINUSE] = "network.sockstat.frag.inuse",
	[SOCKET_TCPTW] = "network.sockstat.tcp.tw",
};
pmDesc socket_metric_descs[] = {
	[SOCKET_TOTAL] = {
		.pmid = PMID_SOCKET_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_TCPINUSE] = {
		.pmid = PMID_SOCKET_TCPINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_UDPINUSE] = {
		.pmid = PMID_SOCKET_UDPINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_RAWINUSE] = {
		.pmid = PMID_SOCKET_RAWINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_FRAGINUSE] = {
		.pmid = PMID_SOCKET_FRAGINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_TCPTW] = {
		.pmid = PMID_SOCKET_TCPTW,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};
pmID socket_metric_pmids[SOCKET_METRIC_COUNT];

static int socket_handles[SOCKET_METRIC_COUNT];

struct act_metrics socket_metrics = {
	.count = SOCKET_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= socket_handles,
	.descs = socket_metric_descs,
	.names = socket_metric_names,
	.pmids = socket_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for IP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_ip_metrics(struct activity *a)
{
	act_add_metric(a, NET_IP_INRECEIVES);
	act_add_metric(a, NET_IP_FORWDATAGRAMS);
	act_add_metric(a, NET_IP_INDELIVERS);
	act_add_metric(a, NET_IP_OUTREQUESTS);
	act_add_metric(a, NET_IP_REASMREQDS);
	act_add_metric(a, NET_IP_REASMOKS);
	act_add_metric(a, NET_IP_FRAGOKS);
	act_add_metric(a, NET_IP_FRAGCREATES);

	for (size_t _i = 0; _i < NET_IP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_ip_metric_names[] = {
	[NET_IP_INRECEIVES] = "network.ip.inreceives",
	[NET_IP_FORWDATAGRAMS] = "network.ip.forwdatagrams",
	[NET_IP_INDELIVERS] = "network.ip.indelivers",
	[NET_IP_OUTREQUESTS] = "network.ip.outrequests",
	[NET_IP_REASMREQDS] = "network.ip.reasmreqds",
	[NET_IP_REASMOKS] = "network.ip.reasmoks",
	[NET_IP_FRAGOKS] = "network.ip.fragoks",
	[NET_IP_FRAGCREATES] = "network.ip.fragcreates",
};
pmDesc net_ip_metric_descs[] = {
	[NET_IP_INRECEIVES] = {
		.pmid = PMID_NET_IP_INRECEIVES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FORWDATAGRAMS] = {
		.pmid = PMID_NET_IP_FORWDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_INDELIVERS] = {
		.pmid = PMID_NET_IP_INDELIVERS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_OUTREQUESTS] = {
		.pmid = PMID_NET_IP_OUTREQUESTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_REASMREQDS] = {
		.pmid = PMID_NET_IP_REASMREQDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_REASMOKS] = {
		.pmid = PMID_NET_IP_REASMOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FRAGOKS] = {
		.pmid = PMID_NET_IP_FRAGOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FRAGCREATES] = {
		.pmid = PMID_NET_IP_FRAGCREATES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_ip_metric_pmids[NET_IP_METRIC_COUNT];

static int net_ip_handles[NET_IP_METRIC_COUNT];

struct act_metrics net_ip_metrics = {
	.count = NET_IP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_ip_handles,
	.descs = net_ip_metric_descs,
	.names = net_ip_metric_names,
	.pmids = net_ip_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for IP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eip_metrics(struct activity *a)
{
	act_add_metric(a, NET_EIP_INHDRERRORS);
	act_add_metric(a, NET_EIP_INADDRERRORS);
	act_add_metric(a, NET_EIP_INUNKNOWNPROTOS);
	act_add_metric(a, NET_EIP_INDISCARDS);
	act_add_metric(a, NET_EIP_OUTDISCARDS);
	act_add_metric(a, NET_EIP_OUTNOROUTES);
	act_add_metric(a, NET_EIP_REASMFAILS);
	act_add_metric(a, NET_EIP_FRAGFAILS);

	for (size_t _i = 0; _i < NET_EIP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_eip_metric_names[] = {
	[NET_EIP_INHDRERRORS] = "network.ip.inhdrerrors",
	[NET_EIP_INADDRERRORS] = "network.ip.inaddrerrors",
	[NET_EIP_INUNKNOWNPROTOS] = "network.ip.inunknownprotos",
	[NET_EIP_INDISCARDS] = "network.ip.indiscards",
	[NET_EIP_OUTDISCARDS] = "network.ip.outdiscards",
	[NET_EIP_OUTNOROUTES] = "network.ip.outnoroutes",
	[NET_EIP_REASMFAILS] = "network.ip.reasmfails",
	[NET_EIP_FRAGFAILS] = "network.ip.fragfails",
};
pmDesc net_eip_metric_descs[] = {
	[NET_EIP_INHDRERRORS] = {
		.pmid = PMID_NET_EIP_INHDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INADDRERRORS] = {
		.pmid = PMID_NET_EIP_INADDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INUNKNOWNPROTOS] = {
		.pmid = PMID_NET_EIP_INUNKNOWNPROTOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INDISCARDS] = {
		.pmid = PMID_NET_EIP_INDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_OUTDISCARDS] = {
		.pmid = PMID_NET_EIP_OUTDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_OUTNOROUTES] = {
		.pmid = PMID_NET_EIP_OUTNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_REASMFAILS] = {
		.pmid = PMID_NET_EIP_REASMFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_FRAGFAILS] = {
		.pmid = PMID_NET_EIP_FRAGFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_eip_metric_pmids[NET_EIP_METRIC_COUNT];

static int net_eip_handles[NET_EIP_METRIC_COUNT];

struct act_metrics net_eip_metrics = {
	.count = NET_EIP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_eip_handles,
	.descs = net_eip_metric_descs,
	.names = net_eip_metric_names,
	.pmids = net_eip_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp_metrics(struct activity *a)
{
	act_add_metric(a, NET_ICMP_INMSGS);
	act_add_metric(a, NET_ICMP_OUTMSGS);
	act_add_metric(a, NET_ICMP_INECHOS);
	act_add_metric(a, NET_ICMP_INECHOREPS);
	act_add_metric(a, NET_ICMP_OUTECHOS);
	act_add_metric(a, NET_ICMP_OUTECHOREPS);
	act_add_metric(a, NET_ICMP_INTIMESTAMPS);
	act_add_metric(a, NET_ICMP_INTIMESTAMPREPS);
	act_add_metric(a, NET_ICMP_OUTTIMESTAMPS);
	act_add_metric(a, NET_ICMP_OUTTIMESTAMPREPS);
	act_add_metric(a, NET_ICMP_INADDRMASKS);
	act_add_metric(a, NET_ICMP_INADDRMASKREPS);
	act_add_metric(a, NET_ICMP_OUTADDRMASKS);
	act_add_metric(a, NET_ICMP_OUTADDRMASKREPS);

	for (size_t _i = 0; _i < NET_ICMP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_icmp_metric_names[] = {
	[NET_ICMP_INMSGS] = "network.icmp.inmsgs",
	[NET_ICMP_OUTMSGS] = "network.icmp.outmsgs",
	[NET_ICMP_INECHOS] = "network.icmp.inechos",
	[NET_ICMP_INECHOREPS] = "network.icmp.inechoreps",
	[NET_ICMP_OUTECHOS] = "network.icmp.outechos",
	[NET_ICMP_OUTECHOREPS] = "network.icmp.outechoreps",
	[NET_ICMP_INTIMESTAMPS] = "network.icmp.intimestamps",
	[NET_ICMP_INTIMESTAMPREPS] = "network.icmp.intimestampreps",
	[NET_ICMP_OUTTIMESTAMPS] = "network.icmp.outtimestamps",
	[NET_ICMP_OUTTIMESTAMPREPS] = "network.icmp.outtimestampreps",
	[NET_ICMP_INADDRMASKS] = "network.icmp.inaddrmasks",
	[NET_ICMP_INADDRMASKREPS] = "network.icmp.inaddrmaskreps",
	[NET_ICMP_OUTADDRMASKS] = "network.icmp.outaddrmasks",
	[NET_ICMP_OUTADDRMASKREPS] = "network.icmp.outaddrmaskreps",
};
pmDesc net_icmp_metric_descs[] = {
	[NET_ICMP_INMSGS] = {
		.pmid = PMID_NET_ICMP_INMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTMSGS] = {
		.pmid = PMID_NET_ICMP_OUTMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INECHOS] = {
		.pmid = PMID_NET_ICMP_INECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INECHOREPS] = {
		.pmid = PMID_NET_ICMP_INECHOREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTECHOS] = {
		.pmid = PMID_NET_ICMP_OUTECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTECHOREPS] = {
		.pmid = PMID_NET_ICMP_OUTECHOREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INTIMESTAMPS] = {
		.pmid = PMID_NET_ICMP_INTIMESTAMPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INTIMESTAMPREPS] = {
		.pmid = PMID_NET_ICMP_INTIMESTAMPREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTTIMESTAMPS] = {
		.pmid = PMID_NET_ICMP_OUTTIMESTAMPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTTIMESTAMPREPS] = {
		.pmid = PMID_NET_ICMP_OUTTIMESTAMPREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INADDRMASKS] = {
		.pmid = PMID_NET_ICMP_INADDRMASKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INADDRMASKREPS] = {
		.pmid = PMID_NET_ICMP_INADDRMASKREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTADDRMASKS] = {
		.pmid = PMID_NET_ICMP_OUTADDRMASKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTADDRMASKREPS] = {
		.pmid = PMID_NET_ICMP_OUTADDRMASKREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_icmp_metric_pmids[NET_ICMP_METRIC_COUNT];

static int net_icmp_handles[NET_ICMP_METRIC_COUNT];

struct act_metrics net_icmp_metrics = {
	.count = NET_ICMP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_icmp_handles,
	.descs = net_icmp_metric_descs,
	.names = net_icmp_metric_names,
	.pmids = net_icmp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp_metrics(struct activity *a)
{
	act_add_metric(a, NET_EICMP_INERRORS);
	act_add_metric(a, NET_EICMP_OUTERRORS);
	act_add_metric(a, NET_EICMP_INDESTUNREACHS);
	act_add_metric(a, NET_EICMP_OUTDESTUNREACHS);
	act_add_metric(a, NET_EICMP_INTIMEEXCDS);
	act_add_metric(a, NET_EICMP_OUTTIMEEXCDS);
	act_add_metric(a, NET_EICMP_INPARMPROBS);
	act_add_metric(a, NET_EICMP_OUTPARMPROBS);
	act_add_metric(a, NET_EICMP_INSRCQUENCHS);
	act_add_metric(a, NET_EICMP_OUTSRCQUENCHS);
	act_add_metric(a, NET_EICMP_INREDIRECTS);
	act_add_metric(a, NET_EICMP_OUTREDIRECTS);

	for (size_t _i = 0; _i < NET_EICMP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_eicmp_metric_names[] = {
	[NET_EICMP_INERRORS] = "network.icmp.inerrors",
	[NET_EICMP_OUTERRORS] = "network.icmp.outerrors",
	[NET_EICMP_INDESTUNREACHS] = "network.icmp.indestunreachs",
	[NET_EICMP_OUTDESTUNREACHS] = "network.icmp.outdestunreachs",
	[NET_EICMP_INTIMEEXCDS] = "network.icmp.intimeexcds",
	[NET_EICMP_OUTTIMEEXCDS] = "network.icmp.outtimeexcds",
	[NET_EICMP_INPARMPROBS] = "network.icmp.inparmprobs",
	[NET_EICMP_OUTPARMPROBS] = "network.icmp.outparmprobs",
	[NET_EICMP_INSRCQUENCHS] = "network.icmp.insrcquenchs",
	[NET_EICMP_OUTSRCQUENCHS] = "network.icmp.outsrcquenchs",
	[NET_EICMP_INREDIRECTS] = "network.icmp.inredirects",
	[NET_EICMP_OUTREDIRECTS] = "network.icmp.outredirects",
};
pmDesc net_eicmp_metric_descs[] = {
	[NET_EICMP_INERRORS] = {
		.pmid = PMID_NET_EICMP_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTERRORS] = {
		.pmid = PMID_NET_EICMP_OUTERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP_INDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP_OUTDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP_INTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP_OUTTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INPARMPROBS] = {
		.pmid = PMID_NET_EICMP_INPARMPROBS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTPARMPROBS] = {
		.pmid = PMID_NET_EICMP_OUTPARMPROBS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INSRCQUENCHS] = {
		.pmid = PMID_NET_EICMP_INSRCQUENCHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTSRCQUENCHS] = {
		.pmid = PMID_NET_EICMP_OUTSRCQUENCHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INREDIRECTS] = {
		.pmid = PMID_NET_EICMP_INREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTREDIRECTS] = {
		.pmid = PMID_NET_EICMP_OUTREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_eicmp_metric_pmids[NET_EICMP_METRIC_COUNT];

static int net_eicmp_handles[NET_EICMP_METRIC_COUNT];

struct act_metrics net_eicmp_metrics = {
	.count = NET_EICMP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_eicmp_handles,
	.descs = net_eicmp_metric_descs,
	.names = net_eicmp_metric_names,
	.pmids = net_eicmp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for TCP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_tcp_metrics(struct activity *a)
{
	act_add_metric(a, NET_TCP_ACTIVEOPENS);
	act_add_metric(a, NET_TCP_PASSIVEOPENS);
	act_add_metric(a, NET_TCP_INSEGS);
	act_add_metric(a, NET_TCP_OUTSEGS);

	for (size_t _i = 0; _i < NET_TCP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_tcp_metric_names[] = {
	[NET_TCP_ACTIVEOPENS] = "network.tcp.activeopens",
	[NET_TCP_PASSIVEOPENS] = "network.tcp.passiveopens",
	[NET_TCP_INSEGS] = "network.tcp.insegs",
	[NET_TCP_OUTSEGS] = "network.tcp.outsegs",
};
pmDesc net_tcp_metric_descs[] = {
	[NET_TCP_ACTIVEOPENS] = {
		.pmid = PMID_NET_TCP_ACTIVEOPENS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_PASSIVEOPENS] = {
		.pmid = PMID_NET_TCP_PASSIVEOPENS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_INSEGS] = {
		.pmid = PMID_NET_TCP_INSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_OUTSEGS] = {
		.pmid = PMID_NET_TCP_OUTSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_tcp_metric_pmids[NET_TCP_METRIC_COUNT];

static int net_tcp_handles[NET_TCP_METRIC_COUNT];

struct act_metrics net_tcp_metrics = {
	.count = NET_TCP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_tcp_handles,
	.descs = net_tcp_metric_descs,
	.names = net_tcp_metric_names,
	.pmids = net_tcp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for TCP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_etcp_metrics(struct activity *a)
{
	act_add_metric(a, NET_ETCP_ATTEMPTFAILS);
	act_add_metric(a, NET_ETCP_ESTABRESETS);
	act_add_metric(a, NET_ETCP_RETRANSSEGS);
	act_add_metric(a, NET_ETCP_INERRS);
	act_add_metric(a, NET_ETCP_OUTRSTS);

	for (size_t _i = 0; _i < NET_ETCP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_etcp_metric_names[] = {
	[NET_ETCP_ATTEMPTFAILS] = "network.tcp.attemptfails",
	[NET_ETCP_ESTABRESETS] = "network.tcp.estabresets",
	[NET_ETCP_RETRANSSEGS] = "network.tcp.retranssegs",
	[NET_ETCP_INERRS] = "network.tcp.inerrs",
	[NET_ETCP_OUTRSTS] = "network.tcp.outrsts",
};
pmDesc net_etcp_metric_descs[] = {
	[NET_ETCP_ATTEMPTFAILS] = {
		.pmid = PMID_NET_ETCP_ATTEMPTFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_ESTABRESETS] = {
		.pmid = PMID_NET_ETCP_ESTABRESETS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_RETRANSSEGS] = {
		.pmid = PMID_NET_ETCP_RETRANSSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_INERRS] = {
		.pmid = PMID_NET_ETCP_INERRS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_OUTRSTS] = {
		.pmid = PMID_NET_ETCP_OUTRSTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_etcp_metric_pmids[NET_ETCP_METRIC_COUNT];

static int net_etcp_handles[NET_ETCP_METRIC_COUNT];

struct act_metrics net_etcp_metrics = {
	.count = NET_ETCP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_etcp_handles,
	.descs = net_etcp_metric_descs,
	.names = net_etcp_metric_names,
	.pmids = net_etcp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for UDP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_udp_metrics(struct activity *a)
{
	act_add_metric(a, NET_UDP_INDATAGRAMS);
	act_add_metric(a, NET_UDP_OUTDATAGRAMS);
	act_add_metric(a, NET_UDP_NOPORTS);
	act_add_metric(a, NET_UDP_INERRORS);

	for (size_t _i = 0; _i < NET_UDP_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_udp_metric_names[] = {
	[NET_UDP_INDATAGRAMS] = "network.udp.indatagrams",
	[NET_UDP_OUTDATAGRAMS] = "network.udp.outdatagrams",
	[NET_UDP_NOPORTS] = "network.udp.noports",
	[NET_UDP_INERRORS] = "network.udp.inerrors",
};
pmDesc net_udp_metric_descs[] = {
	[NET_UDP_INDATAGRAMS] = {
		.pmid = PMID_NET_UDP_INDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_OUTDATAGRAMS] = {
		.pmid = PMID_NET_UDP_OUTDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_NOPORTS] = {
		.pmid = PMID_NET_UDP_NOPORTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_INERRORS] = {
		.pmid = PMID_NET_UDP_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_udp_metric_pmids[NET_UDP_METRIC_COUNT];

static int net_udp_handles[NET_UDP_METRIC_COUNT];

struct act_metrics net_udp_metrics = {
	.count = NET_UDP_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_udp_handles,
	.descs = net_udp_metric_descs,
	.names = net_udp_metric_names,
	.pmids = net_udp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_sock6_metrics(struct activity *a)
{
	act_add_metric(a, NET_SOCK6_TCPINUSE);
	act_add_metric(a, NET_SOCK6_UDPINUSE);
	act_add_metric(a, NET_SOCK6_RAWINUSE);
	act_add_metric(a, NET_SOCK6_FRAGINUSE);

	for (size_t _i = 0; _i < NET_SOCK6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_sock6_metric_names[] = {
	[NET_SOCK6_TCPINUSE] = "network.sockstat.tcp6.inuse",
	[NET_SOCK6_UDPINUSE] = "network.sockstat.udp6.inuse",
	[NET_SOCK6_RAWINUSE] = "network.sockstat.raw6.inuse",
	[NET_SOCK6_FRAGINUSE] = "network.sockstat.frag6.inuse",
};
pmDesc net_sock6_metric_descs[] = {
	[NET_SOCK6_TCPINUSE] = {
		.pmid = PMI_ID(60, 73, 0),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_UDPINUSE] = {
		.pmid = PMI_ID(60, 73, 1),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_RAWINUSE] = {
		.pmid = PMI_ID(60, 73, 3),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_FRAGINUSE] = {
		.pmid = PMI_ID(60, 73, 4),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};
pmID net_sock6_metric_pmids[NET_SOCK6_METRIC_COUNT];

static int net_sock6_handles[NET_SOCK6_METRIC_COUNT];

struct act_metrics net_sock6_metrics = {
	.count = NET_SOCK6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_sock6_handles,
	.descs = net_sock6_metric_descs,
	.names = net_sock6_metric_names,
	.pmids = net_sock6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_ip6_metrics(struct activity *a)
{
	act_add_metric(a, NET_IP6_INRECEIVES);
	act_add_metric(a, NET_IP6_OUTFORWDATAGRAMS);
	act_add_metric(a, NET_IP6_INDELIVERS);
	act_add_metric(a, NET_IP6_OUTREQUESTS);
	act_add_metric(a, NET_IP6_REASMREQDS);
	act_add_metric(a, NET_IP6_REASMOKS);
	act_add_metric(a, NET_IP6_INMCASTPKTS);
	act_add_metric(a, NET_IP6_OUTMCASTPKTS);
	act_add_metric(a, NET_IP6_FRAGOKS);
	act_add_metric(a, NET_IP6_FRAGCREATES);

	for (size_t _i = 0; _i < NET_IP6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_ip6_metric_names[] = {
	[NET_IP6_INRECEIVES] = "network.ip6.inreceives",
	[NET_IP6_OUTFORWDATAGRAMS] = "network.ip6.outforwdatagrams",
	[NET_IP6_INDELIVERS] = "network.ip6.indelivers",
	[NET_IP6_OUTREQUESTS] = "network.ip6.outrequests",
	[NET_IP6_REASMREQDS] = "network.ip6.reasmreqds",
	[NET_IP6_REASMOKS] = "network.ip6.reasmoks",
	[NET_IP6_INMCASTPKTS] = "network.ip6.inmcastpkts",
	[NET_IP6_OUTMCASTPKTS] = "network.ip6.outmcastpkts",
	[NET_IP6_FRAGOKS] = "network.ip6.fragoks",
	[NET_IP6_FRAGCREATES] = "network.ip6.fragcreates",
};
pmDesc net_ip6_metric_descs[] = {
	[NET_IP6_INRECEIVES] = {
		.pmid = PMID_NET_IP6_INRECEIVES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTFORWDATAGRAMS] = {
		.pmid = PMID_NET_IP6_OUTFORWDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_INDELIVERS] = {
		.pmid = PMID_NET_IP6_INDELIVERS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTREQUESTS] = {
		.pmid = PMID_NET_IP6_OUTREQUESTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_REASMREQDS] = {
		.pmid = PMID_NET_IP6_REASMREQDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_REASMOKS] = {
		.pmid = PMID_NET_IP6_REASMOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_INMCASTPKTS] = {
		.pmid = PMID_NET_IP6_INMCASTPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTMCASTPKTS] = {
		.pmid = PMID_NET_IP6_OUTMCASTPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_FRAGOKS] = {
		.pmid = PMID_NET_IP6_FRAGOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_FRAGCREATES] = {
		.pmid = PMID_NET_IP6_FRAGCREATES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_ip6_metric_pmids[NET_IP6_METRIC_COUNT];

static int net_ip6_handles[NET_IP6_METRIC_COUNT];

struct act_metrics net_ip6_metrics = {
	.count = NET_IP6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_ip6_handles,
	.descs = net_ip6_metric_descs,
	.names = net_ip6_metric_names,
	.pmids = net_ip6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eip6_metrics(struct activity *a)
{
	act_add_metric(a, NET_EIP6_INHDRERRORS);
	act_add_metric(a, NET_EIP6_INADDRERRORS);
	act_add_metric(a, NET_EIP6_INUNKNOWNPROTOS);
	act_add_metric(a, NET_EIP6_INTOOBIGERRORS);
	act_add_metric(a, NET_EIP6_INDISCARDS);
	act_add_metric(a, NET_EIP6_OUTDISCARDS);
	act_add_metric(a, NET_EIP6_INNOROUTES);
	act_add_metric(a, NET_EIP6_OUTNOROUTES);
	act_add_metric(a, NET_EIP6_REASMFAILS);
	act_add_metric(a, NET_EIP6_FRAGFAILS);
	act_add_metric(a, NET_EIP6_INTRUNCATEDPKTS );

	for (size_t _i = 0; _i < NET_EIP6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_eip6_metric_names[] = {
	[NET_EIP6_INHDRERRORS] = "network.ip6.inhdrerrors",
	[NET_EIP6_INADDRERRORS] = "network.ip6.inaddrerrors",
	[NET_EIP6_INUNKNOWNPROTOS] = "network.ip6.inunknownprotos",
	[NET_EIP6_INTOOBIGERRORS] = "network.ip6.intoobigerrors",
	[NET_EIP6_INDISCARDS] = "network.ip6.indiscards",
	[NET_EIP6_OUTDISCARDS] = "network.ip6.outdiscards",
	[NET_EIP6_INNOROUTES] = "network.ip6.innoroutes",
	[NET_EIP6_OUTNOROUTES] = "network.ip6.outnoroutes",
	[NET_EIP6_REASMFAILS] = "network.ip6.reasmfails",
	[NET_EIP6_FRAGFAILS] = "network.ip6.fragfails",
	[NET_EIP6_INTRUNCATEDPKTS] = "network.ip6.intruncatedpkts",
};
pmDesc net_eip6_metric_descs[] = {
	[NET_EIP6_INHDRERRORS] = {
		.pmid = PMID_NET_EIP6_INHDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INADDRERRORS] = {
		.pmid = PMID_NET_EIP6_INADDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INUNKNOWNPROTOS] = {
		.pmid = PMID_NET_EIP6_INUNKNOWNPROTOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INTOOBIGERRORS] = {
		.pmid = PMID_NET_EIP6_INTOOBIGERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INDISCARDS] = {
		.pmid = PMID_NET_EIP6_INDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_OUTDISCARDS] = {
		.pmid = PMID_NET_EIP6_OUTDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INNOROUTES] = {
		.pmid = PMID_NET_EIP6_INNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_OUTNOROUTES] = {
		.pmid = PMID_NET_EIP6_OUTNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_REASMFAILS] = {
		.pmid = PMID_NET_EIP6_REASMFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_FRAGFAILS] = {
		.pmid = PMID_NET_EIP6_FRAGFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INTRUNCATEDPKTS] = {
		.pmid = PMID_NET_EIP6_INTRUNCATEDPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_eip6_metric_pmids[NET_EIP6_METRIC_COUNT];

static int net_eip6_handles[NET_EIP6_METRIC_COUNT];

struct act_metrics net_eip6_metrics = {
	.count = NET_EIP6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_eip6_handles,
	.descs = net_eip6_metric_descs,
	.names = net_eip6_metric_names,
	.pmids = net_eip6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_ICMP6_INMSGS);
	act_add_metric(a, NET_ICMP6_OUTMSGS);
	act_add_metric(a, NET_ICMP6_INECHOS);
	act_add_metric(a, NET_ICMP6_INECHOREPLIES);
	act_add_metric(a, NET_ICMP6_OUTECHOREPLIES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBQUERIES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBRESPONSES);
	act_add_metric(a, NET_ICMP6_OUTGROUPMEMBRESPONSES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBREDUCTIONS);
	act_add_metric(a, NET_ICMP6_OUTGROUPMEMBREDUCTIONS);
	act_add_metric(a, NET_ICMP6_INROUTERSOLICITS);
	act_add_metric(a, NET_ICMP6_OUTROUTERSOLICITS);
	act_add_metric(a, NET_ICMP6_INROUTERADVERTISEMENTS);
	act_add_metric(a, NET_ICMP6_INNEIGHBORSOLICITS);
	act_add_metric(a, NET_ICMP6_OUTNEIGHBORSOLICITS);
	act_add_metric(a, NET_ICMP6_INNEIGHBORADVERTISEMENTS);
	act_add_metric(a, NET_ICMP6_OUTNEIGHBORADVERTISEMENTS);

	for (size_t _i = 0; _i < NET_ICMP6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_icmp6_metric_names[] = {
	[NET_ICMP6_INMSGS] = "network.icmp6.inmsgs",
	[NET_ICMP6_OUTMSGS] = "network.icmp6.outmsgs",
	[NET_ICMP6_INECHOS] = "network.icmp6.inechos",
	[NET_ICMP6_INECHOREPLIES] = "network.icmp6.inechoreplies",
	[NET_ICMP6_OUTECHOREPLIES] = "network.icmp6.outechoreplies",
	[NET_ICMP6_INGROUPMEMBQUERIES] = "network.icmp6.ingroupmembqueries",
	[NET_ICMP6_INGROUPMEMBRESPONSES] = "network.icmp6.ingroupmembresponses",
	[NET_ICMP6_OUTGROUPMEMBRESPONSES] = "network.icmp6.outgroupmembresponses",
	[NET_ICMP6_INGROUPMEMBREDUCTIONS] = "network.icmp6.ingroupmembreductions",
	[NET_ICMP6_OUTGROUPMEMBREDUCTIONS] = "network.icmp6.outgroupmembreductions",
	[NET_ICMP6_INROUTERSOLICITS] = "network.icmp6.inroutersolicits",
	[NET_ICMP6_OUTROUTERSOLICITS] = "network.icmp6.outroutersolicits",
	[NET_ICMP6_INROUTERADVERTISEMENTS] = "network.icmp6.inrouteradvertisements",
	[NET_ICMP6_INNEIGHBORSOLICITS] = "network.icmp6.inneighborsolicits",
	[NET_ICMP6_OUTNEIGHBORSOLICITS] = "network.icmp6.outneighborsolicits",
	[NET_ICMP6_INNEIGHBORADVERTISEMENTS] = "network.icmp6.inneighboradvertisements",
	[NET_ICMP6_OUTNEIGHBORADVERTISEMENTS] = "network.icmp6.outneighboradvertisements",
};
pmDesc net_icmp6_metric_descs[] = {
	[NET_ICMP6_INMSGS] = {
		.pmid = PMID_NET_ICMP6_INMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTMSGS] = {
		.pmid = PMID_NET_ICMP6_OUTMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INECHOS] = {
		.pmid = PMID_NET_ICMP6_INECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INECHOREPLIES] = {
		.pmid = PMID_NET_ICMP6_INECHOREPLIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTECHOREPLIES] = {
		.pmid = PMID_NET_ICMP6_OUTECHOREPLIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBQUERIES] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBQUERIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBRESPONSES] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBRESPONSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTGROUPMEMBRESPONSES] = {
		.pmid = PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBREDUCTIONS] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTGROUPMEMBREDUCTIONS] = {
		.pmid = PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INROUTERSOLICITS] = {
		.pmid = PMID_NET_ICMP6_INROUTERSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTROUTERSOLICITS] = {
		.pmid = PMID_NET_ICMP6_OUTROUTERSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INROUTERADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_INROUTERADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INNEIGHBORSOLICITS] = {
		.pmid = PMID_NET_ICMP6_INNEIGHBORSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTNEIGHBORSOLICITS] = {
		.pmid = PMID_NET_ICMP6_OUTNEIGHBORSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INNEIGHBORADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTNEIGHBORADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_icmp6_metric_pmids[NET_ICMP6_METRIC_COUNT];

static int net_icmp6_handles[NET_ICMP6_METRIC_COUNT];

struct act_metrics net_icmp6_metrics = {
	.count = NET_ICMP6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_icmp6_handles,
	.descs = net_icmp6_metric_descs,
	.names = net_icmp6_metric_names,
	.pmids = net_icmp6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_EICMP6_INERRORS);
	act_add_metric(a, NET_EICMP6_INDESTUNREACHS);
	act_add_metric(a, NET_EICMP6_OUTDESTUNREACHS);
	act_add_metric(a, NET_EICMP6_INTIMEEXCDS);
	act_add_metric(a, NET_EICMP6_OUTTIMEEXCDS);
	act_add_metric(a, NET_EICMP6_INPARMPROBLEMS);
	act_add_metric(a, NET_EICMP6_OUTPARMPROBLEMS);
	act_add_metric(a, NET_EICMP6_INREDIRECTS);
	act_add_metric(a, NET_EICMP6_OUTREDIRECTS);
	act_add_metric(a, NET_EICMP6_INPKTTOOBIGS);
	act_add_metric(a, NET_EICMP6_OUTPKTTOOBIGS);

	for (size_t _i = 0; _i < NET_EICMP6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_eicmp6_metric_names[] = {
	[NET_EICMP6_INERRORS] = "network.icmp6.inerrors",
	[NET_EICMP6_INDESTUNREACHS] = "network.icmp6.indestunreachs",
	[NET_EICMP6_OUTDESTUNREACHS] = "network.icmp6.outdestunreachs",
	[NET_EICMP6_OUTTIMEEXCDS] = "network.icmp6.outtimeexcds",
	[NET_EICMP6_INPARMPROBLEMS] = "network.icmp6.inparmproblems",
	[NET_EICMP6_OUTPARMPROBLEMS] = "network.icmp6.outparmproblems",
	[NET_EICMP6_INTIMEEXCDS] = "network.icmp6.intimeexcds",
	[NET_EICMP6_INREDIRECTS] = "network.icmp6.inredirects",
	[NET_EICMP6_OUTREDIRECTS] = "network.icmp6.outredirects",
	[NET_EICMP6_INPKTTOOBIGS] = "network.icmp6.inpkttoobigs",
	[NET_EICMP6_OUTPKTTOOBIGS] = "network.icmp6.outpkttoobigs",
};
pmDesc net_eicmp6_metric_descs[] = {
	[NET_EICMP6_INERRORS] = {
		.pmid = PMID_NET_EICMP6_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP6_INDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP6_OUTDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP6_INTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP6_OUTTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INPARMPROBLEMS] = {
		.pmid = PMID_NET_EICMP6_INPARMPROBLEMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTPARMPROBLEMS] = {
		.pmid = PMID_NET_EICMP6_OUTPARMPROBLEMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INREDIRECTS] = {
		.pmid = PMID_NET_EICMP6_INREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTREDIRECTS] = {
		.pmid = PMID_NET_EICMP6_OUTREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INPKTTOOBIGS] = {
		.pmid = PMID_NET_EICMP6_INPKTTOOBIGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTPKTTOOBIGS] = {
		.pmid = PMID_NET_EICMP6_OUTPKTTOOBIGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_eicmp6_metric_pmids[NET_EICMP6_METRIC_COUNT];

static int net_eicmp6_handles[NET_EICMP6_METRIC_COUNT];

struct act_metrics net_eicmp6_metrics = {
	.count = NET_EICMP6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_eicmp6_handles,
	.descs = net_eicmp6_metric_descs,
	.names = net_eicmp6_metric_names,
	.pmids = net_eicmp6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for UDPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_udp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_UDP6_INDATAGRAMS);
	act_add_metric(a, NET_UDP6_OUTDATAGRAMS);
	act_add_metric(a, NET_UDP6_NOPORTS);
	act_add_metric(a, NET_UDP6_INERRORS);

	for (size_t _i = 0; _i < NET_UDP6_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *net_udp6_metric_names[] = {
	[NET_UDP6_INDATAGRAMS] = "network.udp6.indatagrams",
	[NET_UDP6_OUTDATAGRAMS] = "network.udp6.outdatagrams",
	[NET_UDP6_NOPORTS] = "network.udp6.noports",
	[NET_UDP6_INERRORS] = "network.udp6.inerrors",
};
pmDesc net_udp6_metric_descs[] = {
	[NET_UDP6_INDATAGRAMS] = {
		.pmid = PMID_NET_UDP6_INDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_OUTDATAGRAMS] = {
		.pmid = PMID_NET_UDP6_OUTDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_NOPORTS] = {
		.pmid = PMID_NET_UDP6_NOPORTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_INERRORS] = {
		.pmid = PMID_NET_UDP6_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID net_udp6_metric_pmids[NET_UDP6_METRIC_COUNT];

static int net_udp6_handles[NET_UDP6_METRIC_COUNT];

struct act_metrics net_udp6_metrics = {
	.count = NET_UDP6_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= net_udp6_handles,
	.descs = net_udp6_metric_descs,
	.names = net_udp6_metric_names,
	.pmids = net_udp6_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metrics for huge pages statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_huge_metrics(struct activity *a)
{
	act_add_metric(a, MEM_HUGE_TOTALBYTES);
	act_add_metric(a, MEM_HUGE_FREEBYTES);
	act_add_metric(a, MEM_HUGE_RSVDBYTES);
	act_add_metric(a, MEM_HUGE_SURPBYTES);

	for (size_t _i = 0; _i < MEM_HUGE_METRIC_COUNT; _i++)
		pcp_alloc_handle(a->metrics, _i, 0, PM_IN_NULL, NULL);
}

const char *mem_huge_metric_names[] = {
	[MEM_HUGE_TOTALBYTES] = "mem.util.hugepagesTotalBytes",
	[MEM_HUGE_FREEBYTES] = "mem.util.hugepagesFreeBytes",
	[MEM_HUGE_RSVDBYTES] = "mem.util.hugepagesRsvdBytes",
	[MEM_HUGE_SURPBYTES] = "mem.util.hugepagesSurpBytes",
};
pmDesc mem_huge_metric_descs[] = {
	[MEM_HUGE_TOTALBYTES] = {
		.pmid = PMID_MEM_HUGE_TOTALBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_FREEBYTES] = {
		.pmid = PMID_MEM_HUGE_FREEBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_RSVDBYTES] = {
		.pmid = PMID_MEM_HUGE_RSVDBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_SURPBYTES] = {
		.pmid = PMID_MEM_HUGE_SURPBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};
pmID mem_huge_metric_pmids[MEM_HUGE_METRIC_COUNT];

static int mem_huge_handles[MEM_HUGE_METRIC_COUNT];

struct act_metrics mem_huge_metrics = {
	.count = MEM_HUGE_METRIC_COUNT,
	.max_inst	= 1,
	.handles	= mem_huge_handles,
	.descs = mem_huge_metric_descs,
	.names = mem_huge_metric_names,
	.pmids = mem_huge_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP instances for fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_fan_instances(struct activity *a)
{
	int inst = 0;
	char buf[32];
	
	for (inst = 0; inst < a->item_list_sz; inst++) {
		pmsprintf(buf, sizeof(buf), "fan%d", inst + 1);
		act_add_instance(a, POWER_FAN_DEVICE, buf, inst);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_fan_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_pwr_fan_instances(a);
	}

	act_add_metric(a, POWER_FAN_RPM);
	act_add_metric(a, POWER_FAN_DRPM);
	act_add_metric(a, POWER_FAN_DEVICE);

	pcp_alloc_item_list_handles(a);
}

const char *power_fan_metric_names[] = {
	[POWER_FAN_RPM] = "power.fan.rpm",
	[POWER_FAN_DRPM] = "power.fan.drpm",
	[POWER_FAN_DEVICE] = "power.fan.device",
};
pmDesc power_fan_metric_descs[] = {
	[POWER_FAN_RPM] = {
		.pmid = PMID_POWER_FAN_RPM,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, -1, 1, 0, PM_TIME_MIN, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_FAN_DRPM] = {
		.pmid = PMID_POWER_FAN_DRPM,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, -1, 1, 0, PM_TIME_MIN, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_FAN_DEVICE] = {
		.pmid = PMID_POWER_FAN_DEVICE,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID power_fan_metric_pmids[POWER_FAN_METRIC_COUNT];

struct act_metrics power_fan_metrics = {
	.count = POWER_FAN_METRIC_COUNT,
	.descs = power_fan_metric_descs,
	.names = power_fan_metric_names,
	.pmids = power_fan_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metric instances for temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_temp_instances(struct activity *a)
{
	int inst = 0;
	char buf[32];

	for (inst = 0; inst < a->item_list_sz; inst++) {
		pmsprintf(buf, sizeof(buf), "temp%d", inst + 1);
		act_add_instance(a, POWER_TEMP_DEVICE, buf, inst);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_temp_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_pwr_temp_instances(a);
	}

	act_add_metric_ex(a, POWER_TEMP_CELSIUS, PM_UNIT_TEMPERATURE, PM_TEMPERATURE_C);
	act_add_metric(a, POWER_TEMP_PERCENT);	/* dimensionless: % of high threshold */
	act_add_metric(a, POWER_TEMP_DEVICE);

	pcp_alloc_item_list_handles(a);
}

const char *power_temp_metric_names[] = {
	[POWER_TEMP_CELSIUS] = "power.temp.celsius",
	[POWER_TEMP_PERCENT] = "power.temp.percent",
	[POWER_TEMP_DEVICE] = "power.temp.device",
};
pmDesc power_temp_metric_descs[] = {
	[POWER_TEMP_CELSIUS] = {
		.pmid = PMID_POWER_TEMP_CELSIUS,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_TEMP_PERCENT] = {
		.pmid = PMID_POWER_TEMP_PERCENT,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_TEMP_DEVICE] = {
		.pmid = PMID_POWER_TEMP_DEVICE,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID power_temp_metric_pmids[POWER_TEMP_METRIC_COUNT];

struct act_metrics power_temp_metrics = {
	.count = POWER_TEMP_METRIC_COUNT,
	.descs = power_temp_metric_descs,
	.names = power_temp_metric_names,
	.pmids = power_temp_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metric instances for voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_in_instances(struct activity *a)
{
	int inst = 0;
	char buf[32];

	for (inst = 0; inst < a->item_list_sz; inst++) {
		pmsprintf(buf, sizeof(buf), "in%d", inst);
		act_add_instance(a, POWER_IN_DEVICE, buf, inst);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_in_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_pwr_in_instances(a);
	}

	act_add_metric_ex(a, POWER_IN_VOLTAGE, PM_UNIT_VOLTAGE, PM_VOLTAGE_V);
	act_add_metric(a, POWER_IN_PERCENT);	/* dimensionless: % of high threshold */
	act_add_metric(a, POWER_IN_DEVICE);

	pcp_alloc_item_list_handles(a);
}

const char *power_in_metric_names[] = {
	[POWER_IN_VOLTAGE] = "power.in.voltage",
	[POWER_IN_PERCENT] = "power.in.percent",
	[POWER_IN_DEVICE] = "power.in.device",
};
pmDesc power_in_metric_descs[] = {
	[POWER_IN_VOLTAGE] = {
		.pmid = PMID_POWER_IN_VOLTAGE,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_IN_PERCENT] = {
		.pmid = PMID_POWER_IN_PERCENT,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_IN_DEVICE] = {
		.pmid = PMID_POWER_IN_DEVICE,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID power_in_metric_pmids[POWER_IN_METRIC_COUNT];

struct act_metrics power_in_metrics = {
	.count = POWER_IN_METRIC_COUNT,
	.descs = power_in_metric_descs,
	.names = power_in_metric_names,
	.pmids = power_in_metric_pmids,
};

/*
 * **************************************************************************
 * Define PCP metric instances for battery statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_bat_instances(struct activity *a)
{
	int inst = 0;
	struct sa_item *list;

	for (list = a->item_list; list != NULL; list = list->next) {
		act_add_instance(a, POWER_BAT_STATUS, list->item_name, inst++);
	}
}

/*
 * **************************************************************************
 * Define PCP metrics for battery statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_bat_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_pwr_bat_instances(a);
	}

	act_add_metric(a, POWER_BAT_CAPACITY);
	act_add_metric(a, POWER_BAT_STATUS);

	pcp_alloc_item_list_handles(a);
}

const char *power_bat_metric_names[] = {
	[POWER_BAT_CAPACITY] = "power.bat.capacity",
	[POWER_BAT_STATUS] = "power.bat.status",
};
pmDesc power_bat_metric_descs[] = {
	[POWER_BAT_CAPACITY] = {
		.pmid = PMID_POWER_BAT_CAPACITY,
		.indom = PMI_INDOM(34, 4),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_BAT_STATUS] = {
		.pmid = PMID_POWER_BAT_STATUS,
		.indom = PMI_INDOM(34, 4),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_INSTANT,
	},
};
pmID power_bat_metric_pmids[POWER_BAT_METRIC_COUNT];

struct act_metrics power_bat_metrics = {
	.count = POWER_BAT_METRIC_COUNT,
	.descs = power_bat_metric_descs,
	.names = power_bat_metric_names,
	.pmids = power_bat_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metric instances for USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_usb_instances(struct activity *a)
{
	int inst = 0;
	char buf[32];

	for (inst = 0; inst < a->item_list_sz; inst++) {
		pmsprintf(buf, sizeof(buf), "usb%d", inst);
		act_add_instance(a, POWER_USB_BUS, buf, inst);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_usb_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_pwr_usb_instances(a);
	}

	act_add_metric(a, POWER_USB_BUS);
	act_add_metric(a, POWER_USB_VENDORID);
	act_add_metric(a, POWER_USB_PRODUCTID);
	act_add_metric(a, POWER_USB_MAXPOWER);
	act_add_metric(a, POWER_USB_MANUFACTURER);
	act_add_metric(a, POWER_USB_PRODUCTNAME);

	pcp_alloc_item_list_handles(a);
}

const char *power_usb_metric_names[] = {
	[POWER_USB_BUS] = "power.usb.bus",
	[POWER_USB_VENDORID] = "power.usb.vendorId",
	[POWER_USB_PRODUCTID] = "power.usb.productId",
	[POWER_USB_MAXPOWER] = "power.usb.maxpower",
	[POWER_USB_MANUFACTURER] = "power.usb.manufacturer",
	[POWER_USB_PRODUCTNAME] = "power.usb.productName",
};
pmDesc power_usb_metric_descs[] = {
	[POWER_USB_BUS] = {
		.pmid = PMID_POWER_USB_BUS,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_VENDORID] = {
		.pmid = PMID_POWER_USB_VENDORID,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_PRODUCTID] = {
		.pmid = PMID_POWER_USB_PRODUCTID,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_MAXPOWER] = {
		.pmid = PMID_POWER_USB_MAXPOWER,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_MANUFACTURER] = {
		.pmid = PMID_POWER_USB_MANUFACTURER,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_PRODUCTNAME] = {
		.pmid = PMID_POWER_USB_PRODUCTNAME,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};
pmID power_usb_metric_pmids[POWER_USB_METRIC_COUNT];

struct act_metrics power_usb_metrics = {
	.count = POWER_USB_METRIC_COUNT,
	.descs = power_usb_metric_descs,
	.names = power_usb_metric_names,
	.pmids = power_usb_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP metric instances for filesystem statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_filesystem_instances(struct activity *a)
{
	int inst = 0;
	struct sa_item *list;

	for (list = a->item_list; list != NULL; list = list->next) {
		act_add_instance(a, FILESYS_CAPACITY, list->item_name, inst++);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for filesystem statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_filesystem_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_filesystem_instances(a);
	}

	act_add_metric(a, FILESYS_CAPACITY);
	act_add_metric(a, FILESYS_FREE);
	act_add_metric(a, FILESYS_USED);
	act_add_metric(a, FILESYS_FULL);
	act_add_metric(a, FILESYS_MAXFILES);
	act_add_metric(a, FILESYS_FREEFILES);
	act_add_metric(a, FILESYS_USEDFILES);
	act_add_metric(a, FILESYS_AVAIL);

	pcp_alloc_item_list_handles(a);
}

const char *filesys_metric_names[] = {
	[FILESYS_CAPACITY] = "filesys.capacity",
	[FILESYS_FREE] = "filesys.free",
	[FILESYS_USED] = "filesys.used",
	[FILESYS_FULL] = "filesys.full",
	[FILESYS_MAXFILES] = "filesys.maxfiles",
	[FILESYS_FREEFILES] = "filesys.freefiles",
	[FILESYS_USEDFILES] = "filesys.usedfiles",
	[FILESYS_AVAIL] = "filesys.avail",
};
pmDesc filesys_metric_descs[] = {
	[FILESYS_CAPACITY] = {
		.pmid = PMID_FILESYS_CAPACITY,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FREE] = {
		.pmid = PMID_FILESYS_FREE,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_USED] = {
		.pmid = PMID_FILESYS_USED,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FULL] = {
		.pmid = PMID_FILESYS_FULL,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_DOUBLE,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_MAXFILES] = {
		.pmid = PMID_FILESYS_MAXFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FREEFILES] = {
		.pmid = PMID_FILESYS_FREEFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_USEDFILES] = {
		.pmid = PMID_FILESYS_USEDFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_AVAIL] = {
		.pmid = PMID_FILESYS_AVAIL,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};
pmID filesys_metric_pmids[FILESYS_METRIC_COUNT];

struct act_metrics filesys_metrics = {
	.count = FILESYS_METRIC_COUNT,
	.descs = filesys_metric_descs,
	.names = filesys_metric_names,
	.pmids = filesys_metric_pmids,
};

/*
 ***************************************************************************
 * Define PCP instances for Fibre Channel HBA statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_fchost_instances(struct activity *a)
{
	int inst = 0;
	struct sa_item *list;

	for (list = a->item_list; list != NULL; list = list->next) {
		act_add_instance(a, FCHOST_INBYTES, list->item_name, inst++);
	}
}

/*
 ***************************************************************************
 * Define PCP metrics for Fibre Channel HBA statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_fchost_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_fchost_instances(a);
	}

	act_add_metric(a, FCHOST_INFRAMES);
	act_add_metric(a, FCHOST_OUTFRAMES);
	act_add_metric(a, FCHOST_INBYTES);
	act_add_metric(a, FCHOST_OUTBYTES);

	pcp_alloc_item_list_handles(a);
}

const char *fchost_metric_names[] = {
	[FCHOST_INFRAMES] = "fchost.in.frames",
	[FCHOST_OUTFRAMES] = "fchost.out.frames",
	[FCHOST_INBYTES] = "fchost.in.bytes",
	[FCHOST_OUTBYTES] = "fchost.out.bytes",
};
pmDesc fchost_metric_descs[] = {
	[FCHOST_INFRAMES] = {
		.pmid = PMID_FCHOST_INFRAMES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_OUTFRAMES] = {
		.pmid = PMID_FCHOST_OUTFRAMES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_INBYTES] = {
		.pmid = PMID_FCHOST_INBYTES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_OUTBYTES] = {
		.pmid = PMID_FCHOST_OUTBYTES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};
pmID fchost_metric_pmids[FCHOST_METRIC_COUNT];

struct act_metrics fchost_metrics = {
	.count = FCHOST_METRIC_COUNT,
	.descs = fchost_metric_descs,
	.names = fchost_metric_names,
	.pmids = fchost_metric_pmids,
};

/*
 * **************************************************************************
 * Define PCP metrics for pressure-stall CPU statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psicpu_metrics(struct activity *a)
{
	act_add_metric(a, PSI_CPU_SOMETOTAL);
	act_add_metric(a, PSI_CPU_SOMEAVG);
}

/*
 * **************************************************************************
 * Define PCP metrics for pressure-stall I/O statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psiio_metrics(struct activity *a)
{
	act_add_metric(a, PSI_IO_SOMETOTAL);
	act_add_metric(a, PSI_IO_SOMEAVG);
	act_add_metric(a, PSI_IO_FULLTOTAL);
	act_add_metric(a, PSI_IO_FULLAVG);
}

/*
 * **************************************************************************
 * Define PCP metrics for pressure-stall memory statistics.
 *
 * IN
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psimem_metrics(struct activity *a)
{
	act_add_metric(a, PSI_MEM_SOMETOTAL);
	act_add_metric(a, PSI_MEM_SOMEAVG);
	act_add_metric(a, PSI_MEM_FULLTOTAL);
	act_add_metric(a, PSI_MEM_FULLAVG);
}

/*
 ***************************************************************************
 * Define PCP instances for pressure-stall statistics.
 *
 * IN:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psi_instances(struct activity *a)
{
	act_add_instance(a, PSI_CPU_SOMEAVG, "10 second", 10);
	act_add_instance(a, PSI_CPU_SOMEAVG, "1 minute", 60);
	act_add_instance(a, PSI_CPU_SOMEAVG, "5 minute", 300);
}

/*
 ***************************************************************************
 * Define PCP metrics for pressure-stall statistics.
 *
 * IN:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psi_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pcp_def_psi_instances(a);
	}

	if (a->id == A_PSI_CPU) {
		/* Create metrics for A_PSI_CPU */
		pcp_def_psicpu_metrics(a);
		/*
		 * Pre-allocate to 3 (the avg time-window count) before any
		 * scalar handle so the stride stays correct when max_inst grows.
		 */
		pcp_alloc_handles(a->metrics, 3);
		pcp_alloc_handle(a->metrics, PSI_CPU_SOMETOTAL, 0, PM_IN_NULL, NULL);
		pcp_alloc_handle(a->metrics, PSI_CPU_SOMEAVG,   0, 10,  "10 second");
		pcp_alloc_handle(a->metrics, PSI_CPU_SOMEAVG,   1, 60,  "1 minute");
		pcp_alloc_handle(a->metrics, PSI_CPU_SOMEAVG,   2, 300, "5 minute");
	}
	else if (a->id == A_PSI_IO) {
		/* Create metrics for A_PSI_IO */
		pcp_def_psiio_metrics(a);
		pcp_alloc_handles(a->metrics, 3);
		pcp_alloc_handle(a->metrics, PSI_IO_SOMETOTAL, 0, PM_IN_NULL, NULL);
		pcp_alloc_handle(a->metrics, PSI_IO_SOMEAVG,   0, 10,  "10 second");
		pcp_alloc_handle(a->metrics, PSI_IO_SOMEAVG,   1, 60,  "1 minute");
		pcp_alloc_handle(a->metrics, PSI_IO_SOMEAVG,   2, 300, "5 minute");
		pcp_alloc_handle(a->metrics, PSI_IO_FULLTOTAL, 0, PM_IN_NULL, NULL);
		pcp_alloc_handle(a->metrics, PSI_IO_FULLAVG,   0, 10,  "10 second");
		pcp_alloc_handle(a->metrics, PSI_IO_FULLAVG,   1, 60,  "1 minute");
		pcp_alloc_handle(a->metrics, PSI_IO_FULLAVG,   2, 300, "5 minute");
	}
	else if (a->id == A_PSI_MEM) {
		/* Create metrics for A_PSI_MEM */
		pcp_def_psimem_metrics(a);
		pcp_alloc_handles(a->metrics, 3);
		pcp_alloc_handle(a->metrics, PSI_MEM_SOMETOTAL, 0, PM_IN_NULL, NULL);
		pcp_alloc_handle(a->metrics, PSI_MEM_SOMEAVG,   0, 10,  "10 second");
		pcp_alloc_handle(a->metrics, PSI_MEM_SOMEAVG,   1, 60,  "1 minute");
		pcp_alloc_handle(a->metrics, PSI_MEM_SOMEAVG,   2, 300, "5 minute");
		pcp_alloc_handle(a->metrics, PSI_MEM_FULLTOTAL, 0, PM_IN_NULL, NULL);
		pcp_alloc_handle(a->metrics, PSI_MEM_FULLAVG,   0, 10,  "10 second");
		pcp_alloc_handle(a->metrics, PSI_MEM_FULLAVG,   1, 60,  "1 minute");
		pcp_alloc_handle(a->metrics, PSI_MEM_FULLAVG,   2, 300, "5 minute");
	}
}

const char *psi_cpu_metric_names[] = {
	[PSI_CPU_SOMETOTAL] = "kernel.all.pressure.cpu.some.total",
	[PSI_CPU_SOMEAVG] = "kernel.all.pressure.cpu.some.avg",
};
pmDesc psi_cpu_metric_descs[] = {
	[PSI_CPU_SOMETOTAL] = {
		.pmid = PMID_PSI_CPU_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_CPU_SOMEAVG] = {
		.pmid = PMID_PSI_CPU_SOMEAVG,
		.indom = PMI_INDOM(60, 37),	/* shared with io/mem avg */
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};
pmID psi_cpu_metric_pmids[PSI_CPU_METRIC_COUNT];

struct act_metrics psi_cpu_metrics = {
	.count = PSI_CPU_METRIC_COUNT,
	.descs = psi_cpu_metric_descs,
	.names = psi_cpu_metric_names,
	.pmids = psi_cpu_metric_pmids,
};

const char *psi_io_metric_names[] = {
	[PSI_IO_SOMETOTAL] = "kernel.all.pressure.io.some.total",
	[PSI_IO_SOMEAVG] = "kernel.all.pressure.io.some.avg",
	[PSI_IO_FULLTOTAL] = "kernel.all.pressure.io.full.total",
	[PSI_IO_FULLAVG] = "kernel.all.pressure.io.full.avg",
};
pmDesc psi_io_metric_descs[] = {
	[PSI_IO_SOMETOTAL] = {
		.pmid = PMID_PSI_IO_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_IO_SOMEAVG] = {
		.pmid = PMID_PSI_IO_SOMEAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[PSI_IO_FULLTOTAL] = {
		.pmid = PMID_PSI_IO_FULLTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_IO_FULLAVG] = {
		.pmid = PMID_PSI_IO_FULLAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};
pmID psi_io_metric_pmids[PSI_CPU_METRIC_COUNT];

struct act_metrics psi_io_metrics = {
	.count = PSI_IO_METRIC_COUNT,
	.descs = psi_io_metric_descs,
	.names = psi_io_metric_names,
	.pmids = psi_io_metric_pmids,
};

const char *psi_mem_metric_names[] = {
	[PSI_MEM_SOMETOTAL] = "kernel.all.pressure.memory.some.total",
	[PSI_MEM_SOMEAVG] = "kernel.all.pressure.memory.some.avg",
	[PSI_MEM_FULLTOTAL] = "kernel.all.pressure.memory.full.total",
	[PSI_MEM_FULLAVG] = "kernel.all.pressure.memory.full.avg",
};
pmDesc psi_mem_metric_descs[] = {
	[PSI_MEM_SOMETOTAL] = {
		.pmid = PMID_PSI_MEM_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_MEM_SOMEAVG] = {
		.pmid = PMID_PSI_MEM_SOMEAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[PSI_MEM_FULLTOTAL] = {
		.pmid = PMID_PSI_MEM_FULLTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_MEM_FULLAVG] = {
		.pmid = PMID_PSI_MEM_FULLAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};
pmID psi_mem_metric_pmids[PSI_IO_METRIC_COUNT];

struct act_metrics psi_mem_metrics = {
	.count = PSI_MEM_METRIC_COUNT,
	.descs = psi_mem_metric_descs,
	.names = psi_mem_metric_names,
	.pmids = psi_mem_metric_pmids,
};

#else	/* undef HAVE_PCP */
void pcp_def_percpu_intr_metrics(struct activity *a, int cpu) {}
void pcp_def_pcsw_metrics(struct activity *a) {}
void pcp_def_irq_metrics(struct activity *a) {}
void pcp_def_swap_metrics(struct activity *a) {}
void pcp_def_paging_metrics(struct activity *a) {}
void pcp_def_io_metrics(struct activity *a) {}
void pcp_def_memory_metrics(struct activity *a) {}
void pcp_def_ktables_metrics(struct activity *a) {}
void pcp_def_queue_metrics(struct activity *a) {}
void pcp_def_disk_metrics(struct activity *a) {}
void pcp_probe_dm_instances(struct activity *a) {}
void pcp_probe_md_instances(struct activity *a) {}
void pcp_probe_part_instances(struct activity *a) {}
void pcp_probe_zram_instances(struct activity *a) {}
void pcp_def_net_dev_metrics(struct activity *a) {}
void pcp_def_serial_metrics(struct activity *a) {}
void pcp_def_net_nfs_metrics(struct activity *a) {}
void pcp_def_net_nfsd_metrics(struct activity *a) {}
void pcp_def_net_sock_metrics(struct activity *a) {}
void pcp_def_net_ip_metrics(struct activity *a) {}
void pcp_def_net_eip_metrics(struct activity *a) {}
void pcp_def_net_icmp_metrics(struct activity *a) {}
void pcp_def_net_eicmp_metrics(struct activity *a) {}
void pcp_def_net_tcp_metrics(struct activity *a) {}
void pcp_def_net_etcp_metrics(struct activity *a) {}
void pcp_def_net_udp_metrics(struct activity *a) {}
void pcp_def_net_sock6_metrics(struct activity *a) {}
void pcp_def_net_ip6_metrics(struct activity *a) {}
void pcp_def_net_eip6_metrics(struct activity *a) {}
void pcp_def_net_icmp6_metrics(struct activity *a) {}
void pcp_def_net_eicmp6_metrics(struct activity *a) {}
void pcp_def_net_udp6_metrics(struct activity *a) {}
void pcp_def_huge_metrics(struct activity *a) {}
void pcp_def_pwr_fan_metrics(struct activity *a) {}
void pcp_def_pwr_temp_metrics(struct activity *a) {}
void pcp_def_pwr_in_metrics(struct activity *a) {}
void pcp_def_pwr_bat_metrics(struct activity *a) {}
void pcp_def_pwr_usb_metrics(struct activity *a) {}
void pcp_def_filesystem_metrics(struct activity *a) {}
void pcp_def_fchost_metrics(struct activity *a) {}
void pcp_def_psi_metrics(struct activity *a) {}
#endif /* HAVE_PCP */
