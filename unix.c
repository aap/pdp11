#include "11.h"

void
initclock(Clock *clk, long period)
{
	clock_gettime(CLOCK_REALTIME, &clk->newtime);
	clk->oldtime = clk->newtime;
	clk->period = period;
}

int
handleclock(Clock *clk)
{
	struct timespec diff;
	clock_gettime(CLOCK_REALTIME, &clk->newtime);
	diff.tv_sec = clk->newtime.tv_sec - clk->oldtime.tv_sec;
	diff.tv_nsec = clk->newtime.tv_nsec - clk->oldtime.tv_nsec;
	if(diff.tv_nsec < 0){
		diff.tv_nsec += 1000000000;
		diff.tv_sec -= 1;
	}
	if(diff.tv_nsec >= clk->period){
		clk->oldtime.tv_nsec += clk->period;
		if(clk->oldtime.tv_nsec >= 1000000000){
			clk->oldtime.tv_nsec -= 1000000000;
			clk->oldtime.tv_sec += 1;
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

int
timestamp(char *buffer, size_t n)
{
	struct tm *tm;
	time_t t;

	t = time(NULL);
	if (t == (time_t)-1)
		return -1;
	tm = localtime(&t);
	if (tm == NULL)
		return -1;
	if(strftime(buffer, n, "%Y%m%d%H%M%S", tm) == 0)
		return -1;
	return 0;
}
