#
#ident	"@(#)Makefile 1.17     97/08/06 SMI"
#
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libtnfprobe/Makefile
#

include ../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all	:= 	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

#definitions for install_h target
HDRS=		com.h writer.h probe.h
ROOTHDRDIR=	$(ROOT)/usr/include/tnf
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)
$(ROOTHDRS) := 	FILEMODE = 0644
CHECKHDRS =	$(HDRS:%.h=%.check)

.KEEP_STATE:

all install clean clobber lint:  $(SUBDIRS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS)

check: $(CHECKHDRS)

$(ROOTLIBDIR) $(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/% : %
	$(INS.file)

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
