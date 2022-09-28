#
#ident	"@(#)Makefile.com	1.24	98/01/21 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		ld

include 	$(SRC)/cmd/Makefile.cmd
include 	$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	ld.o
BLTOBJ=		msg.o

OBJS =		$(BLTOBJ) $(MACHOBJS) $(COMOBJS)
.PARALLEL:	$(OBJS)

MAPFILE=	../common/mapfile-vers


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6
INTLLIB=
package :=	INTLLIB = /usr/lib/libintl.so.1


CPPFLAGS=	-I. -I../common -I../../include \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		-I../../include/$(MACH) \
		$(CPPFLAGS.master)
LDFLAGS +=	-zlazyload -Wl,-Bdirect -Yl,$(SGSPROTO) -M $(MAPFILE) -R$(SGSRPATH)
LLDLIBS=	-L ../../libld/$(MACH) -lelf -ldl \
		-L ../../liblddbg/$(MACH) -llddbg $(INTLLIB)
LDLIBS +=	$(LLDLIBS)
LINTFLAGS +=	$(LDLIBS)
CLEANFILES +=	$(LINTOUT)

native :=	LDFLAGS = -R$(SGSPROTO) -znoversion
native :=	LLDLIBS = -L$(SGSPROTO) -lelf -ldl -llddbg \
			/usr/lib/libintl.so.1
BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/ld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ld_msg

SRCS=		$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c)  $(BLTDATA)

ROOTCCSBIN=	$(ROOT)/usr/ccs/bin
ROOTCCSBINPROG=	$(PROG:%=$(ROOTCCSBIN)/%)

CLEANFILES +=	$(BLTFILES)

FILEMODE=	0755
