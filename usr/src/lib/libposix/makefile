#
#ident "@(#)Makefile  1.6     97/08/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libposix/Makefile
#
LIBRARY =	libposix4.a
VERS =		.1
INCLIST=	-Iinc 

include ../../Makefile.master
include ../Makefile.lib

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

# include common library definitions

CPPFLAGS=	$(INCLIST) $(CPPFLAGS.master) -D_REENTRANT
LIBS =		$(DYNLIB)

#HDRS = 
ROOTHDRDIR =	$(ROOT)/usr/include/sys
ROOTHDRS =	$(HDRS:%=$(ROOTHDRDIR)/%)

$(ROOTHDRS) :=	FILEMODE = 644

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

install:	$(ROOTLIBS) $(ROOTLINKS)

$(ROOTHDRDIR)/% : %
		$(INS.file)

install_h: $(ROOTHDRS)

clean clobber:	FRC
		@cd $(MACH); pwd; $(MAKE) $@

links:
		-$(RM) $(MACH)
		$(SYMLINK) `mach` $(MACH)

FRC:
