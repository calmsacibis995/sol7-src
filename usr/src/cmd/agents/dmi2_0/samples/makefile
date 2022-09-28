# Copyright 02/03/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.20 97/02/03 Sun Microsystems"


#
# dmi2.0/sample/Makefile 
#
 
include ../../Makefile.vars
include ../Makefile.incl.sun

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#
 
AGENT1 = namedir
BINPROG = $(AGENT1) 
OPTBIN = $(ROOT)/opt/SUNWconn/sea/dmi/sample
OPTBINPROG = $(PROG:%=$(OPTBIN)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(OPTBIN)/%: %
	$(INS.file)

SOURCES.cc = \
	ci_main.cc \
	namedir_callbacks.cc

OBJECTS = \
	$(OBJ_DIR)/ci_main.o \
	$(OBJ_DIR)/namedir_callbacks.o

NAMEDIROBJS = \
	$(OBJ_DIR)/namedir_callbacks.o \
	$(OBJ_DIR)/namedir.o

CIOBJS = \
	$(OBJ_DIR)/namedir_callbacks.o \
	$(OBJ_DIR)/ci_main.o

NAMEDIR_INCLUDES = ../include/ci_callback_svc.hh \
		   ../include/ciapi.hh

LIBS = -lnsl -Bstatic -lrwtool -Bdynamic

LINKLIBDIR = $(LDLIBS) \
             -L../dmisp \
             -L../dmidb \
             -L../../snmp/snmplib \
             -L../../snmp/agent \
             -L./

CILIBS = $(LINKLIBDIR) -ldmici -ldmimi -ldmi $(LIBS) 
NAMEDIRLIBS = $(LINKLIBDIR) -ldmici -ldmimi -ldmi $(LIBS)

CFLAGS += -DDEBUG -DSVR4 -D_REENTRANT -DSOLARIS2 -I../include
CCFLAGS += -DDEBUG -DSVR4 -D_REENTRANT -DSOLARIS2 -I../include

PURIFY_HOME= /net/suntools/export/tools/sparc/bin
PURIFY= $(PURIFY_HOME)/purify

LINT = lint

.KEEP_STATE:

FILEMODE = 0755
OWNER = root
GROUP = sys

PROG=$(BINPROG)
CLOBBERFILES= $(SOURCES.cc)

# Targets 

all:   $(BINPROG)

install: all $(OPTBINPROG)

dmici:  $(CIOBJS) 
	$(CCC) -ptr$(OBJ_DIR) -o $@ -R $(LD_RUN_PATH)  $(CIOBJS) $(CILIBS)
namedir: $(NAMEDIROBJS)
	$(CCC) -ptr$(OBJ_DIR) -o $@ -R $(LD_RUN_PATH)  $(NAMEDIROBJS) $(NAMEDIRLIBS)

namedir.cc: $(NAMEDIR_INCLUDES)


clean:
	$(RM) $(TARGETS) $(OBJECTS)
	$(RM) -r $(OBJ_DIR)/Templates.DB

clobber: clean

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ

