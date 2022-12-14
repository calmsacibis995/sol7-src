#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All rights reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#
# Copyright (c) 1994, 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)mk.rc1.d.sh	1.16	98/02/22 SMI"
#

STARTLST="01MOUNTFSYS"

STOPLST="00ANNOUNCE 33audit 35volmgt 36utmpd 40cron 40nscd 40syslog 41autofs \
41rpc 28nfs.server"

INSDIR=${ROOT}/etc/rc1.d

if [ ! -d ${INSDIR} ] 
then 
	mkdir ${INSDIR} 
	eval ${CH}chmod 755 ${INSDIR}
	eval ${CH}chgrp sys ${INSDIR}
	eval ${CH}chown root ${INSDIR}
fi 
for f in ${STOPLST}
do 
	name=`echo $f | sed -e 's/^..//'`
	rm -f ${INSDIR}/K$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/K$f
done
for f in ${STARTLST}
do 
	name=`echo $f | sed -e 's/^..//'`
	rm -f ${INSDIR}/S$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/S$f
done
