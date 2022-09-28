# Copyright 02/12/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.25 97/02/12 Sun Microsystems"

#
# snmp/snmplib/Makefile
#

include ../../Makefile.vars

# OPTIONAL_INCLUDE_FILE1:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE1)


include $(SRC)/cmd/Makefile.cmd

#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#

LIBNAME= ssasnmp

EXT = 1
AGENT1 = lib$(LIBNAME).so
AGENT = lib$(LIBNAME).so.$(EXT)
LIBPROG = $(AGENT)
OPTLIB= $(ROOT)/usr/lib
OPTPROG = $(PROG:%=$(OPTLIB)/%)

INS.file = @$(TEST) -d $(OPTLIB) || mkdir -p $(OPTLIB) ; \
	   $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $< ;\
           cd $(OPTLIB) ; $(RM) $(AGENT1) ; ln -s $(@F) $(AGENT1)

$(OPTLIB)/%: %
	$(INS.file)


LINTTARGET= $(BIN)/llib-l$(LIBNAME).ln


SOURCES.c=\
	impl.c\
	error.c\
	trace.c\
	signals.c\
	asn1.c\
	pdu.c\
	request.c\
	trap.c\
	snmp_api.c\
	madman_api.c\
	madman_trap.c

SOURCES.h=\
	snmp_msg.h\
	impl.h\
	error.h\
	trace.h\
	signals.h\
	asn1.h\
	snmp.h\
	pdu.h\
	request.h\
	trap.h\
	snmp_api.h\
	madman_api.h\
	madman_trap.h

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

LINTOBJECTS= $(SOURCES.c:%.c=$(BIN)/%.ln)

DEFINES = 

CPPFLAGS += -I.

CFLAGS += -c -Kpic -xregs=no%appl

LDFLAGS += $(LDLIBS)

LINTFLAGS= -aumx

LINT=	lint

DIRMODE= 0775
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
 
all: $(SOURCES.h) snmp-mapfile-vers $(LIBPROG)
 
install: all $(OPTPROG)

$(LIBPROG): $(BIN) $(OBJECTS)
	$(CC) -G -M snmp-mapfile-vers $(OBJECTS) -h$(@F) -o $(LIBPROG) $(LDLIBS)
	@$(RM) $(AGENT1)
	@ln -s $(LIBPROG) $(AGENT1)

$(BIN)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@

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
	$(RM) $(OBJECTS) $(LIBPROG) snmp-mapfile-vers
	$(RM) $(LINTOBJECTS) $(LINTTARGET)

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE2:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE2)

include $(SRC)/cmd/Makefile.targ
