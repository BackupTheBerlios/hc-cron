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

static char rcsid[] = "$Id: crontab.c,v 1.5 2000/06/18 09:53:30 fbraun Exp $";

/* crontab - install and manage per-user crontab files
 * vix 02may87 [RCS has the rest of the log]
 * vix 26jan87 [original]
 */


#define	MAIN_PROGRAM
#include "cron.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef USE_UTIMES
# include <sys/time.h>
#else
# include <time.h>
# include <utime.h>
#endif
#if defined(POSIX)
# include <locale.h>
#endif

enum opt_t
{ opt_unknown, opt_list, opt_delete, opt_edit, opt_replace };

#if DEBUGGING
static char *Options[] = { "???", "list", "delete", "edit", "replace" };
#endif

static pid_t Pid;
static char User[MAX_UNAME], RealUser[MAX_UNAME];
static char Filename[MAX_FNAME];
static FILE *NewCrontab;
static int CheckErrorCount;
static enum opt_t Option;
static struct passwd *pw;
static void list_cmd __P ((void)),
  delete_cmd __P ((void)),
  edit_cmd __P ((void)),
  poke_daemon __P ((void)),
  check_error __P ((char *)),
  parse_args __P ((int c, char *v[], char **const f));
static int replace_cmd __P ((void));


static void
usage (char *msg)
{
  fprintf (stderr, "%s: usage error: %s\n", ProgramName, msg);
  fprintf (stderr, "usage:\t%s [-u user] file\n", ProgramName);
  fprintf (stderr, "\t%s [-u user] { -e | -l | -r }\n", ProgramName);
  fprintf (stderr, "\t\t(default operation is replace, per 1003.2)\n");
  fprintf (stderr, "\t-e\t(edit user's crontab)\n");
  fprintf (stderr, "\t-l\t(list user's crontab)\n");
  fprintf (stderr, "\t-r\t(delete user's crontab)\n");
  exit (ERROR_EXIT);
}


int
main (int argc, char *argv[])
{
  int exitstatus;
  char *config_file;		/* Name of our configuration file;  NULL if none */

  Pid = getpid ();
  ProgramName = argv[0];

#if defined(POSIX)
  setlocale (LC_ALL, "");
#endif

#if HAVE_SETLINEBUF
  setlinebuf (stderr);
#endif /*HAVE_SETLINEBUF */
  parse_args (argc, argv, &config_file);	/* sets many globals, opens a file */

  read_config (config_file, &allow_only_root, &log_syslog, &allow_file,
	       &deny_file, &crondir, &spool_dir, &log_file, &syscrontab,
	       &lastrun_file, &pidfile, &mailprog, &mailargs);

  set_cron_cwd ();
  if (!allowed (User))
    {
      fprintf (stderr,
	       "You (%s) are not allowed to use this program (%s)\n",
	       User, ProgramName);
      fprintf (stderr, "See crontab(1) for more information\n");
      log_it (RealUser, Pid, "AUTH", "crontab command not allowed");
      exit (ERROR_EXIT);
    }
  exitstatus = OK_EXIT;
  switch (Option)
    {
    case opt_list:
      list_cmd ();
      break;
    case opt_delete:
      delete_cmd ();
      break;
    case opt_edit:
      edit_cmd ();
      break;
    case opt_replace:
      if (replace_cmd () < 0)
	exitstatus = ERROR_EXIT;
      break;
    }
  exit (0);
 /*NOTREACHED*/}


