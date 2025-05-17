#include <wiringPi.h>
#include <stdio.h>
#include "threading.h"
#include "11.h"
#include "ukd11a.h"

// CLK:
// 0,1:	P1
// 2:	P2
// 3:	P2, P3

/*
  P1 and P2:
	CLK BUS
	CLK BA
  P1 and P3:
	CLK B
	WR R
	CLK IR
  P2 only:
	CLK D

  P END and JAM CLK:
	ECLK
	CLK U 56:09
	CLK UPP*PUPP

  PART P END: P END, but not P1
*/

struct timespec
timesub(struct timespec t1, struct timespec t2)
{
	struct timespec d;
	d.tv_sec = t2.tv_sec - t1.tv_sec;
	d.tv_nsec = t2.tv_nsec - t1.tv_nsec;
	if(d.tv_nsec < 0){
		d.tv_nsec += 1000000000;
		d.tv_sec -= 1;
	}
	return d;
}

struct timespec
timeadd(struct timespec t1, struct timespec t2)
{
	struct timespec s;
	s.tv_sec = t1.tv_sec + t2.tv_sec;
	s.tv_nsec = t1.tv_nsec + t2.tv_nsec;
	if(s.tv_nsec >= 1000000000){
		s.tv_nsec -= 1000000000;
		s.tv_sec += 1;
	}
	return s;
}

static void
timeaddns(struct timespec *t, int ns)
{
	t->tv_nsec += ns;
	if(t->tv_nsec >= 1000000000){
		t->tv_nsec -= 1000000000;
		t->tv_sec += 1;
	}
}



Uword urom[] = {
#include "ucode/ucode_40.inc"
};

enum
{
	IR_BYTE_INSTR	= 1,
	IR_NO_DATIP	= 2,
	IR_OVLAP_INSTR	= 4,
	IR_OVLAP_CYCLE	= 010,
	IR_CIN1 = 020,	// CIN=1
	IR_CINC = 040,	// CIN=PS(C)
	IR_UALUS = 0100,	// force ALUS from uword
	IR_ALUM_N = 0200,	// ALUM from PS(N)
	IR_ALUS_C = 0400,	// ALUS from PS(C)
	IR_BYTE_CODES = 01000,
};

IRdecode decodetab[0200000];

enum
{
	PSW_N = 010,
	PSW_Z = 004,
	PSW_V = 002,
	PSW_C = 001
};


int dotrace = 0;
#undef trace
#define trace if(dotrace) printf

#define BA	cpu->ba
#define PBA	cpu->bus->addr
#define UNIBUS_DATA cpu->bus->data


static word
rdps(KD11A *cpu)
{
	// TODO: KT11
	return cpu->ps_flags | cpu->ps_t<<4 | cpu->ps_prio<<5;
}

static void
loadps(KD11A *cpu)
{
	cpu->ps_flags = cpu->dmux&017;
	if(cpu->u.sps==7)
		cpu->ps_t = !!(cpu->dmux&020);
	cpu->ps_prio = (cpu->dmux>>5)&7;
}

