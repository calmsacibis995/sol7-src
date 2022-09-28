#
# Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)liblddbg.msg	1.28	98/02/02 SMI"

@ _START_

# Message file for cmd/sgs/liblddbg.

@ MSG_ID_LIBLDDBG


# Usage messages

@ MSG_USE_UNRECOG	"warning: unrecognized debug option `%s' (try help)"

# TRANSLATION_NOTE - Use the following output in C as a reference, 1.
#
# 		 For debugging the runtime linking of an application:
# 			LD_DEBUG=token1,token2  prog
# 		 enables diagnostics to the stderr.  The additional option:
# 			LD_DEBUG_OUTPUT=file
# 		 redirects the diagnostics to an output file created using
# 		 the specified name and the process id as a suffix.  All
# 		 diagnostics are prepended with the process id.
# 
# 		 For debugging the link-editing of an application:
# 			LD_OPTIONS=-Dtoken1,token2 cc -o prog ...
# 		 or,
# 			ld -Dtoken1,token2 -o prog ...
# 		 where placement of -D on the command line is significant,
# 		 and options can be switched off by prepending with `!'.
#
#	The next 13 messages makes the above output in C locale.
#	Use the tabs as alignment characters as needed.
#
@ MSG_USE_RTLD_A	"\t\t For debugging the runtime linking of an \
			 application:"
#
# Be wanred that the next two messages make a sentense.
# That is:
# 			LD_DEBUG=token1,token2  prog
# 		 enables diagnostics to the stderr.  The additional option:
# So do not translate the msgid separately, but handle them together
# Same kind of sentenses appear in this block of messages.
#
@ MSG_USE_RTLD_B	"\t\t\tLD_DEBUG=token1,token2  prog"
@ MSG_USE_RTLD_C	"\t\t enables diagnostics to the stderr.  \
			 The additional option:"
@ MSG_USE_RTLD_D	"\t\t\tLD_DEBUG_OUTPUT=file"
@ MSG_USE_RTLD_E	"\t\t redirects the diagnostics to an output file \
			 created using"
@ MSG_USE_RTLD_F	"\t\t the specified name and the process id as a \
			 suffix.  All"
@ MSG_USE_RTLD_G	"\t\t diagnostics are prepended with the process id."

@ MSG_USE_LD_A		"\t\t For debugging the link-editing of an application:"
@ MSG_USE_LD_B		"\t\t\tLD_OPTIONS=-Dtoken1,token2 cc -o prog ..."
@ MSG_USE_LD_C		"\t\t or,"
@ MSG_USE_LD_D		"\t\t\tld -Dtoken1,token2 -o prog ..."
@ MSG_USE_LD_E		"\t\t where placement of -D on the command line is \
			 significant,"
@ MSG_USE_LD_F		"\t\t and options can be switched off by prepending \
			 with `!'."
#
#	END OF reference 1
#


# TRANSLATION_NOTE
#	The next 19 messages make the following usage table.
#	Tabs are used for alignment.
#
# args	 display input argument processing (ld only)
# basic	 provide basic trace information/warnings
# bindings  display symbol binding; detail flag shows absolute:relative
# 		 addresses (ld.so.1 only)
# detail	 provide more information in conjunction with other options
# entry	 display entrance criteria descriptors (ld only)
# files	 display input file processing (files and libraries)
# help	 display this help message
# libs	 display library search paths; detail flag shows actual
# 		 library lookup (-l) processing
# map	 display map file processing (ld only)
# reloc	 display relocation processing
# sections  display input section processing (ld only)
# segments  display available output segments and address/offset
# 		 processing; detail flag shows associated sections (ld only)
# support	 display support library processing (ld only)
# symbols	 display symbol table processing;
# 		 detail flag shows resolution and linker table addition
# versions	 display version processing
# audit	 display rt-link audit processing
# got	display GOT symbol information (ld only)
#
#
# TRANSLATION_NOTE -- do not translate the first token `args'.
@ MSG_USE_ARGS		"args\t display input argument processing (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `basic'.
@ MSG_USE_AUDIT		"audit\t display rt-link audit processing"
@ MSG_USE_BASIC		"basic\t provide basic trace information/warnings"

