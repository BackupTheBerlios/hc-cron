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

static char rcsid[] = "$Id: misc.c,v 1.5 2000/06/18 09:53:30 fbraun Exp $";

/* vix 26jan87 [RCS has the rest of the log]
 * vix 30dec86 [written]
 */

#include "cron.h"
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif


#if defined(LOG_DAEMON) && !defined(LOG_CRON)
#define LOG_CRON LOG_DAEMON
#endif

static int LogFD = ERR;

int
strcmp_until (char *left, char *right, int until)
{
  register int diff;

  while (*left && *left != until && *left == *right)
    {
      left++;
      right++;
    }

  if ((*left == '\0' || *left == until) &&
      (*right == '\0' || *right == until))
    {
      diff = 0;
    }
  else
    {
      diff = *left - *right;
    }

  return diff;
}


/* strdtb(s) - delete trailing blanks in string 's' and return new length
 */
int
strdtb (char *s)
{
  char *x = s;

  /* scan forward to the null
   */
  while (*x)
    x++;

  /* scan backward to either the first character before the string,
   * or the last non-blank in the string, whichever comes first.
   */
  do
    {
      x--;
    }
  while (x >= s && isspace (*x));

  /* one character beyond where we stopped above is where the null
   * goes.
   */
  *++x = '\0';

  /* the difference between the position of the null character and
   * the position of the first character of the string is the length.
   */
  return x - s;
}


int
set_debug_flags (char *flags)
{
  /* debug flags are of the form    flag[,flag ...]

   * if an error occurs, print a message to stdout and return FALSE.
   * otherwise return TRUE after setting ERROR_FLAGS.
   */

#if !DEBUGGING

  printf ("this program was compiled without debugging enabled\n");
  return FALSE;

#else /* DEBUGGING */

  char *pc = flags;

  DebugFlags = 0;

  while (*pc)
    {
      char **test;
      int mask;

      /* try to find debug flag name in our list.
       */
      for (test = DebugFlagNames, mask = 1;
	   *test && strcmp_until (*test, pc, ','); test++, mask <<= 1)
	;

      if (!*test)
	{
	  fprintf (stderr, "unrecognized debug flag <%s> <%s>\n", flags, pc);
	  return FALSE;
	}

      DebugFlags |= mask;

      /* skip to the next flag
       */
      while (*pc && *pc != ',')
	pc++;
      if (*pc == ',')
	pc++;
    }

  if (DebugFlags)
    {
      int flag;

      fprintf (stderr, "debug flags enabled:");

      for (flag = 0; DebugFlagNames[flag]; flag++)
	if (DebugFlags & (1 << flag))
	  fprintf (stderr, " %s", DebugFlagNames[flag]);
      fprintf (stderr, "\n");
    }

  return TRUE;

#endif /* DEBUGGING */
}


void
set_cron_uid (void)
{
#if HAVE_SETEUID
#define SETEUID_FN seteuid
#define SETEUID_FN_NAME "seteuid"
#else
#define SETEUID_FN setuid
#define SETEUID_FN_NAME "setuid"
#endif

  if (SETEUID_FN (ROOT_UID) < OK)
    {
      fprintf (stderr, "Test of %s failed for uid %d.  Errno=%s(%d).\n",
	       SETEUID_FN_NAME, ROOT_UID, strerror (errno), errno);
      fprintf (stderr, "Cron needs to be able to set the effective userid\n"
	       "in order to run user cron jobs with the user's privileges\n");
      if (errno == EPERM)
	fprintf (stderr, "Cron must run as superuser to perform %s.\n",
		 SETEUID_FN_NAME);
      exit (ERROR_EXIT);
    }
}


void
set_cron_cwd (void)
{
  struct stat sb;

  /* first check for 'crondir' ("/var/spool" or some such)
   */
  if (stat (crondir, &sb) < OK && errno == ENOENT)
    {
      perror (crondir);
      if (OK == mkdir (crondir, 0700))
	{
	  fprintf (stderr, "%s: created\n", crondir);
	  stat (crondir, &sb);
	}
      else
	{
	  fprintf (stderr, "%s: ", crondir);
	  perror ("mkdir");
	  exit (ERROR_EXIT);
	}
    }
  if (!(sb.st_mode & S_IFDIR))
    {
      fprintf (stderr, "'%s' is not a directory, bailing out.\n", crondir);
      exit (ERROR_EXIT);
    }
  if (chdir (crondir) < OK)
    {
      fprintf (stderr, "cannot chdir(%s), bailing out.\n", crondir);
      perror (crondir);
      exit (ERROR_EXIT);
    }

  /* crondir okay (now==CWD), now look at spool_dir ("tabs" or some such)
   */
  if (stat (spool_dir, &sb) < OK && errno == ENOENT)
    {
      perror (spool_dir);
      if (OK == mkdir (spool_dir, 0700))
	{
	  fprintf (stderr, "%s: created\n", spool_dir);
	  stat (spool_dir, &sb);
	}
      else
	{
	  fprintf (stderr, "%s: ", spool_dir);
	  perror ("mkdir");
	  exit (ERROR_EXIT);
	}
    }
  if (!(sb.st_mode & S_IFDIR))
    {
      fprintf (stderr, "'%s' is not a directory, bailing out.\n", spool_dir);
      exit (ERROR_EXIT);
    }
}


/* acquire_daemonlock() - write our PID into /etc/cron.pid, unless
 *	another daemon is already running, which we detect here.
 *
 * note: main() calls us twice; once before forking, once after.
 *	we maintain static storage of the file pointer so that we
 *	can rewrite our PID into the 'pidfile' after the fork.
 *
 * it would be great if fflush() disassociated the file buffer.
 */
void
acquire_daemonlock (int closeflag)
{
  static FILE *fp = NULL;

  if (closeflag && fp)
    {
      fclose (fp);
      fp = NULL;
      return;
    }

  if (!fp)
    {
      char buf[MAX_TEMPSTR];
      int fd, otherpid;

      if ((-1 == (fd = open (pidfile, O_RDWR | O_CREAT, 0644)))
	  || (NULL == (fp = fdopen (fd, "r+"))))
	{
	  snprintf (buf, MAX_TEMPSTR, "can't open or create %s: %s",
		    pidfile, strerror (errno));
	  fprintf (stderr, "%s: %s\n", ProgramName, buf);
	  log_it ("CRON", getpid (), "DEATH", buf);
	  exit (ERROR_EXIT);
	}

      if (flock (fd, LOCK_EX | LOCK_NB) < OK)
	{
	  int save_errno = errno;

	  fscanf (fp, "%d", &otherpid);
	  snprintf (buf, MAX_TEMPSTR, "can't lock %s, otherpid may be %d: %s",
		    pidfile, otherpid, strerror (save_errno));
	  fprintf (stderr, "%s: %s\n", ProgramName, buf);
	  log_it ("CRON", getpid (), "DEATH", buf);
	  exit (ERROR_EXIT);
	}

      (void) fcntl (fd, F_SETFD, 1);
    }

  rewind (fp);
  fprintf (fp, "%d\n", getpid ());
  fflush (fp);
  (void) ftruncate (fileno (fp), ftell (fp));

  /* abandon fd and fp even though the file is open. we need to
   * keep it open and locked, but we don't need the handles elsewhere.
   */
}

/* get_char(file) : like getc() but increment LineNumber on newlines
 */
int
get_char (FILE * file)
{
  int ch;

  /* get a char but collapse all spaces and tabs to one space */
  ch = getc (file);
  if ((ch == ' ') || (ch == '\t'))
    {
      do
	{
	  ch = getc (file);
	}
      while ((ch == ' ') || (ch == '\t'));

      /* ch now holds the first non-whitespace character. Push that back
       * into the stream and set ch = ' '
       */
      ungetc (ch, file);
      ch = ' ';
    };

  if (ch == '\n')
    Set_LineNum (LineNumber + 1);
  return ch;
}


/* unget_char(ch, file) : like ungetc but do LineNumber processing
 */
void
unget_char (int ch, FILE * file)
{
  ungetc (ch, file);
  if (ch == '\n')
    Set_LineNum (LineNumber - 1);
}


/* get_string(str, max, file, termstr) : like fgets() but
 *		(1) has terminator string which should include \n
 *		(2) will always leave room for the null
 *		(3) uses get_char() so LineNumber will be accurate
 *		(4) returns EOF or terminating character, whichever
 */
