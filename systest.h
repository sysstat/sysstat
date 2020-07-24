/*
 * sysstat: System performance tools for Linux
 * (C) 1999-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SYSTEST_H
#define _SYSTEST_H

#include <time.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#ifndef MINORBITS
#define MINORBITS	20
#endif
#define S_MAXMINOR	((1U << MINORBITS) - 1)
#define S_MAXMAJOR	((1U << (32 - MINORBITS)) - 1)

/* Test mode: Use alternate files and syscalls */
#ifdef TEST

#define PRE			"./tests/root"
#define __time(m)		get_unix_time(m)
#define __uname(m)		get_uname(m)
#define __statvfs(m,n)		get_fs_stat(m,n)
#define __getenv(m)		get_env_value(m)
#define __alarm(m)
#define __pause()		next_time_step()
#define __stat(m,n)		virtual_stat(m,n)
#define __opendir(m)		open_list(m)
#define __readdir(m)		read_list(m)
#define __closedir(m)		close_list(m)
#define __realpath(m,n)		get_realname(m,n)
#define __gettimeofday(m,n)	get_day_time(m)
#define __getpwuid(m)		get_usrname(m)
#define __fork(m)		get_known_pid(m)
#define __major(m)		(m >> MINORBITS)
#define __minor(m)		(m & S_MAXMINOR)

#define ROOTDIR		"./tests/root"
#define ROOTFILE	"root"
#define TESTDIR		"./tests"
#define VIRTUALHD	"./tests/root/dev/mapper/virtualhd"
#define _LIST		"_list"

#else

#define PRE	""

#define __time(m)		time(m)
#define __uname(m)		uname(m)
#define __statvfs(m,n)		statvfs(m,n)
#define __getenv(m)		getenv(m)
#define __alarm(m)		alarm(m)
#define __pause()		pause()
#define __stat(m,n)		stat(m,n)
#define __opendir(m)		opendir(m)
#define __readdir(m)		readdir(m)
#define __closedir(m)		closedir(m)
#define __realpath(m,n)		realpath(m,n)
#define __gettimeofday(m,n)	gettimeofday(m,n)
#define __getpwuid(m)		getpwuid(m)
#define __fork(m)		fork(m)
#define __major(m)		major(m)
#define __minor(m)		minor(m)

#endif

/*
 ***************************************************************************
 * Functions prototypes
 ***************************************************************************
 */
#ifdef TEST
void close_list
	(DIR *);
void get_day_time
	(struct timeval *);
char *get_env_value
	(const char *);
int get_fs_stat
	(char *, struct statvfs *);
pid_t get_known_pid
	(void);
char *get_realname
	(char *, char *);
void get_uname
	(struct utsname *);
time_t get_unix_time
	(time_t *);
struct passwd *get_usrname
	(uid_t);
void next_time_step
	(void);
DIR *open_list
	(const char *);
struct dirent *read_list
	(DIR *);
int virtual_stat
	(const char *, struct stat *);

void int_handler
	(int);
#endif

#endif  /* _SYSTEST_H */
