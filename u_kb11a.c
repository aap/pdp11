#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t lword;

enum
{
	M8 = 0377,
	M16 = 0177777
};

enum
{
	PS_CM  = 0140000,
	PS_CM1 = 0100000,	/* user */
	PS_CM0 = 0040000,	/* not kernel */
	PS_PM  = 0030000,
	PS_PM1 = 0020000,
	PS_PM0 = 0010000,
	PS_GPR = 0004000,
	PS_PL  = 0000340,
	PS_T   = 0000020,
	PS_N   = 0000010,
	PS_Z   = 0000004,
	PS_V   = 0000002,
	PS_C   = 0000001
};

enum
{
	SUBROM_ENV = 1,
	SUBROM_VMOD0 = 2,
	SUBROM_VMOD1 = 4,
	SUBROM_ENC = 010,
	SUBROM_CMOD0 = 020,
	SUBROM_CMOD1 = 040,
	SUBROM_ENZN = 0100,
	SUBROM_MODZN = 0200,

	SUBROM_R_PART_PCLASS = 0400,
	SUBROM_R_ICLASS = 01000,
	SUBROM_R_KCLASS = 02000,
	SUBROM_R_MUL_ASHC_MFP = 04000,
	SUBROM_R_MFP_MTP = 010000,
	SUBROM_R_ROR_B_MUL = 020000,
	SUBROM_DIV_ASHC = 040000,
	SUBROM_SXT = 0100000
};

word subrom[32] = {
	0020707,	// ROR.B
	0000705,	// ROL.B
	0000707,	// ASR.B
	0000705,	// ASL.B
	0000000,	// MARK
	0015351,	// MFP
	0010351,	// MTP
	0100751,	// SXT
	0000741,	// CLR.B
	0000721,	// COM.B
	0000750,	// INC.B
	0000752,	// DEC.B
	0000724,	// NEG.B
	0000740,	// ADC.B
	0000722,	// SBC.B
	0003341,	// TST.B
	0000726,	// SUB
	0000351,	// MOV.B
	0003146,	// CMP.B
	0003351,	// BIT.B
	0000751,	// BIC.B
	0000751,	// BIS.B
	0000744,	// ADD
	0000000,	// NOT USED
	0025231,	// MUL
	0045061,	// DIV
	0004341,	// ASH
	0044311,	// ASHC
	0000751,	// XOR
	0000000,	// NOT USED
	0000000,	// NOT USED
	0000000,	// SOB
};

byte alucntl[32] = {
	0060,	// ROR.B
	0354,	// ROL.B
	0060,	// ASR.B
	0054,	// ASL.B
	0000,	// MARK
	0252,	// MFP
	0252,	// MTP
	0243,	// SXT
	0243,	// CLR.B
	0240,	// COM.B
	0140,	// INC.B
	0057,	// DEC.B
	0240,	// NEG.B
	0340,	// ADC.B
	0357,	// SBC.B
	0040,	// TST.B
	0146,	// SUB
	0252,	// MOV.B
	0046,	// CMP.B
	0253,	// BIT.B
	0247,	// BIC.B
	0256,	// BIS.B
	0051,	// ADD
	0000,	// -
	0166,	// MUL
	0154,	// DIV
	0020,	// ASH
	0154,	// ASHC
	0246,	// XOR
	0000,	// -
	0000,	// -
	0000,	// SOB
};

typedef struct Uword Uword;
struct Uword
{
	int brk;
	int brx;
	int srx, drx;
	int srk, drk;
	int ccl;
	int pca, pcb;
	int shf;
	int irk;
	int pwe;
	int pad;
	int bsd;
	int bax;
	int ibs;
	int shc;
	int bct;
	int msc;
	int bsc;
	int amx, bmx;
	int kmx;
	int alu;
	// not buffered
	int fen;
	int bef;
	int adr;
};

enum
{
	INTR_PAUSE = 1,
	BUS_PAUSE,
	BUS_LONG_PAUSE
};

