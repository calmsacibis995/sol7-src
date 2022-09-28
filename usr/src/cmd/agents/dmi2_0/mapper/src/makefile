# Copyright 10/28/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.33 97/10/28 Sun Microsystems"

#
# dmi2.0/mapper/src/Makefile
#

include ../../../Makefile.vars
include ../../Makefile.incl.sun

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#
 
AGENT = snmpXdmid
BINPROG = $(AGENT)
SABIN = $(ROOT)/usr/lib/dmi
SABINPROG = $(BINPROG:%=$(SABIN)/%)

 
INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<
 
$(SABIN)/%: %
	@$(TEST) -d $(SABIN) || mkdir -p $(SABIN)
	$(INS.file)
 
SOURCES.c = \
        dmisa.c \
	dmisamap.c \
	dmisaxlt.c \
	dmisaget.c \
	dmismibg.c \
	dmisagnx.c \
	dmismibn.c \
	dmisaset.c \
	dmiskeyg.c \
	dmiskeyn.c \
	dmisalow.c \
	dmisamcb.c \
	dmisaind.c \
	mi_indicate_svc.c \
	dmisatrp.c \
	trace.c \
	dpitosar.c \
	mapper_stub.c

OBJECTS = \
        dmisa.o \
	dmisamap.o \
	dmisaxlt.o \
	dmisaget.o \
	dmismibg.o \
	dmisagnx.o \
	dmismibn.o \
	dmisaset.o \
	dmiskeyg.o \
	dmiskeyn.o \
	dmisalow.o \
	dmisamcb.o \
	dmisaind.o \
	mi_indicate_svc.o \
	dmisatrp.o \
	trace.o \
	dpitosar.o \
	mapper_stub.o

LDOPTIONS = -norunpath -nolib

DMISA_INCLUDES = ../include/dmisa.h \
		 ../include/dpitosar.h \
		 ../include/indication.h \
		 ../include/snmp_dpi.h \
		 ../include/trace.h

DMISAMAP_INCLUDES = ../include/dmisamap.h

DMISAIND_INCLUDES = ../include/gentrap.h


INCLUDES = -I$(MAPPER_DIR)/include/dmi1.1 \
		-I$(MAPPER_DIR)/include/nls/en_US \
		-I$(MAPPER_DIR)/include/common \
		-I$(MAPPER_DIR)/include \
		-I$(TOP)/include \
		-I$(SNMPLIB_DIR)/snmplib \
		-I$(SNMPLIB_DIR)/agent

MAPPERLIBS = -lsocket -lssasnmp -ldmimi -ldmi -lssagent -lgen -lnsl -Bstatic -lrwtool -Bdynamic -lthread -lC -lc

LINKLIBDIR = $(LDLIBS) -L../../dmisp \
	     -L../../../snmp/snmplib \
	     -L../../../snmp/agent

CFLAGS = -O -DDEBUG -DSVR4 -D_REENTRANT -DSOLARIS2 $(INCLUDES)

PURIFY_HOME= /net/suntools/export/tools/sparc/bin
PURIFY= $(PURIFY_HOME)/purify
 
LINT = lint
 
.KEEP_STATE:
 
FILEMODE = 0755
OWNER = root
GROUP = sys
 
PROG=$(BINPROG)
CLOBBERFILES= $(SOURCES.c)
 
# Targets
 
all: $(BINPROG)
 
install: all $(SABINPROG)

$(BINPROG): $(BIN) ../../dmisp/libdmi.$(SHAREDSUFFIX).$(SHAREDVER) ../../dmisp/libdmimi.$(SHAREDSUFFIX).$(SHAREDVER) ../../../snmp/agent/libssagent.$(SHAREDSUFFIX).$(SHAREDVER) ../../../snmp/snmplib/libssasnmp.$(SHAREDSUFFIX).$(SHAREDVER)  $(OBJECTS)
	$(CCC) -ptr$(OBJ_DIR) -o $(BINPROG) $(LDOPTIONS) \
		$(OBJECTS) $(LINKLIBDIR)  $(MAPPERLIBS)


dmisa.c: $(DMISA_INCLUDES)

dmisamap.c: $(DMISAMAP_INCLUDES)

dmisaind.c: $(DMISAIND_INCLUDES)


#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) $(MYLIBS) $(LIBS)

#
# directories
#
 
$(BIN):
	@$(TEST) -d $@ || mkdir $@
 

clean:
	$(RM) $(BINPROG) $(OBJECTS)

clobber: clean


realclobber: clean
	sccs clean


