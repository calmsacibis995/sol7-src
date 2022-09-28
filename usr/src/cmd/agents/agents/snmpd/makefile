# Copyright 1988 - 09/30/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.31 96/09/30 Sun Microsystems"

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
# Top level makefile for SNMP agent

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

TARGETS = all install bin debug static share shared profile profiled \
	purify demo svr4 profile-svr4 profiled-svr4 debug-svr4 \
        lint etags tags size wc sc print xref pretty indent clean

ITARGETS = install-sunos install-debug install-svr4 install-debug-svr4 install-demo install-purify-svr4
SUBDIRS = etc h lib mibcomp sunagent
INSDIRS = h lib mibcomp etc sunagent


$(TARGETS) :
	for i in $(SUBDIRS);  \
	   do (cd $$i; pwd; $(MAKE) $@); done 

lint:
	for i in $(SUBDIRS); \
	   do (cd $$i; pwd; $(MAKE) lint); done

clobber:
	for i in $(SUBDIRS); \
	   do (cd $$i; pwd; $(MAKE) clobber); done

realclobber:
	for i in $(SUBDIRS); \
	   do (cd $$i; pwd; $(MAKE) realclobber); done

$(ITARGETS): $(SUBDIRS)
	for i in $(INSDIRS);  \
	   do (cd $$i; pwd; $(MAKE) $@ DESTDIR=$(DESTDIR)); done

check:

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
