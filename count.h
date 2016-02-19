/*
 * count.h: Include file used to count items for which
 * statistics will be collected.
 * (C) 1999-2016 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _COUNT_H
#define _COUNT_H

#include "common.h"

/*
 ***************************************************************************
 * Prototypes for functions used to count number of items.
 ***************************************************************************
 */

int get_cpu_nr
	(unsigned int, int);
int get_irqcpu_nr
	(char *, int, int);
int get_diskstats_dev_nr
	(int, int);
int get_irq_nr
	(void);
int get_serial_nr
	(void);
int get_iface_nr
	(void);
int get_disk_nr
	(unsigned int);
int get_freq_nr
	(void);
int get_usb_nr
	(void);
int get_filesystem_nr
	(void);
int get_fchost_nr
	(void);

#endif /* _COUNT_H */
