#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

#pragma ident	"@(#)Makefile	1.9	97/06/03 SMI"

include ../Makefile.cmd

SUBDIRS=lib parse xntpd ntpq ntpdate ntptrace etc.inet init.d

all :=		TARGET=all
install :=	TARGET=install
clobber :=	TARGET=clobber
clean :=	TARGET=clean
lint :=		TARGET=lint

lint:

all install clobber clean:	$(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

install_h check _msg _dc:

FRC:

