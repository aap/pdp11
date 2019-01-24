#include "11.h"

enum {
	SR_C = 1,
	SR_AC_EQ_MQ15 = 2,
	SR_AC_MQ_0 = 4,
	SR_MQ_0 = 010,
	SR_AC_0 = 020,
	SR_AC_1 = 040,
	SR_NEG = 0100,
	SR_OV = 0200
};

enum {
	KE_DIV  = 0777300,
	KE_AC   = 0777302,
	KE_MQ   = 0777304,
	KE_MULT = 0777306,
	KE_SCSR = 0777310,
	KE_NORM = 0777312,
	KE_LSH  = 0777314,
	KE_ASH  = 0777316,
};

#define B31 (1<<31)
#define B30 (1<<30)

static void
setflags(KE11 *ke)
{
	ke->sr &= ~(SR_AC_EQ_MQ15|SR_AC_MQ_0|SR_MQ_0|SR_AC_0|SR_AC_1);
	if(ke->ac == 0 && sgn(ke->mq)==0 ||
	   ke->ac == 0177777 && sgn(ke->mq))
		ke->sr |= SR_AC_EQ_MQ15;
	if(ke->mq == 0 && ke->ac == 0)
		ke->sr |= SR_AC_MQ_0;
	if(ke->mq == 0)
		ke->sr |= SR_MQ_0;
	if(ke->ac == 0)
		ke->sr |= SR_AC_0;
	if(ke->ac == 0177777)
		ke->sr |= SR_AC_1;
}

static void
setflag(KE11 *ke, int flg, uint32 c)
{
	if(c)
		ke->sr |= flg;
	else
		ke->sr &= ~flg;
}

/* Functions are a transcription of the flow charts in the manual.
   May be a bit inefficient but it's nice to be accurate. */

static void
mult(KE11 *ke)
{
	uint32 sum, md;
	int c;

	sum = 0;
	md = ke->ac<<16 | ke->mq;
	c = 0;
	ke->sc = 16;
	do{
		if((ke->x & 1) != c){
			if(ke->x&1)
				sum += ~md + 1;
			else
				sum += md;
		}
		md <<= 1;
		c = ke->x&1;
		ke->x >>= 1;
		ke->sc--;
	}while(ke->sc);
	ke->ac = sum>>16;
	ke->mq = sum;
	ke->sr &= ~SR_C;
	setflags(ke);
}

static uint32
abs32(uint32 x)
{
	return x&B31 ? ~x + 1 : x;
}

static void
divide(KE11 *ke)
{
	uint32 dd, dr;
	uint16 q;
	int c;
	int s;
	int ov;

	dd = ke->ac<<16 | ke->mq;
	dr = ke->x<<16;
	q = 0;
	ov = 0;
	ke->sr &= ~SR_C;
	s = !!(dd&B31);
	ke->sc = 16;
	do{
		c = (dr&B31) == (dd&B31);
		dr >>= 1;
		if(dr&B30)
			dr |= B31;
		q <<= 1;
		q |= c;
		if(c)
			dd += ~dr + 1;
		else
			dd += dr;
		if(ke->sc == 16 && dd != 0 && !!(dd>>31 & 1) == s)
			goto ov;
		ke->sc--;
	}while(ke->sc);

	if(dd == 0 ||
/* is this correct? */
	   dd + abs32(dr) != 0 && !!(dd>>31 & 1) == s){
		c = 1;
		q <<= 1;
		q |= c;
	}else{
		c = (dd&B31) == (dr&B31);
		if(c)
			dd += ~dr + 1;
		else
			dd += dr;
		q += c;
		c = 0;
		q <<= 1;
	}

	/* DOCU manual says DD in one place and DR in another...
	 * which is correct? have to check */
	if((dr>>31 & 1) != (q>>15 & 1) != s)
ov:
		ov = 1;

	ke->ac = dd;
	ke->mq = q;
	setflag(ke, SR_OV, ov != (ke->sr>>6 & 1));
	setflags(ke);
	if(ov)
		setflag(ke, SR_NEG, s);
	return;
}

