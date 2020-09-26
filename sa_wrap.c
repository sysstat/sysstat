/*
 * sysstat - sa_wrap.c: Functions used in activity.c
 * (C) 1999-2020 by Sebastien GODARD (sysstat <at> orange.fr)
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

#include <dirent.h>
#include <string.h>

#include "sa.h"
#include "count.h"

extern unsigned int flags;
extern struct record_header record_hdr;

/*
 ***************************************************************************
 * Reallocate buffer where statistics will be saved. The new size is the
 * double of the original one.
 * This is typically called when we find that current buffer is too small
 * to save all the data.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * New pointer address on buffer.
 ***************************************************************************
 */
void *reallocate_buffer(struct activity *a)
{
	SREALLOC(a->_buf0, void,
		 (size_t) a->msize * (size_t) a->nr_allocated * 2);	/* a->nr2 value is 1 */
	memset(a->_buf0, 0, (size_t) a->msize * (size_t) a->nr_allocated * 2);

	a->nr_allocated *= 2;	/* NB: nr_allocated > 0 */

	return a->_buf0;
}

/*
 ***************************************************************************
 * Read CPU statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_stat_cpu(struct activity *a)
{
	struct stats_cpu *st_cpu
		= (struct stats_cpu *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read CPU statistics */
	do {
		nr_read = read_stat_cpu(st_cpu, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_cpu = (struct stats_cpu *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read process (task) creation and context switch statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_stat_pcsw(struct activity *a)
{
	struct stats_pcsw *st_pcsw
		= (struct stats_pcsw *) a->_buf0;

	/* Read process and context switch stats */
	read_stat_pcsw(st_pcsw);

	return;
}

/*
 ***************************************************************************
 * Read interrupt statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_stat_irq(struct activity *a)
{
	struct stats_irq *st_irq
		= (struct stats_irq *) a->_buf0;
	__nr_t nr_read;

	/* Read interrupts stats */
	do {
		nr_read = read_stat_irq(st_irq, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_irq = (struct stats_irq *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read queue and load statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_loadavg(struct activity *a)
{
	struct stats_queue *st_queue
		= (struct stats_queue *) a->_buf0;

	/* Read queue and load stats */
	read_loadavg(st_queue);

	return;
}

/*
 ***************************************************************************
 * Read memory statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_meminfo(struct activity *a)
{
	struct stats_memory *st_memory
		= (struct stats_memory *) a->_buf0;

	/* Read memory stats */
	read_meminfo(st_memory);

	return;
}

/*
 ***************************************************************************
 * Read swapping statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_swap(struct activity *a)
{
	struct stats_swap *st_swap
		= (struct stats_swap *) a->_buf0;

	/* Read stats from /proc/vmstat */
	read_vmstat_swap(st_swap);

	return;
}

/*
 ***************************************************************************
 * Read paging statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_paging(struct activity *a)
{
	struct stats_paging *st_paging
		= (struct stats_paging *) a->_buf0;

	/* Read stats from /proc/vmstat */
	read_vmstat_paging(st_paging);

	return;
}

/*
 ***************************************************************************
 * Read I/O and transfer rates statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_io(struct activity *a)
{
	struct stats_io *st_io
		= (struct stats_io *) a->_buf0;

	/* Read stats from /proc/diskstats */
	read_diskstats_io(st_io);

	return;
}

/*
 ***************************************************************************
 * Read block devices statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_disk(struct activity *a)
{
	struct stats_disk *st_disk
		= (struct stats_disk *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read stats from /proc/diskstats */
	do {
		nr_read = read_diskstats_disk(st_disk, a->nr_allocated,
					      COLLECT_PARTITIONS(a->opt_flags));

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_disk = (struct stats_disk *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read serial lines statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_tty_driver_serial(struct activity *a)
{
	struct stats_serial *st_serial
		= (struct stats_serial *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read serial lines stats */
	do {
		nr_read = read_tty_driver_serial(st_serial, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_serial = (struct stats_serial *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read kernel tables statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_kernel_tables(struct activity *a)
{
	struct stats_ktables *st_ktables
		= (struct stats_ktables *) a->_buf0;

	/* Read kernel tables stats */
	read_kernel_tables(st_ktables);

	return;
}

/*
 ***************************************************************************
 * Read network interfaces statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_dev(struct activity *a)
{
	struct stats_net_dev *st_net_dev
		= (struct stats_net_dev *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read network interfaces stats */
	do {
		nr_read = read_net_dev(st_net_dev, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_net_dev = (struct stats_net_dev *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	if (!nr_read)
		/* No data read. Exit */
		return;

	/* Read duplex and speed info for each interface */
	read_if_info(st_net_dev, nr_read);

	return;
}

/*
 ***************************************************************************
 * Read network interfaces errors statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_edev(struct activity *a)
{
	struct stats_net_edev *st_net_edev
		= (struct stats_net_edev *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read network interfaces errors stats */
	do {
		nr_read = read_net_edev(st_net_edev, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_net_edev = (struct stats_net_edev *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read NFS client statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_nfs(struct activity *a)
{
	struct stats_net_nfs *st_net_nfs
		= (struct stats_net_nfs *) a->_buf0;

	/* Read NFS client stats */
	read_net_nfs(st_net_nfs);

	return;
}

/*
 ***************************************************************************
 * Read NFS server statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_nfsd(struct activity *a)
{
	struct stats_net_nfsd *st_net_nfsd
		= (struct stats_net_nfsd *) a->_buf0;

	/* Read NFS server stats */
	read_net_nfsd(st_net_nfsd);

	return;
}

/*
 ***************************************************************************
 * Read network sockets statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_sock(struct activity *a)
{
	struct stats_net_sock *st_net_sock
		= (struct stats_net_sock *) a->_buf0;

	/* Read network sockets stats */
	read_net_sock(st_net_sock);

	return;
}

/*
 ***************************************************************************
 * Read IP statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_ip(struct activity *a)
{
	struct stats_net_ip *st_net_ip
		= (struct stats_net_ip *) a->_buf0;

	/* Read IP stats */
	read_net_ip(st_net_ip);

	return;
}

/*
 ***************************************************************************
 * Read IP error statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_eip(struct activity *a)
{
	struct stats_net_eip *st_net_eip
		= (struct stats_net_eip *) a->_buf0;

	/* Read IP error stats */
	read_net_eip(st_net_eip);

	return;
}

/*
 ***************************************************************************
 * Read ICMP statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_icmp(struct activity *a)
{
	struct stats_net_icmp *st_net_icmp
		= (struct stats_net_icmp *) a->_buf0;

	/* Read ICMP stats */
	read_net_icmp(st_net_icmp);

	return;
}

/*
 ***************************************************************************
 * Read ICMP error statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_eicmp(struct activity *a)
{
	struct stats_net_eicmp *st_net_eicmp
		= (struct stats_net_eicmp *) a->_buf0;

	/* Read ICMP error stats */
	read_net_eicmp(st_net_eicmp);

	return;
}

/*
 ***************************************************************************
 * Read TCP statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_tcp(struct activity *a)
{
	struct stats_net_tcp *st_net_tcp
		= (struct stats_net_tcp *) a->_buf0;

	/* Read TCP stats */
	read_net_tcp(st_net_tcp);

	return;
}

/*
 ***************************************************************************
 * Read TCP error statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_etcp(struct activity *a)
{
	struct stats_net_etcp *st_net_etcp
		= (struct stats_net_etcp *) a->_buf0;

	/* Read TCP error stats */
	read_net_etcp(st_net_etcp);

	return;
}

/*
 ***************************************************************************
 * Read UDP statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_udp(struct activity *a)
{
	struct stats_net_udp *st_net_udp
		= (struct stats_net_udp *) a->_buf0;

	/* Read UDP stats */
	read_net_udp(st_net_udp);

	return;
}

/*
 ***************************************************************************
 * Read IPv6 network sockets statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_sock6(struct activity *a)
{
	struct stats_net_sock6 *st_net_sock6
		= (struct stats_net_sock6 *) a->_buf0;

	/* Read IPv6 network sockets stats */
	read_net_sock6(st_net_sock6);

	return;
}

/*
 ***************************************************************************
 * Read IPv6 statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_ip6(struct activity *a)
{
	struct stats_net_ip6 *st_net_ip6
		= (struct stats_net_ip6 *) a->_buf0;

	/* Read IPv6 stats */
	read_net_ip6(st_net_ip6);

	return;
}

/*
 ***************************************************************************
 * Read IPv6 error statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_eip6(struct activity *a)
{
	struct stats_net_eip6 *st_net_eip6
		= (struct stats_net_eip6 *) a->_buf0;

	/* Read IPv6 error stats */
	read_net_eip6(st_net_eip6);

	return;
}

/*
 ***************************************************************************
 * Read ICMPv6 statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_icmp6(struct activity *a)
{
	struct stats_net_icmp6 *st_net_icmp6
		= (struct stats_net_icmp6 *) a->_buf0;

	/* Read ICMPv6 stats */
	read_net_icmp6(st_net_icmp6);

	return;
}

/*
 ***************************************************************************
 * Read ICMPv6 error statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_eicmp6(struct activity *a)
{
	struct stats_net_eicmp6 *st_net_eicmp6
		= (struct stats_net_eicmp6 *) a->_buf0;

	/* Read ICMPv6 error stats */
	read_net_eicmp6(st_net_eicmp6);

	return;
}

/*
 ***************************************************************************
 * Read UDPv6 statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_net_udp6(struct activity *a)
{
	struct stats_net_udp6 *st_net_udp6
		= (struct stats_net_udp6 *) a->_buf0;

	/* Read UDPv6 stats */
	read_net_udp6(st_net_udp6);

	return;
}

/*
 ***************************************************************************
 * Read CPU frequency statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_cpuinfo(struct activity *a)
{
	struct stats_pwr_cpufreq *st_pwr_cpufreq
		= (struct stats_pwr_cpufreq *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read CPU frequency stats */
	do {
		nr_read = read_cpuinfo(st_pwr_cpufreq, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_pwr_cpufreq = (struct stats_pwr_cpufreq *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read fan statistics.
 *
 * IN:
 * @a  Activity structure.
 *
 * OUT:
 * @a  Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_fan(struct activity *a)
{
	struct stats_pwr_fan *st_pwr_fan
		= (struct stats_pwr_fan *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read fan stats */
	do {
		nr_read = read_fan(st_pwr_fan, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_pwr_fan = (struct stats_pwr_fan *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read temperature statistics.
 *
 * IN:
 * @a  Activity structure.
 *
 * OUT:
 * @a  Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_temp(struct activity *a)
{
	struct stats_pwr_temp *st_pwr_temp
		= (struct stats_pwr_temp *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read temperature stats */
	do {
		nr_read = read_temp(st_pwr_temp, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_pwr_temp = (struct stats_pwr_temp *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read voltage input statistics.
 *
 * IN:
 * @a  Activity structure.
 *
 * OUT:
 * @a  Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_in(struct activity *a)
{
	struct stats_pwr_in *st_pwr_in
		= (struct stats_pwr_in *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read voltage input stats */
	do {
		nr_read = read_in(st_pwr_in, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_pwr_in = (struct stats_pwr_in *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read hugepages statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_meminfo_huge(struct activity *a)
{
	struct stats_huge *st_huge
		= (struct stats_huge *) a->_buf0;

	/* Read hugepages stats */
	read_meminfo_huge(st_huge);

	return;
}

/*
 ***************************************************************************
 * Read weighted CPU frequency statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_cpu_wghfreq(struct activity *a)
{
	struct stats_pwr_wghfreq *st_pwr_wghfreq
		= (struct stats_pwr_wghfreq *) a->_buf0;
	__nr_t	nr_read = 0;

	/* Read weighted CPU frequency statistics */
	do {
		nr_read = read_cpu_wghfreq(st_pwr_wghfreq, a->nr_allocated, a->nr2);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			SREALLOC(a->_buf0, void,
				 (size_t) a->msize * (size_t) a->nr2 * (size_t) a->nr_allocated * 2);
			memset(a->_buf0, 0,
			       (size_t) a->msize * (size_t) a->nr2 * (size_t) a->nr_allocated * 2);

			/* NB: nr_allocated > 0 */
			a->nr_allocated *= 2;
			st_pwr_wghfreq = (struct stats_pwr_wghfreq *) a->_buf0;
		}
	}
	while(nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read USB devices statistics.
 *
 * IN:
 * @a  Activity structure.
 *
 * OUT:
 * @a  Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_bus_usb_dev(struct activity *a)
{
	struct stats_pwr_usb *st_pwr_usb
		= (struct stats_pwr_usb *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read USB devices stats */
	do {
		nr_read = read_bus_usb_dev(st_pwr_usb, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_pwr_usb = (struct stats_pwr_usb *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read filesystem statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_filesystem(struct activity *a)
{
	struct stats_filesystem *st_filesystem
		= (struct stats_filesystem *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read filesystems from /etc/mtab */
	do {
		nr_read = read_filesystem(st_filesystem, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_filesystem = (struct stats_filesystem *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read Fibre Channel HBA statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_fchost(struct activity *a)
{
	struct stats_fchost *st_fc
		= (struct stats_fchost *) a->_buf0;
	__nr_t nr_read = 0;

	/* Read FC hosts statistics */
	do {
		nr_read = read_fchost(st_fc, a->nr_allocated);

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_fc = (struct stats_fchost *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Look for online CPU and fill corresponding bitmap.
 *
 * IN:
 * @bitmap_size	Size of the CPU bitmap.
 *
 * OUT:
 * @online_cpu_bitmap
 *		CPU bitmap which has been filled.
 *
 * RETURNS:
 * Number of CPU for which statistics have to be read.
 * 1 means CPU "all", 2 means CPU "all" and CPU 0, etc.
 * Or -1 if the buffer was too small and needs to be reallocated.
 ***************************************************************************
 */
int get_online_cpu_list(unsigned char online_cpu_bitmap[], int bitmap_size)
{
	FILE *fp;
	char line[8192];
	int proc_nr = -2;

	if ((fp = fopen(STAT, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "cpu ", 4))
			continue;

		if (!strncmp(line, "cpu", 3)) {
			sscanf(line + 3, "%d", &proc_nr);

			if ((proc_nr + 1 > bitmap_size) || (proc_nr < 0)) {
				fclose(fp);
				/* Return -1 or 0 */
				return ((proc_nr >= 0) * -1);
			}
			online_cpu_bitmap[proc_nr >> 3] |= 1 << (proc_nr & 0x07);
		}
	}

	fclose(fp);
	return proc_nr + 2;
}

/*
 ***************************************************************************
 * Read softnet statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_softnet(struct activity *a)
{
	struct stats_softnet *st_softnet
		= (struct stats_softnet *) a->_buf0;
	__nr_t nr_read = 0;
	static unsigned char *online_cpu_bitmap = NULL;
	static int bitmap_size = 0;

	/* Read softnet stats */
	do {
		/* Allocate bitmap for online CPU */
		if (bitmap_size < a->nr_allocated) {
			if ((online_cpu_bitmap = (unsigned char *) realloc(online_cpu_bitmap,
									   BITMAP_SIZE(a->nr_allocated))) == NULL) {
				nr_read = 0;
				break;
			}
			bitmap_size = a->nr_allocated;
		}
		memset(online_cpu_bitmap, 0, BITMAP_SIZE(a->nr_allocated));

		/* Get online CPU list */
		nr_read = get_online_cpu_list(online_cpu_bitmap, bitmap_size);

		if (nr_read > 0) {
			/* Read /proc/net/softnet stats */
			nr_read *= read_softnet(st_softnet, a->nr_allocated, online_cpu_bitmap);
		}

		if (nr_read < 0) {
			/* Buffer needs to be reallocated */
			st_softnet = (struct stats_softnet *) reallocate_buffer(a);
		}
	}
	while (nr_read < 0);

	a->_nr0 = nr_read;

	return;
}

/*
 ***************************************************************************
 * Read pressure-stall CPU statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_psicpu(struct activity *a)
{
	struct stats_psi_cpu *st_psicpu
		= (struct stats_psi_cpu *) a->_buf0;

	/* Read pressure-stall CPU stats */
	read_psicpu(st_psicpu);

	return;
}

/*
 ***************************************************************************
 * Read pressure-stall I/O statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_psiio(struct activity *a)
{
	struct stats_psi_io *st_psiio
		= (struct stats_psi_io *) a->_buf0;

	/* Read pressure-stall I/O stats */
	read_psiio(st_psiio);

	return;
}

/*
 ***************************************************************************
 * Read pressure-stall memory statistics.
 *
 * IN:
 * @a	Activity structure.
 *
 * OUT:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
__read_funct_t wrap_read_psimem(struct activity *a)
{
	struct stats_psi_mem *st_psimem
		= (struct stats_psi_mem *) a->_buf0;

	/* Read pressure-stall memory stats */
	read_psimem(st_psimem);

	return;
}

/*
 ***************************************************************************
 * Count number of interrupts that are in /proc/stat file.
 * Truncate the number of different individual interrupts to NR_IRQS.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of interrupts, including total number of interrupts.
 * Value in [0, NR_IRQS + 1].
 ***************************************************************************
 */
__nr_t wrap_get_irq_nr(struct activity *a)
{
	__nr_t n;

	if ((n = get_irq_nr()) > (a->bitmap->b_size + 1)) {
		n = a->bitmap->b_size + 1;
	}

	return n;
}

/*
 ***************************************************************************
 * Find number of serial lines that support tx/rx accounting
 * in /proc/tty/driver/serial file.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of serial lines supporting tx/rx accouting.
 * Number cannot exceed MAX_NR_SERIAL_LINES.
 ***************************************************************************
 */
__nr_t wrap_get_serial_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_serial_nr()) > 0) {
		if (n > MAX_NR_SERIAL_LINES)
			return MAX_NR_SERIAL_LINES;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Find number of interfaces (network devices) that are in /proc/net/dev
 * file.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of network interfaces. Number cannot exceed MAX_NR_IFACES.
 ***************************************************************************
 */
__nr_t wrap_get_iface_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_iface_nr()) > 0) {
		if (n > MAX_NR_IFACES)
			return MAX_NR_IFACES;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Compute number of CPU structures to allocate.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of structures (value in [1, NR_CPUS + 1]).
 * 1 means that there is only one proc and non SMP kernel (CPU "all").
 * 2 means one proc and SMP kernel (CPU "all" and CPU 0).
 * Etc.
 ***************************************************************************
 */
__nr_t wrap_get_cpu_nr(struct activity *a)
{
	return (get_cpu_nr(a->bitmap->b_size, FALSE) + 1);
}

/*
 ***************************************************************************
 * Get number of devices in /proc/diskstats.
 * Always done, since disk stats must be read at least for sar -b
 * if not for sar -d.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of devices. Number cannot exceed MAX_NR_DISKS.
 ***************************************************************************
 */
__nr_t wrap_get_disk_nr(struct activity *a)
{
	__nr_t n = 0;
	unsigned int f = COLLECT_PARTITIONS(a->opt_flags);

	if ((n = get_disk_nr(f)) > 0) {
		if (n > MAX_NR_DISKS)
			return MAX_NR_DISKS;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Get number of fan sensors.
 *
 * IN:
 * @a  Activity structure.
 *
 * RETURNS:
 * Number of fan sensors. Number cannot exceed MAX_NR_FANS.
 ***************************************************************************
 */
__nr_t wrap_get_fan_nr(struct activity *a)
{
	__nr_t n;

	if ((n = get_fan_nr()) > MAX_NR_FANS)
		return MAX_NR_FANS;
	else
		return n;
}

/*
 ***************************************************************************
 * Get number of temp sensors.
 *
 * IN:
 * @a  Activity structure.
 *
 * RETURNS:
 * Number of temp sensors. Number cannot exceed MAX_NR_TEMP_SENSORS.
 ***************************************************************************
 */
__nr_t wrap_get_temp_nr(struct activity *a)
{
	__nr_t n;

	if ((n = get_temp_nr()) > MAX_NR_TEMP_SENSORS)
		return MAX_NR_TEMP_SENSORS;
	else
		return n;
}

/*
 ***************************************************************************
 * Get number of voltage input sensors.
 *
 * IN:
 * @a  Activity structure.
 *
 * RETURNS:
 * Number of voltage input sensors. Number cannot exceed MAX_NR_IN_SENSORS.
 ***************************************************************************
 */
__nr_t wrap_get_in_nr(struct activity *a)
{
	__nr_t n;

	if ((n = get_in_nr()) > MAX_NR_IN_SENSORS)
		return MAX_NR_IN_SENSORS;
	else
		return n;
}

/*
 ***************************************************************************
 * Count number of possible frequencies for CPU#0.
 *
 * IN:
 * @a   Activity structure.
 *
 * RETURNS:
 * Number of CPU frequencies.
 ***************************************************************************
 */
__nr_t wrap_get_freq_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_freq_nr()) > 0)
		return n;

	return 0;
}

/*
 ***************************************************************************
 * Count number of USB devices plugged into the system.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of USB devices. Number cannot exceed MAX_NR_USB.
 ***************************************************************************
 */
__nr_t wrap_get_usb_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_usb_nr()) > 0) {
		if (n > MAX_NR_USB)
			return MAX_NR_USB;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Get number of mounted filesystems from /etc/mtab. Don't take into account
 * pseudo-filesystems.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of filesystems. Number cannot exceed MAX_NR_FS.
 ***************************************************************************
 */
__nr_t wrap_get_filesystem_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_filesystem_nr()) > 0) {
		if (n > MAX_NR_FS)
			return MAX_NR_FS;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Get number of FC hosts.
 *
 * IN:
 * @a	Activity structure.
 *
 * RETURNS:
 * Number of FC hosts. Number cannot exceed MAX_NR_FCHOSTS.
 ***************************************************************************
 */
__nr_t wrap_get_fchost_nr(struct activity *a)
{
	__nr_t n = 0;

	if ((n = get_fchost_nr()) > 0) {
		if (n > MAX_NR_FCHOSTS)
			return MAX_NR_FCHOSTS;
		else
			return n;
	}

	return 0;
}

/*
 ***************************************************************************
 * Check that /proc/pressure directory exists.
 *
 * IN:
 * @a  Activity structure.
 *
 * RETURNS:
 * TRUE if directory exists.
 ***************************************************************************
 */
__nr_t wrap_detect_psi(struct activity *a)
{
	return (check_dir(PRESSURE));
}
