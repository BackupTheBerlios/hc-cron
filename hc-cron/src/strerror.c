#include <stdlib.h>
#include <errno.h>

char *
strerror (int error)
{
  extern char *sys_errlist[];
  extern int sys_nerr;
  static char buf[32];

  if ((error <= sys_nerr) && (error > 0))
    {
      return sys_errlist[error];
    }

  snprintf (buf, 32, "Unknown error: %d", error);
  return buf;
}