static void
parse_args (int argc, char *argv[], char **const config_file_p)
{
  int argch;

#ifdef CONFIG_FILE
  *config_file_p = CONFIG_FILE;
#else
  *config_file_p = NULL;
#endif

  if (!(pw = getpwuid (getuid ())))
    {
      fprintf (stderr, "%s: your UID isn't in the passwd file.\n",
	       ProgramName);
      fprintf (stderr, "bailing out.\n");
      exit (ERROR_EXIT);
    }
  strcpy (User, pw->pw_name);
  strcpy (RealUser, User);
  Filename[0] = '\0';
  Option = opt_unknown;
  while (EOF != (argch = getopt (argc, argv, "u:lerx:")))
    {
      switch (argch)
	{
	case 'x':
	  if (!set_debug_flags (optarg))
	    usage ("bad debug option");
	  break;
	case 'u':
	  if (getuid () != ROOT_UID)
	    {
	      fprintf (stderr, "must be privileged to use -u\n");
	      exit (ERROR_EXIT);
	    }
	  if (!(pw = getpwnam (optarg)))
	    {
	      fprintf (stderr, "%s:  user `%s' unknown\n",
		       ProgramName, optarg);
	      exit (ERROR_EXIT);
	    }
	  (void) strcpy (User, optarg);
	  break;
	case 'l':
	  if (Option != opt_unknown)
	    usage ("only one operation permitted");
	  Option = opt_list;
	  break;
	case 'r':
	  if (Option != opt_unknown)
	    usage ("only one operation permitted");
	  Option = opt_delete;
	  break;
	case 'e':
	  if (Option != opt_unknown)
	    usage ("only one operation permitted");
	  Option = opt_edit;
	  break;
	default:
	  usage ("unrecognized option");
	}
    }

  endpwent ();

  if (Option != opt_unknown)
    {
      if (argv[optind] != NULL)
	{
	  usage ("no arguments permitted after this option");
	}
    }
  else
    {
      if (argv[optind] != NULL)
	{
	  Option = opt_replace;
	  (void) strncpy (Filename, argv[optind], sizeof (Filename) - 1);
	  Filename[sizeof (Filename) - 1] = '\0';
	}
      else
	{
	  usage ("file name must be specified for replace");
	}
    }

  if (Option == opt_replace)
    {
      /* we have to open the file here because we're going to
       * chdir(2) into /var/cron before we get around to
       * reading the file.
       */
      if (!strcmp (Filename, "-"))
	{
	  NewCrontab = stdin;
	}
      else
	{
	  /* relinquish the setuid status of the binary during
	   * the open, lest nonroot users read files they should
	   * not be able to read.  we can't use access() here
	   * since there's a race condition.  thanks go out to
	   * Arnt Gulbrandsen <agulbra@pvv.unit.no> for spotting
	   * the race.
	   */

	  if (swap_uids () < OK)
	    {
	      perror ("swapping uids");
	      exit (ERROR_EXIT);
	    }
	  if (!(NewCrontab = fopen (Filename, "r")))
	    {
	      perror (Filename);
	      exit (ERROR_EXIT);
	    }
	  if (swap_uids () < OK)
	    {
	      perror ("swapping uids back");
	      exit (ERROR_EXIT);
	    }
	}
    }

  Debug (DMISC, ("user=%s, file=%s, option=%s\n",
		 User, Filename, Options[(int) Option]));
}


static void
list_cmd (void)
{
  char n[MAX_FNAME];
  FILE *f;
  int ch;

  log_it (RealUser, Pid, "LIST", User);
  (void) snprintf (n, MAX_FNAME, CRON_TAB (User));
  if (!(f = fopen (n, "r")))
    {
      if (errno == ENOENT)
	fprintf (stderr, "no crontab for %s\n", User);
      else
	perror (n);
      exit (ERROR_EXIT);
    }

  /* file is open. copy to stdout, close.
   */
  Set_LineNum (1);
  while (EOF != (ch = get_char (f)))
    putchar (ch);
  fclose (f);
}


static void
delete_cmd (void)
{
  char n[MAX_FNAME];

  log_it (RealUser, Pid, "DELETE", User);
  (void) snprintf (n, MAX_FNAME, CRON_TAB (User));
  if (unlink (n))
    {
      if (errno == ENOENT)
	fprintf (stderr, "no crontab for %s\n", User);
      else
	perror (n);
      exit (ERROR_EXIT);
    }
  poke_daemon ();
}


static void
check_error (char *msg)
{
  CheckErrorCount++;
  fprintf (stderr, "\"%s\":%d: %s\n", Filename, LineNumber - 1, msg);
}


static void
remove_editor_file (const char Filename[])
{
  /* Delete the temporary file we created for the user to edit.  Because
     we made the user the owner, we may have to swap uids now to have the
     permission to unlink it (if we are setuid cron).
   */
  int rc;

  if (swap_uids () < OK)
    {
      perror ("swapping uids");
      exit (ERROR_EXIT);
    }
  rc = unlink (Filename);
  if (rc < 0)
    fprintf (stderr, "Warning: unable to delete temporary file %s.\n"
	     "errno = %s (%d).\n", Filename, strerror (errno), errno);
  if (swap_uids () < OK)
    {
      perror ("swapping uids back");
      exit (ERROR_EXIT);
    }
}



