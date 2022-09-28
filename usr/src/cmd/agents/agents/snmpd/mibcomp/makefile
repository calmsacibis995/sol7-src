# Copyright 1988 - 12/06/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	2.34 96/12/06 Sun Microsystems"

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


CFILES = mibcomp.c parse.c output.c skel.c numbers.c array.c man.c manh.c \
	type.c default.c include.c util.c
HFILES = mibcomp.h
EXT=c
NAME=mibcomp 
# CFLAGS: set -DDEBUG for debugging. set -DSUN to run under SunOS.
#CFLAGS = -DDEBUG -DSUN
CFLAGS += -DSUN

# currently unused
XSTR= xstr

# default include directries
INCDIRS = -I. -I../h

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

DEBUG	  = $(NAME:%=debug$(TARGET_ARCH)/%)
PURIFY	  = $(NAME:%=purify$(TARGET_ARCH)/%)
STATIC 	  = $(NAME:%=static$(TARGET_ARCH)/%)
PROFILE   = $(NAME:%=profile$(TARGET_ARCH)/%)
SHARED    = $(NAME:%=shared$(TARGET_ARCH)/%)
ISHARED   = $(NAME:%=shared$(TARGET_ARCH)/%)

# modifications for shared (creates .sa and .so files)
$(SHARED) $(ISHARED)	:=	CPPFLAGS += -DPIC
$(SHARED) $(ISHARED)	:=	CFLAGS += -pic -O
$(SHARED) $(ISHARED)	:=	ASFLAGS = -k
$(SHARED) $(ISHARED)	:=	DIR.v = shared$(TARGET_ARCH)

$(SHARED) :=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(ISHARED) :=	VARIANTS.o = $(IMEMBERS:%=$(DIR.v)/%)

# modifications for static (non-debugging -- creates a .a file)
$(STATIC) 	:=	CFLAGS +=
$(STATIC)	:=	DIR.v = static$(TARGET_ARCH)
$(STATIC) 	:=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)

# modifications for debugging (creates a .a file)
# also includes the debugging version of malloc
$(DEBUG) 	:=	CFLAGS += -g
$(DEBUG) 	:=	DDEBUG = -DDEBUG
$(DEBUG) 	:=	MALLOC_LINE= $(AR) $(ARFLAGS) $@ $(DMALLOC)
$(DEBUG) 	:=	DMALLOC = /usr/lib/debug/malloc.o
$(DEBUG)	:=	DIR.v = debug$(TARGET_ARCH)
$(DEBUG) 	:=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)

# modifications for make purify (creates a .a file)
$(PURIFY) 	:=	CFLAGS += -g
$(PURIFY) 	:=	DDEBUG = -DDEBUG
$(PURIFY) 	:=	MALLOC_LINE= $(AR) $(ARFLAGS) $@ $(DMALLOC)
$(PURIFY) 	:=	DMALLOC = /usr/lib/debug/malloc.o
$(PURIFY)	:=	DIR.v = purify$(TARGET_ARCH)
$(PURIFY) 	:=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)
$(PURIFY) 	:=	PURIFYTOOL = purify

# modifications for "make profile" (creates a _p.a file)
$(PROFILE) 	:=	CPPFLAGS += -DGPROF
$(PROFILE) 	:=	CFLAGS += -pg
$(PROFILE)	:=	DIR.v = profile$(TARGET_ARCH)
$(PROFILE) 	:=	VARIANTS.o = $(MEMBERS:%=$(DIR.v)/%)

# Flags to pass to the C compiler
CFLAGS += $(DEFINES) $(DDEBUG) $(INCDIRS)

# hack because CC looks at CCFLAGS, not CFLAGS
CCFLAGS += $(CFLAGS)

PROG=$(DEBUG) $(SHARED) $(SHARED-SVR4)
CLOBBERFILES=$(SCODE) Makefile

# Special make rules for library components
.PRECIOUS:	$(NAME)
#$(SHARED)(shared$(TARGET_ARCH)/%.o):		shared$(TARGET_ARCH)/%.o
#$(ISHARED)(shared$(TARGET_ARCH)/%.o):		shared$(TARGET_ARCH)/%.o
#$(STATIC)(static$(TARGET_ARCH)/%.o):		static$(TARGET_ARCH)/%.o
#$(DEBUG)(debug$(TARGET_ARCH)/%.o):		debug$(TARGET_ARCH)/%.o
#$(PROFILE)(profile$(TARGET_ARCH)/%.o):		profile$(TARGET_ARCH)/%.o

.KEEP_STATE:

# targets 'all', 'bin' and 'shared' are declared in the individual makefile
all: 	static 
install: all
demo: static
debug: debug$(TARGET_ARCH) $(HFILES) $(DEBUG)
purify: purify$(TARGET_ARCH) $(HFILES) $(PURIFY)
static:	static$(TARGET_ARCH) $(HFILES) $(STATIC)
# XXX This is kinda kludgy and needs to be re-thought
slibrary: shared$(TARGET_ARCH) $(HFILES) $(SHARED) $(ISHARED)
share dynamic: shared
profile: profile$(TARGET_ARCH) $(HFILES) $(PROFILE)
profiled: profile

# create the target directory
shared$(TARGET_ARCH) static$(TARGET_ARCH) debug$(TARGET_ARCH) \
 purify$(TARGET_ARCH) profile$(TARGET_ARCH):
	@install -d $@


# how to create the archives (.a, _p.a and .sa)
$(DEBUG) $(STATIC) $(PROFILE) $(ISHARED) $(PURIFY): $$(VARIANTS.o)
	$(RM) $@
	$(CC) -o $@ $(LDLIBS) $(VARIANTS.o)


# how to create the .so file
$(SHARED): $$(VARIANTS.o)
	$(RM) $@
	$(LD) -o $@ -assert pure-text $(VARIANTS.o)

$(SHARED-SVR4): $$(VARIANTS.o)
	$(RM) $@
	$(LD) -o $@ -z text -G $(VARIANTS.o) 

# SNM-custom compilation rules
COMPILE.c=$(PURIFYTOOL) $(CC) $(CPPFLAGS) $(CFLAGS) -c
COMPILE.cc=$(PURIFYTOOL) $(CCC) $(CPPFLAGS) $(CCFLAGS) -c

# how to convert .c files into .o files
debug$(TARGET_ARCH)/%.o static$(TARGET_ARCH)/%.o shared$(TARGET_ARCH)/%.o\
 purify$(TARGET_ARCH)/%.o profile$(TARGET_ARCH)/%.o: %.$(EXT)
	$(COMPILE.$(EXT)) -o $@ $<

##
## Warning - Lint section follows.  Not for the faint of heart.
##

# where linted files live
LINTDIR = lint$(TARGET_ARCH)
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

#
# Need to remove $(NAME) (the a.out target) during a make clean
#

clean:
	$(RM) a.out core *.s *.o *.ln errs *.bak *.BAK *.d *.tcov *mon.out tags TAGS $(NAME)
	$(RM) -r debug$(TARGET_ARCH) purify$(TARGET_ARCH) static$(TARGET_ARCH) shared$(TARGET_ARCH) profile$(TARGET_ARCH) $(LINTDIR)

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

install-svr4 install-sunos install-svr4-debug install-sunos-debug: all

FORCE:

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.targ


