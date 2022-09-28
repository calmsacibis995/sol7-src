#
#ident	"@(#)Makefile.com	1.1	97/09/19 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/librac/Makefile
#
LIBRARY= librac.a
VERS = .1

# objects are listed by source directory

RPC=  \
clnt_generic.o clnt_dg.o rac.o clnt_vc.o  rpcb_clnt.o xdr_rec_subr.o xdr_rec.o


OBJECTS= $(RPC)

# librac build rules

objs/%.o profs/%.o pics/%.o: ../rpc/%.c
	$(COMPILE.c) -DPORTMAP -DNIS  -o $@  $<
	$(POST_PROCESS_O)

# include library definitions
include ../../Makefile.lib

MAPFILE=	../rpc/mapfile-vers

LIBS += $(DYNLIB)

LDLIBS += -lnsl -ldl -lc
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:

$(DYNLIB):	$(MAPFILE)

include ../../Makefile.targ
