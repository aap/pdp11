#include "11.h"
#include "tv.h"
#include "util.h"
#include <sys/poll.h>
#include <pthread.h>

/* maximum:
 * 256 framebuffers
 * 64 keyboards
 * 16 video switch inputs (buffers) per section
 * 32 video switch outputs (displays)
 */

enum {
	WIDTH = 576,
	HEIGHT = 454
};

enum {
	TVLO = 060000,
	TVHI = TVLO + WIDTH*HEIGHT/8,
	CSA = 0157776,
	CREG = 0764044,
	KMS = 0764050,
	KMA = 0764052,
	VSW = 0764060,
	ASW = 0770670,
};

enum {
	ALU_SETC = 0,
	ALU_NOR,
	ALU_ANDC,
	ALU_SETZ,
	ALU_NAND,
	ALU_COMP,
	ALU_XOR,
	ALU_ANDCSD,
	ALU_ORCSD,
	ALU_EQV,
	ALU_SAME,
	ALU_AND,
	ALU_ORSCD,
	ALU_SETO,
	ALU_IOR,
	ALU_SET
};

static pthread_mutex_t lock;
static pthread_mutex_t kblock;
static pthread_cond_t kbcond;
#define GUARD	pthread_mutex_lock(&lock)
#define UNGUARD	pthread_mutex_unlock(&lock)

static word nullfb[16*1024 - 1];

/* map from input switch to buffer number
 * This is the config at tech square that the TV program expects */
static int dpymap[NUMSECTIONS][NUMINPUTS] = {
	/* first section */
	{ -1,	/* null input */
	  0, 1, 2, 3, 4, 5, 6,
	  7, 014, 015, 016, 017, -1, -1, -1 },
	/* first section */
	{ -1,	/* null input */
	  010, 011, 012, 013, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1 }
};

/* List of video switch output and a keyboard */
static struct {
	int dpy;
	int kbd;
} dpykbdlist[NUMOUTPUTS] = {
	{ 0,	0, },	// 809 FAHLMAN, HOLLOWAY, KNIGHT
	{ 01,	014 },	// 820 MINSKY
	{ 02,	021 },	// 824 RICH, DEKLEER
	{ 03,	06 },	// 808 FREILING, ULLMAN
	{ 04,	010 },	// 817 JABARI
	{ 05,	022 },	// 825 Sjoberg
	{ 06,	04 },	// 813 HEWITT
	{ 07,	05 },	// 814 SUSSMAN
	{ 010,	013 },	// 819 GOLDSTEIN
	{ 011,	017 },	// 821A  MARR
	{ 012,	035 },	// 914 COHEN, GOSPER, ETC.
	{ 013,	034 },	// 913 BAISLEY, GREENBLATT
	{ 014,	033 },	// 334 EDWARDS, LEBEL
	{ 015,	030 },	// 925 MOON'S REFRIGERATOR
	{ 016,	031 },	// 902 TAENZER, MASON
	{ 017,	032 },	// 919 Very Small Data losers
	{ 020,	03 },	// 812 YVONNE
	{ 021,	036 },	// 912 9TH FLOOR LOUNGE
	{ 022,	037 },	// 907 CHESS, LISP MACHINES
	{ 023,	01 },	// 810 LAVIN, KUIPERS, MILLER
	{ 024,	02 },	// 919 Very Small Data Bases NORTH (FAR END)
	{ -1,	-1, },
	{ 026,	026 },	// 826 Fredkin
	{ -1,	-1, },
	{ 030,	046 },	// 3rd Floor #6
	{ 031,	031 },	// 815 Horn
	{ -1,	-1, },
	{ 033,	045 },	// 3rd Floor #5
	{ 034,	044 },	// 3rd Floor #4
	{ 035,	043 },	// 3rd Floor #3
	{ 036,	042 },	// 3rd Floor #2
	{ 037,	041 },	// 3rd Floor #1
};

void sendfb(TV *tv, int osw);
void sendbuf(TV *tv, FBuffer *buffer);
void sendupdate(TV *tv, FBuffer *buffer, uint16 addr);