static void
edit_cmd (void)
{
  char n[MAX_FNAME], q[MAX_TEMPSTR], *editor;
  FILE *f;
  int t;
  struct stat statbuf;
  time_t mtime;
  WAIT_T waiter;
  pid_t pid, xpid;

  log_it (RealUser, Pid, "BEGIN EDIT", User);
  (void) snprintf (n, MAX_FNAME, CRON_TAB (User));
  if (!(f = fopen (n, "r")))
    {
      if (errno != ENOENT)
	{
	  perror (n);
	  exit (ERROR_EXIT);
	}
      fprintf (stderr, "no crontab for %s - using an empty one\n", User);
      if (!(f = fopen ("/dev/null", "r")))
	{
	  perror ("/dev/null");
	  exit (ERROR_EXIT);
	}
    }

  /* swap uids for the open so that the temporary file we create to
     edit will be properly owned so that our unprivileged child can
     edit it.  
   */

  if (swap_uids () < OK)
    {
      perror ("swapping uids");
      exit (ERROR_EXIT);
    }

  (void) snprintf (Filename, MAX_FNAME, "/tmp/crontab.%d", Pid);
  if (-1 == (t = open (Filename, O_CREAT | O_EXCL | O_RDWR, 0600)))
    {
      perror (Filename);
      goto fatal;
    }

  if (swap_uids () < OK)
    {
      perror ("swapping uids back");
      exit (ERROR_EXIT);
    }

  if (!(NewCrontab = fdopen (t, "r+")))
    {
      perror ("fdopen");
      goto fatal;
    }

  while (!feof (f) && !ferror (f))
    {
      char line[80 + 1];
      char *retval;

      retval = fgets (line, sizeof (line), f);
      if (retval != NULL)
	{
	  if (strncmp (line, "#CRONTAB", sizeof ("#CRONTAB") - 1) == 0)
	    {
	      /* We have a comment generated by this program, so ignore 
	         the entire line.
	       */
	      while (!feof (f) && !ferror (f)
		     && line[strlen (line) - 1] != '\n')
		fgets (line, sizeof (line), f);
	    }
	  else
	    {
	      /* It's a regular line; copy it over */
	      fputs (line, NewCrontab);
	    }
	}
    }

  fclose (f);
  if (fflush (NewCrontab) < OK)
    {
      perror (Filename);
      exit (ERROR_EXIT);
    }
again:
  rewind (NewCrontab);
  if (ferror (NewCrontab))
    {
      fprintf (stderr, "%s: error while writing new crontab to %s\n",
	       ProgramName, Filename);
    fatal:remove_editor_file (Filename);
      exit (ERROR_EXIT);
    }
  if (fstat (t, &statbuf) < 0)
    {
      perror ("fstat");
      goto fatal;
    }
  mtime = statbuf.st_mtime;

  if ((!(editor = getenv ("VISUAL"))) && (!(editor = getenv ("EDITOR"))))
    {
      editor = EDITOR;
    }

  /* we still have the file open.  editors will generally rewrite the
   * original file rather than renaming/unlinking it and starting a
   * new one; even backup files are supposed to be made by copying
   * rather than by renaming.  if some editor does not support this,
   * then don't use it.  the security problems are more severe if we
   * close and reopen the file around the edit.
   */

  switch (pid = fork ())
    {
    case -1:
      perror ("fork");
      goto fatal;
    case 0:
      /* child */
      if (setuid (getuid ()) < 0)
	{
	  perror ("setuid(getuid())");
	  exit (ERROR_EXIT);
	}
      if (chdir ("/tmp") < 0)
	{
	  perror ("chdir(/tmp)");
	  exit (ERROR_EXIT);
	}
      if (strlen (editor) + strlen (Filename) + 2 >= MAX_TEMPSTR)
	{
	  fprintf (stderr, "%s: editor or filename too long\n", ProgramName);
	  exit (ERROR_EXIT);
	}
      snprintf (q, MAX_TEMPSTR, "%s %s", editor, Filename);
      execlp (_PATH_BSHELL, _PATH_BSHELL, "-c", q, NULL);
      perror (editor);
      exit (ERROR_EXIT);
     /*NOTREACHED*/ default:
      /* parent */
      break;
    }

  /* parent */
  xpid = wait (&waiter);
  if (xpid != pid)
    {
      fprintf (stderr, "%s: wrong PID (%d != %d) from \"%s\"\n",
	       ProgramName, xpid, pid, editor);
      goto fatal;
    }
  if (WIFEXITED (waiter) && WEXITSTATUS (waiter))
    {
      fprintf (stderr, "%s: \"%s\" exited with status %d\n",
	       ProgramName, editor, WEXITSTATUS (waiter));
      goto fatal;
    }
  if (WIFSIGNALED (waiter))
    {
      fprintf (stderr,
	       "%s: \"%s\" killed; signal %d (%score dumped)\n",
	       ProgramName, editor, WTERMSIG (waiter),
	       WCOREDUMP (waiter) ? "" : "no ");
      goto fatal;
    }
  if (fstat (t, &statbuf) < 0)
    {
      perror ("fstat");
      goto fatal;
    }
  if (mtime == statbuf.st_mtime)
    {
      fprintf (stderr, "%s: no changes made to crontab\n", ProgramName);
    }
  else
    {
      fprintf (stderr, "%s: installing new crontab\n", ProgramName);
      switch (replace_cmd ())
	{
	case 0:
	  break;
	case -1:
	  for (;;)
	    {
	      printf ("Do you want to retry the same edit? ");
	      fflush (stdout);
	      q[0] = '\0';
	      (void) fgets (q, sizeof q, stdin);
	      switch (islower (q[0]) ? q[0] : tolower (q[0]))
		{
		case 'y':
		  goto again;
		case 'n':
		  goto abandon;
		default:
		  fprintf (stderr, "Enter Y or N\n");
		}
	    }
	 /*NOTREACHED*/ case -2:
	abandon:
	  fprintf (stderr, "%s: edits left in %s\n", ProgramName, Filename);
	  goto done;
	default:
	  fprintf (stderr, "%s: panic: bad switch() in replace_cmd()\n",
		   ProgramName);
	  goto fatal;
	}
    }
  remove_editor_file (Filename);
done:
  log_it (RealUser, Pid, "END EDIT", User);
}


