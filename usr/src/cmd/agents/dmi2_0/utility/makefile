# Copyright 04/24/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.17 97/04/24 Sun Microsystems"

#
# dmi2.0/utility/Makefile
#

include ../../Makefile.vars
include ../Makefile.incl.sun

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)
 
include $(SRC)/cmd/Makefile.cmd
 
#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#
 
AGENT1 = dmi_cmd
AGENT2 = dmiget
BINPROG = $(AGENT1) $(AGENT2)
USRSBIN = $(ROOT)/usr/sbin
USRSBINPROG = $(BINPROG:%=$(USRSBIN)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(USRSBIN)/%: %
	$(TEST) -d $(USRSBIN) || mkdir -p $(USRSBIN)
	$(INS.file)

SOURCES.cc = \
	dmicmd.cc \
	dmiget.cc

OBJECTS = \
	$(OBJ_DIR)/dmicmd.o \
	$(OBJ_DIR)/dmiget.o

CMDOBJS = \
	$(OBJ_DIR)/dmicmd.o 

GETOBJS = \
	$(OBJ_DIR)/dmiget.o 

INCLUDES = -I../include -I../../include/netmgt
 
LINKLIBDIR = $(LDLIBS) \
             -L../dmisp \
	     -L../../snmp/snmplib \
	     -L../../snmp/agent

LIBS = $(LINKLIBDIR) -ldmimi -ldmi -lnsl -ldl -Bstatic -lrwtool -Bdynamic

LDOPTIONS	= -norunpath -nolib -lC -lc

CCFLAGS += -DDEBUG -DSVR4 -D_REENTRANT -DSOLARIS2 $(INCLUDES)
 
PURIFY_HOME= /net/suntools/export/tools/sparc/bin
PURIFY= $(PURIFY_HOME)/purify

LINT = lint

.KEEP_STATE:

FILEMODE = 0755
OWNER = bin
GROUP = bin

PROG=$(BINPROG)
CLOBBERFILES=$(SOURCES.cc)

# Targets

all: $(BIN) $(BINPROG)

install: $(USRSBINPROG) all

dmi_cmd: $(CMDOBJS) 
	$(CCC) -ptr$(OBJ_DIR) -o $@ $(CMDOBJS) $(LIBS) $(LDOPTIONS)

dmiget: $(GETOBJS) 
	$(CCC) -ptr$(OBJ_DIR) -o $@ $(GETOBJS) $(LIBS) $(LDOPTIONS)

#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(DMI_SRCS) $(LIBS)

clean:
	$(RM) $(BINPROG) $(OBJECTS)
	$(RM) -r $(OBJ_DIR)/Templates.DB

realclobber: clean
	sccs clean

$(BIN) $(BIN_DIR) $(OBJDIR):
	@$(TEST) -d $@ || mkdir -p $@

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)
 
include $(SRC)/cmd/Makefile.targ
 
