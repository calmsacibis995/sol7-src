# Copyright 11/13/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.27 96/11/13 Sun Microsystems"

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
 
LIBNAME = libdb
AGENT = $(LIBNAME).so
LIBPROG = $(AGENT)
OPTLIB = $(ROOT)/var/sea/dmi/mif
OPTPROG = $(PROG:%=$(OPTLIB)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<
 
$(OPTLIB)/%: %
	@$(TEST) -d $(OPTLIB) || mkdir -p $(OPTLIB)
	$(INS.file)
	cd $(ROOT)/opt/SUNWconn/SSA/dmi/lib ; $(RM) libdb.so.1 ; ln -s libdb.so libdb.so.1

SOURCES.cc= config.cc dmi_db.cc dmi_print.cc dbapi.cc

SOURCES.h= ../include/dmi_db.hh \
	   ../include/dmi_print.hh \
	   ../include/dbapi.hh

OBJECTS= $(SOURCES.cc:%.cc=$(BIN)/%.o)

CCFLAGS += -ptr$(BIN) -G -DSVR4 -D_REENTRANT -DSOLARIS2 -I../include   -c

LDFLAGS +=

LINT=	lint

FILEMODE= 0755
OWNER= root
GROUP= sys
 
PURIFY_HOME= /usr/soft/purify3.0

PURIFY= $(PURIFY_HOME)/purify

PROG=$(LIBPROG)
CLOBBERFILES=$(SOURCES.cc) $(SOURCES.h)

CONFIG_INCLUDES = ../include/dmi_db.hh \
	   ../include/util.hh \
	   ../include/search_util.hh \
	   ../include/dmi.hh \
	   ../include/server.h \
	   ../include/common.h \
	   name.h

DMI_PRINT_INCLUDES = ../include/dmi_print.hh

DBAPI_INCLUDES = ../include/dbapi.hh \
		 ../include/dmi_error.hh

.KEEP_STATE:
.KEEP_STATE_FILE: ./$(BIN)/.make.state

#######################################################

#
# all
#

all: $(SOURCES.h) $(LIBPROG) 

install: all $(OPTPROG)

$(LIBPROG): $(BIN) $(BIN)/Templates.DB $(OBJECTS) 
	$(CCC) -ptr$(BIN) -o $(LIBPROG) $(LDFLAGS) $(SHAREDLFLAG) $(OBJECTS) 

$(BIN)/%.o: %.cc
	$(CCC) $(CCFLAGS) $(CPPFLAGS) $(DEFINES) $*.cc -o $@

$(BIN)/config.cc: config.y $(CONFIG_INCLUDES)
	yacc config.y
	cat name.h y.tab.c > $(BIN)/config.cc


$(BIN)/config.lex.cc: config.l
	lex -t config.l > $(BIN)/config.lex.cc

$(BIN)/config.o: $(BIN)/config.cc $(BIN)/config.lex.cc
	$(CCC) -ptr$(BIN) $(SHAREDLFLAG) $(CCFLAGS) $(CPPFLAGS) $(DEFINES) -c $(BIN)/config.cc -o $(BIN)/config.o

parse: $(CONFIG_INCLUDES)
	lex -t config.l > $(BIN)/config.lex.cc
	yacc config.y
	mv y.tab.c $(BIN)/config.cc

dmi_print.cc: $(DMI_PRINT_INCLUDES)

dbapi.cc: $(DBAPI_INCLUDES)

#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.cc)

#
# purify
#

purify:
	$(PURIFY) $(CCC) -o $(LIBPROG) $(LDFLAGS) $(OBJECTS)

#
# clean
#

clean:
	$(RM) $(OBJECTS)
	$(RM) $(BIN)/config.lex.cc $(BIN)/config.cc
	$(RM) -r $(BIN)/Templates.DB

#
# directories
#

$(BIN) $(BIN)/Templates.DB:
	@$(TEST) -d $@ || mkdir -p $@

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
