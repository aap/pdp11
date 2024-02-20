#include "ten11.h"

/* Some of these numbers are also hardcoded! */
enum {
	NUMFBUFFERS = 16,
	NUMCONNECTIONS = 32,
	NUMOUTPUTS = 32,
	NUMINPUTS = 16,
	NUMSECTIONS = 2,
};

/* Mask to get buffer number from CREG. */
#define BUFMASK  037

/* A remote TV connection */
typedef struct TVcon TVcon;
struct TVcon
{
	int fd;
	int dpy;	/* output number */
	int kbd;
};

typedef struct FBuffer FBuffer;
struct FBuffer
{
	word fb[16*1024 - 1];
	word csa;
	word mask;	/* 0 or ~0 for bw flip */

	/* list of all outputs that are driven
	 * by this buffer. */
	int osw[NUMOUTPUTS];
	int nosw;
};

/* The whole TV system */
typedef struct TV TV;
struct TV
{
	/* need this to tell 11 from 10 cycles */
	Ten11 *ten11;

	FBuffer buffers[NUMFBUFFERS];	/* 256 is the theoretical maximum */
	/* two different variables for 10 and 11 */
	word creg11;
	word creg10;
	
	/* Two sections.
	 * Each has 32 outputs that can have one of 16 inputs.
	 * Input 0 is null on both,
	 * that leaves 30 different actual inputs in total. */
	uint8 vswsect[NUMSECTIONS][NUMOUTPUTS];

	word kms;
	uint32 kma_hi;
	uint32 kma_lo;
	int km_haskey;
	word km_kbd;
	word km_key;

	TVcon cons[NUMCONNECTIONS];
	int omap[NUMOUTPUTS];	/* map of outputs to connections */
};
void inittv(TV *tv);
void closetv(TV *tv);
int dato_tv(Bus *bus, void *dev);
int datob_tv(Bus *bus, void *dev);
int dati_tv(Bus *bus, void *dev);
int svc_tv(Bus *bus, void *dev);
int bg_tv(void *dev);
void reset_tv(void *dev);
void handletvs(TV *tv);
void accepttv(int fd, void *arg);
void servetv(TV *tv, int port);
