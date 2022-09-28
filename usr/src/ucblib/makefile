#
#ident	"@(#)Makefile	1.5	97/04/03 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
#
# ucblib/Makefile
#
# include global definitions
include ../Makefile.master

# libbc is special.  It depends on libucb and must be built after libucb.
SUBDIRS= libdbm libcurses libtermcap libucb librpcsoc
sparc_SUBDIR= ../lib/libbc 
BWOSDIRS=

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install clean clobber: $(SUBDIRS) .WAIT $($(MACH)_SUBDIR)

bwos: $(BWOSDIRS) .WAIT $(SUBDIRS) .WAIT $($(MACH)_SUBDIR)

install_h:

$(BWOSDIRS) $(SUBDIRS) $($(MACH)_SUBDIR) : FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

ROOTHDRDIR1= $(ROOT)/usr/ucbinclude
ROOTHDRDIR2= $(ROOT)/usr/include

CSDIR   = .
CSDIRS  = libdbm libucb libtermcap libcurses librpcsoc
CSPATHS = $(CSDIRS:%=$(CSDIR)/%)
CSINCS  = $(ROOTHDRDIR1:%=-I%) $(CSPATHS:%=-I%) $(ROOTHDRDIR2:%=-I%) \
	 $(ENVCPPFLAGS2)
CSCOPE  = cscope
CTAGS   = ctags

.PRECIOUS: cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
		-type d -name '.del-*' -prune -o -type f \
		\( -name '*.[csh]' -o -name 'Makefile*' -o -name 'mapfile*' \) \
		  -print >> cscope.files
	echo "Makefile" >> cscope.files
	echo "Makefile.ucbtarg" >> cscope.files
	echo "libucb/sparc/llib-lucb" >> cscope.files
	echo "libucb/sparcv9/llib-lucb" >> cscope.files
	echo "../Makefile.master" >> cscope.files
	echo "../lib/Makefile.lib" >> cscope.files
	echo "../lib/Makefile.mach" >> cscope.files
	echo "../lib/Makefile.targ" >> cscope.files
	@wc -l cscope.files

tags: tags.list FRC
	${CTAGS} -wt `cat tags.list`

tags.list: cscope.files
	@$(RM) tags.list
	grep '\.c$$' cscope.files > tags.list
	grep '\.s$$' cscope.files >> tags.list
	grep '\.h$$' cscope.files >> tags.list

FRC:

