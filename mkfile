</$objtype/mkfile
BIN=/$objtype/bin

CFLAGS=-FTVwp -DPLAN9
# -DAUTODIAG

TARG=1120 1105 1140
OFILES=\
	eae.$O\
	mem.$O\
	kl11.$O\
	kw11.$O\
	p9.$O\
	rk11.$O\
	rf11.$O\
	dc11_fake.$O

HFILES=11.h\
	ka11.h\
	kd11a.h\
	kd11b.h\
	kl11.h\
	kw11.h

UPDATE=\
	mkfile\
	$HFILES\
	${OFILES:%.$O=%.c}\

</sys/src/cmd/mkmany

$O.1140:	1140.$O kd11a.$O $OFILES $LIB
	$LD $LDFLAGS -o $target $prereq
$O.1120:	1120.$O ka11.$O $OFILES $LIB
	$LD $LDFLAGS -o $target $prereq
$O.1105:	1105.$O kd11b.$O $OFILES $LIB
	$LD $LDFLAGS -o $target $prereq
