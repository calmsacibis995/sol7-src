# Copyright 1988 - 10/18/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.36 96/10/18 Sun Microsystems"

# On Sun OS 3.4/3.5, set BSDREL = 42
# On Sun OS 4.0 set BSDREL = 43
#
# Sun considers its source code as an unpublished, proprietary trade 
# secret, and it is available only under strict license provisions.  
# This copyright notice is placed here only to protect Sun in the event
# the source is deemed a published work.  Disassembly, decompilation, 
# or other means of reducing the object code to human readable form is 
# prohibited by the license agreement under which this code is provided
# to the user or company in possession of this copy.
# 
# RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
# Government is subject to restrictions as set forth in subparagraph 
# (c)(1)(ii) of the Rights in Technical Data and Computer Software 
# clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
# NASA FAR Supplement.
#
#################################################################
# This is 'stock' .. you should not have to touch it.
# Please see the project's Makefile guru to make changes.
#################################################################

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd


TARG_SYS:sh = \
 (                                      \
   UNAME=` uname -r 2>/dev/null` ||.;   \
   case $UNAME in                       \
   '') echo SUNOS;;                     \
   4.*) echo SUNOS;;                    \
   5.*) echo SVR4;;                     \
   *)  echo SVR4;;                      \
   esac                                 \
 )

SUNOS_RANLIB = ranlib
SVR4_RANLIB = true
_RANLIB = $(SUNOS_RANLIB)
RANLIB = $($(TARG_SYS)_RANLIB)

SUNOS_CC = acc
SVR4_CC = cc
_CC = $(SUNOS_CC)
# Solaris 2.6 requires that CC not be reset.
# CC = $($(TARG_SYS)_CC)

BSDREL = 43
NAME = snmp
CFILES = copyrite.c bindint.c bindip.c bindnull.c bindoid.c	\
	 bindstr.c binduint.c buffer.c buildpkt.c		\
	 creatreq.c creatrap.c decode.c encode.c			\
	 getpdu.c localio.c \
	 mibutils.c objectid.c rcv_pkt.c setpdu.c		\
	 setutils.c snmp_d.c snmp_e.c print.c
EXT=c

TARGET-ARCH=$(TARGET_ARCH:-%=%)

# currently unused
XSTR= xstr

# default include directries
INCDIRS += -I. -I../h

LIBTOP = ..

# these get defined later if we're debugging
DDEBUG=
DMALLOC=
MALLOC_LINE=

# headers + yacc, lex, C & CC files make source code
SCODE = $(HFILES) $(CFILES) $(YFILES) $(LFILES) $(CCFILES) 

# convert the source file list into object file list
MEMBERS = $(CFILES:.c=.o) $(CCFILES:.cc=.o) $(DBCFILES:.c=.o)
IMEMBERS = $(ICFILES:.c=.o) $(ICCFILES:.cc=.o)

VARIANTS.o = $(MEMBERS)

DEBUG	  = $(NAME:%=debug-$(TARGET-ARCH)/lib%.a)
PURIFY	  = $(NAME:%=purify-$(TARGET-ARCH)/lib%.a)
STATIC 	  = $(NAME:%=static-$(TARGET-ARCH)/lib%.a)
PROFILE   = $(NAME:%=profile-$(TARGET-ARCH)/lib%_p.a)
SHARED    = $(NAME:%=shared-$(TARGET-ARCH)/lib%.so.$(LIBVERS))
ISHARED   = $(NAME:%=shared-$(TARGET-ARCH)/lib%.sa.$(LIBVERS))

DEBUG-SVR4    = $(NAME:%=debug-$(TARGET-ARCH)/lib%.a).svr4
STATIC-SVR4   = $(NAME:%=static-$(TARGET-ARCH)/lib%.a).svr4
PROFILE-SVR4  = $(NAME:%=profile-$(TARGET-ARCH)/lib%_p.a).svr4
SHARED-SVR4   = $(NAME:%=shared-$(TARGET-ARCH)/lib%.so.$(LIBVERS)).svr4
ISHARED-SVR4  = $(NAME:%=shared-$(TARGET-ARCH)/lib%.sa.$(LIBVERS)).svr4

SHARED-DEMO    = $(NAME:%=sharedemo-$(TARGET-ARCH)/lib%.so.$(LIBVERS))
ISHARED-DEMO   = $(NAME:%=sharedemo-$(TARGET-ARCH)/lib%.sa.$(LIBVERS))
STATIC-DEMO   = $(NAME:%=staticdemo-$(TARGET-ARCH)/lib%.a)

$(SHARED-DEMO) $(ISHARED-DEMO) $(STATIC-DEMO) := CPPFLAGS += -DDEMO

