#
#ident	"@(#)Makefile	1.35	98/01/12 SMI"	/* SVr4.0 %I	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/sendmail/aux/Makefile
#
include		../../Makefile.cmd

PROG= 		mailstats mconnect vacation mailcompat aliasadm

LIBPROG=	mail.local

USRSBINPROG=	makemap

# $(PROG) by default
CLOBBERFILES=	$(LIBPROG) $(USRSBINPROG)

OBJS= 		$(PROG:%=%.o) nisplus.o nisedit.o

SRCS= 		$(PROG:%=%.c) nisplus.c nisedit.c

OTHERSRC= 	../src/convtime.c

OTHEROBJ=	$(OTHERSRC:../src/%.c=%.o)

include		../../Makefile.cmd

LDLIBS +=	-lsocket -lnsl

NISLIB = # comment this back in when ZNS works -lnis

INCPATH=	-I../src -I../db/include

ENVDEF=		-DSYSV -DSOLARIS=20700 -DNOT_SENDMAIL
SUNENVDEF=	-DSUN_EXTENSIONS -DUSE_VENDOR_CF_PATH
DBMDEF=		-DNDBM

CPPFLAGS =	$(INCPATH) $(ENVDEF) $(SUNENVDEF) $(DBMDEF) $(CPPFLAGS.master)

# build rule
%.o: %.c
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

.KEEP_STATE:

all:		$(PROG) $(LIBPROG) $(USRSBINPROG)

safefile.o:	../src/safefile.c
	$(COMPILE.c) ../src/safefile.c
	$(POST_PROCESS_O)

vacation:	vacation.o convtime.o
	$(LINK.c) vacation.o convtime.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

aliasadm:	aliasadm.o nisplus.o nisedit.o
	$(LINK.c) aliasadm.o nisplus.o nisedit.o -o $@ $(NISLIB) $(LDLIBS)
	$(POST_PROCESS)

mail.local:	mail.local.o
	$(LINK.c) mail.local.o -o $@ $(LDLIBS) -lmail
	$(POST_PROCESS)

makemap:	makemap.o safefile.o
	$(LINK.c) makemap.o safefile.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(ROOTLIB)/mail.local $(ROOTUSRSBIN)/makemap

clean:
	$(RM) $(OBJS) $(OTHEROBJ) *.o

lint:		lint_PROG



include	../../Makefile.targ
