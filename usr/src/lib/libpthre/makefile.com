#
#ident	" @(#)Makefile.com 1.8 97/01/13 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libpthread.a
VERS=		.1

include		../../Makefile.lib

COMOBJS=	pthread.o	sys.o		thr.o
OBJECTS=	$(COMOBJS)
SRCS=		$(COMOBJS:%.o=../common/%.c)

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

# -F option normally consumed by the cc driver, so use the -W option of
# the cc driver to make sure this makes it to ld.

CFLAGS +=	-K pic
DYNFLAGS +=	-W l,-Flibthread.so.1 -zinitfirst -zloadfltr $(MAPOPTS)

ROOTLINTLIB=    $(LINTLIB:%=$(ROOTLIBDIR)/%)

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

$(DYNLIB):	$(MAPFILES)

pics/%.o:	../common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

.KEEP_STATE:

all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS) $(ROOTLINTLIB)

lint:		$(LINTLIB)

#include library targets
include		../../Makefile.targ
