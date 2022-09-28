# Copyright 12/06/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.22 96/12/06 Sun Microsystems"

##############################################
#
# Makefile for parser
#
##############################################

#
# Makefile.vars
#

include ../../Makefile.vars

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

TARGET=	mibcodegen

# 
OPTBIN = $(ROOT)/usr/bin
OPTPROG = $(PROG:%=$(OPTBIN)/%)

#
# other variables
#

.KEEP_STATE:

SOURCES.c=\
	parse.c\
	backend.c

SOURCES.h=\
	../snmplib/impl.h\
	../snmplib/error.h\
	../snmplib/asn1.h\
	parse.h

OBJECTS= $(SOURCES.c:%.c=$(BIN)/%.o)

EXT = 1
MYLIBS= ../snmplib/libssasnmp.so.$(EXT)
MYLINKLIBS += -L../snmplib -lssasnmp

LIBS= -lsocket -lnsl -lgen 

DEFINES=

CPPFLAGS += -I. -I../snmplib

CFLAGS += -c 

LDFLAGS += $(LDLIBS)

LINT= lint

MIB_HOME=	../mib

MIBS=		$(MIB_HOME)/mib_core.txt $(MIB_HOME)/mib_example.txt

LINTFLAGS= -aumx

LINT=   lint

CLOBBERFILES=$(SOURCES.c) $(SOURCES.h)


##############################################


#
# all
#

all: $(SOURCES.h) $(TARGET)

install: all

$(TARGET): $(BIN) $(OBJECTS) $(MYLIBS)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS) $(MYLINKLIBS) $(LIBS)

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
# example
#

example:
	$(TARGET) example $(MIBS);


#
# directories
#
 
$(BIN):
	@$(TEST) -d $@ || mkdir $@


#
# clean
#

clean:
	$(RM) $(OBJECTS) $(TARGET)

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
