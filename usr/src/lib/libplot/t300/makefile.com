#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com 1.2	97/10/29 SMI"
#
# lib/libplot/t300/Makefile.com

LIBRARY= lib300.a
VERS= .1

OBJECTS=	\
	arc.o	box.o	circle.o	close.o	\
	dot.o	erase.o	label.o	\
	line.o	linmod.o	move.o	open.o	\
	point.o	space.o	subr.o

# include library definitions
include ../../../Makefile.lib

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-l300
$(LINTLIB):= LINTFLAGS=-nvx
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	$(MAPOPTS)
LDLIBS += -lc -lm

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
