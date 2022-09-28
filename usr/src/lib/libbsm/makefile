#
# Copyright (c) 1988,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.74	98/02/19 SMI"
#
# lib/libbsm/Makefile
#

include ../../Makefile.master

SUBDIRS=	$(MACH)	$(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

COMMONDIR = common

#
# Macros for libbsm header files. These define user-level only interfaces.
#
GENHDRS = audit_uevents.h
HDRS = libbsm.h devices.h
COMMONHDRS =	$(HDRS:%=$(COMMONDIR)/%)
ROOTHDRDIR = 	$(ROOT)/usr/include/bsm
ROOTCHDRS = 	$(HDRS:%=$(ROOTHDRDIR)/%)
ROOTHDRS = 	$(GENHDRS:%=$(ROOTHDRDIR)/%)

CHECKCHDRS =	$(COMMONHDRS:%.h=%.check)
CHECKHDRS =	$(GENHDRS:%.h=%.check)

$(ROOTHDRS) := 	FILEMODE = 0644
$(ROOTCHDRS) :=	FILEMODE = 0644

#
# Macros for libbsm database files. These should probably be installed
# from somewhere else. Until we find that better place, install them
# from here.
#
ETCSECURITYFILES = 	audit_class audit_control audit_event audit_user
ETCSECURITYSRC =	$(ETCSECURITYFILES:%=%.txt)
ROOTETCSECURITY = 	$(ROOT)/etc/security
$(ROOTETCSECURITY) := 	DIRMODE = 0755
$(ROOTETCSECURITY) := 	OWNER = root
$(ROOTETCSECURITY) := 	GROUP = sys
ROOTETCSECURITYFILES =	$(ETCSECURITYFILES:%=$(ROOTETCSECURITY)/%)
$(ROOTETCSECURITYFILES) := FILEMODE = 0640
$(ROOTETCSECURITYFILES) := OWNER = root
$(ROOTETCSECURITYFILES) := GROUP = sys

#
# /etc/security/audit/localhost/files is a symbolic link to /var/audit.
# This is provided so that auditreduce will work in the default configuration.
#
RESA=$(ROOTETCSECURITY)/audit
RESAL=$(RESA)/localhost
VARAUDIT=$(ROOT)/var/audit
AUDITDIRS=$(RESA) $(RESAL) $(VARAUDIT)
$(AUDITDIRS) := FILEMODE = 0750
$(AUDITDIRS) := OWNER = root
$(AUDITDIRS) := GROUP = sys

ARSYMLNK=$(RESAL)/files

.KEEP_STATE:

install: install_data install_dirs

#		$(ROOTUSRLIB) $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS) $(ROOTCHDRS)

check:	$(CHECKHDRS) $(CHECKCHDRS)

install_data: $(ETCSECURITYSRC) $(ROOTETCSECURITY) $(ROOTETCSECURITYFILES)

install_dirs: $(AUDITDIRS) $(ARSYMLNK)

audit_uevents.h: mkhdr.sh audit_event.txt
	sh mkhdr.sh

$(ETCSECURITYFILES) $(RESA): $(ROOTETCSECURITY)

$(RESAL): $(RESA)

$(ARSYMLNK): $(RESAL)

$(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/%:%
	$(INS.file)

$(ROOTHDRDIR)/%:$(COMMONDIR)/%
	$(INS.file)

$(AUDITDIRS):
	$(INS.dir)

$(ARSYMLNK): 
	$(RM) $@
	$(SYMLINK) ../../../../var/audit $@

$(ROOTETCSECURITY):
	$(INS.dir)

$(ROOTETCSECURITY)/%: %.txt
	$(INS.rename)

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
