#
# Copyright (c) 1991-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	97/07/29 SMI"
#
# cmd/sgs/tools/stabs/sparc/Makefile

STABS_SRC = ..

#
#       Define the module and object file sets.
#
STABSPROG	= stabs	  # name doesn't conflict with Makefile.master STABS
STABS_SRCS	= $(STABS_SRC)/main.c $(STABS_SRC)/stabs.c \
		  $(STABS_SRC)/forth.c $(STABS_SRC)/genassym.c
STABS_OBJS	= $(STABS_SRCS:$(STABS_SRC)/%.c=%.o)
LINTS		= $(STABS_OBJS:%.o=%.ln)

SRCS		= $(STABS_SRCS)
OBJECTS		= $(STABS_OBJS)

#
#       Include common rules.
#
CLEANFILES	= $(OBJECTS)

FILEMODE        = 755
OWNER		= bin
GROUP		= bin

include $(SRC)/Makefile.master

CFLAGS		+= -v

INC_PATH = -I$(STABS_SRC)

#
#       Default build targets.
#
.KEEP_STATE:

.PARALLEL: $(STABS_OBJS)

all: $(STABSPROG)

install:

lint:	$(SRCS)
	@$(LINT) $(LINTFLAGS) $(SRCS)

clean:
	$(RM) $(CLEANFILES)

clobber: clean
	$(RM) stabs

clean.lint:
	$(RM) $(LINTS)

$(STABSPROG): $(STABS_OBJS)
	$(LINK.c) -o $@ $(STABS_OBJS) -lm

main.o: $(STABS_SRC)/main.c $(STABS_SRC)/stabs.h
	$(COMPILE.c) $(INC_PATH) -c $(STABS_SRC)/main.c
stabs.o: $(STABS_SRC)/stabs.c $(STABS_SRC)/stabs.h
	$(COMPILE.c) $(INC_PATH) -c $(STABS_SRC)/stabs.c
forth.o: $(STABS_SRC)/forth.c $(STABS_SRC)/stabs.h
	$(COMPILE.c) $(INC_PATH) -c $(STABS_SRC)/forth.c
genassym.o: $(STABS_SRC)/genassym.c $(STABS_SRC)/stabs.h
	$(COMPILE.c) $(INC_PATH) -c $(STABS_SRC)/genassym.c
