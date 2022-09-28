#
#ident	"@(#)Makefile.com	1.29	98/02/06 SMI"
#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# sgs/libelf/Makefile.com


LIBRARY=	libelf.a
VERS=		.1
M4=		m4

MACHOBJS=
COMOBJS=	ar.o		begin.o		cntl.o		cook.o \
		data.o		end.o		fill.o		flag.o \
		getarhdr.o	getarsym.o	getbase.o	getdata.o \
		getehdr.o	getident.o	getphdr.o	getscn.o \
		getshdr.o	hash.o		input.o		kind.o \
		ndxscn.o	newdata.o	newehdr.o	newphdr.o \
		newscn.o	next.o		nextscn.o	output.o \
		rand.o		rawdata.o	rawfile.o	rawput.o \
		strptr.o	update.o	error.o		gelf.o \
		clscook.o
CLASSOBJS=	clscook64.o	newehdr64.o	newphdr64.o	update64.o
BLTOBJS=	msg.o		xlate.o		xlate64.o
MISCOBJS=	String.o	args.o		demangle.o	nlist.o \
		nplist.o
MISCOBJS64=	nlist.o

OBJECTS=	$(BLTOBJS)  $(MACHOBJS)  $(COMOBJS)  $(CLASSOBJS) $(MISCOBJS)

DEMOFILES=	Makefile	README		acom.c		dcom.c \
		pcom.c		tpcom.c

include $(SRC)/lib/Makefile.lib
include $(SRC)/cmd/sgs/Makefile.com

WARLOCKFILES=	$(OBJECTS:%.o=wlocks/%.ll)

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../common $(CPPFLAGS.master) -I../../include \
			-I$(SRCBASE)/uts/$(ARCH)/sys
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

LINTFLAGS =	-nvx -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED \
		-erroff=E_BAD_PTR_CAST_ALIGN
LINTFLAGS64 =	-nvx -D__sparcv9 -errchk=longptr64 -D_ELF64 \
		-erroff=E_CAST_INT_TO_SMALL_INT
LINTOUT =	lint.out.1
XLINTOUT =	lint.out

BUILD.AR=	$(RM) $@ ; \
		$(AR) q $@ `$(LORDER) $(OBJECTS:%=$(DIR)/%)| $(TSORT)`
		$(POST_PROCESS_A)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libelf

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libelf_msg

BLTSRCS=	$(BLTOBJS:%.o=%.c)
LIBSRCS=	$(COMOBJS:%.o=../common/%.c)  $(MISCOBJS:%.o=../misc/%.c) \
		$(MACHOBJS:%.o=%.c)  $(BLTSRCS)
SRCS=		../common/llib-lelf

ROOTDEMODIR=	$(ROOT)/usr/demo/ELF
ROOTDEMOFILES=	$(DEMOFILES:%=$(ROOTDEMODIR)/%)

LIBS +=		$(DYNLIB) $(LINTLIB)

CLEANFILES +=	$(LINTOUT) $(BLTSRCS) $(BLTFILES) $(WARLOCKFILES) \
		$(XLINTOUT)

$(ROOTDEMODIR) :=	OWNER =		root
$(ROOTDEMODIR) :=	GROUP =		bin
$(ROOTDEMODIR) :=	DIRMODE =	775

.PARALLEL:	$(LIBS) $(ROOTDEMOFILES)
