#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#	Copyright (c) 1997 by Sun Microsystems, Inc.
#	All rights reserved.	
#
#ident	"@(#)inittab.sh	1.31	97/12/08 SMI"	SVr4.0 1.18.6.1

case "$MACH" in
sparc)
	TTYM_ARGS='-T sun -d /dev/console -l console -m ldterm,ttcompat'
	;;
i386)
	TTYM_ARGS='-T AT386 -d /dev/console -l console'
	;;

*)
	echo "$0: Error: Unknown architecture \"$MACH\"" >& 2
	exit 1
	;;
esac

echo "\
ap::sysinit:/sbin/autopush -f /etc/iu.ap
ap::sysinit:/sbin/soconfig -f /etc/sock2path
fs::sysinit:/sbin/rcS sysinit		>/dev/console 2<>/dev/console </dev/console
is:3:initdefault:
p3:s1234:powerfail:/usr/sbin/shutdown -y -i5 -g0 >/dev/console 2<>/dev/console
sS:s:wait:/sbin/rcS			>/dev/console 2<>/dev/console </dev/console
s0:0:wait:/sbin/rc0			>/dev/console 2<>/dev/console </dev/console
s1:1:respawn:/sbin/rc1			>/dev/console 2<>/dev/console </dev/console
s2:23:wait:/sbin/rc2			>/dev/console 2<>/dev/console </dev/console
s3:3:wait:/sbin/rc3			>/dev/console 2<>/dev/console </dev/console
s5:5:wait:/sbin/rc5			>/dev/console 2<>/dev/console </dev/console
s6:6:wait:/sbin/rc6			>/dev/console 2<>/dev/console </dev/console
fw:0:wait:/sbin/uadmin 2 0		>/dev/console 2<>/dev/console </dev/console
of:5:wait:/sbin/uadmin 2 6		>/dev/console 2<>/dev/console </dev/console
rb:6:wait:/sbin/uadmin 2 1		>/dev/console 2<>/dev/console </dev/console
sc:234:respawn:/usr/lib/saf/sac -t 300
co:234:respawn:/usr/lib/saf/ttymon -g -h -p \"\`uname -n\` console login: \" $TTYM_ARGS" >inittab

exit 0
