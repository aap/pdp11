#include "11.h"
#include "ten11.h"
#include "util.h"

extern void (*debug) (char *, ...);

void
initten11(Ten11 *ten11, int memsize)
{
}

void
reset_ten11(void *dev)
{
}

static char *re = "";

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

int
svc_ten11(Bus *bus, void *dev)
{
	Ten11 *ten11 = dev;
	uint8 buf[8], len[2];
	int n;
	uint32 a;
	word d;

	if(ten11->fd < 0)
		reconnect(ten11);

	memset(buf, 0, sizeof(buf));
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

	a = buf[3] | buf[2]<<8 | buf[1]<<16;
	d = buf[5] | buf[4]<<8;

	ten11->cycle = 1;
	switch(buf[0]){
	case 1:		/* write */
		bus->addr = a;
		bus->data = d;
		if(a&1) goto be;
		if(dato_bus(bus)) goto be;
		debug("TEN11 write: %06o %06o\n", bus->addr, bus->data);
		buf[0] = 0;
		buf[1] = 1;
		buf[2] = 3;
		writen(ten11->fd, buf, 3);
		break;
	case 2:		/* read */
		bus->addr = a;
		if(a&1) goto be;
		if(dati_bus(bus)) goto be;
		debug("TEN11 read: %06o %06o\n", bus->addr, bus->data);
		buf[0] = 0;
		buf[1] = 3;
		buf[2] = 3;
		buf[3] = bus->data>>8;
		buf[4] = bus->data;
		writen(ten11->fd, buf, 5);
		break;
	default:
		fprintf(stderr, "unknown ten11 message type %d\n", buf[0]);
		break;
	}
	ten11->cycle = 0;
	return 0;
be:
fprintf(stderr, "TEN11 bus error %06o\n", bus->addr);
	buf[0] = 0;
	buf[1] = 1;
	buf[2] = 4;
	writen(ten11->fd, buf, 3);
	ten11->cycle = 0;
	return 0;
}
