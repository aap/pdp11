#include <stdio.h>
#include <signal.h>
#include <wiringPi.h>

#include "threading.h"
#include "11.h"
#include "kw11.h"
#include "kl11.h"
#include "rk11.h"
#include "dc11_fake.h"
//#include "args.h"
#include "ukd11a.h"

/*
LED_ROWS = [20, 21, 22, 23, 24, 25]
SWITCH_ROWS = [16, 17, 18]
COLUMNS = [26, 27, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
*/
int LED_ROWS[] = {28, 29, 3, 4, 5, 6};
int SWITCH_ROWS[] = {27, 0, 1};
int COLUMNS[] = {25, 2, 7, 21, 22, 11, 10, 13, 12, 14, 26, 23};

#define nelem(array) (sizeof(array)/sizeof(array[0]))

void
disablePin(int pin)
{
	pinMode(pin, INPUT);
	pullUpDnControl(pin, PUD_OFF);
}

void
enablePin(int pin)
{
	pinMode(pin, INPUT);
	pullUpDnControl(pin, PUD_UP);
}

void
outputPin(int pin, int val)
{
	pinMode(pin, OUTPUT);
	digitalWrite(pin, val);
}

void
disableRows(void)
{
	int i;
	for(i = 0; i < nelem(LED_ROWS); i++)
		disablePin(LED_ROWS[i]);
	for(i = 0; i < nelem(SWITCH_ROWS); i++)
		disablePin(SWITCH_ROWS[i]);
}

void
enableRows(void)
{
	int i;
	for(i = 0; i < nelem(LED_ROWS); i++)
		outputPin(LED_ROWS[i], 1);
	for(i = 0; i < nelem(SWITCH_ROWS); i++)
		outputPin(SWITCH_ROWS[i], 1);
}

void
disableColumns(void)
{
	int i;
	for(i = 0; i < nelem(COLUMNS); i++)
		disablePin(COLUMNS[i]);
}

void
enableColumns(void)
{
	int i;
	for(i = 0; i < nelem(COLUMNS); i++)
		enablePin(COLUMNS[i]);
}



void
setLights(Console *con)
{
	int i, j;

	outputPin(LED_ROWS[0], 1);
	for(i = 0; i < 12; i++)
		outputPin(COLUMNS[i], (~con->address_lights >> i)&1);
	delay(2);
	outputPin(LED_ROWS[0], 0);

	outputPin(LED_ROWS[1], 1);
	for(i = 0; i < 10; i++)
		outputPin(COLUMNS[i], (~con->address_lights >> (i+12))&1);
	delay(2);
	outputPin(LED_ROWS[1], 0);

	outputPin(LED_ROWS[2], 1);
	for(i = 0; i < 12; i++)
		outputPin(COLUMNS[i], (~con->status_lights >> i)&1);
	delay(2);
	outputPin(LED_ROWS[2], 0);

	outputPin(LED_ROWS[3], 1);
	for(i = 0; i < 12; i++)
		outputPin(COLUMNS[i], (~con->data_lights >> i)&1);
	delay(2);
	outputPin(LED_ROWS[3], 0);

	outputPin(LED_ROWS[4], 1);
	for(i = 0; i < 4; i++)
		outputPin(COLUMNS[i], (~con->data_lights >> (i+12))&1);
	// parity lights
	outputPin(COLUMNS[i++], 0);
	outputPin(COLUMNS[i++], 0);
	// indicators left, going down
	j = 1;
	for(; i < 10; i++)
		outputPin(COLUMNS[i], con->address_select != j++);
	j = 1;
	for(; i < 12; i++)
		outputPin(COLUMNS[i], con->data_select != j++);
	delay(2);
	outputPin(LED_ROWS[4], 0);

	outputPin(LED_ROWS[5], 1);
	for(i = 0; i < 6; i++)
		outputPin(COLUMNS[i], 1);
	// indicators right, going down
	j = 5;
	for(; i < 10; i++)
		outputPin(COLUMNS[i], con->address_select != j++);
	j = 3;
	for(; i < 12; i++)
		outputPin(COLUMNS[i], con->data_select != j++);
	delay(2);
	outputPin(LED_ROWS[5], 0);
}

// this doesn't work very well when you're twisting the knob fast :(
void
handleKnob(int *knob, int *state, int input)
{
	switch(*state){
	case -3:
		switch(input){
		case 0:	// completed down movement
			*state = 0;
			(*knob)--;
			break;
		case 3:	// back to second step
			(*state)++;
			break;
		}
		break;
	case -2:	// seond step down
		switch(input){
		case 1:
			(*state)--;
			break;
		case 2:
			(*state)++;
			break;
		}
		break;
	case -1:	// first step down
		switch(input){
		case 3:
			(*state)--;
			break;
		case 0:
			(*state)++;
			break;
		}
		break;
	case 0:		// neutral
		switch(input){
		case 1:
			(*state)++;
			break;
		case 2:
			(*state)--;
			break;
		}
		break;
	case 1:		// first step up
		switch(input){
		case 3:
			(*state)++;
			break;
		case 0:
			(*state)--;
			break;
		}
		break;
	case 2:		// second step up
		switch(input){
		case 2:
			(*state)++;
			break;
		case 1:
			(*state)--;
			break;
		}
		break;
	case 3:
		switch(input){
		case 0:	// completed up movement
			*state = 0;
			(*knob)++;
			break;
		case 3:	// back to second step
			(*state)--;
			break;
		}
		break;
	}
	(*knob) = ((*knob)+20)%20;
}

void
readSwitches(Console *con)
{
	int i;

	int sw = 0;

	enableColumns();
	outputPin(SWITCH_ROWS[0], 0);
	delay(2);
	for(i = 0; i < nelem(COLUMNS); i++)
		sw |= !digitalRead(COLUMNS[i]) << i;
	disablePin(SWITCH_ROWS[0]);

	outputPin(SWITCH_ROWS[1], 0);
	delay(2);
	for(i = 0; i < nelem(COLUMNS); i++)
		sw |= !digitalRead(COLUMNS[i]) << (i+12);
	disablePin(SWITCH_ROWS[1]);

	con->data_switches = sw & 017777777;

	sw = (sw&020000000)>>14;
	outputPin(SWITCH_ROWS[2], 0);
	delay(2);
	for(i = 0; i < 8; i++)
		sw |= !digitalRead(COLUMNS[i]) << i;
	sw ^= 1;
	lock(&con->sw_lock);
	con->toggled_switches |= sw & ~con->cntl_switches;
	unlock(&con->sw_lock);
	con->cntl_switches = sw;

	sw = 0;
	for(; i < 12; i++)
		sw |= !digitalRead(COLUMNS[i]) << (i-8);
	disablePin(SWITCH_ROWS[2]);

	handleKnob(&con->knob1, &con->knob1state, sw&3);
	handleKnob(&con->knob2, &con->knob2state, (sw>>2)&3);

	switch(con->knob1){
	case 0: case 5: case 10: case 15:
		con->address_select = 0;
		break;
	case 1: case 2:
		con->address_select = 2;
		break;
	case 3: case 4:
		con->address_select = 1;
		break;
	case 6: case 7:
		con->address_select = 5;
		break;
	case 8: case 9:
		con->address_select = 6;
		break;
	case 11: case 12:
		con->address_select = 7;
		break;
	case 13: case 14:
		con->address_select = 8;
		break;
	case 16: case 17:
		con->address_select = 4;
		break;
	case 18: case 19:
		con->address_select = 3;
		break;
	}
	switch(con->knob2){
	case 0: case 5: case 10: case 15:
		con->data_select = 0;
		break;
	case 1: case 2: case 3: case 4:
		con->data_select = 1;
		break;
	case 6: case 7: case 8: case 9:
		con->data_select = 3;
		break;
	case 11: case 12: case 13: case 14:
		con->data_select = 4;
		break;
	case 16: case 17: case 18: case 19:
		con->data_select = 2;
		break;
	}
//	printf("%d %d\n", knob1, knob2);
}

