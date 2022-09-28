#
#pragma ident	"@(#)Makefile	1.14	97/11/07 SMI"
#
# Copyright (c) 1990 - 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdevinfo/Makefile
#

include ../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=      TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint
test :=         TARGET= test

# definitions for install_h target
HDRS=           device_info.h libdevinfo.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=%.check)

.KEEP_STATE:
 
all install clean clobber lint: $(SUBDIRS)

# install rule for install_h target

$(ROOTHDRDIR)/%: %
	$(INS.file)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

