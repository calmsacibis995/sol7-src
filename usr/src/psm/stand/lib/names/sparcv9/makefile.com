#
#pragma	ident	"@(#)Makefile.com	1.1	97/06/30 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/lib/names/sparcv9/Makefile.com
#
# SPARCv9 architecture Makefile for Standalone Library
# Platform-specific, but shared between platforms.
# Firmware dependent.
#

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PSMSYSHDRDIR =	$(TOPDIR)/psm/stand

LIBNAMES =	libnames.a
LINTLIBNAMES =	llib-lnames.ln

# ARCHCMNDIR - common code for several machines of a given isa
# OBJSDIR - where the .o's go

ARCHCMNDIR =	../../sparc/common
OBJSDIR =	objs

CMNSRCS =	uname-i.c uname-m.c cpu2name.c mfgname.c
NAMESRCS =	$(PLATSRCS) $(CMNSRCS)
NAMEOBJS =	$(NAMESRCS:%.c=%.o)

OBJS =		$(NAMEOBJS:%=$(OBJSDIR)/%)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS =	$(CMNSRCS:%=$(ARCHCMNDIR)/%) $(PLATSRCS)

CPPINCS +=	-I$(SRC)/uts/common
CPPINCS +=	-I$(SRC)/uts/sun
CPPINCS +=	-I$(SRC)/uts/sparc
CPPINCS +=	-I$(SRC)/uts/sparc/$(ARCHVERS)
CPPINCS +=	-I$(SRC)/uts/$(PLATFORM)
CPPINCS += 	-I$(ROOT)/usr/include/$(ARCHVERS)
CPPINCS += 	-I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS += 	-I$(PSMSYSHDRDIR)
CPPFLAGS =	$(CPPINCS) $(CPPFLAGS.master) $(CCYFLAG)$(PSMSYSHDRDIR)
CPPFLAGS +=	-D_KERNEL
ASFLAGS =	-P -D__STDC__ -D_ASM $(CPPFLAGS.master) $(CPPINCS)
CFLAGS +=	-v

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBNAMES) .WAIT

lint: $(LINTLIBNAMES)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBNAMES) $(LINTLIBNAMES) a.out core

$(LIBNAMES): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBNAMES): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o names $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(ARCHCMNDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(ARCHCMNDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@
