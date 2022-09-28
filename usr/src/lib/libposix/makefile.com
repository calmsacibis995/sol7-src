#
# Copyright (c) 1990, 1993, 1996-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.15	97/05/24 SMI"
#
# lib/libposix4/Makefile.com

LIBRARY=	libposix4.a
VERS=		.1

OBJECTS=	\
	aio.o		\
	clock_timer.o	\
	fdatasync.o	\
	mqueue.o	\
	pos4.o		\
	pos4obj.o	\
	sched.o		\
	sem.o		\
	shm.o		\
	sigrt.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

# Override LIBS so that only a dynamic library is built.

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	
DYNFLAGS32 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE)
LDLIBS +=	 -laio -lc

CPPFLAGS += -I../inc -I../../libc/inc $(CPPFLAGS.master) -D_REENTRANT
CPPFLAGS64 += -I../inc -I../../libc/inc $(CPPFLAGS.master) -D_REENTRANT


#
# If and when somebody gets around to messaging this, CLOBBERFILE should not
# be cleared (so that any .po file will be clobbered.
#
CLOBBERFILES=	test

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/% ../common/llib-lposix4
	$(INS.file)