# TRANSLATION_NOTE -- do not translate the first token `bindings'.
@ MSG_USE_BINDINGS	"bindings  display symbol binding; detail flag shows \
			 absolute:relative"
@ MSG_USE_BINDINGS_2	"\t\t addresses (ld.so.1 only)"

# TRANSLATION_NOTE -- do not translate the first token `detail'.
@ MSG_USE_DETAIL	"detail\t provide more information in conjunction with \
			 other options"

# TRANSLATION_NOTE -- do not translate the first token `entry'.
@ MSG_USE_ENTRY		"entry\t display entrance criteria descriptors \
			 (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `files'.
@ MSG_USE_FILES		"files\t display input file processing \
			 (files and libraries)"

# TRANSLATION_NOTE -- do not translate the first token `help'.
@ MSG_USE_HELP		"help\t display this help message"

# TRANSLATION_NOTE -- do not translate the first token `libs'.
@ MSG_USE_LIBS		"libs\t display library search paths; detail flag \
			 shows actual"
@ MSG_USE_LIBS_2	"\t\t library lookup (-l) processing"

# TRANSLATION_NOTE -- do not translate the first token `map'.
@ MSG_USE_MAP		"map\t display map file processing (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `reloc'.
@ MSG_USE_RELOC		"reloc\t display relocation processing"

# TRANSLATION_NOTE -- do not translate the first token `sections'.
@ MSG_USE_SECTIONS	"sections  display input section processing (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `segments'.
@ MSG_USE_SEGMENTS	"segments  display available output segments and \
			 address/offset"
@ MSG_USE_SEGMENTS_2	"\t\t processing; detail flag shows associated \
			 sections (ld only)"
# TRANSLATION_NOTE -- do not translate the first token `got'.
@ MSG_USE_GOT		"got\t display GOT symbol information (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `support'.
@ MSG_USE_SUPPORT	"support\t display support library processing (ld only)"

# TRANSLATION_NOTE -- do not translate the first token `symbols'.
@ MSG_USE_SYMBOLS	"symbols\t display symbol table processing;"
@ MSG_USE_SYMBOLS_2	"\t\t detail flag shows resolution and linker table \
			 addition"

# TRANSLATION_NOTE -- do not translate the first token `versions'.
@ MSG_USE_VERSIONS	"versions\t display version processing"


# Argument messages

@ MSG_ARG_FLAG		"arg[%d]\tflag=-%c"
@ MSG_ARG_FILE		"arg[%d]\tfile=%s"


# Bindings messages

@ MSG_BND_TITLE		"binding file=%s to file=%s: symbol `%s'"
@ MSG_BND_PLT		"binding file=%s (%#llx:%#llx) at plt[%d] to file=%s \
			 (%#llx:%#llx): symbol `%s'"
@ MSG_BND_DLSYM		"binding file=%s (dlsym) to file=%s \
			 (%#llx:%#llx): symbol `%s'"
@ MSG_BND_DEFAULT	"binding file=%s (%#llx:%#llx) to file=%s \
			 (%#llx:%#llx): symbol `%s'"
@ MSG_BND_WEAK_1	"binding file=%s to 0x0 (undefined weak): symbol `%s'"
@ MSG_BND_WEAK_2	"binding file=%s (%#llx:%#llx) to 0x0 (undefined weak): \
			 symbol `%s'"

@ MSG_BND_PROFILE	"    profiling symbol[%d];  call count=%d"





@ MSG_REL_COLLECT	"collecting input relocations: section=%s, file=%s"
@ MSG_REL_ACTIVE	"performing active relocations"
@ MSG_REL_CREATING	"creating output relocations"
@ MSG_REL_RELOC		"relocation processing: file=%s"
@ MSG_REL_RELOC_TAIL	"relocation processing: file=%s; finished"

@ MSG_REL_NOLAZY	"file `%s' can not be lazy loaded because it has a\n\
			 \tdata bindings against symbol: `%s'\n\
			 \trelocation type: `%s'"

# Dynamic section messages

