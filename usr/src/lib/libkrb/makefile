#
#ident	"@(#)Makefile	1.22	97/12/08 SMI"
#
# Copyright (c) 1993-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libkrb/Makefile
#
include ../../Makefile.master

SUBDIRS= $(MACH) $(BUILD64) $(MACH64)
 
INTERFLAGS  = -DNOENCRYPTION
INTERCPP    = $(INTERFLAGS)

# definitions for install_h target

COMMON_HDRS=	\
	conf.h error_table.h klog.h kparse.h krb_conf.h \
	lsb_addr_comp.h mit-sipb-copyright.h osconf.h prot.h 

sparc_HDRS= conf-svsparc.h 
i386_HDRS=  conf-bsd386i.h

HDRS= $(COMMON_HDRS) $($(MACH)_HDRS)

ROOTHDRDIR=	$(ROOT)/usr/include/kerberos
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=includes/%.check)

# set mode on installed headers to avoid rebuilds caused by
# alternating file modes.  Corrects side effects from default
# permissions on installed dynamic libraries.
$(ROOTHDRS) :=	FILEMODE= 644

# install rule for install_h target
$(ROOTHDRDIR)/%: includes/%
	$(INS.file)

# targets for test commands in subdirectories; default is static libkrb
all bwos:=	TARGET= all
all_inter:=	TARGET= all
all_domestic:=	TARGET= all
install:=	TARGET= install
install_inter:=	TARGET= install
install_domestic:= TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
dynamic:=	TARGET= dynamic
cleantests:=	TARGET= clean
clobbertests:=	TARGET= clobber

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

# international version is default
all: all_inter

all_inter: $(SUBDIRS)

# international install is default
install: install_inter

install_inter: $(SUBDIRS)

install_domestic all_domestic: $(SUBDIRS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS)

includes/%.check:	includes/%.h
	$(DOT_H_CHECK)

check:	$(CHECKHDRS)

$(ROOTHDRDIR):
	$(INS.dir)

tests dynamic cleantests clobbertests: FRC
	@echo "test command directories..."
	@cd test.cmd;  pwd; $(MAKE) $(TARGET) "INTERFLAGS = $(INTERFLAGS)"
	@cd test.krpc; pwd; $(MAKE) $(TARGET)

clean: cleantests $(SUBDIRS)
clobber : clobbertests cleantests $(SUBDIRS)

FRC:

$(MACH) $(MACH64):   FRC
	@cd $@; pwd; $(MAKE) $(TARGET) "INTERFLAGS = $(INTERFLAGS)"

#  Special target to clean up source tree for distribution
#  Special targets to pick up unused source files

KRB_UNUSED =	\
	cr_err_reply.c getopt.c krb_err.et krbglue.c tf_shm.c

KRB_UFILES =	$(KRB_UNUSED:%=krb/%)

unused.files: $(KRB_UFILES)

FRC:
