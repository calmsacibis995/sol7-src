#
#ident	"@(#)Makefile.com	1.12	98/01/06 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld.a
VERS=		.1

MACHOBJS=	_relocate.o
COMOBJS=	dldump.o	dynamic.o	relocate.o	syms.o \
		util.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(MACHOBJS)  $(COMOBJS)


include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

ROOTLIBDIR=	$(ROOT)/usr/lib

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		-I$(SRCBASE)/uts/common/krtld \
		$(CPPFLAGS.master)
DBGLIB =	-L ../../liblddbg/$(MACH)
ELFLIB =	-L ../../libelf/$(MACH)
DYNFLAGS +=	$(DBGLIB) $(ELFLIB)
ZDEFS=
LDLIBS +=	-lelf -lc
LINTFLAGS +=	-L ../../liblddbg/$(MACH) $(LDLIBS) \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-errchk=longptr64 -D_ELF64 -L ../../liblddbg/$(MACH64) $(LDLIBS)

# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=  DYNFLAGS += -Yl,$(SGSPROTO) -M $(MAPFILE)
#
XXXFLAGS=
$(DYNLIB) :=	XXXFLAGS= -Yl,$(SGSPROTO) -M $(MAPFILE)
DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/librtld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n librtld_msg

SRCS=		$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c)  $(BLTDATA)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
