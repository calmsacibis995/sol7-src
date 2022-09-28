#
#ident	"@(#)Makefile.com	1.4	98/02/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libeti/panel/Makefile.com
#
LIBRARY=	libpanel.a
VERS=		.1

OBJECTS=  \
	bottom.o \
	move.o \
	replace.o \
	update.o \
	delete.o \
	misc.o \
	new.o \
	top.o

# include library definitions
include ../../../Makefile.lib

MAPFILE=        ../common/mapfile-vers
SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS =          $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=      -u -I../inc
LINTFLAGS64=    -u -I../inc -D__sparcv9
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)

ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

STATICLIBDIR=   $(ROOTLIBDIR)
STATICLIB=      $(LIBRARY:%=$(STATICLIBDIR)/%)

DYNLINKLIBDIR=  $(ROOTLIBDIR)
DYNLINKLIB=     $(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

CLEANFILES +=   $(LINTOUT) $(LINTLIB)

CFLAGS +=       -v -I../inc
CFLAGS64 +=     -v -I../inc
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=       -lcurses -lc

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB):      $(MAPFILE)

# include library targets
include ../../../Makefile.targ

llib-lpanel: ../common/llib-lpanel.c
	$(RM) $@
	cp ../common/llib-lpanel.c $@

objs/%.o profs/%.o pics/%.o:    ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for 32-bit libpanel.a
$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)

