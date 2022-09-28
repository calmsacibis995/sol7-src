# Copyright 12/13/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.14 96/12/13 Sun Microsystems"

include ../../Makefile.vars
include ../Makefile.incl.sun

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

AGENT1 = Makefile

EXAMPLE_DESTDIR = $(ROOT)/opt/SUNWconn/sea/dmi/example

AGENT1PROG = $(AGENT1:%=$(EXAMPLE_DESTDIR)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(EXAMPLE_DESTDIR)/%: %
	@$(TEST) -d $(EXAMPLE_DESTDIR) || mkdir -p $(EXAMPLE_DESTDIR)
	$(INS.file)

INCLUDES += -I$(MAPPER_DIR)/include/dmi1.1 \
		-I$(MAPPER_DIR)/include/nls/en_US \
		-I$(MAPPER_DIR)/include/common \
		-I$(MAPPER_DIR)/include \
		-I$(TOP)/include \
		-I$(SNMPLIB_DIR)/snmplib \
		-I$(SNMPLIB_DIR)/agent

CFLAGS += -DSVR4 -D_REENTRANT -DSOLARIS2 $(INCLUDES)

MIEXAMPLE_SRCS = miexample_svc.c 
		
MIEXAMPLE_OBJS = $(MIEXAMPLE_SRCS:%.c=$(OBJ_DIR)/%.o)

MIEXAMPLE_SVC_INCLUDES = miexample.h

#-----------------------------------------------------------------------
#  The following are libraries needed for linking
# (all but the first 2 might not be needed) $MED
#-----------------------------------------------------------------------
LIBS = -lnsl -ldl -Bstatic -lrwtool
EXAMPLELIBS= -lsocket  -ldmimi -ldmi -ldb -lgen $(LIBS)

#-----------------------------------------------------------------------
#  The following are directories of libraries for linking
#-----------------------------------------------------------------------

LINKLIBDIR = -L../dmisp \
	     -L../dmidb \
	     -L../../snmp/snmplib \
	     -L../../snmp/agent

#
# Input file locations
#

EXAMPLE_DIR = $(SRC)/cmd/agents/dmi2.0/example

.KEEP_STATE:

FILEMODE = 0644
OWNER = root
GROUP = sys

PROG=
CLOBBERFILES=$(MIEXAMPLE_SRCS) *.x *.h Makefile

all: $(BIN) $(BIN_DIR)/miexample $(AGENT1)

$(AGENT1):

install: all $(AGENT1PROG)

$(BIN_DIR)/miexample: $(MIEXAMPLE_OBJS) ../dmisp/libdmimi.$(SHAREDSUFFIX) ../dmisp/libdmi.$(SHAREDSUFFIX) ../dmidb/libdb.$(SHAREDSUFFIX) 
	$(CCC) -ptr$(OBJ_DIR) -o $@ -R $(LD_RUN_PATH) $(MIEXAMPLE_OBJS) $(LINKLIBDIR)  $(EXAMPLELIBS)


miexample_svc.c: $(MIEXAMPLE_SVC_INCLUDES)


# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(MIEXAMPLE_SRCS) $(MYLIBS) $(LIBS)


clean:
	$(RM) $(MIEXAMPLE_OBJS)

realclobber: clean
	sccs clean

$(BIN):
	@$(TEST) -d $@ || mkdir -p $@


# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
