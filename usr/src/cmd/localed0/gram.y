/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
%{
#pragma	ident	"@(#)gram.y 1.38	98/02/10  SMI"
%}

/*
 * COPYRIGHT NOTICE
 * 
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 * 
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF/1 1.2
 */
/* @(#)$RCSfile: gram.y,v $ $Revision: 1.4.7.7 $ (OSF) $Date: 1992/11/20 02:37:52 $ */

/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 * 
 * 1.10  com/cmd/nls/gram.y, cmdnls, bos320, 9137320a 9/4/91 13:41:56
 */
/* ----------------------------------------------------------------------
** Tokens for keywords
** ----------------------------------------------------------------------*/

/* keywords identifying the beginning and end of a locale category */
%token KW_END
%token KW_CHARMAP
%token KW_CHARSETID
%token KW_LC_COLLATE
%token KW_LC_CTYPE
%token KW_LC_MONETARY
%token KW_LC_NUMERIC
%token KW_LC_MSG
%token KW_LC_TIME
%token KW_METHODS
%token KW_DISPWID
%token KW_COPY
%token KW_LCBIND

/* keywords support the LCBIND category */
%token KW_CHARCLASS
%token KW_CHARTRANS

/* keywords supporting the LC_METHODS category */
%token KW_MBLEN
%token KW_MBTOWC
%token KW_MBSTOWCS
%token KW_WCTOMB
%token KW_WCSTOMBS
%token KW_WCWIDTH
%token KW_WCSWIDTH
%token KW_MBFTOWC
%token KW_FGETWC
%token KW_CSID
%token KW_WCSID
%token KW_TOWUPPER
%token KW_TOWLOWER
%token KW_WCTYPE
%token KW_ISWCTYPE
%token KW_STRCOLL
%token KW_STRXFRM
%token KW_WCSCOLL
%token KW_WCSXFRM
%token KW_REGCOMP
%token KW_REGEXEC
%token KW_REGFREE
%token KW_REGERROR
%token KW_STRFMON
%token KW_STRFTIME
%token KW_STRPTIME
%token KW_GETDATE
%token KW_WCSFTIME
%token KW_CSWIDTH
%token KW_EUCPCTOWC
%token KW_WCTOEUCPC
%token KW_TRWCTYPE
%token KW_TOWCTRANS
%token KW_WCTRANS
%token KW_FGETWC_AT_NATIVE
%token KW_ISWCTYPE_AT_NATIVE
%token KW_MBFTOWC_AT_NATIVE
%token KW_MBSTOWCS_AT_NATIVE
%token KW_MBTOWC_AT_NATIVE
%token KW_TOWLOWER_AT_NATIVE
%token KW_TOWUPPER_AT_NATIVE
%token KW_WCSCOLL_AT_NATIVE
%token KW_WCSTOMBS_AT_NATIVE
%token KW_WCSXFRM_AT_NATIVE
%token KW_WCTOMB_AT_NATIVE
%token KW_PROCESS_CODE
%token KW_EUC
%token KW_DENSE
%token KW_UCS4
%token KW_FILE_CODE
%token KW_UTF8
%token KW_OTHER
%token KW_WCWIDTH_AT_NATIVE
%token KW_WCSWIDTH_AT_NATIVE
%token KW_TOWCTRANS_AT_NATIVE
%token KW_BTOWC
%token KW_WCTOB
%token KW_MBSINIT
%token KW_MBRLEN
%token KW_MBRTOWC
%token KW_WCRTOMB
%token KW_MBSRTOWCS
%token KW_WCSRTOMBS
%token KW_BTOWC_AT_NATIVE
%token KW_WCTOB_AT_NATIVE
%token KW_MBRTOWC_AT_NATIVE
%token KW_WCRTOMB_AT_NATIVE
%token KW_MBSRTOWCS_AT_NATIVE
%token KW_WCSRTOMBS_AT_NATIVE

/* keywords support LC_COLLATE category */
%token KW_COLLATING_ELEMENT
%token KW_COLLATING_SYMBOL
%token KW_ORDER_START
%token KW_ORDER_END
%token KW_FORWARD
%token KW_BACKWARD
%token KW_NO_SUBSTITUTE
%token KW_POSITION
%token KW_WITH
%token KW_FROM
%token KW_SUBSTITUTE
%token KW_FNMATCH

/* keywords supporting LC_CTYPE category */
%token KW_ELLIPSES

/* keywords supporting the LC_MONETARY category */
%token KW_INT_CURR_SYMBOL
%token KW_CURRENCY_SYMBOL
%token KW_MON_DECIMAL_POINT
%token KW_MON_THOUSANDS_SEP
%token KW_MON_GROUPING
%token KW_POSITIVE_SIGN
%token KW_NEGATIVE_SIGN
%token KW_INT_FRAC_DIGITS
%token KW_FRAC_DIGITS
%token KW_P_CS_PRECEDES
%token KW_P_SEP_BY_SPACE
%token KW_N_CS_PRECEDES
%token KW_N_SEP_BY_SPACE
%token KW_P_SIGN_POSN
%token KW_N_SIGN_POSN
%token KW_DEBIT_SIGN
%token KW_CREDIT_SIGN
%token KW_LEFT_PARENTHESIS
%token KW_RIGHT_PARENTHESIS

/* keywords supporting the LC_NUMERIC category */
%token KW_DECIMAL_POINT
%token KW_THOUSANDS_SEP
%token KW_GROUPING

/* keywords supporting the LC_TIME category */
%token KW_ABDAY
%token KW_DAY
%token KW_ABMON
%token KW_MON
%token KW_D_T_FMT
%token KW_D_FMT
%token KW_T_FMT
%token KW_AM_PM
%token KW_ERA
%token KW_ERA_YEAR
%token KW_ERA_D_FMT
%token KW_ERA_T_FMT
%token KW_ERA_D_T_FMT
%token KW_ALT_DIGITS
%token KW_T_FMT_AMPM
%token KW_M_D_RECENT	/* OSF extension */
%token KW_M_D_OLD	/* OSF extension */
%token KW_DATE_FMT

/* keywords for the LC_MSG category */
%token KW_YESEXPR
%token KW_NOEXPR
%token KW_YESSTR
%token KW_NOSTR

/* tokens for meta-symbols */
%token KW_CODESET
%token KW_ESC_CHAR
%token KW_MB_CUR_MAX
%token KW_MB_CUR_MIN
%token KW_COMMENT_CHAR

/* tokens for user defined symbols, integer constants, etc... */
%token SYMBOL
%token STRING
%token INT_CONST
%token DEC_CONST
%token OCT_CONST
%token HEX_CONST
%token HEX_STRING
%token CHAR_CONST
%token LOC_NAME
%token CHAR_CLASS_SYMBOL
%token CHAR_TRANS_SYMBOL