void
vswinfo(TV *tv)
{
	int i, j;
	int osw;
	printf("Video switch:\n");
	for(i = 0; i < NUMOUTPUTS; i++)
		printf("%o|%o -> %o\n", tv->vswsect[0][i], tv->vswsect[1][i], i);
	printf("frame buffer output:\n");
	for(i = 0; i < NUMFBUFFERS; i++)
		for(j = 0; j < tv->buffers[i].nosw; j++){
			osw = tv->buffers[i].osw[j];
			printf("%o %o: %o %o\n", i, j, osw, tv->omap[osw]);
		}
}

static void
updatevsw(TV *tv)
{
	int i, j, n;
	for(i = 0; i < NUMFBUFFERS; i++){
		n = 0;
		for(j = 0; j < NUMOUTPUTS; j++)
			if(dpymap[0][tv->vswsect[0][j]] == i ||
			   dpymap[1][tv->vswsect[1][j]] == i)
				tv->buffers[i].osw[n++] = j;
		tv->buffers[i].nosw = n;
	}
}

static word
alu(int op, word a, word b)
{
	switch(op){
	case ALU_SETC:	a = ~b; break;
	case ALU_NOR:	a = ~(a|b); break;
	case ALU_ANDC:	a = a&~b; break;
	case ALU_SETZ:	a = 0; break;
	case ALU_NAND:	a = ~(a&b); break;
	case ALU_COMP:	a = ~a; break;
	case ALU_XOR:	a ^= b; break;
	case ALU_ANDCSD: a = ~a&b; break;
	case ALU_ORCSD:	a = a|~b; break;
	case ALU_EQV:	a = ~(a^b); break;
	case ALU_SAME:	break;
	case ALU_AND:	a &= b; break;
	case ALU_ORSCD:	a = ~a|b; break;
	case ALU_SETO:	a = ~0; break;
	case ALU_IOR:	a |= b; break;
	case ALU_SET:	a = b; break;
	default: a = 0;	/* can't happen */
	}
	return a;
}

static int
oc_child(void)
{
	int fd[2];
	pid_t pid;

	const char *prog = getenv("TV11KMS");
	if(prog == NULL)
		return -1;

	if(pipe(fd) == -1)
		return -1;

	pid = fork();
	if(pid == -1)
		return -1;
	else if(pid == 0) {
		close(0);
		dup2(fd[0], 0);
		close(fd[0]);
		close(fd[1]);
		execlp(prog, prog, NULL);
		exit(1);
	}

	close(fd[0]);
	return fd[1];
	
}

static void
oc_output(unsigned char bits)
{
	static int old = -1;
	static int fd = -1;

	if(bits != old) {
		if(fd == -1)
			fd = oc_child();

		(void)write(fd, &bits, 1); /* error? lol yolo */
	}

	old = bits;
}

int
dato_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	int waddr = bus->addr>>1;
	word w, d;
	word *creg;
	FBuffer *curbuf;

	creg = tv->ten11->cycle ? &tv->creg10 : &tv->creg11;
	curbuf = (*creg & BUFMASK) < NUMFBUFFERS ? &tv->buffers[*creg & BUFMASK] : nil;
	d = bus->data;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		if(curbuf == nil)
			return 0;
		/* In Buffer */
		if(bus->addr == CSA){
			curbuf->csa = alu(*creg>>8, curbuf->csa, d);
			curbuf->mask = curbuf->csa&010000 ? ~0 : 0;
			sendbuf(tv, curbuf);
		}else{
			waddr -= TVLO>>1;
			w = curbuf->fb[waddr];
			curbuf->fb[waddr] = alu(*creg>>8, w, d);
			sendupdate(tv, curbuf, waddr);
		}
		return 0;
	}
	switch(bus->addr){
	case CREG:
//printf("writing CREG %06o\n", d);
		*creg = d;
		return 0;
	case CREG+2:
		/* unused, written only as side effect */
		return 0;
	case KMS:
		tv->kms = d & ~037;
		oc_output ((tv->kms >> 8) & 017);
		SETMASK(tv->kma_hi, (d&3)<<16, 3<<16);
		return 0;
	case KMA:
		tv->kma_lo = d & 0174;
		tv->kma_hi = d & ~0177;
		SETMASK(tv->kma_hi, d&~0177, ~0177);
		return 0;
	case VSW:{
		int i, o, s;
		i = d&017;	
		o = d>>8 & 037;
		s = d>>13 & 07;
		if(s < 2)
			tv->vswsect[s][o] = i;
		updatevsw(tv);
		sendfb(tv, o);
		}
		return 0;
	case ASW:
		// TODO, but we don't support audio anyway
		return 0;
	}
	return 1;
}

