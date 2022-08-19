#include <u.h>
#include <libc.h>
#include <thread.h>

#include <stdio.h>
#include <ctype.h>

typedef u8int uint8, byte;
typedef s8int int8;
typedef u16int uint16, word;
typedef u32int uint32;
typedef unsigned int uint;
typedef long size_t;	// whatever


#define sleep_ms sleep
int xmain(int argc, char *argv[]);
void exit(int code);

#define O_RDWR (OREAD|OWRITE)

typedef struct Clock Clock;
struct Clock
{
	Channel *chan;
};

typedef struct Tty Tty;
struct Tty
{
	int fd;
	Channel *chan;
};
