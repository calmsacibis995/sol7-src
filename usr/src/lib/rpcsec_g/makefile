#
# Copyright (c) 1995,1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.15	97/11/01 SMI"
#
include ../../Makefile.master

SUBDIRS= $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

HDRS=

CHECKHDRS= $(HDRS:%.h=%.check)

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all: .WAIT $(SUBDIRS)

lint:   .WAIT $(SUBDIRS)

install: all .WAIT $(SUBDIRS)

install_h:

clean clobber:  $(SUBDIRS)

check: $(CHECKHDRS)

# include library targets
include ../Makefile.targ

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
