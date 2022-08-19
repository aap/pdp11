#include "11.h"

void
exit(int code)
{
	if(code == 0)
		threadexitsall(nil);
	threadexitsall("botch");
}

void
threadmain(int argc, char *argv[])
{
	int ret;
	ret = xmain(argc, argv);
	exit(ret);
}

static void
clockproc(void *arg)
{
	int i;
	Channel *c;
	c = arg;
	for(i = 0; ; i++){
		/* tick at 60Hz */
		if(i == 2){
			i = 0;
			sleep(17);
		}else
			sleep(16);
		sendul(c, 1);
	}
}

void
initclock(Clock *clk)
{
	if(clk->chan == nil){
		clk->chan = chancreate(sizeof(ulong), 5);
		proccreate(clockproc, clk->chan, 1024);
	}
}

int
handleclock(Clock *clk)
{
	static int n;
	// TODO? maybe check more often?
	n = (n+1)%100;
	if(n != 0) return 0;
	return nbrecvul(clk->chan) != 0;
}



int
ttyopen(Tty *tty)
{
	int fd[2], srvfd;
	pipe(fd);
	tty->fd = fd[0];
	srvfd = create("/srv/pdp11tty", OWRITE|ORCLOSE|OCEXEC, 0600);
	if(srvfd < 0)
		return 1;
	fprint(srvfd, "%d", fd[1]);
	return 0;
}

static void
ttyreader(void *arg)
{
	Tty *tty = arg;
	char c;
	for(;;){
		read(tty->fd, &c, 1);
		send(tty->chan, &c);
	}
}

int
ttyinput(Tty *tty, char *c)
{
	if(tty->chan == nil){
		tty->chan = chancreate(sizeof(char), 0);
		proccreate(ttyreader, tty, 1024);
	}
	return nbrecv(tty->chan, c) == 1;
}
