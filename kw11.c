#include "11.h"
#include "kw11.h"

int
dati_kw11(Bus *bus, void *dev)
{
	KW11 *kw = dev;
	if(bus->addr == 777746){
		bus->data = kw->lc_int_enab<<6 |
			kw->lc_clock<<7;
		return 0;
	}
	return 1;
}

int
dato_kw11(Bus *bus, void *dev)
{
	KW11 *kw = dev;
	if(bus->addr == 777546){
		kw->lc_int_enab = bus->data>>6 & 1;
		if((bus->data & 0200) == 0){
			kw->lc_clock = 0;
			kw->lc_int = 0;
		}
		return 0;
	}
	return 1;
}


int
datob_kw11(Bus *bus, void *dev)
{
	/* ignore odd bytes */
	if(bus->addr & 1)
		return 0;
	return dato_kw11(bus, dev);
}

#define CLOCKFREQ (1000000000/60)

static struct timespec oldtime, newtime;

static void
initclock(void)
{
	clock_gettime(CLOCK_REALTIME, &newtime);
	oldtime = newtime;
}

static void
handleclock(KW11 *kw)
{
	struct timespec diff;
	clock_gettime(CLOCK_REALTIME, &newtime);
	diff.tv_sec = newtime.tv_sec - oldtime.tv_sec;
	diff.tv_nsec = newtime.tv_nsec - oldtime.tv_nsec;
	if(diff.tv_nsec < 0){
		diff.tv_nsec += 1000000000;
		diff.tv_sec -= 1;
	}
	if(diff.tv_nsec >= CLOCKFREQ){
		kw->lc_clock = 1;
		kw->lc_int = 1;
		oldtime.tv_nsec += CLOCKFREQ;
		if(oldtime.tv_nsec >= 1000000000){
			oldtime.tv_nsec -= 1000000000;
			oldtime.tv_sec += 1;
		}
	}
}

int
svc_kw11(Bus *bus, void *dev)
{
	KW11 *kw = dev;
	handleclock(kw);
	return kw->lc_int && kw->lc_int_enab ? 6 : 0;
}


int
bg_kw11(void *dev)
{
	KW11 *kw = dev;
	kw->lc_int = 0;
	return 0100;
}

void
reset_kw11(void *dev)
{
	KW11 *kw = dev;
	kw->lc_int_enab = 0;
	// TODO: 1?
	kw->lc_clock = 0;
	kw->lc_int = 0;

	initclock();
}
