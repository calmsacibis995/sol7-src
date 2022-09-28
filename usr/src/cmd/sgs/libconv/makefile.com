#
#ident	"@(#)Makefile.com	1.10	98/01/06 SMI"
#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#

LIBRARY=	libconv.a

COMOBJS=	data.o			deftag.o \
		dl.o			dynamic.o \
		elf.o			globals.o \
		phdr.o			relocate.o \
		relocate_i386.o	\
		relocate_sparc.o	sections.o \
		segments.o		symbols.o \
		version.o

OBJECTS=	$(COMOBJS)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

PICS=		$(OBJECTS:%=pics/%)

CPPFLAGS=	-I. -I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
ARFLAGS=	r

BLTDATA=	$(COMOBJS:%.o=%_msg.h)

SRCS=		$(COMOBJS:%.o=../common/%.c)

LIBNAME=	$(LIBRARY:lib%.a=%)
LINTOUT=	lint.out
LINTLIB=	llib-l$(LIBNAME).ln
LINTFLAGS +=	$(CPPFLAGS) $(LDLIBS)

CLEANFILES +=	$(LINTOUT) $(BLTDATA)
CLOBBERFILES +=	$(LINTLIB)
