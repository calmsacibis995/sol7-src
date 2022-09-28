/*
 * Copyright (C) 1989, 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident  "@(#)_base_conv.c	1.2	97/08/08 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "base_conversion.h"
#include <math.h>

/*
 * The routines in this file were done as inline assembly in the pre-v9 library.
 * That exercise seemed unnecessary given current technology, so they have been
 * rendered as simple C routines here.
 *
 * It may be useful to return to these as a performance tweak later...
 */

/*
 * __quorem gets x/y ; *pr gets x%y
 */
unsigned short
__quorem(unsigned int x, unsigned short y, unsigned short *pr)
{
	*pr = (unsigned short) (x % y);
	return ((unsigned short) (x / y));
}

/*
 * __quorem gets x/10000 ; *pr gets x%10000
 */
unsigned short
__quorem10000(unsigned int x, unsigned short *pr)
{
	*pr = (unsigned short) (x % 10000);
	return ((unsigned short) (x / 10000));
}

unsigned int
__longquorem10000(unsigned int d,  unsigned short *pr)
{
	*pr = d % 10000;
	return (d / 10000);
}

/*
 * p = x + c ; return (p/10000 << 16 | * p%10000)
 */
unsigned int
__carry_in_b10000(_BIG_FLOAT_DIGIT x, unsigned int c)
{
	unsigned int   p = x + c;
	unsigned short  r;

	QUOREM10000(p, r);
	return ((p << 16) | r);
}

