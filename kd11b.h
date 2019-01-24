// 11/05 cpu
typedef struct KD11B KD11B;
struct KD11B
{
	word r[16];
	word b;		// B register before BUT JSRJMP
	word ba;
	word ir;
	Bus *bus;
	byte psw;
	int traps;
	int be;
	int state;

	struct {
		int (*bg)(void *dev);
		void *dev;
	} br[4];

	word sw;

	/* line clock */
	int lc_int_enab;
	int lc_clock;
	int lc_int;

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
void run(KD11B *cpu);
void reset(KD11B *cpu);
