#
# Copyright (c) 1990, 1993, 1996-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)Makefile.com 1.7     97/08/28 SMI"
#
# lib/libmp/Makefile.com
#
# (libmp.so.1 is built from selected platform-specific makefiles)
#

LIBRARY=	libmp.a
VERS=		.2

OBJECTS= gcd.o madd.o mdiv.o mout.o msqrt.o mult.o pow.o util.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M$(MAPFILE)
LDLIBS +=	-lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

#
# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

