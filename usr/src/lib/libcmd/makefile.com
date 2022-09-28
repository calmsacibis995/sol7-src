#
#ident	"@(#)Makefile.com	1.4	97/08/28 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
#
# lib/libcmd/Makefile.com
#
LIBRARY=	libcmd.a
VERS=		.1

OBJECTS=  \
	deflt.o \
	getterm.o \
	magic.o \
	sum.o

# include library definitions
include ../../Makefile.lib

MAPFILE=        ../common/mapfile-vers
SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS +=          $(DYNLIB) $(LINTLIB)

# definitions for lint


LINTFLAGS=      -u -I../inc
LINTFLAGS64=    -u -I../inc
LINTOUT=        lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=   $(LINTOUT) $(LINTLIB)

CFLAGS +=       -v -I../inc
CFLAGS64 +=     -v -I../inc
CPPFLAGS +=       -D_REENTRANT
CPPFLAGS64 +=     -D_REENTRANT
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=       -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):      $(MAPFILE)

#
# create message catalogue files
#
TEXT_DOMAIN= SUNW_OST_OSLIB
_msg: $(MSGDOMAIN) catalog

catalog: 
	sh ../makelibcmdcatalog.sh $(MSGDOMAIN)

$(MSGDOMAIN):
	$(INS.dir)


# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