int diagpassed;

void*
emuthread(void *arg)
{
	KD11A *cpu = (KD11A*)arg;

	KD11A_poweron(cpu);
	for(;;){
		KD11A_service(cpu);
		if(diagpassed){
			printf("PASSED\n");
			exit(0);
		}
	}
}

void
interrupt(int x)
{
	disableRows();
	disableColumns();
	exit(0);
}








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

KD11A cpu;
Bus bus;
KW11 kw11;
KL11 kl11;
RK11 rk11;
DC11 dc11;
Memory memdev = { memory, 0, MEMSIZE };
//Memory romdev = { rom, 0773100>>1, 0773200>>1 };
Busdev membusdev = { nil, &memdev, dati_mem, dato_mem, datob_mem, svc_null, nil, reset_null };
//Busdev rombusdev = { nil, &romdev, dati_rom, dato_rom, datob_rom, svc_null, nil, reset_null };
Busdev klbusdev = { nil, &kl11, dati_kl11, dato_kl11, datob_kl11, svc_kl11, bg_kl11, reset_kl11 };
//Busdev kwbusdev = { nil, &kw11, dati_kw11, dato_kw11, datob_kw11, svc_kw11, bg_kw11, reset_kw11 };
//Busdev rkbusdev = { nil, &rk11, dati_rk11, dato_rk11, datob_rk11, svc_rk11, bg_rk11, reset_rk11 };
//Busdev dcbusdev = { nil, &dc11, dati_dc11, dato_dc11, datob_dc11, svc_dc11, bg_dc11, reset_dc11 };

int
threadmain(int argc, char *argv[])
{
	if(wiringPiSetup() == -1)
		return 1;

	signal(SIGINT, interrupt);

	disableRows();
	disableColumns();

	KD11A_initonce();
	memset(&cpu, 0, sizeof(KD11A));
	memset(&bus, 0, sizeof(Bus));
	cpu.bus = &bus;
	busadddev(&bus, &membusdev);
	//busadddev(&bus, &kwbusdev);
	busadddev(&bus, &klbusdev);
	//busadddev(&bus, &rkbusdev);
	//busadddev(&bus, &dcbusdev);
	//busadddev(&bus, &rombusdev);

//	loadmem("mem.txt");

	loadpt("maindec/ZKAAA0.BIN");	// branch
//	memory[014176>>1] = 0;
	memory[014210>>1] = 0;
//	memory[014224>>1] = 0;
//	loadpt("maindec/ZKABA0.BIN");	// con branch
//	loadpt("maindec/ZKACA0.BIN");	// unary
//	loadpt("maindec/ZKADA0.BIN");	// binary
//	loadpt("maindec/ZKAEA0.BIN");	// rot/shf
//	loadpt("maindec/ZKAFA0.BIN");	// cmp
//	loadpt("maindec/ZKAGA0.BIN");	// cmp not
//	loadpt("maindec/ZKAHA0.BIN");	// move
//	loadpt("maindec/ZKAIA0.BIN");	// bis bic bit
//	loadpt("maindec/ZKAJA0.BIN");	// add
//	loadpt("maindec/ZKAKA0.BIN");	// sub
//	loadpt("maindec/ZKALA0.BIN");	// jmp
//	loadpt("maindec/ZKAMA0.BIN");	// rts rti jsr

	/* open a tty if it exists */
	kl11.ttyfd = open("/tmp/tty", O_RDWR);
	printf("tty connected to %d\n", kl11.ttyfd);


	readSwitches(&cpu.con);	// need to know switches on power up
	threadcreate(emuthread, &cpu);
	for(;;){
		setLights(&cpu.con);
		readSwitches(&cpu.con);
	}

	return 0 ;
}
