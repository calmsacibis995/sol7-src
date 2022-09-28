# Copyright 04/24/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.76 97/04/24 Sun Microsystems"

#
# dmi2.0/dmisp/Makefile
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
 
AGENT = dmispd
BINPROG = $(AGENT)

LIB1 = libdmi.so
LIB2 = libdmici.so
LIB3 = libdmimi.so
LIBPROG1 = $(LIB1).$(SHAREDVER)
LIBPROG2 = $(LIB2).$(SHAREDVER)
LIBPROG3 = $(LIB3).$(SHAREDVER)
LIBPROG = $(LIBPROG1) $(LIBPROG2) $(LIBPROG3)
SABIN = $(ROOT)/usr/lib/dmi
SALIB = $(ROOT)/usr/lib
SABINPROG = $(BINPROG:%=$(SABIN)/%)
SALIBPROG = $(LIBPROG:%=$(SALIB)/%)

 
INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(SABIN)/%: %
	$(INS1.file)
 
$(SALIB)/%: %
	$(INS.file)

INS1.file = $(INS) -s -m $(FILEMODE) -u $(OWNER1) -g $(GROUP1) -f $(@D) $<
SOURCES.c =\
	ci_callback_clnt.c \
	ci_callback_xdr.c \
	common_xdr.c \
	mi_indicate_clnt.c \
	mi_indicate_xdr.c \
	server_clnt.c \
	server_xdr.c

SOURCES.cc =\
	api.cc \
	ci_callback_server.cc \
	ci_callback_svc.cc \
	ciapi.cc \
	database.cc \
	dmi.cc \
	dmi_error.cc \
	dmispd.cc \
	initialization.cc \
	listing.cc \
	mi_main.cc \
	miapi.cc \
	mutex.cc \
	operation.cc \
	regci.cc \
	search_util.cc \
	server_server.cc \
	server_svc.cc \
	spcallci.cc \
	subscription.cc \
	trace.cc \
	util.cc

TARGETS=\
	libdmi.$(SHAREDSUFFIX).$(SHAREDVER) \
	libdmici.$(SHAREDSUFFIX).$(SHAREDVER) \
	libdmimi.$(SHAREDSUFFIX).$(SHAREDVER) \
	$(BINPROG) 

COMMONXDROBJS = \
	$(OBJ_DIR)/common_xdr.o \
	$(OBJ_DIR)/server_xdr.o \
	$(OBJ_DIR)/ci_callback_xdr.o \
	$(OBJ_DIR)/mi_indicate_xdr.o

COMMONOBJS = \
	$(OBJ_DIR)/dmi.o \
	$(OBJ_DIR)/initialization.o \
	$(OBJ_DIR)/listing.o \
	$(OBJ_DIR)/operation.o \
	$(OBJ_DIR)/database.o \
	$(OBJ_DIR)/spcallci.o \
	$(OBJ_DIR)/subscription.o \
	$(OBJ_DIR)/dmi_error.o \
	$(OBJ_DIR)/util.o \
	$(OBJ_DIR)/search_util.o \
	$(OBJ_DIR)/regci.o \
	$(OBJ_DIR)/ci_callback_clnt.o \
	$(OBJ_DIR)/mi_indicate_clnt.o \
	$(OBJ_DIR)/mutex.o \
	$(OBJ_DIR)/trace.o \
	$(OBJ_DIR)/server_svc.o \
	$(OBJ_DIR)/server_server.o 


DBOBJS= \
	$(OBJ_DIR)/config.o \
	$(OBJ_DIR)/dmi_db.o \
	$(OBJ_DIR)/dmi_print.o \
	$(OBJ_DIR)/dbapi.o


SPOBJS = \
	$(OBJ_DIR)/dmispd.o 


CIAPIOBJS =  \
	$(OBJ_DIR)/server_clnt.o \
	$(OBJ_DIR)/ciapi.o \
	$(OBJ_DIR)/api.o \
	$(OBJ_DIR)/ci_callback_server.o \
	$(OBJ_DIR)/ci_callback_svc.o 


MIAPIOBJS = \
	$(OBJ_DIR)/server_clnt.o \
	$(OBJ_DIR)/miapi.o \
 	$(OBJ_DIR)/api.o

OBJECTS = \
	$(COMMONXDROBJS) \
	$(COMMONOBJS) \
	$(CIAPIOBJS) \
	$(MIAPIOBJS) \
	$(SPOBJS) \
	$(DBOBJS)

CI_CALLBACK_XDR_INCLUDES = ../include/ci_callback.h
MI_INDICATE_XDR_INCLUDES = ../include/mi_indicate.h
DMI_INCLUDES = ../include/trace.hh
INITIALIZATION_INCLUDES = ../include/mutex.hh \
			  ../include/subscription.hh
OPERATION_INCLUDES = ../include/spcallci.hh
CIAPI_INCLUDES = ../include/api.hh
CI_CALLBACK_SERVER_INCLUDES = ../include/ci_callbacks.hh
MIAPI_INCLUDES = ../include/miapi.hh
SERVER_SERVER_INCLUDES = ../include/operation.hh \
			 ../include/database.hh \
			 ../include/listing.hh \
			 ../include/initialization.hh \
			 ../include/regci.hh

SPLIBS = -L./. -ldmi $(THREADLIB) $(YACCLIB) $(LIBS) 

CFLAGS += -DSVR4 -D_REENTRANT -DSOLARIS2 $(CPPFLAGS) -I../include -I../../include/netmgt 
CCFLAGS += -ptr$(OBJ_DIR) -pic -xregs=no%appl -DSVR4 -D_REENTRANT -DSOLARIS2 $(CPPFLAGS) \
		-I../include -I../../include/netmgt 

PURIFY_HOME= /net/suntools/export/tools/sparc/bin
PURIFY= $(PURIFY_HOME)/purify

LINT = lint

.KEEP_STATE:

FILEMODE = 0755
OWNER = bin
OWNER1= root
GROUP = bin
GROUP1= sys
 
CLOBBERFILES= $(SOURCES.c) $(SOURCES).cc

# Targets 

all: $(BIN) $(OBJ_DIR) $(TEMPLATESBIN) $(BINPROG) $(LIBPROG)

install: all $(SALIBPROG) ins1 ins2 ins3 $(SABINPROG)

ins1: $(LIBPROG)
	@cd $(SALIB) ; $(RM) $(LIB1) ; ln -s $(LIBPROG1) $(LIB1)

ins2: $(LIBPROG)
	@cd $(SALIB) ; $(RM) $(LIB2) ; ln -s $(LIBPROG2) $(LIB2)

ins3: $(LIBPROG)
	@cd $(SALIB) ; $(RM) $(LIB3) ; ln -s $(LIBPROG3) $(LIB3)

libdmi.$(SHAREDSUFFIX).$(SHAREDVER): $(COMMONXDROBJS) $(COMMONOBJS) $(DBOBJS) dmi-mapfile-vers
	$(CCC) -ptr$(OBJ_DIR) -M dmi-mapfile-vers $(SHAREDLFLAG) -h$(@F) -o libdmi.$(SHAREDSUFFIX).$(SHAREDVER) $(COMMONXDROBJS) $(COMMONOBJS) $(DBOBJS) $(LDLIBS)
	@$(RM) -f libdmi.$(SHAREDSUFFIX)
	@ln -s libdmi.$(SHAREDSUFFIX).$(SHAREDVER) libdmi.$(SHAREDSUFFIX)

libdmici.$(SHAREDSUFFIX).$(SHAREDVER): $(CIAPIOBJS) ci-mapfile-vers
	$(CCC) -ptr$(OBJ_DIR) -M ci-mapfile-vers $(SHAREDLFLAG) -h$(@F) -o libdmici.$(SHAREDSUFFIX).$(SHAREDVER) $(CIAPIOBJS) $(LDLIBS)
	@$(RM) -f libdmici.$(SHAREDSUFFIX)
	@ln -s libdmici.$(SHAREDSUFFIX).$(SHAREDVER) libdmici.$(SHAREDSUFFIX)

libdmimi.$(SHAREDSUFFIX).$(SHAREDVER): $(MIAPIOBJS) mi-mapfile-vers
	$(CCC) -ptr$(OBJ_DIR) -M mi-mapfile-vers $(SHAREDLFLAG) -h$(@F) -o libdmimi.$(SHAREDSUFFIX).$(SHAREDVER) $(MIAPIOBJS) $(LDLIBS)
	@$(RM) -f libdmimi.$(SHAREDSUFFIX)
	@ln -s libdmimi.$(SHAREDSUFFIX).$(SHAREDVER) libdmimi.$(SHAREDSUFFIX)

$(BINPROG):  $(BIN) libdmi.$(SHAREDSUFFIX).$(SHAREDVER) libdmici.$(SHAREDSUFFIX).$(SHAREDVER) libdmimi.$(SHAREDSUFFIX).$(SHAREDVER) $(SPOBJS)
	$(CCC) -ptr$(OBJ_DIR) -o $(BINPROG) -norunpath -nolib \
		$(SPOBJS) $(SPLIBS) -lC -lc

$(OBJ_DIR)/config.cc: config.y name.h
	yacc config.y y.tab.c
	cat name.h y.tab.c > $(OBJ_DIR)/config.cc

$(OBJ_DIR)/config.lex.cc: config.l
	lex -t config.l > $(OBJ_DIR)/config.lex.cc

$(OBJ_DIR)/config.o: $(OBJ_DIR)/config.cc $(OBJ_DIR)/config.lex.cc
	$(CCC) $(SHAREDCCLAG) $(CCFLAGS) $(DEFINES) -c $(OBJ_DIR)/config.cc -o $(OBJ_DIR)/config.o

$(LIBPROG) : $(BIN) $(OBJECTS)

ci_callback_xdr.c: $(CI_CALLBACK_XDR_INCLUDES)

mi_indicate_xdr.c: $(MI_INDICATE_XDR_INCLUDES)

dmi.cc: $(DMI_INCLUDES)

initialization.cc: $(INITIALIZATION_INCLUDES)

operation.cc: $(OPERATION_INCLUDES)

ciapi.cc: $(CIAPI_INCLUDES)

ci_callback_server.cc: $(CI_CALLBACK_SERVER_INCLUDES)

miapi.cc: $(MIAPI_INCLUDES)

server_server.cc: $(SERVER_SERVER_INCLUDES)


#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(DEFINES) $(SOURCES.c) $(SOURCES.cc) $(MYLIBS) $(LIBS)

#
# directories
#


$(BIN) $(TEMPLATESBIN):
	@$(TEST) -d $@ || mkdir $@

$(OBJ_DIR):
	@$(TEST) -d $@/Templates.DB || mkdir -p $@/Templates.DB


#
# clean
# 

clean:
	$(RM) $(TARGETS) $(OBJECTS) $(BINPROG)
	$(RM) -r $(OBJ_DIR)
	unlink libdmi.$(SHAREDSUFFIX)
	unlink libdmimi.$(SHAREDSUFFIX)
	unlink libdmici.$(SHAREDSUFFIX)

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ

