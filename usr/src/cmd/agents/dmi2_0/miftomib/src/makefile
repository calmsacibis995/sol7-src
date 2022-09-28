# Copyright 12/06/96 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.24 96/12/06 Sun Microsystems"

include ../../Makefile.incl.sun

# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.cmd
# include $(OPTIONAL_INCLUDE_FILE)

include $(SRC)/cmd/Makefile.cmd

PROTOINCLUDES = $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3)
INCLUDES += $(PROTOINCLUDES) \
            -I../../mapper/include/common \
	    -I../../mapper/include/dmi1.1 \
	    -I../../include \
	    -I../../mapper/include/nls/en_US

CFLAGS +=  -DMIFTOMIB -DPSL_BUILD -DAIX $(INCLUDES)

MIFTOMIB_SRCS = \
	miftomib.c \
	biftomib.c \
	os_util.c

MIFTOMIB_OBJS = $(MIFTOMIB_SRCS:%.c=$(OBJ_DIR)/%.o)

COMMONSRCS =  \
	pr_enum.c \
	pr_comp.c\
	pr_err.c \
	pr_attr.c \
	pr_class.c \
	pr_key.c \
	pr_keytb.c \
	pr_table.c \
	pr_group.c \
	pr_path.c \
	pr_plib.c \
	pr_tok.c \
	pr_parse.c \
	pr_src.c \
	pr_lex.c \
	pr_util.c \
	mif_db.c 

COMMONOBJS = $(COMMONSRCS:%.c=$(OBJ_DIR)/%.o)

MIFTOMIB_INCLUDES = ../include/pr_main.h \
		    ../include/pr_parse.h \
		    ../include/pr_src.h \
		    ../include/pr_tok.h \
		    ../include/biftomib.h \
		    ../include/pr_plib.h

BIFTOMIB_INCLUDES = ../include/pr_comp.h \
		    ../include/pr_group.h \
		    ../include/pr_attr.h \
		    ../include/pr_enum.h \
 		    ../include/pr_todmi.h \
 		    ../include/pr_lex.h \
		    ../include/pr_key.h

PR_COMP_INCLUDES = ../include/pr_path.h \
		   ../include/pr_table.h

PR_ERR_INCLUDES = ../../mapper/include/dmi1.1/io.h \
		  ../include/pr_class.h

PR_UTIL_INCLUDES = ../include/psl_util.h

PROG=$(TARGET)
CLOBBERFILES=*.c Makefile

MYPROG = miftomib
OPTBIN = $(ROOT)/usr/bin
OPTPROG = $(MYPROG:%=$(OPTBIN)/%)

SHAREDCFLAG=

$(OBJ_DIR)/%.o: $(COMMON_DIR)/src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.KEEP_STATE:

all: $(BIN) $(OBJ_DIR) miftomib

install: all 

#
# Need to use C++ for linking so that it knows where
# to find -lrwtool.
#

miftomib: $(MIFTOMIB_OBJS) $(COMMONOBJS)
	$(CCC) -ptr$(OBJ_DIR) -o $@ $(MIFTOMIB_OBJS) $(COMMONOBJS) $(LINK_FLAGS) $(LIBS)

miftomib.c: $(MIFTOMIB_INCLUDES)

biftomib.c: $(BIFTOMIB_INCLUDES)

pr_comp.c: $(PR_COMP_INCLUDES)

pr_err.c: $(PR_ERR_INCLUDES)

pr_util.c: $(PR_UTIL_INCLUDES)


clean: $(BIN)
	$(RM) miftomib $(MIFTOMIB_OBJS) $(COMMONOBJS)

clobber: clean

realclobber: clean
	sccs clean

$(BIN) $(OBJ_DIR):
	@test -d $@ || mkdir -p $@


# OPTIONAL_INCLUDE_FILE:sh = /bin/sh ../../../misc.sh ../../../../cmd/Makefile.targ
# include $(OPTIONAL_INCLUDE_FILE)
