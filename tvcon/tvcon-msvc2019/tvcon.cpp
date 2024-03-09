#define SDL_beginthread BeginThreadEx
#define SDL_endthread EndThread

#include <limits.h>
#include <Windows.h>
#include <SDL.h>
#include <SDL_net.h>

#define USED(x) ((void)x)
#define SET(x) ((x)=0)

#define	ARGBEGIN	for((argv0||(argv0=*argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt;\
				char _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(*_args && (_argc = *_args++))\
				switch(_argc)
#define	ARGEND		SET(_argt);USED(_argt);USED(_argc);USED(_args);}USED(argv);USED(argc);
#define	ARGF()		(_argt=_args, _args=(char*)"",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	EARGF(x)	(_argt=_args, _args=(char*)"",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): ((x), Abort(), (char*)0)))

#define	ARGC()		_argc

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#define nil NULL
#define WIDTH 576
#define HEIGHT 454

char* argv0;

int scale = 1;
int ctrlslock = 0;
int modmap = 0;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* screentex;
const uint8* keystate;
uint32 fb[WIDTH * HEIGHT];
uint32* finalfb;
uint32 fg = 0x4AFF0000; // Phosphor P39, peak at 525nm.
uint32 bg = 0x00000000;
TCPsocket sock;
int backspace = 017; /* Knight key code for BS. */
uint32 userevent;
int updatebuf = 1;
int updatescreen = 1;
uint8 largebuf[64 * 1024];

enum {
	/* TV to 11 */
	MSG_KEYDN = 0,
	MSG_GETFB,

	/* 11 to TV */
	MSG_FB,
	MSG_WD,
	MSG_CLOSE,
};

#define CONSUME_SPACES(s, c) \
  if (endptr) *endptr = (char *)s;   \
  while (c == ' ' || c == '\t') c = *++s

#define GET_SIGN(s, c, d) \
  d = c == '-' ? -1 : 1;  \
  if (c == '-' || c == '+') c = *++s

#define GET_RADIX(s, c, r)                     \
  if (!(2 <= r && r <= 36)) {                  \
    if (c == '0') {                            \
      t |= 1;                                  \
      c = *++s;                                \
      if (c == 'x' || c == 'X') {              \
        c = *++s;                              \
        r = 16;                                \
      } else if (c == 'b' || c == 'B') {       \
        c = *++s;                              \
        r = 2;                                 \
      } else {                                 \
        r = 8;                                 \
      }                                        \
    } else {                                   \
      r = 10;                                  \
    }                                          \
  } else if (c == '0') {                       \
    t |= 1;                                    \
    c = *++s;                                  \
    if ((r == 2 && (c == 'b' || c == 'B')) ||  \
        (r == 16 && (c == 'x' || c == 'X'))) { \
      c = *++s;                                \
    }                                          \
  }

static const unsigned char kBase36[256] = {
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 0,  0,  0,  0,  0,  0,   //
		0,  11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  //
		26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 0,  0,  0,  0,  0,   //
		0,  11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  //
		26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   //
};

#if !defined(__GNUC__)
template<typename T>
struct NumericLimits {};
template <>
class NumericLimits<int> {
public:
	static constexpr int Min() noexcept { return INT_MIN; }
	static constexpr int Max() noexcept { return INT_MAX; }
};
template <>
class NumericLimits<long> {
public:
	static constexpr int Min() noexcept { return LONG_MIN; }
	static constexpr int Max() noexcept { return LONG_MAX; }
};
template <typename T>
static inline bool __builtin_mul_overflow(T x, T y, T* z) {
	*z = x * y;
	return *z / x != y;
}
template <typename T>
static inline bool __builtin_add_overflow(T x, T y, T* z) {
	*z = x + y;
	return ((x >= 0 && y > NumericLimits<T>::Max() - x) ||
					(x < 0 && y < NumericLimits<T>::Min() - x));
}
#endif

static inline int
IsDigit(int c)
{
	return '0' <= c && c <= '9';
}