@ MSG_DYN_TITLE		"     index  tag           value"
@ MSG_DYN_ENTRY		"%10.10s  %-12.12s  %-10#llx    %s"


# Entrance criteria messages

@ MSG_ECR_TITLE		"%s Entrance Descriptor List (available)"
@ MSG_ECR_DYNAMIC	"Dynamic"
@ MSG_ECR_STATIC	"Static"
@ MSG_ECR_DESC		"entrance descriptor[%d]"


# Elf section messages

@ MSG_ELF_TITLE		"            addr type        size   offset al file"
@ MSG_ELF_ENTRY		"  %3s %#10llx %-5s %#10llx %#8llx %2lld %s"
@ MSG_ELF_HEADER	"ELF Header"


# File messages

@ MSG_FIL_BASIC		"file=%s  [ %s ]"
@ MSG_FIL_ARCHIVE	"file=%s  [ archive ] %s"
@ MSG_FIL_SKIP_1	"file=%s  skipped: already processed as %s"
@ MSG_FIL_SKIP_2	"file=%s  skipped: already processed"
@ MSG_FIL_REUSE		"file=%s  reusing: originally processed as %s"
@ MSG_FIL_UNUSED	"file=%s  unused: does not satisfy any references"
@ MSG_FIL_ANALYZE	"file=%s;  analyzing  %s"
@ MSG_FIL_PROT		"file=%s;  modifying memory protections (%c PROT_WRITE)"
@ MSG_FIL_DELETE	"file=%s;  deleting"
@ MSG_FIL_DLOPEN	"file=%s;  dlopen() called from file=%s  %s"
@ MSG_FIL_DLCLOSE	"file=%s;  dlclose()"
@ MSG_FIL_DLDUMP	"file=%s;  dldump() to file=%s  %s"
@ MSG_FIL_LAZYLOAD	"file=%s;  lazy loaded; bindings from file=`%s' \
			 sym=`%s'"
@ MSG_FIL_PRELOAD	"file=%s;  preloaded"
@ MSG_FIL_NEEDED	"file=%s;  needed by %s"
@ MSG_FIL_FILTER_1	"file=%s;  filtered by %s"
@ MSG_FIL_FILTER_2	"file=%s;  filtee does not exist"
@ MSG_FIL_FIXNAME	"file=%s;  required name=%s"
@ MSG_FIL_AOUT		"file=%s  [ AOUT ]; generating link map"
@ MSG_FIL_CACHE		"file=%s  [ ELF ]; cache version found: %s"
@ MSG_FIL_ELF		"file=%s  [ ELF ]; generating link map"
@ MSG_FIL_LDSO		"file=%s  [ ELF ]"

@ MSG_FIL_DATA_1	"    dynamic:  %#10llx  base:  %#10llx  size:   %#10llx"
@ MSG_FIL_DATA_2	"    entry:    %#10llx  phdr:  %#10llx  phnum:  %10d"
@ MSG_FIL_DATA_3	"     lmid:    %#10llx"
@ MSG_FIL_DATA_4	"    dynamic:  %#10llx  base:  %#10llx"
@ MSG_FIL_DATA_5	"    envp:     %#10llx  auxv:  %#10llx"

@ MSG_FIL_REFCNT_1	"    refcnt:       UNUSED  for file=%s"
@ MSG_FIL_REFCNT_2	"    refcnt:   %10d  for file=%s"
@ MSG_FIL_PERMIT_1	"    permit:   0x%8.8llx  %s"
@ MSG_FIL_PERMIT_2	"              0x%8.8llx"
@ MSG_FIL_PERMIT_3	"    permit:       UNUSED  %s"

# _CHKMSG_SKIP_BEGIN_ i386
# _CHKMSG_SKIP_BEGIN_ sparc
@ MSG_FIL_BND_NEED	"file=%s;  establish explicit (needed) dependencies"
@ MSG_FIL_BND_SYM	"file=%s;  add implicit (reference to external symbol) \
			 dependencies""
