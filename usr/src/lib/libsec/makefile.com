#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.5	97/10/27 SMI"
#
# lib/libsec/Makefile.com

LIBRARY= libsec.a
VERS= .1

OBJECTS=	\
	aclcheck.o	\
	aclmode.o	\
	aclsort.o	\
	acltext.o

# include library definitions
include ../../Makefile.lib

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

$(LINTLIB):= SRCS=../common/llib-lsec
$(LINTLIB):= LINTFLAGS=-nvx

LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	$(MAPOPTS)
LDLIBS += -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILES)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)

