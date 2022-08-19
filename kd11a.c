#include "11.h"
#include "kd11a.h"

int dotrace;
#undef trace
#define trace if(dotrace) printf

#define KJ11
#define KE11
#define KT11

enum {
	PSW_PR = 0340,
	PSW_T = 020,
	PSW_N = 010,
	PSW_Z = 004,
	PSW_V = 002,
	PSW_C = 001,
};

enum {
	// TODO: do this differently
	TRAP_PWR = 2,	// can't happen
	TRAP_BR7 = 4,
	TRAP_BR6 = 010,
	TRAP_BR5 = 040,
	TRAP_BR4 = 0100,
};

enum {
	FLAG_BERR = 1,	// bus error
	FLAG_TRAP = 2,	// trap instruction
	FLAG_INTR = 4,	// interrupt
	FLAG_STALL = 010,	// inhibit red stack trap
	FLAG_OVFLW = 020,	// yellow or red stack

	FLAG_CKOVF = 040,
	FLAG_CKODA = 0100,
	FLAG_OVFLWERR = 0200,	// not an actual flag
	FLAG_DUBBERR = 0400,	// not an actual flag
	FLAG_FAULT = 01000,	// not an actual flag i think?
};

#define ISSET(f) ((cpu->psw&(f)) != 0)

enum {
	STATE_HALTED = 0,
	STATE_RUNNING,
	STATE_WAITING
};

static uint32
ubxt(word a)
{
	return (a&0160000)==0160000 ? a|0600000 : a;
}

static void
tracestate(KD11A *cpu)
{
	(void)cpu;
	trace(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %06o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->cur<<14 | cpu->prev<<12 | cpu->psw);
	trace(" SR0 %06o SR2 %06o\n",
		cpu->sr0, cpu->sr2);
	trace(" PAR %06o %06o %06o %06o %06o %06o %06o %06o\n"
	       "     %06o %06o %06o %06o %06o %06o %06o %06o\n",
		cpu->par[0], cpu->par[1], cpu->par[2], cpu->par[3],
		cpu->par[4], cpu->par[5], cpu->par[6], cpu->par[7],
		cpu->par[010], cpu->par[011], cpu->par[012], cpu->par[013],
		cpu->par[014], cpu->par[015], cpu->par[016], cpu->par[017]);
}

static void
printstate(KD11A *cpu)
{
	(void)cpu;
	printf(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %06o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->cur<<14 | cpu->prev<<12 | cpu->psw);
	printf(" SR0 %06o SR2 %06o\n",
		cpu->sr0, cpu->sr2);
	printf(" PAR %06o %06o %06o %06o %06o %06o %06o %06o\n"
	       "     %06o %06o %06o %06o %06o %06o %06o %06o\n",
		cpu->par[0], cpu->par[1], cpu->par[2], cpu->par[3],
		cpu->par[4], cpu->par[5], cpu->par[6], cpu->par[7],
		cpu->par[010], cpu->par[011], cpu->par[012], cpu->par[013],
		cpu->par[014], cpu->par[015], cpu->par[016], cpu->par[017]);
	printf(" PDR %06o %06o %06o %06o %06o %06o %06o %06o\n"
	       "     %06o %06o %06o %06o %06o %06o %06o %06o\n",
		cpu->acf[0] | cpu->plf[0]<<2,
		cpu->acf[1] | cpu->plf[1]<<2,
		cpu->acf[2] | cpu->plf[2]<<2,
		cpu->acf[3] | cpu->plf[3]<<2,
		cpu->acf[4] | cpu->plf[4]<<2,
		cpu->acf[5] | cpu->plf[5]<<2,
		cpu->acf[6] | cpu->plf[6]<<2,
		cpu->acf[7] | cpu->plf[7]<<2,
		cpu->acf[010] | cpu->plf[010]<<2,
		cpu->acf[011] | cpu->plf[011]<<2,
		cpu->acf[012] | cpu->plf[012]<<2,
		cpu->acf[013] | cpu->plf[013]<<2,
		cpu->acf[014] | cpu->plf[014]<<2,
		cpu->acf[015] | cpu->plf[015]<<2,
		cpu->acf[016] | cpu->plf[016]<<2,
		cpu->acf[017] | cpu->plf[017]<<2);
}

#ifdef KT11
#define USER (cpu->cur&2)
#define ABORT (cpu->sr0 & 0160000)
#define MFP (cpu->mfp)
#define MTP (cpu->mtp)
#define KJ11
#define KT
#else
#define USER 0
#define KERNEL 1
#define ABORT 0
#define MFP 0
#define MTP 0
#define KT if(0)
#endif

#ifdef KJ11
#define SLR cpu->slr
#else
#define SLR 0
#endif

#define PSW	cpu->psw
#define BA	cpu->ba
#define B	cpu->b
#define D	cpu->d
#define R	cpu->r
#define SP	06
#define PC	07
#define TEMP	010
#define SOURCE	011
#define DEST	012
#define IR	013
#define VECT	014
#define USP	016
#define UNIBUS_DATA cpu->bus->data
#define PBA	cpu->bus->addr

void
reset(KD11A *cpu)
{
	Busdev *bd;

	cpu->traps = 0;
	cpu->flags = 0;

	// KJ11
	cpu->slr = 0;

	// KT11
	cpu->sr0 = 0;
	cpu->sr2 = 0;
	cpu->mfp = 0;
	cpu->mtp = 0;

	for(bd = cpu->bus->devs; bd; bd = bd->next)
		bd->reset(bd->dev);
}

static word
rdps(KD11A *cpu)
{
#ifdef KT11
	return cpu->cur<<14 | cpu->prev<<12 | cpu->psw;
#else
	return cpu->psw;
#endif
}

#ifdef KT11
static word
rdpdr(KD11A *cpu, int i)
{
	return cpu->acf[i] | cpu->plf[i]<<2;
}

