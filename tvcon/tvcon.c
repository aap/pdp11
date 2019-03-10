#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>
#include <SDL.h>

#include "../args.h"

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#define nil NULL


#define WIDTH 576
#define HEIGHT 454

char *argv0;

int scale = 1;
int ctrlslock = 0;
int modmap = 0;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screentex;
uint8 *keystate;
uint32 fb[WIDTH*HEIGHT];
uint32 *finalfb;
uint32 fg = 0x4AFF0000; // Phosphor P39, peak at 525nm.
uint32 bg = 0x00000000;
int fd;
int backspace = 017; /* Knight key code for BS. */
uint32 userevent;
int updatebuf = 1;
int updatescreen = 1;

uint8 largebuf[64*1024];


enum {
	/* TV to 11 */
	MSG_KEYDN = 0,
	MSG_GETFB,

	/* 11 to TV */
	MSG_FB,
	MSG_WD,
	MSG_CLOSE,
};

void
panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
	SDL_Quit();
}

uint16
b2w(uint8 *b)
{
	return b[0] | b[1]<<8;
}

void
w2b(uint8 *b, uint16 w)
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

int
dial(char *host, int port)
{
	int flag;
	char portstr[32];
	int sockfd;
	struct addrinfo *result, *rp, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portstr, 32, "%d", port);
	if(getaddrinfo(host, portstr, &hints, &result)){
		perror("error: getaddrinfo");
		return -1;
	}

	for(rp = result; rp; rp = rp->ai_next){
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sockfd < 0)
			continue;
		if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) >= 0)
			goto win;
		close(sockfd);
	}
	freeaddrinfo(result);
	perror("error");
	return -1;

win:
	flag = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

	freeaddrinfo(result);
	return sockfd;
}

void
updatefb(void)
{
	int x, y;
	int i;
	uint32 c;
	int stride;
	uint32 *src, *dst;

	if(scale == 1){
		memcpy(finalfb, fb, WIDTH*HEIGHT*sizeof(uint32));
		return;
	}
	stride = WIDTH*scale;
	src = fb;
	dst = finalfb;
	for(y = 0; y < HEIGHT; y++){
		for(x = 0; x < WIDTH; x++)
			for(i = 0; i < scale; i++)
				dst[x*scale + i] = src[x];
		for(i = 1; i < scale; i++){
			memcpy(dst+stride, dst, stride*sizeof(uint32));
			dst += stride;
		}
		src += WIDTH;
		dst += stride;
	}
}

void
draw(void)
{
	if(updatebuf){
		updatebuf = 0;
		updatefb();
		SDL_UpdateTexture(screentex, nil,
				finalfb, WIDTH*scale*sizeof(uint32));
		updatescreen = 1;
	}
	if(updatescreen){
		updatescreen = 0;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, screentex, nil, nil);
		SDL_RenderPresent(renderer);
	}
}

int
writen(int fd, void *data, int n)
{
	int m;

	while(n > 0){
		m = write(fd, data, n);
		if(m == -1)
			return -1;
		data += m;
		n -= m;
	}

	return 0;
}

int
readn(int fd, void *data, int n)
{
	int m;

	while(n > 0){
		m = read(fd, data, n);
		if(m <= 0)
			return -1;
		data += m;
		n -= m;
	}

	return 0;
}

/* Map SDL scancodes to Knight keyboard codes as best we can */
int scancodemap[SDL_NUM_SCANCODES];

