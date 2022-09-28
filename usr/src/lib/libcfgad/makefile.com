#
#ident	"@(#)Makefile.com	1.3	98/01/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libcfgadm/Makefile
#

LIBRARY= libcfgadm.a
VERS= .1

PICS= pics/config_admin.o

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

OBJECTS= config_admin.o

# include library definitions
include ../../Makefile.lib

MAPFILE= ../common/mapfile-vers
SRCS=           $(OBJECTS:%.o=../common/%.c)

DYNFLAGS += -M $(MAPFILE)
DYNFLAGS += -Wl,-f/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)
#DYNFLAGS += -Wl,-f$(ROOT)/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)

ROOTDYNLIBS= $(DYNLIB:%=$(ROOTLIBDIR)/%)

LIBS = $(DYNLIB) $(LINTLIB)		# Make sure we don't build a static lib

$(LINTLIB):= SRCS=../common/llib-lcfgadm
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%) $(LINTLIB:%=$(ROOTLINTDIR)/%)

ZDEFS=

.KEEP_STATE:

all: $(TXTS) $(LIBS)

$(DYNLIB): $(MAPFILE)

# include library targets
include ../../Makefile.targ

# lint source file installation target

$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