void
__carry_propagate_ten(unsigned int carry, _BIG_FLOAT_DIGIT *psignificand)
{
	/*
	 * Propagate carries in a base-10**4 significand.
	 */

	int	j;
	unsigned int   p;

	j = 0;
	while (carry != 0) {
		p = __carry_in_b10000(psignificand[j], carry);
		psignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
}

void
__multiply_base_two_vector(unsigned short n, _BIG_FLOAT_DIGIT *px,
    unsigned short *py, _BIG_FLOAT_DIGIT product[3])
{
	/*
	 * Given xi and yi, base 2**16 vectors of length n, computes dot
	 * product
	 *
	 * sum (i=0,n-1) of x[i]*y[n-1-i]
	 *
	 * Product may fill as many as three short-unsigned buckets. Product[0]
	 * is least significant, product[2] most.
	 */

	unsigned int   acc, p;
	unsigned short  carry;
	int	i;

	acc = 0;
	carry = 0;
	for (i = 0; i < (int)n; i++) {
		p = __umac(px[i], py[n - 1 - i], acc);
		if (p < acc)
			carry++;
		acc = p;
	}
	product[0] = (_BIG_FLOAT_DIGIT) (acc & 0xffff);
	product[1] = (_BIG_FLOAT_DIGIT) (acc >> 16);
	product[2] = (_BIG_FLOAT_DIGIT) (carry);
}

void
__multiply_base_ten_vector(unsigned short n,  _BIG_FLOAT_DIGIT *px,
    unsigned short *py,  _BIG_FLOAT_DIGIT product[3])
{
	/*
	 * Given xi and yi, base 10**4 vectors of length n, computes dot
	 * product
	 *
	 * sum (i=0,n-1) of x[i]*y[n-1-i]
	 *
	 * Product may fill as many as three short-unsigned buckets. Product[0]
	 * is least significant, product[2] most.
	 */

#define	ABASE	3000000000	/* Base of accumulator. */

	unsigned int   acc;
	unsigned short  carry;
	int	i;

	acc = 0;
	carry = 0;
	for (i = 0; i < (int)n; i++) {
		acc = __umac(px[i], py[n - 1 - i], acc);
		if (acc >= (unsigned int) ABASE) {
			carry++;
			acc -= ABASE;
		}
	}
	/*
	 * NOTE: because acc <= ABASE-1, acc/10000 <= 299999 which would
	 * overflow a short unsigned
	 */
	LONGQUOREM10000(acc, product[0]);
	QUOREM10000(acc, product[1]);
	product[2] = (_BIG_FLOAT_DIGIT) (acc + ((unsigned int)ABASE
	    / 100000000) * carry);
}

unsigned int
___mul_65536_n(unsigned int carry, _BIG_FLOAT_DIGIT *ps, int n)
{
	int 	j;

	for (j = n; j > 0; j--) {
		carry += ((unsigned int) (*ps)) << 16;
		QUOREM10000(carry, *ps);
		ps++;
	}
	return (carry);
}

/*
 * p = x * y + c ; return p
 */
unsigned int
__umac(_BIG_FLOAT_DIGIT x, _BIG_FLOAT_DIGIT y, unsigned int c)
{
	return (x * (unsigned int)y + c);
}

void
__carry_propagate_two(unsigned int carry, _BIG_FLOAT_DIGIT *psignificand)
{
	/*
	 * Propagate carries in a base-2**16 significand.
	 */

	int unsigned p;
	int j;

	j = 0;
	while (carry != 0) {
		p = __carry_in_b65536(psignificand[j], carry);
		psignificand[j++] = (_BIG_FLOAT_DIGIT) (p & 0xffff);
		carry = p >> 16;
	}
}

/* Convert u to four digits at *s which may not be aligned. */
void
__four_digits_quick(short unsigned u, char *s)
{
	int    carry;
	const char	*pt;

	pt = (const char *)__four_digits_quick_table + ((u >> 1) & ~3);
	carry = (u & 7) + pt[3];
	if (carry <= '9') {
		s[3] = carry;
		goto move3;
	}
	s[3] = carry - 10;
	carry = pt[2];
	if (carry <= '8') {
		s[2] = carry + 1;
		goto move2;
	}
	s[2] = carry - 9;
	carry = pt[1];
	if (carry <= '8') {
		s[1] = carry + 1;
		goto move1;
	}
	s[1] = carry - 9;
	s[0] = pt[0] + 1;
	return;
move3:
	s[2] = pt[2];
move2:
	s[1] = pt[1];
move1:
	s[0] = pt[0];
}

/*
 * Computes number of trailing zeros of a double.
 */
int
__trailing_zeros(double d)
{
	char	*pc, *pc0;

	pc0 = ((char *)&d) + 7;
	pc = pc0;
	while (*pc == 0)
		pc--;
	return (pc0 - pc);
}

/*
 * Multiplies two normal or subnormal doubles, returns result and exceptions.
 */
double
__mul_set(double x, double y, enum __fbe_type *pe)
{
	/*
	 * Result has <= 50 significant bits,
	 * will be exact in any arithmetic.
	 */
	if ((__trailing_zeros(x) + __trailing_zeros(y)) >= 7) {
		*pe = __fbe_none;
	} else {		/* Result may not be exact. */
		*pe = __fbe_one;
	}
	return (x * y);
}

/*
 * Divides two normal or subnormal doubles x/y, returns result and
 * exceptions.
 */
double
__div_set(double x, double y, enum __fbe_type *pe)
{
	double z;

	z = x / y;

	if (((__trailing_zeros(z) + __trailing_zeros(y)) >= 7) &&
	    ((z * y) == x)) {
		/*
		 * Residual has <= 50 significant bits, will be exact in any
		 * arithmetic.
		 */
		*pe = __fbe_none;
	} else {
		/*
		 * Residual may not be inexact, hence result
		 * may not be exact.
		 */
		*pe = __fbe_one;
	}
	return (z);
}

/*
 * Adds two normal or subnormal doubles x+y, returns result and exceptions. x
 * should have larger magnitude.
 */
double
__add_set(double x, double y, enum __fbe_type *pe)
{
	double z;

	z = x + y;

	if ((z - x) == y)
		*pe = __fbe_none;
	else {
		/*
		 * Residual may not be inexact, hence result
		 * may not be exact.
		 */
		*pe = __fbe_one;
	}

	return (z);
}

double
__abs_ulp(double x)
{				/* Returns one ulp of abs(x). */
	double_equivalence kx;

	kx.x = x;
	kx.f.significand2 ^= 1;	/* Flip lsb. */
	kx.x -= x;
	kx.f.msw.sign = 0;
	return (kx.x);
}

#define	MSW	0
/*
 * 2**52 - all doubles >= this
 * are integral
 */
#define	BIGINT	4.503599627370496e15

double
__arint(double x)
{
	double rx;

	if (((int *)(&x))[MSW] >= 0) {
		rx = x + BIGINT;
		rx -= BIGINT;
	} else {
		rx = x - BIGINT;
		rx += BIGINT;
	}
	return (rx);
}

double
__dabs(double d)
{
	return ((d < 0.0) ? -d : d);
}

typedef const union {
	unsigned int ul[2];
	double d;
} union_ul_d;

typedef const union {
	unsigned int ul[4];
	long double ld;
} union_ul_ld;

static union_ul_d NaNx = 	{ 0x7fffffff, 0xffffffffU };
static union_ul_d Infx = 	{ 0x7ff00000, 0x00000000 };
static union_ul_d fmaxx =	{ 0x7fefffff, 0xffffffffU };
static union_ul_d fminx =	{ 0x00000000, 0x00000001 };
static union_ul_d sNaNx =	{ 0x7ff00000, 0x00000001 };
static union_ul_d fmaxsx =	{ 0x000fffff, 0xffffffffU };
static union_ul_d fminnx =	{ 0x00100000, 0x00000000 };

double
min_subnormal(void)
{
	return (fminx.d);
}

double
max_subnormal(void)
{
	return (fmaxsx.d);
}

double
min_normal(void)
{
	return (fminnx.d);
}

double
max_normal(void)
{
	return (fmaxx.d);
}

double
infinity(void)
{
	return (Infx.d);
}


double
quiet_nan(int n)	/* was long */
{
	return (NaNx.d);
}

double
signaling_nan(int n)	/* was long */
{
	return (sNaNx.d);
}
