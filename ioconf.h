/*
 * ioconf: ioconf configuration file handling code
 * Original code (C) 2004 by Red Hat (Charlie Bennett <ccb@redhat.com>)
 *
 * Modified and maintained by Sebastien GODARD (sysstat <at> orange.fr)
 */

#ifndef _IOCONF_H
#define _IOCONF_H

#include "sysconfig.h"

#define IOC_NAMELEN	32
#define IOC_DESCLEN	64
#define IOC_DEVLEN	48
#define IOC_LINESIZ	256
#define IOC_FMTLEN	16
#define IOC_XFMTLEN	(IOC_FMTLEN + IOC_NAMELEN + 3)

#ifndef MINORBITS
#define MINORBITS	20
#endif
#define IOC_MAXMINOR	((1U << MINORBITS) - 1)
#ifndef MAX_BLKDEV
/* #define MAX_BLKDEV	((1U << (32 - MINORBITS)) - 1) */
/* Use a lower value since this value is used to allocate arrays statically in ioconf.c */
#define MAX_BLKDEV	511
#endif

#define K_NODEV	"nodev"

#define IS_WHOLE(maj,min)	((min % ioconf[maj]->blkp->pcount) == 0)

/*
 * When is C going to get templates?
 */
#define IOC_ALLOC(P,TYPE,SIZE)			\
     do {					\
         if (P == NULL) {			\
              P = (TYPE *) malloc(SIZE);	\
              if (P == NULL) {			\
                   perror("malloc");		\
                   ioc_free();			\
                   goto free_and_return;	\
              }					\
         }					\
     }						\
     while (0)
/* That dummy while allows ';' on the line that invokes the macro... */


struct blk_config {
	char name[IOC_NAMELEN];	/* device basename */
	char cfmt[IOC_XFMTLEN];	/* controller format string */
	char dfmt[IOC_FMTLEN];	/* disk format string */
	char pfmt[IOC_FMTLEN + 2];	/* partition format string */
	/* ctrlno is in the ioc_entry */
	unsigned int ctrl_explicit;	/* use "cN" in name */
	unsigned int dcount;		/* number of devices handled by this major */
	unsigned int pcount;		/* partitions per device */
	char desc[IOC_DESCLEN];
	/* disk info unit # conversion function */
	char *(*cconv)(unsigned int);

	/* extension properties (all this for initrd?) */
	char ext_name[IOC_NAMELEN];
	unsigned int ext;		/* flag - this is an extension record */
	unsigned int ext_minor;		/* which minor does this apply to */
};

#define BLK_CONFIG_SIZE	(sizeof(struct blk_config))


struct ioc_entry {
	int live;			/* is this a Direct entry? */
	unsigned int ctrlno;		/* controller number */
	unsigned int basemajor;		/* Major number of the template */
	char *desc;			/* (dynamic) per-controller description */
	struct blk_config *blkp;	/* the real info, may be a shared ref */
};

#define IOC_ENTRY_SIZE	(sizeof(struct ioc_entry))


int ioc_iswhole
	(unsigned int, unsigned int);
char *ioc_name
	(unsigned int, unsigned int);
char *transform_devmapname
	(unsigned int, unsigned int);

#endif
