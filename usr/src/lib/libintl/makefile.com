#
#ident	"@(#)Makefile.com 1.1	97/11/25 SMI"
#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libintl/Makefile.com

LIBRARY=	libintl.a
VERS=		.1

ROOTARLINK=	$(ROOTLIBDIR)/$(LIBRARY)

# include library definitions
include		../../Makefile.lib

MAPFILES=	../common/mapfile-vers $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

LIBS = $(DYNLIB) $(LINTLIB)

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

$(LINTLIB):= SRCS=../common/llib-lintl
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=
 
LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

CLOBBERFILES += $(LIBS) lint.out $(ROOTARLINK) $(ROOTLIBS) $(ROOTLINKS) \
		$(ROOTLINT)

# include library targets
include ../../Makefile.targ

$(ROOTARLINK):
		-$(RM) $@; $(SYMLINK) $(LIBNULL) $@

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
