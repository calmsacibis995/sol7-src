#
#pragma ident	"@(#)Makefile	1.6	97/09/17 SMI"
#
# Copyright (c) 1989-97 by Sun Microsystems, Inc.
#
# lib/libplot/plot/Makefile
#
include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all clean clobber delete install lint: $(SUBDIRS)


$(MACH) $(MACH64):	FRC
	 @cd $@; pwd; $(MAKE) $(TARGET)

FRC:
