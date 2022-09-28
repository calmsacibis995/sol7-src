#
#ident	"@(#)Makefile	1.3	97/07/24 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libbsdmalloc/Makefile
#

include         ../../Makefile.master

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

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
HDRS=
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

