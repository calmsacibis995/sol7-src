#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com 1.1	97/09/25 SMI"
#
# lib/libplot/t4014/Makefile.com

LIBRARY= lib4014.a
VERS= .1

OBJECTS=	\
	arc.o	box.o	circle.o	close.o	\
	dot.o	erase.o	label.o	\
	line.o	linemod.o	move.o	open.o	\
	point.o	space.o	subr.o

# include library definitions
include ../../../Makefile.lib

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u
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
