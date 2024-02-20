#include "11.h"
#include "ten11.h"
#include "util.h"

/* define this for a XGP-11 with 11/20. Make sure to link correctly! */
#ifdef AUTODIAG
// just so linking works, we don't actually do AUTODIAG in the xgp11
int diagpassed;
#endif

#ifdef KD11Bp
#include "kd11b.h"
#else
#include "ka11.h"
#include "kw11.h"
#include "kl11.h"
#endif
#include "xgp.h"
#include <stdarg.h>

// in words
#define MEMSIZE (28*1024)

uint16 memory[MEMSIZE];
void (*debug) (char *, ...);
FILE *logfile;

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
	return (word)(int8)b;
}

void
quiet (char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	va_end(ap);
}

void
log (char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(logfile, format, ap);
	fflush(logfile);
	va_end(ap);
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
		/* odd number of bytes is probably allowed but we can't do it */
		if(n&1)
			goto botch;
		n >>= 1;
		a >>= 1;
		while(n--){
			if(fread(&lo, 1, 1, f) < 1) goto botch;
			s += lo;
			if(fread(&hi, 1, 1, f) < 1) goto botch;
			s += hi;
			w = WD(hi, lo);
			memory[a++] = w;
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
dumpmem(word start, word end)
{
	start >>= 1;
	end >>= 1;
	for(; start != end; start++)
		printf("%06o: %06o\n", start<<1, memory[start]);
}

Bus bus;

#ifdef KD11Bp
KD11B cpu;
#define TTYDEV cpu
#else
KA11 cpu;
KW11 kw11;
KL11 kl11;
#define TTYDEV kl11
#endif

Memory memdev = { memory, 0, MEMSIZE };
Busdev membusdev = { nil, &memdev, dati_mem, dato_mem, datob_mem, svc_null, nil, reset_null };
KE11 ke11;
Busdev kebusdev = { nil, &ke11, dati_ke11, dato_ke11, datob_ke11, svc_null, nil, reset_ke11 };

#ifndef KD11Bp
Busdev klbusdev = { nil, &kl11, dati_kl11, dato_kl11, datob_kl11, svc_kl11, bg_kl11, reset_kl11 };
Busdev kwbusdev = { nil, &kw11, dati_kw11, dato_kw11, datob_kw11, svc_kw11, bg_kw11, reset_kw11 };
#endif

XGP xgp;
Busdev xgpbusdev = { nil, &xgp, dati_xgp, dato_xgp, datob_xgp, svc_xgp, bg_xgp, reset_xgp };
Ten11 ten11;
Busdev ten11busdev = { nil, &ten11, dati_ten11, dato_ten11, datob_ten11, svc_ten11, nil, reset_ten11 };

void
setunibus(uint8 n)
{
	printf("unibus %d OK\n", n);
	return;
}

char *argv0;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-p port] server\n", argv0);
	fprintf(stderr, "\tserver: host running pdp-10\n");
	fprintf(stderr, "\t-p: pdp-10 port; default 1110\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	uint32 sleep;
	int port;
	char *mfile = NULL;

	memset(&cpu, 0, sizeof(cpu));
	memset(&bus, 0, sizeof(Bus));
	cpu.bus = &bus;
	if(0)
		busadddev(&bus, &membusdev);
#ifndef KD11Bp
	busadddev(&bus, &kwbusdev);
	busadddev(&bus, &klbusdev);
#endif
	busadddev(&bus, &kebusdev);
	busadddev(&bus, &xgpbusdev);
	busadddev(&bus, &ten11busdev);

	port = 1110;
	sleep = 0;
	debug = quiet;
	ARGBEGIN{
	case 'p':
		port = atoi(EARGF(usage()));
		break;
	case 'f':
		mfile = EARGF(usage());
		break;
	case 's':
		sleep = atoi(EARGF(usage()));
		break;
	case 'd':
		logfile = stderr;
		debug = log;
		break;
	}ARGEND;

	if((mfile == NULL) == (argc < 1))
		usage();

	ten11.host = argv[0];
	ten11.port = port;
	ten11.cycle = 0;
	ten11.fd = -1;
	ten11.file = mfile;
	ten11.start = 0;
	ten11.length = MEMSIZE;
	loadmem("mem.txt");

//	if(loadpt("maindec/MAINDEC-11-D0NA-PB.ptap"))
//	if(loadpt("maindec/MAINDEC-11-D2AA-PB.ptap"))
//		return 0;

//	cpu.r[7] = 0200;
//	cpu.sw = 0104000;

	//void eaetest(KE11 *ke);
	//eaetest(&ke11);

	initxgp(&xgp);
	initten11(&ten11);

	reset(&cpu);

	ttyopen(&TTYDEV.tty);

	/* wait until we get some data from the 10 */
	cpu.r[7] = 0;
	cpu.throttle = sleep;
	bus.addr = 0;
	bus.data = 0777;
	dato_bus(&bus);
	run(&cpu);

	return 0;
}
