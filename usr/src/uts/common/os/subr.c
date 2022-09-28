/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subr.c	1.47	97/11/01 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/tuneable.h>
#include <sys/cpuvar.h>
#include <sys/archsystm.h>
#include <sys/map.h>
#include <vm/seg_kmem.h>
#include <sys/vmmac.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <sys/model.h>
#include <sys/kmem.h>
#include <sys/strlog.h>

/*
 * Routine which sets a user error; placed in
 * illegal entries in the bdevsw and cdevsw tables.
 */

int
nodev()
{
	return (curthread->t_lwp ?
	    ttolwp(curthread)->lwp_error = ENXIO : ENXIO);
}

/*
 * Null routine; placed in insignificant entries
 * in the bdevsw and cdevsw tables.
 */

int
nulldev()
{
	return (0);
}

static kmutex_t udevlock;

/*
 * Generate an unused major device number.
 */
major_t
getudev(void)
{
	static major_t next = 0;
	major_t ret;

	/*
	 * As above, ensure that we start allocating major numbers
	 * above the 'devcnt' count.  The only limit we
	 * place on the number is that it should be a legal
	 * 32-bit SVR4 major number and be greater than or
	 * equal to devcnt in the current system)
	 */
	mutex_enter(&udevlock);
	if (next == 0)
		next = devcnt;
	if (next < L_MAXMAJ32 && next >= devcnt)
		ret = next++;
	else
		ret = (major_t)-1;
	mutex_exit(&udevlock);
	return (ret);
}

/*
 * Compress 'long' device number encoding to 32-bit device number
 * encoding.  If it won't fit, we return failure, but set the
 * device number to 32-bit NODEV for the sake of our callers.
 */
int
cmpldev(dev32_t *dst, dev_t dev)
{
#if defined(_LP64)
	if (dev == NODEV) {
		*dst = NODEV32;
	} else {
		major_t major = dev >> L_BITSMINOR;
		minor_t minor = dev & L_MAXMIN;

		if (major > L_MAXMAJ32 || minor > L_MAXMIN32) {
			*dst = NODEV32;
			return (0);
		}

		*dst = (dev32_t)((major << L_BITSMINOR32) | minor);
	}
#else
	*dst = (dev32_t)dev;
#endif
	return (1);
}

/*
 * Expand 32-bit dev_t's to long dev_t's.  Expansion always "fits"
 * into the return type, but we're careful to expand NODEV explicitly.
 */
dev_t
expldev(dev32_t dev32)
{
#ifdef _LP64
	if (dev32 == NODEV32)
		return (NODEV);
	return (makedevice((dev32 >> L_BITSMINOR32) & L_MAXMAJ32,
	    dev32 & L_MAXMIN32));
#else
	return ((dev_t)dev32);
#endif
}

/*
 * C-library string functions.  Assembler versions of others are in
 * ml/string.s.
 */

/*
 * Copy s2 to s1, truncating or null-padding to always copy n bytes.
 * Return s1.
 */
char *
strncpy(char *s1, const char *s2, size_t n)
{
	char *os1 = s1;

	n++;
	while (--n != 0 && (*s1++ = *s2++) != '\0')
		;
	if (n != 0)
		while (--n != 0)
			*s1++ = '\0';
	return (os1);
}

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
 */
char *
strrchr(const char *sp, int c)
{
	const char *r = 0;

	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);

	return ((char *)r);
}

/*
 * Like strrchr(), except
 * (a) it takes a maximum length for the string to be searched, and
 * (b) if the string ends with a null, it is not considered part of
 *     the string.
 */
char *
strnrchr(const char *sp, int c, size_t n)
{
	const char *r = 0;

	while (n-- > 0 && *sp) {
		if (*sp == c)
			r = sp;
		sp++;
	}

	return ((char *)r);
}

/*
 * Compare two byte streams.  Returns 0 if they're identical, 1
 * if they're not.
 */
int
bcmp(const void *s1_arg, const void *s2_arg, size_t len)
{
	const char *s1 = s1_arg;
	const char *s2 = s2_arg;

	while (len--)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

int
memlow(void)
{
	return (freemem <= tune.t_gpgslo);
}

/* takes a numeric char, yields an int */
#define	CTOI(c)		((c) & 0xf)
/* takes an int, yields an int */
#define	TEN_TIMES(n)	(((n) << 3) + ((n) << 1))

/*
 * Returns the integer value of the string of decimal numeric
 * chars beginning at **str.
 * Does no overflow checking.
 * Note: updates *str to point at the last character examined.
 */
int
stoi(char **str)
{
	char	*p = *str;
	int	n;
	int	c;

	for (n = 0; (c = *p) >= '0' && c <= '9'; p++) {
		n = TEN_TIMES(n) + CTOI(c);
	}
	*str = p;
	return (n);
}

/*
 * Simple-minded conversion of a long into a null-terminated character
 * string.  Caller must ensure there's enough space to hold the result.
 */
void
numtos(unsigned long num, char *s)
{
	char prbuf[40];

	char *cp = prbuf;

	do {
		*cp++ = "0123456789"[num % 10];
		num /= 10;
	} while (num);

	do {
		*s++ = *--cp;
	} while (cp > prbuf);
	*s = '\0';
}

/*
 * Infinity is a concept and not a number.
 * Previously we were understanding infinity as a number and this
 * caused problems when we tried to increase the limit to maximum
 * representable value.
 * Here we use the rlim_infinity_map to help us with the correct
 * system maximum for the concept Infinity.
 * rlim_infinity_map maps the unlimited value to system defined maximum.
 * The values are defined in param.c file.
 * We also define macros in user.h file to get the actual values of
 * the system limits if the user structure has some resource as unlimited.
 */

int
rlimit(int resource, rlim64_t softlimit, rlim64_t hardlimit)
{
	struct proc *p = ttoproc(curthread);

	rlim64_t actual_softlimit, actual_hardlimit;
	rlim64_t current_hardlimit;

	if (softlimit == RLIM64_INFINITY && hardlimit != RLIM64_INFINITY)
		return (EINVAL);

	actual_softlimit = (softlimit == RLIM64_INFINITY) ?
	    rlim_infinity_map[resource] : softlimit;
	actual_hardlimit = (hardlimit == RLIM64_INFINITY) ?
	    rlim_infinity_map[resource] : hardlimit;

	if (actual_softlimit > actual_hardlimit)
		return (EINVAL);

#ifdef _LP64
	current_hardlimit = u.u_rlimit[resource].rlim_max;
#else
	/*
	 * Resource limits are now longlong's and therefore
	 * reads are no longer atomic.
	 */
	mutex_enter(&p->p_lock);
	current_hardlimit = u.u_rlimit[resource].rlim_max;
	mutex_exit(&p->p_lock);
#endif

	current_hardlimit = (current_hardlimit == RLIM64_INFINITY) ?
	    rlim_infinity_map[resource] : current_hardlimit;

	if (actual_hardlimit > current_hardlimit && !suser(CRED()))
		return (EPERM);

	if (actual_softlimit > rlim_infinity_map[resource])
		softlimit = rlim_infinity_map[resource];

	if (actual_hardlimit > rlim_infinity_map[resource])
		hardlimit = rlim_infinity_map[resource];

	/*
	 * Prevent multiple threads from updating the
	 * rlimit members in a random order.
	 */
	mutex_enter(&p->p_lock);
	u.u_rlimit[resource].rlim_cur = softlimit;
	u.u_rlimit[resource].rlim_max = hardlimit;
	mutex_exit(&p->p_lock);

	return (0);
}

#ifndef _LP64
/*
 * Keep these entry points for 32-bit systems but enforce the use
 * of MIN/MAX macros on 64-bit systems.  The DDI header files already
 * define min/max as macros so drivers shouldn't need these functions.
 */

int
min(int a, int b)
{
	return (a < b ? a : b);
}

int
max(int a, int b)
{
	return (a > b ? a : b);
}

u_int
umin(u_int a, u_int b)
{
	return (a < b ? a : b);
}

u_int
umax(u_int a, u_int b)
{
	return (a > b ? a : b);
}

#endif /* !_LP64 */

/*
 * XXX - strictly speaking, the following routines should
 * go in common/os/string.c or something.
 */

/*
 * Note:  strlen() is implemented in assembly language for performance.
 */

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}

