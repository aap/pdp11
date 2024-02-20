/* The 10-11 interface */
typedef struct Ten11 Ten11;
struct Ten11
{
	char *host;
	int port;;
	int cycle;
	int fd;
};

extern void initten11(Ten11 *ten11, int memsize);
extern void reset_ten11(void *dev);
extern int svc_ten11(Bus *bus, void *dev);
