#
#ident	"@(#)Makefile.com	1.5	97/11/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/Makefile.master

PKGARCHIVE=	.
DATAFILES=	copyright prototype_com prototype_$(MACH) preinstall \
		postremove depend
README=		SUNWonld-README
FILES=		$(DATAFILES) pkginfo
PACKAGE= 	SUNWonld
ROOTONLD=	$(ROOT)/opt/SUNWonld
ROOTREADME=	$(README:%=$(ROOTONLD)/%)

CLEANFILES=	$(FILES) awk_pkginfo ../bld_awk_pkginfo
CLOBBERFILES=	$(PACKAGE)

../%:		../common/%.ksh
		$(RM) $@
		cp $< $@
		chmod +x $@

$(ROOTONLD)/%:	../common/%
		$(RM) $@
		cp $< $@
		chmod +x $@
