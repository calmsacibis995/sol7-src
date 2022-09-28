#
#ident	"@(#)Makefile	1.22	98/01/21 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/sendmail/cf/Makefile

include		../../Makefile.cmd

TXTS=		cf/main-v7sun.mc cf/subsidiary-v7sun.mc \
		domain/solaris-generic.m4

CFS=		sendmail.cf main.cf subsidiary.cf

SUBCFS=		cf/main-v7sun.cf cf/subsidiary-v7sun.cf

COMMONM4FILES=	m4/version.m4 m4/cf.m4 m4/cfhead.m4 m4/proto.m4 \
		ostype/solaris2.ml.m4 domain/solaris-generic.m4 \
		mailer/local.m4 mailer/smtp.m4

ROOTETCMAIL = $(ROOTETC)/mail

ROOTETCMAILCF=	$(CFS:%=$(ROOTETCMAIL)/%)

ROOTLIBMAIL =	$(ROOTLIB)/mail

BUILDPARTS =	$(ROOTLIBMAIL)/README \
		$(ROOTLIBMAIL)/cf/main-v7sun.cf \
		$(ROOTLIBMAIL)/cf/main-v7sun.mc \
		$(ROOTLIBMAIL)/cf/subsidiary-v7sun.cf \
		$(ROOTLIBMAIL)/cf/subsidiary-v7sun.mc \
		$(ROOTLIBMAIL)/domain/generic.m4 \
		$(ROOTLIBMAIL)/domain/solaris-generic.m4 \
		$(ROOTLIBMAIL)/feature/allmasquerade.m4 \
		$(ROOTLIBMAIL)/feature/always_add_domain.m4 \
		$(ROOTLIBMAIL)/feature/bestmx_is_local.m4 \
		$(ROOTLIBMAIL)/feature/bitdomain.m4 \
		$(ROOTLIBMAIL)/feature/domaintable.m4 \
		$(ROOTLIBMAIL)/feature/generics_entire_domain.m4 \
		$(ROOTLIBMAIL)/feature/genericstable.m4 \
		$(ROOTLIBMAIL)/feature/limited_masquerade.m4 \
		$(ROOTLIBMAIL)/feature/mailertable.m4 \
		$(ROOTLIBMAIL)/feature/masquerade_entire_domain.m4 \
		$(ROOTLIBMAIL)/feature/masquerade_envelope.m4 \
		$(ROOTLIBMAIL)/feature/nocanonify.m4 \
		$(ROOTLIBMAIL)/feature/nodns.m4 \
		$(ROOTLIBMAIL)/feature/notsticky.m4 \
		$(ROOTLIBMAIL)/feature/nouucp.m4 \
		$(ROOTLIBMAIL)/feature/nullclient.m4 \
		$(ROOTLIBMAIL)/feature/redirect.m4 \
		$(ROOTLIBMAIL)/feature/remote_mode.m4 \
		$(ROOTLIBMAIL)/feature/smrsh.m4 \
		$(ROOTLIBMAIL)/feature/stickyhost.m4 \
		$(ROOTLIBMAIL)/feature/sun_reverse_alias_files.m4 \
		$(ROOTLIBMAIL)/feature/sun_reverse_alias_nis.m4 \
		$(ROOTLIBMAIL)/feature/sun_reverse_alias_nisplus.m4 \
		$(ROOTLIBMAIL)/feature/use_ct_file.m4 \
		$(ROOTLIBMAIL)/feature/use_cw_file.m4 \
		$(ROOTLIBMAIL)/feature/uucpdomain.m4 \
		$(ROOTLIBMAIL)/feature/virtusertable.m4 \
		$(ROOTLIBMAIL)/m4/cf.m4 \
		$(ROOTLIBMAIL)/m4/cfhead.m4 \
		$(ROOTLIBMAIL)/m4/nullrelay.m4 \
		$(ROOTLIBMAIL)/m4/proto.m4 \
		$(ROOTLIBMAIL)/m4/version.m4 \
		$(ROOTLIBMAIL)/mailer/local.m4 \
		$(ROOTLIBMAIL)/mailer/smtp.m4 \
		$(ROOTLIBMAIL)/mailer/uucp.m4 \
		$(ROOTLIBMAIL)/ostype/solaris2.m4 \
		$(ROOTLIBMAIL)/ostype/solaris2.ml.m4 \
		$(ROOTLIBMAIL)/sh/makeinfo.sh

FILEMODE=	444
M4=		m4 -DSUN_HIDE_INTERNAL_DETAILS

$(BUILDPARTS) := OWNER=root
$(BUILDPARTS) := GROUP=mail

# build rule
cf/%.cf:	cf/%.mc
	cd cf; $(M4) ../m4/cf.m4 $< > $@
	cd ..

.KEEP_STATE:

all:		$(CFS)

sendmail.cf :	subsidiary.cf
	$(CP) subsidiary.cf $@

subsidiary.cf:	cf/subsidiary-v7sun.cf
	$(CP) cf/subsidiary-v7sun.cf $@

main.cf:	cf/main-v7sun.cf
	$(CP) cf/main-v7sun.cf $@

cf/subsidiary-v7sun.cf:	cf/subsidiary-v7sun.mc $(COMMONM4FILES) \
			feature/remote_mode.m4
	cd cf; $(M4) ../m4/cf.m4 subsidiary-v7sun.mc > subsidiary-v7sun.cf

cf/main-v7sun.cf:	cf/main-v7sun.mc $(COMMONM4FILES)
	cd cf; $(M4) ../m4/cf.m4 main-v7sun.mc > main-v7sun.cf

install:	all $(ROOTETCMAILCF) $(BUILDPARTS)

clean:
	$(RM) $(CFS) $(SUBCFS)

clobber: 	clean

$(CFS): $(TXTS)

$(ROOTETCMAIL)/%.cf: %.cf
	$(INS.file)

$(ROOTLIBMAIL)/%:	%
	$(INS.file)
