#
#
# ident	"@(#)Makefile	1.74	98/02/03 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# pkgdefs/Makefile

include $(SRC)/Makefile.master

sparcv9_SUBDIRS= \
	SUNWarcx \
	SUNWcarx.u \
	SUNWcg6x.u \
	SUNWcprx.u \
	SUNWcsxu \
	SUNWcslx \
	SUNWesxu \
	SUNWipcx \
	SUNWkvmx.u \
	SUNWlocx \
	SUNWpmux \
	SUNWssadx \
	SUNWhmdx \
	SUNWpdx \
	SUNWtnfcx \
	SUNWtoox \
	SUNWvolux \
	SUNWxwdvx \
	SUNWfnsx \
	SUNWfnx5x \
	SUNWgsdhx \
	SUNWgssx \
	SUNWrsgx \
	SUNWbtoox \
	SUNWpwbx.u \
	SUNWluxlx \
	SUNWluxdx.u

sparc_SUBDIRS= \
	SUNWbcp  \
	SUNWcar.c  \
	SUNWcar.d  \
	SUNWcar.m  \
	SUNWcar.u  \
	SUNWcg6.c  \
	SUNWcg6.d  \
	SUNWcg6.m  \
	SUNWcg6.u  \
	SUNWcg6h  \
	SUNWcvc.u  \
	SUNWcvcr.u \
	SUNWcvcx.u \
	SUNWdfb.c  \
	SUNWdfb.d  \
	SUNWdfb.m  \
	SUNWdfb.u  \
	SUNWkey  \
	SUNWkvm.c  \
	SUNWkvm.d  \
	SUNWkvm.m  \
	SUNWkvm.u  \
	SUNWluxl \
	SUNWluxd.d \
	SUNWluxd.u \
	SUNWluxop \
	SUNWses \
	SUNWssad  \
	SUNWssaop  \
	SUNWsxr.m  \
	SUNWhmd    \
	SUNWhmdu   \
	SUNWpd    \
	SUNWpdu	  \
	SUNWcpr.m \
	SUNWcpr.u \
	SUNWvygdr.m \
	SUNWpwb.u

i386_SUBDIRS= \
	SUNWcar.i  \
	SUNWcoff  \
	SUNWdfb.i  \
	SUNWkvm.i  \
	SUNWs53

COMMON_SUBDIRS= \
	SUNWaccr  \
	SUNWaccu  \
	SUNWapppr  \
	SUNWapppu  \
	SUNWatfsr  \
	SUNWatfsu  \
	SUNWarc  \
	SUNWast  \
	SUNWbnur  \
	SUNWbnuu  \
	SUNWbtool  \
	SUNWcsd  \
	SUNWcsr  \
	SUNWcsu  \
	SUNWcsl  \
	SUNWdfbh  \
	SUNWdhcsr  \
	SUNWdhcsu  \
	SUNWdoc  \
	SUNWesu  \
	SUNWfac  \
	SUNWfns  \
	SUNWfnsx5  \
	SUNWftpr  \
	SUNWftpu  \
	SUNWgssdh  \
	SUNWgss	\
	SUNWgssk \
	SUNWhea  \
	SUNWipc  \
	SUNWloc  \
	SUNWmibii \
	SUNWnisr  \
	SUNWnisu  \
	SUNWntpr  \
	SUNWntpu  \
	SUNWosdem \
	SUNWypr \
	SUNWypu \
	SUNWpcmci  \
	SUNWpcmcu  \
	SUNWpcr	\
	SUNWpcu	\
	SUNWpppk  \
	SUNWpsf	\
	SUNWpmu \
	SUNWpsr	\
	SUNWpsu	\
	SUNWrsg \
	SUNWrsgk \
	SUNWsacom \
	SUNWsadmi \
	SUNWsasnm \
	SUNWscpr  \
	SUNWscpu  \
	SUNWscplp  \
	SUNWsndmr  \
	SUNWsndmu  \
	SUNWsra  \
	SUNWsrh  \
	SUNWsutl  \
	SUNWter  \
	SUNWtnfc  \
	SUNWtnfd  \
	SUNWtoo  \
	SUNWvolr  \
	SUNWvolu  \
	SUNWxcu4  \
	SUNWxwdv \
	SUNWpmr

$(BUILD64)SUBDIRS64=$($(MACH64)_SUBDIRS)

SUBDIRS= $($(MACH)_SUBDIRS) $(COMMON_SUBDIRS) $(SUBDIRS64)

XMODS32= \
	SUNWcry \
	SUNWstatl \
	SUNWsvvs

$(BUILD64)XMODS64= SUNWcry64 SUNWsvvsx

XMODS= $(XMODS32) $(XMODS64)


all :=         		TARGET= all
install := 		TARGET= install
install_h :=            TARGET= install_h
clean :=                TARGET= clean
clobber :=              TARGET= clobber

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@

.KEEP_STATE:

.PARALLEL: $(SUBDIRS) $(XMODS)

all install: awk_pkginfo $(SUBDIRS) $(XMODS)

clean clobber: $(SUBDIRS) $(XMODS)
	$(RM) awk_pkginfo bld_awk_pkginfo

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) -i $(TARGET)

$(XMODS):       FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

awk_pkginfo: bld_awk_pkginfo
	./bld_awk_pkginfo -m $(MACH) -p "$(RELEASE)/$(VERSION)" -o $@

FRC:
