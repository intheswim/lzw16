### Background 

<pre> 

This is classic implementation of LZW  algorithm based on  Mark Nelson's book on
data compression from 1995. It uses variable-width dictinary codes up to 15-bit. 

Original development was done in 1996-97. 

It has been modified to be a single executable  performing both compression  and
decompression, taking maximum bit length as argument for Compress2() function. 

The program has  been tested on Ubuntu 18.04 (with gcc and clang) and on Windows
10 (with Visual Studio 2019). 

</pre> 

### Usage and Examples 

<pre> 

Running make  produces (1) executable  `lzw10`; (2) static  library `liblzw10.a`
with  three exported functions called  Compress, Compress2 and  Decompress  (see
export.h)  and  (3)  test  program  `lzw_test` statically linked with  the above
library. 

Type `./lzw10` to see all command line options. 

Examples: 

`./lzw10 -p sample.txt sample.lzw` (pack sample.txt) 

`./lzw10 -u sample.lzw sample_copy.txt` (unpack sample.lzw) 

`./lzw10 -tv sample.txt` (test compression/decompression, verbose mode) 

`./lzw10 -large` (test synthetic data) 

`./lzw10 -b14 -large 10` (test synthetic data size 10 x 256 Kb, use max 14-bit) 

'./lzw10 -pv  -b12 sample.txt  sample.lzw` (pack  sample.txt  using  codes up to
12-bit width, verbose) 

</pre> 

### Notes and Limitations 

<pre> 

1. It supports input files up to 2GB in size. 

2. It is currently supported on little-endian machines only. 

</pre> 

### Possible improvements 

<pre> 

1. Add big endian support. 

2.  Input file size  limit can be fixed by replacing 4-byte  file size header in
compressed file with 48- or 64-bit values. 

3. BUFFLEN define (see common.h) which is shared between Compress and Decompress
calls can be included in compressed output file header  and then set dynamically
by Decompress. 

4. Add functions taking binary buffers as opposed to filenames. 

The code  can be relatively easily  converted to support  16-bit and even larger
maximum width bit  encoding. For this, some variables in  Compress  must be made
32-bit, and Decompress' NOT_CODE  value must  be more  than 16-bits, along  with
stack/suffx/prefix arrays. 

At  the  same time, as maximum code width grows, we hit the  "law of diminishing
returns" on data compression while also increasing  memory  requirements, so for
most practical purposes it's not really worth doing. 

Original versions  written in 1996 had maximum codes of 12-13 bits also in  part
due  to memory  constraints  of DOS  and eary Windows; 15-bit maximum on  modern
computers gives more that adequate compresion for LZW while  taking advantage of
vastly larger available memory. 


</pre> 

### License 

[MIT](https://choosealicense.com/licenses/mit/) 
