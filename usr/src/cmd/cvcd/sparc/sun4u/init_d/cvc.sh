#! /usr/bin/sh
#
# ident	"@(#)cvc.sh	1.13	98/01/15 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# Startup script for Network Console
#

case "$1" in

'start')
	if [ `uname -i` = "SUNW,Ultra-Enterprise-10000" -a \
	     -x /platform/SUNW,Ultra-Enterprise-10000/lib/cvcd ]
	then
		/platform/SUNW,Ultra-Enterprise-10000/lib/cvcd
	fi
	;;

'stop')
	echo "stopping Network Console"
	pidlist=`/usr/bin/ps -f -u 0`
	CVC_PID=`echo "$pidlist" | grep cvcd | awk '{print $2}'`
	if [ -n "$CVC_PID" ]
	then
		/usr/bin/kill -KILL ${CVC_PID}
	fi
	;;

*)
	echo "Usage: $0 { start | stop }"
	;;
esac

exit 0
