/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma	ident	"@(#)loadtab.c	1.6	97/08/08 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/localedef.h>
#include <thread.h>
#include <synch.h>
#include <string.h>
#include <stdarg.h>

#define	_WCHAR_CSMASK	0x30000000
#define	_WCHAR_S_MASK	0x7f
#define	_WCHAR_SHIFT	7
#define	_WCHAR_SHIFT2	1
#define	_WCHAR_SHIFT3	2
#define	_WCHAR_SHIFT4	3
#define	_WCHAR_BITMASK1	0x00000080
#define	_WCHAR_BITMASK2	0x00008080
#define	_WCHAR_BITMASK3	0x00808080
#define	_WCHAR_BITMASK4	0x80808080U
#define	_WCHAR_MASK1	(_WCHAR_S_MASK)
#define	_WCHAR_MASK2	(_WCHAR_S_MASK << _WCHAR_SHIFT)
#define	_WCHAR_MASK3	(_WCHAR_S_MASK << (_WCHAR_SHIFT * 2))
#define	_WCHAR_MASK4	(_WCHAR_S_MASK << (_WCHAR_SHIFT * 3))
#define	_LEN1_BASE	0x00000020
#define	_LEN1_END	0x0000007f
#define	_LEN2_BASE	0x00001020
#define	_LEN2_END	0x00003fff
#define	_LEN3_BASE	0x00081020
#define	_LEN3_END	0x001fffff
#define	_LEN4_BASE	0x04081020
#define	_LEN4_END	0x0fffffff
#define	_SS2	0x8e
#define	_SS3	0x8f
#define	_SB_CS1_SIZE	96
#define	_SB_CS1_START	0xa0
#define	_SB_CS1_END		0xff

/* For the application that has been linked with libw.a */
/* _lflag = 1 if multi-byte character table is updated */
int	_lflag = 0;

/* for BC (libw.a) - not static */
struct _wctype	*_wcptr[3] = {
	0, 0, 0
};

#ifdef _REENTRANT
mutex_t	_locale_lock = DEFAULTMUTEX;
#endif

#if defined(PIC)
static wchar_t *create_conv_table(int, wchar_t, wchar_t, int,
	wchar_t *, wchar_t *);
static int	create_ctype_table(const unsigned int *,
	unsigned char **, unsigned int **);
static int	build_wcptr(int, int, wchar_t, wchar_t, char *);
static wchar_t	findbase(int, int);
static void	free_allpointers(int, ...);
static void	free_wcptr(int, char *);
#endif /* PIC */

/*
 * Function: _loadtab
 *
 * INPUT -
 *
 * OUTPUT -
 * set _wcptr[] for the backward compatibility with euc locales.
 *
 * RETURN -
 * On successful completion or the target locale is not EUC,
 * return 0.
 * Otherwise, -1 is returned.
 *
 * NOTE: for BC (libw.a) - don't put prototype in a header file
 */
