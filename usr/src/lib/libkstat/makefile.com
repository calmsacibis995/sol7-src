#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.6	97/08/28 SMI"
#
# lib/libkstat/Makefile.com

LIBRARY= libkstat.a
VERS= .1

OBJECTS= kstat.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I..
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES += 	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I..
CFLAGS64 +=	-v -I..
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/kstat.o pics/kstat.o: ../kstat.h

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