int
get_string (char *string, int size, FILE * file, char *terms)
{
  int ch;

  while (EOF != (ch = get_char (file)) && !strchr (terms, ch))
    {
      if (size > 1)
	{
	  *string++ = (char) ch;
	  size--;
	}
    }

  if (size > 0)
    *string = '\0';

  return ch;
}


/* skip_comments(file) : read past comment (if any)
 */
void
skip_comments (FILE * file)
{
  int ch;

  while (EOF != (ch = get_char (file)))
    {
      /* ch is now the first character of a line.
       */

      while (ch == ' ' || ch == '\t')
	ch = get_char (file);

      if (ch == EOF)
	break;

      /* ch is now the first non-blank character of a line.
       */

      if (ch != '\n' && ch != '#')
	break;

      /* ch must be a newline or comment as first non-blank
       * character on a line.
       */

      while (ch != '\n' && ch != EOF)
	ch = get_char (file);

      /* ch is now the newline of a line which we're going to
       * ignore.
       */
    }
  if (ch != EOF)
    unget_char (ch, file);
}


/* int in_file(char *string, FILE *file)
 *	return TRUE if one of the lines in file matches string exactly,
 *	FALSE otherwise.
 */
static int
in_file (char *string, FILE * file)
{
  char line[MAX_TEMPSTR];

  rewind (file);
  while (fgets (line, MAX_TEMPSTR, file))
    {
      if (line[0] != '\0')
	line[strlen (line) - 1] = '\0';
      if (0 == strcmp (line, string))
	return TRUE;
    }
  return FALSE;
}


/* int allowed(char *username)
 *	returns TRUE if (allow_file exists and user is listed)
 *	or (deny_file exists and user is NOT listed)
 *	or (neither file exists but user=="root" so it's okay)
 */
int
allowed (char *username)
{
  static int init = FALSE;
  static FILE *allow, *deny;

  if (!init)
    {
      init = TRUE;
      if (allow_file && deny_file)
	{
	  allow = fopen (allow_file, "r");
	  deny = fopen (deny_file, "r");
	  Debug (DMISC, ("allow/deny enabled, %d/%d\n", !!allow, !!deny));
	}
      else
	{
	  allow = NULL;
	  deny = NULL;
	}
    }

  if (allow)
    return (in_file (username, allow));
  if (deny)
    return (!in_file (username, deny));

  if (allow_only_root)
    return (strcmp (username, ROOT_USER) == 0);
  else
    return TRUE;
}


void
log_it (char *username, int xpid, char *event, char *detail)
{
  pid_t pid = xpid;
  char *msg;
  time_t now = time ((time_t) 0);
  register struct tm *t = localtime (&now);
  int msg_size;
  static int syslog_open = 0;

  if (log_file)
    {
      /* we assume that MAX_TEMPSTR will hold the date, time, &punctuation.
       */
      msg_size =
	strlen (username) + strlen (event) + strlen (detail) + MAX_TEMPSTR;
      msg = malloc (msg_size);
      if (msg == NULL)
	{
	  /* damn, out of mem and we did not test that before... */
	  fprintf (stderr, "%s: Run OUT OF MEMORY while %s\n",
		   ProgramName, __FUNCTION__);
	  return;
	}
      if (LogFD < OK)
	{
	  LogFD = open (log_file, O_WRONLY | O_APPEND | O_CREAT, 0600);
	  if (LogFD < OK)
	    {
	      fprintf (stderr, "%s: can't open log file\n", ProgramName);
	      perror (log_file);
	    }
	  else
	    {
	      (void) fcntl (LogFD, F_SETFD, 1);
	    }
	}

      /* we have to snprintf() it because fprintf() doesn't always write
       * everything out in one chunk and this has to be atomically appended
       * to the log file.
       */
      snprintf (msg, msg_size, "%s (%02d/%02d-%02d:%02d:%02d-%d) %s (%s)\n",
		username,
		t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
		pid, event, detail);

      /* we have to run strlen() because snprintf() returns (char*) on old BSD
       */
      if (LogFD < OK || write (LogFD, msg, strlen (msg)) < OK)
	{
	  if (LogFD >= OK)
	    perror (log_file);
	  fprintf (stderr, "%s: can't write to log file\n", ProgramName);
	  write (STDERR, msg, strlen (msg));
	}

      free (msg);
    }

#ifdef HAVE_SYSLOG_H
  if (log_syslog)
    {
      if (!syslog_open)
	{
	  /* we don't use LOG_PID since the pid passed to us by
	   * our client may not be our own.  therefore we want to
	   * print the pid ourselves.
	   */
# ifdef LOG_DAEMON
	  openlog (ProgramName, LOG_PID, LOG_CRON);
# else
	  openlog (ProgramName, LOG_PID);
# endif
	  syslog_open = TRUE;	/* assume openlog success */
	}

      syslog (LOG_INFO, "(%s) %s (%s)\n", username, event, detail);
    }
#if DEBUGGING
  if (DebugFlags)
    {
      fprintf (stderr, "log_it: (%s %d) %s (%s)\n",
	       username, pid, event, detail);
    }
#endif
}
#endif /* HAVE_SYSLOG_H */


