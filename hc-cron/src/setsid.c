#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int
setsid (void)
{
  int newpgrp;

  int fd;
  newpgrp = setpgid ((pid_t) 0, getpid ());
  if ((fd = open ("/dev/tty", 2)) >= 0)
    {
      (void) ioctl (fd, TIOCNOTTY, (char *) 0);
      (void) close (fd);
    }
  return newpgrp;
}
