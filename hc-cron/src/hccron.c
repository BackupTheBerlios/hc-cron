/* Copyright 1998 by Felix Braun
 * All rights reserved
 * 
 * This code is placed under the GNU General Public License. Please see the 
 * file COPYING for details.
 * 
 * This should go together with cron by Paul Vixie
 */
static char rcsid[] =
  "$Id: hccron.c,v 1.2 1999/11/12 16:18:50 fbraun Exp $";

#include <sys/stat.h>		/* for stat() and open() */
#include <unistd.h>		/* for stat() and close() */
#include <time.h>		/* for localtime() in build_cu_list */
#include <stdlib.h>		/* for malloc() and free() */
#include <sys/types.h>		/* these two for utime() in save_lastrun */
#include <utime.h>
#include <fcntl.h>		/* for open() */

#include "cron.h"

#define	MAX_MSGLEN	25

/* entry:	db	- initialized cron database
 * 		cul_head- (list_cu **)uninitialized
 * return:	-
 * action:	builds a chronologically ordered linked list of jobs that 
 *		occured between shutdown and reboot
 */
void
build_cu_list (cron_db * db, list_cu ** cul_head)
{
  struct stat lr;
  time_t reboot;
  time_t curstore;
  register time_t *current = &curstore;
  register struct tm *curtm;
  register user *u;
  register entry *e;
  char msg[MAX_MSGLEN];
  list_cu *cul_tail = NULL;
  unsigned int counter = 0;

  reboot = time (NULL);
  *cul_head = NULL;

  if (stat (LASTRUN_FILE, &lr) != OK)
    {
      Debug (DLOAD, ("could not find lastrun file!\n"));
    }
  else
    {
      Debug (DLOAD, ("read lastrun file.\n"));
      for (*current = lr.st_mtime; *current < reboot; *current += 60)
	{

	  /* This is quite inefficient on systems with many jobs/users, instead 
	   * we should extract a list of possible catch up jobs first.
	   * However, our main audience is home-computers with only a small
	   * amount of users, also big systems don't reset that often and
	   * this is executed only once for every startup. So here you go.
	   */
	  for (u = db->head; u != NULL; u = u->next)
	    {
	      for (e = u->crontab; e != NULL; e = e->next)
		{
		  if (e->flags & RUN_DOWNTIME)
		    {

		      /* This might be quite slow. However, if I need to write
		       * a routine that is intelligent enough for all the special
		       * cases (30/31 days per month, leap years, DST ...) it
		       * wouldn't be any faster. Anyway we aren't in a hurry.
		       */
		      curtm = localtime (current);

		      /* to be really clean I'd have to do this:

		         curtm->tm_min -= FIRST_MINUTE;
		         curtm->tm_hour -= FIRST_HOUR; 
		         curtm->tm_mday -= FIRST_DOM;
		         curtm->tm_mon = curtm->tm_mon +1 - FIRST_MONTH;
		         curtm->tm_wday -= FIRST_DOW;

		         * but as the optimizer doesn't recognize that all of these
		         * except FIRST_DOM are no-ops, and we are in a time-critical
		         * loop, I adjust only FIRST_DOM further down
		       */

		      if (bit_test (e->minute, curtm->tm_min)
			  && bit_test (e->hour, curtm->tm_hour)
			  && bit_test (e->month, curtm->tm_mon)
			  && (((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
			      ? (bit_test (e->dow, curtm->tm_wday)
				 && bit_test (e->dom,
					      (curtm->tm_mday -
					       FIRST_DOM)))
			      : (bit_test (e->dow, curtm->tm_wday)
				 || bit_test (e->dom,
					      (curtm->tm_mday - FIRST_DOM)))))
			{
			  Debug (DSCH, ("found catch-up job: %s\n", e->cmd));
			  counter++;
			  if (e->flags & RUN_DOWN_ONCE)
			    e->flags &= ~RUN_DOWNTIME;
			  if (*cul_head != NULL)
			    {
			      if (
				  (cul_tail->next =
				   malloc (sizeof (list_cu))))
				{
				  cul_tail = cul_tail->next;
				  cul_tail->next = NULL;
				  cul_tail->cul_u = u;
				  cul_tail->cul_e = e;
				  cul_tail->rtime = *current;
				}
			    }
			  else
			    {
			      if ((*cul_head = malloc (sizeof (list_cu))))
				{
				  cul_tail = *cul_head;
				  cul_tail->next = NULL;
				  cul_tail->cul_u = u;
				  cul_tail->cul_e = e;
				  cul_tail->rtime = *current;
				}
			    }
			}	/* if (bit_test) */
		    }		/* if (e->flags & RUN_DOWNTIME) */
		}		/* for e */
	    }			/* for u */
	}			/* for *current */
      snprintf (msg, MAX_MSGLEN, "%d jobs to catch up", counter);
      log_it ("CRON", getpid (), "STARTUP", msg);
    }				/* else */
}

/* entry:	cul - chronologically sorted catch up list
 * return:	list_cu * - pointer to new head of list
 * action:	schedules jobs and deletes them from the list
 */
list_cu *
run_cu_list (list_cu * cul)
{
  time_t cur;
  list_cu *tmp;

  cur = cul->rtime;
  do
    {
      if (!(cul->cul_e->flags & RUN_ONLY_IDLE))
	{
	  cul->cul_e->flags |= RUN_ONLY_IDLE | RUN_IDLE_ONCE;
	};
      job_add (cul->cul_e, cul->cul_u);
      tmp = cul->next;
      free (cul);
      cul = tmp;
    }
  while (cul && (cul->rtime <= cur));
  return (cul);
}

/* entry:	catch up list
 * return:	-
 * action:	saves the timestamp of the oldest job on the catch-up list
 * 		or the current time if catch up list is NULL
 */
void
save_lastrun (list_cu * cul)
{

  int file;
  struct utimbuf utbuf;

  if (cul)
    {
      utbuf.actime = cul->rtime;
      utbuf.modtime = cul->rtime;
    }
  else
    {
      utbuf.modtime = time (NULL);
      utbuf.actime = utbuf.modtime;
    }

  file = open (LASTRUN_FILE, (O_WRONLY | O_CREAT),
	       (S_IRUSR | S_IWUSR | S_IRGRP));
  if (file != -1)
    {
      close (file);
      utime (LASTRUN_FILE, &utbuf);
    }
}

RETSIGTYPE sigterm_handler (int x)
{
  log_close ();
  save_lastrun (CatchUpList);
  exit (OK_EXIT);
}
