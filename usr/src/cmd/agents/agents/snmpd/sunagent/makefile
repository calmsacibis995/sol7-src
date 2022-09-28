# Copyright 01/13/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.79 97/01/13 Sun Microsystems"

#
# snmpd/sunagent/Makefile
#

include ../../../Makefile.vars

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

# This makefile determines which OS it is running under and executes
# the appropriate Makefile
 
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
   '')echo ;;                           \
   i386)echo -Di386;;                   \
   sparc) echo;;                        \
   *) echo ;;                           \
   esac                                 \
  )
 
 
OS_VER:sh = \
 (                                      \
   UNAME=` uname -r 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo ;;                          \
   4.*) echo ;;                         \
   5.0) echo ;;                         \
   5.1) echo ;;                         \
   5.2) echo ;;                         \
   5.3) echo ;;                         \
   5.4) echo -DSOL24HIGHER;;            \
   5.5) echo -DSOL24HIGHER -DSOL25HIGHER;;            \
   5.5.1) echo -DSOL24HIGHER -DSOL25HIGHER;;    \
   5.6) echo -DSOL24HIGHER -DSOL25HIGHER -DSOL26HIGHER;;            \
   *)   echo ;;                         \
   esac                                 \
 )

#
# In order to conform with the Solaris 2.6 way of packaging things,
# we now need to put any deliverable that is specified in the
# prototype file into a common "proto" area, so that their
# packaging scheme knows where to obtain the input binaries.
#

AGENT = mibiisa
BINPROG = $(AGENT)

MIBAGENT = sun.mib
LIBPROG = $(MIBAGENT)
SAOPTBIN = $(ROOT)/usr/lib/snmp
SAOPTMIB = $(ROOT)/var/snmp/mib
SAOPTBINPROG = $(PROG:%=$(SAOPTBIN)/%)
SAOPTMIBPROG = $(MIBAGENT:%=$(SAOPTMIB)/%)

INS.file = $(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<
INS.rfile = $(INS) -s -m $(RFILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

$(SAOPTBIN)/%: %
	@$(TEST) -d $(SAOPTBIN) || mkdir -p $(SAOPTBIN)
	chgrp $(GROUP) $(SAOPTBIN)
	$(INS.file)

$(SAOPTMIB)/%: %
	@$(TEST) -d $(SAOPTMIB) || mkdir -p $(SAOPTMIB)
	chgrp $(GROUP) $(SAOPTMIB)
	$(INS.rfile)

SOURCES.c=\
	agent.c	\
	arp-svr4.c \
	contbl-svr4.c \
	general-svr4.c \
	icmp-svr4.c \
	if-svr4.c \
	ip-svr4.c \
	iprte-svr4.c \
	libfuncs.c \
	main.c \
	mib.c \
	pstable-svr4.c \
	read_con.c \
	rstat.c \
	sendtrap.c \
	setup-svr4.c \
	snmpvars.c \
	sys-svr4.c \
	tcp-svr4.c \
	udp-svr4.c

SOURCES.h=\
	agent.h \
	general.h \
	patchlevel.h \
	snmpvars.h

OBJECTS= $(SOURCES.c:%.c=static$(TARGET_ARCH)/%.o)

INCDIRS = -I../h
NEWDEF = -DBOGEY -DSVR4
DEFINES = -DNO_STDLIB -DNO_PP -DSTRNICMP -DBSD_RELEASE=43 -DSGRP -DOIDC_32
CFLAGS += -c $(NEWDEF) $(DEFINES) $(DDEBUG) $(INCDIRS) $(ARCH_DEF) $(OS_VER)

CEES = main.c
LIBCEES = agent.c arp-svr4.c contbl-svr4.c icmp-svr4.c \
         if-svr4.c ip-svr4.c iprte-svr4.c  general-svr4.c \
        libfuncs.c mib.c read_con.c sendtrap.c setup-svr4.c snmpvars.c  \
        sys-svr4.c tcp-svr4.c udp-svr4.c pstable-svr4.c rstat.c
 
LIBOBJS = $(LIBCEES:%.c=static$(TARGET_ARCH)/%.o)
COBJS = $(CEES:%.c=static$(TARGET_ARCH)/%.o)
HFILES = agent.h general.h snmpvars.h
LIBNAME = static$(TARGET_ARCH)/libsun.a
SNMPLIBDIR = ../lib/static$(TARGET_ARCH)
SUNLIBDIR = ./static$(TARGET_ARCH)
LDFLAGS += -L$(ROOT)/usr/lib -L$(SNMPLIBDIR) -L$(SUNLIBDIR) \
	-lsnmp -lsun -lrpcsvc -lnsl -lkstat -lsocket

LINT=	lint

.KEEP_STATE:

#
# Input directories within our build area
#

BINDIR = $(SRC)/cmd/agents/agents/snmpd/sunagent/bin-sparc
MIBDIR = $(SRC)/cmd/agents/agents/snmpd/etc

FILEMODE = 0755
RFILEMODE = 0644
OWNER = root
GROUP = sys

PROG=$(BINPROG) 
CLOBBERFILES= $(SOURCES.c)

# Targets

all: $(BIN) $(SOURCES.h) $(BINPROG)
	touch $(MIBAGENT)


install: all $(SAOPTBINPROG) $(SAOPTMIBPROG)

$(BINPROG): $(BIN) $(LIBNAME) $(COBJS)
	$(CC) -o $(AGENT) $(LDFLAGS) $(COBJS) $(LIBNAME) $(LIBS) ../lib/static$(TARGET_ARCH)/libsnmp.a $(LIBNAME) ../lib/static$(TARGET_ARCH)/libsnmp.a

static$(TARGET_ARCH)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFINES) $*.c -o $@

$(LIBNAME):     static$(TARGET_ARCH) $(LIBOBJS) $(HFILES)
	$(RM) $(LIBNAME)
	($(AR) -r $(LIBNAME) ${LIBOBJS})


#
# lint
#

lint:
	$(LINT) $(LDFLAGS) $(CPPFLAGS) $(DEFINES) $(SOURCES.c) $(MYLIBS) $(LIBS)

#
# directories
#

$(BIN) static$(TARGET_ARCH):
	@$(TEST) -d $@ || mkdir $@

#
# purify
#

purify:
	$(PURIFY) $(CC) -o $(AGENT) $(LDFLAGS) $(OBJECTS) $(MYLIBS) $(LIBS)


#
# clean
#

clean:
	$(RM) $(OBJECTS) $(BINPROG)

realclobber: clean
	sccs clean

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../misc.sh ../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ
