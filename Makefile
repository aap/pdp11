CFLAGS=-fno-diagnostics-color -fno-diagnostics-show-caret -DAUTODIAG -O3

all: pdp1145 pdp1105 pdp1120

pdp1145: u_kb11a.o
	$(CC) $(CFLAGS) -o $@ $^

pdp1105: 1105.o kd11b.o eae.o mem.o util.o
	$(CC) $(CFLAGS) -o $@ $^
1105.o: 11.h kd11b.h

pdp1120: 1120.o ka11.o kw11.o kl11.o eae.o mem.o util.o
	$(CC) $(CFLAGS) -o $@ $^
1120.o: 11.h ka11.h

ka11.o: 11.h ka11.h
kd11b.o: 11.h kd11b.h
kw11.o: 11.h kw11.h
kl11.o: 11.h kl11.h
eae.o: 11.h
mem.o: 11.h
util.o: 11.h
