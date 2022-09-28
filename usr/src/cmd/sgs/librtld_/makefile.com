#
# ident	"@(#)Makefile.com	1.8	98/01/06 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld_db.a
VERS=		.1

COMOBJS=	rtld_db.o	rd_elf.o
COMOBJS64=	rd_elf64.o
MACHOBJS=	rd_mach.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ) $(COMOBJS) $(MACHOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
DYNFLAGS +=	-Wl,-M$(MAPFILE)
ZDEFS=

LINTFLAGS +=	$(LDLIBS)
LINTFLAGS64 +=	-errchk=longptr64 -D_ELF64 $(LDLIBS)



BLTDEFS=	msg.h
BLTDATA=	msg.c

BLTFILES=	$(BLTDEFS) $(BLTDATA)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA)

SRCS=		$(COMOBJS:%.o=../common/%.c) $(MACHOBJS:%.o=%.c) $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