Uword urom[256] = {
#include "ucode.inc"
};

// TODO: is this even necessary?
typedef struct KB11Aregs KB11Aregs;
struct KB11Aregs
{
	word br;
	word pca, pcb;
	word sr;
	word dr;
	word sc;	// 6 bits
	word ir;
	word lr;
	word bra;
	byte sl;
	byte pb;
	word d;

	byte rar;
	Uword rbr;

	word ps;
};

typedef struct KB11A KB11A;
struct KB11A
{
	KB11Aregs c;
	KB11Aregs n;
	word gs[16];
	word gd[16];

	word radr;
	Uword rom;
	byte alucntl;

	/* data path */
	byte destcon;
	byte srccon;
	byte tv;
	word k0mx;
	word k1mx;
	word amx;
	word bmx;
	word bamx;
	word shfr;
	word brmx;
	int alu;

	/* IR decode */
	int sf;
	int df;
	int dm0_mfp_mtp;

	/* ROM decode */
	int dm0_mfp_mtp_cond;

	/* various flip flops that probably don't
	   have to be in the Regs struct */
	int grab_obd;

	// TODO
	int conf;
	int brq_true;
	int cnsl_act;
	int sel_int;
	int sel_mem;

	// TMP, KT11 placeholders
	int ps_restore;

	// other placeholders
	word unibusd;
	word intbus;

	// console or hardcoded
	int sv;
	lword sw;
};

void
dumpromw(Uword w)
{
	printf("%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o"
		"\t%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o"
		"\t%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o\t%o\n",
		w.brk,
		w.brx,
		w.srx,
		w.drx,
		w.srk,
		w.drk,
		w.ccl,
		w.pca,
		w.pcb,
		w.shf,
		w.irk,
		w.pwe,
		w.pad,
		w.bsd,
		w.bax,
		w.ibs,
		w.shc,
		w.bct,
		w.msc,
		w.bsc,
		w.amx,
		w.bmx,
		w.kmx,
		w.alu,
		w.fen,
		w.bef,
		w.adr);
}

void
dumpstate(KB11A *kb)
{
	printf("A/%06o B/%06o BA/%06o\n",
		kb->amx, kb->bmx, kb->bamx);
	printf("ALU/%07o SHFR/%06o\n",
		kb->alu, kb->shfr);
	printf("PCA/%06o PCB/%06o\n",
		kb->c.pca, kb->c.pcb);
	printf("SR/%06o DR/%06o\n",
		kb->c.sr, kb->c.dr);
	printf("BR/%06o IR/%06o\n",
		kb->c.br, kb->c.ir);
}

word sxt(byte b) { return b & 0200 ? 0177400|b : b; }

// TODO: move things outta here
void
ir_decodeXXXX(KB11A *kb)
{
	printf("IR/%06o\n", kb->c.ir);

	// IRCH
	int irch_bin = (kb->c.ir&0070000) != 0070000 &&
		(kb->c.ir&0070000) != 0000000;
	int irch_sub = (kb->c.ir&0170000) == 0160000;
	int irch_subroma;
	switch(kb->c.ir & 0070000){
	case 0000000: irch_subroma = kb->c.ir>>6 & 017; break;
	case 0010000: irch_subroma = 021; break;
	case 0020000: irch_subroma = 022; break;
	case 0030000: irch_subroma = 023; break;
	case 0040000: irch_subroma = 024; break;
	case 0050000: irch_subroma = 025; break;
	case 0060000: irch_subroma = irch_sub ? 020 : 026; break;
	case 0070000: irch_subroma = 030 | kb->c.ir>>9 & 7; break;
	}
	word irch_subword = subrom[irch_subroma];
	kb->alucntl = alucntl[irch_subroma];
	int ircc_rom_dec_ena = (kb->c.ir&0170000) == 0070000 ||
		(kb->c.ir&0077000) == 0005000 ||
		(kb->c.ir&0077000) == 0006000 ||
		irch_bin;

	// IRCB
	int ircb_swab = (kb->c.ir&0177700) == 0000300;
	int ircb_movb = (kb->c.ir&0170000) == 0110000;
	int ircb_fclass = (kb->c.ir&0170000) == 0170000;
	int ircb_jmp_jsr = (kb->c.ir&0177700) == 0000100 ||
		(kb->c.ir&0177000) == 0004000;
	int ircb_fjclass = ircb_fclass || ircb_jmp_jsr;
	int ircb_neg_b = (kb->c.ir&0077700) == 0005400;
	int ircb_obd_asrb_rorb = kb->grab_obd &&
		((kb->c.ir&0177700) == 0106000 ||
		 (kb->c.ir&0177700) == 0106200);
	int ircb_mul_ashc_mfp = ircc_rom_dec_ena &&
		irch_subword&SUBROM_R_MUL_ASHC_MFP;
	int ircb_part_pclass = ircc_rom_dec_ena &&
		irch_subword&SUBROM_R_PART_PCLASS;
	int ircb_pclass = ircb_part_pclass || ircb_swab || ircb_movb;
	int ircb_iclass = ircc_rom_dec_ena &&
		irch_subword&SUBROM_R_ICLASS;
	int ircb_kclass = ircc_rom_dec_ena &&
		irch_subword&SUBROM_R_KCLASS;

	// IRCC
	int ircc_mfp_mtp = ircc_rom_dec_ena && irch_subword&SUBROM_R_MFP_MTP;
	int ircc_dm0_mfp_mtp = ircc_mfp_mtp && (kb->c.ir&0000077) == 0000000;
	int ircc_oclass = (kb->c.ir&0170000) == 0010000 ||
		(kb->c.ir&0077700) == 0006600;

	// IRCD
	int nobyte = (kb->c.ir&0070000) == 0060000 ||
		(kb->c.ir&0177400) == 0106400;
	int ircd_wdin = (kb->c.ir&0100000) == 0 || nobyte;
	int ircd_byin = (kb->c.ir&0100000) != 0 && !nobyte && !ircb_fclass;
	int ircd_sbyn = ircd_byin &&
		(kb->c.ir&0077000) != 0005000 &&
		(kb->c.ir&0077000) != 0006000;
	int ircd_rtt = kb->c.ir == 0000006;
	// TODO: these are inhibited by TMCB TRAP INH
	int ircd_iot = kb->c.ir == 0000004;
	int ircd_opcode3 = kb->c.ir == 0000003;
	int ircd_trap = (kb->c.ir&01777400) == 0104400;
	int ircd_emt = (kb->c.ir&01777400) == 0104000;

	printf("%o %o\n", irch_subroma, irch_subword);

}

int
get_gsa(KB11A *kb)
{
	int set1 = !kb->cnsl_act && kb->c.ps&PS_GPR && kb->sf<6 ||
		kb->c.ps&PS_CM0 && kb->sf==6;
	int gsa;
	switch(kb->c.rbr.pad){
	case 0:
	case 1: gsa = kb->sf | (kb->c.ps&PS_CM1 && kb->sf==6); break;
	case 4: gsa = kb->sf | 1; break;
	case 2:
	case 3: gsa = 5; break;
	case 6: gsa = 0; break;
	case 5: gsa = kb->df | (!kb->cnsl_act && kb->df==6 &&
		kb->dm0_mfp_mtp_cond ? kb->c.ps&PS_PM1 : kb->c.ps&PS_CM1);
		break;
	case 7: gsa = kb->c.ps&PS_CM1 ? 7 : 6; break;
	}
	return gsa|set1<<3;
}

int
get_gda(KB11A *kb)
{
	int set1 = !kb->cnsl_act && kb->c.ps&PS_GPR && kb->df<6 ||
		kb->c.ps&PS_CM0 && kb->df==6 && !kb->dm0_mfp_mtp_cond;
	int gda;
	switch(kb->c.rbr.pad){
	case 0: gda = kb->sf | (kb->c.ps&PS_CM1 && kb->sf==6); break;
	case 4: gda = kb->sf | 1; break;
	case 1:
	case 5: gda = kb->df | (!kb->cnsl_act && kb->df==6 &&
		kb->dm0_mfp_mtp_cond ? kb->c.ps&PS_PM1 : kb->c.ps&PS_CM1);
		break;
	case 2: gda = 5; break;
	case 6: gda = 0; break;
	case 3:
	case 7: gda = kb->c.ps&PS_CM1 ? 7 : 6; break;
	}
	return gda|set1<<3;
}

void
load_sr(KB11A *kb)
{
	if(kb->c.rbr.srk){
		// TODO: don't set here
		kb->sf = kb->c.ir>>6 & 7;
		int srsel = kb->c.rbr.srx == 1 || kb->c.rbr.srx&2 && kb->sf!=7;
		kb->n.sr = srsel ? kb->gs[get_gsa(kb)] : kb->shfr;
	}
}

void
load_dr(KB11A *kb)
{
	switch(kb->c.rbr.drk){
	case 0:	break;
	case 1: kb->n.dr = kb->c.dr>>1 | kb->alu<<15; break;
	case 2: kb->n.dr = kb->c.dr<<1; break;	// TODO
	case 3:
		if(kb->c.rbr.drx == 3)
			// TODO: this his held low until 5 even, do we need that?
			kb->n.dr = 0;
		else{
			// TODO: don't set here, wrong too
			kb->df = kb->c.ir & 7;
			int drsel = kb->c.rbr.drx == 1 || kb->c.rbr.drx&2 && kb->df!=7;
			kb->n.dr = drsel ? kb->gd[get_gda(kb)] : kb->shfr;
		}
		break;
	}
}

void
update_state(KB11A *kb)
{
	// TODO: tv

	switch(kb->c.rbr.kmx){
	case 0:
		kb->k0mx = 1;
		kb->k1mx = sxt(kb->sv);
		break;
	case 1:
		kb->k0mx = 2;	// TODO: correct?
		kb->k1mx = kb->tv;
		break;
	case 2:
		kb->k0mx = kb->srccon;
		if(kb->c.sr & 0200) kb->k0mx |= 0177400;
		kb->k1mx = kb->c.br & 0377;
		break;
	case 3:
		kb->k0mx = kb->destcon;
		if(kb->c.br & 0200) kb->k0mx |= 0177400;
		kb->k1mx = sxt(kb->c.br) & 0173377;
		break;
	default: assert(0);
	}
	kb->k1mx &= ~1;

	switch(kb->c.rbr.amx){
	case 0: kb->amx = kb->c.dr; break;
	case 1: kb->amx = kb->c.pcb; break;
	case 2: kb->amx = kb->c.sr; break;
	case 3: kb->amx = kb->c.br; break;
	default: assert(0);
	}

	switch(kb->c.rbr.bmx){
	case 0: kb->bmx = kb->k0mx; break;
	case 1: kb->bmx = kb->k1mx; break;
	case 2: kb->bmx = kb->c.sr; break;
	case 3: kb->bmx = kb->c.br; break;
	default: assert(0);
	}

	switch(kb->c.rbr.bax){
	case 0: kb->bamx = kb->c.dr; break;
	case 1: kb->bamx = kb->c.pcb; break;
	case 2: kb->bamx = kb->c.sr; break;
	case 3: kb->bamx = 0; break;	// TODO: EALU
	default: assert(0);
	}

	int shf = kb->c.rbr.shf;
	switch(kb->c.rbr.alu){
	case 0: kb->alu = ~kb->amx&M16; break;
	case 1: kb->alu = kb->bmx; break;
	case 2: kb->alu = kb->amx; break;
	case 3: kb->alu = kb->amx + kb->bmx; break;
	case 4: kb->alu = kb->amx + (kb->amx&(~kb->bmx&M16)); break;
	case 5: kb->alu = kb->amx + kb->amx; break;
	case 6: kb->alu = kb->amx + (~kb->bmx&M16) + 1; break;
	case 7:
		// change shf
		assert(0 && "yikes, not implemented yet");
	default: assert(0);
	}

	switch(shf){
	case 0: kb->shfr = kb->alu<<8 | kb->alu>>8; break;
	case 1: kb->shfr = kb->c.pcb; break;
	case 2: kb->shfr = kb->alu; break;
	// TODO: this is wrong
	case 3: kb->shfr = kb->alu>>1;
assert(0); break;
	default: assert(0);
	}

	switch(kb->c.rbr.ibs){
	case 0: kb->intbus = 0; break;
	case 1: kb->intbus = kb->sw; break;
	case 2: kb->intbus = 0; break;
	case 3: kb->intbus = kb->c.ps; break;
	default: assert(0);
	}

	int wantunibus = kb->c.rbr.bsd==INTR_PAUSE;	// TODO: DDC STOP?
	int wantintbus = kb->c.rbr.ibs&1;	// TODO: SW, KT11, FP
	kb->sel_int = !wantunibus && wantintbus;
	// we don't have fast mem
	kb->sel_mem = !wantunibus && !wantintbus && 0;

	switch(kb->c.rbr.brx<<2 | kb->sel_int<<1 | kb->sel_mem){
	case 0: case 1: case 2: case 3: case 7:
		kb->brmx = kb->shfr;
		break;
	case 4: kb->brmx = kb->unibusd; break;
	case 5: kb->brmx = 0; break;	// membus, we don't have it
	case 6: kb->brmx = kb->intbus; break;
	default: assert(0);
	}
}

void
update_rom(KB11A *kb)
{
	kb->rom = urom[kb->c.rar];
dumpromw(kb->rom);
	// TODO: update state
}

byte
get_afork(KB11A *kb)
{
	// TODO
	return 0;
}

byte
get_bfork(KB11A *kb)
{
	// TODO
	return 0;
}

byte
get_cfork(KB11A *kb)
{
	// TODO
	return 0;
}

byte
get_brcab(KB11A *kb)
{
	// TODO
	switch(kb->rom.bef){
	case 0: return 0;
	case 1: assert(0); return 0;	// TODO
	case 2: assert(0); return 0;	// TODO
	case 3: assert(0); return 0;	// TODO
	case 4: assert(0); return 0;	// TODO
	case 5: assert(0); return 0;	// TODO
	case 6: return  ~kb->c.br>>9 & 040 | kb->ps_restore<<4;
	case 7: assert(0); return 0;	// TODO
	case 010: assert(0); return 0;	// TODO
	case 011: assert(0); return 0;	// TODO
	case 012: return kb->conf<<5 | !kb->brq_true<<4;
	case 013: assert(0); return 0;	// TODO
	case 014: return 0;	// TODO
	case 015: assert(0); return 0;	// TODO
	case 016: assert(0); return 0;	// TODO
	case 017: assert(0); return 0;	// TODO
	default: assert(0);
	}
	return 0;
}

void
update_radr(KB11A *kb)
{
	byte af = kb->c.rbr.fen & 1 ? get_afork(kb) : M8;
	byte bf = kb->c.rbr.fen & 2 ? get_bfork(kb) : M8;
	byte cf = kb->c.rbr.fen & 4 ? get_cfork(kb) : M8;
	kb->radr = (kb->rom.adr | get_brcab(kb)) & cf & bf & af;
	printf("getting new address %o\n", kb->radr);
}

