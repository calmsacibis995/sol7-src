#
#pragma ident	"@(#)Makefile.com	1.3	98/02/06 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libgen/Makefile

LIBRARY=	libgen.a
VERS=		.1

OBJECTS= \
bgets.o        bufsplit.o     copylist.o \
eaccess.o      gmatch.o       isencrypt.o    mkdirp.o       p2open.o  \
pathfind.o     reg_compile.o  reg_step.o \
rmdirp.o       strccpy.o      strecpy.o      strfind.o      strrspn.o  \
strtrns.o

# include library definitions
include ../../Makefile.lib

# 32-bit environment mapfile
MAPFILE=       ../common/mapfile-vers

SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=      -u -I..
LINTFLAGS64=    -u -I..
LINTOUT=        lint.out

LINTSRC= 	$(LINTLIB:%.ln=%)

ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

DYNLINKLIBDIR=	$(ROOTLIBDIR)
DYNLINKLIB=	$(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

CLEANFILES=     $(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I../inc
CFLAGS64 +=	-v -I../inc
CPPFLAGS +=	-D_REENTRANT -I../inc
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=	-lc

CLOBBERFILES +=	$(LIBRARY)

.KEEP_STATE:

lint:	$(LINTLIB)

$(DYNLIB):	$(MAPFILE)

# Include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for 32-bit libgen.a
$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)
