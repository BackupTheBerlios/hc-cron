#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

double
getloadavg (void)
{
#ifdef LOADAVG_FILE
  int file;
  char load[6];

  if ((file = open (LOADAVG_FILE, O_RDONLY)))
    {
      read (file, load, 5);
      load[5] = 0;
      Debug (DMISC, ("load average: %s", load));
      close (file);
      return strtod (load);
    }
  else
    {
      log_it ("CRON", getpid (), "could not open", LOADAVG_FILE);
      return 0;
    };
#else /* we have now way of getting the loadavg */
  return 0;
#endif /* LOADAVG_FILE */
}
