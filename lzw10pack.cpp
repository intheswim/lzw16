/* Copyright (c) 1996-2021 Yuriy Yakimenko        */
/* This code is based on Mark Nelson's 1995 book. */

/**************************************************/
/*  LZW compression program with full dictionary  */
/*  reset when filled up. Variable width  codes   */ 
/*  up to 15 bits in output.                      */   
/**************************************************/

#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <cstdint>

#include <mutex>

#define USE_STL_HASH

#ifdef USE_STL_HASH
#include <unordered_map>
#endif 

#ifndef USE_STL_HASH

#define MISSING_KEY       (0xFFFFFFFFL) 

#endif 

class LZWPacker
{
  private:

#ifdef USE_STL_HASH
    std::unordered_map<uint32_t,int16_t> table;
#else   
    uint32_t *table ;  // important note: hashtable has combination of value (last MAX_BITS and key which is OR combination of byte plus previous key).
                        // when MAX_BITS is 12, 32-bit row is enough (12 + 12 + 8 = 32).
                        // but when MAX_BITS is 13-16, we need extra space. This is why table (32-bit, for keys) + extra (16-bit, for codes) is used.
    uint16_t * extra;
#endif 

    uint32_t OUTLEN;
    uint32_t MAX_BITS ;

    uint32_t HT_SIZE, HT_KEY_MASK, HT_CLEAR_CODE, HT_MAX_CODE; 

    static const unsigned OUTPUT_INCREMENT = 4096;
    unsigned char * outline ;

    FILE *fp ;
    FILE *fout ;

    uint16_t RunCode ;
    int16_t RunningBits ;
    uint32_t CodeBuffer;
    int16_t CurBufferShift;
    uint16_t EOFCode ;
    bool verbose, diagnostics;

    mutable std::mutex _mtx;

  public:
  LZWPacker ()
  {
#ifndef USE_STL_HASH    
    table = NULL;
    extra = NULL;
#endif

    OUTLEN = OUTPUT_INCREMENT;

    outline = NULL;

    fp = NULL;
    fout = NULL;

    RunCode = 256;
    RunningBits = 9;
    CodeBuffer = 0;
    CurBufferShift = 0;
    EOFCode = 511;

    verbose = false;
    diagnostics = false;
  }
  ~LZWPacker ()
  {
    _mtx.unlock();
  }

  LZWPacker (const LZWPacker &) = delete;

  LZWPacker & operator=(const LZWPacker &) = delete;

  private:

  bool setupConsts (int bits)
  {
      if (bits < 8 || bits > 15) 
        return false;

      MAX_BITS = bits;
      HT_SIZE = (1 << (bits + 1));
      HT_KEY_MASK = HT_SIZE - 1;
      HT_MAX_CODE = (1 << bits);
      HT_CLEAR_CODE = HT_MAX_CODE - 2;

      return true; 
  }

#ifndef USE_STL_HASH
  uint32_t HT_GET_KEY(const uint32_t key) const
  {
    return table[key];
  }
#endif   

  void ClearHashTable (void)
  {
#ifdef USE_STL_HASH
    table.clear();
#else 
    memset(table, 0xFF, HT_SIZE * sizeof(uint32_t));
    memset (extra, 0xFF, HT_SIZE * sizeof(uint16_t));
#endif     
  }

  void DeleteHashTable (void)
  {
#ifndef USE_STL_HASH    
    free (table);
    table = NULL;
    free (extra);
    extra = NULL;
#endif     

    free (outline);
    outline = NULL;
  }

  int OutByte(const uint16_t code, uint32_t & len)
  {
    if (code == HT_CLEAR_CODE)
    {
      if (diagnostics)
      {
        printf ("Writing %d bytes\n", (int)len);
      }

      if ((len & 0x7FFF) == len) // fits in 15 bits
      {
        unsigned char byte = (len >> 8) & 0xFF;
        fwrite (&byte, 1, 1, fout);
        byte = len & 0xFF;
        fwrite (&byte, 1, 1, fout);
      }
      else 
      {
        unsigned char byte = 255;
        fwrite (&byte, 1, 1, fout);
        fwrite (&len, 1, 4, fout);
      }

      if (len != fwrite (outline, 1, len, fout))
      {
        fprintf (stderr, "Write error. Out of disk space? \n");
        return 0;
      }

      memset (outline, 0, len);

      len = 0;
    }
    else
    {
      if (len == OUTLEN)
      {
        OUTLEN += OUTPUT_INCREMENT;

        if (OUTLEN < len) // overflow
        {
          fprintf (stderr, "Length too large. Cannot proceed.\n");
          return 0;
        }

        if (diagnostics)
          printf ("reallocating outline to %d\n", OUTLEN);

        void *saved_ptr = outline;
        outline = (unsigned char *)realloc(outline, OUTLEN);
        
        if (NULL == outline)
        {
          fprintf (stderr, "Failed to reallocate memory: %s\n", strerror (errno));
          free (saved_ptr);

          return 0;
        }
      }

      outline[len++] = (uint8_t)code;
    }

    return 1;
  }