static void
wrpar(KD11A *cpu, int i, int b)
{
	if(!b || (BA&1)==0)
		cpu->par[i] = cpu->par[i]&07400 | UNIBUS_DATA&0377;
	if(!b || BA&1)
		cpu->par[i] = UNIBUS_DATA&07400 | cpu->par[i]&0377;
	cpu->acf[i] &= ~0100;	// W bit
}
static void
wrpdr(KD11A *cpu, int i, int b)
{
	if(!b || (BA&1)==0)
		cpu->acf[i] = UNIBUS_DATA & 016;
	if(!b || BA&1)
		cpu->plf[i] = UNIBUS_DATA>>2 & 017700;
	cpu->acf[i] &= ~0100;	// W bit
}

static void
setMM(KD11A *cpu, int mm)
{
	if(cpu->flags & FLAG_STALL){
		cpu->mode = 0;
		cpu->relocate = 0;
		return;
	}
	switch(mm&014){
	case 00: cpu->mode = cpu->cur; break;
	case 04: cpu->mode = cpu->temp; break;
	case 010: cpu->mode = cpu->mtp ? cpu->prev : cpu->cur; break;
	case 014: cpu->mode = cpu->mfp ? cpu->prev : cpu->cur; break;
	}
	cpu->relocate = (cpu->sr0&1) || (cpu->sr0&0400 && mm&1);
}
#else
#define setMM(cpu, mm)
#endif

static int
remapBA(KD11A *cpu, int wr)
{
#ifdef KT11
	int i, pg, abrt;
	cpu->wrpg = nil;
	if(!cpu->relocate){
		PBA = ubxt(BA);
		cpu->flags &= ~FLAG_FAULT;
		return 0;
	}
	pg = BA>>13;
	i = (cpu->mode&2 ? 0 : 8) | pg;
	PBA = ((BA&017777) + (cpu->par[i]<<6)) & 0777777;

	abrt = 0;
	// not resident (or illegal mode)
	if((cpu->acf[i]&2)==0 || (6>>cpu->mode)&1)
		abrt |= 0100000;
	// page length
	if(cpu->acf[i]&010 ? ((BA&017700) < cpu->plf[i]) : ((BA&017700) > cpu->plf[i]))
		abrt |= 0040000;
	// read-only
	if(wr && (cpu->acf[i]&6)==2)
		abrt |= 0020000;
	abrt |= cpu->mode<<5 | pg<<1;
	if(!ABORT)
		SETMASK(cpu->sr0, abrt, 0160156);
	if((abrt&0160000) && !(cpu->flags & FLAG_STALL) && !(cpu->flags&FLAG_CKODA && BA&1)){
		cpu->flags |= FLAG_FAULT;
		return 1;
	}
	if(wr &&
	   !((PBA&0777770) == 0777570 && (PBA&0777776) != 0777570))	// ignore write to SR registers
		cpu->wrpg = &cpu->acf[i];
	cpu->flags &= ~FLAG_FAULT;
	return 0;
#else
	PBA = ubxt(BA);
	return 0;
#endif
}

static int
ovflwerr(KD11A *cpu)
{
	cpu->flags |= FLAG_OVFLWERR | FLAG_STALL;
		cpu->mode = 0;
		cpu->relocate = 0;
	cpu->flags &= ~(FLAG_TRAP | FLAG_INTR);
	return 1;
}

static int
buserr(KD11A *cpu)
{
	if(cpu->flags & FLAG_BERR){
		cpu->flags |= FLAG_DUBBERR | FLAG_STALL;
			cpu->mode = 0;
			cpu->relocate = 0;
	}
	cpu->flags |= FLAG_BERR;
	cpu->flags &= ~(FLAG_TRAP | FLAG_INTR);
	return 1;
}

static int
checkbus(KD11A *cpu, int wr, int pse)
{
	int err = 0;
	int ba = BA&~1;
	if((wr|pse) && !(cpu->mode&2) && cpu->flags&FLAG_CKOVF){
		if(ba == 0177776 ||	// PSW
#ifdef KJ11
		   ba == 0177774 ||	// SLR
#endif
		   ba == 0177570){	// SR
			cpu->flags |= FLAG_OVFLW;	// proc address
			err = ovflwerr(cpu);
// TODO: what's going on here with KT11?
		}else if((BA&0177400) < SLR){
			cpu->flags |= FLAG_OVFLW;
			err = ovflwerr(cpu);
		}else if((BA&0177400) == SLR){
			cpu->flags |= FLAG_OVFLW;	// yellow zone
			if((BA&0340)!=0340)
				err = ovflwerr(cpu);
		}
	}
	if(cpu->flags&FLAG_CKODA && BA&1)
		err = buserr(cpu);
	return err;
}

static int
dati_cpu(KD11A *cpu)
{
	/* internal registers */
	// actually just BA without KT11
	if(PBA > 0760000){
#ifdef KT11
		if((PBA&0777760) == 0772300){
			UNIBUS_DATA = rdpdr(cpu, 8 + (PBA>>1 & 7));
			goto ok;
		}else if((PBA&0777760) == 0772340){
			UNIBUS_DATA = cpu->par[8 + (PBA>>1 & 7)];
			goto ok;
		}else if((PBA&0777760) == 0777600){
			UNIBUS_DATA = rdpdr(cpu, PBA>>1 & 7);
			goto ok;
		}else if((PBA&0777760) == 0777640){
			UNIBUS_DATA = cpu->par[PBA>>1 & 7];
			goto ok;
		}else if((PBA&~1) == 0777572){
			UNIBUS_DATA = cpu->sr0;
			goto ok;
		}else if((PBA&~1) == 0777574){
			UNIBUS_DATA = 0;
			goto ok;
		}else if((PBA&~1) == 0777576){
			UNIBUS_DATA = cpu->sr2;
			goto ok;
		}else
#endif
		if((PBA&0777400) == 0777400){
			switch(PBA&0377){
			case 0170: case 0171:
				UNIBUS_DATA = cpu->sw;
				goto ok;
#ifdef KJ11
			case 0374: case 0375:
				UNIBUS_DATA = cpu->slr;
				goto ok;
#endif
			case 0376: case 0377:
				UNIBUS_DATA = rdps(cpu);
				goto ok;
			}
		}
	}
	return 0;
ok:	return 1;
}

