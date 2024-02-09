#include "11.h"
#include "xgp.h"
#include "util.h"
#include "print.h"
#include <sys/wait.h>
#include <pthread.h>

/*
  Emulates MIT's Unibus hardware interface to an XGP printer.  The
  hardware is similar to the earlier CMU interface, and it seems
  likely it was an inspiration for the MIT version.

  Most of the emulation was made from reading the XGP-11 source code.
  Details about character mode is from an XSCRIBL paper from CMU.  A
  theory of the theory of operation follows.

  It seems the FMOT bit is used to start the paper moving.  When the
  paper is up to speed, FDRY is set.  Paper then moves at a constant
  rate: one 1/200 inch line per 7.75 milliseconds.

  The software is suposed to set the MAR register and FGO before a
  scanline sync is signaled.  Failure to do so will result in an FSYN
  error.  If FGO is set, then data will start to fetch from the
  address in MAR, and shift out to the CRT video signal.

  Data is encoded as 8-bit bytes.  The printer starts each line in
  "character" mode.  The first byte is the number of bits, followed by
  just enough bytes to encode those bits.  The bits are shifted out
  LSB first.  A new bit count follows.

  If the character mode bit count it 0, the next byte is 0 for
  "vector" mode, 1 for stop, or 2 for "image" mode.  Any other value
  is an error.

  In vector mode alternating bytes specify counts of white and black
  bits, respectively.  Two consequtive zero counts escape back to
  character mode.

  Stop clears the FGO bit and sets FDONE.

  In image mode, data is shifted out verbatim until an overscan error
  occurs.

  Any error clears FGO and sets FDONE.

  If FDONE is set and FDIE is also set, an interrupt for vector 370 is
  raised.

  If FCUTI is set in the XCUT register, a cutter immediately cuts off
  the paper.  This happens 4020 (octal) lines after a line printed to
  paper.  It seems the paper must keep moving for at least twice this
  amount after FMOT has been turned off for the last of the output to
  reach the cutter.
*/

#define MAX_WIDTH 1728
#define XGP_VEC 0370
#define SPEED_READY 300
#define SYNC_PERIOD 7750000 // 7.75ms in nanoseconds
#define MKTOCT 04020 // Magic distance in 60ths sec from drum exposure to paper cutter

enum {
	XCR = 0772100,		// Xerox control register
	MAR = 0772102,		// Memory address register
	XSR = 0772104,		// Xerox status register
	XCUT = 0772106,		// Xerox paper cut
};

/* XGP control bits */
enum {
	FERR = 0100000,		// ERROR
	FMOT = 02000,		// MOTION
	FDONE = 0200,		// DONE
	FDIE = 0100,		// DONE INTERUPT ENABLE
	FCUTI = 4,		// CUT PAPER NOW!
	FCUT = 2,		// CUT PAPER
	FGO = 1			// GO
};

/* XGP status bits */
enum {
	FOS = 0100000,		// OVERSCAN
	FSYN = 040000,		// SYNC ERROR
	FOR = 020000,		// DMA OVERRUN
	FNXM = 010000,		// NXM
	FRDYC = 04000,		// DEV RDY CHANGED
	FBCB = 02000,		// BAD CONTROL BYTE IN CHAR MODE
	FRDY = 0100,		// DEVICE READY (INCLUDES "PAPER UP TO SPEED")
	FACT = 1		// ACTIVE
};

void xgp_char(XGP *xgp, uint8 data);

void
xgp_idle(XGP *xgp, Bus *bus)
{
	// Waiting for CRT retrace
}

int
dato_xgp(Bus *bus, void *dev)
{
	char file[256];
	XGP *xgp = dev;
	int offset, lines;
	word d;

	d = bus->data;
	switch(bus->addr){
	case XCR:
		if((d & FMOT) != 0 && xgp->speed == 0)
			fprintf(stderr, "Paper in motion\n");
		xgp->xcr = d;
		if((xgp->xcr & FMOT) != 0 && (xgp->xcr & FRDY) == 0)
			print_start();
		if(d & FGO) {
			if(xgp->xsr & FRDY) {
				xgp->xcr |= FGO;
				xgp->xcr &= ~FDONE;
				xgp->go = xgp_idle;
				xgp->mode = xgp_char;
				xgp->dots = 0;
			} else
				xgp->xcr |= FERR;
		}
		return 0;
	case MAR:
		xgp->mar = d;
		return 0;
	case XSR:
		xgp->xsr = d;
		return 0;
	case XCUT:
		if(bus->data & FCUTI) {
			offset = MKTOCT * 100000 / 60 / 775;
			lines = print_finish(offset, file, sizeof file);
			if(lines > 0 && *file != 0)
				fprintf(stderr, "Printed %d lines, %.1f inches to %s\n",
					lines, lines / 200.0, file);
			else if(lines < 0)
				fprintf(stderr, "Printing error\n");
			fprintf(stderr, "Paper cut\n");
		}
		return 0;
	}
	return 1;
}

int
datob_xgp(Bus *bus, void *dev)
{
	XGP *xgp = dev;
	word d, m;

	d = bus->data;
	m = bus->addr&1 ? ~0377 : 0377;
	switch(bus->addr&~1){
	case XCR:
		return 0;
	case MAR:
		return 0;
	case XSR:
		return 0;
	case XCUT:
		return 0;
	}
	return 1;
}

int
dati_xgp(Bus *bus, void *dev)
{
	XGP *xgp = dev;

	switch(bus->addr){
	case XCR:
		bus->data = xgp->xcr;
		return 0;
	case MAR:
		bus->data = xgp->mar;
		return 0;
	case XSR:
		bus->data = xgp->xsr;
		return 0;
	case XCUT:
		return 0;
	}
	return 1;
}

void
xgp_done(XGP *xgp, uint16 errors)
{
	xgp->xcr |= FDONE;
	xgp->xcr &= ~FGO;
	xgp->go = xgp_idle;
	if(errors) {
		xgp->xcr |= FERR;
		xgp->xsr |= errors;
	}
}

void
xgp_output(XGP *xgp, uint8 data, int n)
{
	int m = 1;
	while(n-- > 0) {
		print_dot(data & m);
		m <<= 1;
		xgp->dots++;
		if(xgp->dots >= MAX_WIDTH) {
			xgp_done(xgp, FOS);
			return;
		}
	}
}

void
xgp_image(XGP *xgp, uint8 data)
{
	xgp_output(xgp, data, 8);
}

void
xgp_vector(XGP *xgp, uint8 data)
{
	if(data == 0)
		xgp->cnt++;
	else
		xgp->cnt = 0;

	if(xgp->cnt == 2)
		xgp->mode = xgp_char;

	if(data == 0)
		return;

	while(data > 8) {
		xgp_output(xgp, xgp->bw, 8);
		data -= 8;
		if(xgp->xcr & FDONE)
			return;
	}
	xgp_output(xgp, xgp->bw, data);

	xgp->bw ^= 0xFF;
}

void
xgp_command(XGP *xgp, uint8 data)
{
	switch(data) {
	case 0x00:
		xgp->mode = xgp_vector;
		xgp->cnt = 0;
		xgp->bw = 0;
		break;
	case 0x01:
		xgp_done(xgp, 0);
		break;
	case 0x02:
		xgp->mode = xgp_image;
		break;
	default:
		xgp_done(xgp, FBCB);
		break;
	}
}

void
xgp_char_data(XGP *xgp, uint8 data)
{
	if(xgp->cnt > 8) {
		xgp_output(xgp, data, 8);
		xgp->cnt -= 8;
	} else {
		xgp_output(xgp, data, xgp->cnt);
		xgp->mode = xgp_char;
	}
}

void
xgp_char(XGP *xgp, uint8 data)
{
	if(data == 0x00) {
		xgp->mode = xgp_command;
	} else {
		xgp->cnt = data;
		xgp->mode = xgp_char_data;
	}	  
}

void
reset_xgp(void *dev)
{
	XGP *xgp = dev;

	xgp->xcr = 0;
	xgp->mar = 0;
	xgp->xsr = 0;
	xgp->go = xgp_idle;
	xgp->mode = xgp_char;
	initclock(&xgp->clk, SYNC_PERIOD);
}

void
initxgp(XGP *xgp)
{
	xgp->speed = 0;
}

void
xgp_byte(XGP *xgp, uint8 data)
{
	if(xgp->xcr & FGO)
		xgp->mode(xgp, data);
}

void
xgp_read(XGP *xgp, Bus *bus)
{
	bus->addr = xgp->mar;
	xgp->mar += 2;
	if(dati_bus(bus)) goto be;
	xgp_byte(xgp, bus->data & 0xFF);
	xgp_byte(xgp, bus->data >> 8);
	return;	

be:
	xgp_done(xgp, FNXM);
}

void
xgp_go(XGP *xgp, Bus *bus)
{
	xgp_read(xgp, bus);
}

void
xgp_sync(XGP *xgp)
{
	int wstatus;
	if(waitpid(-1, &wstatus, WNOHANG) > 0) {
		if(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0)
			fprintf(stderr, "Printing error\n");
	}

	if(xgp->xcr & FMOT)
		xgp->speed += (xgp->speed < SPEED_READY);
	else if (--xgp->stopping == 0) {
		xgp->stopping = 1;
		if(xgp->speed == SPEED_READY)
			fprintf(stderr, "XGP stopping\n");
		if(xgp->speed == 1)
			fprintf(stderr, "XGP stopped\n");
		xgp->speed -= (xgp->speed > 0);
	}

	if(xgp->speed >= SPEED_READY) {
		if((xgp->xsr & FRDY) == 0) {
			fprintf(stderr, "Paper up to speed\n");
			xgp->stopping = 3*MKTOCT;
		}
		print_line();
		xgp->xsr |= FRDY;
	} else
		xgp->xsr &= ~FRDY;

	if((xgp->xcr & FRDY) == 0)
		return;

	if(xgp->xcr & FGO)
		xgp->go = xgp_go;
	else
		xgp_done(xgp, FSYN);
}

int
svc_xgp(Bus *bus, void *dev)
{
	XGP *xgp = dev;
	if(handleclock(&xgp->clk))
		xgp_sync(xgp);
	if(xgp->xcr & FGO)
		xgp->go(xgp, bus);
	if((xgp->xcr & (FDONE|FDIE)) == (FDONE|FDIE))
		return 5;
	else
		return 0;
}

int
bg_xgp(void *dev)
{
	XGP *xgp = dev;
	if((xgp->xcr & (FDONE|FDIE)) == (FDONE|FDIE))
		return XGP_VEC;
	return 0;
}
