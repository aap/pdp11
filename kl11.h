// stolen from KD11-B for now

typedef struct KL11 KL11;
struct KL11
{
	/* keyboard */
	int rcd_busy;
	int rcd_rdr_enab;
	int rcd_int_enab;
	int rcd_int;
	int rcd_da;
	byte rcd_b;

	/* printer */
	int xmit_int_enab;
	int xmit_maint;
	int xmit_int;
	int xmit_tbmt;
	byte xmit_b;

	int ttyfd;
};
int dati_kl11(Bus *bus, void *dev);
int dato_kl11(Bus *bus, void *dev);
int datob_kl11(Bus *bus, void *dev);
int svc_kl11(Bus *bus, void *dev);
int bg_kl11(void *dev);
void reset_kl11(void *dev);