static int
dato_cpu(KD11A *cpu, int b)
{
	word mask;
	mask = b ? (BA&1 ? 0177400 : 0377) : 0177777;

	/* internal registers */
	// actually just BA without KT11
	if(PBA > 0760000){
#ifdef KT11
		if((PBA&0777760) == 0772300){
			wrpdr(cpu, 8 + (PBA>>1 & 7), b);
			goto ok;
		}else if((PBA&0777760) == 0772340){
			wrpar(cpu, 8 + (PBA>>1 & 7), b);
			goto ok;
		}else if((PBA&0777760) == 0777600){
			wrpdr(cpu, PBA>>1 & 7, b);
			goto ok;
		}else if((PBA&0777760) == 0777640){
			wrpar(cpu, PBA>>1 & 7, b);
			goto ok;
		}else if((PBA&~1) == 0777572){
			SETMASK(cpu->sr0, UNIBUS_DATA, mask&0160401);
			goto ok;
		}else if((PBA&~3) == 0777574)
			goto ok;
		else
#endif
		if((PBA&0777400) == 0777400){
			switch(PBA&0377){
			case 0170: case 0171:
				/* can't write switches */
				goto ok;
#ifdef KJ11
			case 0374: case 0375:
				SETMASK(cpu->slr, UNIBUS_DATA, mask&0177400);
				goto ok;
#endif
			case 0376: case 0377:
				if(mask&0377)
					SETMASK(cpu->psw, UNIBUS_DATA, ~PSW_T);
				KT if(mask&0177400){
					cpu->prev = UNIBUS_DATA>>12 & 3;
					cpu->cur = UNIBUS_DATA>>14 & 3;
				}
				goto ok;
			}
		}
	}
	return 0;
ok:	return 1;
}

static int
datix(KD11A *cpu, int pse)
{
trace("dati%s %o %06o ", pse ? "p" : "", cpu->flags, cpu->ba);
	if(remapBA(cpu, pse))
		return 1;
trace(" phys(%o) <- ", PBA);
	if(checkbus(cpu, 0, pse))
		return 1;

	if(dati_cpu(cpu))
		goto ok;

	if((pse ? datip_bus : dati_bus)(cpu->bus))
		return buserr(cpu);
ok:
	trace("%06o\n", UNIBUS_DATA);
	return 0;
}

static int
dati(KD11A *cpu)
{
	return datix(cpu, 0);
}

static void
codes(KD11A *cpu)
{
	SETMASK(cpu->psw, cpu->newpsw, cpu->newmask);
}

static int
dato(KD11A *cpu, int b)
{
	UNIBUS_DATA = D;
trace("dato %o %06o -> %06o ", cpu->flags, cpu->bus->data, cpu->ba);
	if(remapBA(cpu, 1))
		return 1;
trace(" phys(%o)\n", PBA);

	codes(cpu);

	if(checkbus(cpu, 1, 0))
		return 1;

	if(cpu->wrpg)
		*cpu->wrpg |= 0100;	// set if writing to page

	if(dato_cpu(cpu, b))
		goto ok;

	if((b ? datob_bus : dato_bus)(cpu->bus))
		return buserr(cpu);
ok:
	return 0;
}

static word
setBA(KD11A *cpu, word ba, int r, int ov, int oda)
{
	if(ov && r == SP && !(cpu->flags & FLAG_STALL))
		cpu->flags |= FLAG_CKOVF;
	else
		cpu->flags &= ~FLAG_CKOVF;
	if(oda)
		cpu->flags |= FLAG_CKODA;
	else
		cpu->flags &= ~FLAG_CKODA;
	return BA = ba;
}

static int
addr_dst(KD11A *cpu, int m, int b)
{
	int dm, df;
	int ai;

	df = m&7;
	dm = m>>3;
	if(df == SP && USER) df = USP;	// KT11-SP
	ai = 1 + (!b || (df&6)==6 || dm&1);
	switch(dm){
	case 0:
		assert(0);	// we don't do that here
		break;
	case 1:
		// MOV00
		D = setBA(cpu, R[df], df, 1, !b);	// BUG: D missing in microcode flow
		goto MOV07;
	case 2:
		// MOV01
		setBA(cpu, R[df], df, 1, !b);
		D = R[df] + ai;
		goto MOV07;
	case 3:
		// MOV03
		setBA(cpu, R[df], df, 0, 1);
		setMM(cpu, 0);
		D = R[df] + 2;
		R[df] = D;
		if(dati(cpu)) return 1;
		goto MOV11;
	case 4:
		// MOV02
		D = setBA(cpu, R[df]-ai, df, 1, !b);
		MOV07:
		R[df] = D;
		break;
	case 5:
		// MOV04
		D = setBA(cpu, R[df]-2, df, 0, 1);
		R[df] = D;
		setMM(cpu, 0);
		if(dati(cpu)) return 1;
		goto MOV11;
	case 6:
		// MOV05/06
		 setBA(cpu, R[PC], PC, 0, 1);	// only if not still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// MOV08
		B = R[DEST] = UNIBUS_DATA;
		// MOV09
		setBA(cpu, R[df]+B, df, 1, !b);
		break;
	case 7:
		// MOV05/06
		 setBA(cpu, R[PC], PC, 0, 1);	// only if not still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// MOV08
		B = R[DEST] = UNIBUS_DATA;
		// MOV10
		setBA(cpu, R[df]+B, df, 0, 1);	// oda BUG???
		setMM(cpu, 0);
		if(dati(cpu)) return 1;
		MOV11:
		B = R[DEST] = UNIBUS_DATA;
		// MOV12
		setBA(cpu, R[DEST], DEST, 0, !b);
		break;
	}
	return 0;
}