static long
Strtol(const char* s, char** endptr, long base)
{
	char t = 0;
	long x = 0;
	long d, c = *s;
	CONSUME_SPACES(s, c);
	GET_SIGN(s, c, d);
	GET_RADIX(s, c, base);
	if ((c = kBase36[c & 255]) && --c < base) {
		if (!((t |= 1) & 2)) {
			do {
				if (__builtin_mul_overflow(x, base, &x) ||
					__builtin_add_overflow(x, c * d, &x)) {
					x = d > 0 ? LONG_MAX : LONG_MIN;
					//errno = ERANGE;
					t |= 2;
				}
			} while ((c = kBase36[*++s & 255]) && --c < base);
		}
	}
	if (t && endptr) *endptr = (char *)s;
	return x;
}

static int
Atoi(const char* s)
{
	int x, c, d;
	do c = *s++;
	while (c == ' ' || c == '\t');
	d = c == '-' ? -1 : 1;
	if (c == '-' || c == '+') c = *s++;
	for (x = 0; IsDigit(c); c = *s++) {
		if (__builtin_mul_overflow(x, 10, &x) ||
			  __builtin_add_overflow(x, (c - '0') * d, &x)) {
			//errno = ERANGE;
			if (d > 0) return INT_MAX;
			return INT_MIN;
		}
	}
	return x;
}

static inline int
bsr(unsigned long long x)
{
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
	return __builtin_clzll(x) ^ 63;
#else
	static const char kDebruijn[64] = {
			0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61,
			54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4,  62,
			46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
			25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5,  63,
	};
	x |= x >> 1, x |= x >> 2;
	x |= x >> 4, x |= x >> 8;
	x |= x >> 16, x |= x >> 32;
	return kDebruijn[(x * 0x03f79d71b4cb0a89) >> 58];
#endif
}

static unsigned long long
tpenc(unsigned c)
{
	static const unsigned short kTpEnc[32 - 7] = {
			1 | 0300 << 8, 1 | 0300 << 8, 1 | 0300 << 8, 1 | 0300 << 8, 2 | 0340 << 8,
			2 | 0340 << 8, 2 | 0340 << 8, 2 | 0340 << 8, 2 | 0340 << 8, 3 | 0360 << 8,
			3 | 0360 << 8, 3 | 0360 << 8, 3 | 0360 << 8, 3 | 0360 << 8, 4 | 0370 << 8,
			4 | 0370 << 8, 4 | 0370 << 8, 4 | 0370 << 8, 4 | 0370 << 8, 5 | 0374 << 8,
			5 | 0374 << 8, 5 | 0374 << 8, 5 | 0374 << 8, 5 | 0374 << 8, 5 | 0374 << 8,
	};
	int e, n;
	unsigned long long w;
	if (c < 0200) return c;
	e = kTpEnc[bsr(c) - 7];
	n = e & 0xff;
	w = 0;
	do {
		w |= 0200 | (c & 077);
		w <<= 8;
		c >>= 6;
	} while (--n);
	return c | w | e >> 8;
}

static char
IsControl(unsigned c)
{
	return c <= 0x1F || (0x7F <= c && c <= 0x9F);
}

static int
GetMonospaceCharacterWidth(unsigned c)
{
	return !IsControl(c)
		+ (c >= 0x1100 &&
			(c <= 0x115f || c == 0x2329 || c == 0x232a ||
				(c >= 0x2e80 && c <= 0xa4cf && c != 0x303f) ||
				(c >= 0xac00 && c <= 0xd7a3) ||
				(c >= 0xf900 && c <= 0xfaff) ||
				(c >= 0xfe10 && c <= 0xfe19) ||
				(c >= 0xfe30 && c <= 0xfe6f) ||
				(c >= 0xff00 && c <= 0xff60) ||
				(c >= 0xffe0 && c <= 0xffe6) ||
				(c >= 0x20000 && c <= 0x2fffd) ||
				(c >= 0x30000 && c <= 0x3fffd)));
}

static int
PrintChar(HANDLE stream, int c)
{
	size_t n;
	DWORD wrote;
	unsigned long long w;
	unsigned char buf[6];
	w = tpenc(c);
	n = 0;
	do {
		buf[n++] = (unsigned char)w;
	} while ((w >>= 8));
	WriteFile(stream, buf, n, &wrote, NULL);
	return GetMonospaceCharacterWidth(c);
}

