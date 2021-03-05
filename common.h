#pragma once

#include "export.h"

#ifdef _MSC_VER
#define PATH_MAX 260
#else 
#include <linux/limits.h>   // PATH_MAX
#include <stddef.h>         // size_t
#endif 

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#define PACKER_VERSION  0
#define VARIABLE_WIDTH  1
#define BUFFLEN         16384    /* the larger, the better for compression. This value must be the same for coder and decoder. */

#define DEFAULT_MAX_BITS    15
#define SUPPORTED_MAX_BITS  16


long fileSize (const char *filename);
bool is_big_endian(void);
void cleanup (const char *outfile, int flags);
bool file_exists (const char *filename);
char *str_dup (const char *s);

#ifndef _MSC_VER
int tmpnam_s(char* temp_name, size_t sizeInChars);
#endif 