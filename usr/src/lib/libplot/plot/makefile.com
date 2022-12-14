#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com 1.3	97/10/29 SMI"
#
# lib/libplot/plot/Makefile.com

LIBRARY= libplot.a
VERS= .1

OBJECTS=	\
	arc.o	box.o	circle.o	close.o	\
	cont.o	dot.o	erase.o		label.o	\
	line.o	linmod.o	move.o	open.o	\
	point.o	putsi.o	space.o

# include library definitions
include ../../../Makefile.lib

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lplot
$(LINTLIB):= LINTFLAGS=-nvx

LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	$(MAPOPTS)
LDLIBS += -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILES)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
