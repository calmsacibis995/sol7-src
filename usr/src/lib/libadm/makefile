#
#pragma ident	"@(#)Makefile	1.15	97/08/26 SMI"
#
# Copyright (c) 1990,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libadm/Makefile
#

include		../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all clean clobber install lint: $(SUBDIRS)

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

