#
# Copyright (c) 1994-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	97/08/20 SMI"
#
# lib/liblm/Makefile.com

LIBRARY= liblm.a
VERS= .1

OBJECTS= lm_shutdown.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS = $(DYNLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

.KEEP_STATE:

all: $(LIBS)

lint:	$(LINTLIB)

$(DYNLIB):	$(MAPFILE)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
