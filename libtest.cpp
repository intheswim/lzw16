/* LZW (variable code length) static library test. 
 * Copyright (c) 2021 Yuriy Yakimenko
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include "export.h"

#include <chrono>
#include <iostream>

#include "common.h"

#if defined(__linux__)
#define LIBTEST_MAIN
#endif 

#ifdef LIBTEST_MAIN  // add LIBTEST #define to compile the test with static library

int main (int argc, char *argv[])
{
    int bits = DEFAULT_MAX_BITS;
    bool bits_set = false;

    char inputFile [PATH_MAX] = { 0 };

    char compressedFile [PATH_MAX] = { 0 };

    char outputFile [PATH_MAX] = { 0 };

    if (2 == argc)
    {
        strcpy_s (inputFile, PATH_MAX, argv[1]);
    } 
    else if (3 == argc)
    {
        if (0 == strncmp (argv[1], "-b", 2))
        {
            char *ptr = argv[1];
            bits = atoi (ptr + 2);

            bits_set = true;

            strcpy_s(inputFile, PATH_MAX, argv[2]);
        }
    }

    if (strlen (inputFile) == 0 || (bits < 9 || bits > SUPPORTED_MAX_BITS))
    {
        printf ("Usage: %s [-b{12-16}] fileToCompress\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf ("Testing compression on: %s\n", inputFile);

    tmpnam_s (compressedFile, sizeof(compressedFile));

    tmpnam_s (outputFile, sizeof(outputFile));

    assert (strlen(compressedFile) > 0 && strlen (outputFile) > 0);

    std::chrono::high_resolution_clock::time_point start;

    start = std::chrono::high_resolution_clock::now();

    int ret = 0;

    if (!bits_set)
        ret = Compress (inputFile, compressedFile, VERBOSE_OUTPUT);
    else 
        ret = Compress2 (inputFile, compressedFile, VERBOSE_OUTPUT, bits);

    printf ("Compression %s.\n", ret ? "successful" : "failed");

    if (!ret)
        return EXIT_FAILURE;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);    

    std::cout << duration.count() << " microsecs\n";

    start = std::chrono::high_resolution_clock::now();

    ret = Decompress (compressedFile, outputFile, VERBOSE_OUTPUT);

    printf ("Decompression %s.\n", ret ? "successful" : "failed");

    if (!ret)
        return EXIT_FAILURE;

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);    

    std::cout << duration.count() << " microsecs\n";

    remove (compressedFile);
    remove (outputFile);

    return EXIT_SUCCESS;
}

#endif // LIBTEST_MAIN
