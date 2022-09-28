#
#ident	"@(#)Makefile	1.3	97/09/09 SMI"
#
# Copyright (c) 1990-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ipcs/Makefile
#

PROG=	ipcs

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
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
