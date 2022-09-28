# Copyright 1988 - 09/30/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.61 96/09/30 Sun Microsystems"

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

# top-level Master makefile for agents

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../misc.sh ../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

# Use $TARG_SYS for OS dependent variables
#
TARG_SYS:sh = \
 (                                      \
   UNAME=` uname -r 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo SUNOS;;                     \
   4.*) echo SUNOS;;                    \
   5.*) echo SVR4;;                     \
   *)  echo SVR4;;                      \
   esac                                 \
 )


AGENTDIR = $(DESTDIR)/agents

SUNOS_SUBDIRS= snmpd

SVR4_SUBDIRS= snmpd

DEBUG_SUBDIRS= snmpd 

PURIFY_SUBDIRS= snmpd 

SUBDIRS = $($(TARG_SYS)_SUBDIRS)

all 	:=  TARGET = all
install :=  TARGET = install
debug	:=  TARGET = debug
purify	:=  TARGET = purify
install-svr4 :=  TARGET = install-svr4
install-debug-svr4 :=  TARGET = install-debug-svr4
install-purify-svr4 :=  TARGET = install-purify-svr4
install-sunos :=  TARGET = install-sunos
install-debug-sunos :=  TARGET = install-debug-sunos
clean   :=  TARGET = clean
check	:=  TARGET = check
realclobber := TARGET = realclobber
 
all:  $(SUBDIRS)

install:
	cd snmpd; make install

debug:  $(DEBUG_SUBDIRS)
purify:  $(PURIFY_SUBDIRS)

lint:
	@cd $@; pwd; $(MAKE) lint

install-svr4 install-sunos install-purify-svr4 \
install-debug-svr4 install-debug-sunos : installdirs $(SUBDIRS)

installdirs : FRC
	@test -d $(AGENTDIR) || mkdir -p $(AGENTDIR)

clean: $(SUBDIRS)

realclobber: $(SUBDIRS)

check: $(SUBDIRS)
	-@sccs check	

$(SUBDIRS) : FRC
	@cd $@; pwd; $(MAKE) $(TARGET) DESTDIR=${DESTDIR}

FRC :

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../misc.sh ../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
