#
#ident	"@(#)Makefile	1.2	97/06/30 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/i386/prom/Makefile
#

include ../../../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386

SUBDIRS = $($(MACH)_ARCHITECTURES)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