/*
 * Compare strings (at most n bytes): return *s1-*s2 for the last
 * characters in s1 and s2 which were compared.
 */
int
strncmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == s2)
		return (0);
	n++;
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return ((n == 0) ? 0 : *s1 - *--s2);
}

/*
 * Return bit position of least significant bit set in mask,
 * starting numbering from 1.
 */
int
ffs(long mask)
{
	int i;

	if (mask == 0)
		return (0);
	for (i = 1; i <= NBBY * sizeof (mask); i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}
	return (0);
}

/* Concatenate string s2 to string s1 */
char *
strcat(char *s1, const char *s2)
{
	char *s = s1;

	while (*s)
		s++;		/* find the end of string s1 */

	(void) strcpy(s, s2);
	return (s1);
}

/*
 * Return the ptr in sp at which the character c first
 * appears; NULL if not found
 */
char *
strchr(const char *sp, int c)
{
	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (0);
}

/*
 * Convert hex string to u_int. Assumes no leading "0x".
 */
u_int
atou(char *s)
{
	u_int val = 0;
	u_int digit;

	while (*s) {
		if (*s >= '0' && *s <= '9')
			digit = *s++ - '0';
		else if (*s >= 'a' && *s <= 'f')
			digit = *s++ - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F')
			digit = *s++ - 'A' + 10;
		else
			break;
		val = (val * 16) + digit;
	}
	return (val);
}

/*
 * Tables to convert a single byte to/from binary-coded decimal (BCD).
 */
u_char byte_to_bcd[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
};

u_char bcd_to_byte[256] = {		/* CSTYLED */
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
	60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89,  0,  0,  0,  0,  0,  0,
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
};

/*
 * Hot-patch a single instruction in the kernel's text.
 * If you want to patch multiple instructions you must
 * arrange to do it so that all intermediate stages are
 * sane -- we don't stop other cpus while doing this.
 * Size must be 1, 2, or 4 bytes with iaddr aligned accordingly.
 */
void
hot_patch_kernel_text(caddr_t iaddr, uint32_t new_instr, u_int size)
{
	caddr_t vaddr;
	uintptr_t off = (uintptr_t)iaddr & PAGEOFFSET;

	vaddr = kmxtob(rmalloc_wait(kernelmap, 1));
	segkmem_mapin(&kvseg, vaddr, PAGESIZE, PROT_READ | PROT_WRITE,
	    hat_getkpfnum(iaddr - off), HAT_LOAD_NOCONSIST);

	switch (size) {
	case 1:
		*(uint8_t *)(vaddr + off) = new_instr;
		break;
	case 2:
		*(uint16_t *)(vaddr + off) = new_instr;
		break;
	case 4:
		*(uint32_t *)(vaddr + off) = new_instr;
		break;
	default:
		panic("illegal hot-patch");
	}

	membar_enter();
	sync_icache(vaddr + off, size);
	sync_icache(iaddr, size);
	segkmem_mapout(&kvseg, vaddr, PAGESIZE);
	rmfree(kernelmap, 1, btokmx(vaddr));
}

/*
 * Routine to report an attempt to execute non-executable data.  If the
 * address executed lies in the stack, explicitly say so.
 */
void
report_stack_exec(proc_t *p, caddr_t addr)
{
	char *buf;
	const size_t bufsize = 200;

	if (!noexec_user_stack_log)
		return;

	buf = kmem_alloc(bufsize, KM_SLEEP);

	if (addr < p->p_usrstack && addr >= (p->p_usrstack - p->p_stksize)) {
		(void) sprintf(buf, "%s[%d] attempt to execute code "
		    "on stack by uid %d\n", p->p_user.u_comm,
		    p->p_pid, p->p_cred->cr_ruid);
	} else {
		(void) sprintf(buf, "%s[%d] attempt to execute non-executable "
		    "data at 0x%p by uid %d\n", p->p_user.u_comm,
		    p->p_pid, (void *) addr, p->p_cred->cr_ruid);
	}

	(void) strlog(0, 1, 0, SL_CONSOLE | SL_NOTE, buf, 0);

	kmem_free(buf, bufsize);
	delay(hz / 50);
}
