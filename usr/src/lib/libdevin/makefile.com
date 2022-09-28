#
#pragma ident	"@(#)Makefile.com	1.4	97/10/22 SMI"
#
# Copyright (c) 1990 - 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdevinfo/Makefile
#
LIBRARY=	libdevinfo.a
VERS=		.1

OBJECTS=	devfswalk.o devfssubr.o devfsinfo.o \
		devinfo.o devinfo_prop_decode.o

# include library definitions
include ../../Makefile.lib

MAPFILES=	../mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

LIBS +=	$(DYNLIB)

LDLIBS += -lc

CPPFLAGS +=	-v
DYNFLAGS=	-h $(DYNLIB) -ztext $(MAPOPTS)

LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

.KEEP_STATE:

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../%.c
	$(COMPILE.c) -o $@ $<
