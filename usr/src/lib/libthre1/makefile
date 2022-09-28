#
#ident	"@(#)Makefile	1.14	97/10/30 SMI"
#
# Copyright (c) 1993-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_db/Makefile
#

LIBRARY= libthread_db.a
VERS= .0

OBJECTS=  \
thread_db.o

# libsocket build rules
#objs/%.o profs/%.o pics/%.o: inet/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)
#
#objs/%.o profs/%.o pics/%.o: socket/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

SRCS=	$(OBJECTS:%.o=%.c)

ZDEFS=
THR = -I../libthread
CPPFLAGS += $(THR)/inc $(THR)/common $(THR)/$(MACH) $(THR)/$(MACH)/ml $(THR)/sys/inc
LIBS += $(DYNLIB)
LDLIBS += -ldl -lc
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)
	(cd $(MACH); $(MAKE) all;)
	$(BUILD64)(cd $(MACH64); $(MAKE) all;)

install: all $(ROOTLIBS) $(ROOTLINKS)
	(cd $(MACH); $(MAKE) install;)
	$(BUILD64)(cd $(MACH64); $(MAKE) install;)

$(DYNLIB): $(MAPFILE)

clean:	clean_mach

clean_mach:
	(cd $(MACH); $(MAKE) clean;)
	$(BUILD64)(cd $(MACH64); $(MAKE) clean;)

# include library targets
include ../Makefile.targ

lint: 

