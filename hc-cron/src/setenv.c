#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

int
setenv (char *name, char *value, int overwrite)
{
  char *tmp;
  int tmp_size;

  if (overwrite && getenv (name))
    return -1;

  tmp_size = strlen (name) + strlen (value) + 2;
  if (!(tmp = malloc (tmp_size)))
    {
      errno = ENOMEM;
      return -1;
    }

  /* boy, that was really broken... */
  snprintf (tmp, tmp_size, "%s=%s", name, value);
  return putenv (tmp);		/* intentionally orphan 'tmp' storage */
}
