
%{

#include <string.h>
#include <ctype.h>
#include <rw/cstring.h>

/* macro */
/*
#define DMI_DB_DEBUG_LEX printf("\tDMI_DB_LEX: %s at line %d\n", yytext, yylineno);
*/
#define DMI_DB_DEBUG_LEX 

/* base macro */
#define BASE_8	8
#define BASE_10 10
#define BASE_16 16

/* value macro */
#define INT_TOKEN yylval.ival
#define STR_TOKEN yylval.sval

/* functions */
static int dmi_db_atoi(char *, int);
static char* idl_escape_string(char*,int);
static char idl_escape_reader(char **);

%}

ws		[ \t]+
comment 	\/\/.*\n
quoted_string 	"\""[^\"\\]*"\""
escape_quoted_string 	"\""([^\"\\]|(\\.))*"\""
identifier	[a-zA-Z][_a-zA-Z0-9-]*
wildchar	\*
nl 	\n
openbrace	\{
closebrace	\}
openbracket	\(
closebracket	\)
equal		=
coma		\,
decimal_int	-?[0-9]+
hex_int		0[xX][a-fA-F0-9]+
oct_int		0[0-7]+


language	[lL][aA][nN][gG][uU][aA][gG][eE]
start		[sS][tT][aA][rR][tT]
end		[eE][nN][dD]
component	[cC][oO][mM][pP][oO][nN][eE][nN][tT]
name		[nN][aA][mM][eE]	
direct-interface [dD][iI][rR][eE][cC][tT]-[iI][nN][tT][eE][rR][fF][aA][cC][eE]
description	[dD][eE][sS][cC][rR][iI][pP][tT][iI][oO][nN]
path		[pP][aA][tT][hH]
dos		[dD][oO][sS]
macos		[mM][aA][cC][oO][sS]
os2		[oO][sS]2
unix		[uU][nN][iI][xX]
win16		[wW][iI][nN]16
win32		[wW][iI][nN]32
win9x		[wW][iI][nN]9x
winnt		[wW][iI][nN][nN][tT]
enum		[eE][nN][uU][mM]
type		[tT][yY][pP][eE]
group		[gG][rR][oO][uU][pP]
class		[cC][lL][aA][sS][sS]
id		[iI][dD]
key		[kK][eE][yY]
pragma		[pP][rR][aA][gG][mM][aA]
table		[tT][aA][bB][lL][eE]
attribute	[aA][tT][tT][rR][iI][bB][uU][tT][eE]
access		[aA][cC][cC][eE][sS][sS]
read-only	[rR][eE][aA][dD]-[oO][nN][lL][yY]
read-write	[rR][eE][aA][dD]-[wW][rR][iI][tT][eE]
write-only	[wW][rR][iI][tT][eE]-[oO][nN][lL][yY]
storage		[sS][tT][oO][rR][aA][gG][eE]
specific	[sS][pP][eE][cC][iI][fF][iI][cC]
common		[cC][oO][mM][mM][oO][nN]
unsupported	[uU][nN][sS][uU][pP][pP][oO][rR][tT][eE][dD]
unknown		[uU][nN][kK][nN][oO][wW][nN]
counter		[cC][oO][uU][nN][tT][eE][rR]
counter64	[cC][oO][uU][nN][tT][eE][rR]64
date		[dD][aA][tT][eE]
gauge		[gG][aA][uU][gG][eE]
octetstring	[oO][cC][tT][eE][tT][sS][tT][rR][iI][nN][gG]
displaystring	[dD][iI][sS][pP][lL][aA][yY][sS][tT][rR][iI][nN][gG]
string		[sS][tT][rR][iI][nN][gG]
int		[iI][nN][tT]
integer		[iI][nN][tT][eE][gG][eE][rR]
integer64	[iI][nN][tT][eE][gG][eE][rR]64
int64		[iI][nN][tT]64
value		[vV][aA][lL][uU][eE]

%%

{language}		return LANGUAGE;
{start}			return START;
{end}			return END;
{component}		return COMPONENT;
{name}			return NAME;
{direct-interface}	return DIRECTINTERFACE;
{description}		return DESCRIPTION;
{path}			return PATH;
{dos}			return DOS;
{macos}			return MACOS;
{os2}			return OS2;
{unix}			return UNIX;
{win16}			return WIN16;
{win32}			return WIN32;
{win9x}			return WIN9X;
{winnt}			return WINNT;
{enum}			return ENUM;
{type}			return TYPE;
{group}			return GROUP;
{class}			return CLASS;
{id}			return ID;
{key}			return KEY;
{pragma}		return PRAGMA;
{table}			return TABLE;
{attribute}		return ATTRIBUTE;
{access}		return ACCESS;
{read-only}		return READONLY;
{read-write}		return READWRITE;
{write-only}		return WRITEONLY;
{storage}		return STORAGE;
{specific}		return SPECIFIC;
{common}		return COMMON;
{unsupported}		return UNSUPPORTED;
{unknown}		return UNKNOWN;
{counter}		return COUNTER;
{counter64}		return COUNTER64;
{date}			return DATE;
{gauge}			return GAUGE;
{octetstring}		return OCTETSTRING;
{displaystring}		return DISPLAYSTRING;
{string}		return STRING;
{integer64}		return INTEGER64;
{int64}			return INTEGER64;
{integer}		return INTEGER;
{int}			return INT;
{value}			return VALUE;



{ws}				;
{comment}			;	
{nl}+				{ ; }
{openbrace}			{ DMI_DB_DEBUG_LEX return OPENBRACE; }
{closebrace}			{ DMI_DB_DEBUG_LEX return CLOSEBRACE; }
{openbracket}			{ DMI_DB_DEBUG_LEX return OPENBRACKET; }
{closebracket}			{ DMI_DB_DEBUG_LEX return CLOSEBRACKET; }
{equal}				{ DMI_DB_DEBUG_LEX return EQUAL; }
{coma}				{ DMI_DB_DEBUG_LEX return COMA; }
{decimal_int}			{ DMI_DB_DEBUG_LEX 
				  INT_TOKEN = dmi_db_atoi(yytext,BASE_10);
				  return DECNUMBER;
				}
{hex_int}			{ DMI_DB_DEBUG_LEX 
				  INT_TOKEN = dmi_db_atoi(yytext,BASE_16);
				  return HEXNUMBER;
				}

{oct_int}			{ DMI_DB_DEBUG_LEX
				  INT_TOKEN = dmi_db_atoi(yytext,BASE_8);
				  return OCTNUMBER;
				}
{quoted_string}		        {
                 		 yytext[strlen(yytext)-1] = '\0';
                  		 STR_TOKEN = new RWCString(yytext + 1);
                  		 return QUOTEDSTRING;
                		}
{escape_quoted_string}	        {
                  		 idl_escape_string(yytext,0);
                  		 STR_TOKEN = new RWCString(yytext);
                  		 return QUOTEDSTRING;
                		}
{identifier}			{ DMI_DB_DEBUG_LEX return IDENTIFIER; }
{wildchar}			{ DMI_DB_DEBUG_LEX return WILDCHAR; }
.				{ 
						/* error */
				}


%%

/*************************************************************************/


static int
dmi_db_atoi(char *s, int b)
{
	int neg = 0;
	int r = 0;

	if(*s == '-'){
	  neg = 1;
	  s++;
	}

        if (b == 8 && *s == '0')
          s++;
        else if (b == 16 && *s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X'))
          s += 2;

	int last_r = r;
        for (; *s; s++) {
          if (last_r > r) {
/*
            idl_global->err()->error0(UTL_Error::EIDL_COERCION_FAILURE);
*/
          }
          last_r = r;
          if (*s <= '9' && *s >= '0')
            r = (r * b) + (*s - '0');
          else if (b > 10 && *s <= 'f' && *s >= 'a')
            r = (r * b) + (*s - 'a' + 10);
          else if (b > 10 && *s <= 'F' && *s >= 'A')
            r = (r * b) + (*s - 'A' + 10);
          else
            break;
        }
        if (last_r > r) {
/*
           idl_global->err()->error0(UTL_Error::EIDL_COERCION_FAILURE);
*/
        }
	if (neg) r *= -1;
        return r;
}

/*
 * Convert (some) escaped characters into their ascii values
 */
static char
idl_escape_reader(
    char **p_cursor
)
{
    int ch = *(*p_cursor)++;
 
    switch (ch) {
    case '"':
        return '"';
    case '\\':
        return '\\';
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    case 'x':
        {
            // hex value
            int out = 0;
            for (int i = 0; i < 2 && isxdigit(ch = **p_cursor); i++) {
                out = out * 16 + ch -
                        (isdigit(ch) ? '0' : ch >= 'a' ? 'a' - 10 : 'A' - 10);
                *p_cursor += 1;
            }
            return out;
        }
        break;
      default:
        // check for octal value
        if (ch >= '0' && ch <= '7') {
            int out = ch - '0';
            for (int i = 0;
                 i < 2 && (ch = **p_cursor) >= '0' && ch <= '7';
                 i++)
                out = out * 8 + ch - '0', *p_cursor += 1;
            return out;
        } else {
          return ch - 'a';
        }
        break;
    }
}      


char *
idl_escape_string(
    char * string,
    int include_quotes
)
{
    char *src = string + 1;
    char *dst;
    if (include_quotes)
        dst = string + 1;
    else
        dst = string;
    int ch;
    while ((ch = *src++) != '"') {
        if (ch == '\\')
            ch = idl_escape_reader(&src);
        *dst++ = ch;
    }
    if (include_quotes) {
        *dst++ = '"';
    }
    *dst = 0;
    return src;
}


