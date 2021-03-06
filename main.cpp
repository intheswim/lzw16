/* LZW variable width up to 16-bit data compression implementation. 
 * Copyright (c) 1996-2021 Yuriy Yakimenko
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*------------------------------------------------------------*/
/*                                                            */ 
/*  Includes test option (Linux and Windows only).            */
/*  Uses cksum or certUtil to verify results.                 */
/*  Replace with custom cksum on other platforms.             */
/*                                                            */ 
/*------------------------------------------------------------*/

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ONE_KILOBYTE 1024

enum ByteSequence { SEQ_CONSTANT = 0, SEQ_INCREASING, SEQ_RANDOM };

enum ArgOption { PARSE_ERROR = -1, SYNTHETIC_TEST = 0, FLAG_PACK = 1, FLAG_UNPACK = 2, FLAG_TEST = 3 };

struct progArguments
{
    char *inputFile;
    char *outputFile;
    int flags;
    int bits, kb256;
    progArguments ()
    {
      inputFile = NULL;
      outputFile = NULL;
      flags = 0;
      bits = 0;
      kb256 = 0;
    }
    ~progArguments ()
    {
      free (inputFile);
      free (outputFile);
    }
};


static void show_command (const char* cmd) 
{
  char buffer[128];

#if defined(__linux__)
  FILE* pipe = popen(cmd, "r");
#elif defined(_WIN32)
  FILE* pipe = _popen(cmd, "r");
#endif
  
  if (!pipe) 
  {
    fprintf(stderr, "popen() failed!");
    return;
  }
    
  while (fgets(buffer, sizeof(buffer), pipe) != NULL) 
  {
    printf ("%s", buffer);    
  } 

#if defined(__linux__)
  pclose(pipe);
#elif defined(_WIN32)
  _pclose(pipe);
#endif 
}

static void printSyntax ()
{
  printf ("syntax: lzw10 -(p|u|t) [-v -f -k -t] [-bN] inputFile outputFile \n");
  printf ("        lzw10 -large [N] \n");
  printf ("\t -p - pack \n");
  printf ("\t -u - unpack \n");
  printf ("\t -v - verbose \n");
  printf ("\t -f - force overwrite; applicable with -u option only \n");
  printf ("\t -k - keep dirty/incomplete output file on failure \n");
  printf ("\t -t - test option; requires only inputFile \n");
  printf ("\t -bN - set maximum code bits. N from 12 to %d. Default is %d.\n", SUPPORTED_MAX_BITS, DEFAULT_MAX_BITS);
  printf ("\t -large - synthetic data test; N is size in 256 Kb units. Default N is 32.\n");
}

static enum ArgOption parseArguments (int argc, char *argv[], progArguments & params)
{
    int fileNameSet = false;

    int flagPack = 0;
    int flagUnpack = 0;
    int flagForce = 0;
    int flagVerbose = 0;
    int flagKeepDirty = 0;
    int flagTest = 0;
    int flagDiagnostics = 0;
    int bits = DEFAULT_MAX_BITS;

    bool bits_set = false;

    int i, j;

    char combined_flags[32] = { 0 };

    if (argc == 1)
    {
        return PARSE_ERROR;
    }

