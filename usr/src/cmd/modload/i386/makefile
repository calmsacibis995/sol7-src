#
#ident	"@(#)Makefile	1.2	97/06/08 SMI"
#
# Copyright (c) 1991-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/modload/i386/Makefile
#
# makefile for loadable module utilities

# add_drv 
PROG = add_drv

include ../../Makefile.cmd

# Generic

SRCS =  $(PROG:%=%.c)
OBJECTS = $(SRCS:%.c=%.o)

ROOTDRVPROG = $(DRVPROG:%=$(ROOTUSRSBIN)/%)
DRVCOMMONOBJ = drvsubr.o
DRVCOMMONSRC = $(DRVCOMMONOBJ:%.o=../%.c)

CLOBBERFILES = $(PROG) 

# install specifics

OWNER = root
$(ROOTDRVPROG) := FILEMODE = 0555

add_drv:= LDLIBS += -ldevinfo -lelf -ldl

.KEEP_STATE:

all: $(PROG) 

add_drv:	add_drv.o $(DRVCOMMONOBJ)
	$(LINK.c)  -o $@ $< add_drv.o $(DRVCOMMONOBJ) $(LDLIBS)

install: all $(ROOTUSRSBINPROG32)

clean:
	$(RM) $(OBJECTS) $(DRVCOMMONOBJ) core

lint: lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

include ../../Makefile.targ
