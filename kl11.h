typedef struct KL11 KL11;
struct KL11
{
	int maint;

	/* keyboard */
	int rdr_int_enab;
	int rdr_busy;
	int rdr_enab;
	int rdr_done;
	byte rdr_buf;

	/* printer */
	int pun_int_enab;
	int pun_ready;
	int pun_halt;
	byte pun_buf;


	int intr_flags;

	Tty tty;
};
int dati_kl11(Bus *bus, void *dev);
int dato_kl11(Bus *bus, void *dev);
int datob_kl11(Bus *bus, void *dev);
int svc_kl11(Bus *bus, void *dev);
int bg_kl11(void *dev);
void reset_kl11(void *dev);
