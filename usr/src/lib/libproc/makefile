#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	97/12/23 SMI"
#
# lib/libproc/Makefile

include ../../Makefile.master

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

# definitions for install_h target
HDRS=		libproc.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=common/%.check)

$(ROOTHDRS) :=  FILEMODE= 644

# install rule for install_h target
$(ROOTHDRDIR)/%: common/%
	$(INS.file)

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
