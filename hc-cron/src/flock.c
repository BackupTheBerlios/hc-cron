#include <errno.h>
#include <sys/file.h>

/* The following flock() emulation snarfed intact *) from the HP-UX
 * "BSD to HP-UX porting tricks" maintained by
 * system@alchemy.chem.utoronto.ca (System Admin (Mike Peterson))
 * from the version "last updated: 11-Jan-1993"
 * Snarfage done by Jarkko Hietaniemi <Jarkko.Hietaniemi@hut.fi>
 * *) well, almost, had to K&R the function entry, HPUX "cc"
 * does not grok ANSI function prototypes */

/*
 * flock (fd, operation)
 *
 * This routine performs some file locking like the BSD 'flock'
 * on the object described by the int file descriptor 'fd',
 * which must already be open.
 *
 * The operations that are available are:
 *
 * LOCK_SH  -  get a shared lock.
 * LOCK_EX  -  get an exclusive lock.
 * LOCK_NB  -  don't block (must be ORed with LOCK_SH or LOCK_EX).
 * LOCK_UN  -  release a lock.
 *
 * Return value: 0 if lock successful, -1 if failed.
 *
 * Note that whether the locks are enforced or advisory is
 * controlled by the presence or absence of the SETGID bit on
 * the executable.
 *
 * Note that there is no difference between shared and exclusive
 * locks, since the 'lockf' system call in SYSV doesn't make any
 * distinction.
 *
 * The file "<sys/file.h>" should be modified to contain the definitions
 * of the available operations, which must be added manually (see below
 * for the values).
 */

/* this code has been reformatted by vixie */

int
flock (int fd, int operation)
{
  int i;

  switch (operation)
    {
    case LOCK_SH:		/* get a shared lock */
    case LOCK_EX:		/* get an exclusive lock */
      i = lockf (fd, F_LOCK, 0);
      break;

    case LOCK_SH | LOCK_NB:	/* get a non-blocking shared lock */
    case LOCK_EX | LOCK_NB:	/* get a non-blocking exclusive lock */
      i = lockf (fd, F_TLOCK, 0);
      if (i == -1)
	if ((errno == EAGAIN) || (errno == EACCES))
	  errno = EWOULDBLOCK;
      break;

    case LOCK_UN:		/* unlock */
      i = lockf (fd, F_ULOCK, 0);
      break;

    default:			/* can't decipher operation */
      i = -1;
      errno = EINVAL;
      break;
    }

  return (i);
}
