#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	97/12/02 SMI"
#
# lib/watchmalloc/Makefile.com

LIBRARY = watchmalloc.a
VERS = .1

OBJECTS = malloc.o

# include library definitions
include ../../Makefile.lib

MAPFILE = ../common/mapfile-vers
SRCS = $(OBJECTS:%.o=../common/%.c)

LIBS = $(DYNLIB)
LDLIBS += -lc
CFLAGS += -v -I../common
CFLAGS64 += -v -I../common
CPPFLAGS += -D_REENTRANT
DYNFLAGS += -M $(MAPFILE)

LINTFLAGS = -mux -I../common
LINTFLAGS64 = -mux -D__sparcv9 -I../common

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB): $(MAPFILE)

# include library targets
include ../../Makefile.targ

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
