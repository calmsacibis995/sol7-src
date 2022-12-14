#
#pragma ident	"@(#)Makefile.com	1.2	97/08/31 SMI"
#
# Copyright (c) 1990,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libadm/Makefile.com
#

LIBRARY=	libadm.a
VERS=		.1

OBJECTS= \
ckdate.o     ckgid.o      ckint.o      ckitem.o     ckkeywd.o    ckpath.o  \
ckrange.o    ckstr.o      cktime.o     ckuid.o      ckyorn.o     data.o  \
devattr.o    devreserv.o  devtab.o     dgrpent.o    getdev.o     getdgrp.o  \
getinput.o   getvol.o     listdev.o    listdgrp.o   memory.o     pkginfo.o  \
pkgnmchk.o   pkgparam.o   putdev.o     putdgrp.o    puterror.o   puthelp.o  \
putprmpt.o   puttext.o    rdwr_vtoc.o  regexp.o     space.o      fulldevnm.o

MAPFILE=	../common/mapfile-vers

include		../../Makefile.lib

LIBS +=		$(DYNLIB) $(LINTLIB)

CPPFLAGS +=	-I../inc
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS += -lc

CLEANFILES=	$(LINTOUT) $(LINTLIB)

LDLIBS +=       -lelf

$(LINTLIB):= SRCS=../common/llib-ladm
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

$(DYNLIB):	$(MAPFILE)

.KEEP_STATE:

lint:		$(LINTLIB)

include		../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

