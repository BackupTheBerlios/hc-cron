#include <ctype.h>
#define MkLower(ch)	(isupper(ch) ? tolower(ch) : ch)

int
strcasecmp (char *left, char *right)
{
  while (*left && (MkLower (*left) == MkLower (*right)))
    {
      left++;
      right++;
    }
  return MkLower (*left) - MkLower (*right);
}
