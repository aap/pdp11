#include "threading.h"
#include <assert.h>

#define USED(x) (void)(x)

/*
 * Locks
 */

static pthread_mutex_t initmutex = PTHREAD_MUTEX_INITIALIZER;

static void
lockinit(Lock *lk)
{
	pthread_mutexattr_t attr;

	pthread_mutex_lock(&initmutex);
	if(lk->init == 0){
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&lk->mutex, &attr);
		pthread_mutexattr_destroy(&attr);
		lk->init = 1;
	}
	pthread_mutex_unlock(&initmutex);
}

void
lock(Lock *lk)
{
	if(!lk->init)
		lockinit(lk);
	if(pthread_mutex_lock(&lk->mutex) != 0)
		abort();
}

int
canlock(Lock *lk)
{
	int r;

	if(!lk->init)
		lockinit(lk);
	r = pthread_mutex_trylock(&lk->mutex);
	if(r == 0)
		return 1;
	if(r == EBUSY)
		return 0;
	abort();
}

void
unlock(Lock *lk)
{
	if(pthread_mutex_unlock(&lk->mutex) != 0)
		abort();
}



static void
rendinit(Rendez *r)
{
	pthread_condattr_t attr;

	pthread_mutex_lock(&initmutex);
	if(r->init == 0){
		pthread_condattr_init(&attr);
		pthread_cond_init(&r->cond, &attr);
		pthread_condattr_destroy(&attr);
		r->init = 1;
	}
	pthread_mutex_unlock(&initmutex);
}

void
rsleep(Rendez *r)
{
	if(!r->init)
		rendinit(r);
	if(pthread_cond_wait(&r->cond, &r->l->mutex) != 0)
		abort();
}

void
rwakeup(Rendez *r)
{
	if(!r->init)
		return;
	if(pthread_cond_signal(&r->cond) != 0)
		abort();
}

void
rwakeupall(Rendez *r)
{
	if(!r->init)
		return;
	if(pthread_cond_broadcast(&r->cond) != 0)
		abort();
}


/*
 * Threads
 */


static Lock idlock;
static pthread_t *threads;
static int numthreads;
static int maxthreads;

static __thread int myid;
static __thread void *mypointer;

static int
addthreadid(void)
{
	int id;

	if(numthreads >= maxthreads){
		maxthreads += 64;
		threads = realloc(threads, maxthreads*sizeof(*threads));
	}
	id = numthreads++;
	threads[id] = 0;
	return id;
}

static pthread_t
gethandle(int id)
{
	pthread_t th;
	lock(&idlock);
	th = threads[id];
	unlock(&idlock);
	return th;
}

struct ThreadArg
{
	int id;
	void *(*f)(void*);
	void *arg;
};

static void*
trampoline(void *p)
{
	struct ThreadArg *args;
	void *(*f)(void*);
	void *arg;

	args = p;
	myid = args->id;
	f = args->f;
	arg = args->arg;
	free(args);
	return f(arg);
}

int
threadcreate(void *(*f)(void*), void *arg)
{
	struct ThreadArg *args;
	int id;

	lock(&idlock);
	id = addthreadid();
	args = malloc(sizeof(*args));
	args->id = id;
	args->f = f;
	args->arg = arg;
	pthread_create(&threads[id], nil, trampoline, args);
	unlock(&idlock);
	return id;
}

void
threadexits(void *ret)
{
	pthread_exit(ret);
}

int
threadid(void)
{
	return myid;
}

void**
threaddata(void)
{
	return &mypointer;
}

void
threadkill(int id)
{
	assert(id >= 0 && id < numthreads);
	pthread_cancel(gethandle(id));
}

void
threadwait(int id)
{
	pthread_join(gethandle(id), nil);
}

int
main(int argc, char *argv[])
{
	int id;

	id = addthreadid();
	threads[id] = pthread_self();
	return threadmain(argc, argv);
}


/*
 * Channels
 */


Channel*
chancreate(int elemsize, int bufsize)
{
	Channel *c;

	c = malloc(sizeof(Channel) + bufsize*elemsize);
	if(c == nil)
		return nil;
	memset(c, 0, sizeof(Channel));
	c->elemsize = elemsize;
	c->bufsize = bufsize;
	c->nbuf = 0;
	c->buf = (uchar*)(c+1);
	c->full.l = &c->lock;
	c->empty.l = &c->lock;
	return c;
}

void
chanclose(Channel *c)
{
	lock(&c->lock);
	c->closed = 1;
	rwakeupall(&c->full);
	rwakeupall(&c->empty);
	unlock(&c->lock);
}

void
chanfree(Channel *c)
{
	// TODO: don't free chans still in use
	free(c);
}

static int cansend(Channel *c) { return c->nbuf < c->bufsize; }
static int canrecv(Channel *c) { return c->nbuf > 0; }

static void
chansend_(Channel *c, void *p)
{
	uchar *pp;

	assert(cansend(c));
	pp = c->buf + (c->off+c->nbuf)%c->bufsize * c->elemsize;
	memmove(pp, p, c->elemsize);
	c->nbuf++;
}

static void
chanrecv_(Channel *c, void *p)
{
	uchar *pp;

	assert(canrecv(c));
	pp = c->buf + c->off*c->elemsize;
	memmove(p, pp, c->elemsize);
	c->nbuf--;
	if(++c->off == c->bufsize)
		c->off = 0;
}

int
chansend(Channel *c, void *p)
{
	lock(&c->lock);
	while(!(c->closed || cansend(c)))
		rsleep(&c->full);
	/* closed or can send */
	if(c->closed){
		/* can never send to closed chan */
		unlock(&c->lock);
		return -1;
	}
	chansend_(c, p);
	rwakeup(&c->empty);
	unlock(&c->lock);
	return 1;
}

int
chanrecv(Channel *c, void *p)
{
	lock(&c->lock);
	while(!(c->closed || canrecv(c)))
		rsleep(&c->empty);
	/* closed or can receive */
	if(canrecv(c)){
		/* can still receive from closed chan */
		chanrecv_(c, p);
		rwakeup(&c->full);
		unlock(&c->lock);
		return 1;
	}
	unlock(&c->lock);
	return -1;
}

int
channbsend(Channel *c, void *p)
{
	lock(&c->lock);
	if(c->closed){
		/* can never send to closed chan */
		unlock(&c->lock);
		return -1;
	}
	if(cansend(c)){
		chansend_(c, p);
		rwakeup(&c->empty);
		unlock(&c->lock);
		return 1;
	}
	unlock(&c->lock);
	return 0;
}

int
channbrecv(Channel *c, void *p)
{
	lock(&c->lock);
	if(canrecv(c)){
		/* can still receive from closed chan */
		chanrecv_(c, p);
		rwakeup(&c->full);
		unlock(&c->lock);
		return 1;
	}
	if(c->closed){
		unlock(&c->lock);
		return -1;
	}
	unlock(&c->lock);
	return 0;
}
