/* Copyright 1988,1990,1993,1994 by Paul Vixie
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

static char rcsid[] = "$Id: cron.c,v 1.10 2001/03/10 19:26:15 Hazzl Exp $";

#define	MAIN_PROGRAM
#include "cron.h"
#include <sys/signal.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif

static void usage __P ((void)),
  run_reboot_jobs __P ((cron_db *)),
  cron_tick __P ((cron_db *)),
  cron_sync __P ((void)), cron_sleep __P ((void)),
#ifdef USE_SIGCHLD
  sigchld_handler __P ((int)),
#endif
  sighup_handler __P ((int)),
  parse_args __P ((int c, char *v[], int *const p, char **const f));


static void
usage (void)
{
  fprintf (stderr, "usage:  %s [-x debugflag[,...]]\n", ProgramName);
  exit (ERROR_EXIT);
}


int
main (int argc, char *argv[])
{
  cron_db database;
  char *config_file;		/*Name of our configuration file; NULL if none */
  struct sigaction my_sigaction;

  /* We need to put Program_Name in its own storage because later we will
   * modify argv[0] in an attempt to change the process name.
   */
  ProcessName = argv[0];
  ProgramName = strdup (argv[0]);

#if HAVE_SETLINEBUF
  setlinebuf (stdout);
  setlinebuf (stderr);
#endif /*HAVE_SETLINEBUF */

  parse_args (argc, argv, &pass_environment, &config_file);

  read_config (config_file, &allow_only_root, &log_syslog, &allow_file,
	       &deny_file, &crondir, &spool_dir, &log_file, &syscrontab,
	       &lastrun_file, &pidfile, &mailprog, &mailargs);

#ifdef USE_SIGCHLD
  memset (&my_sigaction, 0, sizeof (my_sigaction));
  my_sigaction.sa_handler = sigchld_handler;
  my_sigaction.sa_flags |= SA_NOCLDSTOP | SA_NODEFER;
  if (sigaction (SIGCHLD, &my_sigaction, NULL))
    perror ("sigaction");
#else
  (void) signal (SIGCHLD, SIG_IGN);
#endif

  memset (&my_sigaction, 0, sizeof (my_sigaction));
  my_sigaction.sa_handler = sigterm_handler;
  if (sigaction (SIGTERM, &my_sigaction, NULL))
    perror ("sigaction");

  memset (&my_sigaction, 0, sizeof (my_sigaction));
  my_sigaction.sa_handler = sighup_handler;
  if (sigaction (SIGHUP, &my_sigaction, NULL))
    perror ("sigaction");

  acquire_daemonlock (0);
  set_cron_uid ();
  set_cron_cwd ();

#if HAVE_SETENV
  if (!pass_environment)
    setenv ("PATH", _PATH_DEFPATH, 1);
#endif

  /* if there are no debug flags turned on, fork as a daemon should. */

#if DEBUGGING
  if (DebugFlags)
    {
#else
  if (0)
    {
#endif
      (void) fprintf (stderr, "[%d] cron started\n", getpid ());
    }
  else
    {
      switch (fork ())
	{
	case -1:
	  log_it ("CRON", getpid (), "DEATH", "can't fork");
	  exit (0);
	  break;
	case 0:
	  /* child process */
	  log_it ("CRON", getpid (), "STARTUP", "fork ok");
	  (void) setsid ();
	  break;
	default:
	  /* parent process should just die */
	  _exit (0);
	}
    }

  acquire_daemonlock (0);

  /* initialize waiting for busy disk */
  init_diskload ();

  database.head = NULL;
  database.tail = NULL;
  database.mtime = (time_t) 0;

  Debug (DMISC, ("about to load database"));
  load_database (&database);
  Debug (DMISC, ("about to build catch up list"));
  build_cu_list (&database, &CatchUpList);
  Debug (DMISC, ("about to run reboot jobs"));
  run_reboot_jobs (&database);

  while (TRUE)
    {
      cron_sync (); 
# if DEBUGGING
      if (!(DebugFlags & DTEST))
# endif	 /*DEBUGGING*/
	  cron_sleep ();

      load_database (&database);

      /* first catch up any jobs that are on the CatchUpList */
      if (CatchUpList && (!jhead))
	CatchUpList = run_cu_list (CatchUpList);
      /* then run the regular jobs for this minute */
      cron_tick (&database);
    }
}


static void
run_reboot_jobs (cron_db * db)
{
  register user *u;
  register entry *e;

  for (u = db->head; u != NULL; u = u->next)
    {
      for (e = u->crontab; e != NULL; e = e->next)
	{
	  if (e->flags & WHEN_REBOOT)
	    {
	      job_add (e, u);
	    }
	}
    }
  (void) job_runqueue ();
}


