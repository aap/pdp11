typedef struct RS11 RS11;
struct RS11
{
	FILE *fp;

	word ads;
};

typedef struct RF11 RF11;
struct RF11
{
	Bus *bus;

	word dcs;
	word wc;
	word cma;
	word dar;
	word dae;
	word dbr;
	word ma;

	int done;

	int d;
	RS11 drives[8];

	int dirty;
	int track;	// in buffer
	byte buf[2048*2];
};
int dati_rf11(Bus *bus, void *dev);
int dato_rf11(Bus *bus, void *dev);
int datob_rf11(Bus *bus, void *dev);
int svc_rf11(Bus *bus, void *dev);
int bg_rf11(void *dev);
void reset_rf11(void *dev);

void attach_rs11(RF11 *rf, int n, char *path);
void detach_rs11(RF11 *rf, int n);