static int
addr_jmp(KD11A *cpu, int m)
{
	int dm, df;

	df = m&7;
	dm = m>>3;
	if(df == SP && USER) df = USP;	// KT11-SP
	switch(dm){
	case 0:
		assert(0);	// we don't do that here
		break;
	case 1:
		// JMP00
		D = R[df];
		JMP04:
		B = R[TEMP] = D;
		break;
	case 2:
		// JMP01
		D = R[df] + 2;
		R[df] = D;
		// JMP02
		D = R[df] - 2;
		goto JMP04;
	case 3:
		// JMP05
		setBA(cpu, R[df], df, 0, 1);
		setMM(cpu, 0);
		D = R[df] + 2;
		R[df] = D;
		if(dati(cpu)) return 1;
		goto JMP11;
	case 4:
		// JMP03
		D = R[df] - 2;
		R[df] = D;
		goto JMP04;
	case 5:
		// JMP06
		D = setBA(cpu, R[df]-2, df, 0, 1);
		setMM(cpu, 0);
		R[df] = D;
		if(dati(cpu)) return 1;
		goto JMP11;
	case 6:
		// JMP08
		 setBA(cpu, R[PC], PC, 0, 1);	// actually still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// JMP14
		B = R[TEMP] = UNIBUS_DATA;
		// JMP15
		D = R[df] + B;
		goto JMP04;
	case 7:
		// JMP07
		 setBA(cpu, R[PC], PC, 0, 1);	// actually still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// JMP09
		B = R[TEMP] = UNIBUS_DATA;
		// JMP10
		D = setBA(cpu, R[df]+B, df, 0, 1);
		setMM(cpu, 0);
		if(dati(cpu)) return 1;
		JMP11:
		B = R[TEMP] = UNIBUS_DATA;
	}
	return 0;
}

static int
fetch_src(KD11A *cpu, int m, int b)
{
	int sm, sf;
	int ai;

	sf = m&7;
	sm = m>>3;
	if(sf == SP && USER) sf = USP;	// KT11-SP
	ai = 1 + (!b || (sf&6)==6 || sm&1);
	switch(sm){
	case 0:
		assert(0);	// we don't do that here
		break;
	case 1:
		// SRC00
		setBA(cpu, R[sf], sf, 0, !b);
		setMM(cpu, 014);
		goto SRC14;
	case 2:
		// SRC01
		setBA(cpu, R[sf], sf, 0, !b);
		setMM(cpu, 014);
		D = R[sf] + ai;
		goto SRC03;
	case 3:
		// SRC04
		setBA(cpu, R[sf], sf, 0, 1);
		setMM(cpu, 0);
		D = R[sf] + 2;
		R[sf] = D;
		if(dati(cpu)) return 1;
		goto SRC12;
	case 4:
		// SRC02
		D = setBA(cpu, R[sf]-ai, sf, 0, !b);
		setMM(cpu, 014);
		goto SRC03;
	case 5:
		// SRC05
		D = setBA(cpu, R[sf]-2, sf, 0, 1);
		setMM(cpu, 0);
		R[sf] = D;
		if(dati(cpu)) return 1;
		goto SRC12;
	case 6:
		// SRC06
		 setBA(cpu, R[PC], PC, 0, 1);	// actually still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// SRC07
		B = R[SOURCE] = UNIBUS_DATA;
		// SRC08
		setBA(cpu, R[sf]+B, sf, 0, !b);
		setMM(cpu, 014);
		goto SRC14;
	case 7:
		// SRC09
		 setBA(cpu, R[PC], PC, 0, 1);	// actually still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// SRC10
		B = R[SOURCE] = UNIBUS_DATA;
		// SRC11
		setBA(cpu, R[sf]+B, sf, 0, 1);
		setMM(cpu, 0);
		if(dati(cpu)) return 1;
		goto SRC12;
//

		SRC03:
		R[sf] = D;
		if(dati(cpu)) return 1;
		goto SRC15;

		SRC12:
		B = R[SOURCE] = UNIBUS_DATA;
		// SRC13
		setBA(cpu, R[SOURCE], SOURCE, 0, !b);
		setMM(cpu, 014);
		SRC14:
		if(dati(cpu)) return 1;
		SRC15:
		B = R[SOURCE] = UNIBUS_DATA;
	}
	if(b && BA&1){
		// SRC16
		D = B&0177400 | (B>>8)&0377;
		// SRC17
		B = R[SOURCE] = D;
	}
	return 0;
}

static int
fetch_dst(KD11A *cpu, int m, int b, int pse)
{
	int dm, df;
	int ai;

	df = m&7;
	dm = m>>3;
	if(df == SP && USER) df = USP;	// KT11-SP
	ai = 1 + (!b || (df&6)==6 || dm&1);
	switch(dm){
	case 0:
		assert(0);	// we don't do that here
		break;
	case 1:
		// DST00
		setBA(cpu, R[df], df, pse, !b);
		setMM(cpu, 1);
		goto DST14;
	case 2:
		// DST01
		setBA(cpu, R[df], df, pse, !b);
		setMM(cpu, 1);
		D = R[df] + ai;
		goto DST03;
	case 3:
		// DST04
		setBA(cpu, R[df], df, 0, 1);
		setMM(cpu, 0);
		D = R[df] + 2;
		R[df] = D;
		if(dati(cpu)) return 1;
		goto DST12;
	case 4:
		// DST02
		D = setBA(cpu, R[df]-ai, df, pse, !b);
		setMM(cpu, 1);
		goto DST03;
		break;
	case 5:
		// DST05
		D = setBA(cpu, R[df]-2, df, 0, 1);
		setMM(cpu, 0);
		R[df] = D;
		if(dati(cpu)) return 1;
		goto DST12;
	case 6:
		// DST06/07
		 setBA(cpu, R[PC], PC, 0, 1);	// only if not still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// DST09
		B = R[DEST] = UNIBUS_DATA;
		// DST10
		setBA(cpu, R[df]+B, df, pse, !b);
		setMM(cpu, 1);
		goto DST14;
	case 7:
		// DST06/07
		 setBA(cpu, R[PC], PC, 0, 1);	// only if not still set from fetch
		setMM(cpu, 0);
		D = R[PC] + 2;
		R[PC] = D;
		if(dati(cpu)) return 1;
		// DST09
		B = R[DEST] = UNIBUS_DATA;
		// DST11
		setBA(cpu, R[df]+B, df, 0, 1);
		setMM(cpu, 0);
		if(dati(cpu)) return 1;
		// DST12
		B = R[DEST] = UNIBUS_DATA;
		// DST13
		setBA(cpu, R[DEST], DEST, pse, !b);
		setMM(cpu, 1);
		goto DST14;
//

		DST03:
		R[df] = D;
		if(datix(cpu, pse)) return 1;
		goto DST15;

		DST12:
		B = R[DEST] = UNIBUS_DATA;
		// DST13:
		setBA(cpu, R[DEST], DEST, pse, !b);
		setMM(cpu, 1);
		DST14:
		if(datix(cpu, pse)) return 1;
		DST15:
		B = R[DEST] = UNIBUS_DATA;
	}
	if(b && BA&1){
		// DST16
		D = B&0177400 | (B>>8)&0377;
		// DST17
		B = R[DEST] = D;
	}
	return 0;
}