void
t1(KB11A *kb)
{
	printf("T1\n");

	kb->n.rbr.bsd = kb->rom.bsd;
	kb->n.rbr.bax = kb->rom.bax;
	kb->n.rbr.ibs = kb->rom.ibs;
	kb->n.rbr.shc = kb->rom.shc;
	kb->n.rbr.bct = kb->rom.bct;
	kb->n.rbr.msc = kb->rom.msc;
	kb->n.rbr.bsc = kb->rom.bsc;
	kb->n.rbr.amx = kb->rom.amx;
	kb->n.rbr.bmx = kb->rom.bmx;
	kb->n.rbr.kmx = kb->rom.kmx;
	kb->n.rbr.alu = kb->rom.alu;

	load_sr(kb);
	load_dr(kb);
	if(kb->c.rbr.drk == 3)
		/* not quite correct, but should be ok */
		kb->grab_obd = kb->n.dr & 1;
	if(kb->c.rbr.brk)
printf("load BR %o %o\n", kb->c.rbr.brx<<2 | kb->sel_int<<1 | kb->sel_mem, kb->brmx);
		kb->n.br = kb->brmx;

	/* falling edge */
	kb->n.rbr.pwe = kb->rom.pwe;
	kb->n.rbr.pad = kb->rom.pad;

	kb->c = kb->n;
	update_state(kb);
}

void
t2(KB11A *kb)
{
	printf("T2\n");

	kb->n.rbr.brk = kb->rom.brk;
	kb->n.rbr.brx = kb->rom.brx;
	kb->n.rbr.srx = kb->rom.srx;
	kb->n.rbr.drx = kb->rom.drx;
	kb->n.rbr.srk = kb->rom.srk;
	kb->n.rbr.drk = kb->rom.drk;
	kb->n.rbr.ccl = kb->rom.ccl;
	kb->n.rbr.pca = kb->rom.pca;
	kb->n.rbr.pcb = kb->rom.pcb;
	kb->n.rbr.shf = kb->rom.shf;
	kb->n.rbr.irk = kb->rom.irk;

	kb->c = kb->n;
	update_state(kb);
}

void
t3(KB11A *kb)
{
	printf("T3\n");

	update_radr(kb);
	kb->n.rar = kb->radr;
	if(kb->c.rbr.bct == 7){	// TODO: or
		// TODO: BEND, see UBCB CLR UNI
		printf("	BEND (TODO)\n");
	}
	if(kb->c.rbr.msc == 6){
		// TODO: BRQ strobe, TMCE
		printf("	BRQ STROBE (TODO)\n");
		// CLK CONF
		// if(ubcf_s_inst) tmca_conf = 1;
		// BRQ CLK
	}

	kb->c = kb->n;
	update_state(kb);
	update_rom(kb);
}

void
t4(KB11A *kb)
{
	printf("T4\n");
	kb->c = kb->n;
	update_state(kb);
}

void
t5(KB11A *kb)
{
	printf("T5\n");
	kb->c = kb->n;
	update_state(kb);
}

void
zap(KB11A *kb)
{
	kb->n.rar = 0200;
}

void
power(KB11A *kb)
{
	// TODO: don't do this
	memset(kb, 0, sizeof(KB11A));

	kb->ps_restore = 0;
	// eh?
	kb->conf = 1;

	memset(&kb->n.rbr, 0, sizeof(Uword));
	zap(kb);

	kb->c = kb->n;
	update_rom(kb);
}

void
test(KB11A *kb)
{
	kb->n.pcb = 01234;
	kb->n.dr = 04321;
	kb->sw = 0112233;
	kb->unibusd = 0123321;

	int i;
	for(i = 0; i < 3; i++){
		t1(kb);
		dumpstate(kb);
		t2(kb);
		dumpstate(kb);
		if(kb->c.rbr.bsd == INTR_PAUSE)
			printf("	INTR PAUSE\n");
		t3(kb);
		dumpstate(kb);
		t4(kb);
		dumpstate(kb);
		t5(kb);
		dumpstate(kb);
		printf("----------\n");
	}
	t1(kb);
	dumpstate(kb);

/*
	kb->c.ir = 0012701;
	kb->c.ir = 0066700;
	ir_decode(kb);
	update_state(kb);
*/
}

int
main()
{
	KB11A *kb;
	kb = malloc(sizeof(KB11A));

	power(kb);
	test(kb);

	return 0;
}