int
datob_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	int waddr = bus->addr>>1;
	word w, d, m;
	word *creg;
	FBuffer *curbuf;

	creg = tv->ten11->cycle ? &tv->creg10 : &tv->creg11;
	curbuf = (*creg & BUFMASK < NUMFBUFFERS) ? &tv->buffers[*creg & BUFMASK] : nil;
	d = bus->data;
	m = bus->addr&1 ? ~0377 : 0377;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		if(curbuf == nil)
			return 0;
		/* In Buffer */
		if((bus->addr&~1) == CSA){
			w = alu(*creg>>8, curbuf->csa, d);
			SETMASK(curbuf->csa, w, m);
			curbuf->mask = curbuf->csa&010000 ? ~0 : 0;
			sendbuf(tv, curbuf);
		}else{
			waddr -= TVLO>>1;
			w = alu(*creg>>8, curbuf->fb[waddr], d);
			SETMASK(curbuf->fb[waddr], w, m);
			sendupdate(tv, curbuf, waddr);
		}
		return 0;
	}
	switch(bus->addr&~1){
	case CREG:
//printf("writing CREGb %06o\n", d);
		SETMASK(*creg, d, m);
		return 0;
	case CREG+2:
		/* unused, written only as side effect */
		return 0;
	case KMS:
		/* Don't know if this is allowed,
		 * so catch it if it happens. */
		assert(0 && "KMS write");
		return 0;
	case KMA:
		assert(0 && "KMA write");
		return 0;
	case VSW:
	case ASW:
		// don't allow byte writes to this
		// doesn't make that much sense
		return 0;
	}
	return 1;
}

int
dati_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	word *creg;
	FBuffer *curbuf;

	creg = tv->ten11->cycle ? &tv->creg10 : &tv->creg11;
	curbuf = (*creg & BUFMASK) < NUMFBUFFERS ? &tv->buffers[*creg & BUFMASK] : nil;
	int waddr = bus->addr>>1;
	if(bus->addr >= TVLO && bus->addr < 0160000){
		if(curbuf == nil)
			return 0;
		/* In Buffer */
		if((bus->addr&~1) == CSA)
			bus->data = curbuf->csa;
		else{
			waddr -= TVLO>>1;
			bus->data = curbuf->fb[waddr];
		}
		return 0;
	}
	switch(bus->addr){
	case CREG:
		bus->data = *creg;
		return 0;
	case CREG+2:
		/* unused, read only as side effect */
		bus->data = 0;
		return 0;
	case KMS:
		bus->data = tv->kms&~037 | tv->kma_hi>>16 & 3;
		return 0;
	case KMA:
		bus->data = tv->kma_hi | tv->kma_lo;
		return 0;
	case VSW:
	case ASW:
		/* write only */
		bus->data = 0;
		return 0;
	}
	return 1;
}

void
reset_tv(void *dev)
{
	TV *tv = dev;
	memset(tv->buffers, 0, sizeof(tv->buffers));
	tv->creg11 = 0;
	tv->creg10 = 0;
	memset(tv->vswsect, 0, sizeof(tv->vswsect));

	tv->kms = 0;
	tv->kma_hi = 0;
	tv->kma_lo = 0;
	tv->km_haskey = 0;
	tv->km_kbd = 0;
	tv->km_key = 0;

	updatevsw(tv);
}

void
inittv(TV *tv)
{
	TVcon *con;
	int i;
	for(con = tv->cons; con < &tv->cons[NUMCONNECTIONS]; con++){
		con->fd = -1;
		con->dpy = -1;
		con->kbd = -1;
	}
	for(i = 0; i < NUMOUTPUTS; i++)
		tv->omap[i] = -1;
	pthread_mutex_init(&lock, nil);
	pthread_mutex_init(&kblock, nil);
	pthread_cond_init(&kbcond, nil);
}

/*
 * The network TV connections
 */