  int CompressCode(const uint16_t Code, uint32_t & len)
  {
    if (Code == HT_CLEAR_CODE)
    {
      CodeBuffer |= (((uint32_t)Code) << CurBufferShift);
      CurBufferShift += RunningBits;

      while (CurBufferShift > 0)
      {
        if (!OutByte(CodeBuffer & 0xFF, len))
          return 0;

        CodeBuffer >>= 8;
        CurBufferShift -= 8;
      }
      if (!OutByte(HT_CLEAR_CODE, len))
          return 0;

      CurBufferShift = 0;
    }
    else if (Code == EOFCode)
    {
      CodeBuffer |= (((uint32_t)Code) << CurBufferShift);
      CurBufferShift += RunningBits;
      while (CurBufferShift > 0)
      {
        if (!OutByte(CodeBuffer & 0xFF, len)) return 0;

        CodeBuffer >>= 8;
        CurBufferShift -= 8;
      }

      if (!OutByte(HT_CLEAR_CODE, len)) return 0;

      CurBufferShift = 0;
    }
    else
    {
      CodeBuffer |= (((uint32_t)Code) << CurBufferShift);
      CurBufferShift += RunningBits;
      while (CurBufferShift >= 8)
      {
        if (!OutByte(CodeBuffer & 0xFF, len)) return 0;
        CodeBuffer >>= 8;
        CurBufferShift -= 8;
      }
    }
    if (RunCode == EOFCode)
    {
      RunningBits++;
      EOFCode = (EOFCode << 1) + 1;
    }

    return 1;
  }

  bool InitHashTable (void)
  {
#ifdef USE_STL_HASH
    table.reserve (HT_SIZE);
#else     
    table = (uint32_t *)malloc(HT_SIZE * sizeof(uint32_t));
    extra = (uint16_t *)malloc(HT_SIZE * sizeof(uint16_t));
    
    if (table == NULL || extra == NULL)
      return false;
#endif       
    
    ClearHashTable();

    outline = (unsigned char *)malloc (OUTLEN);

    if (outline == NULL) return false;

    return true;
  }

#ifndef USE_STL_HASH
  uint32_t KeyItem(const uint32_t Item) const 
  {
    return ((Item >> MAX_BITS) ^ Item) & HT_KEY_MASK;
  }
#endif   

  void InsertHashTable (const uint32_t Key, const int16_t Code)
  {
#ifdef USE_STL_HASH
    table[Key] = Code;
#else     
    uint32_t HKey = KeyItem(Key);

    while (HT_GET_KEY(HKey) != MISSING_KEY)
      HKey = (HKey + 1) & HT_KEY_MASK;  

    table[HKey] = Key;
    extra[HKey] = Code;
#endif     
  }

  int16_t ExistHashTable (const uint32_t Key) const 
  {
#ifdef USE_STL_HASH
    auto it = table.find (Key);

    if (it == table.end()) 
    {
      return -1;
    }

    return it->second;
#else 
    uint32_t HKey = KeyItem(Key);
    uint32_t HTKey;

    while ((HTKey = HT_GET_KEY(HKey)) != MISSING_KEY)
    {
      if (Key == HTKey)
      {
        return extra[HKey];
      }

      HKey = (HKey + 1) & HT_KEY_MASK;
    }  

    return -1;
#endif     
  }

  public:

  int Compress(const char *filename, const char *outfile, int flags, int bits = DEFAULT_MAX_BITS)
  {
    unsigned char *buffer;
    uint16_t CurCode; 
    int16_t NewCode; // must be signed
    int32_t NewKey;
    int len, i;
    const char label[4] = "LZW";
    const uint8_t version = PACKER_VERSION;

    uint32_t out_pos = 0;

    _mtx.lock(); // we want to allow calling Compress only once, since it allocates memory, etc. for class instance.
                 // mutex is released in destructor when all memory is freed.

    if (!setupConsts (bits))
    {
      fprintf (stderr, "Invalid encoding.\n");
      return 0;
    }

    if (is_big_endian())
    {
      fprintf (stderr, "Not supported on big endian machines.\n");
      return 0;
    }

    fp = fopen(filename, "rb");
    
    if (NULL == fp)
    {
      fprintf (stderr, "Cannot open input file \'%s\'.\n", filename);
      fprintf (stderr, "%s\n", strerror(errno)); 
      return 0;
    }

    fout = fopen(outfile, "wb");

    if (NULL == fout)
    {
      fprintf (stderr, "Cannot open output file \'%s\'.\n", outfile);
      fprintf (stderr, "%s\n", strerror(errno));
      fclose (fp);
      return 0;
    }

    if (!InitHashTable())
    {
      fprintf(stderr, "Failed to allocate memory: %s\n", strerror (errno));
      fclose (fp);
      fclose (fout);
      return 0;
    }

    buffer = (unsigned char *)malloc(BUFFLEN);

    if (!buffer)
    {
      fclose (fp);
      fclose (fout);
      fprintf (stderr, "Cannot allocate memory: %s\n", strerror (errno));
      DeleteHashTable();
      return 0;
    }

    fwrite(label, 1, 4, fout);

    fwrite (&version, 1, 1, fout);

    unsigned char infoBits = 0;

    infoBits |= (is_big_endian() ? 1 : 0);
    infoBits |= VARIABLE_WIDTH ? 2 : 0;
    // leaving 2 bits reserved.
    infoBits |= ((MAX_BITS - 8) << 4); // we use left 4 bits for MAX_BITS information; can be between 8 and 23.

    fwrite (&infoBits, 1, 1, fout);

    // write size of input file.
    fseek (fp, 0, SEEK_END);
    uint32_t inputSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    fwrite (&inputSize, 1, sizeof(uint32_t), fout);

    bool compress_ok = true;

    verbose = (0 != (flags & VERBOSE_OUTPUT));
    diagnostics = (0 != (flags & DIAGNOSTIC_OUTPUT));

    while (compress_ok)
    {
      len = fread(buffer, 1, BUFFLEN, fp);
      if (len == 0)
        break;

      CurCode = *buffer;

      for (i = 1; i < len && compress_ok; i++)
      {
        NewKey = (((uint32_t)CurCode) << 8) + buffer[i];
        if ((NewCode = ExistHashTable(NewKey)) >= 0)
        {
          CurCode = NewCode;
        }
        else
        {
          if (!CompressCode(CurCode, out_pos))
          {
            compress_ok = false;
            break;
          }

          CurCode = buffer[i];
          if (RunCode == HT_CLEAR_CODE)
          {
            if (diagnostics)
              printf ("resetting (HT_CLEAR_CODE)\n");

            if (!CompressCode(HT_CLEAR_CODE, out_pos))
            {
              compress_ok = false;
              break;
            }

            ClearHashTable();
            RunCode = 256;
            RunningBits = 9;
            EOFCode = 511;
          }
          else
          {
            InsertHashTable(NewKey, RunCode++);
          }
        }
      }

      if (!CompressCode(CurCode, out_pos))
      {
        compress_ok = false;
      }
    }

    if (compress_ok)
    {
      CompressCode (EOFCode, out_pos);
      CompressCode (0, out_pos);
    }

    DeleteHashTable();

    free (buffer);
    fclose (fp);
    fclose (fout);

    return compress_ok ? 1 : 0;
  }
}; // end of class

int Compress(const char *filename, const char *outfile, int flags)
{
  LZWPacker packer;
  int ret = packer.Compress (filename, outfile, flags);

  if (ret == 0)
  {
    cleanup (outfile, flags);
  }

  else if (flags & VERBOSE_OUTPUT)
  {
    long orig_size = fileSize (filename);
    long compressed_size = fileSize (outfile);

    printf ("Compression ratio %.2f%%\n", 100.0 * compressed_size / orig_size);
  }

  return ret;
}

int Compress2 (const char *filename, const char *outfile, int flags, int max_bits)
{
  LZWPacker packer;

  if (flags & VERBOSE_OUTPUT)
  {
    printf ("Compression using max bits = %d\n", max_bits);
  }

  int ret = packer.Compress (filename, outfile, flags, max_bits);

  if (ret == 0)
  {
    cleanup (outfile, flags);
  }

  else if (flags & VERBOSE_OUTPUT)
  {
    long orig_size = fileSize (filename);
    long compressed_size = fileSize (outfile);

    printf ("Compression ratio %.2f%%\n", 100.0 * compressed_size / orig_size);
  }

  return ret;
}

