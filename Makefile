CFLAGS=-Wall -std=c99 -D_DEBUG -O0 -g
INCLUDE=-Ithirdparty/volk
LDFLAGS=-ldl -lX11

all: vlkTest triangle.vert.spv triangle.frag.spv

vlkTest: main.c vlk_window.c vlk_window.h
	${CC} ${CFLAGS} ${LDFLAGS} ${INCLUDE} vlk_window.c main.c -o vlkTest

triangle.vert.spv: triangle.vert
	glslangValidator triangle.vert -V -o triangle.vert.spv

triangle.frag.spv: triangle.frag
	glslangValidator triangle.frag -V -o triangle.frag.spv
