#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

//#include "cron.h"
//#include "options.h"

#define BUF_OFFSET 544 // offset in /proc/stat where we start looking for disk_io
#define BUF_LEN 128 // max len of disk_io info
#define DISK_STRING "\ndisk_io: (3,0):("
#define DISK_STRING_LEN 16

extern int diskavg_file;
extern char diskavg_matrix[];

/* diskload.c - makes cron wait on startup for the disk to calm down before
 * starting any jobs. For this we read the number of disk irqs from /proc/stat.
 * 
 * Drawbacks of the current implementation: 
 *	- we just read the irqs for the load of the first installed disk, all 
 *	  otheres are silently ignored,
 */

static char rcsid[] =
  "$Id: diskload.c,v 1.8 2001/05/31 02:27:15 Hazzl Exp $";

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
  (void) memset (matrix,(int) len, 256);
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
  size_t pos;

  pos = str_len - 1;
  while (pos < buf_len)
    {
      if (matrix[(int) buffer[pos]])
	pos += matrix[(int) buffer[pos]];
      else
	{
	  if (memcmp (&buffer[pos - str_len], string, str_len))
	    pos++; /* pos += str_len is tempting but assumes that the last char
		    * occurs only once FIXME
		    * we could look for the last char instead in if and use the
		    * second to last occurence in the matrix */
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
 * 	   or -1 on any error. If called multiple times within one 
 *	   second, we return the same result each time.
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
  static unsigned int load_store = 0;
  unsigned int load_tmp;
  static int result = 0;

  time_tmp = time_store;
  if (time_tmp != time (&time_store))
    {
      if (lseek (fd, BUF_OFFSET, SEEK_SET) != BUF_OFFSET)
	return -1;
      if ( read (fd, buffer, BUF_LEN) == -1 )
	return -1;

      buffer[BUF_LEN] = 0;
      if ((found = search (buffer, BUF_LEN, DISK_STRING, DISK_STRING_LEN, matrix)))
	{
	  load_tmp = load_store;
	  load_store = (unsigned int) strtol (++found, NULL, 10);
	  result = (int) ((load_store - load_tmp) / (time_store - time_tmp));
	}
      else
	return -1;
    }
  return result;
}

void
init_diskload (void)
{
  init_search (DISK_STRING, diskavg_matrix);
  if ((diskavg_file = open ("/proc/stat", O_RDONLY)) == -1)
    {
      log_it ("CRON", getpid (), "STARTUP",
	      "could not open /proc/stat. No disk busy waiting");
      return;
    };
  /* we have to call get_diskload() here because the first invocation does
   * not return any meaningful results. 
   */
  (void) get_diskload (diskavg_file, diskavg_matrix);
}
