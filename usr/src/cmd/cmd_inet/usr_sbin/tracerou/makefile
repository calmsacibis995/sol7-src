#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#


# Generated automatically from Makefile.in by configure.
#  Copyright (c) 1988, 1989, 1990, 1991, 1992, 1995, 1996, 1997
# 	The Regents of the University of California.  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that: (1) source code distributions
#  retain the above copyright notice and this paragraph in its entirety, (2)
#  distributions including binary code include the above copyright notice and
#  this paragraph in its entirety in the documentation or other materials
#  provided with the distribution, and (3) all advertising materials mentioning
#  features or use of this software display the following acknowledgement:
#  ``This product includes software developed by the University of California,
#  Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
#  the University nor the names of its contributors may be used to endorse
#  or promote products derived from this software without specific prior
#  written permission.
#  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# @(#) $Header: Makefile.in,v 1.24 97/04/22 13:31:20 leres Exp $ (LBL)



#pragma ident   "@(#)Makefile 1.1     98/01/12 SMI" 



PROG=		traceroute
OBJS=		traceroute.o ifaddrlist.o 
SUIDPROG=	traceroute


SRCS=		$(OBJS:.o=.c)
HDRS=		ifaddrlist.h

include ../../../Makefile.cmd

$(ROOTUSRSBIN)/traceroute	:=	FILEMODE= 04555
$(ROOTUSRSBIN)/traceroute	:=	OWNER= root



CPPFLAGS += -O -I.
LDLIBS += -lsocket -lnsl  	

.KEEP_STATE:

.INIT:	$(HDRS)


all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:	
	$(RM) $(OBJS) tags

lint:	lint_SRCS

tags:	$(SRCS)
	ctags $(SRCS)

include ../../../Makefile.targ