static void
write_header_if_wanted (FILE * output, int *const n_header_lines_p)
{
/*----------------------------------------------------------------------------
   Unless the first line of file NewCrontab is #NODONTEDIT..., put the 
   "do not edit" warning and file generation info in file 'output'.
   In any case, return the number of lines we wrote to the file as
   *n_header_lines_p.
-----------------------------------------------------------------------------*/
  const time_t now = time (NULL);
  char line1[80 + 1];

  rewind (NewCrontab);
  fgets (line1, sizeof (line1), NewCrontab);
  if (!feof (NewCrontab) && !ferror (NewCrontab))
    {
      if (strncmp (line1, "#NODONTEDIT", sizeof ("#NODONTEDIT") - 1) != 0)
	{
	  fprintf (output, "#CRONTAB "
		   "DO NOT EDIT THIS FILE - "
		   "edit the master and reinstall.\n");
	  fprintf (output, "#CRONTAB "
		   "(%s installed by %s on %-24.24s)\n",
		   Filename, ProgramName, ctime (&now));
	  fprintf (output, "#CRONTAB " "(hc-cron version -- %s)\n", rcsid);
	  *n_header_lines_p = 3;	/* We just wrote 3 lines of header */
	}
      else
	*n_header_lines_p = 0;
    }
}


/* returns	0	on success
 *		-1	on syntax error
 *		-2	on install error
 */
