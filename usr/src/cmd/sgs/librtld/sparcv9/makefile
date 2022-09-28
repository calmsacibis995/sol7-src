#
#ident	"@(#)Makefile	1.3	98/01/06 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/cmd/sgs/librtld/Makefile.com

.KEEP_STATE:

ROOTDYNLIB64 =	$(DYNLIB:%=$(ROOTLIBDIR64)/%)

LINTFLAGS64 +=	-D__sparcv9

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB64)

lint:		$(LINTLIB)

CPPFLAGS +=	-D_ELF64


include		$(SRC)/cmd/sgs/librtld/Makefile.targ
include		../../Makefile.sub.64

DBGLIB =	-L ../../liblddbg/$(MACH64)
ELFLIB =	-L ../../libelf/$(MACH64)


