#
#ident	"@(#)Makefile.com	1.6	97/09/05 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/Makefile.com
#

CFLAGS += -v
CFLAGS64 += -v

lint	:= LINTFLAGS = -mux
lint	:= LINTFLAGS64 = -mux -D__sparcv9

ROOTPROCBIN =		$(ROOT)/usr/proc/bin
ROOTPROCLIB =		$(ROOT)/usr/proc/lib

ROOTPROCBIN32 =		$(ROOT)/usr/proc/bin/$(MACH32)
ROOTPROCLIB32 =		$(ROOT)/usr/proc/lib

ROOTPROCBIN64 =		$(ROOT)/usr/proc/bin/$(MACH64)
ROOTPROCLIB64 =		$(ROOT)/usr/proc/lib/$(MACH64)

ROOTPROCBINPROG =	$(PROG:%=$(ROOTPROCBIN)/%)
ROOTPROCLIBLIB =	$(LIBS:%=$(ROOTPROCLIB)/%)

ROOTPROCBINPROG32 =	$(PROG:%=$(ROOTPROCBIN32)/%)
ROOTPROCLIBLIB32 =	$(LIBS:%=$(ROOTPROCLIB32)/%)

ROOTPROCBINPROG64 =	$(PROG:%=$(ROOTPROCBIN64)/%)
ROOTPROCLIBLIB64 =	$(LIBS:%=$(ROOTPROCLIB64)/%)

$(ROOTPROCLIB)/%: %
	$(INS.file)

$(ROOTPROCBIN32)/%: %
	$(INS.file)

$(ROOTPROCLIB32)/%: %
	$(INS.file)

$(ROOTPROCBIN64)/%: %
	$(INS.file)

$(ROOTPROCLIB64)/%: %
	$(INS.file)
