/*
 *	generic_key.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)generic_key.c	1.1	97/11/19 SMI"

#include <mp.h>
#include <time.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#define	BASEBITS		(8 * sizeof (short) - 1)
#define	BASE			(1 << BASEBITS)

extern void des_setparity(char *);
extern void des_setparity_g(des_block *);

/*
 * Generate a seed
 */
static unsigned short *
getseed(int keylen, unsigned char *pass)
{
	int i;
	int rseed;
	struct timeval tv;
	unsigned short *seedval;
	char *seed;
	int seedsize = keylen / BASEBITS + 1;
	extern short * _mp_xalloc(int, char *);

	seedval = (unsigned short *) _mp_xalloc(seedsize, "");
	seed = (char *)seedval;

	(void) gettimeofday(&tv, (struct timezone *)NULL);
	rseed = tv.tv_sec + tv.tv_usec;

	for (i = 0; i < 8; i++) {
		rseed ^= (rseed << 8) | pass[i];
	}
	(void) srandom(rseed);

	for (i = 0; i < seedsize; i++) {
		seed[i] = (random() & 0xff) ^ pass[i % 8];
	}

	return (seedval);
}

/*
 * Adjust the input key so that it is 0-filled on the left
 */
static void
adjust(char *keyout, char *keyin, int keylen)
{
	char *p;
	char *s;
	int hexkeybytes = (keylen+3)/4;

	for (p = keyin; *p; p++);
	for (s = keyout + hexkeybytes; p >= keyin; p--, s--) {
		*s = *p;
	}
	while (s >= keyout) {
		*s-- = '0';
	}
}

void
__generic_gen_dhkeys(int keylen, char *xmodulus, int proot,
    char *public, char *secret, char *pass)
{
	int i;
	MINT *pk = mp_itom(0);
	MINT *sk = mp_itom(0);
	MINT *tmp;
	MINT *base = mp_itom(BASE);
	MINT *root = mp_itom(proot);
	MINT *modulus = mp_xtom(xmodulus);
	unsigned short r;
	unsigned short *seed;
	char *xkey;

	seed = getseed(keylen, (u_char *)pass);
	for (i = 0; i < keylen/(BASEBITS + 1); i++) {
		r = seed[i] %((unsigned short)BASE);
		tmp = mp_itom(r);
		mp_mult(sk, base, sk);
		mp_madd(sk, tmp, sk);
		mp_mfree(tmp);
	}
	free(seed);

	tmp = mp_itom(0);
	mp_mdiv(sk, modulus, tmp, sk);
	mp_mfree(tmp);
	mp_pow(root, sk, modulus, pk);
	xkey = mp_mtox(sk);
	(void) adjust(secret, xkey, keylen);
	xkey = mp_mtox(pk);
	(void) adjust(public, xkey, keylen);

	mp_mfree(sk);
	mp_mfree(base);
	mp_mfree(pk);
	mp_mfree(root);
	mp_mfree(modulus);
}

static void
extractdeskeys(MINT *ck, int keylen, des_block keys[], int keynum)
{
	MINT *a;
	short r;
	int i;
	short base = (1 << 8);
	char *k;
	int len = 8 * sizeof (des_block) * keynum;
	extern void _mp_move(MINT *, MINT *);

	a = mp_itom(0);
	_mp_move(ck, a);

	for (i = 0; i < ((keylen - len)/2)/8; i++)
		mp_sdiv(a, base, a, &r);

	k = (char *)keys;
	for (i = 0; i < sizeof (des_block) * keynum; i++) {
		mp_sdiv(a, base, a, &r);
		*k++ = r;
	}

	mp_mfree(a);

	for (i = 0; i < keynum; i++)
		if (keylen == 192)
			des_setparity((char *)&keys[i]);
		else
			des_setparity_g(&keys[i]);
}



void
__generic_common_dhkeys(char *pkey, char *skey, int keylen,
    char *xmodulus, des_block keys[], int keynum)
{
	MINT *pk = mp_xtom(pkey);
	MINT *sk = mp_xtom(skey);
	MINT *modulus = mp_xtom(xmodulus);
	MINT *ck = mp_itom(0);

	mp_pow(pk, sk, modulus, ck);

	extractdeskeys(ck, keylen, keys, keynum);

	mp_mfree(pk);
	mp_mfree(sk);
	mp_mfree(modulus);
	mp_mfree(ck);
}
