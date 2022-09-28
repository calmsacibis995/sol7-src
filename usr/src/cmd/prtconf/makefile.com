#
#ident	"@(#)Makefile.com	1.2	97/10/25 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/prtconf/Makefile.com

PROG=	prtconf
OBJS=	$(PROG).o pdevinfo.o prt_xxx.o
SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

#CFLAGS	+=	-v
#CFLAGS64 +=	-v
LDLIBS	+= -ldevinfo -lelf

OWNER= root
GROUP= sys
FILEMODE= 02555

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ

