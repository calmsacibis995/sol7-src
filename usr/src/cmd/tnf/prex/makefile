#
# ident	"@(#)Makefile 1.22 97/09/16 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/tnf/prex/Makefile
#

PROG=  prex

include	../../Makefile.cmd

SUBDIRS = 	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:    $(SUBDIRS)

clean clobber lint: 	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)


$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include	../../Makefile.targ
