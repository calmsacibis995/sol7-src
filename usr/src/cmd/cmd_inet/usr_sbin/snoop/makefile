#
#ident	"@(#)Makefile	1.11	98/01/26 SMI"	/* SunOS	*/
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=	snoop
OBJS=	dlprims.o snoop.o snoop_arp.o snoop_bparam.o \
	snoop_capture.o snoop_dhcp.o snoop_display.o snoop_dns.o snoop_ether.o \
	snoop_filter.o snoop_http.o snoop_icmp.o snoop_ip.o snoop_ipaddr.o \
	snoop_mount.o snoop_nfs.o snoop_nfs3.o snoop_nfs_acl.o \
	snoop_nis.o snoop_nisplus.o snoop_nlm.o snoop_ntp.o snoop_pf.o \
	snoop_pmap.o snoop_rip.o snoop_rpc.o snoop_rpcprint.o snoop_rpcsec.o \
	snoop_rport.o snoop_rquota.o snoop_rstat.o snoop_solarnet.o \
	snoop_tcp.o snoop_tftp.o snoop_udp.o

SRCS=	$(OBJS:.o=.c)
HDRS=	snoop.h

include	../../../Makefile.cmd

CPPFLAGS += -I.
LDLIBS += -ldhcp -lsocket -lnsl

.KEEP_STATE:

.INIT: $(HDRS)


all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install:	all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS) tags

lint:	lint_SRCS

tags:	$(SRCS)
	ctags $(SRCS)

include	../../../Makefile.targ
