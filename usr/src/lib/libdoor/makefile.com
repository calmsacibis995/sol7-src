#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	97/09/01 SMI"
#
# lib/libdoor/Makefile.com

LIBRARY= libdoor.a
VERS= .1

OBJECTS= door_calls.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I.. -D__sparcv9
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

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
