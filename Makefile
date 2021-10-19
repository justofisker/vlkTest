CFLAGS=-Wall -std=c99 -D_DEBUG -O0 -g -Ithirdparty/volk
LDFLAGS=-ldl

all: vlkTest

vlkTest: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o vlkTest
