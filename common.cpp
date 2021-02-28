#include "common.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdint.h>

#if defined(__linux__)
    /* Linux  */
#include <unistd.h> /* access */
#elif defined (_WIN32)
#include <io.h>
#endif


long fileSize (const char *filename)
{
  FILE *fp = fopen (filename, "rb");

  if (!fp) return -1;

  fseek (fp, 0, SEEK_END);

  long ret = ftell (fp);

  fclose (fp);

  return ret;
}

bool is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return (bint.c[0] == 1); 
}

void cleanup (const char *outfile, int flags)
{
  if (0 == (flags & KEEP_ON_ERROR))
  {
    if (file_exists (outfile))
      remove (outfile);
  }
}

bool file_exists (const char *filename)
{
#if defined(__linux__)
    return (access(filename, F_OK) == 0); 
#elif defined(_WIN32)
  return (_access(filename, 0) == 0);
#endif
}

char *str_dup (const char *s) /* strdup replacement. */
{
  size_t size = strlen (s) + 1;
  char *ret = (char *)malloc (size);

  if (!ret) return NULL;
  
  strcpy (ret, s);
  return ret;
}
