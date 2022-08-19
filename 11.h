#ifdef PLAN9
#include "p9.h"
#else
#include "unix.h"
#endif

#define WD(hi, lo) W((hi)<<8 | (lo))
#define W(w) ((word)(w))
#define M8  0377
#define M16 0177777
#define B7  0000200
#define B15 0100000
#define B31 020000000000

#define SETMASK(l, r, m) l = (((l)&~(m)) | ((r)&(m)))

//#define trace printf
#define trace(...)

word sgn(word w);
word sxt(byte b);

typedef struct Bus Bus;
typedef struct Busdev Busdev;

struct Busdev
{
	Busdev *next;
	void *dev;
	int (*dati)(Bus *bus, void *dev);
	int (*dato)(Bus *bus, void *dev);
	int (*datob)(Bus *bus, void *dev);
	int (*svc)(Bus *bus, void *dev);
	int (*bg)(void *dev);
	void (*reset)(void *dev);
};
void reset_null(void *dev);

struct Bus
{
	Busdev *devs;
	uint32 addr;
	word data;
	int pause;
};
int dati_bus(Bus *bus);
int datip_bus(Bus *bus);
int dato_bus(Bus *bus);
int datob_bus(Bus *bus);

typedef struct Memory Memory;
struct Memory
{
	word *mem;
	uint32 start, end;
};
int dati_mem(Bus *bus, void *dev);
int dato_mem(Bus *bus, void *dev);
int datob_mem(Bus *bus, void *dev);
int dati_rom(Bus *bus, void *dev);
int dato_rom(Bus *bus, void *dev);
int datob_rom(Bus *bus, void *dev);

typedef struct KE11 KE11;
struct KE11
{
	word ac;
	word mq;
	word x;
	byte sc;	/* 6 bits */
	byte sr;
};
int dati_ke11(Bus *bus, void *dev);
int dato_ke11(Bus *bus, void *dev);
int datob_ke11(Bus *bus, void *dev);
void reset_ke11(void *dev);

void initclock(Clock *clk);
int handleclock(Clock *clk);
int ttyopen(Tty *tty);
int ttyinput(Tty *tty, char *c);
