#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define nil NULL
typedef unsigned char uchar;
typedef unsigned int uint;

typedef struct Lock Lock;
struct Lock
{
	int init;
	pthread_mutex_t mutex;
};
void lock(Lock *lk);
int canlock(Lock *lk);
void unlock(Lock *lk);


typedef struct Rendez Rendez;
struct Rendez
{
	int init;
	Lock *l;
	pthread_cond_t cond;
};
void rsleep(Rendez *r);
void rwakeup(Rendez *r);
void rwakeupall(Rendez *r);


typedef struct Channel Channel;
struct Channel
{
	int bufsize;
	int elemsize;
	uchar *buf;
	int nbuf;
	int off;

	int closed;
	Lock lock;
	Rendez full, empty;
};
Channel *chancreate(int elemsize, int bufsize);
void chanclose(Channel *chan);
void chanfree(Channel *c);
int chansend(Channel *c, void *p);
int chanrecv(Channel *c, void *p);
int channbsend(Channel *c, void *p);
int channbrecv(Channel *c, void *p);

int threadmain(int argc, char *argv[]);
int threadcreate(void *(*f)(void*), void *arg);
void threadexits(void *ret);
int threadid(void);
void **threaddata(void);
void threadkill(int id);
void threadwait(int id);