void
log_close (void)
{
  if (LogFD != ERR)
    {
      close (LogFD);
      LogFD = ERR;
    }
}


/* two warnings:
 *	(1) this routine is fairly slow
 *	(2) it returns a pointer to static storage
 */
char *
first_word (register char *s, register char *t)
{
  static char retbuf[2][MAX_TEMPSTR + 1];	/* sure wish C had GC */
  static int retsel = 0;
  register char *rb, *rp;

  /* select a return buffer */
  retsel = 1 - retsel;
  rb = &retbuf[retsel][0];
  rp = rb;

  /* skip any leading terminators */
  while (*s && (NULL != strchr (t, *s)))
    {
      s++;
    }

  /* copy until next terminator or full buffer */
  while (*s && (NULL == strchr (t, *s)) && (rp < &rb[MAX_TEMPSTR]))
    {
      *rp++ = *s++;
    }

  /* finish the return-string and return it */
  *rp = '\0';
  return rb;
}


/* warning:
 *	heavily ascii-dependent.
 */
void
mkprint (register char *dst, register unsigned char *src, register int len)
{
  while (len-- > 0)
    {
      register unsigned char ch = *src++;

      if (ch < ' ')
	{			/* control character */
	  *dst++ = '^';
	  *dst++ = ch + '@';
	}
      else if (ch < 0177)
	{			/* printable */
	  *dst++ = ch;
	}
      else if (ch == 0177)
	{			/* delete/rubout */
	  *dst++ = '^';
	  *dst++ = '?';
	}
      else
	{			/* parity character */
	  /* well, the following snprintf is paranoid, but that will
	   * keep grep happy */
	  snprintf (dst, 5, "\\%03o", ch);
	  dst += 4;
	}
    }
  *dst = '\0';
}


/* warning:
 *	returns a pointer to malloc'd storage, you must call free yourself.
 */
char *
mkprints (register unsigned char *src, register unsigned int len)
{
  register char *dst = malloc (len * 4 + 1);

  mkprint (dst, src, len);

  return dst;
}


#ifdef MAIL_DATE
/* Sat, 27 Feb 93 11:44:51 CST
 * 123456789012345678901234567
 */
char *
arpadate (time_t * clock)
{
  time_t t = clock ? *clock : time (0L);
  struct tm *tm = localtime (&t);
  static char ret[30];		/* zone name might be >3 chars */

  (void) snprintf (ret, 30, "%s, %2d %s %2d %02d:%02d:%02d %s",
		   DowNames[tm->tm_wday],
		   tm->tm_mday,
		   MonthNames[tm->tm_mon],
		   tm->tm_year,
		   tm->tm_hour, tm->tm_min, tm->tm_sec, TZONE (*tm));
  return ret;
}
#endif /*MAIL_DATE */

#ifdef HAVE_SETREUID
int
swap_uids ()
{
  return setreuid (geteuid (), getuid ());
}

int
swap_uids_back ()
{
  return swap_uids ();
}
#else /*HAVE_SETREUID */
static int save_euid;
int
swap_uids ()
{
  save_euid = geteuid ();
  return seteuid (getuid ());
}

int
swap_uids_back ()
{
  return seteuid (save_euid);
}
#endif /*HAVE_SETREUID */
