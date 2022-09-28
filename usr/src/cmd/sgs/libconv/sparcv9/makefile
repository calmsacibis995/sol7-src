#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	98/01/06 SMI"
#
# cmd/sgs/libconv/sparcv9/Makefile

include 	../Makefile.com

LINTFLAGS +=	-D__sparcv9 -errchk=longptr64 -D_ELF64


.PARALLEL:	$(PICS)
.KEEP_STATE:

install all:	$(LIBRARY) .WAIT

lint:		$(LINTLIB)

include 	../Makefile.targ
include		../../Makefile.sub.64
