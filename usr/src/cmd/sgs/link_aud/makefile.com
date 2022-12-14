#
#ident	"@(#)Makefile.com	1.12	98/02/04 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved
#


include		../../../../lib/Makefile.lib

SGSPROTO=	../../proto/$(MACH)

TRUSSLIB=	truss.so.1
TRUSSSRC=	truss.c

SYMBINDREP=	symbindrep.so.1
SYMBINDREPSRC=	symbindrep.c

PERFLIB=	perfcnt.so.1
PERFSRC=	perfcnt.c hash.c

WHOLIB=		who.so.1
WHOSRC=		who.c

BINDLIB=	bindings.so.1
BINDSRC=	bindings.c

ONSCRIPTS=	perfcnt symbindrep
ONPROGS=	dumpbind
ONLIBS=		$(SYMBINDREP) $(PERFLIB) $(BINDLIB)

USRSCRIPTS=	sotruss whocalls
CCSLIBS=	$(TRUSSLIB) $(WHOLIB)
 
PICDIR=		pics
OBJDIR=		objs

TRUSSPICS=	$(TRUSSSRC:%.c=$(PICDIR)/%.o) $(PICDIR)/env.o
PERFPICS=	$(PERFSRC:%.c=$(PICDIR)/%.o) $(PICDIR)/env.o
WHOPICS=	$(WHOSRC:%.c=$(PICDIR)/%.o) $(PICDIR)/env.o
SYMBINDREPPICS=	$(SYMBINDREPSRC:%.c=$(PICDIR)/%.o) $(PICDIR)/env.o
BINDPICS=	$(BINDSRC:%.c=$(PICDIR)/%.o) $(PICDIR)/env.o


$(TRUSSLIB):=	PICS=$(TRUSSPICS)
$(PERFLIB) :=	PICS=$(PERFPICS)
$(WHOLIB):=	PICS=$(WHOPICS)
$(SYMBINDREP):=	PICS=$(SYMBINDREPPICS)
$(BINDLIB):=	PICS=$(BINDPICS)

$(TRUSSLIB):=	LDLIBS += -lmapmalloc -lc
$(PERFLIB):=	LDLIBS += -lmapmalloc -lc
$(WHOLIB):=	LDLIBS += -lelf -lmapmalloc -ldl -lc
$(SYMBINDREP):=	LDLIBS += -lmapmalloc -lc
$(BINDLIB):=	LDLIBS += -lmapmalloc -lc

$(ROOTCCSLIB) :=	OWNER =		root
$(ROOTCCSLIB) :=	GROUP =		bin
$(ROOTCCSLIB) :=	DIRMODE =	775

CPPFLAGS=	-I../common -I. $(CPPFLAGS.master)
DYNFLAGS =	-zdefs -ztext
LDFLAGS +=	-Yl,$(SGSPROTO)
LINTFLAGS +=	$(LDLIBS) -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-errchk=longptr64 -D_ELF64 $(LDLIBS)
CLEANFILES +=	$(LINTOUT) $(OBJDIR)/* $(PICDIR)/*
CLOBBERFILES +=	$(ONSCRIPTS) $(ONPROGS) $(ONLIBS) $(CCSLIBS) $(USRSCRIPTS)

ROOTONLDLIB=		$(ROOT)/opt/SUNWonld/lib
ROOTONLDLIBS=		$(ONLIBS:%=$(ROOTONLDLIB)/%)

ROOTONLDBIN=		$(ROOT)/opt/SUNWonld/bin
ROOTONLDBINPROG=	$(ONSCRIPTS:%=$(ROOTONLDBIN)/%) \
			$(ONPROGS:%=$(ROOTONLDBIN)/%)

ROOTCCSLIB=		$(ROOT)/usr/lib/link_audit
ROOTCCSLIB64=		$(ROOT)/usr/lib/link_audit/$(MACH64)
ROOTCCSLIBS=		$(CCSLIBS:%=$(ROOTCCSLIB)/%)
ROOTCCSLIBS64=		$(CCSLIBS:%=$(ROOTCCSLIB64)/%)

ROOTUSRBIN=		$(ROOT)/usr/bin
ROOTUSRBINS=		$(USRSCRIPTS:%=$(ROOTUSRBIN)/%)

FILEMODE=	0755

.PARALLEL:	$(LIBS) $(PROGS) $(SCRIPTS) $(TRUSSPICS) $(PERFPICS) \
		$(WHOPICS) $(SYMBINDREPPICS) $(BINDPICS)
