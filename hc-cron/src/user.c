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

static char rcsid[] = "$Id: user.c,v 1.4 2000/06/18 09:53:31 fbraun Exp $";

/* vix 26jan87 [log is in RCS file]
 */


#include "cron.h"

void
free_user (user * u)
{
  entry *e, *ne;

  free (u->name);
  for (e = u->crontab; e != NULL; e = ne)
    {
      ne = e->next;
      free_entry (e);
    }
  free (u);
}


user *
load_user (int crontab_fd, struct passwd *pw,	/* NULL implies syscrontab */
	   char *name)
{
  char envstr[MAX_ENVSTR];
  FILE *file;
  user *u;
  entry *e;
  int status;
  char **envp;

  if (!(file = fdopen (crontab_fd, "r")))
    {
      perror ("fdopen on crontab_fd in load_user");
      return NULL;
    }

  Debug (DPARS, ("load_user()\n"));
  /* file is open.  build user entry, then read the crontab file.
   */
  u = (user *) malloc (sizeof (user));
  u->name = strdup (name);
  u->crontab = NULL;

  /* 
   * initialize environment.  This will be copied/augmented for each entry.
   * Lines in the crontab add to and override any settings here, in entry.c.
   * LOGNAME and USER are overidden in any case in entry.c.
   */
  if (pass_environment)
    {
      char envstr[MAX_ENVSTR + 1];
      envp = env_copy (environ);	/* Start with our own environment */
      /* Override our HOME with user's home directory per passwd file */
      snprintf (envstr, MAX_ENVSTR, "HOME=%s", pw->pw_dir);
      envp = env_set (envp, envstr);
    }
  else
    envp = env_init ();		/* Start with an empty environment */

  /*
   * load the crontab
   */
  while ((status = load_env (envstr, file)) >= OK)
    {
      switch (status)
	{
	case ERR:
	  free_user (u);
	  u = NULL;
	  goto done;
	case FALSE:
	  e = load_entry (file, NULL, pw, envp);
	  if (e)
	    {
	      e->next = u->crontab;
	      u->crontab = e;
	    }
	  break;
	case TRUE:
	  envp = env_set (envp, envstr);
	  break;
	}
    }

done:
  env_free (envp);
  fclose (file);
  Debug (DPARS, ("...load_user() done\n"));
  return u;
}