%{

#include "symtab.h"
#include "method.h"
#include "semstack.h"
#include "err.h"
#include "locale.h"
#include <limits.h>
#include "locdef.h"

symtab_t cm_symtab;

extern _LC_charmap_t  charmap;
extern _LC_collate_t  collate;
extern _LC_ctype_t    ctype;
extern _LC_monetary_t monetary;
extern _LC_numeric_t  numeric; 
extern _LC_time_t     lc_time;
extern _LC_messages_t     messages;

/* sem_chr.c */
extern void	fill_euc_info(_LC_euc_info_t *);
extern void	check_digit_values(void);
extern void	sem_symbol_range_def_euc(void);
extern void	sem_symbol_range_def(void);
extern void	sem_symbol_def_euc(void);
extern void	sem_symbol_def(void);
extern void	sem_set_sym_val(char *, item_type_t);
extern void	sem_set_str(char **);
extern void	sem_set_diglist(char **);
extern void	sem_set_int(signed char *);
extern void	sem_set_str_lst(char **, int);
extern void	sem_set_str_cat(char **, int);
extern void	sem_digit_list(void);
extern void	sem_char_ref(void);
extern void	sem_existing_symbol(char *);
extern void	sem_symbol(char *);

/* sem_coll.c */
extern symbol_t	*sem_get_coll_tgt(void);
extern void	sem_collate(void);
extern void	sem_init_colltbl(void);
extern void	sem_spec_collsym(void);
extern void	sem_def_substr(void);
extern void	sem_set_dflt_collwgt(void);
extern void	sem_set_dflt_collwgt_range(void);
extern void	sem_setcollwgt(_LC_weight_t *);
extern void	sem_collel_list(_LC_weight_t *, symbol_t *, int);
extern void	sem_push_collel(void);
extern void	sem_coll_sym_ref(void);
extern void	sem_coll_literal_ref(void);
extern void	sem_def_collel(void);
extern void	sem_sort_spec(void);
extern void	setup_substr(void);
extern void	sem_set_collwgt(_LC_weight_t *);

/* check.c */
extern void	check_upper(void);
extern void	check_lower(void);
extern void	check_alpha(void);
extern void	check_space(void);
extern void	check_cntl(void);
extern void	check_punct(void);
extern void	check_graph(void);
extern void	check_print(void);
extern void	check_digits(void);
extern void	check_xdigit(void);

/* sem_ctype.c */
extern void	add_charclass(_LC_ctype_t *, struct lcbind_table *,
	_LC_bind_tag_t, int);
extern void	add_ctype(_LC_ctype_t *, struct lcbind_table *,
	char *);
extern void	sem_set_lcbind_symbolic_value(void);
extern void	add_char_ct_name(_LC_ctype_t *,	struct lcbind_table *,
	char *,	_LC_bind_tag_t,	char *,	unsigned int, int);
extern void	push_char_range(void);
extern void	push_char_sym(void);

/* sem_method.c */
extern void	check_methods(void);
extern void	set_method(int, int);

/* sem_xlat.c */
extern void	add_transformation(_LC_ctype_t *, struct lcbind_table *,
	char *);
extern void	sem_push_xlat(void);

/* copy.c */
extern void	copy_locale(int);

/* scan.c */
extern int	yylex(void);

extern char yytext[];
extern char sym_text[];
extern int  skip_to_EOL;
extern struct lcbind_table *Lcbind_Table;
extern int  Charmap_pass;
extern int  instring;		/* scan.c */
extern int	lp64p;			/* localedef.c */

static _LC_bind_tag_t lcbind_tag;
static char *ctype_symbol_name;
int method_class=SB_CODESET;
int mb_cur_max;
int max_disp_width=0;
static int sort_mask = 0;
static int cur_order = 0;
_LC_euc_info_t euc_info = {
	(char) 1, (char) 0, (char) 0, (char) 0,	/* EUC width info */
	(char) 1, (char) 0, (char) 0, (char) 0, /* screen width info */
	(wchar_t) 0, (wchar_t) 0, (wchar_t) 0,	/* cs1,cs2,cs3_base */
	(wchar_t) 0,				/* dense_end */
	(wchar_t) 0, (wchar_t) 0, (wchar_t) 0	/* cs1,cs2,cs3_adjustment */
};
int Native = TRUE;		/* if TRUE we are always in dense mode */

int Euc_filecode = FALSE;
int	Utf8_filecode = FALSE;
int	Other_filecode = FALSE;
int	Euc_proccode = FALSE;
int	Dense_proccode = FALSE;
int	Ucs4_proccode = FALSE;
int	Cswidth = FALSE;

/* Flags for determining if the category was empty when it was defined and
   if it has been defined before */

int lc_time_flag = 0;
int lc_monetary_flag = 0;
int lc_ctype_flag = 0;
int lc_message_flag = 0;
int lc_numeric_flag = 0;
int lc_collate_flag = 0;

int lc_has_collating_elements = 0;

int	args = 1;		/* number of arguments in method assign list */

static int arblen;
static int user_defined = 0;
static char *lcbind_charclass;

static _LC_weight_t weights;
static symbol_t *coll_tgt;

static int	new_extfmt = FALSE;
static int	old_extfmt = FALSE;
%}

%%
/* ----------------------------------------------------------------------
** GRAMMAR for files parsed by the localedef utility.  This grammar 
** supports both the CHARMAP and the LOCSRC definitions.  The 
** implementation will call yyparse() twice.  Once to parse the CHARMAP 
** file, and once to parse the LOCSRC file.
** ----------------------------------------------------------------------*/

file    : charmap
    	| category_list
        | method_def
	;

category_list :
   	category_list category
	| category
	;

/* ----------------------------------------------------------------------
** CHARMAP GRAMMAR 
**
** This grammar parses the charmap file as specified by POSIX 1003.2.
** ----------------------------------------------------------------------*/
charmap : charmap charmap_sect
	| charmap_sect
	;

charmap_sect : 
	metasymbol_assign
	| KW_CHARMAP '\n' charmap_stat_list KW_END KW_CHARMAP '\n'
	{
	    if (Charmap_pass == 1)
		fill_euc_info(&euc_info);
	    else if (Charmap_pass == 2)
		check_digit_values();
	}
        | charsets_def
	;

charmap_stat_list : 
        charmap_stat_list charmap_stat
        | charmap_stat
	;

charmap_stat :
	symbol_def
  	| symbol_range_def
	;

symbol_range_def :
	symbol KW_ELLIPSES symbol byte_list {skip_to_EOL++;} '\n'
	{
	    if (Charmap_pass == 1)
		sem_symbol_range_def_euc();
	    else if (Charmap_pass == 2)
		sem_symbol_range_def();
	}
	;

symbol_def :
	symbol byte_list {skip_to_EOL++;} '\n'
	{
	    if (Charmap_pass == 1)
		sem_symbol_def_euc();
	    else if (Charmap_pass == 2)
		sem_symbol_def();
	}
	;

metasymbol_assign : 
	KW_MB_CUR_MAX number '\n'
  	{
	    item_t *it;
	  
	    it = sem_pop();
	    if (it->type != SK_INT)
		INTERNAL_ERROR;

	    mb_cur_max		  = it->value.int_no;
	    charmap.cm_mb_cur_max = it->value.int_no;

	    (void) sem_push(it);

	    if (method_class != USR_CODESET) {
		if (mb_cur_max == 1)
		    method_class = SB_CODESET;
		else if ((mb_cur_max > 1) && (mb_cur_max <= MB_LEN_MAX))
		    method_class = MB_CODESET;
		else
		    INTERNAL_ERROR;
	    }
	    check_methods();  /* Insure that required methods are present */
	    sem_set_sym_val("<mb_cur_max>", SK_INT);
	}
	| KW_MB_CUR_MIN number '\n'
  	{
	    item_t *it;

	    it = sem_pop();
	    if (it->type != SK_INT)
	       INTERNAL_ERROR;
	    charmap.cm_mb_cur_min = it->value.int_no;
	    if (it->value.int_no != 1) {
		diag_error(ERR_INV_MB_CUR_MIN,it->value.int_no);
		destroy_item(it);
	    }
	    else {
	        (void) sem_push(it);
	        sem_set_sym_val("<mb_cur_min>", SK_INT);
	    }
	}
	| KW_CODESET text '\n'
  	{
	    item_t *it;
	    int i;

	    /* The code set name must consist of character in the PCS -
	       which is analagous to doing an isgraph in the C locale */

	    it = sem_pop();
	    if (it->type != SK_STR)
	       INTERNAL_ERROR;
	    for (i=0; yytext[i] != '\0'; i++){
	        if (!isgraph(yytext[i]))
		   error(ERR_INV_CODE_SET_NAME,yytext);
	    }
	    (void) sem_push(it);
	    sem_set_sym_val("<code_set_name>", SK_STR);
	    charmap.cm_csname = MALLOC(char,strlen(yytext)+1);
	    strcpy(charmap.cm_csname, yytext);
	}
	;

