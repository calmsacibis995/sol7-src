# Copyright 1988 - 10/18/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.31 96/10/18 Sun Microsystems"

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

ITARGETS = install install-debug install-svr4 install-debug-svr4 \
	   install-purify-svr4 install-demo install-sunos


FILES = etc-encode.h \
	print.h \
	buildpkt.h \
	libfuncs.h \
	buffer.h \
	encode.h \
	objectid.h \
	localio.h \
	decode.h \
	asn1.h \
	mib.h \
	snmp.h


PROG=$(TARGETS) $(ITARGETS)
CLOBBERFILES=$(FILES)

BIN_DIR = bin$(TARGET_ARCH)

.KEEP_STATE:

$(TARGETS): $(BIN) $(FILES)
	
$(ITARGETS): $(FILES)

clean: 
	rm -rf *~ ,* %*% a.out core *.ln tags

realclobber: clean
	sccs clean

$(BIN) $(BIN_DIR):
	@$(TEST) -d $@ || mkdir -p $@

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ

