/*
 * count.h: Include file used to count items for which
 * statistics will be collected.
 * (C) 1999-2013 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _COUNT_H
#define _COUNT_H

#include "common.h"

/*
 ***************************************************************************
 * Prototypes for functions used to count number of items.
 ***************************************************************************
 */

extern int
	get_cpu_nr(unsigned int);
extern int
	get_irqcpu_nr(char *, int, int);
extern int
	get_diskstats_dev_nr(int, int);

extern int
	get_irq_nr(void);
extern int
	get_serial_nr(void);
extern int
	get_iface_nr(void);
extern int
	get_disk_nr(unsigned int);
extern int
	get_freq_nr(void);
extern int
	get_usb_nr(void);
extern int
	get_filesystem_nr(void);

#endif /* _COUNT_H */
