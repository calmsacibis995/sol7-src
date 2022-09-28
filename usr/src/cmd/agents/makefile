# Copyright 12/09/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.22 96/12/09 Sun Microsystems"

# cmd/agents/Makefile

#########################################################
#
# Makefile for the Solstice Enterprise Agent
#
# Targets:
#	all - compile everything
#	install - create installation directories and copy files
#	package - create packages in installation directory
#	clean - remove all .o's, binaries, libraries, detritus
#	clobber - make clean plus removing sources. Forces sccs get
#	lint - run lint on the source files
#
#########################################################

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh misc.sh ../Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

# Use $TARG_SYS for OS dependent variables
#
# TARG_SYS gets defined as SVR4 or SUNOS
#
TARG_SYS:sh =                           \
 (                                      \
   UNAME=` uname -r 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo SUNOS;;                     \
   4.*) echo SUNOS;;                    \
   5.*) echo SVR4;;                     \
   A.09.*) echo HPUX9;;                 \
   *)  echo SVR4;;                      \
   esac                                 \
 )

ROOTDIR	: sh=echo ${ROOTDIR:-"`pwd`"}
PKGLOC	: sh=echo ${PKGLOC:-'${ROOTDIR}/PACKAGES.`uname -p`'}

#
# all
#

all:
	cd snmp; make all
	cd dmi2.0; make all
	cd agents; make all

DIRMODE=	0775
OWNER=		root
GROUP=		sys
ROOTVAR=	$(ROOT)/var

ROOTDIRS=	$(ROOTETC)/dmi \
		$(ROOTETC)/dmi/ciagent \
		$(ROOTETC)/dmi/conf \
		$(ROOTETC)/snmp \
		$(ROOTETC)/snmp/conf \
		$(ROOTLIB)/dmi \
		$(ROOTLIB)/snmp \
		$(ROOTVAR)/dmi \
		$(ROOTVAR)/dmi/db \
		$(ROOTVAR)/dmi/map \
		$(ROOTVAR)/dmi/mif \
		$(ROOTVAR)/snmp \
		$(ROOTVAR)/snmp/mib

$(ROOTDIRS):
	$(INS.dir)

#
# Need a "make install" target for integrating into Solaris 2.6.
#

install: $(ROOTDIRS)
	cd snmp; make install
	cd dmi2.0; make install
	cd agents; make install
	

#
# clean
#

clean:
	cd snmp; make clean
	cd dmi2.0; make clean
	cd agents; make clean

# Need a lint target for Solaris2.6 integration.

lint:
	cd snmp; make lint
	cd dmi2.0; make lint
	cd agents; make lint

# We need a real make clobber with sccs clean.

realclobber:
	cd snmp; make realclobber
	cd dmi2.0; make realclobber
	cd agents; make realclobber

package:
	cd ../../pkgdefs/SUNWmibii; pwd; make install
	cd ../../pkgdefs/SUNWsacom; pwd; make install
	cd ../../pkgdefs/SUNWsadmi; pwd; make install
	cd ../../pkgdefs/SUNWsasnm; pwd; make install

include $(SRC)/cmd/Makefile.targ
