#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	97/05/25 SMI"
#
# lib/libsec/Makefile

include		../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# definitions for install_h target
HDRS=
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: common/%
	$(INS.file)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)
	
$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
