#
#ident	"@(#)Makefile	1.6	97/09/09 SMI"
#
# Copyright (c) 1989, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/dmesg/Makefile

PROG= dmesg

include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

ROOTSYMLINK =	$(ROOTPROG)

.KEEP_STATE:

all:	$(SUBDIRS)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@

clean clobber lint:	$(SUBDIRS)

install: $(SUBDIRS) $(ROOTSYMLINK)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTUSRSBINPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
