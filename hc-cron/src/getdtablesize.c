#include <limits.h>
#include <unistd.h>

int
getdtablesize (void)
{
#ifdef _SC_OPEN_MAX
  return sysconf (_SC_OPEN_MAX);
#else
  return _POSIX_OPEN_MAX;
#endif
}
