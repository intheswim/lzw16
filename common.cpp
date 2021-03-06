#include "common.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdint.h>
#include <errno.h>

#if defined(__linux__)
    /* Linux  */
#include <unistd.h> /* access */
#elif defined (_MSC_VER)
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

#ifndef _MSC_VER // MS compatibility adapters for "secure" functions.

int tmpnam_s(char* temp_name, size_t sizeInChars)
{
    char buffer [PATH_MAX] ;

    if (NULL == tmpnam(buffer))
        return -1; 

    if (strlen (buffer) >= sizeInChars)
    {
        return ERANGE;
    }

    if (temp_name == NULL)
    {
        return EINVAL;
    }

    strcpy (temp_name, buffer);

    return 0;
}

int strcpy_s(char *dest, size_t dest_len, const char *src)
{
     if (!src || !dest)
        return EINVAL;

    if (strlen (src) >= dest_len)
        return ERANGE;

    strcpy (dest, src);

    return 0;
}

#endif // _MSC_VER
