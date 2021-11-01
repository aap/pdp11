// PiDP-11/70
typedef struct Console Console;
struct Console
{
	int status_lights;
	int address_lights;
	int address_select;
	int data_lights;
	int data_select;

	int data_switches;
	int cntl_switches;
	int toggled_switches;
	int knob1, knob1state;
	int knob2, knob2state;

	Lock sw_lock;
};

enum {
	SW_LAMPTEST = 1,
	SW_LOAD_ADDR = 2,
	SW_EXAM = 4,
	SW_DEP = 010,
	SW_CONT = 020,
	SW_HALT = 040,
	SW_SINST = 0100,
	SW_START = 0200
};

typedef struct Uword Uword;
struct Uword
{
	byte clk;	// 3b type of cycle
	byte cir;	// 1b clock IR
	byte wr;		// 2b write register
	byte cb;		// 1b clock B
	byte cd;		// 1b clock D
	byte cba;	// 1b clock BA
	byte bus;	// 3b
	byte dad;	// 4b
	byte sps;	// 3b
	byte alu;	// 5b ALU cntl
	byte sbc;	// 4b B constants
	byte sbm;	// 4b B MUX
	byte sdm;	// 2b D MUX
	byte sba;	// 1b BA MUX
	byte ubf;	// 5b branch
	byte srx;	// 4b register select
	byte rif;	// 4b R immediate
	byte upf;	// 8/9b next address
	byte mm;

	char *name;
};

typedef struct IRdecode IRdecode;
struct IRdecode
{
	int flags;
	uint ctable;
	uint vtable;
	word brconst;
	byte bubc22;
	byte bubc31;
	byte bubc33[2];
	byte bubc34[2];
	byte bubc35[2];
	byte bubc36;
	byte bubc37;
	byte comux;
	byte alu1, alu2;
	byte stpm;
};

// PDP-11/40
typedef struct KD11A KD11A;
struct KD11A
{
	// console
	Console con;
	int swtch;
	int consl;
	int exam;
	int dep;
	int start;

	int reset;
	int pwrup_init;

	// status
	int berr;
	int trap;
	int intr;
	int awby;
	int brsv;
	int bovflw;
	int pwrdn;
	int stall;
	int wait;
	int ovlap;

	int ps_flags;
	int pasta, pastc;
	int ps_t;
	int ps_prio;
	int brq;

	// data
	word dr;
	int dcry;
	word br;
	word ba;
	word reg[16];
	// combinational
	int radr;
	word rd;
	word bc;
	word dmux;
	int coutmux;
	word bmux;
	word bamux;
	word alu, alu_cry;

	// timing
	int enable_mclk;
	int idle;
	int clkon;

	// IR
	word ir;
	IRdecode irdec;
	// not sure how important these are
	int ovlap_cycle;
	int ovlap_instr;

	// microcontrol
	Uword u;
	int bubc;
	int upp, pupp;
	int jberr, jpup, perr;

	// Bus
	Bus *bus;
	int busc;
	int busff;
	int ckoda, ckovf;
	int ovfl_cond;
	// ownership
	int bbsy;
	int cbr, brptr, npr;
	int nodat;
	// remember BRs
	struct {
		int (*bg)(void *dev);
		void *dev;
	} busreq[4];
	int br_highest;

	int step;

	struct timespec start_time, inst_time, bus_time;
};

void KD11A_initonce(void);
void KD11A_poweron(KD11A *cpu);
void KD11A_service(KD11A *cpu);