static void
cron_tick (cron_db * db)
{
  register struct tm *tm = localtime (&TargetTime);
  register int minute, hour, dom, month, dow;
  register user *u;
  register entry *e;

  /* make 0-based values out of these so we can use them as indicies
   */
  minute = tm->tm_min - FIRST_MINUTE;
  hour = tm->tm_hour - FIRST_HOUR;
  dom = tm->tm_mday - FIRST_DOM;
  month = tm->tm_mon + 1 /* 0..11 -> 1..12 */  - FIRST_MONTH;
  dow = tm->tm_wday - FIRST_DOW;

  Debug (DSCH, ("[%d] tick(%d,%d,%d,%d,%d)\n",
		getpid (), minute, hour, dom, month, dow));
  /* the dom/dow situation is odd.  '* * 1,15 * Sun' will run on the
   * first and fifteenth AND every Sunday;  '* * * * Sun' will run *only*
   * on Sundays;  '* * 1,15 * *' will run *only* the 1st and 15th.  this
   * is why we keep 'e->dow_star' and 'e->dom_star'.  yes, it's bizarre.
   * like many bizarre things, it's the standard.
   */
  for (u = db->head; u != NULL; u = u->next)
    {
      for (e = u->crontab; e != NULL; e = e->next)
	{
	  Debug (DSCH | DEXT, ("user [%s:%d:%d:...] cmd=\"%s\"\n",
			       env_get ("LOGNAME", e->envp),
			       e->uid, e->gid, e->cmd));
	  if (bit_test (e->minute, minute)
	      && bit_test (e->hour, hour)
	      && bit_test (e->month, month)
	      && (((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
		  ? (bit_test (e->dow, dow) && bit_test (e->dom, dom))
		  : (bit_test (e->dow, dow) || bit_test (e->dom, dom))))
	    {
	      job_add (e, u);
	    }
	}
    }
}


/* the task here is to figure out how long it's going to be until :00 of the
 * following minute and initialize TargetTime to this value.  TargetTime
 * will subsequently slide 60 seconds at a time, with correction applied
 * implicitly in cron_sleep().  it would be nice to let cron execute in
 * the "current minute" before going to sleep, but by restarting cron you
 * could then get it to execute a given minute's jobs more than once.
 * instead we have the chance of missing a minute's jobs completely, but
 * that's something sysadmin's know to expect what with crashing computers..
 * 
 * Patch from <pererik@onedial.se>:
 *   Do cron_sync() before each cron_sleep(), to handle changes to the system
 *   time.
 */
static void
cron_sync (void)
{
  register struct tm *tm;

  TargetTime = time ((time_t *) 0);
  tm = localtime (&TargetTime);
  TargetTime += (60 - tm->tm_sec);
}


static void
cron_sleep (void)
{
  register int seconds_to_wait;

  do
    {
      seconds_to_wait = (int) (TargetTime - time ((time_t *) 0));
      Debug (DSCH, ("[%d] TargetTime=%ld, sec-to-wait=%d\n",
		    getpid (), TargetTime, seconds_to_wait));
      /* if we intend to sleep, this means that it's finally
       * time to empty the job queue (execute it).
       *
       * if we run any jobs, we'll probably screw up our timing,
       * so go recompute.
       *
       * note that we depend here on the left-to-right nature
       * of &&, and the short-circuiting.
       */
    }
  while (seconds_to_wait > 0 && job_runqueue ());

  while (seconds_to_wait > 0)
    {
      Debug (DSCH, ("[%d] sleeping for %d seconds\n",
		    getpid (), seconds_to_wait));
      seconds_to_wait = (int) sleep ((unsigned int) seconds_to_wait);
    }
}


#ifdef USE_SIGCHLD
static RETSIGTYPE
sigchld_handler (int x)
{
  WAIT_T waiter;
  pid_t pid;

  for (;;)
    {
#if 1
      pid = waitpid (-1, &waiter, WNOHANG);
#else
      pid = wait3 (&waiter, WNOHANG, (struct rusage *) 0);
#endif
      switch (pid)
	{
	case -1:
	  Debug (DPROC, ("[%d] sigchld...no children\n", getpid ()));
	  return;
	case 0:
	  Debug (DPROC, ("[%d] sigchld...no dead kids\n", getpid ()));
	  return;
	default:
	  Debug (DPROC,
		 ("[%d] sigchld...pid #%d died, stat=%d\n",
		  getpid (), pid, WEXITSTATUS (waiter)));
	  save_lastrun (CatchUpList);
	}
    }
}
#endif /*USE_SIGCHLD */


static RETSIGTYPE
sighup_handler (int x)
{
  log_close ();
}


static void
parse_args (int argc, char *argv[], int *const pass_environment_p,
	    char **const config_file_p)
{
  int argch;

  /*Set defaults */
  *pass_environment_p = 0;

#ifdef CONFIG_FILE
  *config_file_p = CONFIG_FILE;
#else
  *config_file_p = NULL;
#endif

  while (EOF != (argch = getopt (argc, argv, "ef:x:")))
    {
      switch (argch)
	{
	case 'f':
	  *config_file_p = optarg;
	  break;
	case 'e':
	  pass_environment = 1;
	  break;
	case 'x':
	  if (!set_debug_flags (optarg))
	    usage ();
	  break;
	default:
	  usage ();
	}
    }
}