@ MSG_FIL_BND_DELETE	"file=%s;  remove explicit and implicit dependencies"
@ MSG_FIL_BND_DLOPEN	"file=%s;  establish dlopen/group dependencies"
@ MSG_FIL_BND_DLCLOSE	"file=%s;  remove dlopen dependencies"
@ MSG_FIL_BND_UNPERMIT	"file=%s;  remove unnecessary permits"
# _CHKMSG_SKIP_END_ sparc
# _CHKMSG_SKIP_END_ i386

@ MSG_FIL_CACHE_ERR	"cache file=%s: %s"
@ MSG_FIL_CACHE_ERR_1	"unable to process file"
@ MSG_FIL_CACHE_ERR_2	"corrupt or truncated file"
@ MSG_FIL_CACHE_ERR_3	"could not map appropriately"
@ MSG_FIL_CACHE_ERR_4	"unable to reserve mapping"

@ MSG_FIL_GLOBAL	"[ GLOBAL ]"

@ MSG_FIL_ERR_REJECTED	"file=%s  rejected"
@ MSG_FIL_ERR_MACH	"file=%s  rejected: machine type mismatch: %s"
@ MSG_FIL_ERR_CLASS	"file=%s  rejected: ELF class mismatch: 32-bit/64-bit"
@ MSG_FIL_ERR_LIBVER	"file=%s  rejected: libelf version mismatch: %d"
@ MSG_FIL_ERR_DATA	"file=%s  rejected: data format mismatch: MSB/LSB"
@ MSG_FIL_ERR_FLAGS	"file=%s  rejected: bad ELF flags value: 0x%x"
@ MSG_FIL_ERR_BADMATCH	"file=%s  rejected: ELF flags mismatch: 0x%x"
@ MSG_FIL_ERR_HALFLAG	"file=%s  rejected: HaL R1 vendor extensions required"
@ MSG_FIL_ERR_TYPE	"file=%s  rejected: bad ELF type: %s"



# Libs messages

@ MSG_LIB_INITPATH	"Library Search Paths (initial)"
@ MSG_LIB_UPPATH	"Library Search Paths (-L updated)"
@ MSG_LIB_LOPT		"find lib=-l%s; path=%s"
@ MSG_LIB_REQUIRED	"find lib=%s; path=%s (required by %s)"
@ MSG_LIB_DEFAULT	" search path=%s  (default)"
@ MSG_LIB_ENVPATH	" search path=%s  (LD_LIBRARY_PATH)"
@ MSG_LIB_LIBPATH	" search path=%s  (LIBPATH or -YP)"
@ MSG_LIB_RPATH		" search path=%s  (RPATH from file %s)"
@ MSG_LIB_YPATH		" search path=%s  replaces  path=%s  (-Y%c)"

@ MSG_LIB_FIND		"find library=%s; searching"
@ MSG_LIB_TRYING	" trying path=%s"
@ MSG_LIB_IGNORE	" ignore path=%s  (insecure directory name)"


# Mapfile messages

@ MSG_MAP_MAPFILE	"map file=%s"

@ MSG_MAP_SEG_DECL_1	"segment declaration (=), segment added:"
@ MSG_MAP_SEG_DECL_2	"segment declaration (=), segment updated:"
@ MSG_MAP_SEG_DECL_3	"implicit segment declaration (:), segment added:"
@ MSG_MAP_SEG_DECL_4	"implicit segment declaration (@), segment added:"
@ MSG_MAP_SEG_DECL_5	"size-symbol declaration (@), segment updated:"

@ MSG_MAP_MAP_DIR	"mapping directive (:), entrance criteria added:"

@ MSG_MAP_SEC_ORDER	"map section ordering, segment: %s section: \
			 %s index: %d"

@ MSG_MAP_SYM_SCOPE	"symbol scope definition ({})"
@ MSG_MAP_SYM_SIZE	"size-symbol declaration (@), symbol=%s; %s"
@ MSG_MAP_SYM_VER_1	"%s, %s; symbol=%s  (%s)"
@ MSG_MAP_SYM_VER_2	"%s; symbol=%s  (%s)"

@ MSG_MAP_CNT_DEF_1	"library control definition (-), %s; %s=%s"
@ MSG_MAP_CNT_DEF_2	"library control definition (-), %s; %s"

