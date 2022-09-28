#
# Copyright (c) 1992-1995,1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.27	98/01/21 SMI"
#
# lib/libgss/Makefile
#
include ../../Makefile.master

# defines the duplicate sources we share with gsscred
GSSCRED_DIR = ../../cmd/gss/gsscred
DUPLICATE_SRC = gsscred_xfn.c gsscred_utils.c gsscred_file.c # gen_oids.c
CLEAN_SRC = $(DUPLICATE_SRC) gen_oids.c

SUBDIRS= $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

#add the gsscred directory as part of the include path

HDRS=

ROOTDIRS= $(ROOT)/usr/include

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

GSSMECH_DIR=	$(ROOT)/usr/lib/gss

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)


all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=         TARGET= lint

.KEEP_STATE:

all:  $(DUPLICATE_SRC) .WAIT $(SUBDIRS)

lint:	$(DUPLICATE_SRC) .WAIT $(SUBDIRS)

install: $(GSSMECH_DIR) all .WAIT $(SUBDIRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

clean clobber:	$(SUBDIRS)

check: $(CHECKHDRS)

#copy the source files that we share with the gsscred utility
gsscred_xfn.c: $(GSSCRED_DIR)/gsscred_xfn.c
	rm -f gsscred_xfn.c
	cp -p $(GSSCRED_DIR)/gsscred_xfn.c .

gsscred_utils.c: $(GSSCRED_DIR)/gsscred_utils.c
	rm -f gsscred_utils.c
	cp -p $(GSSCRED_DIR)/gsscred_utils.c .

gsscred_file.c:	$(GSSCRED_DIR)/gsscred_file.c
	rm -f gsscred_file.c
	cp -p $(GSSCRED_DIR)/gsscred_file.c .

$(ROOTDIRS):
	$(INS.dir)

$(GSSMECH_DIR):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.file)

# include library targets
include ../Makefile.targ

#additional dependencies

$(MACH) $(MACH64):   FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
 

FRC:
