#
#ident	"@(#)Makefile.com	1.2	97/09/22 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		elfdump

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJ=		elfdump.o
BLTOBJ=		msg.o

OBJS=		$(BLTOBJ) $(COMOBJ)

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
LDFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE) -R$(SGSRPATH)
LDLIBS +=	-lelf -L../../liblddbg/$(MACH) -llddbg

LINTFLAGS +=	$(LDLIBS)

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/elfdump

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n elfdump_msg

SRCS=		$(COMOBJ:%.o=../common/%.c) $(BLTDATA)

ROOTCCSBIN=	$(ROOT)/usr/ccs/bin
ROOTCCSBINPROG=	$(PROG:%=$(ROOTCCSBIN)/%)

CLEANFILES +=	$(LINTOUT) $(BLTFILES)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
