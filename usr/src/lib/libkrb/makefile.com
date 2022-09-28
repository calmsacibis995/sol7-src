#
#ident	"@(#)Makefile.com	1.2	97/11/25 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libkrb/Makefile.com
#
LIBRARY= libkrb.a
VERS = .1

# objects are listed by source directory

KRB=	\
	kntoln.o create_auth_reply.o create_death_packet.o \
	create_ciph.o create_ticket.o krb_debug_decl.o decomp_ticket.o \
	dest_tkt.o extract_ticket.o fgetst.o getrealm.o get_ad_tkt.o \
	get_admhst.o get_cred.o get_in_tkt.o get_krbhst.o get_krbrlm.o \
	get_phost.o get_pw_tkt.o get_request.o get_svc_in_tkt.o \
	get_tf_fullname.o get_tf_realm.o getst.o in_tkt.o k_gethostname.o \
	klog.o kname_parse.o kparse.o krb_err_txt.o krb_get_in_tkt.o \
	kuserok.o log.o mk_req.o mk_err.o mk_priv.o mk_safe.o month_sname.o \
	netread.o netwrite.o one.o pkt_cipher.o pkt_clen.o rd_req.o rd_err.o \
	rd_priv.o rd_safe.o read_service_key.o recvauth.o save_credentials.o \
	send_to_kdc.o sendauth.o stime.o tf_util.o tkt_string.o krb_util.o \
	krb_err.o \
	${OCOMPATOBJS} ${SETENVOBJS} ${STRCASEOBJS} ${GETOPTOBJS} ${SHMOBJS}

RPC=	\
	auth_kerb.o authkerb_prot.o kerb_subr.o svcauth_kerb.o

COM_ERR = \
	error_message.o et_name.o init_et.o perror.o

OCOMPATOBJS =
SETENVOBJS = #XXXsetenv.o
STRCASEOBJS = strcasecmp.o
GETOPTOBJS =
SHMOBJS =

OBJECTS= $(KRB) $(COM_ERR) $(RPC)
KRBFLAGS = -DSYSV -DSunOS=50

# objects which interface with NIS
NISOBJS     = get_admhst.o get_krbhst.o get_krbrlm.o
NISDEPS     = $(NISOBJS:%=objs/%)
NISDEPS    += $(NISOBJS:%=profs/%)
NISDEPS    += $(NISOBJS:%=pics/%)
$(NISDEPS) := KRBFLAGS += -DNIS

INTERFLAGS  = -DNOENCRYPTION
INTERCPP    = $(INTERFLAGS)

# libkrb build rules
objs/%.o profs/%.o pics/%.o: ../des/%.c
	$(COMPILE.c) $(KRBFLAGS) $(INTERCPP) -I../des -I../krb -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../krb/%.c
	$(COMPILE.c) $(KRBFLAGS) $(INTERCPP) -I../krb -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../rpc/%.c
	$(COMPILE.c) -DPORTMAP $(INTERCPP) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../com_err/%.c
	$(COMPILE.c) $(KRBFLAGS) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../../Makefile.lib

LIBS += $(DYNLIB)
LDLIBS += -lsocket -lnsl -ldl -lc
MAPFILE= ../mapfile-vers
DYNFLAGS += -M $(MAPFILE)

# definitions for install_h target

COMMON_HDRS=	\
	conf.h error_table.h klog.h kparse.h krb_conf.h \
	lsb_addr_comp.h mit-sipb-copyright.h osconf.h prot.h 

sparc_HDRS= conf-svsparc.h 
i386_HDRS=  conf-bsd386i.h

HDRS= $(COMMON_HDRS) $($(MACH)_HDRS)

ROOTHDRDIR=	$(ROOT)/usr/include/kerberos
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=includes/%.check)

# set mode on installed headers to avoid rebuilds caused by
# alternating file modes.  Corrects side effects from default
# permissions on installed dynamic libraries.
$(ROOTHDRS) :=	FILEMODE= 644

# install rule for install_h target
$(ROOTHDRDIR)/%: includes/%
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

$(DYNLIB): $(MAPFILE)

$(ROOTHDRDIR):
	$(INS.dir)

tests dynamic cleantests clobbertests: FRC
	@echo "test command directories..."
	@cd ../des;       pwd; $(MAKE) $(TARGET) "INTERFLAGS = $(INTERFLAGS)"
	@cd ../test.cmd;  pwd; $(MAKE) $(TARGET) "INTERFLAGS = $(INTERFLAGS)"
	@cd ../test.krpc; pwd; $(MAKE) $(TARGET)

FRC:

# include library targets
include ../../Makefile.targ
