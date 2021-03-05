CC=clang++

CFLAGS = -Wall -Wextra -O2 -pedantic
CLIBS = -lm

all : main makelib libtest

lzw10pack	:	lzw10pack.cpp
		$(CC) $(CFLAGS) -c lzw10pack.cpp

lzw10unpack : lzw10unpack.cpp
		$(CC) $(CFLAGS) -c lzw10unpack.cpp

common: common.cpp
		$(CC) $(CFLAGS) -c common.cpp

main : main.cpp common lzw10pack lzw10unpack
		$(CC) $(CFLAGS) -o lzw10 main.cpp lzw10pack.o lzw10unpack.o common.o

makelib: lzw10pack.o lzw10unpack.o common.o
		ar rcs liblzw10.a lzw10pack.o lzw10unpack.o common.o

libtest : libtest.cpp
		$(CC) $(CFLAGS) -DLIBTEST_MAIN -o lzw_test libtest.cpp $(CLIBS) -L. -llzw10

.PHONY: clean

clean :
		-rm lzw10pack.o lzw10unpack.o common.o lzw10 liblzw10.a