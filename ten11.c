#include "11.h"
#include "ten11.h"
#include "util.h"
#include <signal.h>
#include <stdatomic.h>
#include <sys/mman.h>

#define UNIMOBY (128*1024)

typedef struct {
	atomic_uint_fast32_t pdp10_pid;
	atomic_uint_fast8_t pdp10_command;
	atomic_uint_fast32_t pdp10_address;
	atomic_uint_fast16_t pdp10_data;

	atomic_uint_fast32_t pdp11_pid;
	atomic_uint_fast8_t pdp11_status;
	atomic_uint_fast16_t pdp11_data;
	volatile uint8 pdp11_core[UNIMOBY];

	atomic_uint_fast16_t memory[UNIMOBY];
} shared_memory_t;

extern void (*debug) (char *, ...);
static shared_memory_t *mem;
static atomic_bool flag;

static char *re = "";

int
dati_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint32 waddr = bus->addr>>1;
	waddr -= ten11->start;
	if(waddr < ten11->length){
		bus->data = atomic_load(&mem->memory[waddr]);
		return 0;
	}
	return 1;
}

int
dato_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint32 waddr = bus->addr>>1;
	waddr -= ten11->start;
	if(waddr < ten11->length){
		atomic_store(&mem->memory[waddr], bus->data);
		return 0;
	}
	return 1;
}

int
datob_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint32 waddr = bus->addr>>1;
	waddr -= ten11->start;
	if(waddr < ten11->length){
		word d = atomic_load(&mem->memory[waddr]);
		d &= (bus->addr&1) ? 0377 : ~0377;
		d |= bus->data & ((bus->addr&1) ? ~0377 : 0377);
		atomic_store(&mem->memory[waddr], d);
		return 0;
	}
	return 1;
}

static void
reconnect(Ten11 *ten11)
{
	ten11->fd = dial(ten11->host, ten11->port);
	while (ten11->fd == -1) {
		sleep(5);
		ten11->fd = dial(ten11->host, ten11->port);
	}
	nodelay(ten11->fd);
	printf("%sconnected to PDP-10\n", re);
	re = "re";
}

static void
handle_usr1(int sig)
{
	if(mem->pdp10_command == TEN11_ALIVE)
		kill(mem->pdp10_pid, SIGUSR2);
	else
		atomic_store(&flag, 1);
}

void
initten11(Ten11 *ten11)
{
	int i;
	int fd = open(ten11->file, O_CREAT|O_RDWR, 0600);
	if(fd == -1)
		exit(1);
	if(ftruncate(fd, sizeof(shared_memory_t)) == -1)
		exit(1);
	mem = mmap(NULL, sizeof(shared_memory_t), PROT_READ|PROT_WRITE,
		   MAP_SHARED, fd, 0);
	if(mem == MAP_FAILED)
		exit(1);
	mem->pdp11_pid = getpid();
	memset(mem->pdp11_core, 0, sizeof mem->pdp11_core);
	for(i = 0; i < ten11->length; i++)
		mem->pdp11_core[ten11->start + i] = 1;
	signal(SIGUSR1, handle_usr1);
}

void
reset_ten11(void *dev)
{
	atomic_store(&flag, 0);
}

static uint8
ten11_command(Ten11 *ten11, uint32 *a, word *d)
{
	uint8 buf[8], len[2];
	int n;
	if(0) {
		if(ten11->fd < 0)
			reconnect(ten11);
		memset(buf, 0, 8);
		if(!hasinput(ten11->fd))
			return 0;
		n = read(ten11->fd, len, 2);
		if(n != 2){
			printf("fd closed, reconnecting...\n");
			close(ten11->fd);
			ten11->fd = -1;
			return 0;
		}
		if(len[0] != 0){
			fprintf(stderr, "unibus botch, exiting\n");
			exit(0);
		}
		if(len[1] > 6){
			fprintf(stderr, "unibus botch, exiting\n");
			exit(0);
		}
		readn(ten11->fd, buf, len[1]);
	 	*a = buf[3] | buf[2]<<8 | buf[1]<<16;
 		*d = buf[5] | buf[4]<<8;
		return buf[0];
	} else {
		if(!atomic_load(&flag))
			return 0;
		atomic_store(&flag, 0);
		*a = atomic_load(&mem->pdp10_address);
		*d = atomic_load(&mem->pdp10_data);
		return atomic_load(&mem->pdp10_command);
	}
}

static void
ten11_write(Ten11 *ten11, uint32 a, word d)
{
	debug("TEN11 write: %06o %06o\n", a, d);
	if(0) {
		uint8 buf[3];
		buf[0] = 0;
		buf[1] = 1;
		buf[2] = 3;
		writen(ten11->fd, buf, 3);
	} else {
		kill(mem->pdp10_pid, SIGUSR2);
	}
}

static void
ten11_read(Ten11 *ten11, uint32 a, word d)
{
	debug("TEN11 read: %06o %06o\n", a, d);
	if(0) {
		uint8 buf[5];
		buf[0] = 0;
		buf[1] = 3;
		buf[2] = 3;
		buf[3] = d>>8;
		buf[4] = d;
		writen(ten11->fd, buf, 5);
	} else {
		atomic_store(&mem->pdp11_data, d);
		kill(mem->pdp10_pid, SIGUSR2);
	}
}

static void
ten11_err(Ten11 *ten11, uint32 a)
{
	fprintf(stderr, "TEN11 bus error %06o\n", a);
	if (0) {
		uint8 buf[3];
		buf[0] = 0;
		buf[1] = 1;
		buf[2] = 4;
		writen(ten11->fd, buf, 3);
	} else {
		kill(mem->pdp10_pid, SIGUSR2);
	}
}

int
svc_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint8 command;
	uint32 a;
	word d;

	command = ten11_command(ten11, &a, &d);
	if (command == 0)
		return 0;

	ten11->cycle = 1;
	switch(command){
	case TEN11_WRITE:	/* write */
		bus->addr = a;
		bus->data = d;
		if(a&1) goto be;
		if(dato_bus(bus)) goto be;
		ten11_write(ten11, a, d);
		break;
	case TEN11_READ:	/* read */
		bus->addr = a;
		if(a&1) goto be;
		if(dati_bus(bus)) goto be;
		ten11_read(ten11, a, bus->data);
		break;
	default:
		fprintf(stderr, "unknown ten11 message type %d\n", command);
		break;
	}
	ten11->cycle = 0;
	return 0;
be:
	ten11_err(ten11, a);
	ten11->cycle = 0;
	return 0;
}