int
_loadtab(void)
{
#if defined(PIC)
	wchar_t	cs1_base, cs2_base, cs3_base;
	wchar_t	cs1_end, cs2_end, cs3_end;
	int	bytelen1, bytelen2, bytelen3;
	int	i, rc;
	/* this function should have been protected with */
	/* mutex (_locale_lock) */
	static char	mallocp = 0;

	_lflag = 1;

	/* clear _wcptr */
	for (i = 0; i < 2; i++) {
		free_wcptr(i, &mallocp);
	}

	if (!__lc_charmap->cm_eucinfo) {
		/* no euc locale */
		return (0);
	}

	bytelen1 = (int)__lc_charmap->cm_eucinfo->euc_bytelen1;
	bytelen2 = (int)__lc_charmap->cm_eucinfo->euc_bytelen2;
	bytelen3 = (int)__lc_charmap->cm_eucinfo->euc_bytelen3;
	cs1_end = __lc_charmap->cm_eucinfo->dense_end;
	cs2_end = cs1_end;	/* so far */
	cs3_end = cs1_end;	/* so far */


	if (bytelen1 != 0) {	/* Codeset1 exists. */
		/* find out the real cs1 base value */
		cs1_base = findbase(0, bytelen1);
		if (cs1_base == WEOF) {
			/* invalid codeset */
			return (-1);
		}
		cs3_end = cs1_base - 1;
		cs2_end = cs1_base - 1;
		rc = build_wcptr(0, bytelen1, cs1_base, cs1_end,
			&mallocp);
		if (rc == -1) {
			return (-1);
		}
	} else {
		return (0);
	}
	if (bytelen3 != 0) {	/* Codeset3 exists. */
		/* find out the real cs3 base value */
		cs3_base = findbase(2, bytelen3);
		if (cs3_base == WEOF) {
			/* invalid codeset */
			free_wcptr(0, &mallocp);
			return (-1);
		}
		cs2_end = cs3_base - 1;
		rc = build_wcptr(2, bytelen3, cs3_base, cs3_end, NULL);
		if (rc == -1) {
			free_wcptr(0, &mallocp);
			return (-1);
		}
	}
	if (bytelen2 != 0) {	/* Codeset2 exists. */
		/* find out the real cs2 base value */
		cs2_base = findbase(1, bytelen2);
		if (cs2_base == WEOF) {
			/* invalid codeset */
			free_wcptr(0, &mallocp);
			free_wcptr(2, NULL);
			return (-1);
		}
		rc = build_wcptr(1, bytelen2, cs2_base, cs2_end, NULL);
		if (rc == -1) {
			free_wcptr(0, &mallocp);
			free_wcptr(2, NULL);
			return (-1);
		}
	}

#ifdef	DDEBUG
	{
		int	cs, i, j, s;

		for (i = 0; i < 3; i++) {
			if (_wcptr[i]) {
				printf("{%x, %x, %x, %x, %x, %x, %x}\n",
					_wcptr[i]->tmin, _wcptr[i]->tmax,
					_wcptr[i]->index, _wcptr[i]->type,
					_wcptr[i]->cmin, _wcptr[i]->cmax,
					_wcptr[i]->code);
			} else {
				printf("{0, 0, 0, 0, 0, 0, 0}\n");
			}
		}


		for (cs = 0; cs < 3; cs++) {
			if (!_wcptr[cs]) {
				continue;
			}
			i = 0;
			j = 0;
			s = _wcptr[cs]->tmax - _wcptr[cs]->tmin;
			printf("unsigned char index%d[] = {\n", cs + 1);
			while (i <= s) {
				printf("0x%02x,  ", _wcptr[cs]->index[i]);
				j++;
				if (j > 7) {
					j = 0;
					printf("\n");
				}
				i++;
			}
			printf("};\n");
			printf("unsigned type%d[] = {\n", cs + 1);
			i = 0;
			j = 0;
			while (_wcptr[cs]->type[i]) {
				printf("0x%08x,  ", _wcptr[cs]->type[i]);
				j++;
				if (j > 4) {
					j = 0;
					printf("\n");
				}
				i++;
			}
			printf("};\n");
			printf("unsigned int code%d[] = {\n", cs + 1);
			i = 0;
			j = 0;
			s = _wcptr[cs]->cmax - _wcptr[cs]->cmin;
			while (i <= s) {
				printf("0x%x,  ", _wcptr[cs]->code[i]);
				j++;
				if (j > 7) {
					printf("\n");
					j = 0;
				}
				i++;
			}
			printf("};\n");
		}
	}
#endif /* DDEBUG */
	return (0);
#else /* !PIC */
	return (0);
#endif /* PIC */
}

#if defined(PIC)
/*
 * Function: create_conv_table
 *
 * INPUT -
 * bytelen : byte length of the target codeset
 * base_dense : code value of the base of the codeset in dense
 * end_dense  : code value of the end of the codeset in dense
 * ntrans : number of the transformation tables
 *
 * OUTPUT -
 * min_conv : minimum code value in dense for conversion
 * max_conv : maximum code value in dense for conversion
 *
 * RETURN -
 * the pointer to the created conversion table
 */
static wchar_t *
create_conv_table(
	int	bytelen,
	wchar_t	base_dense,
	wchar_t	end_dense,
	int	ntrans,
	wchar_t	*min_conv,
	wchar_t	*max_conv)
{
	int	i, len, count, cflag;
	wchar_t	*conv_table;
	wchar_t	densepc, eucpc, packs;
	wchar_t	min_conv_euc, max_conv_euc;
	wchar_t	wc, conv;
	wchar_t	start, stop;
	_LC_transtabs_t	*transtabs, *t;
	_LC_transnm_t	*transname;
	static const	unsigned int	bitmask[] = {
		_WCHAR_BITMASK1,	_WCHAR_BITMASK2,
		_WCHAR_BITMASK3,	_WCHAR_BITMASK4,
		0
	};
	int	*idx;
	wchar_t	*tmin;
	wchar_t	*tmax;
	wchar_t	*cmin;
	wchar_t	*cmax;

	transname = __lc_ctype->transname;
	transtabs = (_LC_transtabs_t *)__lc_ctype->transtabs;

	if ((tmin = malloc(sizeof (wchar_t) * ntrans)) == NULL) {
		return (NULL);
	}
	if ((tmax = malloc(sizeof (wchar_t) * ntrans))	== NULL) {
		free_allpointers(1, (void *)tmin);
		return (NULL);
	}
	if ((cmin = malloc(sizeof (wchar_t) * ntrans))	== NULL) {
		free_allpointers(2, (void *)tmin, (void *)tmax);
		return (NULL);
	}
	if ((cmax = malloc(sizeof (wchar_t) * ntrans))	== NULL) {
		free_allpointers(3, (void *)tmin, (void *)tmax,
			(void *)cmin);
		return (NULL);
	}

	if ((idx = malloc(sizeof (int) * ntrans))	== NULL) {
		free_allpointers(4, (void *)tmin, (void *)tmax,
			(void *)cmin, (void *)cmax);
		return (NULL);
	}
	for (i = 0; i < ntrans; i++) {
		/* transname[] starts from 1, not 0 */
		idx[i] = transname[i + 1].index;
		tmin[i] = transname[i + 1].tmin;
		tmax[i] = transname[i + 1].tmax;
	}

	*min_conv = end_dense;
	*max_conv = base_dense;
	cflag = 0;
	for (i = 0; i < ntrans; i++) {
		if ((tmax[i] < base_dense) ||
			(tmin[i] > end_dense)) {	/* out of range */
			idx[i] = 0;
			continue;
		}
		cflag = 1;
		if (tmin[i] < base_dense) {
			cmin[i] = base_dense;
		} else {
			cmin[i] = tmin[i];
		}
		if (tmax[i] > end_dense) {
			cmax[i] = end_dense;
		} else {
			cmax[i] = tmax[i];
		}
		if (cmin[i] < *min_conv) {
			*min_conv = cmin[i];
		}
		if (cmax[i] > *max_conv) {
			*max_conv = cmax[i];
		}
	}
	if (cflag == 0) {
		free_allpointers(5, (void *)tmin, (void *)tmax,
			(void *)cmin, (void *)cmax, (void *)idx);
		return (NULL);
	}

	min_conv_euc = _wctoeucpc(__lc_charmap, *min_conv);
	if (min_conv_euc == -1) {
		free_allpointers(5, (void *)tmin, (void *)tmax,
			(void *)cmin, (void *)cmax, (void *)idx);
		return (NULL);
	}
	max_conv_euc = _wctoeucpc(__lc_charmap, *max_conv);
	if (max_conv_euc == -1) {
		free_allpointers(5, (void *)tmin, (void *)tmax,
			(void *)cmin, (void *)cmax, (void *)idx);
		return (NULL);
	}
	len = max_conv_euc - min_conv_euc + 1;

	if ((conv_table = malloc(sizeof (wchar_t) * len)) == NULL) {
		free_allpointers(5, (void *)tmin, (void *)tmax,
			(void *)cmin, (void *)cmax, (void *)idx);
		return (NULL);
	}
	(void) memset(conv_table, 0, sizeof (wchar_t) * len);

	count = 0;
	for (eucpc = min_conv_euc; eucpc <= max_conv_euc; eucpc++) {
		wc = eucpc & ~_WCHAR_CSMASK;
		packs = ((_WCHAR_MASK4 & wc) << _WCHAR_SHIFT4) |
			((_WCHAR_MASK3 & wc) << _WCHAR_SHIFT3) |
			((_WCHAR_MASK2 & wc) << _WCHAR_SHIFT2) |
			(_WCHAR_MASK1 & wc);
		packs |= bitmask[bytelen - 1];
		*(conv_table + count) = packs;
		count++;
	}

	for (i = 0; i < ntrans; i++) {
		if (idx[i] == 0) {		/* no valid transtable */
			continue;
		}
		t = transtabs + idx[i];
		while (t) {
			if (t->tmax < cmin[i]) {
				t = t->next;
				continue;
			}
			if (t->tmin > cmax[i]) {
				break;
			}
			if (t->tmin <= cmin[i]) {
				start = cmin[i];
			} else {
				start = t->tmin;
			}
			if (t->tmax > cmax[i]) {
				stop = cmax[i];
			} else {
				stop = t->tmax;
			}
			for (densepc = start; densepc <= stop; densepc++) {
				conv = t->table[densepc - t->tmin];
				if (conv == densepc) {
					continue;
				}
				eucpc = _wctoeucpc(__lc_charmap, conv);
				if (eucpc == -1) {
					free_allpointers(6, (void *)tmin,
						(void *)tmax, (void *)cmin,
						(void *)cmax, (void *)idx,
						(void *)conv_table);
					return (NULL);
				}
				wc = eucpc & ~_WCHAR_CSMASK;
				packs = ((_WCHAR_MASK4 & wc) << _WCHAR_SHIFT4) |
					((_WCHAR_MASK3 & wc) << _WCHAR_SHIFT3) |
					((_WCHAR_MASK2 & wc) << _WCHAR_SHIFT2) |
					(_WCHAR_MASK1 & wc);
				packs |= bitmask[bytelen - 1];
				*(conv_table + (densepc - *min_conv)) = packs;
			}
			t = t->next;
		}
	}
	free_allpointers(5, (void *)tmin, (void *)tmax,
		(void *)cmin, (void *)cmax, (void *)idx);
	return (conv_table);
}


/*
 * Function: create_ctype_table
 *
 * INPUT -
 * mask : pointer to __lc_ctype->mask
 *
 * OUTPUT -
 * Following pointers will be set:
 * qidx : pointer to (struct _wctype)->index
 * qmsk : pointer to (struct _wctype)->type
 *
 * RETURN -
 * On successful completion, returns 0.
 * Otherwise, -1 will be returned.
 */
static int
create_ctype_table(
	const unsigned int	*mask,
	unsigned char	**qidx,
	unsigned int	**qmsk)
{

	unsigned int	type[_SB_CS1_SIZE];
	unsigned int	mask_value;
	int	i, x;
	int	index = -1;

	if ((*qidx = malloc(sizeof (unsigned char) * _SB_CS1_SIZE)) == NULL) {
		return (-1);
	}

	for (i = 0; i < _SB_CS1_SIZE; i++) {
		mask_value = *(mask + i + _SB_CS1_START);

		for (x = 0; x <= index; x++) {
			if (mask_value == type[x]) {
				*(*qidx + i) = (unsigned char)x;
				break;
			}
		}
		if (x > index) {
			index++;
			type[index] = mask_value;
			*(*qidx + i) = (unsigned char)index;
		}
	}
	if ((*qmsk = malloc(sizeof (unsigned int) * (index + 1 + 1))) == NULL) {
		free(qidx);
		return (-1);
	}
	(void) memcpy(*qmsk, type, sizeof (unsigned int) * (index + 1));
	*(*qmsk + index + 1 + 1) = 0;
	return (0);
}

/*
 * Function: build_wcptr
 *
 * INPUT -
 * codeset    : codeset
 * bytelen    : byte length of the target codeset
 * base_dense : code value of the base of the codeset in dense
 * end_dense  : code value of the end of the codeset in dense
 * mallocp    : flag to determine whether memory for ctype table
 *		has been allocated or not
 *
 * OUTPUT -
 * set _wcptr[].
 * mallocp    : if memory for ctype table has been dynamically allocated,
 *	*mallocp will be set to 1
 *
 * RETURN -
 * On successful completion, returns 0.
 * Otherwise, -1 will be returned.
 */
static int
build_wcptr(
	int	codeset,
	int	bytelen,
	wchar_t	base_dense,
	wchar_t	end_dense,
	char	*mallocp)
{
	wchar_t	*tbl = NULL;
	wchar_t	base_euc, end_euc;
	unsigned char	*qidx;
	unsigned int	*qmask;
	struct _wctype	*p;
	int	ret;
	int	ntrans;
	wchar_t	min_conv_dense = 0;
	wchar_t	max_conv_dense = 0;
	wchar_t	min_conv_euc = 0;
	wchar_t	max_conv_euc = 0;


	/* the number of transformation tables */
	ntrans = __lc_ctype->ntrans;

	/* allocate memory for a _wcptr structure */
	if ((p = malloc(sizeof (struct _wctype))) == NULL) {
		return (-1);
	}

	if (ntrans != 0) {
		/* if this code has transformation table, */
		/* create conversion table. */
		tbl = create_conv_table(bytelen,
			base_dense, end_dense, ntrans,
			&min_conv_dense, &max_conv_dense);
		if (tbl == NULL) {
			free(p);
			return (-1);
		}
		min_conv_euc = _wctoeucpc(__lc_charmap, min_conv_dense);
		max_conv_euc = _wctoeucpc(__lc_charmap, max_conv_dense);
		if ((min_conv_euc == -1) || (max_conv_euc == -1)) {
			free(tbl);
			free(p);
			return (-1);
		}
		min_conv_euc &= ~_WCHAR_CSMASK;
		max_conv_euc &= ~_WCHAR_CSMASK;
	}

	base_euc = _wctoeucpc(__lc_charmap, base_dense);
	end_euc  = _wctoeucpc(__lc_charmap,  end_dense);
	if ((base_euc == -1) || (end_euc == -1)) {
		if (tbl) {
			free(tbl);
		}
		free(p);
		return (-1);
	}
	base_euc &= ~_WCHAR_CSMASK;
	end_euc  &= ~_WCHAR_CSMASK;

	if ((codeset == 0) &&
		(__lc_ctype->qidx == NULL) &&
		(__lc_ctype->qmask == NULL)) {
		/* European locale */
		ret = create_ctype_table(__lc_ctype->mask, &qidx, &qmask);
		if (ret == -1) {
			if (tbl) {
				free(tbl);
			}
			free(p);
			return (-1);
		}
		base_euc = _SB_CS1_START & _WCHAR_S_MASK; /* 0x20 */
		end_euc = _SB_CS1_END & _WCHAR_S_MASK; /* 0x7f */
		if (mallocp) {
			*mallocp = 1;
		} else {
			if (tbl) {
				free(tbl);
			}
			free(p);
			return (-1);
		}
	} else if ((codeset == 0) &&
		((__lc_ctype->qidx == NULL) ||
		(__lc_ctype->qmask == NULL))) {
		/* invalid locale */
		if (tbl) {
			free(tbl);
		}
		free(p);
		return (-1);
	} else {
		qidx = (unsigned char *)__lc_ctype->qidx + (base_dense - 256);
		qmask = (unsigned int *)__lc_ctype->qmask;
	}

	p->tmin = base_euc;
	p->tmax = end_euc;
	p->index = qidx;
	p->type = qmask;
	p->cmin = min_conv_euc;
	p->cmax = max_conv_euc;
	p->code = tbl;
	_wcptr[codeset] = p;

	return (0);
}

/*
 * Function: findbase
 *
 * INPUT -
 * codeset    : codeset
 * bytelen    : byte length of the target codeset
 *
 * OUTPUT -
 * find base value for the codeset
 *
 * RETURN -
 * On successful completion, the base value will be returned.
 * Otherwise, (wchar_t)WEOF will be returned.
 */
static wchar_t
findbase(int codeset, int bytelen)
{
	wchar_t	base_euc, end_euc;
	wchar_t	base_dense;
	wchar_t	wc;
	wchar_t	mask[3] = {
		0x30000000,
		0x10000000,
		0x20000000
	};
	unsigned char	str[MB_LEN_MAX];
	int	rc;

	if ((codeset < 0) || (codeset > 2)) {
		return (WEOF);
	}
	if (bytelen == 1) {
		base_euc = mask[codeset] | _LEN1_BASE;
		end_euc = mask[codeset] | _LEN1_END;
	} else if (bytelen == 2) {
		base_euc = mask[codeset] | _LEN2_BASE;
		end_euc = mask[codeset] | _LEN2_END;
	} else if (bytelen == 3) {
		base_euc = mask[codeset] | _LEN3_BASE;
		end_euc = mask[codeset] | _LEN3_END;
	} else if (bytelen == 4) {
		base_euc = mask[codeset] | _LEN4_BASE;
		end_euc = mask[codeset] | _LEN4_END;
	}

	for (wc = base_euc; wc <= end_euc; wc++) {
		rc = wctomb((char *)str, wc);
		if (rc == -1) {
			continue;
		} else {
			base_dense = _eucpctowc(__lc_charmap, wc);
			if (base_dense == -1) {
				return (WEOF);
			}
			return (base_dense);
		}
	}
	return (WEOF);
}


static void
free_allpointers(int nptr, ...)
{
	va_list	ap;
	void	*p;
	int	i;

	va_start(ap, nptr);

	for (i = 0; i < nptr; i++) {
		p = va_arg(ap, void *);
		if (p) {
			free(p);
		}
	}

	va_end(ap);
}


static void
free_wcptr(
	int	codeset,
	char	*mallocp)
{
	if (_wcptr[codeset]) {
		if (_wcptr[codeset]->code) {
			/* clear conversion table */
			free(_wcptr[codeset]->code);
		}
		if (codeset == 0) {
			if (mallocp && *mallocp) {
				/* if codeset1 and memory for */
				/* index and type have been allocated */
				/* free them. */
				free(_wcptr[0]->index);
				free(_wcptr[0]->type);
				/* clear mallocp */
				*mallocp = 0;
			}
		}
		free(_wcptr[codeset]);
	}
}
#endif /* PIC */
