# Copyright 12/09/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.21 96/12/09 Sun Microsystems"

#########################################################
#
# Makefile for dmi2.0
#
#########################################################

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../misc.sh ../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

#
# all
#

all:
	cd dmisp; make all
	cd mapper/src; make all
	cd samples; make all
	cd utility; make all
	cd miftomib/src; make all
	cd sacom; make all
	cd ciagent; make all

install:
	cd dmisp; make install
	cd mapper/src; make install
#
# dmici and namedir in samples should not get put into proto area.
#
#	cd samples; make install
	cd utility; make install
	cd miftomib/src; make install
	cd sacom; make install
	cd ciagent; make install

lint:
	cd dmisp; make lint
	cd mapper/src; make lint
	cd samples; make lint
	cd utility; make lint
	cd miftomib/src; make lint
	cd sacom; make lint
	cd ciagent; make lint


#
# clean
#

clean:
	cd dmisp; make clean
	cd mapper/src; make clean
	cd samples; make clean
	cd utility; make clean
	cd miftomib/src; make clean
	cd sacom; make clean
	cd ciagent; make clean

realclobber:
	cd dmisp; make realclobber
	cd mapper/src; make realclobber
	cd samples; make realclobber
	cd utility; make realclobber
	cd miftomib/src; make realclobber
	cd sacom; make realclobber
	cd ciagent; make realclobber


# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../misc.sh ../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
