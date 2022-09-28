#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	97/12/23 SMI"
#
# lib/libw/sparcv9/Makefile

include ../Makefile.com
include ../../Makefile.lib.64

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

OBJECTS = empty.o
SRCS= $(OBJECTS:%.o=../common/%.c)

LIBS =	$(DYNLIB)

DYNFLAGS +=	-F libc.so.1 $(MAPOPTS)

CLOBBERFILES = $(LIBS) $(ROOTLIBS64) $(ROOTLINKS64)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)

lint:

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $< 2>&1 | fgrep -v "empty translation unit" \
				   | cat
	$(POST_PROCESS_O)
