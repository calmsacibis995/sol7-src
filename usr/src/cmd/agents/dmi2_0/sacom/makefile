#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.52	97/12/08 SMI"

#
# dmi2.0/sacom/Makefile
#

include ../../Makefile.vars

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

TARG_SYS:sh = \
 (                                      \
   UNAME=` uname -r 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo SUNOS;;                     \
   4.*) echo SUNOS;;                    \
   5.*) echo SVR4;;                     \
   *)  echo SVR4;;                      \
   esac                                 \
 )


ARCH_DEF:sh = \
  (                                     \
   UNAME=` uname -p 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo ;;                           \
   i386) echo i386;;			\
   sparc) echo sparc;;			\
   *) echo ;;                           \
   esac                                 \
  )


#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#

AGENT1 = init.snmpdx
AGENT2 = snmpdx.rsrc
AGENT3 = snmpdx.reg
AGENT4 = snmpdx.acl
AGENT5 = snmpd.conf
AGENT6 = mibiisa.reg
AGENT7 = init.dmi
AGENT8 = snmpXdmid.conf
AGENT9 = dmispd.conf
AGENT10 = enterprises.oid
AGENT11 = test.map
AGENT12 = mibiisa.rsrc
AGENT13 = sp.mif


INIT_DESTDIR = $(ROOT)/etc/init.d
SNMP_CONF_DESTDIR = $(ROOT)/etc/snmp/conf
DMI_CONF_DESTDIR = $(ROOT)/etc/dmi/conf
OID_CONF_DESTDIR = $(ROOT)/etc/snmp/conf
MAP_TEST_DESTDIR = $(ROOT)/var/dmi/map
DMI_MIF_DESTDIR = $(ROOT)/var/dmi/mif

#
# The following directories are called DESTDIR's, but
# nothing gets installed there. They must be created
# for the BFU system to come up.
#

DMI_DB_DESTDIR = $(ROOT)/var/dmi/db
SNMP_DESTDIR = $(ROOT)/var/snmp


AGENT1PROG = $(AGENT1:%=$(INIT_DESTDIR)/%)
AGENT2PROG = $(AGENT2:%=$(SNMP_CONF_DESTDIR)/%)
AGENT3PROG = $(AGENT3:%=$(SNMP_CONF_DESTDIR)/%)
AGENT4PROG = $(AGENT4:%=$(SNMP_CONF_DESTDIR)/%)
AGENT5PROG = $(AGENT5:%=$(SNMP_CONF_DESTDIR)/%)
AGENT6PROG = $(AGENT6:%=$(SNMP_CONF_DESTDIR)/%)
AGENT7PROG = $(AGENT7:%=$(INIT_DESTDIR)/%)
AGENT8PROG = $(AGENT8:%=$(DMI_CONF_DESTDIR)/%)
AGENT9PROG = $(AGENT9:%=$(DMI_CONF_DESTDIR)/%)
AGENT10PROG = $(AGENT10:%=$(OID_CONF_DESTDIR)/%)
AGENT11PROG = $(AGENT11:%=$(MAP_TEST_DESTDIR)/%)
AGENT12PROG = $(AGENT12:%=$(SNMP_CONF_DESTDIR)/%)
AGENT13PROG = $(AGENT13:%=$(DMI_MIF_DESTDIR)/%)

$(AGENT1PROG) := FILEMODE = 744
$(AGENT7PROG) := FILEMODE = 744

$(AGENT13PROG) := FILEMODE = 0644

