#
#ident	"@(#)Makefile	1.3	98/01/06 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
include ../Makefile.com

LINTFLAGS64 +=	-D__sparcv9
CPPFLAGS +=	-D_SYSCALL32
OBJECTS +=	$(COMOBJS64)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)
install:	all $(ROOTDYNLIB64) $(ROOTLINKS64)

include	../Makefile.targ
include	../../Makefile.sub.64
