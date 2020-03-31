// 11/40 cpu - TODO
typedef struct KD11A KD11A;
struct KD11A
{
	word r[16];
	word b, d;
	word ba;
	word ir;
	Bus *bus;
	byte psw;
	int traps;
	int flags;
	int state;

	struct {
		int (*bg)(void *dev);
		void *dev;
	} br[4];

	word sw;

	// KJ11
	word slr;

	// KT11
	word mode, cur, prev, temp;
	word par[16];
	int acf[16];
	int plf[16];
	int *wrpg;
	word sr0, sr2;
	int relocate;
	int mfp, mtp;

	int n;
	int newpsw, newmask;	// flag update
};
void run(KD11A *cpu);
void reset(KD11A *cpu);
