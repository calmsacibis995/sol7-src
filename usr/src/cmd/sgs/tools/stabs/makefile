#
#ident	"@(#)Makefile	1.2	97/07/29 SMI"
#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/cmd/sgs/tools/Makefile.com

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

SUBDIRS_sparc	= sparc

# build only on archictures that have a SUBDIRS_arch entry above
SUBDIRS		= $(SUBDIRS_$(MACH))

.KEEP_STATE:

all:		$(SUBDIRS)
install:	$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)
lint:		$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
