#
#ident	"@(#)Makefile	1.5	97/09/09 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/prun/Makefile
#

PROG = prun

include ../../Makefile.cmd
include ../Makefile.com

SUBDIRS	=	$(MACH)

$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROCBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROCBINPROG)

clean clobber lint:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../../Makefile.targ