# modifications for shared (creates .sa and .so files)
$(SHARED) $(ISHARED) $(SHARED-SVR4) $(ISHARED-SVR4) $(SHARED-DEMO) \
$(ISHARED-DEMO)  		:=	CPPFLAGS += -DPIC
$(SHARED) $(ISHARED) $(SHARED-SVR4) $(ISHARED-SVR4) $(SHARED-DEMO) \
$(ISHARED-DEMO)			:=	CFLAGS += -pic -O
$(SHARED) $(ISHARED) $(SHARED-SVR4) $(ISHARED-SVR4) $(SHARED-DEMO) \
$(ISHARED-DEMO)			:=	ASFLAGS = -k

$(SHARED) $(ISHARED)	:=	DIR.v = shared-$(TARGET-ARCH)
$(SHARED-SVR4) $(ISHARED-SVR4)	:=	DIR.v = shared-$(TARGET-ARCH).svr4
$(SHARED-DEMO) $(ISHARED-DEMO)	:=	DIR.v = sharedemo-$(TARGET-ARCH)

$(SHARED) $(SHARED-SVR4) $(SHARED-DEMO) := VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(ISHARED) $(ISHARED-SVR4) $(ISHARED-DEMO) \
				:= VARIANTS.o = $(IMEMBERS:%=$(DIR.v)/%)

# modifications for static (non-debugging -- creates a .a file)
$(STATIC) $(STATIC-SVR4) $(STATIC-DEMO) := CFLAGS += -O
$(STATIC) $(STATIC-SVR4) $(STATIC-DEMO)	:= VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(STATIC)	:=	DIR.v = static-$(TARGET-ARCH)
$(STATIC-SVR4)	:=	DIR.v = static-$(TARGET-ARCH).svr4
$(STATIC-DEMO)	:=	DIR.v = staticdemo-$(TARGET-ARCH)

# modifications for debugging (creates a .a file)
# also includes the debugging version of malloc
$(DEBUG) $(DEBUG-SVR4) $(PURIFY) := CFLAGS += -g
$(DEBUG) $(DEBUG-SVR4) $(PURIFY) := DDEBUG = -DDEBUG
$(DEBUG) $(DEBUG-SVR4) $(PURIFY) := VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(DEBUG) $(PURIFY)  	:=	MALLOC_LINE= $(AR) -r $@ $(DMALLOC)
$(DEBUG) $(PURIFY)  	:=	DMALLOC = /usr/lib/debug/malloc.o

$(DEBUG) 	:=	DIR.v = debug-$(TARGET-ARCH)
$(DEBUG-SVR4)	:=	DIR.v = debug-$(TARGET-ARCH).svr4
$(PURIFY) 	:=	DIR.v = purify-$(TARGET-ARCH)
$(PURIFY)	:=	PURIFYTOOL = purify


# modifications for "make profile" (creates a _p.a file)
$(PROFILE) $(PROFILE-SVR4) :=	CPPFLAGS += -DGPROF
$(PROFILE) $(PROFILE-SVR4) :=	CFLAGS += -pg -O
$(PROFILE) $(PROFILE-SVR4) :=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(PROFILE)	:=	DIR.v = profile-$(TARGET-ARCH)
$(PROFILE-SVR4)	:=	DIR.v = profile-$(TARGET-ARCH).svr4

# Flags to pass to the C compiler
CFLAGS += $(DEFINES) $(DDEBUG) $(INCDIRS)
CFLAGS += -O -DNO_PP -DNO_STDLIB -DBSD_RELEASE=${BSDREL} -DOIDC_32 -DSGRP
# hack because CC looks at CCFLAGS, not CFLAGS
CCFLAGS += $(CFLAGS)

# Special make rules for library components
.PRECIOUS:	$(NAME)
$(SHARED)(shared-$(TARGET-ARCH)/%.o):		shared-$(TARGET-ARCH)/%.o
$(ISHARED)(shared-$(TARGET-ARCH)/%.o):		shared-$(TARGET-ARCH)/%.o
$(STATIC)(static-$(TARGET-ARCH)/%.o):		static-$(TARGET-ARCH)/%.o
$(DEBUG)(debug-$(TARGET-ARCH)/%.o):		debug-$(TARGET-ARCH)/%.o
$(PURIFY)(purify-$(TARGET-ARCH)/%.o):		purify-$(TARGET-ARCH)/%.o
$(PROFILE)(profile-$(TARGET-ARCH)/%.o):		profile-$(TARGET-ARCH)/%.o

$(SHARED-SVR4)(shared-$(TARGET-ARCH).svr4/%.o):	shared-$(TARGET-ARCH).svr4/%.o
$(ISHARED-SVR4)(shared-$(TARGET-ARCH)/%.o):	shared-$(TARGET-ARCH).svr4/%.o
$(STATIC-SVR4)(static-$(TARGET-ARCH)/%.o):	static-$(TARGET-ARCH).svr4/%.o
$(DEBUG-SVR4)(debug-$(TARGET-ARCH)/%.o):	debug-$(TARGET-ARCH).svr4/%.o
$(PROFILE-SVR4)(profile-$(TARGET-ARCH)/%.o):	profile-$(TARGET-ARCH).svr4/%.o

