#include "11.h"
#include "kl11.h"

int
dati_kl11(Bus *bus, void *dev)
{
	KL11 *kl = dev;
	if(bus->addr >= 0777560 && bus->addr < 0777570){
		switch(bus->addr&6){
		/* Receive */
		case 0:
			bus->data = kl->rcd_int_enab<<6 |
				kl->rcd_int<<7 |
				kl->rcd_busy<<1;
			break;
		case 2:
			bus->data = kl->rcd_b;
			kl->rcd_b = 0;
			kl->rcd_da = 0;
			kl->rcd_int = 0;
			break;
		/* Transmit */
		case 4:
			bus->data = kl->xmit_maint<<2 |
				kl->xmit_int_enab<<6 |
				kl->xmit_int<<7;
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
dato_kl11(Bus *bus, void *dev)
{
	KL11 *kl = dev;
	if(bus->addr >= 0777560 && bus->addr < 0777570){
		switch(bus->addr&7){
		/* Receive */
		case 0:
			// TODO: RDR ENAB
			kl->rcd_rdr_enab = bus->data & 1;
			if(!kl->rcd_int_enab && bus->data&0100 && kl->rcd_da)
				kl->rcd_int = 1;
			kl->rcd_int_enab = bus->data>>6 & 1;
			break;
		case 2:
			/* read only */
			break;
		/* Transmit */
		case 4:
			// TODO: MAINT
			kl->xmit_maint = bus->data>>2 & 1;
			if(!kl->xmit_int_enab && bus->data&0100 && kl->xmit_tbmt)
				kl->xmit_int = 1;
			kl->xmit_int_enab = bus->data>>6 & 1;
			break;
		case 6:
			kl->xmit_b = bus->data;
			kl->xmit_tbmt = 0;
			kl->xmit_int = 0;
			break;

		/* respond but don't do anything */
		case 1:
		case 3:
		case 5:
		case 7:
			break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}


int
datob_kl11(Bus *bus, void *dev)
{
	return dato_kl11(bus, dev);
}

int NNN;

int
svc_kl11(Bus *bus, void *dev)
{
	KL11 *kl = dev;

	NNN++;
	if(NNN == 20){
	/* transmit */
	if(!kl->xmit_tbmt){
		uint8 c = kl->xmit_b & 0177;
		write(kl->ttyfd, &c, 1);
#ifdef AUTODIAG
	extern int diagpassed;
	if(c == '\a')
		diagpassed = 1;
#endif
		kl->xmit_tbmt = 1;
		kl->xmit_int = 1;
	}

	/* receive */
	if(hasinput(kl->ttyfd)){
		kl->rcd_busy = 1;
		kl->rcd_rdr_enab = 0;
		read(kl->ttyfd, &kl->rcd_b, 1);
		kl->rcd_da = 1;
		kl->rcd_busy = 0;
		kl->rcd_int = 1;
	}
	NNN = 0;
	}

	return kl->rcd_int && kl->rcd_int_enab ||
		kl->xmit_int && kl->xmit_int_enab ? 4 : 0;
}


int
bg_kl11(void *dev)
{
	KL11 *kl = dev;
	if(kl->rcd_int && kl->rcd_int_enab){
		kl->rcd_int = 0;
printf("rx trap\n");
		return 060;
	}

	if(kl->xmit_int && kl->xmit_int_enab){
		kl->xmit_int = 0;
printf("tx trap\n");
		return 064;
	}
	assert(0);	// can't happen
	return 0;
}


void
reset_kl11(void *dev)
{
	KL11 *kl = dev;
	kl->rcd_busy = 0;
	kl->rcd_rdr_enab = 0;
	kl->rcd_int_enab = 0;
	kl->rcd_int = 0;
	kl->rcd_da = 0;
	kl->rcd_b = 0;

	kl->xmit_int_enab = 0;
	kl->xmit_maint = 0;
	kl->xmit_int = 1;
	kl->xmit_tbmt = 1;
	kl->xmit_b = 0;
}
