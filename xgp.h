/* The 10-11 interface */
typedef struct Ten11 Ten11;
struct Ten11
{
	int cycle;
	int fd;
};


/* The whole XGP system */
typedef struct XGP XGP;
struct XGP
{
	Clock clk;
	word xcr;
	word mar;
	word xsr;
	void (*go)(XGP *xgp, Bus *bus);
	void (*mode)(XGP *xgp, uint8 data);
	word cnt;
	uint8 bw;
	int speed;
	int dots;
	int stopping;
};
void initxgp(XGP *xgp);
void closexgp(XGP *xgp);
int dato_xgp(Bus *bus, void *dev);
int datob_xgp(Bus *bus, void *dev);
int dati_xgp(Bus *bus, void *dev);
int svc_xgp(Bus *bus, void *dev);
int bg_xgp(void *dev);
void reset_xgp(void *dev);
void acceptxgp(int fd, void *arg);
void servexgp(XGP *xgp, int port);