/* ----------------------------------------------------------------------
** LOCSRC GRAMMAR 
**
** This grammar parses the LOCSRC file as specified by POSIX 1003.2.
** ----------------------------------------------------------------------*/
category : regular_category 
	{
	    if (user_defined)
		diag_error(ERR_USER_DEF);
	}
	| non_reg_category
	;

regular_category : '\n' 
	| lc_collate
	| lc_ctype
	| lc_monetary
	| lc_numeric
	| lc_msg
	| lc_time
  	;

non_reg_category : unrecognized_cat
	;

/* ----------------------------------------------------------------------
** LC_COLLATE
**
** This section parses the LC_COLLATE category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_collate : 
	coll_sect_hdr coll_stats order_spec KW_END KW_LC_COLLATE '\n'
	{ 
	    sem_collate(); 
	}
        | coll_sect_hdr order_spec KW_END KW_LC_COLLATE '\n'
	{ 
	    sem_collate(); 
	}
	| coll_sect_hdr	KW_COPY locale_name '\n' KW_END KW_LC_COLLATE '\n'
	{
	    copy_locale(LC_COLLATE);
	}
	;

coll_sect_hdr :
    	KW_LC_COLLATE '\n'
	{
	  sem_init_colltbl();
	}
	;

coll_stats :
  	coll_stats coll_stat
	| coll_stat
	;

coll_stat : '\n'
        | KW_COLLATING_ELEMENT symbol KW_FROM string '\n'
    	{
	    sem_def_collel();
		lc_has_collating_elements = 1;
	}
	| KW_COLLATING_SYMBOL symbol '\n'
	{
	    sem_spec_collsym();
	}
	| KW_SUBSTITUTE string KW_WITH string '\n'
	{
	    sem_def_substr();
	}
	;

order_spec : 
	KW_ORDER_START sort_sect coll_spec_list KW_ORDER_END
	{
	    lc_collate_flag = 1;
	}
	| KW_ORDER_START sort_sect coll_spec_list KW_ORDER_END white_space
	{
	    lc_collate_flag = 1;
	}
  	;

white_space : white_space '\n'
        | '\n'
        ;

sort_sect : '\n'
	{
	    item_t *i;

	    i = create_item(SK_INT, _COLL_FORWARD_MASK);
	    (void) sem_push(i);

	    collate.co_nord++;

	    sem_sort_spec();
	    if (collate.co_nsubs >= 1)
	        setup_substr();
	}
	| sort_modifier_spec '\n'
	{
	    sem_sort_spec();
	    if (collate.co_nsubs >= 1)
	        setup_substr();
	}
	;


sort_modifier_spec :
	sort_modifier_spec ';' sort_modifier_list
	{
	    if (collate.co_nord == COLL_WEIGHTS_MAX) 
		diag_error(ERR_COLL_WEIGHTS);
	    collate.co_nord ++;
	}
	| sort_modifier_list
	{
	    if (collate.co_nord == COLL_WEIGHTS_MAX) 
		diag_error(ERR_COLL_WEIGHTS);
	    collate.co_nord ++;
	}
	;

sort_modifier_list :
	sort_modifier_list ',' sort_modifier
	{
	    item_t *i;
	
	    /* The forward and backward mask are mutually exclusive */
	    /* Ignore the second mask and continue processing       */

	    i = sem_pop();
	    if (((i->value.int_no & _COLL_FORWARD_MASK) 
	           && (sort_mask == _COLL_BACKWARD_MASK)) 
	       ||
	       ((i->value.int_no & _COLL_BACKWARD_MASK)
	          && (sort_mask == _COLL_FORWARD_MASK))) {
		diag_error(ERR_FORWARD_BACKWARD);
		(void) sem_push(i);
	    }
	    else {
	        i->value.int_no |= sort_mask;
	        (void) sem_push(i);
	    }
	}
	| sort_modifier
	{
	    item_t *i;

	    i = create_item(SK_INT, sort_mask);
	    (void) sem_push(i);
	}
	;

sort_modifier :
	KW_FORWARD             { sort_mask = _COLL_FORWARD_MASK;  }
	| KW_BACKWARD          { sort_mask = _COLL_BACKWARD_MASK; }
	| KW_NO_SUBSTITUTE     { sort_mask = _COLL_NOSUBS_MASK;   }
	| KW_POSITION          { sort_mask = _COLL_POSITION_MASK; }
	;

coll_spec_list :
        coll_spec_list coll_symbol_ref '\n'
  	{
	    sem_set_dflt_collwgt();
	}
	| coll_symbol_ref '\n'
  	{
	    sem_set_dflt_collwgt();
	}
	| coll_spec_list char_coll_range
	| char_coll_range
        | coll_spec_list coll_ell_spec
        | coll_ell_spec
        | coll_spec_list '\n'
        | '\n'
        ;

char_coll_range :
	coll_symbol_ref '\n' KW_ELLIPSES '\n' coll_symbol_ref '\n'
	{
	    sem_set_dflt_collwgt_range();
	}
        ;

coll_ell_spec :
       	coll_symbol_ref { coll_tgt = sem_get_coll_tgt(); } coll_rhs_list '\n'
    	{
	    sem_set_collwgt(&weights);
	}
	;

coll_rhs_list :
     	coll_rhs_list ';' coll_ell_list
	{
	    sem_collel_list(&weights, coll_tgt, ++cur_order);
	}
	| coll_ell_list
	{
	    sem_collel_list(&weights, coll_tgt, cur_order=0);
	}
	;

coll_ell_list :
	'"' coll_symbol_list '"'
	| coll_symbol_ref
	{
	    item_t *i;

	    i = create_item(SK_INT, 1);
	    sem_push_collel();
	    (void) sem_push(i);
	}
	;

coll_symbol_list :
	coll_symbol_list coll_symbol_ref
	{
	    item_t *i;

	    i = sem_pop();
	    if (i==NULL || i->type != SK_INT)
	  	INTERNAL_ERROR;
	    i->value.int_no++;
	    sem_push_collel();
	    (void) sem_push(i);
	}
	| coll_symbol_ref
	{
	    item_t *i;

	    i = create_item(SK_INT, 1);
	    sem_push_collel();
	    (void) sem_push(i);
	}
	;

coll_symbol_ref : 
        char_symbol_ref
        {   
	    sem_coll_sym_ref();
        }
	| byte_list
	{
	    sem_coll_literal_ref();
	    sem_coll_sym_ref();
	}
        ;
        
/* -----------------------------------------------------------------------
** LC_CTYPE
**
** This section parses the LC_CTYPE category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_ctype :
	KW_LC_CTYPE '\n' 
	{ 
	    /* The LC_CTYPE category can only be defined once in a file */

	    if (lc_ctype_flag)
	        diag_error(ERR_DUP_CATEGORY,"LC_CTYPE");
        } 
        lc_ctype_spec_list KW_END KW_LC_CTYPE '\n'
	{
	    /* A category with no text is an error (POSIX) */

	    if (!lc_ctype_flag)
	        diag_error(ERR_EMPTY_CAT,"LC_CTYPE");
	    else {
		check_upper();
		check_lower();
		check_alpha();
		check_space();
		check_cntl();
		check_punct();
		check_graph();
		check_print();
		check_digits();
		check_xdigit();
	    }
	}
	| KW_LC_CTYPE '\n' KW_COPY locale_name '\n' KW_END KW_LC_CTYPE '\n'
	{
	    copy_locale(LC_CTYPE);
	}
	| KW_LC_CTYPE '\n' KW_END KW_LC_CTYPE '\n'
	{
	    lc_ctype_flag = 1;

	    /* A category with no text is an error (POSIX) */

	    diag_error(ERR_EMPTY_CAT,"LC_CTYPE");

	}
	;

lc_ctype_spec_list :
	lc_ctype_spec_list lc_ctype_spec
	| lc_ctype_spec
	;

/* craigm */
lc_ctype_spec : '\n'
	| KW_CHARCLASS {arblen=0;} charclass_keywords '\n'
	{
	    lc_ctype_flag = 1;
	    add_charclass(&ctype, Lcbind_Table, _LC_TAG_CCLASS, arblen);
	}
