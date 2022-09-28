#
# Copyright (c) 1989, 1996, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/print/Makefile.sp
# Common makefile definitions (should be) used by all print(lp) makefiles
#
#ident	"@(#)Makefile.sp	1.10	97/12/08 SMI"

include		$(SRC)/cmd/Makefile.cmd

LPROOT=		$(SRC)/cmd/lp
NPRTROOT=	$(LPROOT)
ROOTVAR=	$(ROOT)/var
ROOTVARSP=	$(ROOT)/var/spool
ROOTVARSPOOLPRINT=	$(ROOTVARSP)/print

ROOTINIT_D=	$(ROOTETC)/init.d
ROOTRC0_D=	$(ROOTETC)/rc0.d
ROOTRCS_D=	$(ROOTETC)/rcS.d
ROOTRC1_D=	$(ROOTETC)/rc1.d
ROOTRC2_D=	$(ROOTETC)/rc2.d


ROOTETCLP=	$(ROOTETC)/lp
ROOTLIBLP=	$(ROOTLIB)/lp
ROOTBINLP=	$(ROOTBIN)/lp
ROOTLIBLPPOST =	$(ROOTLIBLP)/postscript
ROOTLOCALLP=	$(ROOTLIBLP)/local
ROOTLIBPRINT=	$(ROOTLIB)/print
ROOTLIBPRINTBIN=	$(ROOTLIBPRINT)/bin

ROOTUSRUCB=	$(ROOT)/usr/ucb


#
# Typical owner and group for LP things. These can be overridden
# in the individual makefiles.
#
OWNER	=	lp
GROUP	=	lp
SUPER	=	root

#
# $(EMODES): Modes for executables
# $(SMODES): Modes for setuid executables
# $(DMODES): Modes for directories
#
EMODES	=	0555
SMODES	=	04555
DMODES	=	0775


INC	=	$(ROOT)/usr/include
INCSYS  =       $(INC)/sys

LPINC	=	$(SRC)/include
#NPRTINC	=	$(NPRTROOT)/include
NPRTINC	=	$(SRC)/lib
LPLIB	=	$(SRC)/lib
LDLIBS +=	-L$(LPLIB)


LIBNPRT =       -L$(ROOT)/usr/lib -lprint

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint
strip:=          TARGET= strip
catalog:=	TARGET= catalog

ROOTLIBLPPROG=	$(PROG:%=$(ROOTLIBLP)/%)
ROOTBINLPPROG=	$(PROG:%=$(ROOTBINLP)/%)
ROOTETCLPPROG=	$(PROG:%=$(ROOTETCLP)/%)
ROOTUSRUCBPROG=	$(PROG:%=$(ROOTUSRUCB)/%)
ROOTLOCALLPPROG=	$(PROG:%=$(ROOTLOCALLP)/%)
ROOTLIBLPPOSTPROG=	$(PROG:%=$(ROOTLIBLPPOST)/%)
ROOTLIBPRINTPROG=	$(PROG:%=$(ROOTLIBPRINT)/%)

$(ROOTLIBLP)/%	\
$(ROOTBINLP)/%	\
$(ROOTETCLP)/%	\
$(ROOTUSRUCB)/%	\
$(ROOTLOCALLP)/% \
$(ROOTLIBLPPOST)/% \
$(ROOTLIBPRINT)/% :	%
		$(INS.file)
