/*
 * tokeniseString: "Parse and tokenise a String".
 *
 * SYNOPSIS
 *    int countTokens   (const char *str, int nsp, int max)
 *    int tokeniseString(const char *str, char **p, int nsp, int max)
 *
 * DESCRIPTION
 *    tokeniseString:
 *    Given a null-terminated input ascii string, a pointer to an array of
 *    pointers to char, & either the number of spaces required between fields
 *    or a field separation character, sets the pointers to dynamically
 *    allocated memory containing each (null terminated) field. A maximum of
 *    'max' tokens will be created this way and the array 'p' will be
 *    terminated by a null pointer. Therefore, the calling code should
 *    dimension 'p' to max+1. The original string is NOT modified.
 *
 *    NOTE: On exit the last non-null pointer, p[max-1], contains
 *          the entire remainder of the input string. If the application wants
 *          NTOK fields, each terminated according to the field separation
 *          character or number of spaces, then it should dimension p to NTOK+2
 *          and call tokeniseString with max set to NTOK+1.
 *
 *    countTokens:
 *	Employs the same logic as tokeniseString but merely counts the
 *	number of tokens in the string.
 *
 * NOTES
 *    If nsp<' ' interprets it as the number of spaces between fields, &
 *    If nsp>' ' interprets it as a field separation character
 *    If nsp<' ' does not parse within single or double quotes: ' ', " "
 *    Terminates pointer array with a null pointer
 *
 * ARGUMENTS
 *    str:     pointer to ascii string
 *    p:   pointer to array of pointers to char
 *    nsp: # of spaces between adjacent fields or separation character
 *    max: maximum number of tokens to set into p[]. The calling code
 *         should dimension p at least one greater than this number.
 *
 * RETURNS
 *    # tokens in original string.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)tokenise.c 1.3 96/11/21 SMI"

#include <string.h>
#include "utils.h"

int
tokeniseString(const char *str, char **p, int nsp, int max)
{
	int begin = 0, tokenCount = 0;
	int i, l;

	if (!str) {
		p[0] = 0;
		return (0);
	}

	/* ignore any control characters from the end of str[]: */

	l = strlen(str);
	while (l >= 1 && str[l - 1] <= ' ')
		l--;
	if (l <= 0) {
		p[0] = 0;
		return (0);
	}

	if (nsp > ' ') { /* interpret nsp as a field-separation character: */
		for (i = 0; (max <= 0 || tokenCount < max) && i <= l; ) {
			while (i <= l && (str[i] != nsp ||
			    (tokenCount == (max - 1))))
				i++;
			p[tokenCount] = (char *)xmalloc(i - begin + 1);
			strncpy(p[tokenCount], str + begin, i - begin);
			p[tokenCount][i - begin] = '\0';
			begin = ++i;
			tokenCount++;
		}
		p[tokenCount] = 0;
	} else { /* interpret nsp as the min number of spaces between fields: */
		int n, fsi;
		for (i = 0; i < l && (max <= 0 || tokenCount < max); i++) {
			if (!(str[i] <= ' ')) {
				begin = i;
				if (tokenCount == (max - 1))
					fsi = l;
				else { /* advance to end of field: */
					fsi = n = 0;
					while (i < l) {
						/* advance till next "  */
						if (str[i] == '"') {
							while (++i < l &&
							    !(str[i] == '"'))
								/* NULL */;
						} else if (str[i] == '\'') {
							/* advance to next ' */
							while (++i < l &&
							    !(str[i] == '\''))
								/* NULL */;
						}
						i++;
						if (!(str[i] <= ' '))
							fsi = n = 0;
						else {
							n++;
							if (fsi == 0)
								fsi = i;
						}
						if (n >= nsp || str[i] == '\t')
							break;
					}
				}
				p[tokenCount] = (char *)xmalloc(
				    fsi - begin + 1);
				strncpy(p[tokenCount], str + begin,
				    fsi - begin);
				p[tokenCount][fsi - begin] = '\0';
				tokenCount++;
			}
		}
		p[tokenCount] = 0;
	}
	return (tokenCount);
}

int
countTokens(const char *str, int nsp, int max)
{
	int tokenCount = 0;
	int i, l;

	if (!str)
		return (0);

	/* get length of input string less control characters at the end */
	l = strlen(str);
	while (l >= 1 && str[l - 1] <= ' ')
		l--;

	if (nsp > ' ') { /* interpret nsp as a field-separation character: */
		for (i = 0; (max <= 0 || tokenCount < max) && i <= l; i++) {
			while (i <= l && (str[i] != nsp ||
			    tokenCount == (max - 1)))
				i++;
			tokenCount++;
		}
	} else { /* interpret nsp as the min number of spaces between fields: */
		int n;
		for (i = 0; i < l && (max <= 0 || tokenCount < max); i++) {
			if (!(str[i] <= ' ')) {
				if (!tokenCount == (max - 1)) {
				    /* advance to end of field: */
					n = 0;
					while (i < l) {
						if (str[i] == '"') {
						    /* advance till next " */
							while (++i < l &&
							    !(str[i] == '"'))
								/* NULL */;
						} else if (str[i] == '\'') {
							/* advance to next ' */
							while (++i < l &&
							    str[i] == '\'')
								/* NULL */;
						}
						i++;
						if (!(str[i] <= ' '))
							n = 0;
						else
							n++;
						if (n >= nsp || str[i] == '\t')
							break;
					}
				}
				tokenCount++;
			}
		}
	}
	return (tokenCount);
}
