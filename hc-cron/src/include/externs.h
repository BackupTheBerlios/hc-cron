/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#if STDC_HEADERS
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
#endif

#if HAVE_GRP_H
#include <grp.h>
#endif

#if HAVE_VFORK_H
#include <vfork.h>
#endif

#define WAIT_T pid_t

#if HAVE_DIRENT_H
# include <dirent.h>
extern char *tzname[2];
# define TZONE(tm) tzname[(tm).tm_isdst]
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif


#if 0				/*!defined(POSIX) && !defined(ATT) */
/* classic BSD */
extern time_t time ();
extern unsigned sleep ();
extern struct tm *localtime ();
extern struct passwd *getpwnam ();
extern int errno;
extern void perror (), exit (), free ();
extern char *getenv (), *strcpy (), *strchr (), *strtok ();
extern void *malloc (), *realloc ();
#endif

#ifndef HAVE_STRCASECMP
extern int strcasecmp __P ((char *, char *));
#endif

#ifndef HAVE_STRDUP
extern char *strdup __P ((char *));
#endif

#ifndef HAVE_STRERROR
extern char *strerror __P ((int));
#endif

#ifndef HAVE_FLOCK
extern int flock __P ((int, int));
# define LOCK_SH 1
# define LOCK_EX 2
# define LOCK_NB 4
# define LOCK_UN 8
#endif

#ifndef HAVE_SETSID
extern int setsid __P ((void));
#endif

#ifndef HAVE_GETDTABLESIZE
extern int getdtablesize __P ((void));
#endif

#ifndef HAVE_SETENV
extern int setenv __P ((char *, char *, int));
#endif

#ifndef HAVE_GETLOADAVG
extern double getloadavg __P ((void));
#endif
