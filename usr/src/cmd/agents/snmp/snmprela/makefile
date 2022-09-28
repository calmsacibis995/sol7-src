#
#
# ident	"@(#)Makefile	1.29	97/10/30 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# snmp/snmprelayd/Makefile
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

AGENT = snmpdx
BINPROG = $(AGENT)


OPTBIN = $(ROOT)/usr/lib/snmp
OPTPROG = $(PROG:%=$(OPTBIN)/%)

INS.binprog = $(INS) -s -m 0755 -u $(OWNER) -g $(GROUP) -f $(@D) $<
$(OPTBIN)/%: %
	@$(TEST) -d $(OPTBIN) || mkdir -p $(OPTBIN)
	$(INS.binprog)

MYCONF = $(CONFIG_FILES)
CONFLOC = $(ROOT)/etc/snmp/conf
CONFRULE = $(MYCONF:%=$(CONFLOC)/%)
$(CONFLOC)/%:%
	@$(TEST) -d $(CONFLOC) || mkdir -p $(CONFLOC)
	$(INS.file)

STUB.c = mib_handler.c $(AGENT)_tree.c $(AGENT)_appl.c $(AGENT)_trap.c
STUB.h = $(AGENT)_stub.h

SOURCES.c=\
	agent.c\
	subtree.c\
	session.c\
	dispatcher.c\
	res.c\
	sh_table.c\
	$(STUB.c)

SOURCES.h=\
	../snmplib/impl.h\
	../snmplib/error.h\
	../snmplib/trace.h\
	../snmplib/asn1.h\
	../snmplib/signals.h\
	../snmplib/snmp.h\
	../snmplib/pdu.h\
	../snmplib/request.h\
	snmprelay_msg.h\
	agent.h\
	subtree.h\
	session.h\
	config.h\
	dispatcher.h\
	res.h\
	$(STUB.h)\
	../snmplib/trap.h\
	../agent/access.h\
	../agent/agent_msg.h\
	sh_table.h\
	name.h

#
# We need the config files to build the sacom directory,
# so pull them out of SCCS.
#

CONFIG_FILES =\
	mibiisa.rsrc\
	snmpdx.acl\
	snmpdx.reg\
	snmpdx.rsrc\
	enterprises.oid

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

EXT = 1
MYLIBS=	../agent/libssagent.so.$(EXT) ../snmplib/libssasnmp.so.$(EXT)
MYLINKLIBS += -L../agent -lssagent -L../snmplib -lssasnmp

LIBS=	-lsocket -lnsl -ly -ll -lgen

CPPFLAGS += -I. -I../agent -I../snmplib -I${BIN}

CFLAGS += -c 

LDFLAGS += $(LDLIBS)

LINT=lint

SNMP_ROOT= ..
PARSER_HOME= $(SNMP_ROOT)/parser
MIB_HOME= $(SNMP_ROOT)/mib
PURIFY_HOME= /usr/soft/purify3.0


MIBS= $(MIB_HOME)/mib_core.txt $(MIB_HOME)/snmpdx.mib
PARSER= $(PARSER_HOME)/$(BIN)/mibcodegen
PURIFY= $(PURIFY_HOME)/purify

.KEEP_STATE:

#######################################################

#
# all
#

FILEMODE = 0644
$(CONFLOC)/snmpdx.acl := FILEMODE = 0600
OWNER = root
GROUP = sys

PROG=$(BINPROG)
CLOBBERFILES=$(STUB.c) $(STUB.h) $(SOURCES.c) $(SOURCES.h)

all: $(SOURCES.h) $(BINPROG) $(CONFIG_FILES)

install: all $(OPTPROG) $(CONFRULE)


$(BINPROG): $(BIN) $(OBJECTS) $(BIN)/config.o $(MYLIBS)
	$(CC) -o $(BINPROG) $(LDFLAGS) $(OBJECTS) $(BIN)/config.o $(MYLINKLIBS) $(LIBS)

$(BIN)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@

${BIN}/config.c: config.y
	yacc config.y
	cat name.h y.tab.c > ${BIN}/config.c

${BIN}/config.lex.c: config.l
	lex -t config.l > ${BIN}/config.lex.c

$(BIN)/config.o: ${BIN}/config.c ${BIN}/config.lex.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) ${BIN}/config.c -o $(BIN)/config.o


#
# parse
#
parse:	$(MIBS)
	$(PARSER) -b $(AGENT) -f $(MIBS)

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
gen:	
	$(RM) $(AGENT)_stub.h
	$(RM) $(AGENT)_stub.c
	$(RM) $(AGENT)_appl.c
	$(RM) $(AGENT)_tree.c
	$(RM) $(AGENT)_trap.c


clean:
	$(RM) $(OBJECTS) $(BINPROG)
	$(RM) $(BIN)/config.lex.c $(BIN)/config.c

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
