CC=clang++

CFLAGS = -Wall -Wextra -O2 -pedantic
CLIBS = -lm

all : main makelib libtest

lzw16pack	:	lzw16pack.cpp
		$(CC) $(CFLAGS) -c lzw16pack.cpp

lzw16unpack : lzw16unpack.cpp
		$(CC) $(CFLAGS) -c lzw16unpack.cpp

common: common.cpp
		$(CC) $(CFLAGS) -c common.cpp

main : main.cpp common lzw16pack lzw16unpack
		$(CC) $(CFLAGS) -o lzw16 main.cpp lzw16pack.o lzw16unpack.o common.o

makelib: lzw16pack.o lzw16unpack.o common.o
		ar rcs liblzw16.a lzw16pack.o lzw16unpack.o common.o

libtest : libtest.cpp
		$(CC) $(CFLAGS) -o lzw_test libtest.cpp $(CLIBS) -L. -llzw16

.PHONY: clean

clean :
		-rm lzw16pack.o lzw16unpack.o common.o lzw16 liblzw16.a