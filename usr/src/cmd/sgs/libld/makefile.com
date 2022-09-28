#
#ident	"@(#)Makefile.com	1.32	98/02/04 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libld.a
VERS=		.2

G_MACHOBJS=	doreloc.o
L_MACHOBJS= 	machrel.o
COMOBJS=	entry.o		files.o		globals.o	libs.o \
		order.o		outfile.o	place.o		relocate.o \
		resolve.o	sections.o	support.o	syms.o\
		update.o	util.o		version.o \
		args.o		debug.o		ldentry.o	ldglobals.o \
		ldlibs.o	ldmain.o	ldutil.o	map.o
COMOBJS64=	$(COMOBJS:%.o=%64.o)
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(G_MACHOBJS)  $(L_MACHOBJS)  $(COMOBJS)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers
ROOTLIBDIR=	$(ROOT)/usr/lib

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
DBGLIB =	-L ../../liblddbg/$(MACH)
CONVLIB =	-L ../../libconv/$(MACH)
ELFLIB =	-L ../../libelf/$(MACH)
DYNFLAGS +=	$(DBGLIB) $(CONVLIB) $(ELFLIB)

LLDLIBS=	-llddbg -lelf -ldl
ZDEFS=
LDLIBS +=	-lconv $(LLDLIBS) $(INTLLIB) -lc
LINTFLAGS +=	-L ../../liblddbg/$(MACH) -L ../../libconv/$(MACH) \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-errchk=longptr64 -D_ELF64 \
		-L ../../liblddbg/$(MACH64) -L ../../libconv/$(MACH64)
		
LINTOUT =	lint.out.1
XLINTOUT =	lint.out


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=	DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
LDDYNFLAGS =	-zdefs -zcombreloc -Wl,-Bdirect -zlazyload \
		-Wl,-M$(MAPFILE) -Yl,$(SGSPROTO)
$(DYNLIB) :=			XXXFLAGS= $(LDDYNFLAGS)
$(SGSPROTO)/$(DYNLIB) :=	XXXFLAGS= -R$(SGSPROTO) -znoversion

DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libld_msg

SRCS=		../common/llib-lld
LIBSRCS=	$(L_MACHOBJS:%.o=%.c) $(COMOBJS:%.o=../common/%.c) $(BLTDATA) \
		$(G_MACHOBJS:%.o=$(SRCBASE)/uts/$(ARCH)/krtld/%.c)

CLEANFILES +=	$(XLINTOUT) $(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

native :=	LDFLAGS= -R$(SGSPROTO)
native :=	LLDLIBS = -L$(SGSPROTO)
