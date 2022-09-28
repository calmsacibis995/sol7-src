#
# Copyright (c) 1990, 1993, 1996-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.20	98/02/22 SMI"
#
# lib/libkvm/Makefile.com

# We do not ship a static archive library for libkvm (see PSARC 1992/171) as part
# of any package; the $LIBRARY definition is only used to derive the .so name.

LIBRARY=	libkvm.a
VERS=		.1

OBJECTS=	\
	kvmgetcmd.o	\
	kvmgetu.o	\
	kvmnextproc.o	\
	kvmnlist.o	\
	kvmopen.o	\
	kvmrdwr.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lkvm
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
DYNFLAGS32 +=	-Wl,-f,/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 +=	-Wl,-f,/usr/platform/\$$PLATFORM/lib/$(MACH64)/$(DYNLIBPSR)
LDLIBS +=	-lelf -lc

CPPFLAGS = -D_KMEMUSER -D_LARGEFILE64_SOURCE=1
CPPFLAGS += -I.. $(CPPFLAGS.master)

#CPPFLAGS += -D_KVM_DEBUG

#
# If and when somebody gets around to messaging this, CLOBBERFILE should not
# be cleared (so that any .po file will be clobbered.
#
CLOBBERFILES +=	test $(LINTOUT)

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

test: ../common/test.c
	$(COMPILE.c) ../common/test.c
	$(LINK.c) -o $@ test.o -lkvm -lelf

#
# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# lint source file installation target

$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
