# Copyright 12/02/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.11 96/12/02 Sun Microsystems"

#
# dmi2.0/ciagent/Makefile
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

AGENT1 = ciinvoke
#
# namedir.cmd is part of the SDK and shouldn't be installed in proto
#
# AGENT2 = namedir.cmd
BINPROG = $(AGENT1)
OPTBIN = $(ROOT)/etc/dmi/ciagent
OPTBINPROG = $(PROG:%=$(OPTBIN)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(OPTBIN)/%: %
	@$(TEST) -d $(OPTBIN) || mkdir -p $(OPTBIN)
	chgrp $(GROUP) $(ROOT)/etc/dmi
	chgrp $(GROUP) $(OPTBIN)
	$(INS.file)

SOURCES.cc=

SOURCES.h= \
	ciinvoke \
	namedir.cmd

OBJECTS= $(SOURCES.cc:%.cc=$(OBJ_DIR)/%.o)

CCFLAGS += -ptr$(OBJ_DIR) -G -DSVR4 -D_REENTRANT -DSOLARIS2 -I../include   -c
LDFLAGS +=

PURIFY_HOME= /usr/soft/purify3.0
PURIFY= $(PURIFY_HOME)/purify

LINT=	lint

.KEEP_STATE:

FILEMODE = 0755
OWNER = root
GROUP = sys

PROG=$(BINPROG)
CLOBBERFILES= $(SOURCES.cc)

# Targets

all: $(BIN) $(BINPROG)

install: all $(OPTBINPROG)

#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) $(MYLIBS) $(LIBS)

#
# purify
#

purify:
	$(PURIFY) $(CCC) -o $(TARGET) $(LDFLAGS) $(OBJECTS) $(MYLIBS) $(LIBS)

#
# clean
#

clean:
	$(RM) $(SOURCES.h)

realclobber: clean
	sccs clean

$(BIN):
	@$(TEST) -d $@ || mkdir -p $@

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