static inline long
GetVarInt(va_list va, signed char t)
{
	if (t <= 0) return va_arg(va, int);
	return va_arg(va, long);
}

static int
PrintIndent(HANDLE fd, int n)
{
	int i;
	for (i = 0; n > 0; --n) {
		i += PrintChar(fd, ' ');
	}
	return i;
}

static int
PrintZeroes(HANDLE fd, int n)
{
	int i;
	for (i = 0; n > 0; --n) {
		i += PrintChar(fd, '0');
	}
	return i;
}

static int
PrintStr(HANDLE fd, const char* s, int cols)
{
	int n, j, k = 0, i = 0;
	n = strlen(s);
	k += PrintIndent(fd, +cols - n);
	while (i < n) k += PrintChar(fd, s[i++]);
	k += PrintIndent(fd, -cols - n);
	return k;
}

static int
PrintInt(HANDLE fd, long x, int cols, char quot, char zero, int base, bool issigned)
{
	unsigned long long y;
	char z[32];
	int i, j, k, n;
	i = j = 0;
	y = x < 0 && issigned ? -x : x;
	do {
		if (quot && j == 3) z[i++ & 31] = quot, j = 0;
		z[i++ & 31] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[y % base];
	} while (++j, (y /= base));
	k = i + (x < 0 && issigned);
	if (zero) {
		n = PrintZeroes(fd, +cols - k);
	}	else {
		n = PrintIndent(fd, +cols - k);
	}
	if (x < 0 && issigned) n += PrintChar(fd, L'-');
	while (i) n += PrintChar(fd, z[--i & 31]);
	PrintIndent(fd, -cols - n);
	return n;
}

static void
Vfprintf(HANDLE stream, const char* format, va_list va)
{
	const char* s;
	signed char type;
	char quot, ansi, gotr, pdot, zero;
	int b, c, si, prec, cols, sign;
	for (;;) {
		for (;;) {
			if (!(c = *format++ & 0377) || c == L'%') break;
			if (c >= 0300) {
				for (b = 0200; c & b; b >>= 1) {
					c ^= b;
				}
				while ((*format & 0300) == 0200) {
					c <<= 6;
					c |= *format++ & 0177;
				}
			}
		EmitFormatByte:
			PrintChar(stream, c);
		}
		if (!c) break;
		prec = 0;
		pdot = 0;
		cols = 0;
		quot = 0;
		type = 0;
		zero = 0;
		sign = 1;
		for (;;) {
			switch ((c = *format++)) {
			default:
				goto EmitFormatByte;
			case L'n':
				PrintChar(stream, '\n');
				break;
			case L'l':
				++type;
				continue;
			case L'0':
			case L'1':
			case L'2':
			case L'3':
			case L'4':
			case L'5':
			case L'6':
			case L'7':
			case L'8':
			case L'9':
				si = pdot ? prec : cols;
				si *= 10;
				si += c - '0';
				goto UpdateCols;
			case L'*':
				si = va_arg(va, int);
			UpdateCols:
				if (pdot) {
					prec = si;
				}
				else {
					if (si < 0) {
						si = -si;
						sign = -1;
					} else if (!si) {
						zero = 1;
					}
					cols = si;
				}
				continue;
			case L'-':
				sign = -1;
				continue;
			case L'.':
				pdot = 1;
				continue;
			case L'_':
			case L',':
			case L'\'':
				quot = c;
				continue;
			case L'd':
				PrintInt(stream, GetVarInt(va, type), cols * sign, quot, zero, 10, true);
				break;
			case L'u':
				PrintInt(stream, GetVarInt(va, type), cols * sign, quot, zero, 10, false);
				break;
			case L'b':
				PrintInt(stream, GetVarInt(va, type), cols * sign, quot, zero, 2, false);
				break;
			case L'o':
				PrintInt(stream, GetVarInt(va, type), cols * sign, quot, zero, 8, false);
				break;
			case L'x':
				PrintInt(stream, GetVarInt(va, type), cols * sign, quot, zero, 16, false);
				break;
			case L's':
				s = va_arg(va, const char*);
				if (!s) s = "NULL";
				PrintStr(stream, s, cols * sign);
				break;
			case L'c':
				PrintChar(stream, va_arg(va, int));
				break;
			}
			break;
		}
	}
}

