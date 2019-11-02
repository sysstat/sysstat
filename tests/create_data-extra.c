/*
 * create_data-extra.c: Create a binary data file with extra structures
 * (C) 2019 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define UTSNAME_LEN		65
#define FILE_MAGIC_PADDING	48

#define FALSE	0
#define TRUE	1

int main(void)
{
	struct file_magic_12_1_7 {
		unsigned short sysstat_magic;
		unsigned short format_magic;
		unsigned char sysstat_version;
		unsigned char sysstat_patchlevel;
		unsigned char sysstat_sublevel;
		unsigned char sysstat_extraversion;
		unsigned int header_size;	/* Init me! */
		unsigned int upgraded;
		unsigned int hdr_types_nr[3];
		unsigned char pad[FILE_MAGIC_PADDING];
	};

	struct file_magic_12_1_7 f_magic = {
		.sysstat_magic = 0xd596,
		.format_magic = 0x2175,
		.sysstat_version = 12,
		.sysstat_patchlevel = 1,
		.sysstat_sublevel = 7,
		.sysstat_extraversion = 0,
		.upgraded = 0,
		.hdr_types_nr = {1, 1, 12}
	};

	struct file_header_12_1_7 {
		unsigned long long sa_ust_time;
		unsigned long sa_hz		__attribute__ ((aligned (8)));
		unsigned int sa_cpu_nr		__attribute__ ((aligned (8)));
		unsigned int sa_act_nr;
		int sa_year;
		unsigned int act_types_nr[3];
		unsigned int rec_types_nr[3];
		unsigned int act_size;	/* Init me! */
		unsigned int rec_size;	/* Init me! */
		unsigned int extra_next;
		unsigned char sa_day;
		unsigned char sa_month;
		char sa_sizeof_long;
		char sa_sysname[UTSNAME_LEN];
		char sa_nodename[UTSNAME_LEN];
		char sa_release[UTSNAME_LEN];
		char sa_machine[UTSNAME_LEN];
	};

	struct file_header_12_1_7 f_header = {
		.sa_ust_time = 1568533161,
		.sa_hz = 100,
		.sa_cpu_nr = 3,
		.sa_act_nr = 2,
		.sa_year = 2019,
		.act_types_nr = {0, 0, 9},
		.rec_types_nr = {2, 0, 1},
		.extra_next = TRUE,
		.sa_day = 15,
		.sa_month = 9,
		.sa_sizeof_long = 8,
		.sa_sysname = "Linux",
		.sa_nodename = "localhost.localdomain",
		.sa_release = "5.0.16-100.fc28.x86_64",
		.sa_machine = "x86_64"
	};

	struct file_activity_12_1_7 {
		unsigned int id;
		unsigned int magic;
		int nr;
		int nr2;
		int has_nr;
		int size;	/* Init me! */
		unsigned int types_nr[3];
	};

	struct file_activity_12_1_7 f_activity_a_cpu = {
		.id = 1,	/* A_CPU */
		.magic = 0x8b,
		.nr = 3,
		.nr2 = 1,
		.has_nr = TRUE,
		.types_nr = {10, 0, 0}
	};

	struct file_activity_12_1_7 f_activity_a_pcsw = {
		.id = 2,	/* A_PCSW */
		.magic = 0x8b,
		.nr = 1,
		.nr2 = 1,
		.has_nr = FALSE,
		.types_nr = {1, 1, 0}
	};

	struct extra_desc {
		unsigned int extra_nr;
		unsigned int extra_size;	/* Init me! */
		unsigned int extra_next;
		unsigned int extra_types_nr[3];
	};

	struct extra_desc extra_desc_1 = {
		.extra_nr = 2,
		.extra_next = TRUE,
		.extra_types_nr = {1, 0, 1}
	};
	struct extra_desc extra_desc_2 = {
		.extra_nr = 0,
		.extra_next = FALSE,
		.extra_types_nr = {1, 0, 1}
	};

	struct extra_structure_1 {
		unsigned long long es_ull;
		unsigned int es_u;
		char extra_text[20];
	};

	struct extra_structure_1 e_s_1_1 = {
		.es_ull = 0x123456789a,
		.es_u = 0xdff9,
		.extra_text = "Extra_structure_1"
	};
	struct extra_structure_1 e_s_1_2 = {
		.es_ull = 0xabcdef012,
		.es_u = 0xd5aa,
		.extra_text = "Extra structure"
	};

	struct record_header_12_1_7 {
		unsigned long long uptime_cs;
		unsigned long long ust_time;
		unsigned int extra_next;
		unsigned char record_type;
		unsigned char hour;
		unsigned char minute;
		unsigned char second;
	};

	struct record_header_12_1_7 r_header_1 = {
		.uptime_cs = 15000,
		.ust_time = 1568540000,
		.extra_next = TRUE,
		.record_type = 2,	/* R_RESTART */
		.hour = 11,
		.minute = 5,
		.second = 58
	};

	struct extra_desc extra_desc_3 = {
		.extra_nr = 0,
		.extra_next = FALSE,
		.extra_types_nr = {0, 2, 1}
	};

	struct record_header_12_1_7 r_header_2 = {
		.uptime_cs = 15100,
		.ust_time = 1568540100,
		.extra_next = TRUE,
		.record_type = 8,	/* R_EXTRA */
		.hour = 11,
		.minute = 6,
		.second = 0
	};

	struct extra_desc extra_desc_4 = {
		.extra_nr = 1,
		.extra_next = TRUE,
		.extra_types_nr = {0, 2, 1}
	};

	struct extra_structure_2 {
		unsigned long es_ul_1;
		unsigned long es_ul_2;
		unsigned int es_u;
		char extra_text[10];
	};

	struct extra_structure_2 e_s_2_1 = {
		.es_ul_1 = 0x5678ab,
		.es_ul_2 = 0xabcdee,
		.es_u = 3697,
		.extra_text = "Bonjour"
	};

	struct record_header_12_1_7 r_header_3 = {
		.uptime_cs = 15200,
		.ust_time = 1568540200,
		.extra_next = TRUE,
		.record_type = 1,	/* R_STATS */
		.hour = 11,
		.minute = 6,
		.second = 1
	};

	struct extra_desc extra_desc_5 = {
		.extra_nr = 1,
		.extra_next = TRUE,
		.extra_types_nr = {1, 0, 1}
	};

	struct extra_structure_1 e_s_1_3 = {
		.es_ull = 0xabcdefedfb,
		.es_u = 0xd5aa,
		.extra_text = "Last struct"
	};

	struct record_header_12_1_7 r_header_4 = {
		.uptime_cs = 15300,
		.ust_time = 1568540300,
		.extra_next = FALSE,
		.record_type = 1,	/* R_STATS */
		.hour = 11,
		.minute = 6,
		.second = 2
	};

	struct stats_cpu_12_1_7 {
		unsigned long long cpu_user;
		unsigned long long cpu_nice;
		unsigned long long cpu_sys;
		unsigned long long cpu_idle;
		unsigned long long cpu_iowait;
		unsigned long long cpu_steal;
		unsigned long long cpu_hardirq;
		unsigned long long cpu_softirq;
		unsigned long long cpu_guest;
		unsigned long long cpu_guest_nice;
	};

	struct stats_cpu_12_1_7 s_cpu_0_1 = {
		.cpu_user = 1000,
		.cpu_nice = 0,
		.cpu_sys = 500,
		.cpu_idle = 0,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_cpu_12_1_7 s_cpu_1_1 = {
		.cpu_user = 1000,
		.cpu_nice = 0,
		.cpu_sys = 0,
		.cpu_idle = 0,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_cpu_12_1_7 s_cpu_2_1 = {
		.cpu_user = 0,
		.cpu_nice = 0,
		.cpu_sys = 500,
		.cpu_idle = 0,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_cpu_12_1_7 s_cpu_0_2 = {
		.cpu_user = 1100,
		.cpu_nice = 0,
		.cpu_sys = 500,
		.cpu_idle = 100,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_cpu_12_1_7 s_cpu_1_2 = {
		.cpu_user = 1100,
		.cpu_nice = 0,
		.cpu_sys = 0,
		.cpu_idle = 0,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_cpu_12_1_7 s_cpu_2_2 = {
		.cpu_user = 0,
		.cpu_nice = 0,
		.cpu_sys = 500,
		.cpu_idle = 100,
		.cpu_iowait = 0,
		.cpu_steal = 0,
		.cpu_hardirq = 0,
		.cpu_softirq = 0,
		.cpu_guest = 0,
		.cpu_guest_nice = 0
	};

	struct stats_pcsw {
		unsigned long long context_switch;
		unsigned long	   processes	__attribute__ ((aligned (8)));
	};

	struct stats_pcsw s_pcsw_1 = {
		.context_switch = 1234,
		.processes = 543
	};

	struct stats_pcsw s_pcsw_2 = {
		.context_switch = 2234,
		.processes = 643
	};

	int fd;
	int nr_cpu;

	/* Open data file */
	if ((fd = open("data-extra", O_CREAT | O_WRONLY,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		perror("open");
		exit(1);
	}

	/* Write file magic structure */
	f_magic.header_size = sizeof(struct file_header_12_1_7);
	if (write(fd, &f_magic, sizeof(struct file_magic_12_1_7)) < 0) {
		perror("write file magic");
		exit(1);
	}

	/* Write file header structure */
	f_header.act_size = sizeof(struct file_activity_12_1_7);
	f_header.rec_size = sizeof(struct record_header_12_1_7);
	if (write(fd, &f_header, sizeof(struct file_header_12_1_7)) < 0) {
		perror("write file header");
		exit(1);
	}

	/* Write file activity list */
	f_activity_a_cpu.size = sizeof(struct stats_cpu_12_1_7);
	if (write(fd, &f_activity_a_cpu, sizeof(struct file_activity_12_1_7)) < 0) {
		perror("write file activity A_CPU");
		exit(1);
	}
	f_activity_a_pcsw.size = sizeof(struct stats_pcsw);
	if (write(fd, &f_activity_a_pcsw, sizeof(struct file_activity_12_1_7)) < 0) {
		perror("write file activity A_PCSW");
		exit(1);
	}

	/* Write extra desc structure */
	extra_desc_1.extra_size = sizeof(struct extra_structure_1);
	if (write(fd, &extra_desc_1, sizeof(struct extra_desc)) < 0) {
		perror("write 1st file extra desc");
		exit(1);
	}

	/* Write extra structures */
	if (write(fd, &e_s_1_1, sizeof(struct extra_structure_1)) < 0) {
		perror("write file extra struct_1_1");
		exit(1);
	}
	if (write(fd, &e_s_1_2, sizeof(struct extra_structure_1)) < 0) {
		perror("write file extra struct_1_2");
		exit(1);
	}

	/* Write extra desc structure */
	extra_desc_2.extra_size = sizeof(struct extra_structure_1);
	if (write(fd, &extra_desc_2, sizeof(struct extra_desc)) < 0) {
		perror("write 2nd file extra desc");
		exit(1);
	}

	/* Write R_RESTART record */
	if (write(fd, &r_header_1, sizeof(struct record_header_12_1_7)) < 0) {
		perror("write RESTART record");
		exit(1);
	}
	nr_cpu = 3;
	if (write(fd, &nr_cpu, sizeof(int)) < 0) {
		perror("write nr_cpu #1");
		exit(1);
	}
	extra_desc_3.extra_size = sizeof(struct extra_structure_2);
	if (write(fd, &extra_desc_3, sizeof(struct extra_desc)) < 0) {
		perror("write 3rd file extra desc");
		exit(1);
	}

	/* Write R_EXTRA record */
	if (write(fd, &r_header_2, sizeof(struct record_header_12_1_7)) < 0) {
		perror("write EXTRA record");
		exit(1);
	}
	extra_desc_4.extra_size = sizeof(struct extra_structure_2);
	if (write(fd, &extra_desc_4, sizeof(struct extra_desc)) < 0) {
		perror("write 4th file extra desc");
		exit(1);
	}
	if (write(fd, &e_s_2_1, sizeof(struct extra_structure_2)) < 0) {
		perror("write file extra struct_2_1");
		exit(1);
	}
	extra_desc_3.extra_size = sizeof(struct extra_structure_2);
	if (write(fd, &extra_desc_3, sizeof(struct extra_desc)) < 0) {
		perror("write 3rd bis file extra desc");
		exit(1);
	}

	/* Write R_STAT #1 record */
	if (write(fd, &r_header_3, sizeof(struct record_header_12_1_7)) < 0) {
		perror("write STAT #1 record");
		exit(1);
	}
	extra_desc_5.extra_size = sizeof(struct extra_structure_1);
	if (write(fd, &extra_desc_5, sizeof(struct extra_desc)) < 0) {
		perror("write 5th file extra desc");
		exit(1);
	}
	if (write(fd, &e_s_1_3, sizeof(struct extra_structure_1)) < 0) {
		perror("write file extra struct_1_3");
		exit(1);
	}
	extra_desc_4.extra_size = sizeof(struct extra_structure_2);
	if (write(fd, &extra_desc_4, sizeof(struct extra_desc)) < 0) {
		perror("write 6th file extra desc");
		exit(1);
	}
	if (write(fd, &e_s_2_1, sizeof(struct extra_structure_2)) < 0) {
		perror("write file extra struct_2_1 bis");
		exit(1);
	}
	extra_desc_3.extra_size = sizeof(struct extra_structure_2);
	if (write(fd, &extra_desc_3, sizeof(struct extra_desc)) < 0) {
		perror("write 3rd ter file extra desc");
		exit(1);
	}

	/* Write A_CPU stats */
	nr_cpu = 3;
	if (write(fd, &nr_cpu, sizeof(int)) < 0) {
		perror("write nr_cpu #1");
		exit(1);
	}
	if (write(fd, &s_cpu_0_1, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 0_1");
		exit(1);
	}
	if (write(fd, &s_cpu_1_1, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 1_1");
		exit(1);
	}
	if (write(fd, &s_cpu_2_1, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 2_1");
		exit(1);
	}

	/* Write A_PCSW stats */
	if (write(fd, &s_pcsw_1, sizeof(struct stats_pcsw)) < 0) {
		perror("write PCSW stats 1");
		exit(1);
	}

	/* Write R_STAT #2 record */
	if (write(fd, &r_header_4, sizeof(struct record_header_12_1_7)) < 0) {
		perror("write STAT #2 record");
		exit(1);
	}

	/* Write A_CPU stats */
	nr_cpu = 3;
	if (write(fd, &nr_cpu, sizeof(int)) < 0) {
		perror("write nr_cpu #2");
		exit(1);
	}
	if (write(fd, &s_cpu_0_2, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 0_2");
		exit(1);
	}
	if (write(fd, &s_cpu_1_2, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 1_2");
		exit(1);
	}
	if (write(fd, &s_cpu_2_2, sizeof(struct stats_cpu_12_1_7)) < 0) {
		perror("write CPU stats 2_2");
		exit(1);
	}

	/* Write A_PCSW stats */
	if (write(fd, &s_pcsw_2, sizeof(struct stats_pcsw)) < 0) {
		perror("write PCSW stats 2");
		exit(1);
	}

	/* Close data file */
	close(fd);

	return 0;
}
