#
#pragma ident   "@(#)Makefile 1.5     97/08/29 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# libpthread

include		../../Makefile.master

SUBDIRS=	$(MACH) $(MACH64)

all:=		TARGET=	all
install:=	TARGET=	install
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
lint:=		TARGET=	lint

.KEEP_STATE:

all clean clobber install lint:	lib32 $(BUILD64) lib64

lib32:	FRC
	@cd $(MACH); pwd; $(MAKE) $(TARGET)

lib64:	FRC
	@cd $(MACH64); pwd; $(MAKE) $(TARGET)

FRC:
