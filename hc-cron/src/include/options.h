/*
 * $Id: options.h,v 1.1 1999/10/16 17:57:28 fbraun Exp $
 */

#if HAVE_PATHS_H
# include <paths.h>
#endif

#ifndef CRONDIR
			/* CRONDIR is where crond(8) and crontab(1) both chdir
			 * to; SPOOL_DIR, ALLOW_FILE, DENY_FILE, and LOG_FILE
			 * are all relative to this directory.
			 */
#define CRONDIR		"/var/spool"
#endif

			/* SPOOLDIR is where the crontabs live.
			 * This directory will have its modtime updated
			 * whenever crontab(1) changes a crontab; this is
			 * the signal for crond(8) to look at each individual
			 * crontab file and reload those whose modtimes are
			 * newer than they were last time around (or which
			 * didn't exist last time around...)
			 */
#define SPOOL_DIR	"cron"

			/* undefining these turns off their features.  note
			 * that ALLOW_FILE and DENY_FILE must both be defined
			 * in order to enable the allow/deny code.  If neither
			 * LOG_FILE or SYSLOG is defined, we don't log.  If
			 * both are defined, we log both ways.
			 */

#define	ALLOW_FILE	"/etc/cron.allow"
#define DENY_FILE	"/etc/cron.deny"
			/* if ALLOW_FILE and DENY_FILE are not defined or are
			 * defined but neither exists, should crontab(1) be
			 * usable only by root?
			 */
/*#define ALLOW_ONLY_ROOT*/

#define LOG_FILE	"/var/log/cron"

	/* if you want to use syslog(3) instead of appending
			 * to CRONDIR/LOG_FILE (/var/cron/log, e.g.), define
			 * SYSLOG here.  Note that quite a bit of logging
			 * info is written, and that you probably don't want
			 * to use this on 4.2bsd since everything goes in
			 * /usr/spool/mqueue/syslog.  On 4.[34]bsd you can
			 * tell /etc/syslog.conf to send cron's logging to
			 * a separate file.
			 *
			 * Note that if this and LOG_FILE are both defined, 
	 		 * then logging will go to both
			 * places.
			 */
/*#define SYSLOG */

		/* this file keeps track of when cron was last run so
			 * that cron can check for jobs it should have started
			 * during downtime. Added by hcl feb98
			 */
#define LASTRUN_FILE	"/var/lib/cron.lastrun"

			/* we get the system loadaverage from here.
			 * If not defined RUN_ONLY_IDLE is disabled.
			 */
#define LOADAVG_FILE	"/proc/loadavg"
   
  
  /* where should the daemon stick its PID? */
#ifdef _PATH_VARRUN
# define PIDDIR	_PATH_VARRUN
#else
# define PIDDIR "/etc/"
#endif
#define PIDFILE		"%scrond.pid"

			/* 4.3BSD-style crontab */
#define SYSCRONTAB	"/etc/crontab"

			/* what editor to use if no EDITOR or VISUAL
			 * environment variable specified.
			 */
#if defined(_PATH_VI)
# define EDITOR _PATH_VI
#else
# define EDITOR "/usr/ucb/vi"
#endif

#ifndef _PATH_BSHELL
# define _PATH_BSHELL "/bin/sh"
#endif

#ifndef _PATH_DEFPATH
# define _PATH_DEFPATH "/usr/bin:/bin"
#endif

/*
 * these are site-dependent
 */

#define MAXLOADAVG      .8  /* at which loadavg don't run IDLE jobs? */

#ifndef DEBUGGING
#define DEBUGGING 0	/* 1 or 0 -- do you want debugging code built in? */
#endif

			/*
			 * choose one of these MAILCMD commands. Use
			 * /bin/mail for speed; it makes biff bark but doesn't
			 * do aliasing.  /usr/lib/sendmail does aliasing but is
			 * a hog for short messages.  aliasing is not needed
			 * if you make use of the MAILTO= feature in crontabs.
			 * (hint: MAILTO= was added for this reason).
			 */
#define MAILCMD _PATH_SENDMAIL
#define MAILARGS "%s -FCronDaemon -odi -oem -- %s"
			/* -Fx	 = set full-name of sender
			 * -odi	 = Option Deliverymode Interactive
			 * -oem	 = Option Errors Mailedtosender
			 */

/* #define MAILCMD "/bin/mail" */
/* #define MAILARGS "%s -d  %s" */
			/* -d = undocumented but common flag: deliver locally?
			 */

/* #define MAILCMD "/usr/mmdf/bin/submit" */
/* #define MAILARGS "%s -mlrxto %s" */

/* #define MAIL_DATE */
			/* should we include an ersatz Date: header in
			 * generated mail?  if you are using sendmail
			 * for MAILCMD, it is better to let sendmail
			 * generate the Date: header.
			 */
