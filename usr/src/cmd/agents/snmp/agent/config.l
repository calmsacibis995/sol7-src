/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)config.l	1.3 96/07/01 Sun Microsystems"

%{

/***** DEFINES *****/

/*
#define DEBUG_LEX printf("\tLEX: %s(%d) at line %d\n", yytext, yyval, yylineno);
*/
#define DEBUG_LEX


/***** STATIC VARIABLES *****/

static int token_value;


%}

%%

#.*\n					{ ; }
[ \t\n]*				{ ; }

\{					{ DEBUG_LEX return OPENBRACKET; }
\}					{ DEBUG_LEX return CLOSEBRACKET; }
=					{ DEBUG_LEX return EQUAL; }
\,					{ DEBUG_LEX return COMA; }

[cC][oO][mM][mM][uU][nN][iI][tT][iI][eE][sS] { DEBUG_LEX return COMMUNITIES; }
[rR][eE][aA][dD][-][oO][nN][lL][yY]	{ DEBUG_LEX return READONLY; }
[rR][eE][aA][dD][-][wW][rR][iI][tT][eE]	{ DEBUG_LEX return READWRITE; }

[mM][aA][nN][aA][gG][eE][rR][sS]	{ DEBUG_LEX return MANAGERS; }

[tT][rR][aA][pP][-][cC][oO][mM][mM][uU][nN][iI][tT][yY] { DEBUG_LEX return TRAPCOMMUNITY; }
[tT][rR][aA][pP][-][rR][eE][cC][iI][pP][iI][eE][nN][tT][sS] { DEBUG_LEX return TRAPDESTINATORS; }

[a-zA-Z][_a-zA-Z0-9-]*			{ DEBUG_LEX return IDENTIFIER; }

.					{ error_exit("syntax error in %s at line %d: the token %s is not valid",
						config_file, yylineno, yytext);
					}


%%

#undef input
#undef unput
#undef output

/*
# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
# define unput(c) {yytchar= (c);if(yytchar=='\n')yylineno--;*yysptr++=yytchar;}
# define output(c) (void)putc(c,yyout)
*/

#define input() (((yytchar=*lexinput++)=='\n'?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
#define unput(c) {yytchar= (c); if(yytchar=='\n')yylineno--;*--lexinput = yytchar;}
#define output(c)

/*************************************************************************/