    for (i = 1; i < argc; i++)
    {
        if (strncmp (argv[i], "-", 1) == 0) 
        {
            if (fileNameSet)
            {
                return PARSE_ERROR;
            }

            if (strncmp (argv[i], "-b", 2) == 0)
            {
              /* the Compress and Decompress supports bit values from 9 to 15          */
              /* however values below 12 produce low compression and are not practical */

              if (strcmp (argv[i], "-b12") == 0) bits = 12;
              else if (strcmp (argv[i], "-b13") == 0) bits = 13;
              else if (strcmp (argv[i], "-b14") == 0) bits = 14;
              else if (strcmp (argv[i], "-b15") == 0) bits = 15;
              else if (strcmp (argv[i], "-b16") == 0) bits = 16;
              else 
              {
                fprintf (stderr, "Invalid number of bits. Allowed range 12 to %d.\n", SUPPORTED_MAX_BITS);    
                return PARSE_ERROR;
              }

              bits_set = true;

              continue;
            }

            if ((i == 1 || (i == 2 && bits_set)) && 0 == strcmp(argv[i], "-large"))
            {
                params.bits = DEFAULT_MAX_BITS;
                params.kb256 = 32; 

                if (bits_set && bits > 0)
                  params.bits = bits;

                if (i == argc - 2)
                {
                  int k = atoi(argv[i + 1]);

                  if (k > 0)
                  {
                    params.kb256 = k;
                  }
                }

                return SYNTHETIC_TEST; /* large test */
            }

            memset (combined_flags, 0, sizeof (combined_flags));

            strncpy (combined_flags, argv[i], sizeof (combined_flags) - 1);

            for (j = 1; combined_flags[j]; j++)
            {
                char flag = combined_flags[j];

                if (flag == 'p')
                {
                    flagPack = true;
                }
                else if (flag == 'u')
                {
                    flagUnpack = true;
                }
                else if (flag == 'f')
                {
                    flagForce = true;
                }
                else if (flag == 'v')
                {
                    flagVerbose = true;
                }
                else if (flag == 'k')
                {
                    flagKeepDirty = true;
                }
                else if (flag == 't')
                {
                    flagTest = true;
                }
                else if (flag == 'd')
                {
                    flagDiagnostics = true;
                }
                else 
                {
                    fprintf (stderr, "Unknown flag -%c\n", flag);
                    return PARSE_ERROR;
                }
            }
        }
        else /* file names */
        {
            fileNameSet = true;

            if (!params.inputFile)
            {
                params.inputFile = str_dup (argv[i]);
            }
            else if (!params.outputFile)
            {
                params.outputFile = str_dup (argv[i]);
            }
        } 
    }

    if (flagTest + flagPack + flagUnpack > 1) /* inconsistent args */
    {
        fprintf (stderr, "Cannot combine -p, -u and -t flags.\n");
        return PARSE_ERROR;
    }

    if (flagTest + flagPack + flagUnpack == 0) 
    {
        fprintf (stderr, "No pack, unpack or test flags given.\n");
        return PARSE_ERROR;
    }

    if (flagUnpack && bits_set)
    {
        fprintf (stderr, "Cannot cobine -u and -bit flag.\n");
        return PARSE_ERROR;
    }

    if (flagTest)
    {
      if (NULL == params.inputFile)
        return PARSE_ERROR;
    }
    else
    {
      if (NULL == params.inputFile || NULL == params.outputFile)
        return PARSE_ERROR;
    }

    if (flagForce) params.flags |= OVERWRITE_FLAG;
    if (flagVerbose) params.flags |= VERBOSE_OUTPUT;
    if (flagKeepDirty) params.flags |= KEEP_ON_ERROR;
    if (flagDiagnostics) params.flags |= DIAGNOSTIC_OUTPUT;

    params.bits = bits;
    
    ArgOption ret = PARSE_ERROR;

    if (flagTest) ret = FLAG_TEST;
    else if (flagPack) ret = FLAG_PACK;
    else if (flagUnpack) ret = FLAG_UNPACK;

    return ret;
}

static void run_cksum (const char *file)
{
  size_t size = strlen (file) + 1;

  char * command = (char *)malloc (size + 32);

  if (!command)
  { 
    perror ("Cannot allocate memory.");
    return;
  }

#if defined(__linux__)
  sprintf (command, "cksum %s", file); 
#elif defined(_WIN32)
  sprintf(command, "certutil -hashfile %s", file);
#endif
 
  show_command ( command );

  free (command);
}

/*--------------------------------------------------------------------*/
/* For testing purposes only */
/*--------------------------------------------------------------------*/

static int syntheticDataTest (int kilobytes256, int bits = DEFAULT_MAX_BITS, ByteSequence option = SEQ_CONSTANT)
{
  const char input [] = "synth.bin";
  const char packed_input[] = "synth.lzw";
  const char unpacked_input[] = "synth.out";

  if (true)
  { 
    FILE *fp = fopen (input, "w+b");

    const int size = ONE_KILOBYTE;

    char buffer[ONE_KILOBYTE];

    memset (buffer, 0x0A, sizeof(buffer));

    if (option == SEQ_INCREASING)
    {
      for (int i=0; i < size; i++) buffer[i] = (char)(i & 0xFF);
    }
    else if (option == SEQ_RANDOM)
    {
      for (int i=0; i < size; i++) buffer[i] = (char)(rand() & 0xFF);
    }

    for (int i=0; i < 256 * kilobytes256; i++)
    {
        fwrite (buffer, 1, sizeof(buffer), fp);
    }

    fclose (fp);
  } 

  if (0 == Compress2 (input, packed_input, VERBOSE_OUTPUT, bits))
  {
    printf ("Synthetic input compression failed.\n");
    return EXIT_FAILURE;
  }

  printf ("Synthetic input compression successful.\n");

  if (0 == Decompress(packed_input, unpacked_input, OVERWRITE_FLAG))
  {
    printf ("Synthetic input decompression failed.\n");
    return EXIT_FAILURE;
  }

  printf ("Synthetic input decompression successful.\n");

  /* compare two files by running cksum */

  run_cksum (input);
  run_cksum (unpacked_input);

  remove (unpacked_input);
  remove (packed_input);
  remove (input);

  return EXIT_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////

#ifndef LIBTEST_MAIN  // add LIBTEST #define to compile the test with static library

int main(int argc, char *argv[])
{
  progArguments params;

  enum ArgOption option = parseArguments (argc, argv, params);

  if (option == PARSE_ERROR)
  {
    printSyntax ();
    
    return EXIT_FAILURE;
  }

  else if (option == SYNTHETIC_TEST) /* synthetic test */
  {
    return syntheticDataTest (params.kb256, params.bits, SEQ_CONSTANT);
  }

  else if (option == FLAG_PACK)
  {
    if (0 == Compress2 (params.inputFile, params.outputFile, params.flags, params.bits))
    {
      printf ("Compression failed.\n");
      return EXIT_FAILURE;
    }
    else 
    {
      printf ("Compression successful.\n");
      return EXIT_SUCCESS;
    }
  }
  else if (option == FLAG_UNPACK)
  {
    if (0 == Decompress(params.inputFile, params.outputFile, params.flags))
    {
      printf ("Decompression failed.\n");
      return EXIT_FAILURE;
    }
    else 
    {
      printf ("Decompression successful.\n");
      return EXIT_SUCCESS;
    }
  }
  else if (option == FLAG_TEST)
  {
    char temp_name [PATH_MAX], out_name [PATH_MAX];

    tmpnam_s (temp_name, sizeof(temp_name));

    if (0 == Compress2 (params.inputFile, temp_name, params.flags, params.bits ))
    {
      printf ("Compression failed.\n");
      return EXIT_FAILURE;
    }
    else 
    {
      printf ("Compression successful.\n");
    }

    tmpnam_s (out_name, sizeof(out_name));

    if (0 == Decompress(temp_name, out_name, params.flags | OVERWRITE_FLAG))
    {
      printf ("Decompression failed.\n");
      return EXIT_FAILURE;
    }
    else 
    {
      printf ("Decompression successful.\n");
    }

    /* compare 2 files. */

    run_cksum (params.inputFile);
    run_cksum (out_name);

    remove (temp_name);
    remove (out_name);
  }
  
  return EXIT_SUCCESS;
}

#endif // LIBTEST_MAIN
