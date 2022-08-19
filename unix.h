#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "args.h"

typedef uint8_t uint8, byte;
typedef int8_t int8;
typedef uint16_t uint16, word;
typedef uint32_t uint32;
typedef unsigned int uint;

#define nil NULL

int hasinput(int fd);
int readn(int fd, void *data, int n);
int dial(char *host, int port);
void serve(int port, void (*handlecon)(int, void*), void *arg);
void nodelay(int fd);
void sleep_ms (uint32 ms);

#define xmain main

typedef struct Clock Clock;
struct Clock
{
	// TODO
};

typedef struct Tty Tty;
struct Tty
{
	int fd;
};