static int
replace_cmd (void)
{
  char n[MAX_FNAME], envstr[MAX_ENVSTR], tn[MAX_FNAME];
  FILE *tmp;
  int ch, eof;
  entry *e;
  char **envp = env_init ();
  int n_header_lines;		/* Number of #CRONTAB header lines we write */

  (void) snprintf (n, MAX_FNAME, "tmp.%d", Pid);
  (void) snprintf (tn, MAX_FNAME, CRON_TAB (n));
  if (!(tmp = fopen (tn, "w+")))
    {
      fprintf (stderr, "Unable to create new crontab file '%s'.\n"
	       "fopen() failed with errno=%s (%d)\n",
	       tn, strerror (errno), errno);
      if (errno == EACCES)
	fprintf (stderr, "In order to have permission to create such files,\n"
		 "Crontab normally is installed setuid either superuser or \n"
		 "a user or group that owns the crontab files and "
		 "directories.\n");
      return (-2);
    }

  write_header_if_wanted (tmp, &n_header_lines);

  /* copy the crontab to the tmp
   */
  rewind (NewCrontab);
  Set_LineNum (1);
  while (EOF != (ch = get_char (NewCrontab)))
    putc (ch, tmp);
  ftruncate (fileno (tmp), ftell (tmp));
  fflush (tmp);
  rewind (tmp);

  if (ferror (tmp))
    {
      fprintf (stderr, "%s: error while writing new crontab to %s\n",
	       ProgramName, tn);
      fclose (tmp);
      unlink (tn);
      return (-2);
    }

  /* check the syntax of the file being installed.
   */

  /* BUG: was reporting errors after the EOF if there were any errors
   * in the file proper -- kludged it by stopping after first error.
   *              vix 31mar87
   */
  Set_LineNum (1 - n_header_lines);
  CheckErrorCount = 0;
  eof = FALSE;
  while (!CheckErrorCount && !eof)
    {
      switch (load_env (envstr, tmp))
	{
	case ERR:
	  eof = TRUE;
	  break;
	case FALSE:
	  e = load_entry (tmp, check_error, pw, envp);
	  if (e)
	    free (e);
	  break;
	case TRUE:
	  break;
	}
    }

  if (CheckErrorCount != 0)
    {
      fprintf (stderr, "errors in crontab file, can't install.\n");
      fclose (tmp);
      unlink (tn);
      return (-1);
    }

/* Why are we doing this?  We just created this file, so if we're privileged
   to do a chown(), the file's owner is already root.  (Maybe on some 
   systems the file doesn't automatically get the creator's effective uid?)
*/
#ifdef HAVE_FCHOWN
  if (fchown (fileno (tmp), ROOT_UID, -1) < OK)
#else
  if (chown (tn, ROOT_UID, -1) < OK)
#endif
    {
      if (errno == EPERM)
	{
	  /* This just means we aren't running as superuser.  We already
	     passed the test of having enough privilege to create the 
	     crontab file in the crontab directory, so the fact that we
	     aren't superuser just means the system administrator wants
	     the crontabs owned by the owner of Crontab, not root.  So
	     let it be.
	   */
	}
      else
	{
	  perror ("chown");
	  fclose (tmp);
	  unlink (tn);
	  return (-2);
	}
    }

#ifdef HAVE_FCHMOD
  if (fchmod (fileno (tmp), 0600) < OK)
#else
  if (chmod (tn, 0600) < OK)
#endif
    {
      perror ("chmod");
      fclose (tmp);
      unlink (tn);
      return (-2);
    }

  if (fclose (tmp) == EOF)
    {
      perror ("fclose");
      unlink (tn);
      return (-2);
    }

  (void) snprintf (n, sizeof (n), CRON_TAB (User));
  if (rename (tn, n))
    {
      fprintf (stderr, "%s: error renaming %s to %s\n", ProgramName, tn, n);
      perror ("rename");
      unlink (tn);
      return (-2);
    }
  log_it (RealUser, Pid, "REPLACE", User);

  poke_daemon ();

  return (0);
}


static void
poke_daemon (void)
{
#ifdef USE_UTIMES
  struct timeval tvs[2];
  struct timezone tz;

  (void) gettimeofday (&tvs[0], &tz);
  tvs[1] = tvs[0];
  if (utimes (SPOOL_DIR, tvs) < OK)
    {
      fprintf (stderr, "crontab: can't update mtime on spooldir\n");
      perror (SPOOL_DIR);
      return;
    }
#else
  if (utime (SPOOL_DIR, NULL) < OK)
    {
      fprintf (stderr, "crontab: can't update mtime on spooldir\n");
      perror (SPOOL_DIR);
      return;
    }
#endif /*USE_UTIMES */
}
