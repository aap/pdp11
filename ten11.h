/* The 10-11 interface */
typedef struct Ten11 Ten11;
struct Ten11
{
	char *host, *file;
	int port;;
	int cycle;
	int fd;
	uint32 start, length;
};

#define TEN11_WRITE	1
#define TEN11_READ	2
#define TEN11_ALIVE	3

extern void initten11(Ten11 *ten11);
extern void reset_ten11(void *dev);
extern int svc_ten11(Bus *bus, void *dev);
extern int dati_ten11(Bus *bus, void *dev);
extern int dato_ten11(Bus *bus, void *dev);
extern int datob_ten11(Bus *bus, void *dev);
