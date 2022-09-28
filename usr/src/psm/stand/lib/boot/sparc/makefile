#
#ident	"@(#)Makefile	1.1	97/06/30 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/boot/sparc/Makefile

include ../../../../../Makefile.master

SUBDIRS = sun4c sun4m sun4d sun4u

all install clean clobber lint: $(SUBDIRS)

all	:=	TARGET = all
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
install	:=	TARGET = install

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
