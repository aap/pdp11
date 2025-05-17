#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t byte;

typedef struct Uword Uword;
struct Uword
{
	byte clk;	// 3b type of cycle
	byte cir;	// 1b clock IR
	byte wr;		// 2b write register
	byte cb;		// 1b clock B
	byte cd;		// 1b clock D
	byte cba;	// 1b clock BA
	byte bus;	// 3b
	byte dad;	// 4b
	byte sps;	// 3b
	byte alu;	// 5b ALU cntl
	byte sbc;	// 4b B constants
	byte sbm;	// 4b B MUX
	byte sdm;	// 2b D MUX
	byte sba;	// 1b BA MUX
	byte ubf;	// 5b branch
	byte srx;	// 4b register select
	byte rif;	// 4b R immediate
	byte upf;	// 8/9b next address

	char *name;
	int addr;
};

typedef struct KUword KUword;
struct KUword
{
	byte mm;	// 4b

	char *name;
	int addr;
};

typedef struct EUword EUword;
struct EUword
{
	byte con;	// 2b
	byte fc1;	// 1b
	byte fub;	// 1b
	byte mhr;	// 3b
	byte frd;	// 1b

	byte erd;	// 1b
	byte srd;	// 2b
	byte sdr;	// 2b
	byte cvm;	// 3b
	byte nzm;	// 2b
	byte ccc;	// 3b
	byte gpc;	// 3b
	byte cee;	// 1b
	byte cnt;	// 2b
	byte eub;	// 4b
	byte cbr;	// 1b

	char *name;
	int addr;
};

Uword urom[] = {
#include "ucode_40_base.inc"
};

KUword kurom[] = {
#include "ucode_40_kt.inc"
};

Uword urom_eae1[] = {
#include "ucode_40_eae1.inc"
};

EUword urom_eae2[] = {
#include "ucode_40_eae2.inc"
};

void
dumpuw(Uword u)
{
	uint64_t w;
	w = u.clk;
	w <<= 1; w |= u.cir;
	w <<= 2; w |= u.wr;
	w <<= 1; w |= u.cb;
	w <<= 1; w |= u.cd;
	w <<= 1; w |= u.cba;
	w <<= 3; w |= u.bus;
	w <<= 4; w |= u.dad;
	w <<= 3; w |= u.sps;
	w <<= 5; w |= u.alu;
	w <<= 4; w |= u.sbc;
	w <<= 4; w |= u.sbm;
	w <<= 2; w |= u.sdm;
	w <<= 1; w |= u.sba;
	w <<= 5; w |= u.ubf;
	w <<= 4; w |= u.srx;
	w <<= 4; w |= u.rif;
	w <<= 9; w |= u.upf;

	printf("%015lX\n", w);
}

void
dumpkuw(KUword u)
{
	uint64_t w;
	w = u.mm;

	printf("%lX\n", w);
}

void
dumpeuw(EUword u)
{
	uint64_t w;
	w = 0;
/*
	// FIS
	w <<= 2; w |= u.con;
	w <<= 1; w |= u.fc1;
	w <<= 1; w |= u.fub;
	w <<= 3; w |= u.mhr;
	w <<= 1; w |= u.frd;
*/

	w <<= 1; w |= u.erd;
	w <<= 2; w |= u.srd;
	w <<= 2; w |= u.sdr;
	w <<= 3; w |= u.cvm;
	w <<= 2; w |= u.nzm;
	w <<= 3; w |= u.ccc;
	w <<= 3; w |= u.gpc;
	w <<= 1; w |= u.cee;
	w <<= 2; w |= u.cnt;
	w <<= 4; w |= u.eub;
	w <<= 1; w |= u.cbr;

	printf("%06lX\n", w);
}

void
dumpfuw(EUword u)
{
	uint64_t w;
	w = 0;
	w <<= 2; w |= u.con;
	w <<= 1; w |= u.fc1;
	w <<= 1; w |= u.fub;
	w <<= 3; w |= u.mhr;
	w <<= 1; w |= u.frd;

	printf("%02lX\n", w);
}

int
main()
{
	int i;
	FILE *f;

	stdout = fopen("ucode_40_base.rom", "w");
	assert(stdout);
	for(i = 0; i < 256; i++)
		dumpuw(urom[i]);
	fclose(stdout);

	stdout = fopen("ucode_40_ke.rom", "w");
	assert(stdout);
	for(i = 0; i < 256; i++)
		dumpuw(urom_eae1[i]);
	fclose(stdout);

	stdout = fopen("ucode_40_eis.rom", "w");
	assert(stdout);
	for(i = 0; i < 256; i++)
		dumpeuw(urom_eae2[i]);
	fclose(stdout);

	stdout = fopen("ucode_40_fis.rom", "w");
	assert(stdout);
	for(i = 0; i < 256; i++)
		dumpfuw(urom_eae2[i]);
	fclose(stdout);

	stdout = fopen("ucode_40_kt.rom", "w");
	assert(stdout);
	for(i = 0; i < 256; i++)
		dumpkuw(kurom[i]);
	fclose(stdout);

	return 0;
}
