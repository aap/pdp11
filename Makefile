CFLAGS=-Wall -Wno-parentheses -fno-diagnostics-color -fno-diagnostics-show-caret -O3
#CFLAGS+=-DAUTODIAG #-pg

all: tv11 xgp11 pdp1145 pdp1105 pdp1120 pdp1140

tv11: tv11.o tv.o ka11.o eae.o kw11.o kl11.o mem.o unix.o util.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
tv11.o: 11.h ka11.h kw11.h kl11.h tv.h

xgp11: xgp11.o xgp.o ka11.o eae.o kw11.o kl11.o mem.o unix.o util.o print.o \
	lodepng.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
xgp11.o: 11.h ka11.h kw11.h kl11.h xgp.h print.h

lodepng.o:: lodepng.h lodepng.c

print.o:: print.c print.h lodepng.h

lodepng.c: lodepng/lodepng.cpp
	cp $< $@

lodepng.h: lodepng/lodepng.h
	cp $< $@

pdp1145: u_kb11a.o
	$(CC) $(CFLAGS) -o $@ $^

pdp1105: 1105.o kd11b.o eae.o mem.o unix.o util.o
	$(CC) $(CFLAGS) -o $@ $^
1105.o: 11.h kd11b.h

pdp1120: 1120.o ka11.o eae.o kw11.o kl11.o rf11.o rk11.o mem.o unix.o util.o dc11_fake.o
	$(CC) $(CFLAGS) -o $@ $^
1120.o: 11.h ka11.h kw11.h kl11.h rf11.h rk11.h dc11_fake.h

pdp1140: 1140.o kd11a.o kw11.o kl11.o rk11.o mem.o unix.o util.o dc11_fake.o
	$(CC) $(CFLAGS) -o $@ $^
1140.o: 11.h kd11a.h kw11.h kl11.h rk11.h dc11_fake.h

ka11.o: 11.h ka11.h
kd11a.o: 11.h kd11a.h
kd11b.o: 11.h kd11b.h
kw11.o: 11.h kw11.h
kl11.o: 11.h kl11.h
rk11.o: 11.h rk11.h
rf11.o: 11.h rf11.h
dc11_fake.o: 11.h dc11_fake.h
eae.o: 11.h
mem.o: 11.h
util.o: 11.h

clean:
	rm *.o
