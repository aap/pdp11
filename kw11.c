#include "11.h"
#include "kw11.h"

int
dati_kw11(Bus *bus, void *dev)
{
	KW11 *kw = dev;
	if(bus->addr == 0777546){
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
	if(bus->addr == 0777546){
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
	if(bus->addr == 0777546){
		/* ignore odd bytes */
		if(bus->addr & 1)
			return 0;
		return dato_kw11(bus, dev);
	}
	return 1;
}

int
svc_kw11(Bus *bus, void *dev)
{
	KW11 *kw = dev;
	if(handleclock(&kw->clock)){
		kw->lc_clock = 1;
		kw->lc_int = 1;
	}
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

	initclock(&kw->clock);
}
