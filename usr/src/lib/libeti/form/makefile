#
#ident	"@(#)Makefile	1.9	97/09/11 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
#
# lib/libeti/form/Makefile
#
LIBRARY=	libform.a

include ../../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
delete :=       TARGET= delete
install :=      TARGET= install
lint :=         TARGET= lint
catalog :=      TARGET= catalog
package :=      TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# definitions for install_h target
HDRS=		form.h utility.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=inc/%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: inc/%
	$(INS.file)

install_h: $(ROOTHDRS)

check:  $(CHECKHDRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

