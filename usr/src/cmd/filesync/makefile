#
#ident	"@(#)Makefile	1.5	97/04/21 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= filesync

OBJS= action.o anal.o base.o debug.o files.o ignore.o main.o recon.o rename.o rules.o acls.o eval.o
SRCS= $(OBJS:%.o=%.c)
HDRS= filesync.h messages.h database.h

include ../Makefile.cmd

LDLIBS += -lgen

FILEMODE= 0555
OWNER= bin
GROUP= bin

.KEEP_STATE:

all: $(PROG)

$(OBJS): $(HDRS)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

#
# we need our own rule for building a message catalog because our strings 
# are already extracted, and xgettext expects to find them in situ.
#
#	The sed command turns each string define in messages.h into
#	a call to gettext, so that xgettext can extract them.  Also
#	we capture all comments, since the only comments in messages.h
#	are for the benefit of the translators.
#
$(POFILE): messages.h
	sed -e "s/#define/gettext(/" -e "/gettext/s/$$/)/" $? | \
	xgettext -c "" -d $(TEXT_DOMAIN) -
	mv $(TEXT_DOMAIN).po $@

include ../Makefile.targ
