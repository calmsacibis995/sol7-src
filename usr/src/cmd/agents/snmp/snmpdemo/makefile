# Copyright 01/16/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.22 97/01/16 Sun Microsystems"

#####################################################
#
# Makefile for a SNMP Agent
#
#####################################################

#
# Makefile.vars
#

#####################################################
# Edit the following variables as needed
#####################################################

BIN=		bin

SNMP_ROOT=	/usr

SNMP_LIB=	$(SNMP_ROOT)/lib

SNMP_INC=	$(SNMP_ROOT)/include/snmp

PURIFY_HOME=	/usr/soft/purify3.0


AGENT=		snmpdemo

MIBS=		mib_demo.txt


#####################################################

TARGET=		$(AGENT)d


.KEEP_STATE:

SOURCES.c=\
	$(AGENT)_tree.c\
	$(AGENT)_stub.c\
	$(AGENT)_trap.c\
	$(AGENT)_appl.c\
	$(AGENT)_demoEntry.c

SOURCES.h=\
	$(SNMP_INC)/impl.h\
	$(SNMP_INC)/asn1.h\
	$(SNMP_INC)/error.h\
	$(SNMP_INC)/pdu.h\
	$(SNMP_INC)/trap.h\
	$(SNMP_INC)/node.h\
	$(AGENT)_stub.h

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)


#
#  CC flags
#

DEFINES=

MY_INCLUDE=	-I. -I$(SNMP_INC)

CPPFLAGS +=	$(MY_INCLUDE)

CFLAGS=		-c


#
# LD flags
#

EXT = 1
#MYLIBS=\
#	$(SNMP_LIB)/libssagent.so.$(EXT)\
#	$(SNMP_LIB)/libssasnmp.so.$(EXT)
MYLINKLIBS += $(LDLIBS) -L$(SNMP_LIB) -lssagent -L$(SNMP_LIB) -lssasnmp

LIBS=	$(MYLINKLIBS) -lsocket -lnsl -ly -ll -lelf -lgen

TEST=	test

PURIFY= $(PURIFY_HOME)/purify

PARSER=	$(SNMP_ROOT)/$(BIN)/mibcodegen


#####################################################

#
# all
#

all: $(SOURCES.h) $(TARGET)   

install: all

$(TARGET): $(BIN) $(OBJECTS) $(MYLIBS)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS) $(MYLIBS) $(LIBS)

$(BIN)/%.o: %.c  
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@


#
# parse
#

parse:	$(MIBS)
	$(PARSER) -b $(AGENT) -f $(MIBS)


#
# lint
#
 
lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) $(MYLIBS) $(LIBS)


#
# purify
#
 
purify:
	$(PURIFY) $(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS) $(MYLIBS) $(LIBS)


#
# directories
#

$(BIN):
	@$(TEST) -d $@ || mkdir $@


#
# clean
#

clean:
	rm -f $(OBJECTS)

clobber: clean
	sccs clean

