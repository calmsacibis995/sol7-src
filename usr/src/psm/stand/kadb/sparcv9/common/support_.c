/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)support_sparcv9.c	1.12	98/02/17 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <time.h>
#include <tzfile.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/privregs.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#include <sys/openprom.h>
#include <stdarg.h>
#include <adb.h>
#include <ptrace.h>
#include <allregs.h>
#include <sys/promif.h>

#ifdef sun4u
extern int wp_mask;
#endif
u_int npmgrps;
u_int segmask;
int debugkadb = 0;
extern struct bootops *bootops;
extern int pagesize;
extern struct allregs_v9 regsave;
extern char *map(int);
extern void reload_prom_callback();
extern void wp_clrall();
extern	void wp_off();
extern	void wp_vwrite();
extern	void wp_vaccess();
extern	void wp_paccess();
extern	void fiximp();

extern void putchar(int);

int adb_more = 0;
static int more = 0;
static int one_line = 0;
int interrupted = 0;
extern int elf64mode;


static
void
dointr(doit)
{

	putchar('^');
	putchar('C');
	interrupted = 1;
	if (abort_jmp && doit) {
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}
}

void
_exit()
{
	(void) prom_enter_mon();
#ifdef	sun4u
	(void) reload_prom_callback();
#endif
}

/*
 * Print a character on console.
 */
void
putchar(int c)
{
	if (c == '\n') {
		(void) prom_putchar('\r');
		if (one_line || (adb_more && (++more >= adb_more))) {
			one_line = 0;
			prom_printf("\n--More-- ");
			more = 0;
			c = prom_getchar();
			prom_printf("\r        \r");
			if ((c == 'c') || (c == 'C') || (c == ('c' & 037)))
				dointr(1);
			else if (c == '\r')
				one_line = 1;
		} else
			(void) prom_putchar(c);
	} else
		(void) prom_putchar(c);
}

getchar()
{
	register int c;

	one_line = 0;
	while ((c = prom_mayget()) == -1)
		;
	if (c == '\r')
		c = '\n';
	if (c == 0177 || c == '\b') {
		putchar('\b');
		putchar(' ');
		c = '\b';
	}
	putchar(c);
	more = 0;
	return (c);
}

/*
 * Read a line into the given buffer and handles
 * erase (^H or DEL), kill (^U), and interrupt (^C) characters.
 * This routine ASSUMES a maximum input line size of LINEBUFSZ
 * to guard against overflow of the buffer from obnoxious users.
 */
