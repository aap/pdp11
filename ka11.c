#include "11.h"
#include "ka11.h"

int dotrace;
#undef trace
#define trace if(dotrace) printf

enum {
	PSW_PR = 0340,
	PSW_T = 020,
	PSW_N = 010,
	PSW_Z = 004,
	PSW_V = 002,
	PSW_C = 001,
};

enum {
	TRAP_STACK = 1,
	TRAP_PWR = 2,	// can't happen
	TRAP_BR7 = 4,
	TRAP_BR6 = 010,
	TRAP_BR5 = 040,
	TRAP_BR4 = 0100,
	TRAP_CSTOP = 01000	// can't happen?
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

void
tracestate(KA11 *cpu)
{
	(void)cpu;
	trace(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %03o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->psw);
}

void
printstate(KA11 *cpu)
{
	(void)cpu;
	printf(" R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\n"
		" 10 %06o 11 %06o 12 %06o 13 %06o 14 %06o 15 %06o 16 %06o 17 %06o\n"
		" BA %06o IR %06o PSW %03o\n"
		,
		cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3],
		cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7],
		cpu->r[8], cpu->r[9], cpu->r[10], cpu->r[11],
		cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15],
		cpu->ba, cpu->ir, cpu->psw);
}

void
reset(KA11 *cpu)
{
	Busdev *bd;

	cpu->traps = 0;

	for(bd = cpu->bus->devs; bd; bd = bd->next)
		bd->reset(bd->dev);
}

int
dati(KA11 *cpu, int b)
{
trace("dati %06o: ", cpu->ba);
	if(!b && cpu->ba&1)
		goto be;

	/* internal registers */
	if((cpu->ba&0177400) == 0177400){
		switch(cpu->ba&0377){
		case 0170: case 0171:
			cpu->bus->data = cpu->sw;
			goto ok;
		case 0376: case 0377:
			cpu->bus->data = cpu->psw;
			goto ok;
		}
	}

	cpu->bus->addr = ubxt(cpu->ba)&~1;
	if(dati_bus(cpu->bus))
		goto be;
ok:
	trace("%06o\n", cpu->bus->data);
	cpu->be = 0;
	return 0;
be:
	trace("BE\n");
	cpu->be++;
	return 1;
}

int
dato(KA11 *cpu, int b)
{
trace("dato %06o %06o %d\n", cpu->ba, cpu->bus->data, b);
	if(!b && cpu->ba&1)
		goto be;

	/* internal registers */
	if((cpu->ba&0177400) == 0177400){
		switch(cpu->ba&0377){
		case 0170: case 0171:
			/* can't write switches */
			goto ok;
		case 0376: case 0377:
			/* writes 0 for the odd byte.
			   I think this is correct. */
			cpu->psw = cpu->bus->data;
			goto ok;
		}
	}

	if(b){
		cpu->bus->addr = ubxt(cpu->ba);
		if(datob_bus(cpu->bus))
			goto be;
	}else{
		cpu->bus->addr = ubxt(cpu->ba)&~1;
		if(dato_bus(cpu->bus))
			goto be;
	}
ok:
	cpu->be = 0;
	return 0;
be:
	cpu->be++;
	return 1;
}

static void
svc(KA11 *cpu, Bus *bus)
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

static int
addrop(KA11 *cpu, int m, int b)
{
	int r;
	int ai;
	r = m&7;
	m >>= 3;
	ai = 1 + (!b || (r&6)==6 || m&1);
	if(m == 0){
//		assert(0);
		return 0;
	}
	switch(m&6){
	case 0:		// REG
		cpu->b = cpu->ba = cpu->r[r];
		return 0;	// this already is mode 1
	case 2:		// INC
		cpu->ba = cpu->r[r];
		cpu->b = cpu->r[r] = cpu->r[r] + ai;
		break;
	case 4:		// DEC
		cpu->b = cpu->ba = cpu->r[r]-ai;
		if(r == 6 && (cpu->ba&~0377) == 0) cpu->traps |= TRAP_STACK;
		cpu->r[r] = cpu->ba;
		break;
	case 6:		// INDEX
		cpu->ba = cpu->r[7];
		cpu->r[7] += 2;
		if(dati(cpu, 0)) return 1;
		cpu->b = cpu->ba = cpu->bus->data + cpu->r[r];
		break;
	}
	if(m&1){
		if(dati(cpu, 0)) return 1;
		cpu->b = cpu->ba = cpu->bus->data;
	}
	return 0;

}

static int
fetchop(KA11 *cpu, int t, int m, int b)
{
	int r;
	r = m&7;
	if((m&070) == 0)
		cpu->r[t] = cpu->r[r];
	else{
		if(dati(cpu, b)) return 1;
		cpu->r[t] = cpu->bus->data;
		if(b && cpu->ba&1) cpu->r[t] = cpu->r[t]>>8;
	}
	if(b) cpu->r[t] = sxt(cpu->r[t]);
	return 0;
}

static int
readop(KA11 *cpu, int t, int m, int b)
{
	return !(addrop(cpu, m, b) == 0 && fetchop(cpu, t, m, b) == 0);
}

static int
writedest(KA11 *cpu, word v, int b)
{
	int d;
	if((cpu->ir & 070) == 0){
		d = cpu->ir & 7;
		if(b) SETMASK(cpu->r[d], v, 0377);
		else cpu->r[d] = v;
	}else{
		if(cpu->ba&1) v <<= 8;
		cpu->bus->data = v;
		if(dato(cpu, b)) return 1;
	}
	return 0;
}

static void
setnz(KA11 *cpu, word w)
{
	cpu->psw &= ~(PSW_N|PSW_Z);
	if(w & 0100000) cpu->psw |= PSW_N;
	if(w == 0) cpu->psw |= PSW_Z;
}

void
step(KA11 *cpu)
{
	uint by;
	uint br;
	uint b;
	uint c;
	uint src, dst, sf, df, sm, dm;
	word mask, sign;
	int inhov;
	byte oldpsw;

//	printf("fetch from %06o\n", cpu->r[7]);
//	printstate(cpu);

#define SP	cpu->r[6]
#define PC	cpu->r[7]
#define SR	cpu->r[010]
#define DR	cpu->r[011]
#define TV	cpu->r[012]
#define BA	cpu->ba
#define PSW	cpu->psw
#define RD_B	if(sm != 0) if(readop(cpu, 010, src, by)) goto be;\
		if(dm != 0) if(readop(cpu, 011, dst, by)) goto be;\
		if(sm == 0) fetchop(cpu, 010, src, by);\
		if(dm == 0) fetchop(cpu, 011, dst, by)
#define RD_U	if(dm != 0) if(readop(cpu, 011, dst, by)) goto be;\
		if(dm == 0) fetchop(cpu, 011, dst, by);\
		SR = DR
#define WR	if(writedest(cpu, b, by)) goto be
#define NZ	setnz(cpu, b)
#define SVC	goto service
#define TRAP(v)	TV = v; goto trap
#define CLC	cpu->psw &= ~PSW_C
#define CLV	cpu->psw &= ~PSW_V
#define CLCV	cpu->psw &= ~(PSW_V|PSW_C)
#define SEV	cpu->psw |= PSW_V
#define SEC	cpu->psw |= PSW_C
#define C	if(b & 0200000) SEC
#define NC	if((b & 0200000) == 0) SEC
#define BXT	if(by) b = sxt(b)
#define BR	PC += br
#define CBR(c)	if(((c)>>(cpu->psw&017)) & 1) BR
#define PUSH	SP -= 2; if(!inhov && (SP&~0377) == 0) cpu->traps |= TRAP_STACK
#define POP	SP += 2
#define OUT(a,d)	cpu->ba = (a); cpu->bus->data = (d); if(dato(cpu, 0)) goto be
#define IN(d)	if(dati(cpu, 0)) goto be; d = cpu->bus->data
#define INA(a,d)	cpu->ba = a; if(dati(cpu, 0)) goto be; d = cpu->bus->data
#define TR(m)	trace("%06o "#m"\n", PC-2)
#define TRB(m)	trace("%06o "#m"%s\n", PC-2, by ? "B" : "")

	oldpsw = PSW;

	trace("fetch from %06o\n", PC);
	tracestate(cpu);
	cpu->ba = PC;
	PC += 2;	/* increment even on bus error */
	IN(cpu->ir);

	by = !!(cpu->ir&B15);
	br = sxt(cpu->ir)<<1;
	src = cpu->ir>>6 & 077;
	sf = src & 7;
	sm = src>>3 & 7;
	dst = cpu->ir & 077;
	df = dst & 7;
	dm = dst>>3 & 7;
	if(by)	mask = M8, sign = B7;
	else	mask = M16, sign = B15;

	inhov = 0;
	/* Binary */
	switch(cpu->ir & 0170000){
	case 0110000: case 0010000:	TRB(MOV);
		RD_B; CLV;
		b = SR; NZ;
		if(dm==0) cpu->r[df] = SR;
		else writedest(cpu, SR, by);
		SVC;
	case 0120000: case 0020000:	TRB(CMP);
		RD_B; CLCV;
		b = SR + W(~DR) + 1; NC; BXT;
		if(sgn((SR ^ DR) & ~(DR ^ b))) SEV;
		NZ; SVC;
	case 0130000: case 0030000:	TRB(BIT);
		RD_B; CLV;
		b = DR & SR;
		NZ; SVC;
	case 0140000: case 0040000:	TRB(BIC);
		RD_B; CLV;
		b = DR & ~SR;
		NZ; WR; SVC;
	case 0150000: case 0050000:	TRB(BIS);
		RD_B; CLV;
		b = DR | SR;
		NZ; WR; SVC;
	case 0060000:			TR(ADD);
		by = 0; RD_B; CLCV;
		b = SR + DR; C;
		if(sgn(~(SR ^ DR) & (DR ^ b))) SEV;
		NZ; WR; SVC;
	case 0160000:			TR(SUB);
		by = 0; RD_B; CLCV;
		b = DR + W(~SR) + 1; NC;
		if(sgn((SR ^ DR) & (DR ^ b))) SEV;
		NZ; WR; SVC;

	/* Reserved instructions */
	case 0170000: case 0070000: goto ri;
	}

	/* Unary */
	switch(cpu->ir & 0007700){
	case 0005000:	TRB(CLR);
		RD_U; CLCV;
		b = 0;
		NZ; WR; SVC;
	case 0005100:	TRB(COM);
		RD_U; CLV; SEC;
		b = W(~SR);
		NZ; WR; SVC;
	case 0005200:	TRB(INC);
		RD_U; CLV;
		b = W(SR+1); BXT;
		if(sgn(~SR&b)) SEV;
		NZ; WR; SVC;
	case 0005300:	TRB(DEC);
		RD_U; CLV;
		b = W(SR+~0); BXT;
		if(sgn(SR&~b)) SEV;
		NZ; WR; SVC;
	case 0005400:	TRB(NEG);
		RD_U; CLCV;
		b = W(~SR+1); BXT; if(b) SEC;
		if(sgn(b&SR)) SEV;
		NZ; WR; SVC;
	case 0005500:	TRB(ADC);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = SR + c; C; BXT;
		if(sgn(~SR&b)) SEV;
		NZ; WR; SVC;
	case 0005600:	TRB(SBC);
		RD_U; c = !ISSET(PSW_C)-1; CLCV;
		b = W(SR+c); if(c && SR == 0) SEC; BXT;
		if(sgn(SR&~b)) SEV;
		NZ; WR; SVC;
	case 0005700:	TRB(TST);
		RD_U; CLCV;
		b = SR;
		NZ; SVC;

	case 0006000:	TRB(ROR);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = (SR&mask) >> 1; if(c) b |= sign; if(SR & 1) SEC; BXT;
		if((PSW>>3^PSW)&1) SEV;
		NZ; WR; SVC;
	case 0006100:	TRB(ROL);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = (SR<<1) & mask; if(c) b |= 1; if(SR & B15) SEC; BXT;
		if((PSW>>3^PSW)&1) SEV;
		NZ; WR; SVC;
	case 0006200:	TRB(ASR);
		RD_U; c = ISSET(PSW_C); CLCV;
		b = W(SR>>1) | SR&B15; if(SR & 1) SEC; BXT;
		if((PSW>>3^PSW)&1) SEV;
		NZ; WR; SVC;
	case 0006300:	TRB(ASL);
		RD_U; CLCV;
		b = W(SR<<1); if(SR & B15) SEC; BXT;
		if((PSW>>3^PSW)&1) SEV;
		NZ; WR; SVC;

	case 0006400:
	case 0006500:
	case 0006600:
	case 0006700:
		goto ri;

	}

	switch(cpu->ir & 0107400){
	case 0004000:
	case 0004400:	TR(JSR);
		if(dm == 0) goto ill;
		if(addrop(cpu, dst, 0)) goto be;
		DR = cpu->b;
		PUSH; OUT(SP, cpu->r[sf]);
		cpu->r[sf] = PC; PC = DR;
		SVC;
	case 0104000:	TR(EMT); TRAP(030);
	case 0104400:	TR(TRAP); TRAP(034);
	}

	/* Branches */
	switch(cpu->ir & 0103400){
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
	}

	// Hope we caught all instructions we meant to
	assert((cpu->ir & 0177400) == 0);

	/* Misc */
	switch(cpu->ir & 0300){
	case 0100:	TR(JMP);
		if(dm == 0) goto ill;
		if(addrop(cpu, dst, 0)) goto be;
		PC = cpu->b;
		SVC;
	case 0200:
	switch(cpu->ir&070){
	case 000:	TR(RTS);
		BA = SP; POP;
		PC = cpu->r[df];
		IN(cpu->r[df]);
		SVC;
	case 010: case 020: case 030:
		goto ri;
	case 040: case 050:	TR(CCC); PSW &= ~(cpu->ir&017); SVC;
	case 060: case 070:	TR(SEC); PSW |= cpu->ir&017; SVC;
	}
	case 0300:	TR(SWAB);
		RD_U; CLC;
		b = WD(DR & 0377, (DR>>8) & 0377);
		NZ; WR; SVC;
	}

	/* Operate */
	switch(cpu->ir & 7){
	case 0:	TR(HALT); cpu->state = STATE_HALTED; return;
	case 1:	TR(WAIT); cpu->state = STATE_WAITING; SVC;
	case 2:	TR(RTI);
		BA = SP; POP; IN(PC);
		BA = SP; POP; IN(PSW);
		SVC;
	case 3:	TR(BPT); TRAP(014);
	case 4:	TR(IOT); TRAP(020);
	case 5:	TR(RESET); reset(cpu); SVC;
	}

	// All other instructions should be reserved now

ri:	TRAP(010);
ill:	TRAP(4);
be:	if(cpu->be > 1){
		printf("double bus error, HALT\n");
		cpu->state = STATE_HALTED;
		return;
	}
printf("bus error %o\n", cpu->bus->addr);
	TRAP(4);

trap:
	trace("TRAP %o\n", TV);
	PUSH; OUT(SP, PSW);
	PUSH; OUT(SP, PC);
	INA(TV, PC);
	INA(TV+2, PSW);
	/* no trace trap after a trap */
	oldpsw = PSW;

	tracestate(cpu);
	return;		// TODO: is this correct?
//	SVC;

service:
	c = PSW >> 5;
	if(oldpsw & PSW_T){
		oldpsw &= ~PSW_T;
		TRAP(014);
	}else if(cpu->traps & TRAP_STACK){
		cpu->traps &= ~TRAP_STACK;
		inhov = 1;
		TRAP(4);
	}else if(cpu->traps & TRAP_PWR){
		cpu->traps &= ~TRAP_PWR;
		TRAP(024);
	}else if(c < 7 && cpu->traps & TRAP_BR7){
		cpu->traps &= ~TRAP_BR7;
		TRAP(cpu->br[3].bg(cpu->br[3].dev));
	}else if(c < 6 && cpu->traps & TRAP_BR6){
		cpu->traps &= ~TRAP_BR6;
		TRAP(cpu->br[2].bg(cpu->br[2].dev));
	}else if(c < 5 && cpu->traps & TRAP_BR5){
		cpu->traps &= ~TRAP_BR5;
		TRAP(cpu->br[1].bg(cpu->br[1].dev));
	}else if(c < 4 && cpu->traps & TRAP_BR4){
		cpu->traps &= ~TRAP_BR4;
		TRAP(cpu->br[0].bg(cpu->br[0].dev));
	}else
	// TODO? console stop
		/* fetch next instruction */
		return;
}

void
run(KA11 *cpu)
{
	int count;

	count = 0;
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

		if (count++ == 1000) {
			sleep_ms(cpu->throttle);
			count = 0;
		}
	}

	printstate(cpu);
}
