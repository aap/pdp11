#include "11.h"
#include "kl11.h"

// not super accurate
// maintenance mode not implemented

int
dati_kl11(Bus *bus, void *dev)
{
	KL11 *kl = dev;
	if(bus->addr >= 0777560 && bus->addr < 0777570){
		switch(bus->addr&6){
		/* Receive */
		case 0:
			bus->data = kl->rdr_int_enab<<6 |
				kl->rdr_done<<7 |
				kl->rdr_busy<<11;
			break;
		case 2:
			bus->data = kl->rdr_buf;
			kl->rdr_buf = 0;
			kl->rdr_done = 0;
			break;
		/* Transmit */
		case 4:
			bus->data = kl->maint<<2 |
				kl->pun_int_enab<<6 |
				kl->pun_ready<<7;
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
			kl->rdr_enab = bus->data & 1;
			kl->rdr_int_enab = bus->data>>6 & 1;
			if(!kl->rdr_int_enab)
				kl->intr_flags &= ~2;
			else if(kl->rdr_done)
				kl->intr_flags |= 2;
			break;
		case 2:
			/* read only */
			break;
		/* Transmit */
		case 4:
			// TODO: MAINT
			kl->maint = bus->data>>2 & 1;
			kl->pun_int_enab = bus->data>>6 & 1;
			if(!kl->pun_int_enab)
				kl->intr_flags &= ~1;
			else if(kl->pun_ready)
				kl->intr_flags |= 1;
			break;
		case 6:
			kl->pun_buf = bus->data;
			kl->pun_halt = 0;
			kl->pun_ready = 0;
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
	char c;
	KL11 *kl = dev;

	NNN++;
//	if(NNN == 20){
	if(NNN == 20000){
	/* transmit */
	if(!kl->pun_halt){
		uint8 c = kl->pun_buf & 0177;
		write(kl->tty.fd, &c, 1);
#ifdef AUTODIAG
	extern int diagpassed;
	if(c == '\a')
		diagpassed = 1;
#endif
		kl->pun_halt = 1;
		kl->pun_ready = 1;
		if(kl->pun_int_enab)
			kl->intr_flags |= 1;
	}

	/* receive */
	if(ttyinput(&kl->tty, &c)){
		kl->rdr_busy = 1;
		kl->rdr_enab = 0;
		kl->rdr_buf = c;
		kl->rdr_busy = 0;
		kl->rdr_done = 1;
		if(kl->rdr_int_enab)
			kl->intr_flags |= 2;
	}
	NNN = 0;
	}

	return kl->intr_flags ? 4 : 0;
}


int
bg_kl11(void *dev)
{
	KL11 *kl = dev;

	// reader interrupt
	if(kl->intr_flags & 2){
		kl->intr_flags &= ~2;
		return 060;
	}

	// punch interrupt
	if(kl->intr_flags & 1){
		kl->intr_flags &= ~1;
		return 064;
	}
	assert(0);	// can't happen
	return 0;
}


void
reset_kl11(void *dev)
{
	KL11 *kl = dev;
	kl->rdr_busy = 0;
	kl->rdr_enab = 0;
	kl->rdr_int_enab = 0;
	kl->rdr_done = 0;
	kl->rdr_buf = 0;

	kl->pun_int_enab = 0;
	kl->maint = 0;
	kl->pun_ready = 1;
	kl->pun_halt = 1;
	kl->pun_buf = 0;

	kl->intr_flags = 0;
}
