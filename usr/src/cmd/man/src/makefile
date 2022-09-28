#
#ident	"@(#)Makefile	1.17	97/10/07 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# Makefile for cmd/man/src
#

include		../../Makefile.cmd

BINPROG =	man
LIBPROG =	getNAME
LIBSHELL =	makewhatis
LNKPROG =	catman apropos whatis
SGMLUTILDIR = 	util

LDLIBS += -lgen

PROG =		$(BINPROG) $(LIBPROG)

SRCS =		man.c getNAME.c

SHSRCS =	makewhatis.sh

OBJS =		$(SRCS:%.c=%.o)


CLOBBERFILES +=	$(LNKPROG) $(LIBSHELL)

ROOTPROG =	$(BINPROG:%=$(ROOTBIN)/%)
ROOTLIBPROG =	$(LIBPROG:%=$(ROOTLIB)/%)
ROOTLIBSHELL =	$(LIBSHELL:%=$(ROOTLIB)/%)
ROOTLNKPROG =	$(LNKPROG:%=$(ROOTBIN)/%)

OWNER =		root
# for messaging catalog files
POFILE= src.po
POFILES= $(SRCS:%.c=%.po)

.KEEP_STATE :

all :		$(PROG) $(LIBSHELL) $(LNKPROG) buildsubs

makewhatis :	$$@.sh
		cat $@.sh > $@

buildsubs :		
		cd $(SGMLUTILDIR); pwd; $(MAKE) all

install :	all $(ROOTPROG) $(ROOTLIBPROG)	\
		$(ROOTLIBSHELL) $(ROOTLNKPROG) installsubs

installsubs :		
		cd $(SGMLUTILDIR); pwd; $(MAKE) install

$(ROOTLNKPROG) :	$(ROOTBIN)/man
		-$(RM) $@; $(LN) $(ROOTBIN)/man $@

$(LNKPROG) :	man
		-$(RM) $@; $(LN) man $@

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@


clean :		sgml_clean

sgml_clean :
		cd $(SGMLUTILDIR); pwd; $(MAKE) clean

clobber :	sgml_clobber

sgml_clobber :
		cd $(SGMLUTILDIR); pwd; $(MAKE) clobber

strip :
		$(STRIP) $(PROG)

lint : 		lint_SRCS

include		../../Makefile.targ
