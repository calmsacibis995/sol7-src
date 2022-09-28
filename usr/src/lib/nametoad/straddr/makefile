#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.16	97/08/21 SMI"
#
# lib/nametoaddr/straddr/Makefile

include		../../../Makefile.master

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

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

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
