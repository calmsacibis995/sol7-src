#
#ident	"@(#)Makefile	1.1	97/06/30 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/boot/Makefile

include ../../../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386

SUBDIRS = $($(MACH)_ARCHITECTURES)

all install clean clobber lint: $(SUBDIRS)

all	:=	TARGET = all
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
install	:=	TARGET = install

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
