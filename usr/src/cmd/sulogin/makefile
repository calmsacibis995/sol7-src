#
#ident	"@(#)Makefile	1.32	97/07/10 SMI"
#
# Copyright (c) 1993, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= sulogin

include ../Makefile.cmd

CPPFLAGS += -DSECURITY -I../../lib/libcmd/inc

#
LDLIBS_LIST = -lcrypt_i -lpam -lmp -lcmd \
	-lnss_files -lnss_nis -lnss_nisplus -lnss_compat -lnss_dns \
	-lsocket -lnsl -lstraddr

LDLIBS = -Bstatic -lc $(LDLIBS_LIST) -lgen -Bdynamic -ldl -Bstatic

LDFLAGS = $(LDLIBS.cmd) -dy -R/etc/lib $(LDFLAGS.cmd)

FILEMODE = 0555
OWNER = root
GROUP = bin

.KEEP_STATE:

all: $(PROG)

$(PROG): $(PROG).c
	LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) $(PROG).c -o $@ `nm $(ROOT)/etc/lib/nss_files.so.1 |egrep UNDEF | egrep -v 'WEAK|\.' | awk -F'|' '{print "-u " $$NF}' ` $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTSBINPROG)
	$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../sbin/$(PROG) $(ROOTETCPROG)


clean:

lint:	lint_PROG

include ../Makefile.targ
