#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "options.h"

#define BUF_LEN 128

/* diskload.c - makes cron wait on startup for the disk to calm down before
 * starting any jobs. For this we read the number of disk irqs from /proc/stat.
 * 
 * Drawbacks of the current implementation: 
 *	- we just read the irqs for the load of the first installed disk, all 
 *	  otheres are silently ignored,
 * 	- we simply do a sleep wait before passing control to other functions
 * 	  non catch up jobs that occur while the disk is busy will be missed
 */

/* init_search - initializes a seach matrix
 * input: *s : 		string to be searched for
 * 	  *matrix : 	an array of exactly 256 bytes.
 * return: void
 * action: The array pointed to by *matrix will be filled
 * 	   so that it can be used for a search
 */
void
init_search (char *s, char *matrix)
{
  size_t len;
  len = strlen (s);
  memset (matrix, len, 256);
  do
    {
      matrix[(int) *(s++)] = --len;
    }
  while (len);
}


/* search - searches a string in a buffer
 * input: self explanatory names
 * return: pointer to the _last_ byte of the first ocurrence of *string in 
 *	   *buffer or NULL if not found
 */
char *
search (char *buffer,
	size_t buf_len, char *string, size_t str_len, char *matrix)
{
  size_t pos = 0;

  while (pos < buf_len)
    {
      if (matrix[(int) buffer[pos]])
	pos += matrix[(int) buffer[pos]];
      else
	{
	  if (memcmp (&buffer[pos - str_len + 1], string, str_len))
	    pos += str_len;
	  else
	    return &buffer[pos];
	}
    }
  return NULL;
}

/* getdiskload - calculates the average diskload since last call
 * input: fd:	file descriptor for /proc/stat
 * 	  *matrix: search matrix
 * return: average diskload since last call in no of interrupts/sec
 * 	   or -1 on any error or 0 if it is called twice within one second
 * Nota Bene: The first call to getdiskload() returns the average number
 *	      of disk interrups per second since Jan 1, 1970, normally 0.
 */
int
get_diskload (int fd, char *matrix)
{
  char buffer[BUF_LEN + 1];
  char *found;
  static time_t time_store = 0;
  time_t time_tmp;
  static unsigned int result_store = 0;
  unsigned int result_tmp;

  if (lseek (fd, 0, SEEK_SET))
    return -1;
  if (read (fd, buffer, BUF_LEN) != BUF_LEN)
    return -1;
  buffer[BUF_LEN] = 0;

  if ((found = search (buffer, BUF_LEN, "\ndisk ", 6, matrix)))
    {
      result_tmp = result_store;
      result_store = (unsigned int) strtol (found, NULL, 10);
      time_tmp = time_store;
      if (time_tmp == time (&time_store))
	return 0;
      return (int) ((result_store - result_tmp) / (time_store - time_tmp));
    };
  return -1;
}

void
wait_diskload (void)
{
  int da_file, load;
  char search[256];
  init_search ("\ndisk ", search);
  if ((da_file = open ("/proc/stat", O_RDONLY)) == -1)
    {
      log_it("CRON", getpid(), "STARTUP", "could not open /proc/stat. No disk busy waiting");
      return;
    };
   load = get_diskload (da_file, search);
   Debug(DMISC, ("diskload: %d irq/sec\n", load);
  while (load > MAX_DISKLOAD)
     {
	log_it("CRON", getpid(), "STARTUP", "waiting for busy disk");
	sleep(15);
	load = get_diskload(da_file, search);
	Debug(DMISC, ("diskload: %d irq/sec\n", load);
     };
  return;
}
