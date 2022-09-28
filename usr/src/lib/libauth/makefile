#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.15	97/03/12 SMI"
#
# lib/libauth/Makefile
#
LIBRARY= libauth.a
VERS = .1

OBJECTS= ia_framework.o

# include library definitions
include ../Makefile.lib

# library dependency
LDLIBS += -lcmd -ldl -lc

LIBS += $(DYNLIB) $(LINTLIB)

MAPFILE=	mapfile-vers

CLOBBERFILES += $(LINTOUT)

DYNFLAGS += -M $(MAPFILE)

$(LINTLIB):= SRCS=llib-lauth
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=
 
LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

all:  $(LIBS)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

# include library targets
include ../Makefile.targ

# install rule for lint library target
$(ROOTLINTDIR)/%: common/%
	$(INS.file)
