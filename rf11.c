#include "11.h"
#include "rf11.h"

// not particularly accurate
// and no implementation of maintenance mode

// RS11 has:
//  1024 words per track
//  128 cylinders
//  one word every 16us

enum {
	// DCS
	GO	= 01,
	MA	= 010,
	IE	= 0100,
	RDY	= 0200,
	DCLR	= 0400,
	MXF	= 01000,
	WLO	= 02000,
	NED	= 04000,
	DPE	= 010000,
	WCE	= 020000,
	FRZ	= 040000,
	ERR	= 0100000,

	// DAE
	DA14	= 040,
	DLT	= 0200,
	CMA_INH	= 0400,
	NEM	= 02000,
	CTER	= 010000,
	BTER	= 020000,
	ATER	= 040000,
	APE	= 0100000,

	FR_NOP = 0,
	FR_WRITE,
	FR_READ,
	FR_WRITE_CHECK,
};

void svc_rs11(RS11 *drive, RF11 *rf, int selected);
void reset_rs11(RS11 *rf);

static void
rferr(RF11 *rf, int err)
{
	rf->dcs |= err | RDY;
	// interrupt? probably...
	rf->done = 1;
}

// select track to cache in memory
// -1 means invalid and just writes back dirty buffer
static void
seltrack(RF11 *rf, int track)
{
	if(rf->track == track)
		return;

	RS11 *drive = &rf->drives[rf->d];
	if(rf->dirty){
		assert(drive->fp);
		fwrite(rf->buf, 1, 2048*2, drive->fp);
	}
	rf->track = track;

	if(rf->track >= 0){
		int pos = rf->track<<12;
		fseek(drive->fp, pos, 0);
		fread(rf->buf, 1, 2048*2, drive->fp);
		// rewind so we can write back easily
		fseek(drive->fp, pos, 0);
	}
}

static void
seldisk(RF11 *rf, int disk)
{
	if(rf->d == disk)
		return;
	seltrack(rf, -1);
	rf->d = disk&7;
	if(disk&010)
		rferr(rf, NED);
}

int
dati_rf11(Bus *bus, void *dev)
{
	RF11 *rf = dev;
	rf->bus = bus;
	if(bus->addr >= 0777460 && bus->addr < 0777500){
		switch(bus->addr&016){
		case 000: bus->data = rf->dcs; break;
		case 002: bus->data = rf->wc; break;
		case 004: bus->data = rf->cma; break;
		case 006: bus->data = rf->dar; break;
		case 010: bus->data = rf->dae; break;
		case 012: bus->data = rf->dbr; break;
		case 014: bus->data = rf->ma; break;
		case 016: bus->data = rf->drives[rf->d].ads; break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

static int
datox_rf11(Bus *bus, RF11 *rf, word mask)
{
	rf->bus = bus;
	RS11 *drive = &rf->drives[rf->d];
	if(bus->addr >= 0777460 && bus->addr < 0777500){
		switch(bus->addr&017){
		case 000:	// DCS
		case 001:
			SETMASK(rf->dcs, bus->data, mask&0176);
			if(bus->data & GO){
				// start function
				rf->done = 0;
				switch((rf->dcs>>1)&3){
				case FR_NOP:
					rf->dcs |= RDY;
					// ??? interrupt?
					break;
				case FR_READ:
				case FR_WRITE:
				case FR_WRITE_CHECK:
					rf->dcs &= ~RDY;
					if(drive->fp == nil)
						rferr(rf, NED);
					break;
				}
			}
			if(bus->data & DCLR)
				reset_rf11(rf);
			break;
		case 002:	// WC
		case 003:
			SETMASK(rf->wc, bus->data, mask);
			break;
		case 004:	// CMA
		case 005:
			SETMASK(rf->cma, bus->data, mask);
			break;
		case 006:	// DAR
		case 007:
			SETMASK(rf->dar, bus->data, mask);
			break;
		case 010:	// DAE
		case 011:
			SETMASK(rf->dae, bus->data, mask&0637);
			rf->dae &= ~DA14;
			seldisk(rf, (rf->dae>>2)&7);
			break;
		case 012:	// DBR
		case 013:
			SETMASK(rf->dbr, bus->data, mask);
			break;
		case 014:	// MA
		case 015:
			// not implemented
			break;
		case 016:	// ADS
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
dato_rf11(Bus *bus, void *dev)
{
	return datox_rf11(bus, (RF11*)dev, 0177777);
}

int
datob_rf11(Bus *bus, void *dev)
{
	return datox_rf11(bus, (RF11*)dev, bus->addr&1 ? 0177400 : 0377);
}

int
svc_rf11(Bus *bus, void *dev)
{
	int i;
	RF11 *rf = dev;

	for(i = 0; i < 8; i++)
		svc_rs11(&rf->drives[i], rf, rf->d == i);

	if(rf->dcs&(FRZ|WCE|DPE|NED|WLO|MXF))
		rf->dcs |= ERR;
	else
		rf->dcs &= ~ERR;

	return (rf->dcs&IE) &&
// TODO: probably just have one interrupt bit?
	       (rf->done || (rf->dcs&ERR)) ?
		5 : 0;
}

int
bg_rf11(void *dev)
{
	RF11 *rf = dev;
	rf->done = 0;
	return 0204;
}

void
reset_rf11(void *dev)
{
	RF11 *rf = dev;
	rf->dcs = RDY;
	rf->wc = 0;
	rf->cma = 0;
	rf->dar = 0;
	rf->dae = 0;
	seldisk(rf, 0);
	rf->dbr = 0;
	rf->ma = 0;

	rf->done = 0;
}


void
attach_rs11(RF11 *rf, int n, char *path)
{
	if(rf->drives[n].fp)
		detach_rs11(rf, n);
	rf->drives[n].fp = fopen(path, "r+");
	if(rf->drives[n].fp == nil)
		rf->drives[n].fp = fopen(path, "w+");
	if(rf->drives[n].fp == nil)
		fprintf(stderr, "couldn't open '%s'\n", path);
	reset_rs11(&rf->drives[n]);
}

void
detach_rs11(RF11 *rf, int n)
{
	if(rf->drives[n].fp){
		fclose(rf->drives[n].fp);
		rf->drives[n].fp = nil;
	}
	reset_rs11(&rf->drives[n]);
}

void
reset_rs11(RS11 *drive)
{
	drive->ads = 0;
}

static int
busaddr(RF11 *rf)
{
	int addr = rf->cma | (rf->dcs&060)<<12;
	if((rf->dae & CMA_INH)==0){
		rf->cma += 2;
		if(rf->cma == 0)
			SETMASK(rf->dcs, rf->dcs+020, 060);
	}
	return addr;
}

void
svc_rs11(RS11 *drive, RF11 *rf, int selected)
{
	if(drive->fp == nil){
		drive->ads = 0;
		return;
	}

	// read/write data
	if(selected && (rf->dcs&RDY)==0){
		// track address we want
		seltrack(rf, (rf->dar>>11) | (rf->dae&3)<<5);

		// word we're interested in
		if(rf->wc && drive->ads == (rf->dar&03777)){
			// read/write word here
			switch((rf->dcs>>1)&3){
			case FR_READ:
				rf->dbr = WD(rf->buf[drive->ads*2+1], rf->buf[drive->ads*2]);

				// write to bus
				rf->bus->data = rf->dbr;
				rf->bus->addr = busaddr(rf);
				if(dato_bus(rf->bus))
					rferr(rf, NEM);
				break;
			case FR_WRITE:
			case FR_WRITE_CHECK:
				// read from bus
				rf->bus->addr = busaddr(rf);
				if(dati_bus(rf->bus))
					rferr(rf, NEM);
				rf->dbr = rf->bus->data;

				rf->buf[drive->ads*2] = rf->dbr;
				rf->buf[drive->ads*2+1] = rf->dbr>>8;
				rf->dirty = 1;
				break;
			}

			rf->wc++;
			if(rf->wc == 0){
				rf->dcs |= RDY;
				rf->done = 1;
			}else{
				// count
				rf->dar++;
				if(rf->dar == 0){
					// carry into dae
					rf->dae++;
					// this can carry into disk address
					seldisk(rf, (rf->dae>>2)&017);
				}
			}
		}
	}

	// rotate - one word every 16us
	// address of next word on track
	drive->ads = (drive->ads+1)&03777;
}