@ MSG_MAP_SORTSEG	"map sort_seg_list(): original=%s"
@ MSG_MAP_SEGSORT	"map sort_seg_list(): sorted=%s"

# Section messages

@ MSG_SEC_INPUT		"section=%s; input from file=%s"
@ MSG_SEC_ADDED		"section=%s; added to segment=%s"
@ MSG_SEC_CREATED	"section=%s; added to segment=%s (created)"
@ MSG_SEC_DISCARDED	"section=%s; input from file=%s; \
			 section discarded in \
			favour of section=%s from file=%s"


# Segment messages

@ MSG_SEG_DESC_INUSE	"Segment Descriptor List (in use)"
@ MSG_SEG_DESC_AVAIL	"Segment Descriptor List (available)"


# Support messages

@ MSG_SUP_REQ		"support object request=%s  (%s)"
@ MSG_SUP_REQ_ENV	"supplied via SGS_SUPPORT"
@ MSG_SUP_REQ_CMD	"supplied via -S"
@ MSG_SUP_REQ_DEF	"default"

@ MSG_SUP_ROUTINE	"  support object=%s:  provides routine %s"

@ MSG_SUP_CALLING_1	"  calling routine=%s (%s)"
@ MSG_SUP_CALLING_2	"  calling routine=%s (%s)  %s=%s"
@ MSG_SUP_OUTFILE	"output file"
@ MSG_SUP_INFILE	"input file"
@ MSG_SUP_INSEC		"input section"



# Symbol strings
@ MSG_SYM_AR_FILE	"symbol table processing; input file=%s  [ archive ] %s"
@ MSG_SYM_AR_ENTRY	"archive[%lld]=%s"
@ MSG_SYM_AR_CHECK	"archive[%lld]=%s  (%s) checking for tentative override"
@ MSG_SYM_AR_RESOLVE	"archive[%lld]=%s  (%s) resolves undefined or tentative \
			 symbol"
@ MSG_SYM_AR_FORCEDEXRT	"archive[%lld]=%s  (%s) forced extraction"

@ MSG_SYM_SPECIAL	"symbol table processing; building special symbols"
@ MSG_SYM_PROCESS	"symbol table processing; input file=%s  [ %s ]"
@ MSG_SYM_FINAL		"symbol table processing; final update"
@ MSG_SYM_INDEX		"symbol table processing; determining section \
			 symbol's index"
@ MSG_SYM_BSS		"symbol table processing; assigning to bss \
			 (possible copy relocations)"
@ MSG_SYM_REDUCED	"symbol table processing; reducing global symbols"
@ MSG_SYM_VERSION	"symbol table processing; adding version symbols"

@ MSG_SYM_BASIC		"symbol[%lld]=%s"
@ MSG_SYM_ADDING	"symbol[%lld]=%s  (global); adding"
@ MSG_SYM_SECTION	"symbol[%d]=%s  (section); segment=%s"
@ MSG_SYM_RESOLVING	"symbol[%lld]=%s  (global); resolving [%d][%d]"
@ MSG_SYM_UPDATE	"symbol=%s;  updated"
@ MSG_SYM_CREATE	"symbol=%s;  creating"
@ MSG_SYM_REDUCING	"symbol=%s;  reducing to local"
@ MSG_SYM_ELIMINATING	"symbol=%s;  eliminating"
@ MSG_SYM_DISCARDED	"symbol=%s;  discarded because it is part of \
			 discarded section=%s from file=%s"

@ MSG_SYM_AOUT		"symbol=%s;  (original AOUT name)"
@ MSG_SYM_LOOKUP	"symbol=%s;  lookup in file=%s  [ %s ]"
@ MSG_SYM_DLSYM		"symbol=%s;  dlsym() starting at file=%s %s"

# Syminfo strings
@ MSG_SYMI_TITLE1	"\nSyminfo section: %s"
@ MSG_SYMI_TITLE2	"     index flgs boundto                symbol"
@ MSG_SYMI_DTITLE1	"Syminfo information"
@ MSG_SYMI_SELF		"<self>"
@ MSG_SYMI_PARENT	"<parent>"

# Version strings

@ MSG_VER_AVAIL_1	"version availability: file=%s"
@ MSG_VER_AVAIL_2	" available  version                     selected from"
@ MSG_VER_DEF_1		"version definition processing: file=%s"
@ MSG_VER_NEED_1	"version needed processing: file=%s"

# SHF_ORDERED related messages. Token used is sections.

@ MSG_ORDER_TO_BE_SORTED	"Output section to be sorted=%s"
@ MSG_ORDER_SORTED		"Output section sorted=%s"
@ MSG_ORDER_HDR_1		" No. of SHN_BEGIN=%d, SHN_AFTER=%d, sh_info=%d"
@ MSG_ORDER_TITLE_0		" section=%s from %s is not an ordered section."
@ MSG_ORDER_TITLE_1		" section=%s from %s, sh_info=SHN_BEGIN"
@ MSG_ORDER_TITLE_2		" section=%s from %s, sh_info=SHN_AFTER"
@ MSG_ORDER_TITLE_3		" section=%s from %s, sh_info=%s, sort_val=%d"

@ MSG_ORDER_ERR_TITLE		"The SHF_ORDERED section %s from %s has \
				 an error;  flag ignored"

# _CHKMSG_SKIP_BEGIN_ i386
# _CHKMSG_SKIP_BEGIN_ sparc
@ MSG_ORDER_ERR_INFO_RANGE	" The sh_info field is out of range"
@ MSG_ORDER_ERR_INFO_ORDER	" The section pointed by sh_info is an ordered section"
@ MSG_ORDER_ERR_LINK_OUTRANGE	" The sh_link field is out of range"
@ MSG_ORDER_ERR_FLAGS		" The sh_flag is incorrect"
@ MSG_ORDER_ERR_CYCLIC		" The sh_link is cyclic"
@ MSG_ORDER_ERR_LINK_ERROR	" A section pointed to by sh_link has an error"
# _CHKMSG_SKIP_END_ sparc
# _CHKMSG_SKIP_END_ i386

# Link-Auditing Messages

@ MSG_AUD_VERSION	"audit library %s: running at version %d"
@ MSG_AUD_BADVERS	"audit library %s: bad version reported %d: \
			 audit library disabled."
@ MSG_AUD_BOUND		"audit library %s: binding %s"
@ MSG_AUD_SYMNEW	"audit library %s: modified sym bind value, \
			 symbol: %s original=0x%llx new=0x%llx"
@ MSG_AUD_PLTBIND	"audit library %s: la_pltenter: sym: %s \
			 rebound to val: 0x%llx"
@ MSG_AUD_PLTENTER	"audit library %s: la_pltenter: sym: %s:0x%llx"
@ MSG_AUD_SYMBIND	"audit library %s: la_symbind: sym: %s symndx: %d"
@ MSG_AUD_LIBAUDIT	"audit enabled on library: %s"
@ MSG_AUD_INIT		"audit initialization: %s"

# GOT Messages

@ MSG_GOT_TITLE		"Global Offset Table: %d entries"

# Utility messages

@ MSG_UTL_LIST		"Input File List"
@ MSG_UTL_FILE		"input file=%s  (%d)"

@ MSG_UTL_TRANS		"transferring control: %s"
@ MSG_UTL_INIT		"calling init: %s"
@ MSG_UTL_FINI		"calling fini: %s"

# Generic strings

@ MSG_STR_AGAIN		"(again)"
@ MSG_STR_ENTERED	"entered"
@ MSG_STR_NULL		"(null)"
@ MSG_STR_IN		" in"
@ MSG_STR_OUT		"out"
@ MSG_STR_ACT		"act"
@ MSG_STR_OLD		"old"
@ MSG_STR_NEW		"new"
@ MSG_STR_RESOLVED	"resolved"
@ MSG_STR_UNKNOWN	"<unknown>"
@ MSG_STR_ADD		"adding"
@ MSG_STR_UP_1		"updating"
@ MSG_STR_UP_2		"updated"

@ _END_

# Debug enabling tokens (for now these are untranslated)

