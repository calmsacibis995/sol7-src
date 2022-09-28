#
# Makefile for "ifconfig" 
# Copyright (c) 1992-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.22	97/05/22 SMI"
#

PROG =		ifconfig
OBJS=		ifconfig.o revarp.o
LIBDHCCLNT = 	../../../../lib/libdhcclnt/libdhcclnt.a
SRCS=		$(OBJS:%.o=%.c)

# duplicate ROOTSBIN value needed for installation rule
ROOTSBIN = $(ROOT)/sbin
$(ROOTSBIN)/%: %.ps
	$(INS.rename)

include ../../../Makefile.cmd

CPPFLAGS +=     -DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=       $(LIBDHCCLNT) -lsocket -lnsl -ldevinfo

# `PS[ps]' is meant to imply "partially static" because
# the version of ifconfig(1) that goes in $(ROOT)/sbin
# is dynamic wrt ld.so, but static wrt all others.
PS = $(PROG:%=%.ps)
$(PS) := LDLIBS = $(LDLIBS.cmd) $(NEEDU) $(LIBDHCCLNT) -Bdynamic -ldl -Bstatic -ldevinfo -lsocket -lnsl -lelf -lc
$(PROG) := LDLIBS += -ldl -lelf
$(PS) := LDFLAGS = -dy $(LDFLAGS.cmd)
ROOTPSPROG = $(PROG:%=$(ROOTSBIN)/%)
CLOBBERFILES += $(PS)
#
# We need to make these become externally available so
# that nss_files.so.1 (ps) can find them at run time.
NEEDU=		-u __ctype \
		-u isspace \
		-u llseek \
		-u memcmp \
		-u nlist \
		-u rewind \
		-u strcasecmp \
		-u strstr \
		-u vfprintf \
		-u __nss2herrno

.KEEP_STATE:

all:	$(PROG) $(PS)

$(PROG):$(OBJS) $(LIBDHCCLNT)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(PS):	$(OBJS) $(LIBDHCCLNT)
	LD_OPTIONS="-I/etc/lib/ld.so.1 -R/etc/lib" \
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install:	all $(ROOTUSRSBINPROG)  $(ROOTPSPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
