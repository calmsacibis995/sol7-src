#
#ident	"@(#)Makefile.com	1.4	98/02/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libeti/menu/Makefile.com
#
LIBRARY=	libmenu.a
VERS=		.1

OBJECTS=  \
	affect.o \
	chk.o \
	connect.o \
	curitem.o \
	driver.o \
	global.o \
	itemcount.o \
	itemopts.o \
	itemusrptr.o \
	itemvalue.o \
	link.o \
	menuback.o \
	menucursor.o \
	menufore.o \
	menuformat.o \
	menugrey.o \
	menuitems.o \
	menumark.o \
	menuopts.o \
	menupad.o \
	menuserptr.o \
	menusub.o \
	menuwin.o \
	newitem.o \
	newmenu.o \
	pattern.o \
	post.o \
	scale.o \
	show.o \
	terminit.o \
	topitem.o \
	visible.o

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

llib-lmenu: ../common/llib-lmenu.c
	$(RM) $@
	cp ../common/llib-lmenu.c $@

objs/%.o profs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for 32-bit libmenu.a
$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)