@ MSG_TOK_ALL		"all"
@ MSG_TOK_ARGS		"args"
@ MSG_TOK_BINDINGS	"bindings"
@ MSG_TOK_BASIC		"basic"
@ MSG_TOK_DETAIL	"detail"
@ MSG_TOK_ENTRY		"entry"
@ MSG_TOK_FILES		"files"
@ MSG_TOK_HELP		"help"
@ MSG_TOK_LIBS		"libs"
@ MSG_TOK_MAP		"map"
@ MSG_TOK_RELOC		"reloc"
@ MSG_TOK_SECTIONS	"sections"
@ MSG_TOK_SEGMENTS	"segments"
@ MSG_TOK_SUPPORT	"support"
@ MSG_TOK_SYMBOLS	"symbols"
@ MSG_TOK_VERSIONS	"versions"
@ MSG_TOK_AUDIT		"audit"
@ MSG_TOK_GOT		"got"


# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_COMMA		","

@ MSG_SCN_GOT		".got"
@ MSG_SCN_PLT		".plt"
@ MSG_SCN_BSS		".bss"

@ MSG_FMT_INDEX		" [%d]"
@ MSG_FMT_STR		"%s"

@ MSG_PTH_OBJECT	"/tmp/ld.so-OBJECT-"
@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_RTLD_64	"/usr/lib/sparcv9/ld.so.1"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"

# Entrance criteria messages

@ MSG_ECR_NAME		"  ec_name:       %-8s  ec_attrmask:  %s"
@ MSG_ECR_SEGMENT	"  ec_segment:    %-8s  ec_attrbits:  %s"
@ MSG_ECR_NDX		"  ec_ndx:        %-8d  ec_type:      %s"
@ MSG_ECR_FILES		"  ec_files:"
@ MSG_ECR_FILE		"    %s"

# Elf section messages

@ MSG_ELF_MAGIC		"  ei_magic:   { 0x%x, %c, %c, %c }"
@ MSG_ELF_CLASS		"  ei_class:   %-14s  ei_data:      %s"
@ MSG_ELF_MACHINE	"  e_machine:  %-14s  e_version:    %s"
@ MSG_ELF_TYPE		"  e_type:     %s"
@ MSG_ELF_FLAGS		"  e_flags:    %14s"
@ MSG_ELF_FLAGS_FMT	"  e_flags:    %s"
@ MSG_ELF_ESIZE		"  e_entry:    %#14llx  e_ehsize:     %2d  \
			 e_shstrndx:   %2d"
@ MSG_ELF_SHOFF		"  e_shoff:    %#14llx  e_shentsize:  %2d  \
			 e_shnum:      %2d"
@ MSG_ELF_PHOFF		"  e_phoff:    %#14llx  e_phentsize:  %2d  \
			 e_phnum:      %2d"

# Libs messages

@ MSG_LIB_FILE		"  %s"

# Mapfile messages

@ MSG_MAP_USED		"USED"
@ MSG_MAP_NEEDED	"NEEDED"

# Note messages

@ MSG_NOT_TYPE		"    type:         %#-8x"
@ MSG_NOT_NAME		"    %s"
@ MSG_NOT_DESC		"    desc[%d]:      %#x  (%d)"

# Program header messages

@ MSG_PHD_VADDR		"    p_vaddr:      %#-10llx  p_flags:    %s"
@ MSG_PHD_PADDR		"    p_paddr:      %#-10llx  p_type:     %s"
@ MSG_PHD_FILESZ	"    p_filesz:     %#-10llx  p_memsz:    %#llx"
@ MSG_PHD_OFFSET	"    p_offset:     %#-10llx  p_align:    %#llx"

# Section header messages

@ MSG_SHD_ADDR		"    sh_addr:      %#-10llx  sh_flags:   %s"
@ MSG_SHD_SIZE		"    sh_size:      %#-10llx  sh_type:    %s"
@ MSG_SHD_OFFSET	"    sh_offset:    %#-10llx  sh_entsize: %#llx"
@ MSG_SHD_ALIGN		"    sh_addralign: %#-10llx"
@ MSG_SHD_LINK		"    sh_link:      %#-10d  sh_info:    %d"

