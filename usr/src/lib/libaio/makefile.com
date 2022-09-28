#
# Copyright (c) 1996-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.4	97/10/23 SMI"
#
# lib/libaio/Makefile.com

LIBRARY=	libaio.a
VERS=		.1

OBJECTS=	\
	aio.o	\
	posix_aio.o	\
	close.o	\
	fork.o	\
	sig.o	\
	subr.o	\
	ma.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers mapfile-vers
MAPFILE64=	mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

# Override LIBS so that only a dynamic library is built.

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-laio
$(LINTLIB):= LINTFLAGS=-nvx
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	
DYNFLAGS32 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE64)
LDLIBS +=	-lc

COMDIR=		../common

CPPFLAGS += -I. -Iinc -I.. -I$(COMDIR) $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

#
# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

