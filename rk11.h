typedef struct RK05 RK05;
struct RK05
{
	FILE *fp;
	int protect;

	word rkds;

	int func;
	int cyl_timer;

	int sc;	// sector counter
	int cyl;	// cylinder we're on
	int newcyl;	// cylinder we want to be on
	int surf;	// surface
	int wc;	// word counter

	word dsb;
};

typedef struct RK11 RK11;
struct RK11
{
	Bus *bus;

	word rkds;
	word rker;
	word rkcs;
	word rkwc;
	word rkba;
	word rkda;
	word rkmr;
	word rkdb;

	int state;
	int done;

	int d;	// drive selector from rkda
	RK05 drives[8];

	byte buf[512];
};
int dati_rk11(Bus *bus, void *dev);
int dato_rk11(Bus *bus, void *dev);
int datob_rk11(Bus *bus, void *dev);
int svc_rk11(Bus *bus, void *dev);
int bg_rk11(void *dev);
void reset_rk11(void *dev);

void attach_rk05(RK11 *rk, int n, char *path);
void detach_rk05(RK11 *rk, int n);
