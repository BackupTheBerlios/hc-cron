/* This file contains the one function: read_config.  See below. */

#include "cron.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



static void
add_token (char base[], char newtoken[], int base_size)
{

  /* Add this token to the end of base */
  if (strlen (base) == 0)
    strncpy (base, newtoken, base_size - 1);
  else
    {
      strncat (base, " ", base_size - 1 - strlen (base));
      strncat (base, newtoken, base_size - 1 - strlen (base));
    }
  base[base_size - 1] = '\0';
  if (strlen (base) >= base_size - 1)
    {
      fprintf (stderr,
	       "Mail command (MAILARGS) too long.  Maximum total length of "
	       "mail command format string is %d characters.\n",
	       base_size - 1);
      exit (ERROR_EXIT);
    }
}



void
read_config (const char config_file[], int *allow_only_root_p,
	     int *const log_syslog_p, char **const allow_file_p,
	     char **const deny_file_p, char **const crondir_p,
	     char **const spool_dir_p, char **const log_file_p,
	     char **const syscrontab_p, char **const lastrun_file_p,
	     char **const pidfile_p, char **const mailprog_p,
	     char **const mailargs_p)
{
/*----------------------------------------------------------------------------
   Read the configuration file named config_file[] and return the settings
   in it.  The strings are returned in a mixture of malloc'ed storage and
   text or read-only data storage.

   For any parameter not mentioned in the configuration file, return a 
   compiled-in default.

   If config_file[] is NULL, return all compiled-in defaults.

   If can't read configuration file or there's a syntax error in it,
   issue error message and exit the program with exit code ERROR_EXIT.
-----------------------------------------------------------------------------*/
  char ma_build[100];

#define MAX_CONFIG_FILE_LINE 200

  /* Set defaults */
#ifdef ALLOW_ONLY_ROOT
  *allow_only_root_p = 1;
#else
  *allow_only_root_p = 0;
#endif
#ifdef SYSLOG
  *log_syslog_p = 1;
#else
  *log_syslog_p = 0;
#endif
#ifdef ALLOW_FILE
  *allow_file_p = ALLOW_FILE;
#else
  *allow_file_p = NULL;
#endif
#ifdef DENY_FILE
  *deny_file_p = DENY_FILE;
#else
  *deny_file_p = NULL;
#endif
  *crondir_p = CRONDIR;
  *spool_dir_p = SPOOL_DIR;
#ifdef LOG_FILE
  *log_file_p = LOG_FILE;
#else
  *log_file_p = NULL;
#endif
  {
    char default_pidfile[MAX_FNAME + 1];
    snprintf (default_pidfile, MAX_FNAME, PIDFILE, PIDDIR);
    *pidfile_p = strdup (default_pidfile);
  }
  *syscrontab_p = SYSCRONTAB;
  *lastrun_file_p = LASTRUN_FILE;
  *mailprog_p = MAILCMD;
  *mailargs_p = MAILARGS;

  ma_build[0] = '\0';		/* initial value */

  if (config_file)
    {
      FILE *config;

      Debug (DCONF, ("Reading configuration file '%s'\n", config_file));
      config = fopen (config_file, "r");
      if (config == NULL)
	{
	  perror (config_file);
	  exit (ERROR_EXIT);
	}
      else
	{
	  while (!feof (config) && !ferror (config))
	    {
	      char line[MAX_CONFIG_FILE_LINE + 1 + 1];
	      char *retval;

	      retval = fgets (line, sizeof (line), config);
	      if (retval != NULL)
		{
		  Debug (DCONF, ("Read line: '%s'\n", line));
		  if (strlen (line) > MAX_CONFIG_FILE_LINE)
		    {
		      fprintf (stderr, "cron configuration file contains "
			       "line too long.  Maximum line length is %d\n"
			       "Bad line: %s...", MAX_CONFIG_FILE_LINE, line);
		      exit (ERROR_EXIT);
		    }
		  if (line[0] == '#')
		    {		/* ignore comment */
		    }
		  else
		    {
		      char keyword[200 + 1];
		      char value[200 + 1];
		      int tokens_read;

		      tokens_read = sscanf (line, " %[A-Za-z_] = %s",
					    keyword, value);

		      Debug (DCONF, ("sscanf found %d tokens.\n",
				     tokens_read));
		      if (tokens_read == 2)
			Debug (DCONF, ("keyword='%s' value='%s'\n",
				       keyword, value));

		      if (tokens_read == -1)
			{	/* ignore blank line */
			}
		      else if (tokens_read != 2)
			{
			  fprintf (stderr, "cron configuration file has "
				   "invalid syntax.  Line '%s' is not of the "
				   "form KEYWORD=VALUE.\n", line);
			  exit (ERROR_EXIT);
			}
		      else if (strcmp (keyword, "ALLOW_ONLY_ROOT") == 0)
			{
			  *allow_only_root_p = atoi (value);
			}
		      else if (strcmp (keyword, "SYSLOG") == 0)
			{
			  *log_syslog_p = atoi (value);
			}
		      else if (strcmp (keyword, "ALLOW_FILE") == 0)
			{
			  if (strcmp (value, "NONE") == 0)
			    *allow_file_p = NULL;
			  else
			    *allow_file_p = strdup (value);
			}
		      else if (strcmp (keyword, "DENY_FILE") == 0)
			{
			  if (strcmp (value, "NONE") == 0)
			    *deny_file_p = NULL;
			  else
			    *deny_file_p = strdup (value);
			}
		      else if (strcmp (keyword, "CRONDIR") == 0)
			{
			  crondir = strdup (value);
			}
		      else if (strcmp (keyword, "SPOOL_DIR") == 0)
			{
			  spool_dir = strdup (value);
			}
		      else if (strcmp (keyword, "LOG_FILE") == 0)
			{
			  if (strcmp (value, "NONE") == 0)
			    *log_file_p = NULL;
			  else
			    *log_file_p = strdup (value);
			}
		      else if (strcmp (keyword, "PID_FILE") == 0)
			{
			  *pidfile_p = strdup (value);
			}
		      else if (strcmp (keyword, "SYSCRONTAB") == 0)
			{
			  *syscrontab_p = strdup (value);
			}
		      else if (strcmp (keyword, "LASTRUN_FILE") == 0)
			{
			  *lastrun_file_p = strdup (value);
			}
		      else if (strcmp (keyword, "MAILCMD") == 0)
			{
			  *mailprog_p = strdup (value);
			}
		      else if (strcmp (keyword, "MAILARGS") == 0)
			{
			  add_token (ma_build, value, sizeof (ma_build));
			}
		      else
			{
			  fprintf (stderr, "Unrecognized keyword set in "
				   "configuration file '%s': '%s'.\n"
				   "Should be something like 'SYSLOG' or "
				   "'SPOOL_DIR'.\n", config_file, keyword);
			  exit (ERROR_EXIT);
			}
		    }
		}
	    }
	  if (ferror (config))
	    {
	      perror (config_file);
	      exit (ERROR_EXIT);
	    }
	  fclose (config);
	}
      Debug (DCONF, ("Done with configuration file.\n"));
    }
  else
    {
      Debug (DCONF, ("No configuration file.  Using defaults.\n"));
    }

  if (strlen (ma_build) > 0)
    *mailargs_p = strdup (ma_build);

  Debug (DCONF, ("allow_only_root is '%d'\n", *allow_only_root_p));
  Debug (DCONF, ("log_syslog is '%d'\n", *log_syslog_p));
  Debug (DCONF, ("allow_file is '%s'\n", *allow_file_p));
  Debug (DCONF, ("deny_file is '%s'\n", *deny_file_p));
  Debug (DCONF, ("crondir is '%s'\n", *crondir_p));
  Debug (DCONF, ("spool_dir is '%s'\n", *spool_dir_p));
  Debug (DCONF, ("log_file is '%s'\n", *log_file_p));
  Debug (DCONF, ("syscrontab is '%s'\n", *syscrontab_p));
  Debug (DCONF, ("lastrun_file is '%s'\n", *lastrun_file_p));
  Debug (DCONF, ("pidfile is '%s'\n", *pidfile_p));
  Debug (DCONF, ("mailprog is '%s'\n", *mailprog_p));
  Debug (DCONF, ("mailargs is '%s'\n", *mailargs_p));
}
