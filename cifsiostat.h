/*
 * cifsiostat: Report CIFS statistics
 * Copyright (C) 2010 Red Hat, Inc. All Rights Reserved
 * Written by Ivana Varekova <varekova@redhat.com>
 */

#ifndef _CIFSIOSTAT_H
#define _CIFSIOSTAT_H

#include "common.h"

#define CIFSSTATS  PRE "/proc/fs/cifs/Stats"

/* I_: cifsiostat - D_: Display - F_: Flag */
#define I_D_TIMESTAMP		0x001
#define I_D_KILOBYTES		0x002
#define I_D_MEGABYTES		0x004
#define I_D_OMIT_SINCE_BOOT	0x008
#define I_D_PRETTY		0x010
/* Unused			0x020 */
#define I_D_UNIT		0x040
/* Unused			0x080 */

#define DISPLAY_TIMESTAMP(m)	   (((m) & I_D_TIMESTAMP)        == I_D_TIMESTAMP)
#define DISPLAY_KILOBYTES(m)	   (((m) & I_D_KILOBYTES)        == I_D_KILOBYTES)
#define DISPLAY_MEGABYTES(m)	   (((m) & I_D_MEGABYTES)        == I_D_MEGABYTES)
#define DISPLAY_OMIT_SINCE_BOOT(m) (((m) & I_D_OMIT_SINCE_BOOT)  == I_D_OMIT_SINCE_BOOT)
#define DISPLAY_PRETTY(m)	   (((m) & I_D_PRETTY)           == I_D_PRETTY)
#define DISPLAY_UNIT(m)		   (((m) & I_D_UNIT)             == I_D_UNIT)

struct cifs_st {
	unsigned long long rd_bytes     __attribute__ ((aligned (8)));
	unsigned long long wr_bytes     __attribute__ ((packed));
	unsigned long long rd_ops       __attribute__ ((packed));
	unsigned long long wr_ops       __attribute__ ((packed));
	unsigned long long fopens       __attribute__ ((packed));
	unsigned long long fcloses      __attribute__ ((packed));
	unsigned long long fdeletes     __attribute__ ((packed));
};

#define CIFS_ST_SIZE	(sizeof(struct cifs_st))

struct io_cifs {
	char name[MAX_NAME_LEN];
	int exist;
	struct cifs_st *cifs_stats[2];
	struct io_cifs *next;
};

#define IO_CIFS_SIZE	(sizeof(struct io_cifs))

#endif  /* _CIFSIOSTAT_H */
