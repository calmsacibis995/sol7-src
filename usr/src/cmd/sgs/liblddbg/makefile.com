#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.23	98/02/04 SMI"
#

LIBRARY=	liblddbg.a
VERS=		.4

COMOBJS=	args.o		bindings.o	debug.o	\
		dynamic.o	entry.o		elf.o		files.o \
		libs.o		map.o		note.o		phdr.o \
		relocate.o	sections.o	segments.o	shdr.o \
		support.o	syms.o		audit.o		util.o \
		version.o	got.o
COMOBJS64=	files64.o	map64.o		relocate64.o	sections64.o \
		segments64.o	syms64.o	audit64.o	got64.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(COMOBJS)  $(COMOBJS64)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib
MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
CONVLIB		= -L../../libconv/$(MACH)
DYNFLAGS +=	$(CONVLIB)
ZDEFS=
LDLIBS +=	-lconv -lc
LINTFLAGS +=	-L ../../libconv/$(MACH) \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-errchk=longptr64 -D_ELF64 \
		-L ../../libconv/$(MACH64)
LINTOUT =	lint.out.1
XLINTOUT =	lint.out


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=  DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
$(DYNLIB) :=    XXXFLAGS= -Yl,$(SGSPROTO) -M$(MAPFILE) -zcombreloc
#$(DYNLIB) :=    XXXFLAGS= -Yl,/home/rickg/lib -M$(MAPFILE)
DYNFLAGS +=     $(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/liblddbg

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n liblddbg_msg

SRCS=		../common/llib-llddbg
LIBSRCS=	$(COMOBJS:%.o=../common/%.c)  $(BLTDATA)


CLEANFILES +=	$(XLINTOUT) $(LINTOUT) $(LINTLIB) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB)  $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
