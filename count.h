/*
 * count.h: Include file used to count items for which
 * statistics will be collected.
 * (C) 1999-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _COUNT_H
#define _COUNT_H

#include "common.h"

/*
 ***************************************************************************
 * Prototypes for functions used to count number of items.
 ***************************************************************************
 */

__nr_t get_cpu_nr
	(unsigned int, int);
__nr_t get_irqcpu_nr
	(char *, int, int);
__nr_t get_diskstats_dev_nr
	(int, int);
__nr_t get_irq_nr
	(void);
__nr_t get_serial_nr
	(void);
__nr_t get_iface_nr
	(void);
__nr_t get_disk_nr
	(unsigned int);
__nr_t get_freq_nr
	(void);
__nr_t get_usb_nr
	(void);
__nr_t get_filesystem_nr
	(void);
__nr_t get_fchost_nr
	(void);

#endif /* _COUNT_H */