static void
Printf(const char* f, ...)
{
	va_list va;
	va_start(va, f);
	Vfprintf(GetStdHandle(STD_OUTPUT_HANDLE), f, va);
	va_end(va);
}

static void
Fprintf(HANDLE fd, const char* f, ...)
{
	va_list va;
	va_start(va, f);
	Vfprintf(fd, f, va);
	va_end(va);
}

#define vfprintf Vfprintf
#define fprintf Fprintf
#define printf Printf
#define stderr GetStdHandle(STD_ERROR_HANDLE)
#define atoi Atoi
#define strtol Strtol

static void EndThread(void) {
	// xxx: should close handle
	ExitThread(0);
}

static uintptr_t BeginThreadEx(
	void* security,
	unsigned stack_size,
	unsigned(__stdcall* start_address)(void*),
	void* arglist,
	unsigned initflag,
	unsigned* thrdaddr
) {
	return (uintptr_t)CreateThread(
		(LPSECURITY_ATTRIBUTES)security, 
		stack_size, 
		(LPTHREAD_START_ROUTINE)start_address, 
		arglist, 
		initflag,
		(LPDWORD)thrdaddr);
}

static void
panic(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	for (;;) {
		ExitProcess(1);
		SDL_Quit();
	}
}

static void
Abort(void)
{
	panic("abort\n");
}

static inline uint16
b2w(uint8* b)
{
	return b[0] | b[1] << 8;
}

static inline void
w2b(uint8* b, uint16 w)
{
	b[0] = (unsigned char)w;
	b[1] = (unsigned char)(w >> 8);
}

static void
msgheader(uint8* b, uint8 type, uint16 length)
{
	w2b(b, length);
	b[2] = type;
}

static TCPsocket
dial(char* host, int port)
{
	IPaddress address;
	TCPsocket s;
	if (SDLNet_ResolveHost(&address, host, port) == -1)
		panic("Error resolving host name %s: %s\n",
			host, SDLNet_GetError());
	s = SDLNet_TCP_Open(&address);
	if (s == 0)
		panic("Error connecting to %s: %s\n",
			host, SDLNet_GetError());
	return s;
}

void
updatefb(void)
{
	int x, y;
	int i;
	int stride;
	uint32* src, * dst;
	if (scale == 1) {
		SDL_memcpy(finalfb, fb, WIDTH * HEIGHT * sizeof(uint32));
		return;
	}
	stride = WIDTH * scale;
	src = fb;
	dst = finalfb;
	for (y = 0; y < HEIGHT; y++) {
		for (x = 0; x < WIDTH; x++)
			for (i = 0; i < scale; i++)
				dst[x * scale + i] = src[x];
		for (i = 1; i < scale; i++) {
			SDL_memcpy(dst + stride, dst, stride * sizeof(uint32));
			dst += stride;
		}
		src += WIDTH;
		dst += stride;
	}
}

SDL_Rect texrect;
uint32 screenmodes[2] = { 0, SDL_WINDOW_FULLSCREEN_DESKTOP };
int fullscreen;

void
resize(void)
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	//	printf("resize %d %d\n", w, h);
	texrect.x = (w - 1024) / 2;
	texrect.y = (h - 1024) / 2;

	SDL_Event ev;
	SDL_memset(&ev, 0, sizeof(SDL_Event));
	ev.type = userevent;
	SDL_PushEvent(&ev);
}

void
stretch(SDL_Rect* r)
{
	int w, h;
	SDL_GetRendererOutputSize(renderer, &w, &h);
	if ((double)h / texrect.h < (double)w / texrect.w) {
		r->w = texrect.w * h / texrect.h;
		r->h = h;
		r->x = (w - r->w) / 2;
		r->y = 0;
	}
	else {
		r->w = w;
		r->h = texrect.h * w / texrect.w;
		r->x = 0;
		r->y = (h - r->h) / 2;
	}
}

