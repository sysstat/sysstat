/*
 * cgroupstat: Minimal helper to report CPU usage for the current cgroup
 * using Linux cgroup v2 cpu.stat.
 *
 * This tool is intentionally small and self-contained so it can be built
 * alongside sysstat without touching the existing collectors.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -o cgroupstat cgroupstat.c
 *
 * Usage:
 *   ./cgroupstat            # one-shot dump of cpu.stat for current cgroup
 *   ./cgroupstat 1 5        # take 5 samples at 1-second intervals
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Best-effort detection of current cgroup v2 cpu.stat file for this process. */
static int get_cgroup_cpu_stat_path(char *buf, size_t buflen)
{
	FILE *fp;
	char line[4096];
	char cgroup_rel[4096] = {0};
	size_t len;

	/*
	 * First, read /proc/self/cgroup and look for the cgroup v2 line,
	 * which typically has the form:
	 *   0::/user.slice/...
	 */
	fp = fopen("/proc/self/cgroup", "r");
	if (!fp) {
		perror("fopen(/proc/self/cgroup)");
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (strncmp(line, "0::", 3) != 0) {
			continue;
		}

		/* Path starts right after the "0::" prefix. */
		char *path = line + 3;
		len = strcspn(path, "\n");
		if (len >= sizeof(cgroup_rel)) {
			len = sizeof(cgroup_rel) - 1;
		}
		memcpy(cgroup_rel, path, len);
		cgroup_rel[len] = '\0';
		break;
	}
	fclose(fp);

	if (cgroup_rel[0] == '\0') {
		/* Fall back to the cgroup v2 root. */
		snprintf(buf, buflen, "/sys/fs/cgroup/cpu.stat");
		return 0;
	}

	/*
	 * For the common case where cgroup v2 is mounted at /sys/fs/cgroup,
	 * the cpu.stat file for the current cgroup is:
	 *   /sys/fs/cgroup<rel_path>/cpu.stat
	 */
	if (snprintf(buf, buflen, "/sys/fs/cgroup%s/cpu.stat", cgroup_rel) >= (int) buflen) {
		fprintf(stderr, "cgroupstat: cpu.stat path is too long\n");
		return -1;
	}

	return 0;
}

struct cpu_usage {
	unsigned long long usage_usec;
	unsigned long long user_usec;
	unsigned long long system_usec;
};

static int read_cpu_stat(const char *path, struct cpu_usage *u)
{
	FILE *fp;
	char line[256];

	memset(u, 0, sizeof(*u));

	fp = fopen(path, "r");
	if (!fp) {
		perror("fopen(cpu.stat)");
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		char name[64];
		unsigned long long value;

		if (sscanf(line, "%63s %llu", name, &value) != 2)
			continue;

		if (strcmp(name, "usage_usec") == 0) {
			u->usage_usec = value;
		} else if (strcmp(name, "user_usec") == 0) {
			u->user_usec = value;
		} else if (strcmp(name, "system_usec") == 0) {
			u->system_usec = value;
		}
	}

	fclose(fp);
	return 0;
}

static void print_single_sample(const char *path, const struct cpu_usage *u)
{
	printf("cgroup cpu.stat: %s\n", path);
	printf("  usage_usec  = %llu\n", (unsigned long long) u->usage_usec);
	printf("  user_usec   = %llu\n", (unsigned long long) u->user_usec);
	printf("  system_usec = %llu\n", (unsigned long long) u->system_usec);
}

int main(int argc, char **argv)
{
	char cpu_stat_path[4096];
	struct cpu_usage cur, prev;
	int interval = 0;
	int count = 1;

	if (get_cgroup_cpu_stat_path(cpu_stat_path, sizeof(cpu_stat_path)) < 0) {
		return 1;
	}

	if (argc == 3) {
		interval = atoi(argv[1]);
		count = atoi(argv[2]);
		if (interval <= 0 || count <= 0) {
			fprintf(stderr, "Usage: %s [interval count]\n", argv[0]);
			return 1;
		}
	} else if (argc != 1) {
		fprintf(stderr, "Usage: %s [interval count]\n", argv[0]);
		return 1;
	}

	if (count == 1 || interval == 0) {
		if (read_cpu_stat(cpu_stat_path, &cur) < 0) {
			return 1;
		}
		print_single_sample(cpu_stat_path, &cur);
		return 0;
	}

	/* Sample in a loop and print deltas between consecutive reads. */
	if (read_cpu_stat(cpu_stat_path, &prev) < 0) {
		return 1;
	}

	for (int i = 0; i < count; i++) {
		if (sleep(interval) != 0) {
			break;
		}
		if (read_cpu_stat(cpu_stat_path, &cur) < 0) {
			return 1;
		}

		printf("cgroup cpu.stat delta over %d s (sample %d/%d):\n",
		       interval, i + 1, count);
		printf("  usage_usec  = %llu\n",
		       (unsigned long long) (cur.usage_usec  - prev.usage_usec));
		printf("  user_usec   = %llu\n",
		       (unsigned long long) (cur.user_usec   - prev.user_usec));
		printf("  system_usec = %llu\n",
		       (unsigned long long) (cur.system_usec - prev.system_usec));

		prev = cur;
	}

	return 0;
}