void
initkeymap(void)
{
	int i;
	for(i = 0; i < SDL_NUM_SCANCODES; i++)
		scancodemap[i] = -1;

	scancodemap[SDL_SCANCODE_F12] = 000;	/* BREAK */
	scancodemap[SDL_SCANCODE_F2] = 001;	/* ESC */
	scancodemap[SDL_SCANCODE_1] = 002;
	scancodemap[SDL_SCANCODE_2] = 003;
	scancodemap[SDL_SCANCODE_3] = 004;
	scancodemap[SDL_SCANCODE_4] = 005;
	scancodemap[SDL_SCANCODE_5] = 006;
	scancodemap[SDL_SCANCODE_6] = 007;
	scancodemap[SDL_SCANCODE_7] = 010;
	scancodemap[SDL_SCANCODE_8] = 011;
	scancodemap[SDL_SCANCODE_9] = 012;
	scancodemap[SDL_SCANCODE_0] = 013;
	scancodemap[SDL_SCANCODE_MINUS] = 014;	/* - = */
	scancodemap[SDL_SCANCODE_EQUALS] = 015;	/* @ ` */
	scancodemap[SDL_SCANCODE_GRAVE] = 016;	/* ^ ~ */
	scancodemap[SDL_SCANCODE_BACKSPACE] = backspace;
	scancodemap[SDL_SCANCODE_F1] = 0020;	/* CALL */

	scancodemap[SDL_SCANCODE_F3] = 0021;	/* CLEAR */
	scancodemap[SDL_SCANCODE_TAB] = 022;
	scancodemap[SDL_SCANCODE_ESCAPE] = 023;	/* ALT MODE */
	scancodemap[SDL_SCANCODE_Q] = 024;
	scancodemap[SDL_SCANCODE_W] = 025;
	scancodemap[SDL_SCANCODE_E] = 026;
	scancodemap[SDL_SCANCODE_R] = 027;
	scancodemap[SDL_SCANCODE_T] = 030;
	scancodemap[SDL_SCANCODE_Y] = 031;
	scancodemap[SDL_SCANCODE_U] = 032;
	scancodemap[SDL_SCANCODE_I] = 033;
	scancodemap[SDL_SCANCODE_O] = 034;
	scancodemap[SDL_SCANCODE_P] = 035;
	scancodemap[SDL_SCANCODE_LEFTBRACKET] = 036;
	scancodemap[SDL_SCANCODE_RIGHTBRACKET] = 037;
	scancodemap[SDL_SCANCODE_BACKSLASH] = 040;
	// / inf
	// +- delta
	// O+ gamma

	// FORM
	// VTAB
	scancodemap[SDL_SCANCODE_DELETE] = 046;	/* RUBOUT */
	scancodemap[SDL_SCANCODE_A] = 047;
	scancodemap[SDL_SCANCODE_S] = 050;
	scancodemap[SDL_SCANCODE_D] = 051;
	scancodemap[SDL_SCANCODE_F] = 052;
	scancodemap[SDL_SCANCODE_G] = 053;
	scancodemap[SDL_SCANCODE_H] = 054;
	scancodemap[SDL_SCANCODE_J] = 055;
	scancodemap[SDL_SCANCODE_K] = 056;
	scancodemap[SDL_SCANCODE_L] = 057;
	scancodemap[SDL_SCANCODE_SEMICOLON] = 060;	/* ; + */
	scancodemap[SDL_SCANCODE_APOSTROPHE] = 061;	/* : * */
	scancodemap[SDL_SCANCODE_RETURN] = 062;
	// LINE FEED
	scancodemap[SDL_SCANCODE_F3] = 064;		/* next, back */

	scancodemap[SDL_SCANCODE_Z] = 065;
	scancodemap[SDL_SCANCODE_X] = 066;
	scancodemap[SDL_SCANCODE_C] = 067;
	scancodemap[SDL_SCANCODE_V] = 070;
	scancodemap[SDL_SCANCODE_B] = 071;
	scancodemap[SDL_SCANCODE_N] = 072;
	scancodemap[SDL_SCANCODE_M] = 073;
	scancodemap[SDL_SCANCODE_COMMA] = 074;
	scancodemap[SDL_SCANCODE_PERIOD] = 075;
	scancodemap[SDL_SCANCODE_SLASH] = 076;
	scancodemap[SDL_SCANCODE_SPACE] = 077;
}

/* These bits are directly sent to the 11 */
enum {
	MOD_RSHIFT = 0100,
	MOD_LSHIFT = 0200,
	MOD_RTOP = 00400,
	MOD_LTOP = 01000,
	MOD_RCTRL = 02000,
	MOD_LCTRL = 04000,
	MOD_RMETA = 010000,
	MOD_LMETA = 020000,
	MOD_SLOCK = 040000,
};