enum {
	/* TV to 11 */
	MSG_KEYDN = 0,
	MSG_GETFB,

	/* 11 to TV */
	MSG_FB,
	MSG_WD,
	MSG_CLOSE,
};

word
b2w(uint8 *b)
{
	return b[0] | b[1]<<8;
}

void
w2b(uint8 *b, word w)
{
	b[0] = w;
	b[1] = w>>8;
}

void
msgheader(uint8 *b, uint8 type, uint16 length)
{
	w2b(b, length);
	b[2] = type;
}

uint8 largebuf[64*1024];

void
dumpbuf(uint8 *b, int n)
{
	while(n--)
		printf("%o ", *b++);
	printf("\n");
}

static void
packfb(TV *tv, uint8 *dst, int osw, int x, int y, int w, int h)
{
	int stride;
	int n1, n2;
	word *src1, *src2;
	word bw1, bw2;

	stride = WIDTH/16;

	/* We mix the outputs of both sections for the final output.
	 * This feature does not seem to be used by the TV system
	 * but it's theoretically capable of doing something like this.
	 * External video input is not supported here of course. */
	n1 = dpymap[0][tv->vswsect[0][osw]];
	n2 = dpymap[1][tv->vswsect[1][osw]];
//printf("inbuf: %d %d\n", n1, n2);
	src1 = n1 < 0 ? nullfb : &tv->buffers[n1].fb[stride*y + x];
	src2 = n2 < 0 ? nullfb : &tv->buffers[n2].fb[stride*y + x];
	bw1 = n1 < 0 ? 0 : tv->buffers[n1].mask;
	bw2 = n2 < 0 ? 0 : tv->buffers[n2].mask;
	for(y = 0; y < h; y++){
		for(x = 0; x < w; x++){
			/* We mix with an OR */
			w2b(dst, src1[x]^bw1 | src2[x]^bw2);
			dst += 2;
		}
		src1 += stride;
		src2 += stride;
	}
}

void
sendfb(TV *tv, int osw)
{
	TVcon *con = &tv->cons[tv->omap[osw]];
	if(tv->omap[osw] < 0)
		return;
	uint8 *b = largebuf;
	msgheader(b, MSG_FB, 1+8+WIDTH*HEIGHT/8);
	b += 3;
	w2b(b, 0);
	w2b(b+2, 0);
	w2b(b+4, WIDTH/16);
	w2b(b+6, HEIGHT);
	b += 8;
	packfb(tv, b, con->dpy, 0, 0, WIDTH/16, HEIGHT);
	writen(con->fd, largebuf, 3+8+WIDTH*HEIGHT/8);
}

void
sendbuf(TV *tv, FBuffer *buffer)
{
	int i;
	for(i = 0; i < buffer->nosw; i++)
		if(buffer->osw[i] >= 0)
			sendfb(tv, buffer->osw[i]);
}

/* Send a single word update to all displays */
void
sendupdate(TV *tv, FBuffer *buffer, uint16 addr)
{
	uint8 buf[7];
	int i;
	int osw;
	int n1, n2;
	word w1, w2;
	word bw1, bw2;

	/* Don't tell screen to update anything not visible */
	if(addr >= (TVHI-TVLO)/2)
		return;

	msgheader(buf, MSG_WD, 5);
	w2b(buf+3, addr);

//	printf("sending update of %o: %o\n", addr, tv->curbuf->fb[addr]);

	/* Again do the mixing thing for no reason */
	for(i = 0; i < buffer->nosw; i++){
		osw = buffer->osw[i];
		if(osw < 0 || tv->omap[osw] < 0)
			continue;
		n1 = dpymap[0][tv->vswsect[0][osw]];
		n2 = dpymap[1][tv->vswsect[1][osw]];
		w1 = n1 < 0 ? 0 : tv->buffers[n1].fb[addr];
		w2 = n2 < 0 ? 0 : tv->buffers[n2].fb[addr];
		bw1 = n1 < 0 ? 0 : tv->buffers[n1].mask;
		bw2 = n2 < 0 ? 0 : tv->buffers[n2].mask;
		w2b(buf+5, w1^bw1 | w2^bw2);
		writen(tv->cons[tv->omap[osw]].fd, buf, 7);
	}
}