void
gets(buf)
	char buf[];
{
	register char *lp = buf;
	register c;

	for (;;) {
		c = getchar() & 0177;
		switch (c)	{
		case '[':
		case ']':
			putchar('\n');
			if (lp != buf)
				goto defchar;
			*lp++ = (char) c;
			/* FALLTHROUGH */
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
			lp--;
			if (lp < buf)
				lp = buf;
			continue;
		case 'u'&037:			/* ^U */
			lp = buf;
			putchar('^');
			putchar('U');
			putchar('\n');
			continue;
		case 'c'&037:
			dointr(1);
			/* MAYBE REACHED */
			/* fall through */
		default:
		defchar:
			if (lp < &buf[LINEBUFSZ-1]) {
				*lp++ = (char) c;
			} else {
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
		}
	}
}


/*
 * Check for ^C on input
 */
void
tryabort(doit)
{

	if (prom_mayget() == ('c' & 037)) {
		dointr(doit);
		/* MAYBE REACHED */
	}
}

/*
 * Implement pseudo ^S/^Q processing along w/ handling ^C
 * We need to strip off high order bits as monitor cannot
 * reliably figure out if the control key is depressed when
 * prom_mayget() is called in certain circumstances.
 * Unfortunately, this means that s/q will work as well
 * as ^S/^Q and c as well as ^C when this guy is called.
 */
void
trypause()
{
	register int c;

	c = prom_mayget() & 037;

	if (c == ('s' & 037)) {
		while ((c = prom_mayget() & 037) != ('q' & 037)) {
			if (c == ('c' & 037)) {
				dointr(1);
				/* MAYBE REACHED */
			}
		}
	} else if (c == ('c' & 037)) {
		dointr(1);
		/* MAYBE REACHED */
	}
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
void
printn(u_long n, int b)
{
	char prbuf[11];
	register char *cp;

	if (b == 10 && (long)n < 0) {
		putchar('-');
		n = (unsigned)(-(long)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		putchar(*--cp);
	while (cp > prbuf);
}
/*
 * Scaled down version of C Library printf.
 */

void
prf(const char *fmt, va_list adx)
{
	register int b, c;
	register char *s;

loop:
	while ((c = *fmt++) != '%') {
	if (c == '\0')
			return;
		putchar(c);
	}
again:
	c = *fmt++;
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn(va_arg(adx, u_long), b);
		break;
	case 'c':
		b = va_arg(adx, int);
		putchar(b);
		break;
	case 's':
		s = va_arg(adx, char *);
		while (c = *s++)
			putchar(c);
		break;
	}
	goto loop;
}


/*VARARGS1*/
printf(const char *fmt, ...)
{
	va_list x1;

	tryabort(1);
	va_start(x1, fmt);
	prf(fmt, x1);
	va_end(x1);
	return (0);
}
/*
 * Fake gettimeofday call
 * Needed for ctime - we are lazy and just
 * give a bogus approximate answer
 */
void
gettimeofday(struct timeval *tp, struct timezone *tzp)
{

	tp->tv_sec = (1989 - 1970) * 365 * 24 * 60 * 60;	/* ~1989 */
	tzp->tz_minuteswest = 8 * 60;	/* PDT: California ueber alles */
	tzp->tz_dsttime = DST_USA;
}

int errno;

caddr_t
_sbrk(int incr)
{
	extern char start[], end[];
	static caddr_t lim;
	caddr_t val;
	register int i;

	if (nobrk) {
		printf("sbrk:  late call\n");
		errno = ENOMEM;
		return ((caddr_t)-1);
	}
	if (lim == 0) {
		lim = (caddr_t)roundup((uintptr_t)end, pagesize);
	}
	if (incr == 0)
		return (lim);
	incr = btopr(incr);
	if ((lim + ptob(incr)) >= (caddr_t)((u_int)start + DEBUGSIZE)) {
		printf("sbrk:  lim %x + %x exceeds %x\n", lim,
		    ptob(incr), (u_int)start + DEBUGSIZE);
		errno = EINVAL;
		return ((caddr_t)-1);
	}
	if ((val = BOP_ALLOC(bootops, lim, ptob(incr), BO_NO_ALIGN)) != lim) {
		printf("sbrk: BOP_ALLOC failed.\n");
		errno = EINVAL;
		return ((caddr_t)-1);
	}
	pagesused += incr;
	for (i = 0; i < incr; i++, lim += pagesize) {
#if defined(sun4c)
		Setpgmap(lim, PG_V | PG_KW | PGT_OBMEM | getpgmap(lim));
#endif
#ifdef  sun4m
#if 0
		/*
		* XXX - Include VA_TO_PFN so that the pte protection
		* can be changed to be supervisor only.
		*/
		pfn = VA_TO_PFN(addr);
		tmppte.pte_int = PTEOF(0, pfn, MMU_STD_SRWX, 0);
		tmppte.pte_int = PTEOF(0, --lastpg, MMU_STD_SRWX, 0);
		Setpgmap(lim, tmppte.pte_int);
		mmu_flushall();
#endif
#endif
	}
	return (val);
}

#define	PHYSOFF(p, o)	\
	((physadr)(p)+((o)/sizeof (((physadr)0)->r[0])))

/*
 * Fake ptrace - ignores pid and signals
 * Otherwise it's about the same except the "child" never runs,
 * flags are just set here to control action elsewhere.
 */
ptrace(int request, int pid, char *addr, int data, char *addr2)
{
	int rv = 0;

	db_printf(5, "ptrace: %s, pid=%X addr=%X, data=%X, addr2=%X sp=%X",
	    map(request), pid, addr, data, addr2, getsp());

	switch (request) {
	case PTRACE_TRACEME:	/* do nothing */
		break;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		{		/* start of block */
		int success;
		char *dstaddr;

		rv = 0;
		success = 0;
#ifdef KADB64
		rv = peek32(addr);
		db_printf(2, "peek32 returned %X\n", rv);
#else
		rv = peekl(addr);
#endif
		success |= (errno == 0);
		if (errno) {
			dstaddr = (char *)&rv;
			*((short *)dstaddr) = peek((short *)addr);
			success |= (errno == 0);
			if (errno) {
				*dstaddr = Peekc(addr);
				success |= (errno == 0);
				*(dstaddr+1) = Peekc(addr+1);
				success |= (errno == 0);
			}
			*((short *)dstaddr+2) = peek((short *)(addr+2));
			success |= (errno == 0);
			if (errno) {
				*(dstaddr+2) = Peekc(addr+2);
				success |= (errno == 0);
				*(dstaddr+3) = Peekc(addr+3);
				success |= (errno == 0);
			}
		}

		if (!success) {
			rv = -1;
			errno = EFAULT;
		}
		}	/* end of block */
		break;

	case PTRACE_PEEKUSER:
		break;

	case PTRACE_POKEUSER:
		break;

	case PTRACE_POKETEXT:
		rv = poketext(addr, data);
		break;

	case PTRACE_POKEDATA:
		{		/* start of block */
		int success;
		char *datap;

		rv = 0;
		success = 0;
#ifdef	KADB64
		rv = poke32(addr, data);
		db_printf(2, "poke32 returned %X\n", rv);
#else
		rv = pokel(addr, data);
#endif
		success |= (errno == 0);
		if (errno) {
			datap = (char *)&data;
			rv = pokes((short *)addr, *(short *)datap);
			success |= (errno == 0);
			if (errno) {
				rv = pokec(addr, *datap);
				success |= (errno == 0);
				rv = pokec(addr+1, *(datap+1));
				success |= (errno == 0);
			}
			rv = pokes((short *)(addr+2), *(short *)(datap+2));
			success |= (errno == 0);
			if (errno) {
				rv = pokec(addr+2, *(datap+2));
				success |= (errno == 0);
				rv = pokec(addr+3, *(datap+3));
				success |= (errno == 0);
			}
		}

		if (!success) {
			rv = -1;
			errno = EFAULT;
		}
		}	/* end of block */
		break;

	case PTRACE_SINGLESTEP:
		dotrace = 1;
		/* FALLTHROUGH */
	case PTRACE_CONT:
		dorun = 1;
		if ((uintptr_t)addr != 1) {
#ifdef	__sparcv9cpu
			reg->r_pc = regsave.r_pc = (u_long)addr;
#else
			reg->r_pc = (int)addr;
#endif
		}
		break;

	case PTRACE_SETREGS:
#ifdef	__sparcv9cpu
		rv = scopy(addr, (caddr_t)&regsave,
		    sizeof (struct allregs_v9));
#else
		rv = scopy(addr, (caddr_t)reg,
		    sizeof (struct allregs));
#endif
		break;

	case PTRACE_GETREGS:
#ifdef	__sparcv9cpu
		rv = scopy((caddr_t)&regsave, addr,
		    sizeof (struct allregs_v9));
#else
		rv = scopy((caddr_t)reg, addr,
		    sizeof (struct allregs));
#endif

		break;

	case PTRACE_WRITETEXT:
	case PTRACE_WRITEDATA:
		rv = scopy(addr2, addr, data);
		break;

	case PTRACE_READTEXT:
	case PTRACE_READDATA:
		rv = scopy(addr, addr2, data);
		break;

	case PTRACE_WRITEPHYS:
		rv = physmem_write((unsigned long) addr, addr2, data);
		break;

	case PTRACE_READPHYS:
		rv = physmem_read((unsigned long) addr, addr2, data);
		break;

#if defined(sun4u)
	/*
	 * Data breakpoints (i.e. watchpoints). Note that only one
	 * virtual and one physical watchpoint are allowed at a time
	 * on Fusion.
	 */
	case PTRACE_CLRBKPT:		/* clear WP */
		if (addr == (char *)0)	/* clear all WP */
			wp_clrall();
		else
			wp_off(addr);
		break;
	case PTRACE_SETWR:		/* WP - write to vaddr */
		wp_vwrite(addr, wp_mask);
		break;
	case PTRACE_SETAC:		/* WP - (r/w) to vaddr */
		wp_vaccess(addr, wp_mask);
		break;
	case PTRACE_WPPHYS:		/* WP - (r/w) to paddr */
		wp_paccess(addr, wp_mask);
		break;
#endif

	case PTRACE_KILL:
	case PTRACE_ATTACH:
	case PTRACE_DETACH:
	default:
		errno = EINVAL;
		rv = -1;
		break;
	}
	return (rv);
}

/*
 * This localtime is a modified version of offtime from libc, which does not
 * bother to figure out the time zone from the kernel, from environment
 * varaibles, or from Unix files.  We just return things in GMT format.
 */

static int mon_lengths[2][MONS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

struct tm *
localtime(const time_t *clock)
{
	register struct tm *tmp;
	register long days;
	register long rem;
	register int y;
	register int yleap;
	register int *ip;
	static struct tm tm;

	tmp = &tm;
	days = *clock / SECS_PER_DAY;
	rem = *clock % SECS_PER_DAY;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tmp->tm_hour = (int)(rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int)(rem / SECS_PER_MIN);
	tmp->tm_sec = (int)(rem % SECS_PER_MIN);
	tmp->tm_wday = (int)((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0)
	for (;;)	{
			yleap = isleap(y);
			if (days < (long)year_lengths[yleap])
				break;
			++y;
			days = days - (long)year_lengths[yleap];
		} else
		do {
		--y;
		yleap = isleap(y);
		days = days + (long)year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int)days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long)ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long)ip[tmp->tm_mon];
	tmp->tm_mday = (int)(days + 1);
	tmp->tm_isdst = 0;
	/*
	 * tm_zone and and tm_gmoff are not defined in the tm struct anymore.
	 * Find out what to do about this.
	tmp->tm_zone = "GMT";
	tmp->tm_gmtoff = 0;
	 */
	return (tmp);
}

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */


char *
index(char *sp, char c)
{

	do {
		if (*sp == c)
			return (sp);
	} while (*sp++);
	return (NULL);
}

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
 */

char *
rindex(char *sp, char c)
{
	register char *r;

	r = NULL;
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return (r);
}

extern char target_bootname[];
extern char target_bootargs[];
extern char aline[];
extern char *module_path;
extern char start[];
u_long start_addr = (u_long)start;

/*
 * Property intercept routines for kadb, so that it can
 * tell unix it's real name, and it's real bootargs. We
 * also let it figure out our virtual start and end addresses
 * rather than hardcoding them somewhere nasty.
 */
static int
kadb_getprop(struct bootops *bop, char *name, void *buf)
{
	if (strcmp("whoami", name) == 0) {
		(void) strcpy(buf, aline);
	} else if (strcmp("boot-args", name) == 0) {
		(void) strcpy(buf, target_bootargs);
	} else if (strcmp("debugger-start", name) == 0) {
		caddr32_t tmp_addr;
		tmp_addr = (caddr32_t) start_addr;
		if (elf64mode)
			bcopy(&start_addr, buf, sizeof (caddr_t));
		else
			bcopy(&tmp_addr, buf, sizeof (caddr32_t));
	} else if (strcmp("module-path", name) == 0) {
		(void) strcpy(buf, module_path);
	} else
		return (BOP_GETPROP(bop->bsys_super, name, buf));
	return (0);
}

static int
kadb_getproplen(struct bootops *bop, char *name)
{
	if (strcmp("whoami", name) == 0) {
		return (strlen(aline) + 1);
	} else if (strcmp("boot-args", name) == 0) {
		return (strlen(target_bootargs) + 1);
	} else if (strcmp("debugger-start", name) == 0) {
		return (sizeof (void *));
	} else if (strcmp("module-path", name) == 0) {
		return (strlen(module_path) + 1);
	} else
		return (BOP_GETPROPLEN(bop->bsys_super, name));
}

int
kadb_1275_call(void *p)
{
	boot_cell_t *args = (boot_cell_t *)p;
	char	*name;
	int	(*bsys_1275_call)(void *);

	name = boot_cell2ptr(args[0]);
	if (strcmp(name, "getprop") == 0) {
		args[5] = boot_int2cell(kadb_getprop(bootops,
		    boot_cell2ptr(args[3]), boot_cell2ptr(args[4])));
		return (BOOT_SVC_OK);
	} else if (strcmp(name, "getproplen") == 0) {
		args[4] = boot_int2cell(kadb_getproplen(bootops,
		    boot_cell2ptr(args[3])));
		return (BOOT_SVC_OK);
	}
	bsys_1275_call = (int (*)(void *))bootops->bsys_super->bsys_1275_call;
	return ((bsys_1275_call)(p));
}

/* ARGSUSED */
void
early_startup(union sunromvec *rump, int shim, struct bootops *buutops)
{
	extern struct bootops *bootops;
	static struct bootops kadb_bootops;
	extern int kadb_1275_wrapper(void *);

	/*
	 * Save parameters from boot in obvious globals, and set
	 * up the bootops to intercept property look-ups.
	 */
	bootops = buutops;

	prom_init("kadb", rump);
	if (debugkadb)
		(void) prom_enter_mon();

	if (BOP_GETVERSION(bootops) != BO_VERSION) {
		prom_printf("WARNING: %d != %d => %s\n",
		    BOP_GETVERSION(bootops), BO_VERSION,
		    "mismatched version of /boot interface.");
	}

	bcopy((caddr_t)bootops, (caddr_t)&kadb_bootops,
	    sizeof (struct bootops));

	kadb_bootops.bsys_super = bootops;
	/*kadb_bootops.bsys_1275_call = (uint64_t)kadb_1275_call;*/
	kadb_bootops.bsys_1275_call = (uint64_t)kadb_1275_wrapper;

	bootops = &kadb_bootops;

	(void) fiximp();
}
