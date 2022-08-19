#include "11.h"

#define CLOCKFREQ (1000000000/60)

static struct timespec oldtime, newtime;

void
initclock(Clock *clk)
{
	clock_gettime(CLOCK_REALTIME, &newtime);
	oldtime = newtime;
}

int
handleclock(Clock *clk)
{
	struct timespec diff;
	clock_gettime(CLOCK_REALTIME, &newtime);
	diff.tv_sec = newtime.tv_sec - oldtime.tv_sec;
	diff.tv_nsec = newtime.tv_nsec - oldtime.tv_nsec;
	if(diff.tv_nsec < 0){
		diff.tv_nsec += 1000000000;
		diff.tv_sec -= 1;
	}
	if(diff.tv_nsec >= CLOCKFREQ){
		oldtime.tv_nsec += CLOCKFREQ;
		if(oldtime.tv_nsec >= 1000000000){
			oldtime.tv_nsec -= 1000000000;
			oldtime.tv_sec += 1;
		}
		return 1;
	}
	return 0;
}



int
ttyopen(Tty *tty)
{
	/* open a tty if it exists */
	tty->fd = open("/tmp/tty", O_RDWR);
//	tty->fd = open("/dev/ttyUSB0", O_RDWR);
	printf("tty connected to %d\n", tty->fd);
	return tty->fd >= 0;
}

int
ttyinput(Tty *tty, char *c)
{
	if(hasinput(tty->fd)){
		read(tty->fd, c, 1);
		return 1;
	}
	return 0;
}
