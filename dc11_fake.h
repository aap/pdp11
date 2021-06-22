typedef struct DC11 DC11;
struct DC11
{
	int dummy;
};

int dati_dc11(Bus *bus, void *dev);
int dato_dc11(Bus *bus, void *dev);
int datob_dc11(Bus *bus, void *dev);
int svc_dc11(Bus *bus, void *dev);
int bg_dc11(void *dev);
void reset_dc11(void *dev);
