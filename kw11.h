// stolen from KD11-B for now

typedef struct KW11 KW11;
struct KW11
{
	/* line clock */
	int lc_int_enab;
	int lc_clock;
	int lc_int;
	Clock clock;
};
int dati_kw11(Bus *bus, void *dev);
int dato_kw11(Bus *bus, void *dev);
int datob_kw11(Bus *bus, void *dev);
int svc_kw11(Bus *bus, void *dev);
int bg_kw11(void *dev);
void reset_kw11(void *dev);
