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

static char rcsid[] = "$Id: job.c,v 1.4 1999/12/19 16:48:32 fbraun Exp $";

#include "cron.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern job *jhead, *jtail;
extern int diskavg_file;
extern char diskavg_matrix[];

void
job_add (register entry * e, register user * u)
{
  register job *j;

  /* if already on queue, keep going */
  for (j = jhead; j; j = j->next)
    if (j->e == e && j->u == u)
      {
	return;
      }

  /* build a job queue element */
  if (!(j = (job *) malloc (sizeof (job))))
    {
      log_it ("CRON", getpid (), "no memory for job", e->cmd);
    }
  else
    {
      j->next = (job *) NULL;
      j->prev = (job *) NULL;
      j->e = e;
      j->u = u;

      /* add it to the tail */
      if (!jhead)
	{
	  jhead = j;
	}
      else
	{
	  jtail->next = j;
	  j->prev = jtail;
	}
      jtail = j;
    }
}


int
job_runqueue (void)
{
  job *j, *jn;
  int idle;
  int run = 0;

  idle = TRUE;
  if (getloadavg () > MAXLOADAVG)
     {
	idle = FALSE;
	Debug (DSCH, "The CPU is busy. Won't run RUN_ONLY_ILDE jobs");
     } 
   else 
     {
	if (get_diskload (diskavg_file, diskavg_matrix) > MAX_DISKLOAD)
	  {
	     idle = FALSE;
	     Debug (DSCH, "The harddisk is busy. Won't run RUN_ONLY_IDLE jobs");
	  };
     };

  for (j = jhead; j; j = jn)
    {
      jn = j->next;

      /* run command if we are idle
       * or
       * if the RUN_ONLY_IDLE flag is not set
       */
      if (idle || (!(j->e->flags & RUN_ONLY_IDLE)))
	{
	  do_command (j->e, j->u);
	  /* if we want to run idle only once (probably a catch up job)
	   * delete RUN_ONLY_IDLE flag
	   */
	  if (RUN_IDLE_ONCE & j->e->flags)
	    {
	      j->e->flags &= ~RUN_ONLY_IDLE;
	    };
	  if (jn)
	    {
	      jn->prev = j->prev;
	    }
	  else
	    {
	      jtail = j->prev;
	    }
	  if (j->prev)
	    {
	      j->prev->next = jn;
	    }
	  else
	    {
	      jhead = jn;
	    }
	  free (j);
	  run++;
	};
    }
  return run;
}
