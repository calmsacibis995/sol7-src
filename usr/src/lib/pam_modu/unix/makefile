#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.9	97/11/19 SMI"
#
# lib/pam_modules/unix/Makefile
#

# this library is private and is statically linked by usr/src/cmd/passwd.c
PASSWD_LIB=libunixpasswd.a
VERS = .1

SCHOBJECTS=	unix_authenticate.o \
		unix_setcred.o \
		unix_acct_mgmt.o \
		unix_close_session.o \
		unix_open_session.o \
		unix_chauthtok.o \
		unix_update_authtok.o \
		unix_set_authtokattr.o \
		unix_get_authtokattr.o \
		unix_update_authtok_file.o \
		unix_update_authtok_nis.o \
		unix_update_authtok_nisplus.o \
		nispasswd_xdr.o \
		npd_clnt.o \
		switch_utils.o \
		unix_utils.o

PASSWDLIB_OBJS=	pics/unix_update_authtok.o \
		pics/unix_set_authtokattr.o \
		pics/unix_get_authtokattr.o \
		pics/unix_update_authtok_file.o \
		pics/unix_update_authtok_nis.o \
		pics/unix_update_authtok_nisplus.o \
		pics/nispasswd_xdr.o \
		pics/npd_clnt.o \
		pics/switch_utils.o \
		pics/unix_utils.o

DEFOBJECTS=$(SCHOBJECTS:%=def_%)

DEFOBJS=$(DEFOBJECTS:%=objs/%)

DEFPICS=$(DEFOBJECTS:%=pics/%)

OBJECTS= $(SCHOBJECTS)
SRCS= $(OBJECTS:%.o=%.c)

PROTOCOL_DIR = $(ROOT)/usr/include/rpcsvc
DERIVED_FILES= nispasswd_xdr.c

CLOBBERFILES += $(PASSWD_LIB) $(DERIVED_FILES)

# include library definitions
include ../../Makefile.lib


# library dependency
LDLIBS += -lc -lpam -lnsl -lcmd -lmp

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic
LINTFLAGS=

CPPFLAGS += -I$(SRC)/lib/libnsl/include
CPPFLAGS += -DPAM_SECURE_RPC -DPAM_NIS -DPAM_NISPLUS

DYNLIB= pam_unix.so$(VERS)
LIBS = $(DYNLIB)

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR=	$(ROOT)/usr/lib/security
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)

OWNER = root
GROUP = sys

OBJ_DIR=	objs
PIC_DIR=	pics

CP=		cp
TEXT_DOMAIN=	SUNW_OST_SYSOSPAM
MSGFILE=	pam_unix.msg

.KEEP_STATE:

.PARALLEL:	$(DEFOBJS) $(DEFPICS)

BUILD.SO=	$(CC) -o $@ -M mapfile -G $(DYNFLAGS) $(PICS) $(LDLIBS)

all: $(DERIVED_FILES) $(LIBS) $(PASSWD_LIB)

defobjs: $(OBJ_DIR) .WAIT $(DEFOBJS)

defpics: $(PIC_DIR) .WAIT $(DEFPICS)

$(ROOTLIBDIR):
	$(INS.dir)

#install libunixpasswd.a into proto area (usr/lib)
install: all $(ROOTLIBDIR) $(ROOTLIBS) \
	$(ROOTLIBDIR)/pam_unix.so $(ROOTLIBDIR)/$(PASSWD_LIB)

$(ROOTLIBDIR)/pam_unix.so:
	$(RM) $@; $(SYMLINK) $(DYNLIB) $@

# include library targets
include ../../Makefile.targ


# DERIVED_FILES
nispasswd_xdr.c: $(PROTOCOL_DIR)/nispasswd.x
	$(RPCGEN) -c -C -M $(PROTOCOL_DIR)/nispasswd.x > nispasswd_xdr.c

$(PASSWD_LIB) : $(PASSWDLIB_OBJS)
	$(AR) -r $(PASSWD_LIB) $(PASSWDLIB_OBJS)

lint:	$(SRCS:.c=.ln)

#rules to install message table
_msg: $(MSGDOMAIN) $(MSGFILE)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	$(CP) $(MSGFILE) $(MSGDOMAIN)

$(MSGFILE):

$(MSGDOMAIN):
	$(INS.dir)
