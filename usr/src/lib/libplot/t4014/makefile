#
#ident	"@(#)Makefile	1.5	97/09/25 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libplot/t4014/Makefile
#

include ../../../Makefile.master

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

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