/*	| charclass_keyword char_range_list '\n' */
	| charclass_kw char_range_list '\n'
	{
	    lc_ctype_flag = 1;
	    add_ctype(&ctype, Lcbind_Table, ctype_symbol_name);
	    free(ctype_symbol_name);
	}
	| charclass_kw '\n'
	{
	    lc_ctype_flag = 1;
	    add_ctype(&ctype, Lcbind_Table, ctype_symbol_name);
	    free(ctype_symbol_name);
	}
	| KW_CHARTRANS {arblen=0;} charclass_keywords '\n'
	{
	    lc_ctype_flag = 1;
	    add_charclass(&ctype, Lcbind_Table, _LC_TAG_TRANS, arblen);
	}
	| chartrans_kw char_pair_list '\n'
	{
	    lc_ctype_flag = 1;
	    add_transformation(&ctype, Lcbind_Table, ctype_symbol_name);
	    free(ctype_symbol_name);
	}
	;

charclass_keywords :
	charclass_keywords ';' charclass_keyword
	{
	    arblen++;
	}
	| charclass_keyword
	{
	    arblen++;
	}
	;

charclass_keyword :
	SYMBOL
	{
	    item_t *i;

	    i = create_item(SK_STR, sym_text);
	    (void) sem_push(i);
	}
	| CHAR_CLASS_SYMBOL
	{
	    item_t *i;

	    i = create_item(SK_STR, sym_text);
	    (void) sem_push(i);
	}
	| CHAR_TRANS_SYMBOL
	{
	    item_t *i;

	    i = create_item(SK_STR, sym_text);
	    (void) sem_push(i);
	}
	;

charclass_kw :
	CHAR_CLASS_SYMBOL
	{
	    ctype_symbol_name = strdup(sym_text);
	}
	;

chartrans_kw :
	CHAR_TRANS_SYMBOL
	{
	    ctype_symbol_name = strdup(sym_text);
	}
	;

char_pair_list : char_pair_list ';' char_pair
       	| char_pair
	;

char_pair : '(' char_ref ',' char_ref ')'
        {
	    sem_push_xlat();
        }
	;

/* ----------------------------------------------------------------------
** LC_MONETARY
**
** This section parses the LC_MONETARY category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_monetary :
	KW_LC_MONETARY '\n' 
	{
	    /*  The LC_MONETARY category can only be defined once in a */
	    /*  locale						       */

	    if (lc_monetary_flag)
		diag_error(ERR_DUP_CATEGORY,"LC_MONETARY");
	 
	}
        lc_monetary_spec_list KW_END KW_LC_MONETARY '\n'
	{
	    /* A category must have at least one line of text (POSIX) */

	    if (!lc_monetary_flag)
	        diag_error(ERR_EMPTY_CAT,"LC_MONETARY");
	}
	| KW_LC_MONETARY '\n' KW_COPY locale_name '\n' KW_END KW_LC_MONETARY '\n'
	{
	    copy_locale(LC_MONETARY);
	}
	| KW_LC_MONETARY '\n' KW_END KW_LC_MONETARY '\n'
	{
	    lc_monetary_flag++;

	    /* A category must have at least one line of text (POSIX  */

	    diag_error(ERR_EMPTY_CAT,"LC_MONETARY");

	}
	;


lc_monetary_spec_list :
	lc_monetary_spec_list lc_monetary_spec
        | lc_monetary_spec_list '\n'
	{
	    lc_monetary_flag++;
	}
	| lc_monetary_spec
	{
	    lc_monetary_flag++;
	}
        | '\n'
	;

lc_monetary_spec :
  	KW_INT_CURR_SYMBOL string '\n'
	{
	    sem_set_str(&monetary.int_curr_symbol);
	}
	| KW_CURRENCY_SYMBOL string '\n'
	{
	    sem_set_str(&monetary.currency_symbol);
        }
	| KW_MON_DECIMAL_POINT string '\n'
	{ 
	    sem_set_str(&monetary.mon_decimal_point); 
	}
	| KW_MON_THOUSANDS_SEP string '\n'  
	{
	    sem_set_str(&monetary.mon_thousands_sep);
	}
	| KW_POSITIVE_SIGN string '\n'
	{
	    sem_set_str(&monetary.positive_sign);
	}
	| KW_NEGATIVE_SIGN string '\n'
	{
	    sem_set_str(&monetary.negative_sign);
	}
	| KW_MON_GROUPING digit_list '\n'
	{
	    sem_set_diglist(&monetary.mon_grouping);
	}
	| KW_INT_FRAC_DIGITS number '\n'
	{
	    sem_set_int((signed char *)&monetary.int_frac_digits);
	}
	| KW_FRAC_DIGITS number '\n'
	{
	    sem_set_int((signed char *)&monetary.frac_digits);
	}
	| KW_P_CS_PRECEDES number '\n'
	{
	    sem_set_int((signed char *)&monetary.p_cs_precedes);
	}
	| KW_P_SEP_BY_SPACE number '\n'
	{
	    sem_set_int((signed char *)&monetary.p_sep_by_space);
	}
	| KW_N_CS_PRECEDES number '\n'
	{
	    sem_set_int((signed char *)&monetary.n_cs_precedes);
	}
	| KW_N_SEP_BY_SPACE number '\n'
	{
	    sem_set_int((signed char *)&monetary.n_sep_by_space);
	}
	| KW_P_SIGN_POSN number '\n'
	{
	    sem_set_int((signed char *)&monetary.p_sign_posn);
	}
	| KW_N_SIGN_POSN number '\n'
	{
	    sem_set_int((signed char *)&monetary.n_sign_posn);
	}
	;

/* ----------------------------------------------------------------------
** LC_MSG
**
** This section parses the LC_MSG category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_msg :
	KW_LC_MSG '\n' 
	{
	    if (lc_message_flag)
	        diag_error(ERR_DUP_CATEGORY,"LC_MESSAGE");

	}
	lc_msg_spec_list KW_END KW_LC_MSG '\n'
	{
	    if (!lc_message_flag)
		diag_error(ERR_EMPTY_CAT,"LC_MESSAGE");
	}
	| KW_LC_MSG '\n' KW_COPY locale_name '\n' KW_END KW_LC_MSG '\n'
	{
	    copy_locale(LC_MESSAGES);
	}
	| KW_LC_MSG '\n' KW_END KW_LC_MSG '\n'
	{
	    lc_message_flag++;

	    diag_error(ERR_EMPTY_CAT,"LC_MESSAGE");

	}
	;

lc_msg_spec_list :
	lc_msg_spec_list lc_msg_spec
	| lc_msg_spec_list '\n'
	{
	    lc_message_flag++;
	}
	| lc_msg_spec
	{
	    lc_message_flag++;
	}
        | '\n'
	;

lc_msg_spec :
	KW_YESEXPR string '\n'
	{
	    sem_set_str(&messages.yesexpr);
	}
	| KW_NOEXPR string '\n'
        {
	    sem_set_str(&messages.noexpr);
	}
        | KW_YESSTR string '\n'
        {
	    sem_set_str(&messages.yesstr);
	}
	| KW_NOSTR string '\n'
	{
	    sem_set_str(&messages.nostr);
	}
	;

/* ----------------------------------------------------------------------
** LC_NUMERIC
**
** This section parses the LC_NUMERIC category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_numeric :
	KW_LC_NUMERIC '\n' 
	{
	    if (lc_numeric_flag)
		diag_error(ERR_DUP_CATEGORY,"LC_NUMERIC");

	}
	lc_numeric_spec_list KW_END KW_LC_NUMERIC '\n'
	{
	    if (!lc_numeric_flag)
		diag_error(ERR_EMPTY_CAT,"LC_NUMERIC");
	}
	| KW_LC_NUMERIC '\n' KW_COPY locale_name '\n' KW_END KW_LC_NUMERIC '\n'
	{
	    copy_locale(LC_NUMERIC);
	}
	| KW_LC_NUMERIC '\n' KW_END KW_LC_NUMERIC '\n'
	{
	    lc_numeric_flag++;
	    diag_error(ERR_EMPTY_CAT,"LC_NUMERIC");
	}
	;

lc_numeric_spec_list :
	lc_numeric_spec_list lc_numeric_spec
	| lc_numeric_spec
	{
	    lc_numeric_flag++;
	}
        | lc_numeric_spec_list '\n'
	{
	    lc_numeric_flag++;
	}
        | '\n'
	;


lc_numeric_spec :
	KW_DECIMAL_POINT string '\n'
	{
	    sem_set_str(&numeric.decimal_point);
	    if (numeric.decimal_point == NULL) {
	      numeric.decimal_point = "";
	      diag_error(ERR_ILL_DEC_CONST, "");
	    }
	}
	| KW_THOUSANDS_SEP string '\n'
        {
	    sem_set_str(&numeric.thousands_sep);
	}
	| KW_GROUPING digit_list '\n'
        {
	    sem_set_diglist(&numeric.grouping);
	}
	;

/* ----------------------------------------------------------------------
** LC_TIME
**
** This section parses the LC_TIME category section of the LOCSRC
** file.
** ----------------------------------------------------------------------*/

lc_time :
	KW_LC_TIME '\n' 
	{
	    if (lc_time_flag)
	 	diag_error(ERR_DUP_CATEGORY,"LC_TIME");

	}
	lc_time_spec_list KW_END KW_LC_TIME '\n'
	{
	    if (!lc_time_flag)
		diag_error(ERR_EMPTY_CAT,"LC_TIME");
	}
	| KW_LC_TIME '\n' KW_COPY locale_name '\n' KW_END KW_LC_TIME '\n'
	{
	    copy_locale(LC_TIME);
	}
	| KW_LC_TIME '\n' KW_END KW_LC_TIME '\n'
	{
	    lc_time_flag++;

	    diag_error(ERR_EMPTY_CAT,"LC_TIME");

	}
	;

lc_time_spec_list :
	lc_time_spec_list lc_time_spec
	{
	    lc_time_flag++;
	}
	| lc_time_spec
	{
	    lc_time_flag++;
	}
        | lc_time_spec_list '\n'
	{
	    lc_time_flag++;
	}
        | '\n'
	;

lc_time_spec :
	KW_ABDAY string_list '\n'
        {
	    sem_set_str_lst(lc_time.abday,7);
	}
	| KW_DAY string_list '\n'
	{
	    sem_set_str_lst(lc_time.day,7);
	}
	| KW_ABMON string_list '\n'
	{
	    sem_set_str_lst(lc_time.abmon,12);
	}
	| KW_MON string_list '\n'
	{
	    sem_set_str_lst(lc_time.mon,12);
	}
	| KW_D_T_FMT string '\n'
	{
	    sem_set_str(&lc_time.d_t_fmt);
	}
	| KW_D_FMT string '\n'
	{
	    sem_set_str(&lc_time.d_fmt);
	}
	| KW_T_FMT string '\n'
	{
	    sem_set_str(&lc_time.t_fmt);
	}
	| KW_AM_PM string_list '\n'
	{
	    sem_set_str_lst(lc_time.am_pm,2);
	}
	| KW_T_FMT_AMPM string '\n'
	{
	    sem_set_str(&lc_time.t_fmt_ampm);
	}
	| KW_ERA {arblen=0;} arblist '\n'
	{
	    char **arbp = MALLOC(char*, arblen+1);

	    sem_set_str_lst(arbp,arblen);
	    arbp[arblen] = NULL;
	    lc_time.era = arbp;
	}
	| KW_ERA_D_FMT string '\n' 
	{
	    sem_set_str(&lc_time.era_d_fmt);
	}
	| KW_ERA_D_T_FMT string '\n'
	{
	    sem_set_str(&lc_time.era_d_t_fmt);
	}
	| KW_ERA_T_FMT string '\n'
	{
	    sem_set_str(&lc_time.era_t_fmt);
	}
	| KW_ALT_DIGITS {arblen=0;} arblist '\n'
      	{
	    sem_set_str_cat(&lc_time.alt_digits, arblen);
	}
	| KW_DATE_FMT string '\n'
	{
	    sem_set_str(&lc_time.date_fmt);
	}
	;

arblist :
	arblist ';' string
	{
	    arblen++;
	}
	| string
	{
	    arblen++;
	}
	;


unrecognized_cat :
	SYMBOL '\n'
	{
	    user_defined++;
	    while (yylex() != KW_END)	/* LINTED */
			;	    
	}
	SYMBOL '\n'
	{
	    diag_error(ERR_UNDEF_CAT,yytext);
	}
	;

/* ----------------------------------------------------------------------
** METHODS
**
** This section defines the grammar which parses the methods file.
** ----------------------------------------------------------------------*/

/* craigm - more here - redo this structure so method_def can
   be any combination */
method_def : 
	lc_bind
	| methods
	| lc_bind methods
	| methods lc_bind
	;

lc_bind :
	KW_LCBIND '\n'
	{
	}
	symbolic_assign_list lcbind_list KW_END KW_LCBIND '\n'
	{
	}
	;

symbolic_assign_list :
	symbolic_assign_list symbolic_assign
	| symbolic_assign
	| '\n'
	;

/* craigm */
symbolic_assign :
	symbol_name '=' hexadecimal_number '\n'
	{
		sem_set_lcbind_symbolic_value();
	}
	;

symbol_name :
	text
	;

hexadecimal_number :
	hex_string
	{
	    item_t	*hexadecimal_number;
	    unsigned int hex_num;
	    hexadecimal_number = sem_pop();
	    if (sscanf(hexadecimal_number->value.str, "%x", &hex_num) != 1) {
		fprintf(stderr, "ERROR: LCBIND improper hexnumber %s\n",
		    hexadecimal_number->value.str);
		exit(4);
	    }
	    destroy_item(hexadecimal_number);
	    hexadecimal_number = create_item(SK_INT, hex_num);
	    (void) sem_push(hexadecimal_number);
	}
	;

lcbind_list :
	lcbind_list lcbind_value
	| lcbind_value
	| '\n'
	;

lcbind_value :
	CHAR_CLASS_SYMBOL { lcbind_charclass = strdup(sym_text); }
		char_category SYMBOL '\n'
	{
	    /*
	     * user attempting to redefine a predefined charclass
	     * which is NOT allowed.
	     */
	    diag_error(ERR_REDEFINE_CHARCLASS, lcbind_charclass);
	    (void) free(lcbind_charclass);
	}
	| text char_category SYMBOL '\n'
	{
	    item_t *name;
	    char *value;
	    int i;
	    unsigned int mask;
	    int found = 0;
	    extern _LC_ctype_t ctype;
	    extern int length_lcbind_symbol_table;
	    extern struct lcbind_symbol_table lcbind_symbol_table[];

	    /* value = sem_pop(); */
	    value = strdup(sym_text);		/* get SYMBOL */
	    name = sem_pop();
	    for (i = 0; i < length_lcbind_symbol_table; i++) {
		if (!strcmp(value, lcbind_symbol_table[i].symbol_name)) {
		    mask = lcbind_symbol_table[i].value;
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		fprintf(stderr, "ERROR:  LCBIND didn't find value for %s\n",
			value);
		exit(4);
	    }
	    add_char_ct_name(&ctype, Lcbind_Table, name->value.str,
			     lcbind_tag, value, mask, 0);
	    free(value);
	    destroy_item(name);
	}
/* craigm - getting a reduce/reduce conflict from yacc */
	| text char_category hexadecimal_number '\n'
	{
	    extern _LC_ctype_t ctype;
	    item_t *name, *value;

	    value = sem_pop();
	    name = sem_pop();
	    add_char_ct_name(&ctype, Lcbind_Table, name->value.str,
				lcbind_tag, NULL, value->value.int_no, 0);
	    destroy_item(value);
	    destroy_item(name);
	}
	;

char_category :
	KW_CHARCLASS
	{
	    lcbind_tag = _LC_TAG_CCLASS;
	}
	| KW_CHARTRANS
	{
	    lcbind_tag = _LC_TAG_TRANS;
	}
	;

methods :
  	KW_METHODS '\n' 
	{
	    method_class = USR_CODESET;
	}
	method_assign_list KW_END KW_METHODS '\n'
	{
		if (Euc_filecode == TRUE) {
			if ((Utf8_filecode == TRUE) || (Other_filecode == TRUE)) {
				/* multiple file_code is specified */
				error(ERR_MULTI_FILE_CODE);
			}
			if (Euc_proccode == TRUE) {
				if ((Dense_proccode == TRUE) || (Ucs4_proccode == TRUE)) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				if (Cswidth == FALSE) {
					/* cswidth is not specified */
					error(ERR_NO_CSWIDTH);
				}
				/* file/process is euc/euc */
				charmap.cm_fc_type = _FC_EUC;
				charmap.cm_pc_type = _PC_EUC;
			} else if (Dense_proccode == TRUE) {
				if (Ucs4_proccode == TRUE) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				if (Cswidth == FALSE) {
					/* cswidth is not specified */
					error(ERR_NO_CSWIDTH);
				}
				/* file/process is euc/dense */
				charmap.cm_fc_type = _FC_EUC;
				charmap.cm_pc_type = _PC_DENSE;
			} else if (Ucs4_proccode == TRUE) {
				if (Cswidth == FALSE) {
					/* cswidth is not specified */
					error(ERR_NO_CSWIDTH);
				}
				/* file/process is euc/ucs4 */
				charmap.cm_fc_type = _FC_EUC;
				charmap.cm_pc_type = _PC_UCS4;
			} else {
				/* No process_code found */
				/* dense is the default process code */
				if (Cswidth == FALSE) {
					/* cswidth is not specified */
					error(ERR_NO_CSWIDTH);
				}
				/* file/process is euc/dense */
				charmap.cm_fc_type = _FC_EUC;
				charmap.cm_pc_type = _PC_DENSE;
			}
		} else if (Utf8_filecode == TRUE) {
			if (Other_filecode == TRUE) {
				/* multiple file_code is specified */
				error(ERR_MULTI_FILE_CODE);
			}
			if (Cswidth == TRUE) {
				/* cswidth is specified */
				error(ERR_INV_CSWIDTH);
			}
			if (Ucs4_proccode == TRUE) {
				if ((Euc_proccode == TRUE) || (Dense_proccode == TRUE)) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				/* file/process is utf8/ucs4 */
				charmap.cm_fc_type = _FC_UTF8;
				charmap.cm_pc_type = _PC_UCS4;
			} else {
				/* utf8 takes only ucs4 as process code */
				error(ERR_PROC_FILE_MISMATCH);
			}
		} else if (Other_filecode == TRUE) {
			if (Cswidth == TRUE) {
				/* cswidth is specified */
				error(ERR_INV_CSWIDTH);
			}
			if (Euc_proccode == TRUE) {
				if ((Dense_proccode == TRUE) || (Ucs4_proccode == TRUE)) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				/* file/process is other/euc */
				charmap.cm_fc_type = _FC_OTHER;
				charmap.cm_pc_type = _PC_EUC;
			} else if (Dense_proccode == TRUE) {
				if (Ucs4_proccode == TRUE) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				/* file/process is other/dense */
				charmap.cm_fc_type = _FC_OTHER;
				charmap.cm_pc_type = _PC_DENSE;
			} else if (Ucs4_proccode == TRUE) {
				/* file/process is other/ucs4 */
				charmap.cm_fc_type = _FC_OTHER;
				charmap.cm_pc_type = _PC_UCS4;
			} else {
				/* No process_code found */
				/* dense is the default process code */
				charmap.cm_fc_type = _FC_OTHER;
				charmap.cm_pc_type = _PC_DENSE;
			}
		} else {
			/* No file_code found */
			if (Euc_proccode == TRUE) {
				if ((Dense_proccode == TRUE) || (Ucs4_proccode == TRUE)) {
					/* multiple process_code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				if (Cswidth == TRUE) {
					/* assumes file code is euc */
					Euc_filecode = TRUE;
					/* file/process is euc/euc */
					charmap.cm_fc_type = _FC_EUC;
					charmap.cm_pc_type = _PC_EUC;
				} else {
					/* assumes file code is other */
					Other_filecode = TRUE;
					/* file/process is other/euc */
					charmap.cm_fc_type = _FC_OTHER;
					charmap.cm_pc_type = _PC_EUC;
				}
			} else if (Dense_proccode == TRUE) {
				if (Ucs4_proccode == TRUE) {
					/* multiple process code is specified */
					error(ERR_MULTI_PROC_CODE);
				}
				if (Cswidth == TRUE) {
					/* assumes file code is euc */
					Euc_filecode = TRUE;
					/* file/process is euc/dense */
					charmap.cm_fc_type = _FC_EUC;
					charmap.cm_pc_type = _PC_DENSE;
				} else {
					/* assumes file code is other */
					Other_filecode = TRUE;
					/* file/process is other/dense */
					charmap.cm_fc_type = _FC_OTHER;
					charmap.cm_pc_type = _PC_DENSE;
				}
			} else if (Ucs4_proccode == TRUE) {
				if (Cswidth == TRUE) {
					/* assumes file code is euc */
					Euc_filecode = TRUE;
					/* file/process is euc/ucs4 */
					charmap.cm_fc_type = _FC_EUC;
					charmap.cm_pc_type = _PC_UCS4;
				} else {
					/* assumes file code is other */
					Other_filecode = TRUE;
					/* file/process is other/ucs4 */
					charmap.cm_fc_type = _FC_OTHER;
					charmap.cm_pc_type = _PC_UCS4;
				}
			} else {
				/* No process_code found */
				/* dense is the default process code */
				if (Cswidth == TRUE) {
					/* assumes file code is euc */
					Euc_filecode = TRUE;
					/* file/process is euc/dense */
					charmap.cm_fc_type = _FC_EUC;
					charmap.cm_pc_type = _PC_DENSE;
				} else {
					/* assumes file code is other */
					Other_filecode = TRUE;
					/* file/process is other/dense */
					charmap.cm_fc_type = _FC_OTHER;
					charmap.cm_pc_type = _PC_DENSE;
				}
			}
		}
	}
	;

method_assign_list : 
	method_assign_list method_assign
	| method_assign_list '\n'
	| method_assign
        | '\n'
	;

method_string :
	string
	{
		args = 1;
	}
	| string string
	{
		args = 2;
	}
	| string string string
	{
		args = 3;
		if (lp64p == TRUE)
			error(ERR_LP64_WITH_OLD_EXT);
		if (new_extfmt == TRUE)
			error(ERR_MIX_NEW_OLD_EXT);
		old_extfmt = TRUE;
	}
	| string string string string
	{
		args = 4;
		if (old_extfmt == TRUE)
			error(ERR_MIX_NEW_OLD_EXT);
		new_extfmt = TRUE;
	}
	;

method_assign : 
	KW_PROCESS_CODE KW_EUC '\n'
	{
	    Native = FALSE;
		Euc_proccode = TRUE;
		/*
	    charmap.cm_pc_type = _PC_EUC;
		*/
	}
	|
	KW_PROCESS_CODE KW_DENSE '\n'
	{
	    Native = TRUE;
		Dense_proccode = TRUE;
		/*
	    charmap.cm_pc_type = _PC_DENSE;
		*/
	}
	|
	KW_PROCESS_CODE KW_UCS4 '\n'
	{
	    Native = TRUE;
		Ucs4_proccode = TRUE;
		/*
	    charmap.cm_pc_type = _PC_UCS4;
		*/
	}
	|
	KW_FILE_CODE KW_EUC	'\n'
	{
		Euc_filecode = TRUE;
		/*
		charmap.cm_fc_type = _FC_EUC;
		*/
	}
	|
	KW_FILE_CODE KW_UTF8 '\n'
	{
		Utf8_filecode = TRUE;
		/*
		charmap.cm_fc_type = _FC_UTF8;
		*/
	}
	|
	KW_FILE_CODE KW_OTHER '\n'
	{
		Other_filecode = TRUE;
	}
	|
	KW_CSWIDTH num_pairs '\n'
	{
		Cswidth = TRUE;
		/*
	    Euc_filecode = TRUE;
		*/
		/*
	    charmap.cm_fc_type = _FC_EUC;
		*/
	}
	|
	KW_MBLEN method_string '\n'
	{
	    set_method(CHARMAP_MBLEN, args);
	}
	| KW_MBTOWC method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_MBTOWC, args);
	    else
		set_method(CHARMAP_MBTOWC_AT_NATIVE, args);
	}
	| KW_MBSTOWCS method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_MBSTOWCS, args);
	    else
		set_method(CHARMAP_MBSTOWCS_AT_NATIVE, args);
	}
	| KW_WCTOMB method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_WCTOMB, args);
	    else
		set_method(CHARMAP_WCTOMB_AT_NATIVE, args);
	}
	| KW_WCSTOMBS method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_WCSTOMBS, args);
	    else
		set_method(CHARMAP_WCSTOMBS_AT_NATIVE, args);
	}
	| KW_WCWIDTH method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_WCWIDTH, args);
	    else
		set_method(CHARMAP_WCWIDTH_AT_NATIVE, args);
	}
	| KW_WCSWIDTH method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_WCSWIDTH, args);
	    else
		set_method(CHARMAP_WCSWIDTH_AT_NATIVE, args);
	}
	| KW_MBFTOWC method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_MBFTOWC, args);
	    else
		set_method(CHARMAP_MBFTOWC_AT_NATIVE, args);
	}
	| KW_FGETWC method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CHARMAP_FGETWC, args);
	    else
		set_method(CHARMAP_FGETWC_AT_NATIVE, args);
	}
	| KW_TOWUPPER method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CTYPE_TOWUPPER, args);
	    else
		set_method(CTYPE_TOWUPPER_AT_NATIVE, args);
	}
	| KW_TOWLOWER method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CTYPE_TOWLOWER, args);
	    else
		set_method(CTYPE_TOWLOWER_AT_NATIVE, args);
	}
	| KW_WCTYPE method_string '\n'
	{
	    set_method(CTYPE_WCTYPE, args);
	}
	| KW_ISWCTYPE method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CTYPE_ISWCTYPE, args);
	    else
		set_method(CTYPE_ISWCTYPE_AT_NATIVE, args);
	}
	| KW_STRCOLL method_string '\n'
	{
	    set_method(COLLATE_STRCOLL, args);
	}
	| KW_STRXFRM method_string '\n'
	{
	    set_method(COLLATE_STRXFRM, args);
	}
	| KW_WCSCOLL method_string '\n'
	{
	    if (Native == FALSE)
		set_method(COLLATE_WCSCOLL, args);
	    else
		set_method(COLLATE_WCSCOLL_AT_NATIVE, args);
	}
	| KW_WCSXFRM method_string '\n'
	{
	    if (Native == FALSE)
		set_method(COLLATE_WCSXFRM, args);
	    else
		set_method(COLLATE_WCSXFRM_AT_NATIVE, args);
	}
	| KW_REGCOMP method_string '\n'
	{
	    set_method(COLLATE_REGCOMP, args);
	}
	| KW_REGEXEC method_string '\n'
	{
	    set_method(COLLATE_REGEXEC, args);
	}
	| KW_REGFREE method_string '\n'
	{
	    set_method(COLLATE_REGFREE, args);
	}
	| KW_REGERROR method_string '\n'
	{
	    set_method(COLLATE_REGERROR, args);
	}
	| KW_STRFMON method_string '\n'
	{
	    set_method(MONETARY_STRFMON, args);
	}
	| KW_STRFTIME method_string '\n'
	{
	    set_method(TIME_STRFTIME, args);
	}
	| KW_STRPTIME method_string '\n'
	{
	    set_method(TIME_STRPTIME, args);
	}
	| KW_GETDATE method_string '\n'
	{
	    set_method(TIME_GETDATE, args);
	}
	| KW_WCSFTIME method_string '\n'
	{
	    set_method(TIME_WCSFTIME, args);
	}
	| KW_EUCPCTOWC method_string '\n'
	{
	    set_method(CHARMAP_EUCPCTOWC, args);
	}
	| KW_WCTOEUCPC method_string '\n'
	{
	    set_method(CHARMAP_WCTOEUCPC, args);
	}
	| KW_TRWCTYPE method_string '\n'
	{
	    set_method(CTYPE_TRWCTYPE, args);
	}
	| KW_TOWCTRANS method_string '\n'
	{
	    if (Native == FALSE)
		set_method(CTYPE_TOWCTRANS, args);
	    else
		set_method(CTYPE_TOWCTRANS_AT_NATIVE, args);
	}
	| KW_WCTRANS method_string '\n'
	{
	    set_method(CTYPE_WCTRANS, args);
	}
	| KW_FGETWC_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_FGETWC_AT_NATIVE, args);
	}
	| KW_ISWCTYPE_AT_NATIVE method_string '\n'
	{
	    set_method(CTYPE_ISWCTYPE_AT_NATIVE, args);
	}
	| KW_MBFTOWC_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_MBFTOWC_AT_NATIVE, args);
	}
	| KW_MBSTOWCS_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_MBSTOWCS_AT_NATIVE, args);
	}
	| KW_MBTOWC_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_MBTOWC_AT_NATIVE, args);
	}
	| KW_TOWLOWER_AT_NATIVE method_string '\n'
	{
	    set_method(CTYPE_TOWLOWER_AT_NATIVE, args);
	}
	| KW_TOWUPPER_AT_NATIVE method_string '\n'
	{
	    set_method(CTYPE_TOWUPPER_AT_NATIVE, args);
	}
	| KW_WCSCOLL_AT_NATIVE method_string '\n'
	{
	    set_method(COLLATE_WCSCOLL_AT_NATIVE, args);
	}
	| KW_WCSTOMBS_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_WCSTOMBS_AT_NATIVE, args);
	}
	| KW_WCSXFRM_AT_NATIVE method_string '\n'
	{
	    set_method(COLLATE_WCSXFRM_AT_NATIVE, args);
	}
	| KW_WCTOMB_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_WCTOMB_AT_NATIVE, args);
	}
	| KW_WCWIDTH_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_WCWIDTH_AT_NATIVE, args);
	}
	| KW_WCSWIDTH_AT_NATIVE method_string '\n'
	{
	    set_method(CHARMAP_WCSWIDTH_AT_NATIVE, args);
	}
	| KW_TOWCTRANS_AT_NATIVE method_string '\n'
	{
	    set_method(CTYPE_TOWCTRANS_AT_NATIVE, args);
	}
	| KW_FNMATCH method_string '\n'
	{
	    set_method(COLLATE_FNMATCH, args);
	}
	| KW_BTOWC method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_BTOWC, args);
		else
			set_method(CHARMAP_BTOWC_AT_NATIVE, args);
	}
	| KW_WCTOB method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_WCTOB, args);
		else
			set_method(CHARMAP_WCTOB_AT_NATIVE, args);
	}
	| KW_MBSINIT method_string '\n'
	{
		set_method(CHARMAP_MBSINIT, args);
	}
	| KW_MBRLEN method_string '\n'
	{
		set_method(CHARMAP_MBRLEN, args);
	}
	| KW_MBRTOWC method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_MBRTOWC, args);
		else
			set_method(CHARMAP_MBRTOWC_AT_NATIVE, args);
	}
	| KW_WCRTOMB method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_WCRTOMB, args);
		else
			set_method(CHARMAP_WCRTOMB_AT_NATIVE, args);
	}
	| KW_MBSRTOWCS method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_MBSRTOWCS, args);
		else
			set_method(CHARMAP_MBSRTOWCS_AT_NATIVE, args);
	}
	| KW_WCSRTOMBS method_string '\n'
	{
		if (Native == FALSE)
			set_method(CHARMAP_WCSRTOMBS, args);
		else
			set_method(CHARMAP_WCSRTOMBS_AT_NATIVE, args);
	}
	| KW_BTOWC_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_BTOWC_AT_NATIVE, args);
	}
	| KW_WCTOB_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_WCTOB_AT_NATIVE, args);
	}
	| KW_MBRTOWC_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_MBRTOWC_AT_NATIVE, args);
	}
	| KW_WCRTOMB_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_WCRTOMB_AT_NATIVE, args);
	}
	| KW_MBSRTOWCS_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_MBSRTOWCS_AT_NATIVE, args);
	}
	| KW_WCSRTOMBS_AT_NATIVE method_string '\n'
	{
		set_method(CHARMAP_WCSRTOMBS_AT_NATIVE, args);
	}

        ;

