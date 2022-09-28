#
#ident	"@(#)Makefile	1.12	98/01/07 SMI"
#
# Copyright (c) 1995-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparc/sun4u/Makefile
#

TOPDIR =	../../../../..

# override global macros as necessary
TARGET_ARCH =	sun4u
TARG_MACH =     sparcv9
ARCH =		sun4u
MACH =		sparc
MMU =		sfmmu
ARCHVER =	v9
PROMTYPE =	ieee1275
ARCH_PROMDIR =	$(TARG_MACH)/$(PROMTYPE)/common
PROMIFDIR =	$(TOPDIR)/psm/stand/lib/promif
PLATLIBDIR =	$(PROMIFDIR)/$(TARG_MACH)/$(PROMTYPE)/$(TARGET_ARCH)
PLATLIB =	$(PLATLIBDIR)/libplat.a
PLATLDLIBS =	-L$(PLATLIBDIR) -lplat
PROMLIB =	libprom.a
CPRBOOTLINT =	cprbooter_lint cprboot_lint

CPRBOOT_MAPFILE = mapfile.cprboot
CPRBOOTER_MAPFILE = mapfile.cprbooter

ARCHOPTS = -DMP -D__sparcv9cpu
CFLAGS64 += -D_ELF64_SUPPORT

CPRCOMMOBJ = hwmuldiv.o stubs.o samuldiv64.o
CPRBOOTOBJ = cprboot_srt0.o cprboot.o $(CPRCOMMOBJ)
CPRBOOTEROBJ = srt0.o cprbooter.o cpr_readfile.o $(CPRCOMMOBJ)

ALL = cprbooter cprboot

include ../../common/Makefile.com
include $(TOPDIR)/psm/Makefile.psm.64


.KEEP_STATE:

STANDDIR= $(TOPDIR)/stand
CSDIR	= .
CSDIRS	=	$(STANDDIR)/lib $(STANDDIR)/sys ../../../../promif \
		$(TOPDIR)/uts/common/cpr/cpr_compress.c \
		$(TOPDIR)/uts/common/sys/cpr.h \
		$(TOPDIR)/uts/sun4u/sys/cpr_impl.h \
		$(TOPDIR)/psm/stand/cpr/common \
		.

CPPINCS	+= -I${STANDDIR}
		
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	$(ECHO) $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@$(WC) -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	$(CTAGS) -wt `sed 1d cscope.files`
