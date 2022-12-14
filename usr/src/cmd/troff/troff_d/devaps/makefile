#
# ident	"@(#)Makefile	1.17	97/10/29 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/troff/troff.d/devaps/Makefile
#

include 	../../../Makefile.cmd

PROG =		daps

MAKEDEV =	./makedev

DAPSSRCS =	daps.c build.c
SRCS=		$(DAPSSRCS)

OBJS =		$(SRCS:%.c=%.o)

COMMONOBJS =	../draw.o

F1 =		B I R S CT CW CX GB GI GR GS	\
		HI HK HX PO PX S1 SC SM TX
F2 =		DESC C G H BI CE CI HB HL MB MI MR MX PA PB PI TB
F3 =		HM
OTHERFILES =	version
TXTS =		README LINKFILE

OUTF1 =		$(F1:%=%.out)
OUTF2 =		$(F2:%=%.out)
OUTF3 =		$(F3:%=%.out)
OUTFILES =	$(OUTF1) $(OUTF2) $(OUTF3)

ADDFILES =	$(F1:%=%.add)

ROOTFONT =	$(ROOTLIB)/font
ROOTDEVAPS =	$(ROOTFONT)/devaps

# Don"t re-install /usr/lib/font, Targetdirs installs it
#ROOTDIRS =	$(ROOTFONT) $(ROOTDEVAPS)
ROOTDIRS =	$(ROOTDEVAPS)

ROOTFILES =	$(OUTFILES:%=$(ROOTDEVAPS)/%) 	\
		$(ADDFILES:%=$(ROOTDEVAPS)/%)	\
		$(OTHERFILES:%=$(ROOTDEVAPS)/%)

FILEMODE =	644

CPPFLAGS =	-I. -I.. $(CPPFLAGS.master)
LDLIBS +=	-lm

$(MAKEDEV) :=	CPPFLAGS = -I. -I..
$(MAKEDEV) :=	LDLIBS   =

$(PROG) :=		POBJS = $(COMMONOBJS) $(DAPSSRCS:%.c=%.o)

$(ROOTPROG) := FILEMODE = 555

.KEEP_STATE:

.PARALLEL: $(OBJS)

all :		$(TXTS) $(OTHERFILES) $(PROG) $(MAKEDEV) $(OUTFILES) $(ADDFILES)

$(PROG) :	$$(POBJS)
		$(LINK.c) -o $@ $(POBJS) $(LDLIBS)
		$(POST_PROCESS)

$(MAKEDEV):	$$@.c
		$(NATIVECC) -O $(CPPFLAGS) -o $@ $@.c

$(COMMONOBJS) :	$$(@:%.o=%.c)
		cd $(@D); $(MAKE) $(@F)

$(OUTF1) \
$(OUTF2) :	$$(@:%.out=%) $(MAKEDEV)
		$(MAKEDEV) $(@:%.out=%)

# ADDFILES are built indirectly from building OUTF1, but they must
# appear as a target to the NSE
#
$(ADDFILES) :	$(OUTF1)
		@:

$(OUTF3) :	HB.out
		$(RM) $@; $(LN) HB.out $@

install:	all $(ROOTDIRS) $(ROOTPROG) $(ROOTFILES)

$(ROOTDIRS):
		$(INS.dir)

$(ROOTDEVAPS)/%:	% $(ROOTDIRS)
		$(INS.file)

clean:
		$(RM) $(OBJS)

clobber:	clean
		$(RM) $(ADDFILES) $(OUTFILES) $(PROG) $(MAKEDEV)

strip:
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(PROG).c