#define MOD_SHIFT (MOD_LSHIFT | MOD_RSHIFT)
#define MOD_CTRL (MOD_LCTRL | MOD_RCTRL)

/* Map key symbols to Knight keyboard codes as best we can */
int symbolmap[128];

void
initsymbolmap(void)
{
	int i;
	for(i = 0; i < 128; i++)
		symbolmap[i] = -1;

	symbolmap[' '] = 077;
	symbolmap['!'] = 002 | MOD_LSHIFT;
	symbolmap['"'] = 003 | MOD_LSHIFT;
	symbolmap['#'] = 004 | MOD_LSHIFT;
	symbolmap['%'] = 006 | MOD_LSHIFT;
	symbolmap['$'] = 005 | MOD_LSHIFT;
	symbolmap['&'] = 007 | MOD_LSHIFT;
	symbolmap['\''] = 010 | MOD_LSHIFT;
	symbolmap['('] = 011 | MOD_LSHIFT;
	symbolmap[')'] = 012 | MOD_LSHIFT;
	symbolmap['*'] = 061 | MOD_LSHIFT;
	symbolmap['+'] = 060 | MOD_LSHIFT;
	symbolmap[','] = 074;
	symbolmap['-'] = 014;
	symbolmap['.'] = 075;
	symbolmap['/'] = 076;
	symbolmap['0'] = 013;
	symbolmap['1'] = 002;
	symbolmap['2'] = 003;
	symbolmap['3'] = 004;
	symbolmap['4'] = 005;
	symbolmap['5'] = 006;
	symbolmap['6'] = 007;
	symbolmap['7'] = 010;
	symbolmap['8'] = 011;
	symbolmap['9'] = 012;
	symbolmap[':'] = 061;
	symbolmap[';'] = 060;
	symbolmap['<'] = 074 | MOD_LSHIFT;
	symbolmap['='] = 014 | MOD_LSHIFT;
	symbolmap['>'] = 075 | MOD_LSHIFT;
	symbolmap['?'] = 076 | MOD_LSHIFT;
	symbolmap['@'] = 015;
	symbolmap['A'] = 047 | MOD_LSHIFT;
	symbolmap['B'] = 071 | MOD_LSHIFT;
	symbolmap['C'] = 067 | MOD_LSHIFT;
	symbolmap['D'] = 051 | MOD_LSHIFT;
	symbolmap['E'] = 026 | MOD_LSHIFT;
	symbolmap['F'] = 052 | MOD_LSHIFT;
	symbolmap['G'] = 053 | MOD_LSHIFT;
	symbolmap['H'] = 054 | MOD_LSHIFT;
	symbolmap['I'] = 033 | MOD_LSHIFT;
	symbolmap['J'] = 055 | MOD_LSHIFT;
	symbolmap['K'] = 056 | MOD_LSHIFT;
	symbolmap['L'] = 057 | MOD_LSHIFT;
	symbolmap['M'] = 073 | MOD_LSHIFT;
	symbolmap['N'] = 072 | MOD_LSHIFT;
	symbolmap['O'] = 034 | MOD_LSHIFT;
	symbolmap['P'] = 035 | MOD_LSHIFT;
	symbolmap['Q'] = 024 | MOD_LSHIFT;
	symbolmap['R'] = 027 | MOD_LSHIFT;
	symbolmap['S'] = 050 | MOD_LSHIFT;
	symbolmap['T'] = 030 | MOD_LSHIFT;
	symbolmap['U'] = 032 | MOD_LSHIFT;
	symbolmap['V'] = 070 | MOD_LSHIFT;
	symbolmap['W'] = 025 | MOD_LSHIFT;
	symbolmap['X'] = 066 | MOD_LSHIFT;
	symbolmap['Y'] = 031 | MOD_LSHIFT;
	symbolmap['Z'] = 065 | MOD_LSHIFT;
	symbolmap['['] = 036;
	symbolmap['\\'] = 040;
	symbolmap[']'] = 037;
	symbolmap['^'] = 016;
	symbolmap['_'] = 013 | MOD_LSHIFT;
	symbolmap['`'] = 015 | MOD_LSHIFT;
	symbolmap['a'] = 047;
	symbolmap['b'] = 071;
	symbolmap['c'] = 067;
	symbolmap['d'] = 051;
	symbolmap['e'] = 026;
	symbolmap['f'] = 052;
	symbolmap['g'] = 053;
	symbolmap['h'] = 054;
	symbolmap['i'] = 033;
	symbolmap['j'] = 055;
	symbolmap['k'] = 056;
	symbolmap['l'] = 057;
	symbolmap['m'] = 073;
	symbolmap['n'] = 072;
	symbolmap['o'] = 034;
	symbolmap['p'] = 035;
	symbolmap['q'] = 024;
	symbolmap['r'] = 027;
	symbolmap['s'] = 050;
	symbolmap['t'] = 030;
	symbolmap['u'] = 032;
	symbolmap['v'] = 070;
	symbolmap['w'] = 025;
	symbolmap['x'] = 066;
	symbolmap['y'] = 031;
	symbolmap['z'] = 065;
	symbolmap['{'] = 036 | MOD_LSHIFT;
	symbolmap['|'] = 040 | MOD_LSHIFT;
	symbolmap['}'] = 037 | MOD_LSHIFT;
	symbolmap['~'] = 016 | MOD_LSHIFT;
}

