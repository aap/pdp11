CFLAGS=-O3 `sdl2-config --cflags`
LDLIBS=`sdl2-config --libs` -lpthread
tvcon: tvcon.c
	cc $(CFLAGS) -o $@ $< $(LDLIBS)
tvcon.html: tvcon.c
	emcc tvcon.c -s USE_SDL=2 -lwebsocket.js -s PROXY_POSIX_SOCKETS=1 -s USE_PTHREADS=1 -s PROXY_TO_PTHREAD=1 -o tvcon.html
