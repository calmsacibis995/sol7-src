#
#ident	"@(#)Makefile.com	1.14	98/02/04 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#

LIBRARY=	ldprof.a
VERS=		.1

include		../../../../lib/Makefile.lib
include		../../Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	profile.o
BLTOBJ=		msg.o

OBJECTS=	$(COMOBJS) $(BLTOBJ)

DYNFLAGS +=	-zcombreloc -Yl,$(SGSPROTO) -ztext
CPPFLAGS=	-I. -I../common -I../../include \
		-I../../rtld/common \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)

CFLAGS +=	-K pic

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/ldprof

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ldprof_msg

SRCS=		$(COMOBJS:%.o=../common/%.c) $(BLTDATA)
LDLIBS +=	-L../../proto/$(MACH) -lmapmalloc -lc

CLEANFILES +=	$(LINTOUT) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
