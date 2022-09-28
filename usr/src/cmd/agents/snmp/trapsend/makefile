# Copyright 02/12/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.21 97/02/12 Sun Microsystems"

#
# snmp/trapsend/Makefile
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
 
AGENT =	snmp_trapsend
BINPROG = $(AGENT)
OPTBIN = $(ROOT)/usr/sbin
OPTPROG = $(PROG:%=$(OPTBIN)/%)

INS.file = @$(TEST) -d $(OPTBIN) || mkdir -p $(OPTBIN);$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(OPTBIN)/%: %
	$(INS.file)


SOURCES.c=\
	trapsend.c \
	oid.c \
	usage.c

SOURCES.h=\
	../snmplib/impl.h\
	../snmplib/error.h\
	../snmplib/trace.h\
	../snmplib/asn1.h\
	../snmplib/snmp.h\
	../snmplib/pdu.h \
	./oid.h \
	./usage.h

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

MYLINKLIBS += $(LDLIBS) -L../snmplib -lssasnmp

LIBS=	-lsocket -lnsl -lgen

CPPFLAGS += -I. -I../snmplib -I../../include/netmgt -I${BIN}

CFLAGS += -c

LINT=lint

.KEEP_STATE:
 
#######################################################
 
#
# all
#
 
FILEMODE = 0755
OWNER = bin
GROUP = bin
 
PROG=$(BINPROG)
CLOBBERFILES=$(SOURCES.c) $(SOURCES.h)
 
all: $(SOURCES.h) $(BINPROG)
 
install: all $(OPTPROG)


$(BINPROG): $(BIN) $(OBJECTS)
	$(CC) -o $(BINPROG) $(LDFLAGS) $(OBJECTS) $(MYLINKLIBS) $(LIBS)

$(BIN)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@


#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) $(MYLINKLIBS) $(LIBS)


#
# purify
#

purify:
	$(PURIFY) $(CC) -o $(BINPROG) $(LDFLAGS) $(OBJECTS) $(MYLINKLIBS) $(LIBS)


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
