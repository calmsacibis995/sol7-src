#
#ident	"@(#)Makefile	1.21	95/03/09 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librpcsvc/Makefile
#

LIBRARY= librpcsvc.a
VERS = .1

OBJECTS= rstat_simple.o rstat_xdr.o rusers_simple.o rusersxdr.o rusers_xdr.o \
	 rwallxdr.o spray_xdr.o nlm_prot.o sm_inter_xdr.o \
	 bootparam_prot_xdr.o mount_xdr.o rpc_sztypes.o bindresvport.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

LIBS += $(DYNLIB)


CPPFLAGS += -DYP
LDLIBS += -lnsl -lc
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:


$(DYNLIB): $(MAPFILE)

# include library targets
include ../../Makefile.targ
