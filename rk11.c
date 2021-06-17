#include "11.h"
#include "rk11.h"

// not particularly accurate
// and no implementation of maintenance mode

// RK05 has:
//  256 16bit words per sector
//  12 sectors per cylinder
//  2 surfaces
//  203 cylinders per surface
//  1500 rpm

enum {
	// RKDS
	SC_EQ_SA	= 020,
	WPS	= 040,	// write protect status
	ARDY	= 0100,	// access ready
	DRY	= 0200,	// drive ready
	SOK	= 0400,	// sector counter ok
	SIN	= 01000,	// seek incomplete
	DRU	= 02000,	// drive unsafe
	HDEN	= 04000,	// high density disk
	DPL	= 010000,	// drive power low

	// RKCS
	GO	= 1,
	IDE	= 0100,	// interrupt on done enable
	RDY	= 0200,	// ready
	SSE	= 0400,	// stop on soft error
	RWA	= 01000,	// r/w all
	FMT	= 02000,	// format
	IBA	= 04000,	// inhibit incrementing RKBA
	MAINT	= 010000,	// maintenance mode - not implemented
	SCP	= 020000,	// search complete
	HE	= 040000,	// hard error
	ERR	= 0100000,	// error

	// RKER
	WCE	= 1,	// write check error
	CSE	= 2,	// checksum error
	NXS	= 040,	// non existent sector
	NXC	= 0100,	// non existent cylinder
	NXD	= 0200,	// non existent disk
	RTE	= 0400,	// read timing error
	DLT	= 01000,	// data late
	NXM	= 02000,	// non existent memory
	PCE	= 04000,	// programming error
	SKE	= 010000,	// seek error
	WLO	= 020000,	// write lockout violation
	OVR	= 040000,	// overrun
	DRE	= 0100000,	// drive error

	FR_CTL_RESET = 0,
	FR_WRITE,
	FR_READ,
	FR_WRITE_CHECK,
	FR_SEEK,
	FR_READ_CHECK,
	FR_DRIVE_RESET,
	FR_WRITE_LOCK
};
#define ISXFER(func) ((056>>(func))&1)

int
dati_rk11(Bus *bus, void *dev)
{
	RK11 *rk = dev;
	rk->bus = bus;
	if(bus->addr >= 0777400 && bus->addr < 0777420){
		switch(bus->addr&016){
		case 000: bus->data = rk->rkds | rk->drives[rk->d].rkds; break;
		case 002: bus->data = rk->rker; break;
		case 004: bus->data = rk->rkcs; break;
		case 006: bus->data = rk->rkwc; break;
		case 010: bus->data = rk->rkba; break;
		case 012: bus->data = rk->rkda; break;
		case 014: bus->data = rk->rkmr; break;
		case 016: bus->data = rk->rkdb; break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

static int
datox_rk11(Bus *bus, RK11 *rk, word mask)
{
	rk->bus = bus;
	if(bus->addr >= 0777400 && bus->addr < 0777420){
		switch(bus->addr&017){
		case 000:	// RKDS
		case 001:
		case 002:	// RKER
		case 003:
			break;	// read only

		case 004:	// RKCS
		case 005:
			SETMASK(rk->rkcs, bus->data, mask&0017577);
			break;
		case 006:	// RKWC
		case 007:
			SETMASK(rk->rkwc, bus->data, mask);
			break;
		case 010:	// RKBA
		case 011:
			SETMASK(rk->rkba, bus->data, mask);
			break;
		case 012:	// RKDA
		case 013:
			if(rk->rkcs & RDY){
				SETMASK(rk->rkda, bus->data, mask);
				rk->d = (rk->rkda>>13) & 7;
			}else if((rk->rkcs & GO)==0)
				rk->done = 1;
			break;
		case 014:	// RKMR
		case 015:
			// not implemented
			break;
		case 016:	// RKDB
		case 017:
			// read only
			break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}


int
dato_rk11(Bus *bus, void *dev)
{
	return datox_rk11(bus, (RK11*)dev, 0177777);
}

int
datob_rk11(Bus *bus, void *dev)
{
	return datox_rk11(bus, (RK11*)dev, bus->addr&1 ? 0177400 : 0377);
}

static int
blockaddr(int cyl, int surf, int sect)
{
	return (cyl*2 + surf)*12 + sect;
}

static int
busaddr(RK11 *rk)
{
	int addr = rk->rkba | (rk->rkcs&060)<<12;
	if((rk->rkcs & IBA)==0){
		rk->rkba += 2;
		if(rk->rkba == 0)
			SETMASK(rk->rkcs, rk->rkcs+020, 060);
	}
	return addr;
}

static void
rkerr(RK05 *drive, RK11 *rk, int err)
{
	rk->rker |= err;
	if(rk->rker & 177740){
		rk->rkcs |= RDY;
		drive->func = 0;
	}
}

/* start new function */
void
start_rk11(RK05 *drive, RK11 *rk)
{
	drive->func = (rk->rkcs>>1) & 7;
	rk->rkcs &= ~SCP;
	rk->done = 0;


	switch(drive->func){
	case FR_CTL_RESET:
		reset_rk11(rk);
		break;
	case FR_WRITE:
	case FR_READ:
	case FR_WRITE_CHECK:
	case FR_READ_CHECK:
		rk->rkcs &= ~RDY;
		if(drive->fp == nil)
			rkerr(drive, rk, NXD);
		if((rk->rkda&017) >= 12)
			rkerr(drive, rk, NXS);
		if(((rk->rkda>>5)&0377) >= 0312)
			rkerr(drive, rk, NXC);
		if(drive->rkds & WPS &&
		   (drive->func == FR_WRITE || drive->func == FR_WRITE_CHECK))
			rkerr(drive, rk, WLO);
		else
			drive->newcyl = (rk->rkda>>5)&0377;
		break;
	case FR_SEEK:
		drive->newcyl = (rk->rkda>>5)&0377;
		break;
	case FR_DRIVE_RESET:
		reset_rk05(&rk->drives[rk->d]);
		break;
	case FR_WRITE_LOCK:
		drive->rkds |= WPS;
		drive->func = 0;
		break;
	}
}

int
svc_rk11(Bus *bus, void *dev)
{
	int i;
	RK11 *rk = dev;

	if((rk->rkcs & RDY) && (rk->rkcs & GO)){
		rk->rkcs &= ~GO;
		start_rk11(&rk->drives[rk->d], rk);
	}
	for(i = 0; i < 8; i++)
		svc_rk05(&rk->drives[i], rk, rk->d == i);

	if(rk->rker & 177740)
		rk->rkcs |= HE;
	else
		rk->rkcs &= ~HE;
	if(rk->rker)
		rk->rkcs |= ERR;
	else
		rk->rkcs &= ~ERR;

	return (rk->rkcs&IDE) &&
	       (rk->done || (rk->rkcs&HE) || (rk->rkcs&ERR) && (rk->rkcs&SSE)) ?
		5 : 0;
}


int
bg_rk11(void *dev)
{
	RK11 *rk = dev;
	// is this right?
	rk->done = 0;
	return 0220;
}


void
reset_rk11(void *dev)
{
	int i;
	RK11 *rk = dev;
	// TODO
	rk->rkds = 0;
	rk->rker = 0;
	rk->rkcs = RDY;
	rk->rkwc = 0;
	rk->rkba = 0;
	rk->rkda = 0;
	rk->rkmr = 0;
	rk->rkdb = 0;

	rk->state = 0;
	rk->done = 0;

	rk->d = 0;
	for(i = 0; i < 8; i++)
		reset_rk05(&rk->drives[i]);
}



void
attach_rk05(RK11 *rk, int n, char *path)
{
	if(rk->drives[n].fp)
		detach_rk05(rk, n);
	rk->drives[n].fp = fopen(path, "r+");
	if(rk->drives[n].fp == nil)
		rk->drives[n].fp = fopen(path, "w+");
	if(rk->drives[n].fp == nil)
		fprintf(stderr, "couldn't open '%s'\n", path);
	reset_rk05(&rk->drives[n]);
}

void
detach_rk05(RK11 *rk, int n)
{
	if(rk->drives[n].fp){
		fclose(rk->drives[n].fp);
		rk->drives[n].fp = nil;
	}
	reset_rk05(&rk->drives[n]);
}

void
reset_rk05(RK05 *drive)
{
	// TODO: figure out what to actually do here
	drive->rkds = HDEN;

	drive->func = 0;
	drive->cyl_timer = 0;

	drive->newcyl = 0;

	drive->dsb = 0;
}

static void
count(RK05 *drive, RK11 *rk)
{
	word rkda;

	if(rk->rkwc == 0)
		return;

	rkda = rk->rkda+1;
	if((rkda & 017) == 12)
		rkda = (rkda&~017)+020;
	if((rkda&0160000) != (rk->rkda&0160000))
		rkerr(drive, rk, OVR);
	SETMASK(rk->rkda, rkda, 017777);

	// seek
	drive->newcyl = (rkda>>5) & 0377;
}

void
svc_rk05(RK05 *drive, RK11 *rk, int selected)
{
	if(drive->fp == nil){
		reset_rk05(drive);
		return;
	}

	drive->rkds |= DRY | SOK;

	// movement
	// TODO: do some actual timing here?

	// move head (seek)
	drive->rkds &= ~ARDY;
	if(drive->cyl != drive->newcyl){
		if(drive->cyl_timer)
			drive->cyl_timer--;
		if(drive->cyl_timer == 0){
			if(drive->cyl < drive->newcyl)
				drive->cyl++;
			else
				drive->cyl--;
			// some fake value, should be around 640us
			drive->cyl_timer = 500;	
		}

		// search complete
		if(drive->cyl == drive->newcyl &&
		   drive->func == FR_SEEK){
			rk->rkds = (drive-rk->drives)<<13;
			rk->rkcs |= SCP;
			drive->func = 0;
			rk->done = 1;
		}
	}else if(drive->func == 0)
		drive->rkds |= ARDY;

	drive->surf = (rk->rkda>>4)&1;

	// transfer data to/from sector we're interested in
	if(selected && ISXFER(drive->func) &&
	   drive->cyl == drive->newcyl && (rk->rkda&017) == drive->sc){

		// beginning of new sector
		if(drive->wc == 0){
			int pos = blockaddr(drive->cyl, drive->surf, drive->sc)*512;
			fseek(drive->fp, pos, 0);
			fread(drive->buf, 1, 512, drive->fp);
			// rewind so we can write back easily
			fseek(drive->fp, pos, 0);

			rk->state = 1;
		}

		// transfer one word
		if(rk->state == 1 && rk->rkwc){
			rk->rkwc++;

			if(drive->func == FR_READ){
				drive->dsb = WD(drive->buf[drive->wc*2+1], drive->buf[drive->wc*2]);
				rk->rkdb = drive->dsb;

				// write to bus
				rk->bus->data = rk->rkdb;
				rk->bus->addr = busaddr(rk);
				if(dato_bus(rk->bus))
					rkerr(drive, rk, NXM);
			}else if(drive->func == FR_WRITE ||
				 drive->func == FR_WRITE_CHECK){

				// read from bus
				rk->bus->addr = busaddr(rk);
				if(dati_bus(rk->bus))
					rkerr(drive, rk, NXM);
				rk->rkdb = rk->bus->data;

				drive->dsb = rk->rkdb;
				drive->buf[drive->wc*2] = drive->dsb;
				drive->buf[drive->wc*2+1] = drive->dsb>>8;
			}

			// transfer finished
			if(rk->rkwc == 0)
				// have to keep the function around till
				// the end of the block
				rk->done = 1;
		}
	}

	// rotate
	drive->wc++;
	if(drive->wc >= 256){

		// end of sector
		drive->wc = 0;

		// end of current sector
		if(selected && ISXFER(drive->func) &&
		   drive->cyl == drive->newcyl && (rk->rkda&017) == drive->sc){

			if(rk->state == 1){
				count(drive, rk);

				// write back
				if(drive->func == FR_WRITE ||
				   drive->func == FR_WRITE_CHECK)
					fwrite(drive->buf, 1, 512, drive->fp);
				rk->state = 0;
			}

			// end of transfer
			if(rk->rkwc == 0){
				rk->rkcs |= RDY;
				drive->func = 0;
			}
		}

		drive->rkds &= ~SOK;
		drive->sc++;
		// should be one revolution every 40ms
		if(drive->sc >= 12)
			drive->sc = 0;
	}
	drive->rkds &= ~017;
	drive->rkds |= drive->sc;
	if((rk->rkda&017) == drive->sc)
		drive->rkds |= SC_EQ_SA;
	else
		drive->rkds &= ~SC_EQ_SA;
}
