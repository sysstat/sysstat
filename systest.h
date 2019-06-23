/*
 * sysstat: System performance tools for Linux
 * (C) 1999-2019 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SYSTEST_H
#define _SYSTEST_H

#include <time.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

/* Test mode: Use alternate files and syscalls */
#ifdef TEST

#define PRE		"./tests/root"
#define __time(m)	get_unix_time(m)
#define __uname(m)	get_uname(m)
#define __statvfs(m,n)	get_fs_stat(m,n)
#define __getenv(m)	get_env_value(m)
#define __alarm(m)
#define __pause()	next_time_step()
#define __stat(m,n)	virtual_stat(m,n)

#define ROOTDIR		"./tests/root"
#define ROOTFILE	"root"
#define TESTDIR		"./tests"
#define VIRTUALHD	"./tests/root/dev/mapper/virtualhd"

#else

#define PRE	""

#define __time(m)	time(m)
#define __uname(m)	uname(m)
#define __statvfs(m,n)	statvfs(m,n)
#define __getenv(m)	getenv(m)
#define __alarm(m)	alarm(m)
#define __pause()	pause()
#define __stat(m,n)	stat(m,n)

#endif


/*
 ***************************************************************************
 * Functions prototypes
 ***************************************************************************
 */
#ifdef TEST
char *get_env_value
	(char *);
int get_fs_stat
	(char *, struct statvfs *);
void get_uname
	(struct utsname *);
void get_unix_time
	(time_t *);
void next_time_step
	(void);
int virtual_stat
	(const char *, struct stat *);

void int_handler
	(int);
#endif

#endif  /* _SYSTEST_H */
