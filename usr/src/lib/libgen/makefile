#
#pragma ident	"@(#)Makefile	1.12	97/08/16 SMI"
#
# Copyright (c) 1989-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libgen/Makefile

include ../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional statements

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint:=		TARGET= lint

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# definitions for install_h target
HDRS=		regexpr.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHKHDRS=	$(HDRS:%.h=inc/%.h)
CHECKHDRS=	$(CHKHDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: inc/%
	$(INS.file)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
 
FRC:
