# Copyright 02/12/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.26 97/02/12 Sun Microsystems"

#
# snmp/agent/Makefile
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

LIBNAME= ssagent
EXT = 1
AGENT1 = lib$(LIBNAME).so
AGENT = lib$(LIBNAME).so.$(EXT)

LIBPROG = $(AGENT)
OPTLIB = $(ROOT)/usr/lib
OPTPROG = $(PROG:%=$(OPTLIB)/%)

INS.file = $(TEST) -d $(OPTLIB) || mkdir -p $(OPTLIB) ; \
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $< ;\
	cd $(OPTLIB) ; $(RM) $(AGENT1) ; ln -s $(@F) $(AGENT1)



$(OPTLIB)/%: %
	$(INS.file)
	

LINTTARGET= $(BIN)/llib-l$(LIBNAME).ln


SOURCES.c=\
	node.c\
	access.c\
	agent.c\
	personal.c\
	snmpd.c\
	pagent.c\
	subtree.c\
	reg_subtree.c

SOURCES.h=\
	../snmplib/impl.h\
	../snmplib/error.h\
	../snmplib/trace.h\
	../snmplib/signals.h\
	../snmplib/asn1.h\
	../snmplib/snmp.h\
	../snmplib/pdu.h\
	../snmplib/request.h\
	../snmplib/trap.h\
	agent_msg.h\
	node.h\
	access.h\
	agent.h\
	personal.h\
	snmpd.h\
	pagent.h\
	subtree.h\
	reg_subtree.h\
	table.h\
	oid_ar_p.h\
	config.h


OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

LINTOBJECTS= $(SOURCES.c:%.c=$(BIN)/%.ln)

CPPFLAGS += -I. -I../snmplib -I${BIN}

CFLAGS += -c -Kpic -xregs=no%appl

LDFLAGS += $(LDLIBS)

LINTFLAGS= -aumx

LINT=   lint

FILEMODE= 0755
OWNER= bin
GROUP= bin
 

PROG=$(LIBPROG)
CLOBBERFILES=$(SOURCES.c) $(SOURCES.h)

.KEEP_STATE:
 
#######################################################
 
#
# all
#
 
all: $(SOURCES.h) agent-mapfile-vers $(LIBPROG)
 
install: all $(OPTPROG)

$(LIBPROG): $(BIN) $(OBJECTS)
	$(CC) -G -M agent-mapfile-vers $(OBJECTS) -h$(@F) -o $(LIBPROG) $(LDLIBS)
	@$(RM) $(AGENT1)
	@ln -s $(LIBPROG) lib$(LIBNAME).so

$(BIN)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@

${BIN}/personal.c: personal.y
	yacc personal.y
	mv y.tab.c ${BIN}/personal.c

${BIN}/personal.lex.c: personal.l
	lex -t personal.l > ${BIN}/personal.lex.c

$(BIN)/personal.o: ${BIN}/personal.c ${BIN}/personal.lex.c
	$(CC)  $(CFLAGS) $(CPPFLAGS) $(DEFINES) -c ${BIN}/personal.c  -o $(BIN)/personal.o


#
# lint
#

lint:
	$(LINT) $(CPPFLAGS) $(DEFINES) $(SOURCES.c)

lintlib: $(BIN) $(LINTTARGET)

$(LINTTARGET) : $(LINTOBJECTS)
	$(LINT) $(LINTOBJECTS) -o $(LIBNAME)
	mv llib-l$(LIBNAME).ln $(LINTTARGET)

$(BIN)/%.ln: %.c
	$(LINT) $(CPPFLAGS) $(DEFINES) $*.c -C $(BIN)/$*

#
# directories
#

$(BIN):
	@$(TEST) -d $@ || mkdir $@


#
# clean
#

clean:
	$(RM) $(OBJECTS) $(LIBPROG) agent-mapfile-vers
	$(RM) $(LINTOBJECTS) $(LINTTARGET)
	$(RM) ${BIN}/personal.lex.c ${BIN}/personal.c

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