/* Tell the other side what we have allocated for them.
 * Not terribly important but they may want to know. */
void
setdpykbd(int fd, int dpy, int kbd)
{
	uint8 buf[2];
	buf[0] = dpy;
	buf[1] = kbd;
	writen(fd, buf, 2);
}

/* Get first free connection or -1 */
static int
alloccon(TV *tv)
{
	int i;
	for(i = 0; i < NUMCONNECTIONS; i++)
		if(tv->cons[i].fd < 0 && dpykbdlist[i].dpy >= 0)
			return i;
	return -1;
}

/* NB: this should be GUARDed by the caller */
static void
closecon(TV *tv, TVcon *con)
{
	printf("disconnect display %o\n", con->dpy);
	tv->omap[con->dpy] = -1;
	close(con->fd);
	con->fd = -1;
	con->dpy = -1;
	con->kbd = -1;
}

void
closetv(TV *tv)
{
	uint8 buf[3];
	int i;
	for(i = 0; i < NUMCONNECTIONS; i++)
		if(tv->cons[i].fd >= 0){
			msgheader(buf, MSG_CLOSE, 1);
			writen(tv->cons[i].fd, buf, 3);
			/* wait for close */
			read(tv->cons[i].fd, buf, 1);
			/* this will cause the handletv thread
			 * to close the connection...if it makes
			 * it that far */
		}
}

void
accepttv(int fd, void *arg)
{
	TV *tv = arg;
	int c;
	TVcon *con;

	c = alloccon(tv);
	if(c < 0){
		/* Nothing free */
		setdpykbd(fd, -1, -1);
		close(fd);
		return;
	}
	con = &tv->cons[c];
	GUARD;
	con->dpy = dpykbdlist[c].dpy;
	con->kbd = dpykbdlist[c].kbd;
	con->fd = fd;
	nodelay(con->fd);
	tv->omap[con->dpy] = c;
	UNGUARD;
	setdpykbd(con->fd, con->dpy, con->kbd);
	printf("connected display %o\n", con->dpy);
}

int
svc_tv(Bus *bus, void *dev)
{
	TV *tv = dev;
	word key, kbd;
	int ch;

	ch = (tv->kms&0100040) == 0100040 ? 5 : 0;
	if(!tv->km_haskey)
		return ch;
	pthread_mutex_lock(&kblock);
	tv->km_haskey = 0;
	key = tv->km_key;
	kbd = tv->km_kbd;
	pthread_cond_signal(&kbcond);
	pthread_mutex_unlock(&kblock);

	bus->addr = tv->kma_hi | tv->kma_lo;
	bus->data = key;
	dato_bus(bus);
	tv->kma_lo = tv->kma_lo+2 & 0177;
	bus->addr = tv->kma_hi | tv->kma_lo;
	bus->data = (kbd&077) << 8;
	dato_bus(bus);
	tv->kma_lo = tv->kma_lo+2 & 0177;
	tv->kms &= ~0200;	/* DMA done */
	tv->kms |= 0100000;	/* interrupt bit */
	ch = (tv->kms&0100040) == 0100040 ? 5 : 0;
	return ch;
}

int
bg_tv(void *dev)
{
	TV *tv = dev;
	tv->kms &= ~0100000;
	return 0340;
}

/* Send key to emu thread to write to memory.
 * This BLOCKS if the emu thread doesn't consume keys! */
static void
sendkey(TV *tv, word kbd, word key)
{
	if((tv->kms & 0100) == 0)	/* write to mem */
		return;

	/* wait until we can send a key */
	pthread_mutex_lock(&kblock);
	tv->kms |= 0200;
	while(tv->km_haskey != 0)
		pthread_cond_wait(&kbcond, &kblock);

	tv->km_haskey = 1;
	tv->km_key = key;
	tv->km_kbd = kbd;
	pthread_mutex_unlock(&kblock);
}

/* This is executed within GUARDs */
static void
handlemsg(TV *tv, TVcon *con)
{
	uint16 len;
	uint8 *b;
	uint8 type;
	int x, y, w, h;

	if(readn(con->fd, &len, 2)){
err:
		closecon(tv, con);
		return;
	}
	len = b2w((uint8*)&len);

	b = largebuf;
	if(readn(con->fd, b, len))
		goto err;
	type = *b++;
	switch(type){
	case MSG_KEYDN:
		sendkey(tv, con->kbd, b2w(b));
		break;

	case MSG_GETFB:
		x = b2w(b);
		y = b2w(b+2);
		w = b2w(b+4);
		h = b2w(b+6);
		printf("getfb: %d %d %d %d\n", x, y, w, h);

		x /= 16;
		w = (w+15) / 16;
		b = largebuf;
		msgheader(b, MSG_FB, 1+8+w*h*2);
		b += 3;
		w2b(b, x);
		w2b(b+2, y);
		w2b(b+4, w);
		w2b(b+6, h);
		b += 8;
		packfb(tv, b, con->dpy, x, y, w, h);
		writen(con->fd, largebuf, 3+8+w*h*2);
		break;

	default:
		fprintf(stderr, "unknown msg type %d\n", type);
	}
	
}

static void*
handletv_thread(void *arg)
{
	TV *tv = arg;
	int i;
	int nfds;
	int n;
	struct pollfd fds[NUMCONNECTIONS];
	int conmap[NUMCONNECTIONS];
	TVcon *con;

	for(;;){
		nfds = 0;
		for(i = 0; i < NUMCONNECTIONS; i++){
			if(tv->cons[i].fd < 0)
				continue;
			fds[nfds].fd = tv->cons[i].fd;
			fds[nfds].events = POLLIN;
			conmap[nfds] = i;
			nfds++;
		}
		if(nfds == 0){
			sleep_ms (200);
			continue;
		}

		/* We need a timeout here so poll can see
		 * when we open a new connection */
		n = poll(fds, nfds, 200);
		if(n < 0){
			perror("error: poll");
			/* Is this the correct thing to do? */
			return nil;
		}	
		/* timeout */
		if(n == 0)
			continue;

		for(i = 0; i < nfds; i++){
			if(fds[i].revents == 0)
				continue;
			con = &tv->cons[conmap[i]];
			if(fds[i].revents != POLLIN){
				/* When does this happen? */
				GUARD;
				closecon(tv, con);
				UNGUARD;
				continue;
			}
			GUARD;
			/* Check whether it's still the connection
			 * we thought we're using. */
			/* TODO: make this check more sound */
			if(fds[i].fd == con->fd){
				/* All good, talk to the display */
//				printf("handling con %d\n", conmap[i]);
				handlemsg(tv, con);
			}
			UNGUARD;
		}
	}
	return nil;
}

void
handletvs(TV *tv)
{
	pthread_t th;
	pthread_create(&th, nil, handletv_thread, tv);
}

struct Serveargs {
	TV *tv;
	int port;
};

static void*
servetv_thread(void *arg)
{
	struct Serveargs *sa = arg;
	serve(sa->port, accepttv, sa->tv);
	free(sa);
	return nil;
}

void
servetv(TV *tv, int port)
{
	pthread_t th;
	struct Serveargs *sa;
	sa = malloc(sizeof(struct Serveargs));
	sa->tv = tv;
	sa->port = port;
	pthread_create(&th, nil, servetv_thread, sa);
}

#if 0
void
tvtest(TV *tv, Bus *bus)
{
	bus->addr = VSW;
	bus->data = WD(0 | 0, 1);
	dato_bus(bus);
	bus->data = WD(0 | 1, 2);
	dato_bus(bus);
	bus->data = WD(0 | 2, 3);
	dato_bus(bus);
	bus->data = WD(0 | 3, 4);
	dato_bus(bus);
	vswinfo(tv);

	bus->addr = KMA;
	bus->data = 01200;
	dato_bus(bus);
	bus->addr = KMS;
	bus->data = 0140;
	dato_bus(bus);

	bus->addr = KMS;
	dati_bus(bus);
	printf("kms read: %o\n", bus->data);
	bus->addr = KMA;
	dati_bus(bus);
	printf("kma read: %o\n", bus->data);

	bus->addr = CREG;
	bus->data = WD(ALU_SET, 0);
	dato_bus(bus);

	bus->addr = 0060000 + 0037730;
	bus->data = ~0;
	dato_bus(bus);
}
#endif