# Segment messages

@ MSG_SEG_NAME		"segment[%d] sg_name:  %s"
@ MSG_SEG_LENGTH	"    sg_length:    %#x"
@ MSG_SEG_FLAGS		"    sg_flags:     %s"
@ MSG_SEG_SIZESYM	"    sg_sizesym:   %s"
@ MSG_SEG_ORDER		"    sec_order:"
@ MSG_SEG_SECTION	"       sec_name:    %-8s  sec_index:   %d"

# Section messages (used when expanding segment information)

@ MSG_SEC_NAME		"  section[%d] os_name:  %s"

# Symbol strings

@ MSG_SYM_GLOBAL	"global"
@ MSG_SYM_LOCAL		"local"
# _CHKMSG_SKIP_BEGIN_ i386
# _CHKMSG_SKIP_BEGIN_ sparc
@ MSG_SYM_COPY		"copy rel"
@ MSG_SYM_NEXT		"[ RTLD_NEXT ]"
# _CHKMSG_SKIP_END_ sparc
# _CHKMSG_SKIP_END_ i386

# Utility messages

@ MSG_UTL_SEC_1		"  section[%d]=0"
@ MSG_UTL_SEC_2		"  section[%d]=%s"

#
# The following messages are placed here because:
# 	*) it is difficult to keep the table aligned, and to document thme.
#	*) the information here are mainly for programmers, and not for
#	   regular end-users.
#

# Relocation messages
@ MSG_REL_TITLE_1	"\t\ttype\t\t  offset      value  section        symbol"
@ MSG_REL_REL_TITLE_2	"\t\ttype\t\t  offset             section        symbol"
@ MSG_REL_RELA_TITLE_2	"\t\ttype\t\t  offset     addend  section        symbol"
@ MSG_REL_REL_TITLE_3	"\t\ttype\t\t  offset      value                 symbol"
@ MSG_REL_RELA_TITLE_3	"\t\ttype\t\t  offset     addend                 symbol"
@ MSG_REL_RELA_TITLE_4	"\t\t\t\t              value"
@ MSG_REL_ARGS_2	"\t\t\t      %10#llx %10#llx"
@ MSG_REL_ARGS_6	"  %3s %-16s %10#llx %10#llx  %-14.14s %s"
@ MSG_REL_ARGS_5	"  %3s %-16s %10#llx             %-14.14s %s"

# Symbol strings
@ MSG_SYM_TITLE		"%10.10s    value       size     type bind oth \
			 shndx       %s"
@ MSG_SYM_ENTRY		"%10.10s  0x%8.8llx 0x%8.8llx  %4s %4s %-3d %-11.11s %s"

# Version messages
@ MSG_VER_SELECTED	"  SELECTED  %-26.26s  %s"
@ MSG_VER_ALL		"       ALL  %-26.26s"
@ MSG_VER_DEF_2		"     index  version                     dependency"
@ MSG_VER_NEED_2	"            file                        version"
@ MSG_VER_LINE_1	"%10.10s  %-26.26s  %-20s %s"
@ MSG_VER_LINE_2	"%47s  %s"
@ MSG_VER_LINE_3	"%38s  %-20s %s"
@ MSG_VER_LINE_4	"%38s  %s"
@ MSG_VER_LINE_5	"            %-26.26s  %s"


# GOT messages
@ MSG_GOT_COLUMNS	"index\tref\tsymbol"
@ MSG_GOT_SMALL_PIC	"pic"
@ MSG_GOT_PIC		"PIC"
@ MSG_GOT_FORMAT1	"[%05d]\t%3s\t%s"
@ MSG_GOT_FORMAT2	"[%05d]\t%3s\t%s:%s"
@ MSG_GOT_ECOLUMNS	" ndx     addr      value    reloc              \
			 addend   symbol"
@ MSG_GOT_EFORMAT	"[%05d]  %08llx  %08llx %-18s %08llx %s"

@ MSG_SYMI_FMT1		" [%d]"
@ MSG_SYMI_FMT2		"[%d]"
@ MSG_SYMI_FMT3		"%10s %-4s %7s %-15s %s"
