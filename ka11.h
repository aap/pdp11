// 11/20 cpu - TODO
typedef struct KA11 KA11;
struct KA11
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
};
void run(KA11 *cpu);
void reset(KA11 *cpu);