/* ----------------------------------------------------------------------
** CHARSETID
**
** This section defines the grammar which parses the character set id
** classification of characters.
**
**	THIS IS NOT SUPPORTED or USED in OSF/1!!!
** ----------------------------------------------------------------------*/

charsets_def : 
  	KW_CHARSETID '\n' charset_assign_list KW_END KW_CHARSETID '\n'
	{
	    diag_error(ERR_UNDEF_CAT,"CHARSETID");
	}
	;

charset_assign_list : 
	charset_assign_list charset_assign
	| charset_assign_list '\n'
	| charset_assign
        | '\n'
	;


charset_assign : 
	charset_range_assign 
	{
	    /* sem_charset_range_def(&charmap); */
	}
	| charset_simple_assign
	{
	    /* sem_charset_def(&charmap); */
	}
	;

charset_range_assign :
	char_symbol_ref KW_ELLIPSES char_symbol_ref const '\n'
	;

charset_simple_assign :
	char_symbol_ref const '\n'
	;

/* ----------------------------------------------------------------------
** GENERAL
**
** This section parses the syntatic elements shared by one or more of
** the above.
** ----------------------------------------------------------------------*/

digit_list : digit_list ';' number
    	{
	    /* add the new number to the digit list */
	    sem_digit_list();
	}
	| number
	{
	    item_t *i;

	    /* create count and push on top of stack */
	    i = create_item(SK_INT, 1);
	    (void) sem_push(i);
	}
        ;

char_range_list : char_range_list ';' ctype_symbol
        | char_range_list ';' KW_ELLIPSES ';' char_ref
        {
	    push_char_range();
	}  
        | ctype_symbol
	;

ctype_symbol : char_ref
        {
	    push_char_sym();
	}
        ;

char_ref : char_symbol_ref
	{
	    sem_char_ref();
	}
	| const
	| byte_list
	;

char_symbol_ref : SYMBOL
    	{
	    sem_existing_symbol(sym_text);
	}
	;

symbol  : SYMBOL
        {
	    sem_symbol(sym_text);
	}
	;

const   : int_const
	;

string_list : string_list ';' string
	| string
	;

text	: string
	| SYMBOL
	{
	    item_t *i = create_item(SK_STR, sym_text);
	    (void) sem_push(i);
	}
	;

string	: '"' {instring=TRUE;} STRING '"'
	{
	    item_t *i;
	    
	    i = create_item(SK_STR, yytext);
	    (void) sem_push(i);
	}
	| STRING
	{
	    item_t *i;
	    
	    i = create_item(SK_STR, yytext);
	    (void) sem_push(i);
	}
	;

hex_string : HEX_STRING
	{
	    item_t *i;

	    i = create_item(SK_STR, yytext);
	    (void) sem_push(i);
	}
	;

int_const : INT_CONST
        {
	    item_t *i;
	    char *junk;
	    
	    i = create_item(SK_INT, strtol(yytext, &junk, 10));
	    (void) sem_push(i);
	}
        ;

byte_list : CHAR_CONST
        {
	    extern int value;
	    item_t *it;
	    
	    it = create_item(SK_INT, value);
	    (void) sem_push(it);
        } 
        ;
number	: byte_list
	{
	    item_t	*it,*ct;
	    int		c;

	    ct = sem_pop();
	    if (ct->type != SK_INT)
		INTERNAL_ERROR;

	    c = ct->value.int_no;
	    if (c > '9' || c < '0') {
		char s[16];
		if (isprint(c)) {
			/* LINTED */
		    s[0] = (char) c;
		    s[1] = '\0';
		} else
		    sprintf(s,"\\x%2x",c);

	      diag_error(ERR_ILL_DEC_CONST, s);
	    }

	    destroy_item(ct);
	    it = create_item(SK_INT, c-'0');
	    (void) sem_push(it);
	}
	| string
	{
	    item_t *it,*st;
	    int i;
	    char *ep;

	    st = sem_pop();
	    if (st->type != SK_STR)
		INTERNAL_ERROR;

		/* LINTED */
	    i = (int) strtol(st->value.str, &ep, 10);
	    if (st->value.str == ep)
		diag_error(ERR_ILL_DEC_CONST, ep);

	    it = create_item(SK_INT, i);
	    destroy_item(st);
	    (void) sem_push(it);
	}
	;

num_pairs : num_pairs ',' num_pair
	| num_pair
	;

num_pair : number ':' number
	{
	    item_t *char_width, *screen_width;
	    static int codeset = 1;
	    extern _LC_euc_info_t euc_info;

	    screen_width = sem_pop();
	    if (screen_width->type != SK_INT)
		INTERNAL_ERROR;
	    char_width = sem_pop();
	    if (char_width->type != SK_INT)
		INTERNAL_ERROR;

	    if (codeset > MAX_CODESETS)
		error(ERR_TOO_MANY_CODESETS, MAX_CODESETS);

	    switch (codeset)
	    {
	    case 1:		/* codeset 1 */
		if ((char_width->value.int_no + 1) > MB_LEN_MAX)
			error(ERR_MB_LEN_MAX_TOO_BIG, MB_LEN_MAX - 1);
		/* LINTED */
		euc_info.euc_bytelen1 = (char) char_width->value.int_no;
		/* LINTED */
		euc_info.euc_scrlen1  = (char) screen_width->value.int_no;
		if ((unsigned char) euc_info.euc_scrlen1 > charmap.cm_max_disp_width)
			charmap.cm_max_disp_width = euc_info.euc_scrlen1;
		break;
	    case 2:		/* codeset 2 */
		if ((char_width->value.int_no + 1) > MB_LEN_MAX)
			error(ERR_MB_LEN_MAX_TOO_BIG, MB_LEN_MAX - 1);
		/* LINTED */
		euc_info.euc_bytelen2 = (char) char_width->value.int_no;
		/* LINTED */
		euc_info.euc_scrlen2  = (char) screen_width->value.int_no;
		if ((unsigned char) euc_info.euc_scrlen2 > charmap.cm_max_disp_width)
			charmap.cm_max_disp_width = euc_info.euc_scrlen2;
		break;
	    case 3:		/* codeset 3 */
		if ((char_width->value.int_no + 1) > MB_LEN_MAX)
			error(ERR_MB_LEN_MAX_TOO_BIG, MB_LEN_MAX - 1);
		/* LINTED */
		euc_info.euc_bytelen3 = (char) char_width->value.int_no;
		/* LINTED */
		euc_info.euc_scrlen3  = (char) screen_width->value.int_no;
		if ((unsigned char) euc_info.euc_scrlen3 > charmap.cm_max_disp_width)
			charmap.cm_max_disp_width = euc_info.euc_scrlen3;
		break;
	    }

	    destroy_item(char_width);
	    destroy_item(screen_width);

	    codeset++;
	}
	;

locale_name : LOC_NAME
	{
	    item_t *i;
	    
	    i = create_item(SK_STR, yytext);
	    (void) sem_push(i);
		
	}
	;
%%

void	initgram(void) {
}