$(SHARED-DEMO)(sharedemo-$(TARGET-ARCH)/%.o):	sharedemo-$(TARGET-ARCH)/%.o
$(ISHARED-DEMO)(sharedemo-$(TARGET-ARCH)/%.o):	sharedemo-$(TARGET-ARCH)/%.o
$(STATIC-DEMO)(staticdemo-$(TARGET-ARCH)/%.o):	staticemo-$(TARGET-ARCH)/%.o

CLOBBERFILES=$(SCODE) Makefile*

.KEEP_STATE:

# targets 'all', 'bin' and 'shared' are declared in the individual makefile
all: 	static 

install: all

debug: debug-$(TARGET-ARCH) $(HFILES) $(DEBUG)
purify: purify-$(TARGET-ARCH) $(HFILES) $(PURIFY)
static:	static-$(TARGET-ARCH) $(HFILES) $(STATIC)
# XXX This is kinda kludgy and needs to be re-thought
slibrary: shared-$(TARGET-ARCH) $(HFILES) $(SHARED) $(ISHARED)
share dynamic: shared
profiled: profile

debug-svr4:  debug-$(TARGET-ARCH).svr4 $(HFILES) $(DEBUG-SVR4)
profile-svr4: profile-$(TARGET-ARCH).svr4 $(HFILES) $(PROFILE-SVR4)
slibrary-svr4: shared-$(TARGET-ARCH).svr4 $(HFILES) $(SHARED-SVR4) \
		$(ISHARED-SVR4)
share-svr4 dynamic-svr4: shared-svr4
static-demo: staticdemo-$(TARGET-ARCH) $(HFILES) $(STATIC-DEMO)

svr4: static-svr4
demo: static-demo

# create the target directory
shared-$(TARGET-ARCH) static-$(TARGET-ARCH) debug-$(TARGET-ARCH) \
profile-$(TARGET-ARCH) staticdemo-$(TARGET-ARCH) \
 purify-$(TARGET-ARCH) sharedemo-$(TARGET-ARCH):
	@install -d $@

shared-$(TARGET-ARCH).svr4 static-$(TARGET-ARCH).svr4 \
debug-$(TARGET-ARCH).svr4  profile-$(TARGET-ARCH).svr4:
	@install -d $@


# how to create the archives (.a, _p.a and .sa)
$(STATIC-DEMO) $(DEBUG) $(PURIFY) $(STATIC) \
$(PROFILE) $(ISHARED): $$(VARIANTS.o)
	$(RM) $@
	$(AR) -r $@ $(VARIANTS.o)
	$(RANLIB) $@

$(DEBUG-SVR4) $(STATIC-SVR4) $(PROFILE-SVR4) $(ISHARED-SVR4): $$(VARIANTS.o)
	$(RM) $@
#
# ARFLAGS by default is -rv. We don't want verbose.
#
	$(AR) -r $@ $(VARIANTS.o)
	$(RANLIB) $@

# how to create the .so file
$(SHARED-DEMO)$(SHARED): $$(VARIANTS.o)
	$(RM) $@
	$(LD) -o $@ -assert pure-text $(VARIANTS.o)

$(SHARED-SVR4): $$(VARIANTS.o)
	$(RM) $@
	$(LD) -o $@ -z text -G $(VARIANTS.o) 

# SNM-custom compilation rules
COMPILE.c=$(PURIFYTOOL) $(CC) $(CPPFLAGS) $(CFLAGS) -c
COMPILE.cc=$(PURIFYTOOL) $(CCC) $(CPPFLAGS) $(CCFLAGS) -c

# how to convert .c files into .o files
debug-$(TARGET-ARCH)/%.o static-$(TARGET-ARCH)/%.o shared-$(TARGET-ARCH)/%.o \
profile-$(TARGET-ARCH)/%.o staticdemo-$(TARGET-ARCH)/%.o \
 purify-$(TARGET-ARCH)/%.o sharedemo-$(TARGET-ARCH)/%.o: %.$(EXT)
	$(COMPILE.$(EXT)) -o $@ $<

debug-$(TARGET-ARCH).svr4/%.o static-$(TARGET-ARCH).svr4/%.o \
shared-$(TARGET-ARCH).svr4/%.o profile-$(TARGET-ARCH).svr4/%.o: %.$(EXT)
	$(COMPILE.$(EXT)) -o $@ $<


##
## Warning - Lint section follows.  Not for the faint of heart.
##

# where linted files live
LINTDIR = lint-$(TARGET-ARCH)
$(LINTDIR):
	@install -d $@

# name of lint's output file (not the lint library)
LINTOUT = $(LINTDIR)/lint.out
$(LINTOUT):
	touch $@

LINTLIB = $(NAME:%=$(LINTDIR)/llib-l%.ln)

# LINTLIBS, the path names to the needed lint libraries, are stored
# in the local Makefile.

# flags for lint
LINTFLAGS += -bhuz $(DEFINES) $(INCDIRS)

# OK, are you ready?
# 'make lint' creates (with the lint library) a file called LINTOUT
# that gets appended to on every invokation.  However, if nothing has changed
# (if LINTLIB is older than everything else) nothing gets done, since LINTLIB
# is up to date.  If anything is newer than LINTLIB, every source file gets
# re-linted, rather than just the new ones.  It's not easy to fix this
# because of the way lint's "-C" option works.
lint:	$(LINTLIB)

$(LINTLIB): $(LINTDIR) $(LINTOUT) $(SCODE) $(LINTLIBS)
# make the lint library (pass 1)
	echo "***** lint run on `date` *****" >>$(LINTOUT)
	$(LINT.c) -C$(NAME) -o $@ $(CFILES) | tee -a $(LINTOUT)
# Check it against the other libraries for any function call problems (pass 2)
# This also shows us other library's mistakes, but piping the output
# through grep is wrong; we could be losing valuable info that way.
# Better to be a constant reminder that the other libraries need to be fixed.
	echo "%% pass2 %%" >>$(LINTOUT)
# LIBSLINT is specified in the individual makefile, and is a list of
# libraries to pass to lint (typically like "$(LINTLIBS) -lm", etc)
	$(LINT.c) $@ $(LIBSLINT) | tee -a $(LINTOUT)
# we do this so the target is newer than the LINTOUT file
	@touch $@

# Since the lint libraries depend on each other, we create them when needed.
# This is sub-optimal and tends to discourage people from using lint
$(LIBTOP)/libncmodel/$(LINTDIR)/llib-lncmodel.ln:
	cd $(LIBTOP)/libncmodel; $(MAKE) lint

$(LIBTOP)/libpdbms/$(LINTDIR)/llib-lpdbms.ln:
	cd $(LIBTOP)/libpdbms; $(MAKE) lint

$(LIBTOP)/librpc/$(LINTDIR)/llib-lrpc.ln:
	cd $(LIBTOP)/librpc; $(MAKE) lint

# XXX Unfortunately there's no more libnetmgt lint library
# XXX since lint can't handle C++.
#$(LIBTOP)/libnetmgt/$(LINTDIR)/llib-lnetmgt.ln:
#	cd $(LIBTOP)/libnetmgt; $(MAKE) lint

#../../foreign/isam/llib-lisam.ln:
#	cd ../../foreign/isam; $(MAKE) lint

# XXX We should put Brad Rubenstein's OW lint lib here too...

install-svr4 install-svr4-debug install-sunos install-sunos-debug: all

##
## The rest of this is stock and should come from a project master makefile
##

clean:
	$(RM) a.out core *.s *.o *.ln errs *.bak *.BAK *.d *.tcov *mon.out tags TAGS
	$(RM) -r debug-$(TARGET-ARCH) purify-$(TARGET-ARCH) static-$(TARGET-ARCH) shared-$(TARGET-ARCH) profile-$(TARGET-ARCH) $(LINTDIR)

realclobber: clean
	sccs clean

tags:	$(SCODE)
	$(RM) tags; touch tags
	ctags -t -w $(SCODE)

etags:	$(SCODE)
	$(RM) TAGS; touch TAGS
	etags -t $(SCODE)

size:
	# can't 'size' a library file

# char/word/line counts
wc:
	@wc $(SCODE)

# source counts (any line with a semicolon in it)
sc:
	@grep -c \; $(SCODE)

# generate hardcopy
PR = enscript -2r
print:	$(SCODE)
	@ls -ls | $(PR)
	@$(PR) [Mm]akefile $(SUPP) $(SCODE)

# prettyprint
# this is the closest to the Sun guidelines ("Joy Normal Form") as possible
# except indent does nasty things with comments
INDENTOPTS = -bap -bad -nbc -di0 -i4 -nfc1 -l80 -lc79 -nlp -sc
indent:
	for i in $(CFILES); do \
		indent $(INDENTOPTS) $$i; done

pretty: indent

# cross reference
# seems to choke on "void"
CXREF= cxref
xref:
	$(CXREF) -c $(CPPFLAGS) $(CFLAGS) $(SCODE)

crunch:
	#load -Dlint $(CFLAGS) $(CFILES)

wcrunch:
	#load -w -Dlint $(CFLAGS) $(CFILES)

FORCE:

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ

