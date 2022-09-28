#
#ident	"@(#)Makefile	1.45	98/01/05 SMI"	/* SVr4.0 %I	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/sendmail/src/Makefile



PROG=	sendmail

include 	../../Makefile.cmd

# additional .o files needed
OBJADD=sun_compat.o
 
BASEOBJS= alias.o arpadate.o clock.o collect.o conf.o convtime.o daemon.o \
	deliver.o domain.o envelope.o err.o headers.o macro.o main.o \
	map.o mci.o mime.o parseaddr.o queue.o readcf.o recipient.o \
	safefile.o savemail.o srvrsmtp.o stab.o stats.o sysexits.o \
	trace.o udb.o usersmtp.o util.o version.o ${OBJADD}


OBJS=		$(BASEOBJS)

LDLIBS += 	-lkstat -lresolv -lsocket -lnsl

INCPATH=	-I.

ENVDEF=		-DSOLARIS=20700
SUNENVDEF=	-DSUN_EXTENSIONS -DVENDOR_DEFAULT=VENDOR_SUN \
		-DUSE_VENDOR_CF_PATH -DINTER -D_FFR_MAXALIASRECURSION_OPTION
DBMDEF=		-DNDBM -DNIS -DNISPLUS

CPPFLAGS = 	$(INCPATH) $(ENVDEF) $(SUNENVDEF) $(DBMDEF) $(CPPFLAGS.master)

FILEMODE= 	4551
OWNER=		root

ROOTSYMLINKS=	$(ROOTBIN)/newaliases $(ROOTBIN)/mailq 

# build rule
#

.KEEP_STATE:
all:		$(PROG)

$(PROG):	$(BASEOBJS)
	$(LINK.c) -o $@ $(BASEOBJS) $(LDLIBS)
	$(POST_PROCESS)

install:	$(ROOTLIBPROG) $(ROOTSYMLINKS)

$(ROOTSYMLINKS):
	$(RM) $@; $(SYMLINK) ../lib/sendmail $@

clean:
	$(RM) $(OBJS)

lint:		lint_PROG

include		../../Makefile.targ
