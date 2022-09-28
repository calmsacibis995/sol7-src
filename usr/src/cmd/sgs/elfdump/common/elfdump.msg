#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)elfdump.msg	1.2	97/10/31 SMI"


@ _START_

# Message file for cmd/sgs/elfdump.

@ MSG_ID_ELFDUMP

# Usage Messages

@ MSG_USAGE_BRIEF	"usage: %s [-cdeihnprsvG] [-w file] [-N name] file(s)\n"
@ MSG_USAGE_DETAIL1	"\t[-c Dump section header information]\n"
@ MSG_USAGE_DETAIL2	"\t[-d Dump the contents of the .dynamic section]\n"
@ MSG_USAGE_DETAIL3	"\t[-e Dump the elf header.]\n"
@ MSG_USAGE_DETAIL4	"\t[-i Dump the contents of the .interp section]\n"
@ MSG_USAGE_DETAIL5	"\t[-h Dump the contents of the .hash section]\n"
@ MSG_USAGE_DETAIL6	"\t[-n Dump the contents of the .note section]\n"
@ MSG_USAGE_DETAIL7	"\t[-p Dump the program headers]\n"
@ MSG_USAGE_DETAIL8	"\t[-r Dump the contents of the relocation sections]\n"
@ MSG_USAGE_DETAIL9	"\t[-y Dump the contents fo the syminfo sectino]\n"
@ MSG_USAGE_DETAIL10	"\t[-s Dump the contents of the symbol table sections]\n"
@ MSG_USAGE_DETAIL11	"\t[-v Dump the contents of the version sections]\n"
@ MSG_USAGE_DETAIL12	"\t[-w Write the contents of specified section \
			 to file]\n"
@ MSG_USAGE_DETAIL13	"\t[-N qualify an option with a Name]\n"
@ MSG_USAGE_DETAIL14	"\t[-G Dump the contents of the GOT section]\n"


# Errors

@ MSG_ERR_BADFILE	"file %s: invalid file type\n"
@ MSG_ERR_ZEROLINK	"file %s: non-zero sh_link field expected: %s\n"
@ MSG_ERR_FAILURE	"%s: %s failed: %s\n"
@ MSG_ERR_SIBADCOUNT	"%s: %s bad syminfo index: %d\n"
@ MSG_ERR_BADSYM	"%s: %s bad symbol index: %d\n"
@ MSG_ERR_BADDYN	"%s: %s bad index into .dynamic: %d\n"


# Elf Output Messages

@ MSG_ELF_SCN_HEADER	"Section Header[%d]:  sh_name: %s"
@ MSG_ELF_SCN_INTERP	"Interpreter Section:  .interp"
@ MSG_ELF_SCN_VERDEF	"Version Definition Section:  %s"
@ MSG_ELF_SCN_VERNEED	"Version Needed Section:  %s"
@ MSG_ELF_SCN_SYMTAB	"Symbol Table:  %s"
@ MSG_ELF_SCN_RELOC	"Relocation: %s"
@ MSG_ELF_SCN_DYNAMIC	"Dynamic Section:  %s"
@ MSG_ELF_SCN_NOTE	"Note Section:  %s"
@ MSG_ELF_SCN_HASH	"Hash Section:  %s  "
@ MSG_ELF_SCN_PHDRS	"Program Header[%d]:"


@ MSG_ELF_RELOC_RELA	"\ttype\t\t   offset     addend  section        with respect to"
@ MSG_ELF_RELOC_REL	"\ttype\t\t   offset             section        with respect to"
@ MSG_ELF_HASH_BKTS1	"%10.10s  buckets contain %8d symbols"
@ MSG_ELF_HASH_BKTS2	"%10.10s  buckets         %8d symbols (globals)"
@ MSG_ELF_HASH_INFO	"    bucket    symndx    name"
@ MSG_HASH_OVERFLW	"warning: too many symbols to count, bucket=%d count=%d"
@ MSG_ELF_ERR_SHDR	"\tunable to obtain section header: shstrtab[%d]\n"
@ MSG_ELF_ERR_DATA	"\tunable to obtain section data: shstrtab[%d]\n"
@ MSG_ELF_ERR_SCN	"\tunable to obtain section header: section[%d]\n"
@ MSG_ELF_ERR_SCNDATA	"\tunable to obtain section data: section[%d]\n"
@ MSG_ARCHIVE_SYMTAB	"\nSymbol Table: (archive)"
@ MSG_ARCHIVE_FIELDS	"     index    offset    member name and symbol"
@ MSG_GOT_MULTIPLE_1	"file: %s: Multiple relocations against "
@ MSG_GOT_MULTIPLE_2	"the same GOT entry ndx: %d addr: 0x%llx\n"

# Miscellaneous clutter

@ MSG_STR_INDEX		"index"
@ MSG_STR_NAME		"name"
@ MSG_STR_NULL		"(null)"
@ MSG_STR_UNKNOWN	"<unknown>"
@ MSG_STR_SECTION	"%.12s (section)"

@ MSG_ERR_MALLOC	"file %s: malloc: %s\n"
@ MSG_ERR_GOT_CALLOC	"got() - file: %s calloc: %s\n"
@ MSG_ERR_OPEN		"file %s: open: %s\n"
@ MSG_ERR_USAGE		"options -w or -N must augment other options\n"


@ _END_

# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.


# Elf section names

@ MSG_ELF_INTERP	".interp"
@ MSG_ELF_GOT		".got"

# libelf symbol names
@ MSG_ELF_GETEHDR	"elf_getehdr"
@ MSG_ELF_GETPHDR	"elf_getphdr"
@ MSG_ELF_GETSHDR	"elf_getshdr"
@ MSG_ELF_GETSCN	"elf_getscn"
@ MSG_ELF_GETDATA	"elf_getdata"
@ MSG_ELF_GETARHDR	"elf_getarhdr"
@ MSG_ELF_GETARSYM	"elf_getarsym"
@ MSG_ELF_RAND		"elf_rand"
@ MSG_ELF_BEGIN		"elf_begin"
@ MSG_ELF_ARSYM		"ARSYM"

@ MSG_GOT_SYM_NAME	"_GLOBAL_OFFSET_TABLE_"


@ MSG_STR_OPTIONS	"cdeihnprsw:vyGN:"

@ MSG_STR_EMPTY		""
@ MSG_STR_NL		"\n"


@ MSG_FMT_INDENT	"	%s"
@ MSG_FMT_INDEX		" [%d]"
@ MSG_FMT_INDEX2	"[%d]"
@ MSG_FMT_INTEGER	" %d"
@ MSG_FMT_UNSIGNED	" %u"
@ MSG_FMT_HASH_INFO	"%10.10s  %-10s  %s"
@ MSG_FMT_ARSYM1	"%10.10s  0x%8.8x  (%s):%s"
@ MSG_FMT_ARSYM2	"%10.10s  0x%8.8x"
@ MSG_FMT_ARNAME	"%s(%s)"
@ MSG_FMT_NLSTR		"\n%s:"
@ MSG_FMT_NLSTRNL	"\n%s:\n"


@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
