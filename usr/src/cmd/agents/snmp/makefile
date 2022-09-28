# Copyright 09/29/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.27 96/09/29 Sun Microsystems"

#
# snmp/Makefile
#

# OPTIONAL_INCLUDE_FILE1:sh = /bin/sh ../misc.sh ../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE1)

include $(SRC)/cmd/Makefile.cmd

 
#
# all
#

all:
	cd snmplib; make all
	cd agent; make all
	cd parser; make all
	cd snmprelayd; make all
	cd trapsend; make all
	cd mib; make all

install:
	cd snmplib; make install
	cd agent; make install
	cd parser; make install
	cd snmprelayd; make install
	cd trapsend; make install
	cd mib; make install

#
# clean
#

clean:
	cd snmplib; make clean
	cd agent; make clean
	cd parser; make clean
	cd snmprelayd; make clean
	cd trapsend; make clean
	cd mib; make clean

# Need to include a lint target for Solaris 2.6 integration

lint:
	cd snmplib; make lint
	cd agent; make lint
	cd parser; make lint
	cd snmprelayd; make lint
	cd trapsend; make lint
	cd mib; make lint

# Need a realclobber target with sccs clean

realclobber:
	cd snmplib; make realclobber
	cd agent; make realclobber
	cd parser; make realclobber
	cd snmprelayd; make realclobber
	cd trapsend; make realclobber
	cd mib; make realclobber


# OPTIONAL_INCLUDE_FILE2:sh = /bin/sh ../misc.sh ../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE2)

include $(SRC)/cmd/Makefile.targ