$(AGENT4PROG) := RFILEMODE = 0600
$(AGENT5PROG) := RFILEMODE = 0600

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<
INS.rfile = $(INS) -s -m $(RFILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(INIT_DESTDIR)/%: %
	@$(TEST) -d $(INIT_DESTDIR) || mkdir -p $(INIT_DESTDIR)
	@$(TEST) -d $(ROOT)/etc/rc2.d || mkdir -p $(ROOT)/etc/rc2.d
	@$(TEST) -d $(ROOT)/etc/rc3.d || mkdir -p $(ROOT)/etc/rc3.d
	@$(TEST) -d $(ROOT)/var/snmp || mkdir -p $(ROOT)/var/snmp
	$(INS.file)

$(DMI_CONF_DESTDIR)/%: %
	@$(TEST) -d $(DMI_CONF_DESTDIR) || mkdir -p $(DMI_CONF_DESTDIR)
	chgrp $(GROUP) $(ROOT)/etc/dmi
	chgrp $(GROUP) $(DMI_CONF_DESTDIR)
	$(INS.rfile)

$(OID_CONF_DESTDIR)/%: %
	@$(TEST) -d $(OID_CONF_DESTDIR) || mkdir -p $(OID_CONF_DESTDIR)
	chgrp $(GROUP) $(ROOT)/etc/snmp
	chgrp $(GROUP) $(OID_CONF_DESTDIR)
	$(INS.rfile)

$(MAP_TEST_DESTDIR)/%: %
	@$(TEST) -f $(MAP_TEST_DESTDIR) || mkdir -p $(MAP_TEST_DESTDIR)
	chgrp $(GROUP) $(ROOT)/var/dmi
	chgrp $(GROUP) $(MAP_TEST_DESTDIR)
	@$(TEST) -f $(DMI_DB_DESTDIR) || mkdir -p $(DMI_DB_DESTDIR)
	chgrp $(GROUP) $(DMI_DB_DESTDIR)
	@$(TEST) -f $(SNMP_DESTDIR) || mkdir -p $(SNMP_DESTDIR)

	$(INS.rfile)

$(SNMP_CONF_DESTDIR)/%: %
	@$(TEST) -d $(SNMP_CONF_DESTDIR) || mkdir -p $(SNMP_CONF_DESTDIR)
	chgrp $(GROUP) $(ROOT)/etc/snmp
	chgrp $(GROUP) $(SNMP_CONF_DESTDIR)
	$(INS.rfile)

$(DMI_MIF_DESTDIR)/%: %
	@$(TEST) -d $(DMI_MIF_DESTDIR) || mkdir -p $(DMI_MIF_DESTDIR)
	chgrp $(GROUP) $(ROOT)/var/dmi
	chgrp $(GROUP) $(DMI_MIF_DESTDIR)
	$(INS.file)


SOURCES.cc=

SOURCES.h=

OBJECTS= $(SOURCES.cc:%.cc=$(OBJ_DIR)/%.o)

CCFLAGS += -ptr$(OBJ_DIR) -G -DSVR4 -D_REENTRANT -DSOLARIS2 -I../include   -c

LDFLAGS +=

LINT=	lint

PURIFY_HOME= /usr/soft/purify3.0

PURIFY= $(PURIFY_HOME)/purify

#
# Input file locations
#

INIT_DIR = ../../root/etc/init.d
SNMP_CONF_DIR = $(SRC)/cmd/agents/snmp/snmprelayd
AGENT_CONF_DIR = $(SRC)/cmd/agents/agents/snmpd/etc
DMI_MAPPER_CONF_DIR = $(SRC)/cmd/agents/dmi2.0/mapper/src
DMI_CONF_DIR = $(SRC)/cmd/agents/dmi2.0/conf
OID_CONF_DIR = $(SRC)/cmd/agents/schemas/agents
MAP_TEST_DIR = $(SRC)/cmd/agents/dmi2.0/mapper/src
DMI_MIF_DIR = $(SRC)/cmd/agents/dmi2.0/dmidb

 
.KEEP_STATE:

FILEMODE = 0755
RFILEMODE = 0644
OWNER = root
GROUP = sys

PROG=  
CLOBBERFILES= $(SOURCES.c)

#######################################################

#
# all
#

all: $(BIN) $(AGENT1) $(AGENT2) $(AGENT3) $(AGENT4) $(AGENT5) $(AGENT6) $(AGENT7) $(AGENT8) $(AGENT9) $(AGENT10) $(AGENT11) $(AGENT12) $(AGENT13)

$(AGENT1): $(INIT_DIR)/$(AGENT1)
	cp $(INIT_DIR)/$(AGENT1) .

$(AGENT2): $(SNMP_CONF_DIR)/$(AGENT2)
	cp $(SNMP_CONF_DIR)/$(AGENT2) .

$(AGENT3): $(SNMP_CONF_DIR)/$(AGENT3)
	cp $(SNMP_CONF_DIR)/$(AGENT3) .

$(AGENT4): $(SNMP_CONF_DIR)/$(AGENT4)
	cp $(SNMP_CONF_DIR)/$(AGENT4) .

$(AGENT5): $(AGENT_CONF_DIR)/$(AGENT5)
	cp $(AGENT_CONF_DIR)/$(AGENT5) .

$(AGENT6): $(AGENT_CONF_DIR)/$(AGENT6)
	cp $(AGENT_CONF_DIR)/$(AGENT6) .

$(AGENT7): $(INIT_DIR)/$(AGENT7)
	cp $(INIT_DIR)/$(AGENT7) .

$(AGENT8): $(DMI_MAPPER_CONF_DIR)/$(AGENT8)
	cp $(DMI_MAPPER_CONF_DIR)/$(AGENT8) .

$(AGENT9): $(DMI_CONF_DIR)/$(AGENT9)
	cp $(DMI_CONF_DIR)/$(AGENT9) .

$(AGENT10): $(OID_CONF_DIR)/$(AGENT10)
	cp $(OID_CONF_DIR)/$(AGENT10) .

$(AGENT11): $(MAP_TEST_DIR)/$(AGENT11)
	cp $(MAP_TEST_DIR)/$(AGENT11) .

$(AGENT12): $(SNMP_CONF_DIR)/$(AGENT12)
	cp $(SNMP_CONF_DIR)/$(AGENT12) .

$(AGENT13): $(DMI_MIF_DIR)/$(AGENT13)
	cp $(DMI_MIF_DIR)/$(AGENT13) .

install: $(BIN) all $(AGENT1PROG) $(AGENT2PROG) $(AGENT3PROG) $(AGENT4PROG) $(AGENT5PROG) $(AGENT6PROG) $(AGENT7PROG) $(AGENT8PROG) $(AGENT9PROG) $(AGENT10PROG) $(AGENT11PROG) $(AGENT12PROG) $(AGENT13PROG)
	@for dir in rc0.d rcS.d rc1.d rc2.d; do \
		echo cd $(ROOT)/etc/$$dir; \
		cd $(ROOT)/etc/$$dir; \
		echo $(LN) ../init.d/init.snmpdx K07snmpdx; \
		$(RM) K07snmpdx; $(LN) ../init.d/init.snmpdx K07snmpdx; \
		echo $(LN) ../init.d/init.dmi K07dmi; \
		$(RM) K07dmi; $(LN) ../init.d/init.dmi K07dmi; \
	done
	cd $(ROOT)/etc/rc3.d; $(RM) S76snmpdx; $(LN) ../init.d/init.snmpdx S76snmpdx
	cd $(ROOT)/etc/rc3.d; $(RM) S77dmi; $(LN) ../init.d/init.dmi S77dmi


#
# lint
#

lint:

#
# purify
#

purify:

#
# clean
#

clean:
	$(RM) $(AGENT1) $(AGENT2) $(AGENT3) $(AGENT4) $(AGENT5) \
	      $(AGENT6) $(AGENT7) $(AGENT8) $(AGENT9) $(AGENT10) \
	      $(AGENT11) $(AGENT12) $(AGENT13)

realclobber: clean
	sccs clean

$(BIN):
	@test -d $(BIN) || mkdir -p $(BIN)

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
