#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	98/02/04 SMI"

include		$(SRC)/cmd/sgs/libld/Makefile.com

VERS=		.3
MSGFLAG=	-D_ELF64

DYNFLAGS +=	$(LDDYNFLAGS) -R$(SGSRPATH64)

ROOTLIBDIR=	$(ROOT)/usr/lib/$(MACH64)
ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

package install:	all $(ROOTDYNLIB64)

include		$(SRC)/cmd/sgs/libld/Makefile.targ
include		../../Makefile.sub.64

DBGLIB =	-L ../../liblddbg/$(MACH64)
CONVLIB =	-L ../../libconv/$(MACH64)
ELFLIB =	-L ../../libelf/$(MACH64)

CPPFLAGS +=	-D_ELF64
LINTFLAGS64 +=	-D__sparcv9


# Makefile.master.64 changes this to use the ld on wyoming-tools,
# this is in order to use the proto linker instead.
BUILD.SO=	$(CC) $(CFLAGS) -Yl,$(SGSPROTO) -o $@ -G $(DYNFLAGS) \
		$(PICS) -L $(ROOTLIBDIR64) $(LDLIBS)

