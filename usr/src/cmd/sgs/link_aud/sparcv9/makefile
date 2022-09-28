#
#ident	"@(#)Makefile	1.3	98/01/06 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

$(ONLIBS) \
$(CCSLIBS):=	CFLAGS64 +=	-Kpic

$(CCSLIBS):=	CFLAGS64 += "-erroff=E_ASM_DISABLES_OPTIMIZATION"

LINTFLAGS64 +=	-D__sparcv9

.KEEP_STATE:

all:		$(OBJDIR) $(PICDIR) .WAIT $(CCSLIBS) $(CCSSCRIPTS) \
		$(ONSCRIPTS) $(ONLIBS)

install:	all $(ROOTCCSLIB64) .WAIT $(ROOTCCSLIBS64)

install_onld:	install .WAIT $(ROOTONLDLIBS64)

include		../Makefile.targ
include		../../Makefile.sub.64
