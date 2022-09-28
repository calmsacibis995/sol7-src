#
#ident	"@(#)Makefile.com	1.5	98/02/06 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libmalloc/Makefile.com
#
LIBRARY=	libmalloc.a
VERS=		.1

OBJECTS=  \
	malloc.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out
LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

DYNLINKLIBDIR=	$(ROOTLIBDIR)
DYNLINKLIB=	$(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
CPPFLAGS +=	-D_REENTRANT
CPPFLAGS64 +=	-D_REENTRANT
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

CLOBBERFILES += $(LIBRARY)
.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

#
# create message catalogue files
#
TEXT_DOMAIN= SUNW_OST_OSLIB
_msg:		$(MSGDOMAIN) catalog

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

# install rule for 32-bit libmalloc.a
$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)