void
draw(void)
{
	if (updatebuf) {
		updatebuf = 0;
		updatefb();
		SDL_UpdateTexture(screentex, nil,
			finalfb, WIDTH * scale * sizeof(uint32));
		updatescreen = 1;
	}
	if (updatescreen) {
		SDL_Rect screenrect;
		stretch(&screenrect);
		updatescreen = 0;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, screentex, nil, &screenrect);
		SDL_RenderPresent(renderer);
	}
}

int
writen(TCPsocket s, void* data, int n)
{
	if (SDLNet_TCP_Send(s, data, n) < n)
		return -1;
	return 0;
}

int
readn(TCPsocket s, uint8* data, int n)
{
	int m;

	/* The documentation claims a successful call to SDLNet_TCP_Recv
	 * should always return n, and that anything less is an error.
	 * This doesn't appear to be true, so this loop is necessary
	 * to collect the full buffer. */

	while (n > 0) {
		m = SDLNet_TCP_Recv(s, data, n);
		if (m <= 0)
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
	SDL_memset(scancodemap, -1, sizeof(scancodemap));
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

	scancodemap[SDL_SCANCODE_F4] = 0021;	/* CLEAR */
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

#ifdef MOD_SHIFT
#undef MOD_SHIFT
#endif

#define MOD_SHIFT (MOD_LSHIFT | MOD_RSHIFT)
#define MOD_CTRL (MOD_LCTRL | MOD_RCTRL)

/* Map key symbols to Knight keyboard codes as best we can */
int symbolmap[128];

void
initsymbolmap(void)
{
	SDL_memset(symbolmap, -1, sizeof(symbolmap));
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
textinput(char* text)
{
	int key;

	if (text[0] >= 128)
		return;

	key = symbolmap[text[0]];
	if (key < 0)
		return;

	// Add in modifiers except shift, which comes from the table.
	key |= curmod & ~MOD_SHIFT;

	msgheader(largebuf, MSG_KEYDN, 3);
	w2b(largebuf + 3, key);
	writen(sock, largebuf, 5);
}

/* Return true if this key will come as a TextInput event.*/
int
texty_symbol(int key)
{
	// Control characters don't generate TextInput.
	if (curmod & MOD_CTRL)
		return 0;

	// Nor do these function keys.
	switch (key) {
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

	if (ctrlslock && keysym.scancode == SDL_SCANCODE_CAPSLOCK)
		keysym.scancode = SDL_SCANCODE_LCTRL;

	if (keysym.scancode == SDL_SCANCODE_F8) {
		fullscreen = !fullscreen;
		SDL_SetWindowFullscreen(window, screenmodes[fullscreen]);
		resize();
	}

	if (modmap) {
		/* Map RALT to TOP and ignore windows key */
		switch (keysym.scancode) {
		case SDL_SCANCODE_LSHIFT: curmod |= MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod |= MOD_RSHIFT; break;
		case SDL_SCANCODE_LCTRL: curmod |= MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod |= MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod |= MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod |= MOD_RTOP; break;
		}
		if (keystate[SDL_SCANCODE_LGUI] || keystate[SDL_SCANCODE_RGUI])
			return;
	}
	else
		switch (keysym.scancode) {
		case SDL_SCANCODE_LSHIFT: curmod |= MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod |= MOD_RSHIFT; break;
		case SDL_SCANCODE_LGUI: curmod |= MOD_LTOP; break;
		case SDL_SCANCODE_RGUI: curmod |= MOD_RTOP; break;
		case SDL_SCANCODE_LCTRL: curmod |= MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod |= MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod |= MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod |= MOD_RMETA; break;
		}

	if (keysym.scancode == SDL_SCANCODE_F11 && !repeat) {
		uint32 f = SDL_GetWindowFlags(window) &
			SDL_WINDOW_FULLSCREEN_DESKTOP;
		SDL_SetWindowFullscreen(window,
			f ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	// Some, but not all, keys come as both KeyboardEvent and
	// TextInput. Ignore the latter kind here.
	if (texty(keysym.scancode))
		return;

	key = scancodemap[keysym.scancode];
	if (key < 0)
		return;

	key |= curmod;

	msgheader(largebuf, MSG_KEYDN, 3);
	w2b(largebuf + 3, key);
	writen(sock, largebuf, 5);
	//	printf("down: %o\n", key);
}

void
keyup(SDL_Keysym keysym)
{
	if (ctrlslock && keysym.scancode == SDL_SCANCODE_CAPSLOCK)
		keysym.scancode = SDL_SCANCODE_LCTRL;

	if (modmap)
		/* Map RALT to TOP and ignore windows key */
		switch (keysym.scancode) {
		case SDL_SCANCODE_LSHIFT: curmod &= ~MOD_LSHIFT; break;
		case SDL_SCANCODE_RSHIFT: curmod &= ~MOD_RSHIFT; break;
		case SDL_SCANCODE_LCTRL: curmod &= ~MOD_LCTRL; break;
		case SDL_SCANCODE_RCTRL: curmod &= ~MOD_RCTRL; break;
		case SDL_SCANCODE_LALT: curmod &= ~MOD_LMETA; break;
		case SDL_SCANCODE_RALT: curmod &= ~MOD_RTOP; break;
		case SDL_SCANCODE_CAPSLOCK: curmod ^= MOD_SLOCK; break;
		}
	else
		switch (keysym.scancode) {
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
dumpbuf(uint8* b, int n)
{
	while (n--)
		printf("%o ", *b++);
	printf("\n");
}

void
unpackfb(uint8* src, int x, int y, int w, int h)
{
	int i, j;
	uint32* dst;
	uint16 wd;

	dst = &fb[y * WIDTH + x];
	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			if (j % 16 == 0) {
				wd = b2w(src);
				if (keystate[SDL_SCANCODE_F5] && wd != 0)
					printf("%d,%d: %o\n", i, j, wd);
				src += 2;
			}
			dst[j] = wd & 0100000 ? fg : bg;
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
	uint32* dst;
	dst = &fb[addr * 16];
	for (j = 0; j < 16; j++) {
		dst[j] = wd & 0100000 ? fg : bg;
		wd <<= 1;
	}
	updatebuf = 1;
}

void
getfb(void)
{
	uint8* b;
	int x, y, w, h;

	x = 0;
	y = 0;
	w = WIDTH;
	h = HEIGHT;

	b = largebuf;
	msgheader(b, MSG_GETFB, 9);
	b += 3;
	w2b(b, x);
	w2b(b + 2, y);
	w2b(b + 4, w);
	w2b(b + 6, h);
	writen(sock, largebuf, 11);
}

void
getdpykbd(void)
{
	uint8 buf[2];
	if (readn(sock, buf, 2) == -1) {
		fprintf(stderr, "protocol botch\n");
		return;
	}
	printf("%o %o\n", buf[0], buf[1]);
}

int
readthread(void* arg)
{
	uint16 len;
	uint8* b;
	uint8 type;
	int x, y, w, h;
	SDL_Event ev;
	uint8 buf[2];

	SDL_memset(&ev, 0, sizeof(SDL_Event));
	ev.type = userevent;

	while (readn(sock, buf, 2) != -1) {
		len = b2w(buf);
		b = largebuf;
		readn(sock, b, len);
		type = *b++;
		switch (type) {
		case MSG_FB:
			x = b2w(b);
			y = b2w(b + 2);
			w = b2w(b + 4);
			h = b2w(b + 6);
			b += 8;
			unpackfb(b, x * 16, y, w * 16, h);
			SDL_PushEvent(&ev);
			break;

		case MSG_WD:
			getupdate(b2w(b), b2w(b + 2));
			SDL_PushEvent(&ev);
			break;

		case MSG_CLOSE:
			SDLNet_TCP_Close(sock);
			ExitProcess(0);
			break;

		default:
			fprintf(stderr, "unknown msg type %d\n", type);
		}
	}
	printf("connection hung up\n");
	ExitProcess(0);
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
	ExitProcess(0);
}

int
main(int argc, char* argv[])
{
	SDL_Thread* th1;
	SDL_Event event;
	int running;
	char* p;
	int port;
	char* host;

	//SDL_SetMainReady();
	SDL_Init(SDL_INIT_EVERYTHING);
	SDLNet_Init();
	SDL_StopTextInput();

	port = 11100;
	ARGBEGIN{
	case 'p':
		port = atoi(EARGF(usage()));
		break;
	case 'c':
		p = EARGF(usage());
		bg = strtol(p, &p, 16) << 8;
		if (*p++ != ',') usage();
		fg = strtol(p, &p, 16) << 8;
		if (*p++ != '\0') usage();
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
	if (argc < 1)
		usage();
	host = argv[0];
	initkeymap();
	sock = dial(host, port);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	if (SDL_CreateWindowAndRenderer(WIDTH * scale, HEIGHT * scale, 0, &window, &renderer) < 0)
		panic("SDL_CreateWindowAndRenderer() failed: %s\n", SDL_GetError());
	SDL_SetWindowTitle(window, "Knight TV");
	screentex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING, WIDTH * scale, HEIGHT * scale);
	texrect.x = 0;
	texrect.y = 0;
	texrect.w = WIDTH * scale;
	texrect.h = HEIGHT * scale;

	keystate = SDL_GetKeyboardState(nil);

	userevent = SDL_RegisterEvents(1);

	int i;
	for (i = 0; i < WIDTH * HEIGHT; i++)
		fb[i] = bg;

	finalfb = (uint32 *)SDL_malloc(WIDTH * scale * HEIGHT * scale * sizeof(uint32));

	getdpykbd();
	getfb();

	th1 = SDL_CreateThread(readthread, "Read thread", nil);

	running = 1;
	while (running) {
		if (SDL_WaitEvent(&event) < 0)
			panic("SDL_PullEvent() error: %s\n", SDL_GetError());
		switch (event.type) {
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
			switch (event.window.event) {
			case SDL_WINDOWEVENT_MOVED:
			case SDL_WINDOWEVENT_ENTER:
			case SDL_WINDOWEVENT_LEAVE:
			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_FOCUS_LOST:
#ifdef SDL_WINDOWEVENT_TAKE_FOCUS
			case SDL_WINDOWEVENT_TAKE_FOCUS:
#endif
				break;
			case SDL_WINDOWEVENT_RESIZED:
				texrect.x = (event.window.data1 - WIDTH * scale) / 2;
				texrect.y = (event.window.data2 - HEIGHT * scale) / 2;
				// fall through
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

#define UTF16_MASK 0xfc00
#define UTF16_MOAR 0xd800 /* 0xD800..0xDBFF */
#define UTF16_CONT 0xdc00 /* 0xDC00..0xDFFF */
#define IsSurrogate(wc)     ((0xf800 & (wc)) == 0xd800)
#define IsHighSurrogate(wc) ((UTF16_MASK & (wc)) == UTF16_MOAR)
#define IsLowSurrogate(wc)  ((UTF16_MASK & (wc)) == UTF16_CONT)
#define IsUcs2(wc)          (!IsHighSurrogate(wc) && !IsLowSurrogate(wc))
#define IsUtf16Cont(wc)     IsLowSurrogate(wc) /* TODO: DELETE */
#define MergeUtf16(hi, lo)  ((((hi)-0xD800) << 10) + ((lo)-0xDC00) + 0x10000)
#define EncodeUtf16(wc)                                       \
  (((0x0000 <= (wc) && (wc) <= 0xFFFF) ||                     \
    (0xE000 <= (wc) && (wc) <= 0xFFFF))                       \
       ? (wc)                                                 \
   : 0x10000 <= (wc) && (wc) <= 0x10FFFF                      \
       ? (((((wc)-0x10000) >> 10) + 0xD800) |                 \
          (unsigned)((((wc)-0x10000) & 1023) + 0xDC00) << 16) \
       : 0xFFFD)

struct DosArgv {
	const char16_t* s;
	char* p;
	char* pe;
	unsigned wc;
};

static unsigned DecodeDosArgv(const char16_t** s) {
	unsigned x, y;
	for (;;) {
		if (!(x = *(*s)++)) break;
		if (IsUtf16Cont(x)) continue;
		if (IsUcs2(x)) {
			return x;
		}
		else {
			if ((y = *(*s)++)) {
				return MergeUtf16(x, y);
			}
			else {
				return 0;
			}
		}
	}
	return x;
}

static void AppendDosArgv(struct DosArgv* st, wint_t wc) {
	unsigned long long w;
	w = tpenc(wc);
	do {
		if (st->p >= st->pe) break;
		*st->p++ = w & 0xff;
	} while (w >>= 8);
}

/**
 * Tokenizes and transcodes Windows NT CLI args, thus avoiding
 * CommandLineToArgv() schlepping in forty megs of dependencies.
 *
 * @param s is the command line string provided by the executive
 * @param buf is where we'll store double-NUL-terminated decoded args
 * @param size is how many bytes are available in buf
 * @param argv is where we'll store the decoded arg pointer array, which
 *     is guaranteed to be NULL-terminated if max>0
 * @param max specifies the item capacity of argv, or 0 to do scanning
 * @return number of args written, excluding the NULL-terminator; or,
 *     if the output buffer wasn't passed, or was too short, then the
 *     number of args that *would* have been written is returned; and
 *     there are currently no failure conditions that would have this
 *     return -1 since it doesn't do system calls
 * @see test/libc/dosarg_test.c
 * @see libc/runtime/ntspawn.c
 * @note kudos to Simon Tatham for figuring out quoting behavior
 */
static int GetDosArgv(const char16_t *cmdline, char* buf, unsigned size, char** argv, unsigned max) {
	bool inquote;
	struct DosArgv st;
	unsigned i, argc, slashes, quotes;
	st.s = cmdline;
	st.p = buf;
	st.pe = buf + size;
	argc = 0;
	st.wc = DecodeDosArgv(&st.s);
	while (st.wc) {
		while (st.wc && (st.wc == ' ' || st.wc == '\t')) {
			st.wc = DecodeDosArgv(&st.s);
		}
		if (!st.wc) break;
		if (++argc < max) {
			argv[argc - 1] = st.p < st.pe ? st.p : NULL;
		}
		inquote = false;
		while (st.wc) {
			if (!inquote && (st.wc == ' ' || st.wc == '\t')) break;
			if (st.wc == '"' || st.wc == '\\') {
				slashes = 0;
				quotes = 0;
				while (st.wc == '\\') st.wc = DecodeDosArgv(&st.s), slashes++;
				while (st.wc == '"') st.wc = DecodeDosArgv(&st.s), quotes++;
				if (!quotes) {
					while (slashes--) AppendDosArgv(&st, '\\');
				}
				else {
					while (slashes >= 2) AppendDosArgv(&st, '\\'), slashes -= 2;
					if (slashes) AppendDosArgv(&st, '"'), quotes--;
					if (quotes > 0) {
						if (!inquote) quotes--;
						for (i = 3; i <= quotes + 1; i += 3) AppendDosArgv(&st, '"');
						inquote = (quotes % 3 == 0);
					}
				}
			}
			else {
				AppendDosArgv(&st, st.wc);
				st.wc = DecodeDosArgv(&st.s);
			}
		}
		AppendDosArgv(&st, '\0');
	}
	AppendDosArgv(&st, '\0');
	if (size) buf[min(st.p - buf, size - 1)] = '\0';
	if (max) argv[min(argc, max - 1)] = NULL;
	return argc;
}

struct WinArgs {
	char* argv[4096];
	char* envp[4092];
	intptr_t auxv[2][2];
	char argblock[0x8000];
	char envblock[0x8000];
};

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	int rc, count;
	struct WinArgs *wa;
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	wa = (struct WinArgs *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct WinArgs));
	count = GetDosArgv((char16_t *)GetCommandLineW(), wa->argblock, sizeof(wa->argblock), wa->argv, sizeof(wa->argv) / sizeof(wa->argv[0]));
	SDL_SetMainReady();
	rc = main(count, wa->argv);
	HeapFree(GetProcessHeap(), 0, wa);
	return rc;
}
