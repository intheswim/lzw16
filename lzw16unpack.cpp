/* Copyright (c) 1996-2021 Yuriy Yakimenko        */
/* This code is based on Mark Nelson's 1995 book. */

/**************************************************/
/*  LZW decompression program with full           */
/*  dictionary reset when filled up. Variable     */
/*  width codes up to 16 bits in output.          */   
/**************************************************/

#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <cstdint>
#include <mutex>

class LZWUnpacker
{
  private:
    uint16_t * suffix;
    uint16_t * prefix;
    uint16_t * stack;
    unsigned char * outline;
    unsigned char *buffer ;
    uint32_t CurBufferShift;
    int16_t RunningBits;
    uint16_t EOFCode;

    uint32_t MAX_BITS ;
    uint32_t HT_SIZE, HT_KEY_MASK, HT_CLEAR_CODE, HT_MAX_CODE; 

    static const int CLEAR_BYTE = 0xFF;
    static const int NOT_CODE = 0xFFFF;

    static const int INITIAL_BUFFER = 0x8000;

    mutable std::mutex _mtx;

  public:

  LZWUnpacker ()
  {
    buffer = NULL;
    CurBufferShift = 0;
    RunningBits = 0;
    EOFCode = 0;
    suffix = NULL;
    prefix = NULL;
    stack = NULL;
    outline = NULL;
  }
  ~LZWUnpacker ()
  {
    free (buffer);
    free (suffix);
    free (prefix);
    free (stack);
    free (outline);

    _mtx.unlock();
  }

  LZWUnpacker (const LZWUnpacker &) = delete;
  
  LZWUnpacker & operator=(const LZWUnpacker &) = delete;

  private:

  bool setupConsts (int bits)
  {
      if (bits < 9 || bits > SUPPORTED_MAX_BITS) 
        return false;

      MAX_BITS = bits;
      HT_SIZE = (1 << (bits + 1));
      HT_KEY_MASK = HT_SIZE - 1;
      HT_MAX_CODE = (1 << bits);
      HT_CLEAR_CODE = HT_MAX_CODE - 2;

      return true; 
  }

  bool initialAllocs (uint32_t buffer_size)
  {
      buffer = (unsigned char *)malloc ( buffer_size );
      suffix = (uint16_t *)malloc (HT_MAX_CODE * sizeof(uint16_t));
      prefix = (uint16_t *)malloc (HT_MAX_CODE * sizeof(uint16_t));
      stack = (uint16_t *)malloc (BUFFLEN * sizeof(uint16_t));

      outline = (unsigned char *)malloc(BUFFLEN);

      return (buffer && suffix && prefix && stack && outline);
  }

  int16_t GetPrefixChar(uint16_t code) const 
  {
    while (code >= 256)
    {
      assert (code < HT_MAX_CODE);

      code = prefix[code];
    }
    return code;
  }

  uint16_t GetCode ()
  {
    uint32_t val = *(uint32_t *)(buffer + (CurBufferShift >> 3));
    val >>= (CurBufferShift & 0x07);

    CurBufferShift += RunningBits;

    val &= (uint32_t)EOFCode;

    return (uint16_t)val;
  }

  public:

  int Decompress (const char *filename, const char *outfile, int flags)
  {
    int16_t i = 0;
    uint32_t len = 0;
    uint16_t RunCode = 256;
    uint32_t OldCode = NOT_CODE, CurPrefix;
    uint32_t code;
    uint32_t buffer_size = INITIAL_BUFFER;
    char label[4] = { 0 };
    uint8_t version = 255;
    uint32_t StackCount = 0;

    _mtx.lock(); // we want to allow calling Decompress only once, since it allocates memory, etc. for class instance.
                 // mutex is released in destructor when all memory is freed.

    if (is_big_endian())
    {
      fprintf (stderr, "Not supported on big endian machines.\n");
      return 0;
    }

    if (!(flags & OVERWRITE_FLAG) &&  file_exists(outfile))
    {
      // file exists and no overwrite flag set
      fprintf (stderr, "File \'%s\' already exists. Use overwrite flag.\n", outfile);
      return 0;
    }

    FILE *fp = fopen(filename, "rb");

    if (NULL == fp)
    {
      fprintf (stderr, "Cannot open file \'%s\'.\n", filename);
      fprintf (stderr, "%s\n", strerror(errno));
      return 0;
    }

    if (4 != fread(label, 1, 4, fp))
    {
      printf("Not LZW file!\n");
      fclose (fp);
      return 0;
    }

    if (memcmp(label, "LZW", 3) != 0)
    {
      printf("Not LZW file!\n");
      fclose (fp);
      return 0;
    }

    if (1 != fread (&version, 1, 1, fp))
    {
      fprintf(stderr, "Unexpected read error.\n");
      fclose (fp);
      return 0;
    }

    if (version != PACKER_VERSION)
    {
      fprintf(stderr, "Packer/unpacker version mismatch.\n");
      fclose (fp);
      return 0;
    }

    unsigned char infoBits = 0;

    infoBits |= (is_big_endian() ? 1 : 0);
    infoBits |= VARIABLE_WIDTH ? 2 : 0;

    // get infoFlags byte:
    unsigned char infoFlag = 0;

    if (1 != fread (&infoFlag, 1, 1, fp))
    {
      fprintf(stderr, "Unexpected read error.\n");
      fclose (fp);
      return 0;
    }

    // compare only last 4 bits. fiirst 4 bits have "number of bits".

    if ((infoBits & 0x0F) != (infoFlag & 0x0F))
    {
      fprintf(stderr, "Encoding flags mismatch.\n");
      fclose (fp);
      return 0;
    }

    int bits = 8 + (infoFlag >> 4);

    if (!setupConsts (bits))
    {
      fprintf(stderr, "Unsupported encoding.\n");
      fclose (fp);
      return 0;
    }

    // get expected output size:

    uint32_t expectedSize = 0;
    if (4 != fread (&expectedSize, 1, sizeof(uint32_t), fp))
    {
      fprintf(stderr, "Unexpected read error.\n");
      fclose (fp);
      return 0;
    }

    if (flags & VERBOSE_OUTPUT) 
      printf ("Expected output size: %ld.\n", (long)expectedSize);

    FILE *fout = fopen(outfile, "wb");

    if (NULL == fout)
    {
      fprintf (stderr, "Cannot open file \'%s\'\n", outfile);
      fprintf (stderr, "%s\n", strerror(errno));
      fclose (fp);
      return 0;
    }
    
    if (!initialAllocs ( buffer_size ))
    {
      fclose (fp);
      fclose (fout);
      fprintf (stderr, "Cannot allocate memory: %s\n", strerror ( errno ));
      return 0;
    }
    
    memset(prefix, CLEAR_BYTE, HT_SIZE);

    while (true)
    {
      unsigned char byte1 = 0, byte2 = 0;
      int rb = 0;
      rb += (int)fread (&byte1, 1, 1, fp);

      if (1 != rb)
      {
        fprintf (stderr, "Unexpected read error. Position: %ld\n", ftell(fp));

        fclose (fp);
        fclose (fout);
        return 0;
      }

      if (byte1 == 255)
      {
        if (4 != fread (&len, 1, 4, fp))
        {
          fprintf (stderr, "Unexpected read error. Position: %ld\n", ftell(fp));

          fclose (fp);
          fclose (fout);
          return 0;
        }

        if (buffer_size < len)
        {
          buffer_size = len;
          void *saved_ptr = buffer;
          buffer = (unsigned char *)realloc (buffer, len);
          
          if (!buffer)
          {
            fprintf (stderr, "Failed to reallocate memory: %s\n", strerror (errno));
            free (saved_ptr);
            fclose (fp);
            fclose (fout);
            return 0; 
          }
        }
      }
      else
      {
        if (1 != fread (&byte2, 1, 1, fp))
        {
          fprintf (stderr, "Unexpected read error. Position: %ld\n", ftell(fp));

          fclose (fp);
          fclose (fout);
          cleanup (outfile, flags);
          return 0;
        }

        len = byte2 + (byte1 << 8);
      }

      if ((size_t)len != fread(buffer, 1, len, fp))
      {
        fprintf (stderr, "Unexpected end of file reading %d bytes. Position: %ld\n", (int)len, ftell (fp));
        fclose (fp);
        fclose (fout);
        return 0;
      }
      else 
      {
        if (flags & DIAGNOSTIC_OUTPUT)
        { 
          printf ("Read %d bytes\n", (int)len);
        }
      }

      RunCode = 256;
      RunningBits = 9;
      EOFCode = 511;
      OldCode = NOT_CODE;
      CurBufferShift = 0;

      while (true)
      {
        code = GetCode();

        if (code == EOFCode)
        {
          if (i != (int)fwrite (outline, 1, i, fout))
          {
            // write error
            fclose (fout);
            fclose (fp);
            fprintf (stderr, "Write error. Out of disk space?\n");
            return 0;
          }

          fflush (fout);
          fclose (fp);

          // compare expected size with actual size.

          if (expectedSize != ftell (fout))
          {
            fprintf (stderr, "Expected and actual sizes dont match.\n");
            fclose (fout);
            return 0;
          }

          fclose(fout);

          return 1;
        }
        else if (code == HT_CLEAR_CODE)
        {
          memset(prefix, CLEAR_BYTE, HT_SIZE);
          break;
        }
        else
        {
          if (code < 256)
          {
            assert (i < BUFFLEN);
            outline[i++] = (uint8_t)code;
          }
          else
          {
            if (prefix[code] == NOT_CODE)
            {
              CurPrefix = OldCode;
              suffix[RunCode] = GetPrefixChar(OldCode);

              assert (StackCount < BUFFLEN);

              stack[StackCount++] = suffix[RunCode];
            }
            else
              CurPrefix = code;

            while (CurPrefix > 255)
            {
              assert (StackCount < BUFFLEN);

              assert (CurPrefix < HT_MAX_CODE);

              stack[StackCount++] = suffix[CurPrefix];
              CurPrefix = prefix[CurPrefix];
            }

            assert (StackCount < BUFFLEN);
  
            stack[StackCount++] = CurPrefix;

            while (StackCount != 0)
            {
              assert (i < BUFFLEN);
              outline[i++] = (uint8_t)stack[--StackCount];
            }
          }

          if ((OldCode != NOT_CODE))
          {
            prefix[RunCode] = OldCode;
            
            if (code != RunCode)
              suffix[RunCode] = GetPrefixChar(code);

            RunCode++;

            if (RunCode == EOFCode)
            {
              EOFCode = (EOFCode << 1) + 1;
              RunningBits++;

              if (flags & DIAGNOSTIC_OUTPUT)
              {
                printf ("new EOF: %d\n", EOFCode);
              }
            }
          }

          OldCode = code;

          if (i == BUFFLEN)
          {
            if (BUFFLEN != fwrite(outline, 1, BUFFLEN, fout))
            {
              // write error
              fclose (fout);
              fclose (fp);
              fprintf (stderr, "Write error. Out of disk space?\n");
              return 0;
            }

            i = 0;
            OldCode = NOT_CODE;
          }
        }
      }
    }
  }
}; // end of class

int Decompress (const char *filename, const char *outfile, int flags)
{
  LZWUnpacker unpacker;

  int ret = unpacker.Decompress (filename, outfile, flags);

  if (ret == 0)
  {
    cleanup (outfile, flags);
  }

  return ret;
}