static int
writedest(KD11A *cpu, int b)
{
	int d;
	// TODO: can we make this nicer??
	if((cpu->ir & 070) == 0){
		codes(cpu);
		d = cpu->ir & 7;
		if(d == SP && USER) d = USP;	// KT11-SP
		if(b) SETMASK(cpu->r[d], D, 0377);
		else cpu->r[d] = D;
	}else{
		if(b) D = (D<<8) | D&0377;
		setMM(cpu, 1);
		if(dato(cpu, b)) return 1;
	}
	return 0;
}

void
step(KD11A *cpu)
{
	uint by;
	uint br;
	uint c;
	uint src, dst, sf, df, sm, dm;
	int sp;
	word sr, dr;
	word mask, sign;

	uint x, div;
	int res;
	int sc;

#define SVC	goto service
#define TRAP(v)	R[VECT] = D = v; goto trap
#define MOD_NZVC cpu->newmask = (PSW_N|PSW_Z|PSW_V|PSW_C)
#define MOD_NZV cpu->newmask = (PSW_N|PSW_Z|PSW_V)
#define SEV	cpu->newpsw |= PSW_V
#define SEC	cpu->newpsw |= PSW_C
#define SEN	cpu->newpsw |= PSW_N
#define SEZ	cpu->newpsw |= PSW_Z
#define NZ	if(D & sign) cpu->newpsw |= PSW_N;\
		if((D & mask) == 0) cpu->newpsw |= PSW_Z;
#define BR	R[PC] += br
#define CBR(c)	if(((c)>>(PSW&017)) & 1) BR
#define TR(m)	trace("%06o "#m" %06o\n", R[PC]-2, cpu->ir)
#define TRB(m)	trace("%06o "#m"%s %06o\n", R[PC]-2, by ? "B" : "", cpu->ir)

#define DOP(p)	if(sm && fetch_src(cpu, src, by)) goto be;\
		if(dm && fetch_dst(cpu, dst, by, p)) goto be;\
		sr = sm ? R[SOURCE] : R[sf];\
		if(dm == 0) B = R[df]
#define SOP(p)	if(dm && fetch_dst(cpu, dst, by, p)) goto be;\
		dr = dm ? R[DEST] : R[df];
#define WR	if(writedest(cpu, by)) goto be
#define COUT(a, b, s) (((a)^(b))&~(s) | (a)&(b))
#define OV(a, b, s) (~((a)^(b))&((b)^(s)))

	trace("fetch from %06o\n", cpu->r[PC]);
	tracestate(cpu);
	sp = USER ? USP : SP;

	// FET00/01
	setBA(cpu, R[PC], PC, 0, 1);	// assume no overlap
	setMM(cpu, 0);
	if(dati(cpu)) goto be;
	// FET03
	cpu->ir = R[IR] = B = UNIBUS_DATA;
	KT {
		if(!ABORT)
			cpu->sr2 = BA;
		cpu->mtp = cpu->mfp = 0;
	}
recycle:
	// FET04
	D = BA = R[PC] + 2;	// BA not necessary, no overlap
	// FET05
	R[PC] = D;

	by = !!(cpu->ir&B15);
	br = sxt(cpu->ir)<<1;
	src = cpu->ir>>6 & 077;
	sf = src & 7;
	sm = src>>3 & 7;
	KT if(sf == SP && USER) sf = USP;
	dst = cpu->ir & 077;
	df = dst & 7;
	dm = dst>>3 & 7;
	KT if(df == SP && USER) df = USP;
	if(by)	mask = M8, sign = B7;
	else	mask = M16, sign = B15;

	cpu->newpsw = cpu->newmask = 0;
	/* Binary */
	switch(cpu->ir & 0170000){
	case 0110000: case 0010000:	TRB(MOV);
		KT if(MFP && sm == 0 && (sf&7)==SP)
			sf = cpu->prev&2 ? USP : SP;
		KT if(MTP && dm == 0 && (df&7)==SP)
			df = cpu->prev&2 ? USP : SP;
		if(sm && fetch_src(cpu, src, by)) goto be;
		if(dm && addr_dst(cpu, dst, by)) goto be;
		D = sm ? R[SOURCE] : R[sf];
		MOD_NZV;
		NZ;
		if(dm==0){
			codes(cpu);
			if(by) D = R[df] = sxt(D);
			else R[df] = D;
		}else{
			KT if(MFP && sm == 0 && (sf&7)==SP && (cpu->cur&2) == (cpu->prev&2))
				D += 2;	// get SP before update
			if(by) D = (D<<8) | D&0377;
			setMM(cpu, 011);
			if(dato(cpu, by)) goto be;
		}
		SVC;
	case 0120000: case 0020000:	TRB(CMP);
		DOP(0);
		D = sr + ~B + 1;
		MOD_NZVC;
		if(~COUT(sr, ~B, D) & sign) SEC;
		if(OV(sr, ~B, D) & sign) SEV;
		NZ;
		codes(cpu);
		SVC;
	case 0130000: case 0030000:	TRB(BIT);
		DOP(0);
		D = sr & B;
		MOD_NZV;
		NZ;
		codes(cpu);
		SVC;
	case 0140000: case 0040000:	TRB(BIC);
		DOP(1);
		D = ~sr & B;
		MOD_NZV;
		NZ;
		WR;
		SVC;
	case 0150000: case 0050000:	TRB(BIS);
		DOP(1);
		D = sr | B;
		MOD_NZV;
		NZ;
		WR;
		SVC;
	case 0060000:			TR(ADD);
		DOP(1);
		D = sr + B;
		MOD_NZVC;
		if(COUT(sr, B, D) & B15) SEC;
		if(OV(sr, B, D) & B15) SEV;
		NZ;
		WR;
		SVC;
	case 0160000:			TR(SUB);
		by = 0; mask = M16, sign = B15;
		DOP(1);
		D = ~sr + B + 1;
		MOD_NZVC;
		if(~COUT(~sr, B, D) & B15) SEC;
		if(OV(~sr, B, D) & B15) SEV;
		NZ;
		WR;
		SVC;
	/* extended instructions */
	case 0070000:
		switch(sm){
#ifdef KE11
		// EIS - implementation purely functional
		// TODO: check against microcode
		case 0:		TR(MUL);
			if(dm && fetch_src(cpu, dst, 0)) goto be;
			sr = dm ? R[SOURCE] : R[df];
			// TODO: do this differently
			res = (short)sr * (short)R[sf];
			MOD_NZVC;
			if(res < 0) SEN;
			if(res == 0) SEZ;
			if(res < -0100000 || res >= 0100000) SEC;
			codes(cpu);
			R[sf] = res>>16;
			R[sf|1] = res;
			SVC;
		case 1:		TR(DIV);
			if(dm && fetch_src(cpu, dst, 0)) goto be;
			div = (dm ? R[SOURCE] : R[df])<<16;
			x = R[sf]<<16 | R[sf|1];
			MOD_NZVC;
			c = 0;
			if(div & B31){
				c = 1;
				div = -div;
			}
			if(x & B31){
				c ^= 3;
				x = -x;
			}
			if(div == 0) SEC;
			// TODO: is this even correct?
			if(~(x - div) & B31){
				SEV;
				// TODO: what else to do here?
				codes(cpu);
				SVC;
			}
			for(sc = 16; sc; sc--){
				x <<= 1;
				if(~(x-div) & B31){
					x -= div;
					x |= 1;
				}
			}
			D = x;	// quotient
			dr = x>>16;	// remainder
			if(c & 2) dr = -dr;
			if(c & 1) D = -D;
			NZ;
			codes(cpu);
			R[sf] = D;
			R[sf|1] = dr;
			SVC;
		case 2:		TR(ASH);
			if(dm && fetch_src(cpu, dst, 0)) goto be;
			sc = (dm ? R[SOURCE] : R[df]) & 077;
			x = R[sf];
			MOD_NZVC;
			c = 0;
			if(sc & 040){
				for(; sc < 0100; sc++){
					c = x&1;
					x = x>>1 | x&B15;
				}
			}else{
				for(; sc; sc--){
					c = x&B15;
					x = x<<1;
					if((c ^ x) & B15) SEV;
				}
			}
			if(x & B15) SEN;
			if((x&M16) == 0) SEZ;
			if(c) SEC;
			codes(cpu);
			R[sf] = x;
			SVC;
		case 3:		TR(ASHC);
			if(dm && fetch_src(cpu, dst, 0)) goto be;
			sc = (dm ? R[SOURCE] : R[df]) & 077;
			x = R[sf]<<16 | R[sf|1];
			MOD_NZVC;
			c = 0;
			if(sc & 040){
				for(; sc < 0100; sc++){
					c = x&1;
					x = x>>1 | x&B31;
				}
			}else{
				for(; sc; sc--){
					c = x&B31;
					x = x<<1;
					if((c ^ x) & B31) SEV;
				}
			}
			if(x & B31) SEN;
			if(x == 0) SEZ;
			if(c) SEC;
			codes(cpu);
			R[sf] = x>>16;
			R[sf|1] = x;
			SVC;
#else
		case 0:
		case 1:
		case 2:
		case 3:
			goto ri;
#endif

		case 4:		TR(XOR);
			SOP(1);
			D = dr ^ R[sf];
			MOD_NZV;
			NZ;
			WR; SVC;
		case 7:		TR(SOB);
			D = R[sf] - 1;
			R[sf] = D;
			D = R[IR] & 077;
			B = D;
			if(R[sf] != 0){	// actually last D
				D = R[PC] - B;
				R[PC] = D;
				D = R[PC] - B;
				R[PC] = D;
			}
			SVC;
		}
		goto ri;

	/* Reserved instructions */
	case 0170000: goto ri;
	}

	/* Unary */
	switch(cpu->ir & 0007700){
	case 0005000:	TRB(CLR);
		SOP(1);
		D = 0;
		MOD_NZVC;
		NZ;
		WR;
		SVC;
	case 0005100:	TRB(COM);
		SOP(1);
		D = ~dr;
		MOD_NZVC;
		SEC;
		NZ;
		WR; SVC;
	case 0005200:	TRB(INC);
		SOP(1);
		D = dr + 1;
		MOD_NZV;
		if((~dr&D) & sign) SEV;
		NZ;
		WR; SVC;
	case 0005300:	TRB(DEC);
		SOP(1);
		D = dr + ~0;
		MOD_NZV;
		if((dr&~D) & sign) SEV;
		NZ;
		WR; SVC;
	case 0005400:	TRB(NEG);
		SOP(1);
		D = ~dr + 1;
		MOD_NZVC;
		if(D & mask) SEC;
		if((D&dr) & sign) SEV;
		NZ;
		WR; SVC;
	case 0005500:	TRB(ADC);
		SOP(1);
		D = dr + ISSET(PSW_C);
		MOD_NZVC;
		if(COUT(dr, 0, D) & sign) SEC;
		if((D&~dr) & sign) SEV;
		NZ;
		WR; SVC;
	case 0005600:	TRB(SBC);
		SOP(1);
		D = dr + !ISSET(PSW_C)-1;
		MOD_NZVC;
		if(dr == 0 && D == mask) SEC;
		if((~D&dr) & sign) SEV;
		NZ;
		WR; SVC;
	case 0005700:	TRB(TST);
		SOP(0);
		D = dr;
		MOD_NZVC;
		NZ;
		codes(cpu);
		SVC;

	case 0006000:	TRB(ROR);
		SOP(1);
		D = (dr&mask) >> 1 | (ISSET(PSW_C)?sign:0);
		MOD_NZVC;
		NZ;
		if(dr & 1) SEC;
		if((cpu->newpsw>>3^cpu->newpsw)&1) SEV;
		WR; SVC;
	case 0006100:	TRB(ROL);
		SOP(1);
		D = (dr<<1) | ISSET(PSW_C);
		MOD_NZVC;
		NZ;
		if(dr & sign) SEC;
		if((cpu->newpsw>>3^cpu->newpsw)&1) SEV;
		WR; SVC;
	case 0006200:	TRB(ASR);
		SOP(1);
		D = (dr&mask)>>1 | dr&sign;
		MOD_NZVC;
		NZ;
		if(dr & 1) SEC;
		if((cpu->newpsw>>3^cpu->newpsw)&1) SEV;
		WR; SVC;
	case 0006300:	TRB(ASL);
		SOP(1);
		D = dr<<1;
		MOD_NZVC;
		NZ;
		if(dr & sign) SEC;
		if((cpu->newpsw>>3^cpu->newpsw)&1) SEV;
		WR; SVC;

	case 0006400:
		if(by) goto ri;
		TRB(MARK);
		// MRK00
		D = B + R[IR];
		B = D;
		// MRK01
		D = setBA(cpu, R[PC]+(B&0377), PC, 0, 1);	// really SEXB but always positive
		setMM(cpu, 0);
		R[PC] = D;
		// MRK02
		D = R[PC] + 2;
		// MRK03
		R[sp] = D;
		if(dati(cpu)) goto be;
		// MRK04
		D = R[5];
		R[5] = UNIBUS_DATA;
		// MRK05
		R[PC] = D;
		SVC;

#ifdef KT11
	case 0006500:
	case 0106500:	TR(MFPI);
		// change to mov SS,-(sp)
		cpu->mfp = 1;
		x = 010046 | dst<<6;
		goto ktinstr;
	case 0006600:
	case 0106600:	TR(MTPI);
		// change to mov +(sp),DD
		cpu->mtp = 1;
		x = 012600 | dst;
ktinstr:
		// FET06
		D = R[PC] - 2;
		R[PC] = D;
		// FET08
		D = x;		// new instruction
		cpu->ir = D;
		// FET09
		R[IR] = D;
		goto recycle;
#else
	case 0006500:
	case 0106500:
	case 0006600:
	case 0106600:
ktinstr:
		goto ri;
#endif

	case 0006700:
		if(by) goto ri;
		TR(SXT);
		SOP(1);
		D = (PSW & PSW_N) ? 0177777 : 0;
		MOD_NZV;
		NZ;
		WR;
		SVC;
	}

	switch(cpu->ir & 0107400){
	/* Branches */
	case 0000400:	TR(BR); BR; SVC;
	case 0001000:	TR(BNE); CBR(0x0F0F); SVC;
	case 0001400:	TR(BEQ); CBR(0xF0F0); SVC;
	case 0002000:	TR(BGE); CBR(0xCC33); SVC;
	case 0002400:	TR(BLT); CBR(0x33CC); SVC;
	case 0003000:	TR(BGT); CBR(0x0C03); SVC;
	case 0003400:	TR(BLE); CBR(0xF3FC); SVC;
	case 0100000:	TR(BPL); CBR(0x00FF); SVC;
	case 0100400:	TR(BMI); CBR(0xFF00); SVC;
	case 0101000:	TR(BHI); CBR(0x0505); SVC;
	case 0101400:	TR(BLOS); CBR(0xFAFA); SVC;
	case 0102000:	TR(BVC); CBR(0x3333); SVC;
	case 0102400:	TR(BVS); CBR(0xCCCC); SVC;
	case 0103000:	TR(BCC); CBR(0x5555); SVC;
	case 0103400:	TR(BCS); CBR(0xAAAA); SVC;

	case 0004000:
	case 0004400:	TR(JSR);
		if(dm == 0) goto ill;
		if(addr_jmp(cpu, dst)) goto be;
		// JSR00
		D = setBA(cpu, R[sp]-2, sp, 1, 1);
		setMM(cpu, 0);
		R[sp] = D;
		// JSR01
		D = R[sf];
		if(dato(cpu, 0)) goto be;
		// JSR02
		B = R[PC];
		// JSR03
		D = B;
		R[sf] = D;
		// JMP12
		D = R[TEMP];
		// JMP13
		R[PC] = D;
		SVC;
	case 0104000:	TR(EMT); cpu->flags |= FLAG_TRAP; TRAP(030);
	case 0104400:	TR(TRAP); cpu->flags |= FLAG_TRAP; TRAP(034);
	case 0007000:
	case 0007400:
	case 0107000:
	case 0107400:
		goto ri;
	}

	// Hope we caught all instructions we meant to
	assert((cpu->ir & 0177400) == 0);

	/* Misc */
	switch(cpu->ir & 0300){
	case 0100:	TR(JMP);
		if(dm == 0) goto ill;
		if(addr_jmp(cpu, dst)) goto be;
		D = R[TEMP];	// TEMP
		R[PC] = D;
		SVC;
	case 0200:
	switch(cpu->ir&070){
	case 000:	TR(RTS);
		// RTS00
		D = R[df];
		// RTS01
		R[PC] = D;
		// RTS02
		setBA(cpu, R[sp], sp, 0, 1);
		setMM(cpu, 0);
		D = R[sp] + 2;
		R[sp] = D;
		if(dati(cpu)) goto be;
		// RTS03
		R[df] = UNIBUS_DATA;
		SVC;
	case 010: case 020: case 030:
		goto ri;
	case 040: case 050:	TR(CCC); PSW &= ~(cpu->ir&017); SVC;
	case 060: case 070:	TR(SEC); PSW |= cpu->ir&017; SVC;
	}
	case 0300:	TR(SWAB);
		SOP(1);
		D = WD(dr & 0377, (dr>>8) & 0377);
		MOD_NZVC;
		NZ;
		WR;
		SVC;
	}

	/* Operate */
	switch(cpu->ir){
	case 0:	TR(HALT);
		if(USER){
			goto ri;
		}else{
			cpu->state = STATE_HALTED;
			return;
		}
	case 1:	TR(WAIT); cpu->state = STATE_WAITING; SVC;
	case 2:	TR(RTI);
	rti:
		// RTI00
		setBA(cpu, R[sp], sp, 0, 1);
		setMM(cpu, 0);
		D = R[sp] + 2;
		R[sp] = D;
		if(dati(cpu)) goto be;
		// RTI01
		R[PC] = UNIBUS_DATA;
		// RTI02
		setBA(cpu, R[sp], sp, 0, 1);
		setMM(cpu, 0);
		D = R[sp] + 2;
		R[sp] = D;
		if(dati(cpu)) goto be;
		// RTI03
		if(USER){
			SETMASK(cpu->psw, UNIBUS_DATA, mask&037);
		}else{
			cpu->psw = UNIBUS_DATA;
			cpu->prev = UNIBUS_DATA>>12 & 3;
			cpu->cur = UNIBUS_DATA>>14 & 3;
		}
		SVC;
	case 3:	TR(BPT); cpu->flags |= FLAG_TRAP; TRAP(014);
	case 4:	TR(IOT); cpu->flags |= FLAG_TRAP; TRAP(020);
	case 5:	TR(RESET);
		if(USER){
			x = 0240;	// CCC
			goto ktinstr;
		}else{
			reset(cpu);
			SVC;
		}
	case 6:	TR(RTT);
		goto rti;
	}

	// All other instructions should be reserved now

ri:	cpu->flags |= FLAG_TRAP; TRAP(010);
ill:	cpu->flags |= FLAG_TRAP; TRAP(4);

be:
	if(cpu->flags & (FLAG_DUBBERR | FLAG_OVFLWERR)){
		assert(!USER);
		cpu->flags &= ~(FLAG_DUBBERR | FLAG_OVFLWERR);
		R[SP] = 4;
		TRAP(4);
	}
	// oda, nodat, MM fault
	if(cpu->flags & (FLAG_BERR | FLAG_FAULT))
		SVC;
	assert(0);	// shouldn't happen

trap:
	trace("TRAP %o\n", R[VECT]);
	cpu->newmask = 0;	// don't write flags on dato
	cpu->temp = 0;
	setBA(cpu, R[VECT]+2, VECT, 0, 1);
	setMM(cpu, 4);
	if(dati(cpu)) goto be;
	// TRP09
	R[TEMP] = UNIBUS_DATA;		// new PS
	KT {
		cpu->temp = UNIBUS_DATA>>14 & 3;	// TODO: why that?
		sp = (cpu->temp&2) ? USP : SP;
	}
	// TRP10
	D = setBA(cpu, R[sp]-2, sp, 1, 1);
	setMM(cpu, 4);
	R[sp] = D;
	// TRP11
	D = rdps(cpu);
	if(dato(cpu, 0)) goto be;
	// TRP13
	D = setBA(cpu, R[sp]-2, sp, 1, 1);
	setMM(cpu, 4);
	R[sp] = D;
	// TRP14
	D = R[PC];
	if(dato(cpu, 0)) goto be;
	// TRP15
	// MM04 (bug in flow)· does anything happen here?
	// TRP16
	KT {
		cpu->temp = 0;
		cpu->mode = cpu->cur;		// why that?
		cpu->prev = cpu->cur;		// write PS with MM=02
		cpu->cur = R[TEMP]>>14 & 3;
	}
	cpu->psw = R[TEMP];
	cpu->flags &= ~FLAG_STALL;
	// TRP20
	setBA(cpu, R[VECT], VECT, 0, 1);
	setMM(cpu, 4);
	if(dati(cpu)) goto be;
	if((cpu->flags & (FLAG_BERR | FLAG_TRAP | FLAG_INTR)) == 0)
		cpu->flags &= ~FLAG_OVFLW;
	// TRP21
	R[PC] = UNIBUS_DATA;
	cpu->flags &= ~(FLAG_BERR | FLAG_TRAP | FLAG_INTR);

	tracestate(cpu);
	SVC;

service:
	c = PSW >> 5;
	if(cpu->flags & FLAG_FAULT){
		TRAP(0250);
	}else if(cpu->flags & (FLAG_BERR | FLAG_OVFLW)){
		TRAP(4);
	}else if(PSW & PSW_T && cpu->ir != 6){	// not after RTT
		TRAP(014);
	}else if(cpu->traps & TRAP_PWR){
		cpu->traps &= ~TRAP_PWR;
		TRAP(024);
	}else if(c < 7 && cpu->traps & TRAP_BR7){
		cpu->traps &= ~TRAP_BR7;
		cpu->flags |= FLAG_INTR;
		TRAP(cpu->br[3].bg(cpu->br[3].dev));
	}else if(c < 6 && cpu->traps & TRAP_BR6){
		cpu->traps &= ~TRAP_BR6;
		cpu->flags |= FLAG_INTR;
		TRAP(cpu->br[2].bg(cpu->br[2].dev));
	}else if(c < 5 && cpu->traps & TRAP_BR5){
		cpu->traps &= ~TRAP_BR5;
		cpu->flags |= FLAG_INTR;
		TRAP(cpu->br[1].bg(cpu->br[1].dev));
	}else if(c < 4 && cpu->traps & TRAP_BR4){
		cpu->traps &= ~TRAP_BR4;
		cpu->flags |= FLAG_INTR;
		TRAP(cpu->br[0].bg(cpu->br[0].dev));
	}else
		/* fetch next instruction */
		return;
}

