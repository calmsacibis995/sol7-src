# Copyright 12/03/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.13 96/12/03 Sun Microsystems"

#
# snmp/mib/Makefile
#

include ../../Makefile.vars

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#
 
AGENT =	snmpdx.mib
BINPROG = $(AGENT)
OPTMIB = $(ROOT)/var/snmp/mib
OPTPROG = $(PROG:%=$(OPTMIB)/%)
 
INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<
 
$(OPTMIB)/%: %
	@$(TEST) -d $(OPTMIB) || mkdir -p $(OPTMIB)
	chgrp $(GROUP) $(ROOT)/var/snmp
	chgrp $(GROUP) $(OPTMIB)
	$(INS.file)

SOURCES.c=

SOURCES.h= snmpdx.mib

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

CFLAGS += -c

LDFLAGS +=

LINT=	lint

.KEEP_STATE:
 
#######################################################
 
#
# all
#
 
FILEMODE = 0644
OWNER = root
GROUP = sys
 
PROG=$(BINPROG)
CLOBBERFILES=$(SOURCES.c) $(SOURCES.h)
 
all: $(SOURCES.h) $(BINPROG)
 
install: all $(OPTPROG)

$(BINPROG): $(BIN) $(OBJECTS)

$(BIN)/%.o: %.c


#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) 


#
# purify
#

purify:
	$(PURIFY) $(CC) -o $(BINPROG) $(LDFLAGS) $(OBJECTS)


#
# directories
#

$(BIN):
	@$(TEST) -d $@ || mkdir $@


#
# clean
#

clean:
	$(RM) $(OBJECTS) $(BINPROG)

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
