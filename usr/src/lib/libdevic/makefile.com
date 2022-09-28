#
# Copyright (c) 1990-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma #ident   "@(#)Makefile.com 1.2     97/11/09 SMI"
#
# lib/libdevice/Makefile
#
LIBRARY=	libdevice.a
VERS=		.1
OBJECTS=	devctl.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../mapfile-vers
SRCS=		$(OBJECTS:%.o=../%.c)

LIBS =	$(DYNLIB) $(LINTLIB)

LDLIBS += -lc

CPPFLAGS +=	-v -D_REENTRANT
DYNFLAGS +=	-M $(MAPFILE)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I..
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES=	$(LINTOUT) $(LINTLIB)

lint: $(LINTLIB)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../%.c
	$(COMPILE.c) -o $@ $<

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