static void
svc(KD11A *cpu, Bus *bus)
{
	int l;
	Busdev *bd;
	static int brtraps[4] = { TRAP_BR4, TRAP_BR5, TRAP_BR6, TRAP_BR7 };
	for(l = 0; l < 4; l++){
		cpu->br[l].bg = nil;
		cpu->br[l].dev = nil;
	}
	cpu->traps &= ~(TRAP_BR4|TRAP_BR5|TRAP_BR6|TRAP_BR7);
	for(bd = bus->devs; bd; bd = bd->next){
		l = bd->svc(bus, bd->dev);
		if(l >= 4 && l <= 7 && cpu->br[l-4].bg == nil){
			cpu->br[l-4].bg = bd->bg;
			cpu->br[l-4].dev = bd->dev;
			cpu->traps |= brtraps[l-4];
		}
	}
}

void
run(KD11A *cpu)
{
	cpu->state = STATE_RUNNING;
	while(cpu->state != STATE_HALTED){
		svc(cpu, cpu->bus);

#ifdef AUTODIAG
	extern int diagpassed;
	if(diagpassed)
		return;
#endif

		if(cpu->state == STATE_RUNNING ||
		   cpu->state == STATE_WAITING && cpu->traps){
			cpu->state = STATE_RUNNING;
			step(cpu);
		}
	}

	printstate(cpu);
}