static int
dati_cpu(KD11A *cpu)
{
	/* internal registers */
	// actually just BA without KT11
	if(PBA > 0760000){
		if((PBA&0777400) == 0777400){
			switch(PBA&0377){
			case 0170: case 0171:
				UNIBUS_DATA = cpu->con.data_switches;
				goto ok;
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
		if((PBA&0777400) == 0777400){
			switch(PBA&0377){
			case 0170: case 0171:
				/* can't write switches */
				goto ok;
			case 0376: case 0377:
				if(mask&0377 && cpu->u.sps==0)
					loadps(cpu);
				goto ok;
			}
		}
	}
	return 0;
ok:	return 1;
}

static void
businit(KD11A *cpu)
{
	Busdev *bd;
	cpu->cbr = 0;
	cpu->brptr = 0;
	for(bd = cpu->bus->devs; bd; bd = bd->next)
		bd->reset(bd->dev);
}

static uint32
ubxt(word a)
{
	return (a&0160000)==0160000 ? a|0600000 : a;
}

static int
remapBA(KD11A *cpu, int wr)
{
	PBA = ubxt(BA);
	return 0;
}

enum {
	BUS_ODA_ERR = 1,
	BUS_OVFLW_ERR,
	BUS_NODAT
};

static void jamupp(KD11A *cpu);

#define CKOVF (cpu->ckovf && cpu->busc!=0)	// TOOD(KT11): kernel mode
#define ODA_ERR (cpu->ba&1 && cpu->ckoda && !cpu->consl)
#define BOVFLW (cpu->ovfl_cond == 1)
#define OVFLW_ERR (cpu->ovfl_cond == 2)

static void
clr_berr(KD11A *cpu)
{
	// name not quite correct
	cpu->berr = 0;
	cpu->trap = 0;
	cpu->intr = 0;

	cpu->brsv = 0;
	cpu->wait = 0;
}

static void
flag_clr(KD11A *cpu)
{
	cpu->awby = 0;
	cpu->ovlap = 0;
	cpu->bovflw = 0;
}

static void
busstop(KD11A *cpu, int reason)
{
	cpu->busff = 0;
	switch(reason){
	case BUS_ODA_ERR:
printf("ODA ERR\n");
		cpu->jberr = 1;
		jamupp(cpu);
		break;
	case BUS_OVFLW_ERR:
printf("OVFLW ERR\n");
		cpu->jberr = 1;
		jamupp(cpu);
		break;
	case BUS_NODAT:
printf("NODAT\n");
		cpu->nodat = 1;
		jamupp(cpu);
		break;
	}
}

static void
clkptr(KD11A *cpu)
{
	// TODO? we probably won't need it for this model
	// if BUS SACK || NPR || GRANT BR
	// 	return

	cpu->cbr = !!(cpu->con.cntl_switches & SW_HALT);
	cpu->brptr = cpu->brq;
}

// start a bus cycle
// we don't actually MSYN &c. flip-flops
static void
clkmsyn(KD11A *cpu)
{
	remapBA(cpu, 0);
	clkptr(cpu);
	UNIBUS_DATA = cpu->dr;
//printf("BUS cycle %06o %06o\n", cpu->bus->addr, cpu->bus->data);
	if(OVFLW_ERR){
		busstop(cpu, BUS_OVFLW_ERR);
		return;
	}
	if(ODA_ERR){
		busstop(cpu, BUS_ODA_ERR);
		return;
	}
	if(cpu->busc == 0 || cpu->busc == 1){
		// DATI(P)
		if(dati_cpu(cpu) == 0)
			if((cpu->busc == 1 ? datip_bus : dati_bus)(cpu->bus)){
				busstop(cpu, BUS_NODAT);
				return;
			}
		trace("DATI: got %o\n", UNIBUS_DATA);
	}else{
		// DATO(B)
		int b = cpu->busc == 3;

		if(dato_cpu(cpu, b) == 0)
			if((b ? datob_bus : dato_bus)(cpu->bus)){
				busstop(cpu, BUS_NODAT);
				return;
			}
		trace("DATO: wrote %o\n", UNIBUS_DATA);
	}
	cpu->busff = 0;
	cpu->nodat = 0;
	// happens when MSYN and SSYN are asserted
	cpu->clkon = 1;

	timeaddns(&cpu->bus_time, 500);
}

static char *busnames[] = { "DATI", "DATIP", "DATO", "DATOB" };
static void
clkbus(KD11A *cpu)
{
	cpu->busc = cpu->u.bus>>1;
	// set DATOB if byte
	if(cpu->irdec.flags & IR_BYTE_INSTR && cpu->u.dad&1)
		cpu->busc |= 1;
	// clear DATIP for bit/cmp/tst
	if(cpu->irdec.flags & IR_NO_DATIP && cpu->busc==1)
		cpu->busc &= ~1;
	// do not start a cycle under certain conditions
	if(cpu->irdec.flags & IR_NO_DATIP && (cpu->u.dad&012)==012 ||
	   cpu->u.ubf == 037 && !cpu->ovlap_cycle)
		cpu->busff = 0;
	else
		cpu->busff = 1;
//	printf("%d BUS: %s\n", cpu->busff, busnames[cpu->busc]);
}

static void
clkba(KD11A *cpu)
{
	cpu->ckovf = !cpu->stall && (cpu->radr&7)==6 && (cpu->u.dad&016)==6;
	cpu->ckoda = !(cpu->irdec.flags & IR_BYTE_INSTR && cpu->u.dad&1);
	cpu->ba = cpu->bamux;
	cpu->ovfl_cond = 0;
	if(CKOVF){
		int ba = cpu->ba & ~1;
		if(ba < 0400)
			cpu->ovfl_cond = 1;
		if(ba < 0340 ||
		   ba == 0177776 ||	// PSW
//		   ba == 0177774 ||	// SLR TODO(KJ11)
		   ba == 0177570)		// SR
			cpu->ovfl_cond = 2;
	}
}

static void
clkir(KD11A *cpu)
{
	cpu->ir = cpu->dmux;
	cpu->irdec = decodetab[cpu->ir];
	clkptr(cpu);
}

static void
clkflags(KD11A *cpu)
{
	if(cpu->u.sps == 7)
		loadps(cpu);
	else if((cpu->ir&0177740) == 0240){
		if(cpu->u.sps & 2)
			cpu->ps_flags = cpu->ps_flags&1 | cpu->dmux&~1;
		if(cpu->u.sps & 1)
			cpu->ps_flags = cpu->ps_flags&~1 | cpu->dmux&1;
	}else{
		int ndata;
		if(cpu->irdec.flags & IR_BYTE_INSTR)
			ndata = !!(cpu->dr&0200);
		else
			ndata = !!(cpu->dr&0100000);
		if(cpu->u.sps & 2){
			// CLK NVZ
			int zdata, pastb;
			if(cpu->irdec.flags & IR_BYTE_INSTR){
				zdata = (cpu->dr&0377)==0;
				pastb = !!(cpu->bmux&0200);
			}else{
				zdata = cpu->dr==0;
				pastb = !!(cpu->bmux&0100000);
			}
			int vlookup = cpu->pastc<<4 | pastb<<3 | (cpu->ps_flags&1)<<2 | cpu->pasta<<1 | ndata;
			int vdata = (cpu->irdec.vtable>>vlookup)&1;
			cpu->ps_flags &= ~016;
			cpu->ps_flags |= vdata<<1;
			cpu->ps_flags |= zdata<<2;
			cpu->ps_flags |= ndata<<3;
		}
		if(cpu->u.sps & 1){
			// CLK C
			cpu->pastc = cpu->ps_flags&1;
			cpu->ps_flags &= ~1;
			if((cpu->ir&0077500) == 06000)
				cpu->ps_flags |= cpu->dr&1;
			else{
				// what is this weird magic thing?
				int magic = cpu->u.sps==3 && (cpu->ir&070) && (cpu->irdec.flags & IR_BYTE_INSTR && cpu->ba&1);
				int clookup = magic<<4 | cpu->dcry<<3 | (cpu->ps_flags&1)<<2 | cpu->pasta<<1 | ndata;
				cpu->ps_flags |= (cpu->irdec.ctable>>clookup)&1;
			}
		}
	}
}

#define TRACE (cpu->ps_t && cpu->ir == 6)

static int
needservice(KD11A *cpu)
{
	int service1 = TRACE || cpu->berr || cpu->bovflw || cpu->pwrdn;
	return  cpu->ba == 0177570 && (cpu->ir&070)!=0 ||	// TODO: KT11
		cpu->brptr || cpu->cbr || service1;
}

static int
but(KD11A *cpu)
{
	if(cpu->u.ubf == 0)
		return 0;

	int part_p_end = cpu->u.clk&4;	// assume this is the last pulse
	if(part_p_end && cpu->u.ubf >= 030)
		cpu->swtch = 0;

	trace("BUT%02o\n", cpu->u.ubf);
	int bubc = 0;
	int service;
	switch(cpu->u.ubf){
	case 2:
		cpu->reset = 1;
		businit(cpu);
		// fallthrough
	case 1:
		return !(cpu->con.cntl_switches & SW_HALT);

	case 3:
		if(part_p_end){
			cpu->exam = 0;
			if(cpu->u.dad&1)
				cpu->dep = 1;
		}
		// TODO: KT11
		return ((cpu->ba&0177760) == 0177700);

	case 4:
		if(part_p_end){
			cpu->dep = 0;
			if(cpu->u.dad&1)
				cpu->exam = 1;
		}
		// TODO: KT11
		return !((cpu->ba&0177760) == 0177700);

	case 5:
		if(part_p_end){
			cpu->exam = 0;
			cpu->dep = 0;
		}
		return 1;	// really: !cpu->begin;

	case 6:
		if(part_p_end)
			cpu->consl = 1;
		return cpu->swtch;

	case 010:
		if(part_p_end){
			cpu->start = 0;
			cpu->consl = 0;
		}
		return !!(cpu->con.cntl_switches & SW_HALT)<<1;

	case 011:
		// TODO: KT11
		return 1;

	case 012:
		return cpu->dr == 0;

	case 015:
		return (cpu->ir&0177700) == 0004000;

	case 016:
		service = needservice(cpu);
		if(service)
			bubc |= 1;
		break;

	case 017:
		return !!(cpu->ir&010);

	case 021:
		if(cpu->ir&010)
			return 0;
		// fallthrough
	case 022:
		return cpu->irdec.bubc22;

	case 025:
		return cpu->brptr ? 0 :
			cpu->wait ? 3 : 2;

	case 024:
		if(part_p_end){
			cpu->consl = 1;
			cpu->exam = 0;
			cpu->dep = 0;
		}
		return !!(cpu->con.cntl_switches & SW_HALT)<<1;

	case 026:
		if(cpu->ps_t && cpu->ir != 6 ||
		   cpu->berr || cpu->bovflw || cpu->pwrdn)
			bubc = 0;
		else if(!cpu->consl && cpu->con.cntl_switches & SW_HALT)
			bubc = 1;
		else if(cpu->brq || cpu->wait)
			bubc = 2;
		else
			bubc = 3;
		if(part_p_end){
			cpu->consl = 0;
			cpu->exam = 0;
			cpu->dep = 0;
		}
		break;

	case 020:
		if(cpu->irdec.flags & IR_BYTE_INSTR)
			return 3;
		// fallthrough
	case 027:
		service = needservice(cpu);
		if(cpu->ovlap && !service)
			bubc |= 1;
		if(service)
			bubc |= 2;
		break;

	case 030:
		if(cpu->con.cntl_switches & (SW_LOAD_ADDR|SW_EXAM|SW_DEP|SW_CONT))
			bubc |= 4;
		if(cpu->con.cntl_switches & (SW_LOAD_ADDR|SW_CONT) || cpu->start)
			bubc |= 2;
		if(cpu->con.cntl_switches & (SW_LOAD_ADDR|SW_EXAM))
			bubc |= 1;
		break;

	case 031:
		return cpu->irdec.bubc31;

	case 033:
		if(cpu->irdec.flags & IR_BYTE_INSTR && cpu->ba&1)
			return cpu->irdec.bubc33[1];
		else
			return cpu->irdec.bubc33[0];
	case 034:
		if(cpu->irdec.flags & IR_BYTE_INSTR && cpu->ba&1)
			return cpu->irdec.bubc34[1];
		else
			return cpu->irdec.bubc34[0];

	case 035:
		if(cpu->irdec.flags & IR_BYTE_INSTR && cpu->ba&1)
			return cpu->irdec.bubc35[1];
		else
			return cpu->irdec.bubc35[0];
	case 036:
		return cpu->irdec.bubc36;

	case 037:
		if(part_p_end)
			cpu->start = 0;
		// not quite sure this is correct here
		cpu->ovlap = cpu->ovlap_instr;
		// set TRAP = TRAP DATA

		cpu->pastc = 0;

		return cpu->irdec.bubc37 | (cpu->irdec.brconst>>cpu->ps_flags)&1;

	default:
		printf("		not implemented: BUT%02o %o\n", cpu->u.ubf, cpu->pupp);
	}
	return bubc;
}

void
updateALU(KD11A *cpu, int mode, int cin)
{
	word a = cpu->rd;
	word b = cpu->bmux;
	if(mode&020){
		// logical
		switch(mode&017){
		case 000: cpu->alu = ~a; break;
		case 001: cpu->alu = ~a & ~b; break;
		case 002: cpu->alu = ~a & b; break;
		case 003: cpu->alu = 0; break;
		case 004: cpu->alu = ~a | ~b; break;
		case 005: cpu->alu = ~b; break;
		case 006: cpu->alu = a ^ b; break;
		case 007: cpu->alu = a & ~b; break;
		case 010: cpu->alu = ~a | b; break;
		case 011: cpu->alu = ~a ^ b; break;
		case 012: cpu->alu = b; break;
		case 013: cpu->alu = a & b; break;
		case 014: cpu->alu = ~0; break;
		case 015: cpu->alu = a | ~b; break;
		case 016: cpu->alu = a | b; break;
		case 017: cpu->alu = a; break;
		}
		cpu->alu_cry = 0;
	}else{
		// arithmetic
		word xa, xb;
		// some of these should probably be swapped but who cares
		switch(mode&017){
		case 000: xa = a;	xb = 0; break;
		case 001: xa = a | b;	xb = 0; break;
		case 002: xa = a | ~b;	xb = 0; break;
		case 003: xa = ~0;	xb = 0; break;
		case 004: xa = a;	xb = a & ~b; break;
		case 005: xa = a | b;	xb = a & ~b; break;
		case 006: xa = a;	xb = ~b; break;
		case 007: xa = ~a | b;	xb = 0; break;
		case 010: xa = a;	xb = a & b; break;
		case 011: xa = a;	xb = b; break;
		case 012: xa = a | ~b;	xb = a & b; break;
		case 013: xa = ~a | ~b;	xb = 0; break;
		case 014: xa = a;	xb = a; break;
		case 015: xa = a | b;	xb = a; break;
		case 016: xa = a | ~b;	xb = a; break;
		case 017: xa = ~a;	xb = 0; break;
		}
		cpu->alu = xa + xb + cin;
		cpu->alu_cry = (xa^xb)&~cpu->alu | xa&xb;
	}
}

static void
updateDMUX(KD11A *cpu)
{
	switch(cpu->u.sdm){
	case 0:
		cpu->dmux = cpu->rd;
		break;
	case 1:
		cpu->dmux = UNIBUS_DATA;
		break;
	case 2:
		cpu->dmux = cpu->dr;
		break;
	case 3:
		cpu->dmux = cpu->dr>>1 | cpu->dcry<<15;
		break;
	}

}

// update combinational elements
static void
update(KD11A *cpu)
{
// TODO: SP
	cpu->radr = 0;
	if(cpu->u.srx&1)	// SRI
		cpu->radr |= cpu->u.rif;
	if(cpu->u.srx&2)	// SRBA
		cpu->radr |= cpu->ba&017;
	if(cpu->u.srx&4)	// SRD
		cpu->radr |= cpu->ir&7;
	if(cpu->u.srx&010)	// SRS
		cpu->radr |= (cpu->ir>>6)&7;

	int cin = 0;
	int r67, inc2;
	if((cpu->u.sbm&3)==3 || ((cpu->u.sbm>>2)&3)==3)
		switch(cpu->u.sbc){
		case 0:
			if(cpu->stall || cpu->berr)
				cpu->bc = 4;
			else if(cpu->trap)
				cpu->bc = cpu->irdec.stpm;
			else if(TRACE)
				cpu->bc = 014;
			else if(cpu->bovflw)
				cpu->bc = 4;
			else if(cpu->pwrdn)
				cpu->bc = 024;
			break;

		case 001:
			cpu->bc = 1;
			break;
		case 003:
			cpu->bc = 1;
			r67 = cpu->radr==6 || cpu->radr==7;
			inc2 = r67 || !(cpu->irdec.flags & IR_BYTE_INSTR);
			if(cpu->u.alu&1 && inc2)
				cin = 1;
			if(cpu->u.alu&2 && !inc2)
				cin = 1;
			break;
		case 002:
			cpu->bc = 2;
			break;
		case 005:	// 5 undocumented and unused
		case 007:
			cpu->bc = cpu->exam|cpu->dep;
			break;
		case 010:
			cpu->bc = 0177570;
			break;
		case 011:
			cpu->bc = 024;	// TODO? jumpers
			break;
		case 012:
			cpu->bc = 017;
			break;
		case 013:
			cpu->bc = 077;
			break;
		case 014:
			// this seems awfully small
			cpu->bc = !cpu->enable_mclk << 4;
			break;

		case 015:
			cpu->bc = 0250;
			break;

		case 017:
			cpu->bc = 4;
			break;

		default:
		printf("		not implemented: SBC %02o\n", cpu->u.sbc);
			cpu->bc = 0;
		}

	cpu->bmux = 0;
	// BMUX hi
	switch((cpu->u.sbm>>2)&3){
	case 0:
		cpu->bmux |= cpu->br&0177400;
		break;
	case 1:
		cpu->bmux |= (cpu->br&0200) ? 0177400 : 0;
		break;
	case 2:
		cpu->bmux |= (cpu->br<<8)&0177400;
		break;
	case 3:
		cpu->bmux |= cpu->bc&0177400;
		break;
	}

	// BMUX lo
	switch(cpu->u.sbm&3){
	case 0: case 1:
		cpu->bmux |= cpu->br&0377;
		break;
	case 2:
		cpu->bmux |= (cpu->br>>8)&0377;
		break;
	case 3:
		cpu->bmux |= cpu->bc&0377;
		break;
	}

	cpu->rd = 0;
	if(cpu->u.srx)
		cpu->rd |= cpu->reg[cpu->radr];
	if(cpu->u.sps==6 || cpu->u.sps==3 && (cpu->ir&0177740) == 0240)
		cpu->rd |= rdps(cpu);

	int alu;
	if((cpu->u.dad&014) == 014){
		// ALU mode from IR

		if(cpu->irdec.flags & IR_CIN1)
			cin = 1;
		else if(cpu->irdec.flags & IR_CINC)
			cin = cpu->ps_flags&1;

		// ALUM
		if(cpu->irdec.flags & IR_ALUM_N && cpu->ps_flags & PSW_N)
			alu = cpu->irdec.alu2&020;
		else
			alu = cpu->irdec.alu1&020;
		// ALUS
		if(cpu->irdec.flags & IR_UALUS)
			alu |= cpu->u.alu&017;
		else if(cpu->irdec.flags & IR_ALUS_C && cpu->ps_flags & PSW_C)
			alu |= cpu->irdec.alu2&017;
		else
			alu |= cpu->irdec.alu1&017;
	}else{
		// ALU mode from Uword
		if((cpu->u.dad&016) == 010)
			cin = 1;
		alu = cpu->u.alu;
	}
	updateALU(cpu, alu, cin);

	cpu->coutmux = 0;
	switch(cpu->irdec.comux){
	case 0:
		cpu->coutmux = !!(cpu->alu_cry&0100000);
		break;
	case 1:
		cpu->coutmux = !!(cpu->alu_cry&0200);
		break;
	case 2:
		cpu->coutmux = cpu->ps_flags&1;
		break;
	case 3:
		cpu->coutmux = !!(cpu->alu&0100000);
		break;
	}

	updateDMUX(cpu);

	// BAMUX
	switch(cpu->u.sba){
	case 0:
		cpu->bamux = cpu->alu;
		break;
	case 1:
		cpu->bamux = cpu->rd;
		break;
	}
}

static void
uclk(KD11A *cpu)
{
	cpu->pupp = cpu->upp;
	cpu->u = urom[cpu->upp];
	cpu->upp = cpu->u.upf | cpu->bubc;
	trace("clocking %03o %s -> %03o\n", cpu->pupp, cpu->u.name, cpu->upp);
}

static void
p1(KD11A *cpu)
{
	cpu->awby = cpu->u.bus == 2;
	// CLK BUS
	if(cpu->u.bus&1)
		clkbus(cpu);
	// CLK BA
	if(cpu->u.cba)
		clkba(cpu);
	// CLK B
	if(cpu->u.cb)
		cpu->br = cpu->dmux;
	// CLK PS
	if(cpu->u.sps&3)
		clkflags(cpu);
	// WR R
	if(cpu->u.srx){
		switch(cpu->u.wr){
		case 1:
			cpu->reg[cpu->radr] = cpu->reg[cpu->radr]&~0377 | cpu->dmux&0377;
			break;
		case 2:
			cpu->reg[cpu->radr] = cpu->reg[cpu->radr]&0377 | cpu->dmux&~0377;
			break;
		case 3:
			cpu->reg[cpu->radr] = cpu->dmux;
		}
	}
	// CLK IR
	if(cpu->u.cir)
		clkir(cpu);

	// working BUTs (trap sequence)
	switch(cpu->u.ubf){
	case 01:
		if(!(cpu->berr || cpu->trap || cpu->intr || TRACE))
			cpu->bovflw = 0;
		break;
	case 03:
		clr_berr(cpu);
		break;
	case 04:
		if(!(cpu->berr || cpu->trap || cpu->intr || TRACE) && !cpu->bovflw)
			cpu->pwrdn = 0;
		cpu->stall = 0;
		break;
	}
}

static void
p2(KD11A *cpu)
{
	// CLK BUS
	if(cpu->u.bus&1)
		clkbus(cpu);
	if(CKOVF && !(cpu->irdec.flags & IR_NO_DATIP) &&
	   (BOVFLW || OVFLW_ERR))
		cpu->bovflw = 1;
	// CLK BA
	if(cpu->u.cba)
		clkba(cpu);
	// CLK D
	if(cpu->u.cd){
		cpu->dr = cpu->alu;
		cpu->dcry = cpu->coutmux;
		cpu->pasta = cpu->irdec.flags&IR_BYTE_INSTR ? !!(cpu->rd&0200) : !!(cpu->rd&0100000);
	}
}

static void
p3(KD11A *cpu)
{
	cpu->awby = cpu->u.bus == 2;
	// CLK B
	if(cpu->u.cb)
		cpu->br = cpu->dmux;
	// CLK PS
	if(cpu->u.sps&3)
		clkflags(cpu);
	// WR R
	if(cpu->u.srx){
		switch(cpu->u.wr){
		case 1:
			cpu->reg[cpu->radr] = cpu->reg[cpu->radr]&~0377 | cpu->dmux&0377;
			break;
		case 2:
			cpu->reg[cpu->radr] = cpu->reg[cpu->radr]&0377 | cpu->dmux&~0377;
			break;
		case 3:
			cpu->reg[cpu->radr] = cpu->dmux;
		}
		trace("wrote reg R[%o]/%o\n", cpu->radr, cpu->reg[cpu->radr]);
	}
	// CLK IR
	if(cpu->u.cir)
		clkir(cpu);
	if(cpu->pwrdn && cpu->u.sbc==017){
		clr_berr(cpu);
		flag_clr(cpu);
	}

	// working BUTs
	switch(cpu->u.ubf){
	case 01:
		if(!(cpu->berr || cpu->trap || cpu->intr || TRACE))
			cpu->bovflw = 0;
		break;
	case 07:
		cpu->brsv = 0;
		cpu->wait = 0;
		break;
	case 026:
		clkptr(cpu);
		break;
	}
}

static void
updateearly(KD11A *cpu)
{
	cpu->brq = cpu->br_highest > cpu->ps_prio;

	// can next instruction fetch overlap?
	cpu->ovlap_instr = cpu->irdec.flags & IR_OVLAP_INSTR;
	if(TRACE ||
	   cpu->con.cntl_switches & SW_HALT || cpu->pwrdn ||
	   cpu->brq)
		cpu->ovlap_instr = 0;
	// can next memory cycle overlap?
	cpu->ovlap_cycle = (cpu->irdec.flags & IR_OVLAP_CYCLE) || cpu->ovlap_instr;
}

void
microstep(KD11A *cpu)
{
	updateearly(cpu);
	switch(cpu->u.clk>>1){
	case 0: case 1:
		cpu->bubc = but(cpu);
		p1(cpu);
		timeaddns(&cpu->inst_time, 140);
		break;
	case 2:
		cpu->bubc = but(cpu);
		p2(cpu);
		timeaddns(&cpu->inst_time, 200);
		break;
	case 3:
		p2(cpu);
		// D can change, updating DMUX is all we need
		updateDMUX(cpu);
		cpu->bubc = but(cpu);
		p3(cpu);
		timeaddns(&cpu->inst_time, 300);
		break;
	}
	if(cpu->u.clk&1)
		cpu->idle = 1;
	if(cpu->u.clk==0 && cpu->ovlap)
		cpu->idle = 1;
	uclk(cpu);
	update(cpu);

	cpu->clkon = !cpu->idle;
}

static void
jamstart(KD11A *cpu)
{
	if(!cpu->pwrup_init)
		cpu->clkon = 1;
	cpu->jberr = 0;
	cpu->jpup = 0;
	cpu->perr = 0;
	cpu->nodat = 0;
}

static void
jamupp(KD11A *cpu)
{
	cpu->upp = 0;
	int dubberr = (ODA_ERR || cpu->nodat) && cpu->berr;
	if(OVFLW_ERR || dubberr)
		cpu->upp |= 0336;
	if(ODA_ERR)
		cpu->upp |= 2;
	if(cpu->nodat)
		cpu->upp |= cpu->consl ? 030 : 2;
	if(cpu->jpup){
		if(cpu->con.cntl_switches & SW_HALT)
			cpu->upp |= 030;
		else
			cpu->upp |= 0337;
	}
	if(cpu->pwrup_init)
		cpu->upp |= 0377;
	memset(&cpu->u, 0, sizeof(cpu->u));

	if(ODA_ERR || cpu->nodat)
		cpu->berr = !cpu->consl;
	if(cpu->pwrdn || OVFLW_ERR || dubberr)
		cpu->stall = 1;
	cpu->trap = 0;
	cpu->intr = 0;

	// JAM CLK
	printf("JAM %03o\n", cpu->upp);

	cpu->bubc = 0;
	uclk(cpu);
	update(cpu);

	jamstart(cpu);
}

static void
init(KD11A *cpu)
{
	cpu->start = 0;
	cpu->swtch = 0;

	cpu->ckoda = 0;
	cpu->ckovf = 0;
	cpu->busc = 0;
	cpu->jberr = 0;
	cpu->jpup = 0;
	cpu->perr = 0;
	businit(cpu);

	cpu->berr = 0;
	cpu->trap = 0;
	cpu->intr = 0;
	cpu->brsv = 0;
	cpu->pwrdn = 0;
	cpu->stall = 0;
	cpu->wait = 0;
	flag_clr(cpu);

	cpu->ps_flags = 0;
	cpu->pasta = 0;
	cpu->ps_t = 0;
	cpu->ps_prio = 0;

	cpu->busff = 0;
	cpu->nodat = 0;
	clr_berr(cpu);
}

static void
pwr_restart(KD11A *cpu)
{
	cpu->jpup = 1;
	jamupp(cpu);
}

static void
pwrup_init(KD11A *cpu)
{
	cpu->reset = 0;

	cpu->pwrup_init = 1;
	init(cpu);
	jamupp(cpu);
	cpu->pwrup_init = 0;
}

void
KD11A_poweron(KD11A *cpu)
{
/// TMP
for(int i = 0; i < 8; i++)
cpu->reg[i] = i+0700;
	pwrup_init(cpu);
	pwr_restart(cpu);
}

static void
svc(KD11A *cpu, Bus *bus)
{
	int l;
	Busdev *bd;
	for(l = 0; l < 4; l++){
		cpu->busreq[l].bg = nil;
		cpu->busreq[l].dev = nil;
	}
	cpu->br_highest = 0;
	for(bd = bus->devs; bd; bd = bd->next){
		l = bd->svc(bus, bd->dev);
		if(l >= 4 && l <= 7 && cpu->busreq[l-4].bg == nil){
			cpu->busreq[l-4].bg = bd->bg;
			cpu->busreq[l-4].dev = bd->dev;
			if(cpu->br_highest < l)
				cpu->br_highest = l;
		}
	}
}

void
KD11A_service(KD11A *cpu)
{
	lock(&cpu->con.sw_lock);
	int toggled_switches = cpu->con.toggled_switches;
	cpu->con.toggled_switches = 0;
	unlock(&cpu->con.sw_lock);

	int mclk = toggled_switches & SW_LAMPTEST;
	cpu->enable_mclk = !!(cpu->con.data_switches & 010000000);

	cpu->bbsy = 1;	// i don't think we'll ever not own the bus in this model
	if(cpu->awby && cpu->idle && cpu->bbsy)
		cpu->clkon = 1;
	if(cpu->enable_mclk)
		cpu->clkon = 0;

	if(toggled_switches & SW_START){
		cpu->start = 1;
		cpu->swtch = 1;
	}
	if(toggled_switches & (SW_LOAD_ADDR|SW_EXAM|SW_DEP|SW_CONT))
		cpu->swtch = 1;
// not really sure how to do this...
	if((cpu->con.cntl_switches & (SW_HALT|SW_START)) == (SW_HALT|SW_START))
		init(cpu);
	if(mclk || cpu->clkon){
		cpu->idle = 0;
		microstep(cpu);
	}
	if(cpu->busff && cpu->idle)
		clkmsyn(cpu);

	// this can change any time
	if(cpu->u.sdm == 1)
		cpu->dmux = UNIBUS_DATA;

	int top_lights = cpu->pupp;
/*
	top_lights |= cpu->consl<<11;
	top_lights |= cpu->exam<<10;
	top_lights |= cpu->dep<<9;
*/
	top_lights |= cpu->berr<<10;
	top_lights |= !cpu->consl<<9;
	cpu->con.status_lights = top_lights;

	int addr = ubxt(cpu->ba);
	addr |= cpu->ps_flags<<18;
	cpu->con.address_lights = addr;
	cpu->con.data_lights = cpu->dmux;

	// TODO: don't do this if we can't allow NPR
//	if(cpu->step++ & 020)
//		svc(cpu, cpu->bus);

	if(cpu->idle && cpu->reset){
		delay(70);
		cpu->reset = 0;
		cpu->clkon = 1;
	}


	if(cpu->pupp == 032){
		printf("starting timer\n");
		clock_gettime(CLOCK_REALTIME, &cpu->start_time);
		cpu->inst_time.tv_sec = 0;
		cpu->inst_time.tv_nsec = 0;
		cpu->bus_time.tv_sec = 0;
		cpu->bus_time.tv_nsec = 0;
//	}else if(cpu->enable_mclk || cpu->consl){
	}else if(cpu->pupp == 041 || cpu->pupp == 030){
		printf("stopping timer\n");
		struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
		struct timespec total = timeadd(cpu->inst_time, cpu->bus_time);
		struct timespec diff = timesub(cpu->start_time, now);
		printf("Time: inst:%ld.%09ld  bus:%ld.%09ld\n",
			cpu->inst_time.tv_sec, cpu->inst_time.tv_nsec,
			cpu->bus_time.tv_sec, cpu->bus_time.tv_nsec);
		printf("      total:%ld.%09ld  real:%ld.%09ld\n",
			total.tv_sec, total.tv_nsec,
			diff.tv_sec, diff.tv_nsec);
	}
}

static void
decode(word ir)
{
	int kt_instr = 0;	// TODO: KT11

	int ir15 = !!(ir&0100000);
	int ir11 = !!(ir&0004000);
	int ir10 = !!(ir&0002000);
	int ir09 = !!(ir&0001000);
	int ir08 = !!(ir&0000400);
	int ir05 = !!(ir&0000040);
	int ir04 = !!(ir&0000020);
	int ir03 = !!(ir&0000010);

	int ir_14_12_0 = (ir&0070000) == 0000000;
	int mov = (ir&0070000) == 0010000;
	int cmp = (ir&0070000) == 0020000;
	int bit = (ir&0070000) == 0030000;
	int bic = (ir&0070000) == 0040000;
	int bis = (ir&0070000) == 0050000;
	int add_sub = (ir&0070000) == 0060000;
	int ir_14_12_7 = (ir&0070000) == 0070000;
	int add = add_sub && !(ir&0100000);
	int sub = add_sub && (ir&0100000);

	int sm = (ir>>9)&7;
	int dm = (ir>>3)&7;
	int sm0 = sm==0;
	int sm6 = sm==6;
	int sm7 = sm==7;
	int dm0 = dm==0;
	int dm6 = dm==6;
	int dm7 = dm==7;
	int dr7 = (ir&7)==7;

	int sop = (ir&0077000) == 0005000;
	int clr = sop && (ir&0700)==0000;
	int com = sop && (ir&0700)==0100;
	int inc = sop && (ir&0700)==0200;
	int dec = sop && (ir&0700)==0300;
	int neg = sop && (ir&0700)==0400;
	int adc = sop && (ir&0700)==0500;
	int sbc = sop && (ir&0700)==0600;
	int tst = sop && (ir&0700)==0700;
	int bit_cmp_tst = bit || cmp || tst;

	int xor = (ir&0177000) == 0074000;
	int rotshf = (ir&0077400) == 0006000;
	int rotshf_l = rotshf && ir&0100;
	int rotshf_r = rotshf && !(ir&0100);
	int sxt = (ir&0177700) == 0006700;
	int dop = !ir_14_12_0 && !ir_14_12_7;
	int swab = (ir&0177700) == 0000300;
	int jmp = (ir&0177700) == 0000100;
	int jsr = (ir&0177700) == 0004000;
	int jmp_jsr = jmp || jsr;
	int allsop = sop || rotshf || sxt || swab;
	int byte_instr = !add_sub && ir15 && (rotshf || dop || sop);

	int sopmore = sop || rotshf_l;
	int rts = (ir&0177770) == 0200;
	int cc_instr = (ir&0177740) == 0240;

	int mark = (ir&0177700) == 0006400;
	int emt_trap = (ir&0177000) == 0104000;
	int emt = emt_trap && !(ir&0400);
	int trap = emt_trap && (ir&0400);
	int partdop_reg = (xor || !mov && !ir_14_12_0 && !ir_14_12_7 && sm0) && dm0;
	int sob = (ir&0177000) == 0077000;

	int rti_rtt = (ir&0177773) == 0000002;
	int rtt = rti_rtt && (ir&4);
	int reset = ir == 0000005;	// TODO: KT11
	int halt = ir == 0000000;	// TODO: KT11
	int wait = ir == 0000001;
	int iot = ir == 0000004;
	int bpt = ir == 0000003;
	int trap_instr = iot || bpt || emt_trap;

	int br_instr = ir_14_12_0 && (sm==0 && (ir15 || !ir15 && ir08) || sm==1 || sm==2 || sm==3);

	int rot_r = rotshf && (ir&0700)==0000;
	int rot_l = rotshf && (ir&0700)==0100;
	int shf_r = rotshf && (ir&0700)==0200;

	int bubc37 = 0;
	int bubc37_a = !dm0 && (allsop||dop) && !mov && (allsop||sm0);
	int bubc37_b = mov&&sm0 || jmp_jsr;
	int cinstr = ir04 && cc_instr ||
		rotshf_r&&dm0 && byte_instr ||
		(neg || reset) && dm0 ||
		sub && sm0 && dm0;
	int i1k4 = kt_instr || swab&&dm0 || sxt&&dm0 || sob || trap_instr || rts || halt || reset;
	int i1k3 = br_instr || mark || wait || cc_instr || sob || sxt&&dm0 || swab&&dm0 || kt_instr;
	int i1k2 = sopmore&&dm0 || rotshf_r&&dm0 || wait || cc_instr || rts || trap_instr || swab&&dm0 || kt_instr;
	int i1k1 = rotshf_r&&dm0 || mark || partdop_reg || cc_instr || halt || reset || sxt&&dm0 || trap_instr || kt_instr;
	int i1k0 = rti_rtt || cinstr;	// also set if we branch, but we only know that at runtime
	if(xor&&!dm0 ||
	   dop&&!sm0 ||
	   bubc37_b ||
	   bubc37_a)
		bubc37 |= 040;
	if(xor&&!dm0 ||
	   bubc37_a ||
	   i1k4 ||
	   mov&&sm0)
		bubc37 |= 020;
	if(bubc37_b || i1k3)
		bubc37 |= 010;
	int i1src = dop&&!sm0;
	if(ir11 && i1src ||
	   reset ||
	   i1k2 ||
	   ir05 && !i1src && (bubc37&040))
		bubc37 |= 004;
	if(ir10 && i1src ||
	   i1k1 ||
	   ir04 && !i1src && (bubc37&040))
		bubc37 |= 002;
	if(ir09 && i1src ||
	   ir03 && jmp_jsr ||
	   i1k0 ||
	   ir03 && !dm7 && !i1k4 && (bubc37&020))
		bubc37 |= 001;

	// NOTE: this can be disabled by external factors!
	int ovlap_instr =
		(allsop || xor || dop&&sm0) &&
		!(mov&&ir15) &&
		!dr7 &&
		dm0;
	// NOTE: this has ovlap_instr ORed in
	int ovlap_cycle =
		(dm6||dm7) && (allsop || xor || dop&&sm0 || jmp || jsr) ||
		(sm6||sm7) && dop;

	int bubc22 = 0;
	if(sm0)
		bubc22 |= 1;
	if(byte_instr)
		bubc22 |= 2;

	// BUBC31
	int bubc31 = 0;
	if(!bit_cmp_tst && byte_instr)
		bubc31 |= 1;
	if(bit_cmp_tst)
		bubc31 |= 2;

	// BUBC34
	int bubc34 = 0;
	if(dop&&!sm0 || neg || rotshf_r && byte_instr)
		bubc34 |= 001;
	if(rotshf_r || swab || sub)
		bubc34 |= 002;	// also set by SOPMORE && ODD BYTE
	if(sxt || swab || xor)
		bubc34 |= 004;	// also set by DOP && ~ODD BYTE
	if(swab || sxt || rotshf_r)
		bubc34 |= 010;	// also set by DOP && ODD BYTE
	int bubc34_odd = bubc34;
	if(sopmore)
		bubc34_odd |= 002;
	if(dop){
		bubc34_odd |= 010;
		bubc34 |= 004;
	}

	// BUBC33	BUBC34 modified by ODD BYTE!
	int bubc33 = bubc34;
	int bubc33_odd = 017;

	// BUBC36
	int bubc36 = 0;
	if(ir03 || dm6 || sub&&dm0)
		bubc36 |= 001;
	if(ir04)
		bubc36 |= 002;
	if(ir05)
		bubc36 |= 004;
	if(mov&&!dm0)
		bubc36 |= 010;
	if(mov||!dm0)
		bubc36 |= 040;

	// BUBC35	BUBC36 modified by ODD BYTE!
	int bubc35 = bubc36;
	int bubc35_odd = 017;

	int rsvd_instr = bubc37 == 0;
	int ill_instr = bubc37 == 050;

	// ALU
	int comux = 0;
	if(shf_r || rot_r)
		comux |= 2;
	if(shf_r || !rot_r && byte_instr)
		comux |= 1;

	int alum = 0;
	if(bic || bit || xor || clr || com)
		alum = 1;
	int alus = 0;
	if(add_sub || bis || bit || dec || clr)
		alus |= 001;
	if(xor || cmp || bic || bit || dec || clr)
		alus |= 002;
	if(rotshf_l || dec || cmp || xor)
		alus |= 004;
	if(bit || rotshf_l || add_sub || dec)
		alus |= 010;
	int alu1 = alus | alum<<4;
	int alu2 = alu1;
	// if sxt, ALUS from U, ALUM = !PS_N
	if(sxt)
		alu1 |= 020;
	// if sbc, ALUS = 1111*C
	if(sbc)
		alu2 |= 017;

	// status flags, rather complex!
	uint ctable = 0;
	uint vtable = 0;
	for(int i = 0; i < 32; i++){
		int ndata = !!(i&1);
		int pasta = !!(i&2);
		int ps_c = !!(i&4);
		int dc = !!(i&010);
		int pastb = !!(i&010);
		int magic = !!(i&020);
		int pastc = !!(i&020);

		int vdata = 0;
		int subop = cmp || dec || sbc&&pastc || sub || neg;
		int carryop = adc || cmp || inc || add | subop;
		int signb = subop && !(!(dec || sbc&&pastc) && pastb) || add && pastb;
		int signa = !neg && pasta;
		if(rotshf && ps_c != ndata ||
		   ndata && carryop && !signb && !signa ||
		   !ndata && carryop && signb && signa)
			vdata = 1;

		int cdata = 0;
		int keepc = magic || bis || bic || bit || inc || sxt || xor || mov || dec;
		int subopc = com || subop&&!magic&&!dec;
		if(dc && !(sbc || subopc || rotshf_r || keepc) ||	// addition
		   sbc && ps_c && !pasta && ndata ||			// sbc
		   !dc && !sbc && subopc ||				// borrow
		   ps_c && keepc)
		// ROTSHF(R) missing because we only want 32 bits
			cdata = 1;

		vtable |= vdata<<i;
		ctable |= cdata<<i;
	}

	int stpm = 0;
	if(iot || trap || emt)
		stpm |= 020;
	if(trap || emt || rsvd_instr || bpt)
		stpm |= 010;
	if(trap || bpt || ill_instr)
		stpm |= 4;

	IRdecode irdec;
	irdec.flags = 0;
	if(byte_instr)
		irdec.flags |= IR_BYTE_INSTR;
	if(bit_cmp_tst)
		irdec.flags |= IR_NO_DATIP;
	if(ovlap_instr)
		irdec.flags |= IR_OVLAP_INSTR;
	if(ovlap_cycle)
		irdec.flags |= IR_OVLAP_CYCLE;
	if(cmp || inc)
		irdec.flags |= IR_CIN1;
	if(adc || rot_l)
		irdec.flags |= IR_CINC;
	if(sxt)
		irdec.flags |= IR_ALUM_N | IR_UALUS;
	if(sbc)
		irdec.flags |= IR_ALUS_C;
	if(byte_instr || swab)
		irdec.flags |= IR_BYTE_CODES;
	switch(ir&0177400){
	case 0000400: irdec.brconst = 0xFFFF; break;
	case 0001000: irdec.brconst = 0x0F0F; break;
	case 0001400: irdec.brconst = 0xF0F0; break;
	case 0002000: irdec.brconst = 0xCC33; break;
	case 0002400: irdec.brconst = 0x33CC; break;
	case 0003000: irdec.brconst = 0x0C03; break;
	case 0003400: irdec.brconst = 0xF3FC; break;
	case 0100000: irdec.brconst = 0x00FF; break;
	case 0100400: irdec.brconst = 0xFF00; break;
	case 0101000: irdec.brconst = 0x0505; break;
	case 0101400: irdec.brconst = 0xFAFA; break;
	case 0102000: irdec.brconst = 0x3333; break;
	case 0102400: irdec.brconst = 0xCCCC; break;
	case 0103000: irdec.brconst = 0x5555; break;
	case 0103400: irdec.brconst = 0xAAAA; break;
	default: irdec.brconst = 0; break;
	}
	irdec.ctable = ctable;
	irdec.vtable = vtable;
	irdec.bubc22 = bubc22;
	irdec.bubc31 = bubc31;
	irdec.bubc33[0] = bubc33;
	irdec.bubc33[1] = bubc33_odd;
	irdec.bubc34[0] = bubc34;
	irdec.bubc34[1] = bubc34_odd;
	irdec.bubc35[0] = bubc35;
	irdec.bubc35[1] = bubc35_odd;
	irdec.bubc36 = bubc36;
	irdec.bubc37 = bubc37;
	irdec.comux = comux;
	irdec.alu1 = alu1;
	irdec.alu2 = alu2;
	irdec.stpm = stpm;
	decodetab[ir] = irdec;
}

void
KD11A_initonce(void)
{
	int i;
	for(i = 0; i < 0200000; i++)
		decode(i);
}
