CFLAGS=-Wall -std=c99 -D_DEBUG -O0 -g
INCLUDE=-Ithirdparty/volk
LDFLAGS=-ldl -lX11

all: vlkTest

vlkTest: main.c vlk_window.c vlk_window.h
	${CC} ${CFLAGS} ${LDFLAGS} ${INCLUDE} vlk_window.c main.c -o vlkTest