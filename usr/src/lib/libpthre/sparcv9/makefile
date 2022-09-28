#
#ident	"@(#)Makefile 1.2	97/08/29 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libpthread.a
VERS=		.1

include		../../Makefile.lib
include		../../Makefile.lib.64

COMOBJS=	pthread.o	sys.o		thr.o
OBJECTS=	$(COMOBJS)
SRCS=		$(COMOBJS:%.o=../common/%.c)

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

# -F option normally consumed by the cc driver, so use the -W option of
# the cc driver to make sure this makes it to ld.

CFLAGS64 += -K pic
DYNFLAGS +=	-W l,-Flibthread.so.1 -zinitfirst -zloadfltr $(MAPOPTS)

ROOTLINTLIB64=    $(LINTLIB:%=$(ROOTLIBDIR64)/%)

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

$(DYNLIB):	$(MAPFILES)

pics/%.o:	../common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

.KEEP_STATE:

all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB64) $(ROOTLINKS64) $(ROOTLINTLIB64)

lint:		$(LINTLIB)

#include library targets
include		../../Makefile.targ

