#include "11.h"
#include "kd11a.h"
#include "kw11.h"
#include "kl11.h"
#include "rk11.h"
#include "dc11_fake.h"
#include "args.h"

#include <signal.h>

// in words
//#define MEMSIZE (12*1024)
#define MEMSIZE (124*1024)	// max size last 4k is IO

uint16 memory[MEMSIZE];

void
busadddev(Bus *bus, Busdev *dev)
{
	Busdev **bp;
	for(bp = &bus->devs; *bp; bp = &(*bp)->next)
		;
	*bp = dev;
	dev->next = nil;
}

int
dati_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dati(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
datip_bus(Bus *bus)
{
	return dati_bus(bus);
}

int
dato_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->dato(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int
datob_bus(Bus *bus)
{
	Busdev *bd;
	for(bd = bus->devs; bd; bd = bd->next)
		if(bd->datob(bus, bd->dev) == 0)
			return 0;
	return 1;
}

int dati_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int dato_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
int datob_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 1; }
void reset_null(void *dev) { (void)dev; }
int svc_null(Bus *bus, void *dev) { (void)bus; (void)dev; return 0; }

word
sgn(word w)
{
	return (w>>15)&1;
}

word
sxt(byte b)
{
	return (word)(int8_t)b;
}

void
loadmem(char *filename)
{
	FILE *f;
	char line[100], *s;
	word w;

	w = 0;
	f = fopen(filename, "r");
	if(f == nil)
		return;
	while(s = fgets(line, 100, f), s != nil){
		while(isspace(*s)) s++;
		if(*s == ':')
			w = strtol(s+1, nil, 8)>>1;
		if(*s >= '0' && *s <= '7')
			memory[w++] = strtol(s, nil, 8);
	}
	fclose(f);
}

int
loadpt(char *filename)
{
	FILE *f;
	byte hi, lo, s;
	word n, a, w;

	f = fopen(filename, "rb");
	if(f == nil){
		printf("can't open %s\n", filename);
		return 1;
	}

	for(;;){
		s = 0;
		if(fread(&lo, 1, 1, f) < 1) break;
		if(lo != 1)
			continue;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) break;
		s += hi;
		w = WD(hi, lo);
		if(w != 1)
			continue;
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		n = WD(hi, lo);
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(fread(&hi, 1, 1, f) < 1) goto botch;
		s += hi;
		a = WD(hi, lo);
		if(a == 1)
			break;
		if(n == 0)
			continue;
		n -= 6;
		while(n--){
			if(fread(&lo, 1, 1, f) < 1) goto botch;
			s += lo;
			if(a & 1)
				memory[a>>1] = memory[a>>1]&0377 | lo<<8;
			else
				memory[a>>1] = memory[a>>1]&0177400 | lo;
			a++;
		}
		if(fread(&lo, 1, 1, f) < 1) goto botch;
		s += lo;
		if(s)
			goto botch;
	}
	fclose(f);
	return 0;
botch:
	printf("paper tape botch\n");
	fclose(f);
	return 1;
}

void
dumpmem(int start, int end)
{
	start >>= 1;
	end >>= 1;
	for(; start != end; start++)
		printf("%06o: %06o\n", start<<1, memory[start]);
}

// BM792-YB bootrom
// must set switch to device's WC address
word rom[32] = {
	0013701,	// mov @#177570,r1	; read switches
	0177570,
	0000005,	// begin: reset
	0010100,	// mov r1,r2
	0012710,	// mov #-256,@r0	; set WC
	0177400,
	0020027,	// cmp r0,#177344	; dectape?
	0177344,
	0001007,	// bne start	; no
	0012740,	// mov #4002,-(r0)	; yes, rewind tape
	0004002,
	0005710,	// tst @r0	; wait for error
	0100376,	// bpl .-2
	0005740,	// tst -(r0)	; endzone?
	0100363,	// bpl begin	; try again
	0022020,	// cmp (r0)+,(r0)+
	0012740,	// start: mov #5,-(r0)	; start
	0000005,
	0105710,	// tstb @r0	; wait for done
	0100376,	// bpl .-2
	0005710,	// tst @r0	; error?
	0100754,	// bmi begin	; try again
	0105010,	// clrb @r0	; stop dectape
	0000137,	// jmp @#0
	0000000
};

KD11A cpu;
Bus bus;
KW11 kw11;
KL11 kl11;
RK11 rk11;
DC11 dc11;
Memory memdev = { memory, 0, MEMSIZE };
Memory romdev = { rom, 0773100>>1, 0773200>>1 };
Busdev membusdev = { nil, &memdev, dati_mem, dato_mem, datob_mem, svc_null, nil, reset_null };
Busdev rombusdev = { nil, &romdev, dati_rom, dato_rom, datob_rom, svc_null, nil, reset_null };
Busdev klbusdev = { nil, &kl11, dati_kl11, dato_kl11, datob_kl11, svc_kl11, bg_kl11, reset_kl11 };
Busdev kwbusdev = { nil, &kw11, dati_kw11, dato_kw11, datob_kw11, svc_kw11, bg_kw11, reset_kw11 };
Busdev rkbusdev = { nil, &rk11, dati_rk11, dato_rk11, datob_rk11, svc_rk11, bg_rk11, reset_rk11 };
Busdev dcbusdev = { nil, &dc11, dati_dc11, dato_dc11, datob_dc11, svc_dc11, bg_dc11, reset_dc11 };

char *argv0;

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", argv0);
	exit(0);
}

#ifdef AUTODIAG
int diagpassed;

int
rundiag(KD11A *cpu, char *ptfile)
{
	word sw;
	memset(memory, 0, MEMSIZE*2);
	if(loadpt(ptfile)){
		fprintf(stderr, "couldn't open file '%s'\n", ptfile);
		return 1;
	}
	sw = cpu->sw;
	reset(cpu);
	memset(cpu, 0, sizeof(KD11A));
	cpu->bus = &bus;
	cpu->sw = sw;
	diagpassed = 0;
	cpu->r[7] = 0200;	
	run(cpu);
	if(diagpassed){
		printf("passed %s\n", ptfile);
		return 0;
	}
	return 1;
}
#endif


int
main(int argc, char *argv[])
{
	memset(&cpu, 0, sizeof(KD11A));
	memset(&bus, 0, sizeof(Bus));
	cpu.bus = &bus;
	busadddev(&bus, &membusdev);
	busadddev(&bus, &kwbusdev);
	busadddev(&bus, &klbusdev);
	busadddev(&bus, &rkbusdev);
	busadddev(&bus, &dcbusdev);
	busadddev(&bus, &rombusdev);

	ARGBEGIN{
	}ARGEND;

	if(argc < 0)
		usage();

	loadmem("mem.txt");

	reset(&cpu);

	/* open a tty if it exists */
	kl11.ttyfd = open("/tmp/tty", O_RDWR);
	printf("tty connected to %d\n", kl11.ttyfd);

#ifdef AUTODIAG
	printf("running diags\n");
	/* basic tests */
	if(rundiag(&cpu, "maindec/ZKAAA0.BIN")) return 0;	// branch
	if(rundiag(&cpu, "maindec/ZKABA0.BIN")) return 0;	// con branch
	if(rundiag(&cpu, "maindec/ZKACA0.BIN")) return 0;	// unary
	if(rundiag(&cpu, "maindec/ZKADA0.BIN")) return 0;	// binary
	if(rundiag(&cpu, "maindec/ZKAEA0.BIN")) return 0;	// rot/shf
	if(rundiag(&cpu, "maindec/ZKAFA0.BIN")) return 0;	// cmp
	if(rundiag(&cpu, "maindec/ZKAGA0.BIN")) return 0;	// cmp not
	if(rundiag(&cpu, "maindec/ZKAHA0.BIN")) return 0;	// move
	if(rundiag(&cpu, "maindec/ZKAIA0.BIN")) return 0;	// bis bic bit
	if(rundiag(&cpu, "maindec/ZKAJA0.BIN")) return 0;	// add
	if(rundiag(&cpu, "maindec/ZKAKA0.BIN")) return 0;	// sub
	if(rundiag(&cpu, "maindec/ZKALA0.BIN")) return 0;	// jmp
	if(rundiag(&cpu, "maindec/ZKAMA0.BIN")) return 0;	// rts rti jsr

	/* 11/40 traps */
	if(rundiag(&cpu, "maindec/BKDMD0.BIC")) return 0;

	/* 11/40 instructions */
	if(rundiag(&cpu, "maindec/CKBAB0.BIC")) return 0;	// sxt
	if(rundiag(&cpu, "maindec/CKBBB0.BIC")) return 0;	// sob
	if(rundiag(&cpu, "maindec/CKBCC0.BIC")) return 0;	// xor
	if(rundiag(&cpu, "maindec/CKBDC0.BIC")) return 0;	// mark
	if(rundiag(&cpu, "maindec/CKBEC0.BIC")) return 0;	// rtt
	if(rundiag(&cpu, "maindec/CKBFD0.BIC")) return 0;	// stack limit

	/* EIS */
	if(rundiag(&cpu, "maindec/CKBIB0.BIC")) return 0;	// ash
	if(rundiag(&cpu, "maindec/CKBJA0.BIC")) return 0;	// ashc
	if(rundiag(&cpu, "maindec/CKBKA0.BIC")) return 0;	// mul
	if(rundiag(&cpu, "maindec/CKBLA0.BIC")) return 0;	// div

	/* KT11 */
	if(rundiag(&cpu, "maindec/BKTAD1.BIC")) return 0;	// basic logic
	if(rundiag(&cpu, "maindec/BKTBB0.BIC")) return 0;	// access keys
	if(rundiag(&cpu, "maindec/BKTCB0.BIC")) return 0;	// moves
	if(rundiag(&cpu, "maindec/BKTDC0.BIC")) return 0;	// states
	if(rundiag(&cpu, "maindec/BKTFD0.BIC")) return 0;	// abort

	/* system exerciser with KT11 */
/*	// no idea how this works
	cpu.sw = 0100001;
	cpu.sw |= 0370;	// disable a couple of devices
	if(rundiag(&cpu, "maindec/BKTGD1.BIC")) return 0;	// exerciser
*/
	printf("passed all diagnostics\n");
		return 0;
#endif

	// to boot UNIX v6
	attach_rk05(&rk11, 0, "unix6/disk0.rk");
	attach_rk05(&rk11, 1, "unix6/disk1.rk");
	attach_rk05(&rk11, 2, "unix6/disk2.rk");

	cpu.r[7] = 0173100;
	cpu.sw = 0177406;	// RK11 boot
//	cpu.sw = 0000001;
//	cpu.sw = 0173030;
//	cpu.sw = 0104000;
	run(&cpu);

	return 0;
}
