#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.6	97/11/06 SMI"
#
# cmd/gss/gsscred/Makefile

SBINPROG = gsscred

INCDIRS = -I. -I$(ROOT)/usr/include

LIBPATH = -L$(ROOT)/usr/lib

PROG= $(SBINPROG)

GSSCREDOBJS = gsscred.o gsscred_utils.o gsscred_xfn.o gsscred_file.o

OBJS	= $(GSSCREDOBJS)
SRCS	= $(OBJS:.o=.c)

include ../../Makefile.cmd

ROOTBINPROG=		$(BINPROG:%=$(ROOTBIN)/%)
ROOTSBINPROG=		$(SBINPROG:%=$(ROOTUSRSBIN)/%)

$(ROOTUSRSBIN)/gsscred :=	OWNER=	root
$(ROOTUSRSBIN)/gsscred :=	GROUP=	sys

COPTFLAG += -xs $(INCDIRS) $(LIBPATH)

LDLIBS += -lgss -lxfn -lnsl -lsocket -ldl

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

gsscred:	$(OBJS) 
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)


install: all $(DIRS) $(ROOTBINPROG) $(ROOTSBINPROG)

install_h:

clean: 
	$(RM) $(OBJS)
	$(RM) $(PROG)

lint: lint_SRCS

include ../../Makefile.targ
