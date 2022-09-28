#
#ident	"@(#)Makefile	1.13	97/10/01 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ls/Makefile
#

PROG=	ls

include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)
# Don't install isaexec yet
#	-$(RM) $(ROOTPROG)
#	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