int
texty_ignore(int key)
{
	return 0;
}

int (*texty)(int) = texty_ignore;

int curmod;

void
textinput(char *text)
{
	int key;

	if (text[0] >= 128)
		return;

	key = symbolmap[text[0]];
	if(key < 0)
		return;

	// Add in modifiers except shift, which comes from the table.
	key |= curmod & ~MOD_SHIFT;

	msgheader(largebuf, MSG_KEYDN, 3);
	w2b(largebuf+3, key);
	writen(fd, largebuf, 5);
}

/* Return true if this key will come as a TextInput event.*/
int
texty_symbol(int key)
{
	// Control characters don't generate TextInput.
	if(curmod & MOD_CTRL)
        	return 0;

	// Nor do these function keys.
	switch(key){
	case SDL_SCANCODE_F1:
	case SDL_SCANCODE_F2:
	case SDL_SCANCODE_F3:
	case SDL_SCANCODE_F4:
	case SDL_SCANCODE_F5:
	case SDL_SCANCODE_F6:
	case SDL_SCANCODE_F7:
	case SDL_SCANCODE_F8:
	case SDL_SCANCODE_F9:
	case SDL_SCANCODE_F10:
	case SDL_SCANCODE_F11:
	case SDL_SCANCODE_F12:
	case SDL_SCANCODE_TAB:
	case SDL_SCANCODE_ESCAPE:
	case SDL_SCANCODE_DELETE:
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_BACKSPACE:
        	return 0;
	}

	// Plain letters, numbers, and symbols do.
	return 1;
}

