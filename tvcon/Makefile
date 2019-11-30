CFLAGS=-O3 `sdl2-config --cflags`
LDLIBS=`sdl2-config --libs` -lpthread
tvcon: tvcon.c
	cc $(CFLAGS) -o $@ $< $(LDLIBS)
