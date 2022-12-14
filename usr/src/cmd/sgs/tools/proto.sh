#!/bin/sh
#
# Generate a proto area suitable for the current architecture ($(MACH))
# sufficient to support the sgs build.

if [ "X$CODEMGR_WS" = "X" -o "X$MACH" = "X" ] ; then
	echo "CODEMGR_WS and MACH environment variables must be set"
	exit 1
fi

dirs="$CODEMGR_WS/proto \
	$CODEMGR_WS/proto/root_$MACH \
	$CODEMGR_WS/proto/root_$MACH/usr \
	$CODEMGR_WS/proto/root_$MACH/usr/demo \
	$CODEMGR_WS/proto/root_$MACH/usr/lib \
	$CODEMGR_WS/proto/root_$MACH/usr/lib/link_audit \
	$CODEMGR_WS/proto/root_$MACH/usr/lib/pics \
	$CODEMGR_WS/proto/root_$MACH/usr/4lib \
	$CODEMGR_WS/proto/root_$MACH/usr/bin \
	$CODEMGR_WS/proto/root_$MACH/usr/ccs \
	$CODEMGR_WS/proto/root_$MACH/usr/ccs/bin \
	$CODEMGR_WS/proto/root_$MACH/usr/include \
	$CODEMGR_WS/proto/root_$MACH/usr/include/sys \
	$CODEMGR_WS/proto/root_$MACH/usr/xpg4 \
	$CODEMGR_WS/proto/root_$MACH/usr/xpg4/bin \
	$CODEMGR_WS/proto/root_$MACH/etc \
	$CODEMGR_WS/proto/root_$MACH/etc/lib \
	$CODEMGR_WS/proto/root_$MACH/opt \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/bin \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/doc \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/lib \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/lib/adb \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man/man1 \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man/man1l \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man/man3t \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man/man3l \
	$CODEMGR_WS/proto/root_$MACH/opt/SUNWonld/man/man3x"

if [ $MACH = "sparc" ]; then
	dirs="$dirs \
	$CODEMGR_WS/proto/root_$MACH/usr/lib/sparcv9 \
	$CODEMGR_WS/proto/root_$MACH/usr/lib/link_audit/sparcv9 \
	$CODEMGR_WS/proto/root_$MACH/usr/lib/pics/sparcv9
	"
	
fi

for dir in `echo $dirs`
do
	if [ ! -d $dir ] ; then
		echo $dir
		mkdir $dir
		chmod 777 $dir
	fi
done

# We need a local copy of libc_pic.a (we should get this from the parent
# workspace, but as we can't be sure how the proto area is constructed there
# simply take it from the base OS).

if [ ! -f $CODEMGR_WS/proto/root_$MACH/usr/lib/pics/libc_pic.a ] ; then
	echo "cp /usr/lib/pics/libc_pic.a $CODEMGR_WS/proto/root_$MACH/usr/lib/pics"
	cp /usr/lib/pics/libc_pic.a $CODEMGR_WS/proto/root_$MACH/usr/lib/pics
fi

if [ $MACH = "sparc" -a ! -f $CODEMGR_WS/proto/root_$MACH/usr/lib/pics/sparcv9/libc_pic.a ]; then
	echo "cp /usr/lib/pics/sparcv9/libc_pic.a $CODEMGR_WS/proto/root_$MACH/usr/lib/pics/sparcv9"
	cp /usr/lib/pics/sparcv9/libc_pic.a $CODEMGR_WS/proto/root_$MACH/usr/lib/pics/sparcv9
fi
