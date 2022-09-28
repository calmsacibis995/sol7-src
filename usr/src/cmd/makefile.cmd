#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.cmd	1.45	97/11/22 SMI"
#
# cmd/Makefile.cmd
#
# Definitions common to command source.
#
# include global definitions; SRC should be defined in the shell.
# SRC is needed until RFE 1026993 is implemented.

include $(SRC)/Makefile.master

LN=		ln
CP=		cp
SH=		sh
ECHO=		echo
MKDIR=		mkdir
TOUCH=		touch

FILEMODE=	0555
LIBFILEMODE=	0444
STATIC=		$(STATPROG:%=%.static)
XPG4=		$(XPG4PROG:%=%.xpg4)

ROOTBIN=	$(ROOT)/usr/bin
ROOTLIB=	$(ROOT)/usr/lib
ROOTSHLIB=	$(ROOT)/usr/share/lib
ROOTSBIN=	$(ROOT)/sbin
ROOTUSRSBIN=	$(ROOT)/usr/sbin
ROOTUSRSBINSTAT=$(ROOT)/usr/sbin/static
ROOTETC=	$(ROOT)/etc
ROOTCCSBIN=	$(ROOT)/usr/ccs/bin
ROOTUSRKVM=	$(ROOT)/usr/kvm
ROOTXPG4=	$(ROOT)/usr/xpg4
ROOTXPG4BIN=	$(ROOT)/usr/xpg4/bin
ROOTLOCALEDEF=	$(ROOT)/usr/lib/localedef
ROOTCHARMAP=	$(ROOTLOCALEDEF)/charmap
ROOTI18NEXT=	$(ROOTLOCALEDEF)/extensions
ROOTI18NEXT64=	$(ROOTLOCALEDEF)/extensions/$(MACH64)
ROOTBIN32=	$(ROOTBIN)/$(MACH32)
ROOTBIN64=	$(ROOTBIN)/$(MACH64)
ROOTLIB64=	$(ROOTLIB)/$(MACH64)
ROOTUSRSBIN32=	$(ROOTUSRSBIN)/$(MACH32)
ROOTUSRSBIN64=	$(ROOTUSRSBIN)/$(MACH64)

ISAEXEC=	$(ROOT)/usr/lib/isaexec

LDLIBS =	$(LDLIBS.cmd)
LDFLAGS.cmd =	$(STRIPFLAG) $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3)
LDFLAGS =	$(LDFLAGS.cmd)
$(STATIC) :=	LDFLAGS = $(LDFLAGS.cmd) -dn

LINTFLAGS=	-ax
LINTOUT=	lint.out

ROOTPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTSHFILES=	$(SHFILES:%=$(ROOTBIN)/%)
ROOTLIBPROG=	$(PROG:%=$(ROOTLIB)/%)
ROOTLIBSHFILES= $(SHFILES:%=$(ROOTLIB)/%)
ROOTSHLIBPROG=	$(PROG:%=$(ROOTSHLIB)/%)
ROOTSBINPROG=	$(PROG:%=$(ROOTSBIN)/%)
ROOTUSRSBINPROG=$(PROG:%=$(ROOTUSRSBIN)/%)
ROOTBOOTPROG=	$(STATPROG:%=$(ROOTSBINSTAT)/%)
ROOTSTATPROG=	$(STATPROG:%=$(ROOTUSRSBINSTAT)/%)
ROOTETCPROG=	$(PROG:%=$(ROOTETC)/%)
ROOTCCSBINPROG=	$(PROG:%=$(ROOTCCSBIN)/%)
ROOTUSRKVMPROG=	$(PROG:%=$(ROOTUSRKVM)/%)
ROOTXPG4PROG=	$(XPG4PROG:%=$(ROOTXPG4BIN)/%)
ROOTLOCALEPROG=	$(PROG:%=$(ROOTLOCALEDEF)/%)
ROOTPROG64=	$(PROG:%=$(ROOTBIN64)/%)
ROOTPROG32=	$(PROG:%=$(ROOTBIN32)/%)
ROOTUSRSBINPROG32=	$(PROG:%=$(ROOTUSRSBIN32)/%)
ROOTUSRSBINPROG64=	$(PROG:%=$(ROOTUSRSBIN64)/%)

$(ROOTBIN)/%: %
	$(INS.file)

$(ROOTLIB)/%: %
	$(INS.file)

$(ROOTBIN64)/%: %
	$(INS.file)

$(ROOTLIB64)/%: %
	$(INS.file)

$(ROOTBIN32)/%: %
	$(INS.file)

$(ROOTSHLIB)/%: %
	$(INS.file)

$(ROOTSBIN)/%: %
	$(INS.file)

$(ROOTUSRSBIN)/%: %
	$(INS.file)

$(ROOTUSRSBIN32)/%: %
	$(INS.file)

$(ROOTUSRSBIN64)/%: %
	$(INS.file)

$(ROOTETC)/%: %
	$(INS.file)

$(ROOTCCSBIN)/%: %
	$(INS.file)

$(ROOTUSRKVM)/%: %
	$(INS.file)

$(ROOTUSRSBINSTAT)/%: %.static
	$(INS.rename)

$(ROOTXPG4BIN)/%: %.xpg4
	$(INS.rename)

$(ROOTLOCALEDEF)/%: %
	$(INS.file)

$(ROOTCHARMAP)/%: %
	$(INS.file)

$(ROOTI18NEXT)/%: %
	$(INS.file)

$(ROOTI18NEXT64)/%: %
	$(INS.file)

# build rule for statically linked programs with single source file.
%.static: %.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

%.xpg4: %.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

# Define the majority text domain in this directory.
TEXT_DOMAIN= SUNW_OST_OSCMD	
DCMSGDOMAIN= $(MSGROOT)/LC_TIME/$(TEXT_DOMAIN)

CLOBBERFILES += $(XPG4) $(STATIC) $(DCFILE)

# This flag is being added only for SCO (x86) compatibility
i386_SPFLAG=    -D_iBCS2
sparc_SPFLAG=

iBCS2FLAG = $($(MACH)_SPFLAG)