static void
norm(KE11 *ke)
{
	uint32 sh;

	sh = ke->ac<<16 | ke->mq;
	ke->sc = 0;
	ke->sr &= ~(SR_OV|SR_C);
	setflag(ke, SR_NEG, sh & B31);
	if(ke->sr & SR_NEG)
		ke->sr |= SR_OV;
loop:
	if((sh>>31 & 1) != (sh>>30 & 1))
		goto done;
	if(sh == (3U<<30))
		goto done;
	if(ke->sc == 31)
		goto done;
	sh <<= 1;
	ke->sc++;
	goto loop;
done:
	ke->ac = sh>>16;
	ke->mq = sh;
	setflags(ke);
}

static void
lsh(KE11 *ke)
{
	uint32 sh;
	int ov;

	sh = ke->ac<<16 | ke->mq;
	setflag(ke, SR_NEG, sh&B31);
	ov = 0;
loop:
	ke->sc &= 077;
	if(ke->sc == 0)
		goto done;
	if(ke->sc&040){
		/* negative, shift right */
		setflag(ke, SR_C, sh&1);
		sh >>= 1;
		ke->sr &= ~SR_NEG;
		ke->sc++;
	}else{
		/* positive, shift left */
		setflag(ke, SR_C, sh&B31);
		sh <<= 1;
		if((ke->sr>>6 & 1) != (sh>>31 & 1))
			ov = 1;	/* sign changed */
		setflag(ke, SR_NEG, sh&B31);
		ke->sc--;
	}
	goto loop;
done:
	setflag(ke, SR_OV, ov != (ke->sr>>6 & 1));
	ke->ac = sh>>16;
	ke->mq = sh;
	setflags(ke);
}

static void
ash(KE11 *ke)
{
	uint32 sh;
	int ov;

	sh = ke->ac<<16 | ke->mq;
	setflag(ke, SR_NEG, sh&B31);
	ov = 0;
loop:
	ke->sc &= 077;
	if(ke->sc == 0)
		goto done;
	if(ke->sc&040){
		/* negative, shift right */
		setflag(ke, SR_C, sh&1);
		sh >>= 1;
		if(ke->sr & SR_NEG)
			sh |= B31;
		ke->sc++;
	}else{
		/* positive, shift left */
		sh <<= 1;
		setflag(ke, SR_C, sh&B31);
		if(ke->sr & SR_NEG)
			sh |= B31;
		else
			sh &= ~B31;
		if((ke->sr & SR_C) != (ke->sr>>6 & 1))
			ov = 1;	/* sign changed */
		ke->sc--;
	}
	goto loop;
done:
	setflag(ke, SR_OV, ov != (ke->sr>>6 & 1));
	ke->ac = sh>>16;
	ke->mq = sh;
	setflags(ke);
}

