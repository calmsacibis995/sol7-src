#
#ident	"@(#)Makefile	1.9	97/01/06 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# cmd/cmd-inet/dhcpc/sbin/Makefile
#

DHCCLNTLIBDIR = ../../../../lib/libdhcclnt
DCLIB = $(DHCCLNTLIBDIR)/libdhcclnt.a

OBJS = dhcpagent.o dhcpinfo.o
PROG = dhcpagent dhcpinfo

include ../../../Makefile.cmd

CPPFLAGS += -I$(DHCCLNTLIBDIR)
$(PROG) := LDLIBS += -Bdynamic -ldl -Bstatic $(DCLIB) -lsocket -lnsl -lelf -lc
$(PROG) := LDFLAGS = -dy -R/etc/lib $(LDFLAGS.cmd)

# Workaround for 4015159 (sparc optimizer bug)
sparc_COPTFLAG = -xO1

.KEEP_STATE:

all: $(PROG)

dhcpagent: dhcpagent.o $(DCLIB)
	LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) -o $@ $@.o $(LDLIBS)
	$(POST_PROCESS)

dhcpinfo: dhcpinfo.o $(DCLIB)
	LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) -o $@ $@.o $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTSBINPROG)

lint: dhcpagent.ln dhcpinfo.ln

dhcpagent.ln: dhcpagent.c
	$(LINT.c) dhcpagent.c -L$(DHCCLNTLIBDIR) -ldhcclnt -ldl -lsocket -lnsl -lelf -lc

dhcpinfo.ln: dhcpinfo.c
	$(LINT.c) dhcpinfo.c -L$(DHCCLNTLIBDIR) -ldhcclnt -ldl -lsocket -lnsl -lelf -lc

clean:
	$(RM) $(OBJS)

include ../../../Makefile.targ