void
keydown(SDL_Keysym keysym, Uint8 repeat)
{
	int key;

	if(ctrlslock && keysym.scancode == SDL_SCANCODE_CAPSLOCK)
		keysym.scancode = SDL_SCANCODE_LCTRL;

	if(modmap){
		/* Map RALT to TOP and ignore windows key */
		switch(keysym.scancode){
		case SDL_SCANCODE_LSHIFT: curmod |= MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod |= MOD_RSHIFT; break;
		case SDL_SCANCODE_LCTRL: curmod |= MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod |= MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod |= MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod |= MOD_RTOP; break;
		}
		if(keystate[SDL_SCANCODE_LGUI] || keystate[SDL_SCANCODE_RGUI])
			return;
	}else
		switch(keysym.scancode){
		case SDL_SCANCODE_LSHIFT: curmod |= MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod |= MOD_RSHIFT; break;
		case SDL_SCANCODE_LGUI: curmod |= MOD_LTOP; break;
		case SDL_SCANCODE_RGUI: curmod |= MOD_RTOP; break;
		case SDL_SCANCODE_LCTRL: curmod |= MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod |= MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod |= MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod |= MOD_RMETA; break;
		}

	if(keysym.scancode == SDL_SCANCODE_F11 && !repeat){
		uint32 f = SDL_GetWindowFlags(window) &
			SDL_WINDOW_FULLSCREEN_DESKTOP;
		SDL_SetWindowFullscreen(window,
			f ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	// Some, but not all, keys come as both KeyboardEvent and
	// TextInput. Ignore the latter kind here.
	if(texty(keysym.scancode))
		return;

	key = scancodemap[keysym.scancode];
	if(key < 0)
		return;

	key |= curmod;

	msgheader(largebuf, MSG_KEYDN, 3);
	w2b(largebuf+3, key);
	writen(fd, largebuf, 5);
//	printf("down: %o\n", key);
}

void
keyup(SDL_Keysym keysym)
{
	if(ctrlslock && keysym.scancode == SDL_SCANCODE_CAPSLOCK)
		keysym.scancode = SDL_SCANCODE_LCTRL;

	if(modmap)
		/* Map RALT to TOP and ignore windows key */
		switch(keysym.scancode){
		case SDL_SCANCODE_LSHIFT: curmod &= ~MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod &= ~MOD_RSHIFT; break;
		case SDL_SCANCODE_LCTRL: curmod &= ~MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod &= ~MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod &= ~MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod &= ~MOD_RTOP; break;
		case SDL_SCANCODE_CAPSLOCK: curmod ^= MOD_SLOCK; break;
		}
	else
		switch(keysym.scancode){
		case SDL_SCANCODE_LSHIFT: curmod &= ~MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod &= ~MOD_RSHIFT; break;
		case SDL_SCANCODE_LGUI: curmod &= ~MOD_LTOP; break;
		case SDL_SCANCODE_RGUI: curmod &= ~MOD_RTOP; break;
		case SDL_SCANCODE_LCTRL: curmod &= ~MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod &= ~MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod &= ~MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod &= ~MOD_RMETA; break;
		case SDL_SCANCODE_CAPSLOCK: curmod ^= MOD_SLOCK; break;
		}
//	printf("up: %d %o %o\n", keysym.scancode, scancodemap[keysym.scancode], curmod);
}

void
dumpbuf(uint8 *b, int n)
{
	while(n--)
		printf("%o ", *b++);
	printf("\n");
}

void
unpackfb(uint8 *src, int x, int y, int w, int h)
{
	int i, j;
	uint32 *dst;
	uint16 wd;

	dst = &fb[y*WIDTH + x];
	for(i = 0; i < h; i++){
		for(j = 0; j < w; j++){
			if(j%16 == 0){
				wd = b2w(src);
if(keystate[SDL_SCANCODE_F5] && wd != 0)
printf("%d,%d: %o\n", i, j, wd);
				src += 2;
			}
			dst[j] = wd&0100000 ? fg : bg;
			wd <<= 1;
		}
		dst += WIDTH;
	}
	updatebuf = 1;
}

void
getupdate(uint16 addr, uint16 wd)
{
	int j;
	uint32 *dst;
	dst = &fb[addr*16];
	for(j = 0; j < 16; j++){
		dst[j] = wd&0100000 ? fg : bg;
		wd <<= 1;
	}
	updatebuf = 1;
}

void
getfb(void)
{
	uint8 *b;
	int x, y, w, h;

	x = 0;
	y = 0;
	w = WIDTH;
	h = HEIGHT;

	b = largebuf;
	msgheader(b, MSG_GETFB, 9);
	b += 3;
	w2b(b, x);
	w2b(b+2, y);
	w2b(b+4, w);
	w2b(b+6, h);
	writen(fd, largebuf, 11);
}

void
getdpykbd(void)
{
	uint8 buf[2];
	if(readn(fd, buf, 2) == -1){
		fprintf(stderr, "protocol botch\n");
		return;
	}
	printf("%o %o\n", buf[0], buf[1]);
}

void*
readthread(void *arg)
{
	uint16 len;
	uint8 *b;
	uint8 type;
	int x, y, w, h;
	SDL_Event ev;

	SDL_memset(&ev, 0, sizeof(SDL_Event));
	ev.type = userevent;

	while(readn(fd, &len, 2) != -1){
		len = b2w((uint8*)&len);
		b = largebuf;
		readn(fd, b, len);
		type = *b++;
		switch(type){
		case MSG_FB:
			x = b2w(b);
			y = b2w(b+2);
			w = b2w(b+4);
			h = b2w(b+6);
			b += 8;
			unpackfb(b, x*16, y, w*16, h);
			SDL_PushEvent(&ev);
			break;

		case MSG_WD:
			getupdate(b2w(b), b2w(b+2));
			SDL_PushEvent(&ev);
			break;

		case MSG_CLOSE:
			close(fd);
			exit(0);

		default:
			fprintf(stderr, "unknown msg type %d\n", type);
		}
	}
	printf("connection hung up\n");
	exit(0);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-c bg,fg] [-2] [-B] [-C] [-p port] server\n", argv0);
	fprintf(stderr, "\t-c: set background and foreground color (in hex)\n");
	fprintf(stderr, "\t-2: scale 2x\n");
	fprintf(stderr, "\t-B: map backspace to rubout\n");
	fprintf(stderr, "\t-C: map shift lock to control\n");
	fprintf(stderr, "\t-S: map keys by according to symbols\n");
	fprintf(stderr, "\t-M: map RALT to TOP and ignore windows keys\n");
	fprintf(stderr, "\t-p: tv11 port; default 11100\n");
	fprintf(stderr, "\tserver: host running tv11\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	pthread_t th1, th2;
	SDL_Event event;
	int running;
	char *p;
	int port;
	char *host;

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_StopTextInput();

	port = 11100;
	ARGBEGIN{
	case 'p':
		port = atoi(EARGF(usage()));
		break;
	case 'c':
		p = EARGF(usage());
		bg = strtol(p, &p, 16)<<8;
		if(*p++ != ',') usage();
		fg = strtol(p, &p, 16)<<8;
		if(*p++ != '\0') usage();
		break;
	case 'B':
		/* Backspace is Rubout. */
		backspace = 046;
		break;
	case 'C':
		ctrlslock++;
		break;
	case 'S':
		initsymbolmap();
		texty = texty_symbol;
		SDL_StartTextInput();
		break;
	case 'M':
		modmap++;
		break;
	case '2':
		scale++;
		break;
	}ARGEND;

	if(argc < 1)
		usage();

	host = argv[0];

	initkeymap();

	fd = dial(host, port);

	if(SDL_CreateWindowAndRenderer(WIDTH*scale, HEIGHT*scale, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());

	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING, WIDTH*scale, HEIGHT*scale);

	keystate = SDL_GetKeyboardState(nil);

	userevent = SDL_RegisterEvents(1);

	int i;
	for(i = 0; i < WIDTH*HEIGHT; i++)
		fb[i] = bg;

	finalfb = malloc(WIDTH*scale*HEIGHT*scale*sizeof(uint32));

	getdpykbd();
	getfb();

	pthread_create(&th1, nil, readthread, nil);

	running = 1;
	while(running){
		if(SDL_WaitEvent(&event) < 0)
			panic("SDL_PullEvent() error: %s\n", SDL_GetError());
		switch(event.type){
		case SDL_MOUSEBUTTONDOWN:
			break;

		case SDL_TEXTINPUT:
			textinput(event.text.text);
			break;
		case SDL_KEYDOWN:
			keydown(event.key.keysym, event.key.repeat);
			break;
		case SDL_KEYUP:
			keyup(event.key.keysym);
			break;
		case SDL_QUIT:
			running = 0;
			break;

		case SDL_USEREVENT:
			/* framebuffer changed */
			draw();
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event){
			case SDL_WINDOWEVENT_MOVED:
			case SDL_WINDOWEVENT_ENTER:
			case SDL_WINDOWEVENT_LEAVE:
			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_FOCUS_LOST:
#ifdef SDL_WINDOWEVENT_TAKE_FOCUS
			case SDL_WINDOWEVENT_TAKE_FOCUS:
#endif
				break;
			default:
				/* redraw */
				updatescreen = 1;
				draw();
				break;
			}
			break;
		}

	}
	return 0;
}
