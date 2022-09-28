#
# ident	"@(#)Makefile	1.3	98/01/09 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cvcd/sparc/sun4u/cvcd
#
PROG=		cvcd
PLATFORM=	SUNW,Ultra-Enterprise-10000

#	Create default so empty rules don't confuse make
CLASS=		32

SRCS=		$(PROG:%=%.c)

include ../../../../Makefile.cmd
include ../../../../../Makefile.psm

FILEMODE= 0700
OWNER= root
GROUP= sys

ROOTPSMPROG	= $(ROOT_PSM_LIB_DIR)/$(PROG)

CPPFLAGS = -I$(USR_PSM_INCL_DIR) $(CPPFLAGS.master)

LDLIBS +=	-dn -B static -lnsl -lc -ldl_stubs \
		-lnss_files -lnss_nis -lnss_nisplus -lnss_compat -lnss_dns \
		-lresolv -lsocket -lstraddr -lnsl

CLOBBERFILES +=	$(SRCS)

.KEEP_STATE:

all: $(PROG) 

install: all .WAIT $(ROOTPSMPROG)

clean:
	$(RM) $(PROG)

lint: lint_PROG

include ../../../../Makefile.targ
include ../../../../../Makefile.psm.targ
