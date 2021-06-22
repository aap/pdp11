#include "11.h"
#include "dc11_fake.h"

// just a dummy

int
dati_dc11(Bus *bus, void *dev)
{
	if(bus->addr >= 0774000 && bus->addr < 0774400){
		int unit = (bus->addr>>3)&037;
		switch(bus->addr&6){
		/* Receive */
		case 0:
			bus->data = 0;
			break;
		case 2:
			bus->data = 0;
			break;
		/* Transmit */
		case 4:
			bus->data = 0;
			break;
		case 6:
			/* write only */
			bus->data = 0;
			break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

int
dato_dc11(Bus *bus, void *dev)
{
	if(bus->addr >= 0774000 && bus->addr < 0774400){
		switch(bus->addr&7){
		/* Receive */
		case 0:
		case 1:
			break;
		case 2:
		case 3:
			/* read only */
			break;
		/* Transmit */
		case 4:
		case 5:
			break;
		case 6:
		case 7:
			break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}


int
datob_dc11(Bus *bus, void *dev)
{
	return dato_dc11(bus, dev);
}

int
svc_dc11(Bus *bus, void *dev)
{
	return 0;
}


int
bg_dc11(void *dev)
{
	return 0300;
}


void
reset_dc11(void *dev)
{
}