int
dato_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777320){
//		printf("EAE DATO %o %o\n", bus->addr, bus->data);
		switch(bus->addr){
		case KE_DIV:
			ke->x = bus->data;
			divide(ke);
			break;
		case KE_AC:
			ke->ac = bus->data;
			break;
		case KE_MQ:
			ke->mq = bus->data;
			/* sign extend into AC */
			if(ke->mq&0100000)
				ke->ac = 0177777;
			else
				ke->ac = 0;
			break;
		case KE_MULT:
			ke->x = bus->data;
			mult(ke);
			break;
		case KE_SCSR:
			ke->sc = bus->data & 077;
			SETMASK(ke->sr, bus->data>>8, 0301);
			break;
		case KE_NORM:
			norm(ke);
			break;
		case KE_LSH:
			ke->sc = bus->data & 077;
			lsh(ke);
			break;
		case KE_ASH:
			ke->sc = bus->data & 077;
			ash(ke);
			break;
		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

int
datob_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777320){
//		printf("EAE DATOB %o %o\n", bus->addr, bus->data);
		switch(bus->addr){
		case KE_DIV:
			ke->x = sxt(bus->data);
			divide(ke);
			break;
		case KE_AC:
			ke->ac = sxt(bus->data);
			break;
		case KE_AC+1:
			SETMASK(ke->ac, bus->data, 0177400);
			break;
		case KE_MQ:
			ke->mq = sxt(bus->data);
			/* sign extend into AC */
			if(ke->mq&0100000)
				ke->ac = 0177777;
			else
				ke->ac = 0;
			break;
		case KE_MQ+1:
			SETMASK(ke->mq, bus->data, 0177400);
			/* sign extend into AC */
			if(ke->mq&0100000)
				ke->ac = 0177777;
			else
				ke->ac = 0;
			break;
		case KE_MULT:
			ke->x = sxt(bus->data);
			mult(ke);
			break;
		case KE_SCSR:
			break;
		case KE_NORM:
			norm(ke);
			break;
		case KE_LSH:
			ke->sc = bus->data & 077;
			lsh(ke);
			break;
		case KE_ASH:
			ke->sc = bus->data & 077;
			ash(ke);
			break;
		case KE_DIV+1:
		case KE_MULT+1:
		case KE_SCSR+1:
		case KE_NORM+1:
		case KE_LSH+1:
		case KE_ASH+1:
			break;

		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

int
dati_ke11(Bus *bus, void *dev)
{
	KE11 *ke = dev;
	if(bus->addr >= 0777300 && bus->addr < 0777320){
//		printf("EAE DATI %o\n", bus->addr);
		switch(bus->addr){
		case KE_DIV:
		case KE_MULT:
		case KE_LSH:
		case KE_ASH:
			bus->data = 0;
			break;
		case KE_AC:
			bus->data = ke->ac;
			break;
		case KE_MQ:
			bus->data = ke->mq;
			break;
		case KE_SCSR:
			bus->data = WD(ke->sr, ke->sc);
			break;
		case KE_NORM:
			bus->data = ke->sc & 077;
			break;

		default: assert(0);	/* can't happen */
		}
		return 0;
	}
	return 1;
}

void
reset_ke11(void *dev)
{
	KE11 *ke = dev;
	ke->ac = 0;
	ke->mq = 0;
	ke->x = 0;
	ke->sc = 0;
	ke->sr = 0;
	setflags(ke);
}

void
eaetest(KE11 *ke)
{
/*
	// normalize
//	ke->ac = ~0; ke->mq = ~0;
//	ke->ac = 0177770; ke->mq = 0;
//	ke->ac = 0100000; ke->mq = 0;
	ke->ac = 0; ke->mq = 0;
	norm(ke);
*/
/*
	ke->sc = 0100-3;
	ke->ac = 0123456; ke->mq = 0111222;
	ash(ke);
*/
/*
	ke->ac = 0; ke->mq = 00123;
	ke->x = 0321;
	mult(ke);
*/
/*
	ke->ac = 00021; ke->mq = 0;
	ke->x = 0321;
	divide(ke);
*/

/*	int a, b, c, d;
	//some multiplication test
	for(a = 0; a < 0077777; a++){
		for(b = 0; b < 0077777; b++){
			c = a * b;
			ke->ac = 0; ke->mq = a;
			ke->x = b;
			mult(ke);
			d = ke->ac<<16 | ke->mq;
			assert(c == d);
			if(a == 0)
				continue;
			ke->x = a;
			divide(ke);
			assert(ke->ac == 0);
			assert(ke->mq == b);
		}
		printf("%o\n", a);
	}
	// division
	for(a = 0; a < 0077777; a++){
		printf("%o\n", a);
		for(b = 1; b < 0077777; b++){
			ke->ac = 0; ke->mq = a;
			ke->x = b;
			divide(ke);
			assert(ke->mq == a / b);
			assert(ke->ac == a % b);
		}
	}
*/
	
//	printf("%06o %06o %d %o\n", ke->ac, ke->mq, ke->sc, ke->sr);
}
