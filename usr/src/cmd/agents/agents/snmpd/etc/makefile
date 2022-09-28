# Copyright 1988 - 12/11/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.42 96/12/11 Sun Microsystems"

# Sun considers its source code as an unpublished, proprietary trade 
# secret, and it is available only under strict license provisions.  
# This copyright notice is placed here only to protect Sun in the event
# the source is deemed a published work.  Disassembly, decompilation, 
# or other means of reducing the object code to human readable form is 
# prohibited by the license agreement under which this code is provided
# to the user or company in possession of this copy.
# 
# RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
# Government is subject to restrictions as set forth in subparagraph 
# (c)(1)(ii) of the Rights in Technical Data and Computer Software 
# clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
# NASA FAR Supplement.
#

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

# make targets
TARGETS = all install bin debug purify static share shared profile profiled \
	svr4 demo profile-svr4 profiled-svr4 debug-svr4 \
        lint tags etags size wc sc print xref pretty indent

ITARGETS = install-debug install-svr4 install-debug-svr4 install-purify-svr4 
S1TARGETS = install-sunos
FILES = snmpd.conf
RCFILES = init.snmpd

PROG=$(TARGETS)
CLOBBERFILES=$(RCFILES) $(FILES) agents mgrs.v1 mibiisa.reg sun.mib

BIN_DIR = bin$(TARGET_ARCH)

.KEEP_STATE:

$(TARGETS): $(BIN) $(BINDIR) $(FILES) $(RCFILES)
	
$(ITARGETS): all
	install -d $(DESTDIR)/etc
	install $(FILES) $(DESTDIR)/agents 
	install $(RCFILES) $(DESTDIR)/etc

$(S1TARGETS):
	@install -d $(DESTDIR)/etc
	install $(FILES) $(DESTDIR)/agents
	install $(RCFILES) $(DESTDIR)/etc

clean: 
	rm -rf *~ ,* %*% a.out core *.ln tags

realclobber: clean
	sccs clean

$(BIN) $(BIN_DIR):
	@$(TEST) -d $@ || mkdir -p $@

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
