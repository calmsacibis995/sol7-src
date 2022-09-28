/*
 * Copyright (c) 1992-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	i86pc memory/hardware probe routines
 *
 *	This file contains memory management routines to provide
 *	Sparc machine prom functionality
 */

#pragma ident	"@(#)probe.c	1.41	98/02/11 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootdef.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/cmosram.h>
#include <sys/machine.h>
#include <sys/eisarom.h>
#include <sys/salib.h>
#include <sys/promif.h>
#include "cbus.h"

#define	ptalign(p) 	((int *)((uint)(p) & ~(MMU_PAGESIZE-1)))

void dcache_l1l2(void);
void ecache_l1l2(void);
int ibm_l2(void);
void disablecache(void);
void enablecache(void);
void flushcache(void);
paddr_t get_fontptr(void);
void get_eisanvm(void);
void holdit(void);
void read_default_memory(void);
int fastmemchk(ulong memsrt, ulong cnt, ulong step, ushort flag);
int memwrap(ulong memsrt, ulong memoff);
int memchk(ulong memsrt, ulong cnt, ushort flag);
void memtest(void);
paddr_t getfontptr(unsigned short which);
void probe_eisa(void);
void probe(void);
int qsamap(struct bootmem *map);
int int15_e801(struct bootmem *map);
int validate_entry(uint64_t in_base, uint64_t in_len, uint *ret_base32p,
	uint *ret_len32p, uint64_t *ret_base64p, uint64_t *ret_len64p);

extern caddr_t rm_malloc(size_t size, u_int align, caddr_t virt);
extern void rm_free(caddr_t, size_t);
extern void wait100ms(void);
extern struct sysenvmt *sep;
extern struct bootinfo *bip;
extern struct bootenv *btep;
extern struct bootops *bop;
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *s1, void *s2, size_t n);
extern BOOL CbusInitializePlatform(struct bootmem *, unsigned long,
				int *, int, ushort);
extern void check_hdbios(void);
extern int doint(void);
extern int is_MC(void);
extern int is_EISA(void);
extern int is486(void);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

#define	SYS_MODEL() *(char *)0xFFFFE
#define	MODEL_AT	(unchar)0xFC
#define	MODEL_MC	(unchar)0xF8

#define	COMPAQ_STRING_ADDR	((char *)0xFFFEA)
#define	COMPAQ_STRING		"COMPAQ"

#define	EISAIDLOC ((long *)0xFFFD9)
#define	EISAIDSTR (*(long *)"EISA")

#define	MEM1M	(ulong)0x100000
#define	MEM4M	(ulong)0x400000
#define	MEM32M	(ulong)0x2000000
#define	MEM64M	(ulong)0x4000000
#define	MEM1G	(ulong)0x40000000
#define	MEM2G	(ulong)0x80000000
#define	MEM4G_1	(ulong)0xffffffff	/* 4G minus 1 to fit in ulong */
#define	MEM4G	(uint64_t)0x100000000

extern char outline[64];
extern int boot_device;
static int flush_l2_flag;

void
probe(void)
{
	int	i;
	extern struct	int_pb		ic;

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Begin probe\n");
#endif
	(void) memcpy((caddr_t)physaddr(bootinfo.id), (caddr_t)0xfed00,
	    sizeof (bootinfo.id));

/*
 * Get the ega font pointer locations. This has to be collected
 * after shadow ram has been turned off.
 */
	sep->font_ptr[0] = getfontptr(0x0300);	/* 8 x 8  */
	sep->font_ptr[1] = getfontptr(0x0200);	/* 8 x 14 */
	sep->font_ptr[2] = getfontptr(0x0500);	/* 9 x 14 */
	sep->font_ptr[3] = getfontptr(0x0600);	/* 8 x 16 */
	sep->font_ptr[4] = getfontptr(0x0700);	/* 9 x 16 */

	/* get these again after shadow ram change */

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK) {
		for (i = 0; i < 5; i++)
			printf("egafont[%d] 0x%x\n", i, sep->font_ptr[i]);
	}
#endif

	(void) memset(outline, 0, sizeof (outline));
	/* Gather memory size info from CMOS and BIOS ram */
	sep->CMOSmembase = CMOSreadwd(BMLOW);
	(void) bsetprop(bop, "cmos-membase",
		sprintf(outline, "%d", sep->CMOSmembase), 0, 0);
	sep->CMOSmemext  = CMOSreadwd(EMLOW);
	(void) bsetprop(bop, "cmos-memext",
		sprintf(outline, "%d", sep->CMOSmemext), 0, 0);
	sep->CMOSmem_hi  = CMOSreadwd(EMLOW2);
	(void) bsetprop(bop, "cmos-memhi",
		sprintf(outline, "%d", sep->CMOSmem_hi), 0, 0);

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("sep->CMOSmem_hi 0x%x\n", sep->CMOSmem_hi);
#endif

	ic.intval = 0x15;
	ic.ax = 0x8800;
	(void) doint();
	sep->sys_mem = ic.ax;

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Begin machine identification.\n");
#endif

	sep->machine = MPC_UNKNOWN;
	(void) bsetprop(bop, "machine-mfg", "unknown", 0, 0);

	if (is_MC()) {	/* do Micro Channel initialization */
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK) {
			printf("micro channel bus\n");
		}
#endif
		sep->machflags |= MC_BUS;
		(void) bsetprop(bop, "bus-type", "mc", 0, 0);
		read_default_memory();
		sep->machine = MPC_MC386;
		(void) bsetprop(bop, "machine-mfg", "generic MC", 0, 0);
		/* bip->machenv.machine = M_PS2; */
	}
	/* insert other bus types here when needed...... */
	else {	/* if all else fails, must be AT type ISA or EISA */
		/* do generic ISA/EISA initialization */
		sep->machine = MPC_AT386;
		(void) bsetprop(bop, "machine-mfg", "generic AT", 0, 0);
		read_default_memory();
		/* this must come before Eisa default mem setup */

		if (is_EISA()) {
			sep->machflags |= EISA_BUS;
			(void) bsetprop(bop, "bus-type", "eisa", 0, 0);
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK) {
				printf("EISA bus\n");
			}
#endif
		} else {	/* at this point, must be ISA */
			(void) bsetprop(bop, "bus-type", "isa", 0, 0);
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK) {
				printf("ISA bus\n");
			}
#endif
		}
	}

	if (strncmp(COMPAQ_STRING_ADDR, COMPAQ_STRING,
	    strlen(COMPAQ_STRING)) == 0) {
		/*
		 * will end up in hw_provider[] in kernel, returned
		 * by sysinfo(SI_HW_PROVIDER...)
		 */
		bsetprop(bop, "si-hw-provider", "COMPAQ", 0, 0);
	}

	if (is486()) {
#ifdef BOOT_DEBUG
	    if (btep->db_flag & BOOTTALK) {
		printf("486 machine\n");
	    }
#endif
	    sep->machflags |= IS_486;
	    (void) bsetprop(bop, "cpu-type", "486", 0, 0);

	} else {		/* we don't have 586's....yet! */
	    sep->machflags = 0;
	    (void) bsetprop(bop, "cpu-type", "386", 0, 0);
	}

/*
 *	look for memory above 16 MB - remove the 16MB barrier
 *	for all machines.
 */
	for (i = 0; btep->memrng[i].extent != 0; i++)
		;

/*
 *	set the last entry to 16Mb - 4Gb
 */
	btep->memrng[i].base = 0x1000000;
	btep->memrng[i].extent = (MEM4G_1 - MEM16M) + 1;
	btep->memrng[i++].flags = B_MEM_EXPANS;
	btep->memrngcnt = i;

	if (sep->machine == MPC_AT386 || sep->machine == MPC_MC386) {
/*		btep->db_flag |= (MEMDEBUG | BOOTTALK); turn off debug mode */
		memtest();
	}

	btep->db_flag &= ~BOOTTALK;
	check_hdbios();
}


/*
 * Separate this from probe() so that the property
 * is stored in kmem.
 */
void
probe_eisa(void)
{
	if (is_EISA())
		get_eisanvm();
}

paddr_t
getfontptr(unsigned short which)
{
	extern struct	int_pb		ic;

	ic.intval = 0x10;
	ic.ax = 0x1130;
	ic.bx = which;
	ic.cx = ic.dx = ic.es = ic.bp = 0;
	if (doint()) {
		printf("doint error getting font pointers, may be trouble\n");
		return (0);
	}
	else
		return ((uint)((ic.es) << 4) + ic.bp);
}

#define	MEMTEST0	(ulong)0x00000000
#define	MEMTEST1	(ulong)0xA5A5A5A5
#define	MEMTEST2	(ulong)0x5A5A5A5A
#define	MEM16M		(ulong)0x1000000

extern  void flcache(), savcr0(), rcache(), flush_l2(ulong *);
extern  int dcache();
extern	caddr_t top_realmem;
extern	caddr_t top_bootmem;
extern	caddr_t max_bootaddr;
extern	struct	bootmem64 mem64avail[];
extern	int mem64availcnt;

void
memtest(void)
{

	struct  bootmem	*map, *mrp;
	ulong		memsrt;
	ulong		base_sum, ext_sum;
	int		extmem_clip = 0;
	int		i;
	paddr_t		base_avail = 0;		/* XXX is this right? */


/*	set page boundary for base memory				*/
	base_sum = (ulong)ptalign(sep->base_mem * 1024);

/*	set page boundary for extended memory				*/
	ext_sum = (ulong)ptalign(sep->CMOSmemext * 1024);

	if (btep->db_flag & (MEMDEBUG | BOOTTALK)) {
		printf("Memory test begin\n");
		printf("Given %d memory areas as follows:\n", btep->memrngcnt);
		for (mrp = &btep->memrng[0]; mrp->extent > 0; mrp++)
			printf("Base 0x%x size 0x%x flag 0x%x\n",
				mrp->base, mrp->extent, mrp->flags);
		printf("memsize: CMOS base %dKB, expansion %dKB,"
			" sys >1MB(max in PS2) %dKB\n",
			sep->CMOSmembase, sep->CMOSmemext,
			sep->CMOSmem_hi);
		printf("         BIOS base %dKB, system %dKB\n",
			sep->base_mem, sep->sys_mem);
		printf("sizer base_sum = %x, ext_sum = %x \n",
				base_sum, ext_sum);
		(void) goany();
	}

/*
 *	set aside the first & second avail slots for use by boot
 */
	bip->memavailcnt = 0;
	map = &bip->memavail[0];

	map->base = 0;
	map->extent = (long)ptalign(top_realmem);
	map->flags |= B_MEM_BOOTSTRAP;
	map++;
	bip->memavailcnt++;
	map->base = (paddr_t)(1 * 1024 * 1024);			/* 1M */
	map->extent = (long)top_bootmem - (long)map->base;	/* 3M */
	map->flags |= B_MEM_BOOTSTRAP;
	map++;
	bip->memavailcnt++;

	/* set default for mmu-modlist property to "mmu32"	*/
	(void) bsetprop(bop, "mmu-modlist", "mmu32", 0, 0);

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Testing for Corollary\n");
#endif
	if (CbusInitializePlatform(map,
		(unsigned long)top_bootmem,
		&bip->memavailcnt, B_MAXARGS, B_MEM_EXPANS) == TRUE)
			goto noprobe;

	/*
	 * if int 15h,e820h is supported on this system, we can skip
	 * the memory probing
	 */
	if (qsamap(map))
		goto noprobe;

	/*
	 * if int 15h,e801h is supported on this system, we can skip
	 * the memory probing
	 */
	if (int15_e801(map))
		goto noprobe;

	/* we have to do the memory sizing the old way */
	if (sep->machflags & IS_486) {
		savcr0();	/* save previous value of cr0 reg */
		(void) dcache(); /* disable and flush on-chip i486 cache, */
				/* and flush any external caches */

		if ((sep->machflags & MC_BUS) && (ibm_l2() == 1)) {
			flush_l2_flag = 1;
		}
	}

	mrp = &btep->memrng[0];
	mrp[btep->memrngcnt].extent = 0;

	for (; mrp->extent != 0; mrp++) {

		(void) memcpy(map, mrp, sizeof (struct bootmem));

		if (map->flags & B_MEM_BASE) {	/* check for base memory */
			map->base = base_avail;
			map->extent = base_sum - map->base;
		} else {	/* check for extended and shadow memory */
			memsrt = map->base;
			if (map->flags & B_MEM_TREV)
				memsrt -= NBPC;

			if (map->flags & B_MEM_SHADOW) {
				goto below;	/* sigh... for lint */
			/*
			 * For memory above 16M:-
			 * no memory scan will be performed unless
			 * there must be at least 3 pages of non-wrap memory
			 * and no clipped extended memory below
			 */
			} else if (memsrt >= MEM16M) {
				for (i = 0; i < 3; i++) {
				    if (memwrap(MEM16M+(ulong)(i*NBPC), MEM16M))
						break;
				}
				if (i < 3)
					map->extent = 0;
			} else if (map->extent > ext_sum) {
				/*
				 * For memory below 16M:-
				 * clip extended memory to CMOS recorded limit
				 */
				map->extent = ext_sum;
				extmem_clip++;
			}
below:

			/* skip if running out of extended memory */
			if (!map->extent)
				continue;

			if (memsrt >= MEM1M) {
				if (map->extent = fastmemchk(memsrt,
				    map->extent, MEM1M, map->flags)) {
					if (map->flags & B_MEM_TREV)
						map->base -= map->extent;
					/*
					 * decrement CMOS ext sum for non-shadow
					 * memory below 16M
					 */
					if ((memsrt < MEM16M) &&
					    !(map->flags & B_MEM_SHADOW))
						ext_sum -= map->extent;
				}
			} else if (map->extent = memchk(memsrt, map->extent,
			    map->flags)) {
				if (map->flags & B_MEM_TREV)
					map->base -= map->extent;
				/*
				 * decrement CMOS ext sum for non-shadow memory
				 * below 16M
				 */
				if ((memsrt < MEM16M) &&
				    !(map->flags & B_MEM_SHADOW))
					ext_sum -= map->extent;
			}
		}
		if (map->extent) {
			map++;
			bip->memavailcnt++;
		}
	}

	map->extent = 0;

	if (sep->machflags & IS_486) {
#ifdef notdef
	    if ((sep->machflags & MC_BUS) && (ibm_l2() == 1)) {
		ecache_l1l2();
	    }
#endif
	    rcache();
	}

noprobe:
	if (btep->db_flag & BOOTTALK) {
		printf("Memory test complete\n");
		printf("Found %d memory avail areas as follows:\n",
			bip->memavailcnt);
		for (map = &bip->memavail[0]; map->extent > 0; map++)
			printf("Base %x  size %x flgs 0x%x\n",
				map->base, map->extent, map->flags);
		for (i = 0; mem64avail[i].extent; i++)
			printf("Base %llx  size %llx flgs 0x%x\n",
				mem64avail[i].base, mem64avail[i].extent,
				mem64avail[i].flags);
		printf("Bootsize 0x%x\n", btep->bootsize);
		(void) goany();
	}
}

int
memchk(ulong memsrt, ulong cnt, ushort flag)
{
	int	bytecnt = 0;
	ulong	*ta;
	ulong	memsave1;
	ulong	memsave2;

	ta = (ulong *)memsrt;
	if (btep->db_flag & BOOTTALK)
		printf("Memory test starting %x %s cnt %x", ta,
			(flag & B_MEM_TREV)?"down":"up", cnt);

	for (; cnt; cnt -= NBPC) {
		memsave1 = *ta;
		memsave2 = *(ta+1);
		*ta++ = MEMTEST1;
		*ta-- = MEMTEST0;
		if (sep->machflags & IS_486)
		    flcache();		/* flush i486 on-chip cache, and */
		if (flush_l2_flag)
		    flush_l2(ta);	/* write-back any external cache */
		if (*ta == MEMTEST1) {
			*ta++ = MEMTEST2;
			*ta-- = MEMTEST0;
			if (sep->machflags & IS_486)
			    flcache();
			if (flush_l2_flag)
			    flush_l2(ta);

			if (*ta == MEMTEST2) {
				bytecnt += NBPC;
				*ta = memsave1;
				*(ta+1) = memsave2;
			} else {
				if (btep->db_flag & MEMDEBUG)
					printf(" abort 2");
				break;
			}
		} else {
			if (btep->db_flag & MEMDEBUG)
				printf(" abort 1");
			break;
		}
		if (flag & B_MEM_TREV)
			ta -= (NBPC / sizeof (long));
		else
			ta += (NBPC / sizeof (long));
	}

	if (btep->db_flag & (BOOTTALK|MEMDEBUG))
		printf(" ended at %x, area size %x\n", ta, bytecnt);

	return (bytecnt);
}


int
memwrap(ulong memsrt, ulong memoff)
{
	ulong	*ta;
	ulong	*ta_wrap;
	ulong	save_val;
	int	mystatus = 1;	/* assume memory is wrap around		*/

	ta = (ulong *)memsrt;
	ta_wrap = (ulong *)(memsrt-memoff);
	save_val = *ta;
	*ta = MEMTEST1;
	if (sep->machflags & IS_486)
	    flcache();		/* flush i486 on-chip cache, and */
	if (flush_l2_flag)
	    flush_l2(ta);	/* write-back any external cache */
	if ((*ta == MEMTEST1) && (*ta != *ta_wrap)) {
	    *ta = MEMTEST2;
	    if (sep->machflags & IS_486)
		flcache();
	    if (flush_l2_flag)
		flush_l2(ta);
	    if ((*ta == MEMTEST2) && (*ta != *ta_wrap))
		mystatus = 0;
	}
/*	restore original value whether wrap or no wrap			*/
	*ta = save_val;

#ifdef BOOT_DEBUG
	printf("memwrap: ta= 0x%x ta_wrap= 0x%x memory %s\n", ta,
		ta_wrap, (mystatus? "WRAP": "NO WRAP"));
#endif

	return (mystatus);
}

int
fastmemchk(ulong memsrt, ulong cnt, ulong step, ushort flag)
{
	int	bytecnt = 0;
	ulong   *ta;
	ulong   memsave1;
	ulong   memsave2;

	ta = (ulong *)memsrt;
	if (btep->db_flag & BOOTTALK)
		printf("Fast Memory test starting %x %s cnt %x step %x\n", ta,
			(flag & B_MEM_TREV)?"down":"up", cnt, step);

	for (; cnt != 0; cnt -= step) {
		memsave1 = *ta;
		memsave2 = *(ta+1);
		*ta++ = MEMTEST1;
		*ta-- = MEMTEST0;
		if (sep->machflags & IS_486)
			flcache();	/* flush i486 on-chip cache, and */
		if (flush_l2_flag)
			flush_l2(ta);	/* write-back any external cache */
		if (*ta == MEMTEST1) {
			*ta++ = MEMTEST2;
			*ta-- = MEMTEST0;
			if (sep->machflags & IS_486)
				flcache();
			if (flush_l2_flag)
				flush_l2(ta);

			if (*ta == MEMTEST2) {
				bytecnt += step;
				*ta = memsave1;
				*(ta+1) = memsave2;
			} else {
				if (btep->db_flag & MEMDEBUG)
					printf(" abort 2");
				break;
			}
		} else {
			if (btep->db_flag & MEMDEBUG)
				printf(" abort 1");
			break;
		}
		if (flag & B_MEM_TREV)
			ta -= (step / sizeof (long));
		else
			ta += (step / sizeof (long));
	}
	/*
	 * make sure the last 4 Mb are full
	 */
	if (bytecnt) {
		if (flag & B_MEM_TREV)
			ta += (step / sizeof (long));
		else
			ta -= (step /sizeof (long));
		bytecnt -= step;
		bytecnt += memchk((ulong)ta, step, flag);
	}

	if (btep->db_flag & (BOOTTALK|MEMDEBUG))
		printf("Return area size %x\n", bytecnt);

	return (bytecnt);
}

void
read_default_memory(void)
{
	int i;

	btep->memrngcnt = 1;

/*
	btep->memrng[0].base = 0;
	btep->memrng[0].extent = 640 * 1024;
	btep->memrng[0].flags = B_MEM_BASE;
*/

	btep->memrng[0].base = 0x400000;
	btep->memrng[0].extent = (12 * 1024 * 1024);
	btep->memrng[0].flags = B_MEM_EXPANS;

	btep->memrng[1].base = 0;
	btep->memrng[1].extent = 0;
	btep->memrng[1].flags = 0;

	btep->memrng[2].base = 0;
	btep->memrng[2].extent = 0;
	btep->memrng[2].flags = 0;

	btep->memrng[3].base = 0;
	btep->memrng[3].extent = 0;
	btep->memrng[3].flags = 0;

	btep->bootsize = btop((u_int)max_bootaddr);

	/* ===================================== */

	bip->memavailcnt = 0;

	bip->memavail[0].base = 0;
	bip->memavail[0].extent = 0;
	bip->memavail[0].flags = B_MEM_BASE;

	bip->memavail[1].base = 0;
	bip->memavail[1].extent = 0;
	bip->memavail[1].flags = B_MEM_EXPANS;

	for (i = 2; i < B_MAXARGS; i++) {
		bip->memavail[i].base = 0;
		bip->memavail[i].extent = 0;
		bip->memavail[i].flags = 0;
	}

	for (i = 0; i < B_MAXMEM64; i++) {
		mem64avail[i].base = 0;
		mem64avail[i].extent = 0;
		mem64avail[i].flags = 0;
	}
}

#if 0					/* vla fornow..... */
shomem(used, idm, cnt, bmp)
char *idm;
int  cnt, used;
struct bootmem *bmp;
{
	int i;

	printf("%s %d\n", idm, cnt);
	for (i = 0; i < cnt; i++, bmp++) {
		printf("%d %x %x %x", i, bmp->base, bmp->extent, bmp->flags);
		if (used)
			printf(" %x\n", bootenv.sectaddr[i]);
		else
			printf("\n");
	}
	(void) goany();
}
#endif					/* vla fornow..... */

#if defined(lint)
/*
 * inline functions defined here for lint's sake.
 */

/*ARGSUSED*/
u_char	inb(int port) { return (*(u_char *)port); }
void	outb(int port, u_char v) { *(u_char *)port = v; }

#endif /* defined(lint) */

void
get_eisanvm(void)
{
	extern struct	int_pb		ic;
	int	i, j;
	int	status;
	struct	es_slot		slotbuf;
	struct	es_slot		*es_slotp;
	struct	es_func		*es_funcp;
	int	number_of_functions;
	int	memory_needed;
	char	*nvm_data;

	sep->machflags |= EISA_NVM_DEF;
	/*
	 * First see how much memory we need.
	 */
	memory_needed = 0;
	for (i = 0; i < EISA_MAXSLOT; i++) {
		ic.intval = 0x15;
		ic.ax = 0xd800;
		ic.cx = (ushort)(i & 0xffff);
		ic.bx = ic.dx = ic.si = ic.di = ic.bp = ic.es = 0;
		status = doint();
		slotbuf.es_slotinfo.eax.word.ax = ic.ax;
		slotbuf.es_slotinfo.edx.word.dx = ic.dx;
		memory_needed += sizeof (struct es_slot);
		if (slotbuf.es_slotinfo.eax.byte.ah != 0)	/* error */
			continue;
		number_of_functions = slotbuf.es_slotinfo.edx.byte.dh;
		memory_needed += number_of_functions * sizeof (struct es_func);
	}
	/*
	 * make sure we have enough kmem space
	 */
	if (!(nvm_data = rm_malloc(memory_needed, 0, 0))) {
		printf("No memory for EISA NVRAM property\n");
		return;
	}

	es_slotp = (struct es_slot *)nvm_data;
	if (btep->db_flag & BOOTTALK) {
		printf("get_eisanvm: es_slotp= 0x%x\n", es_slotp);
	}

	for (i = 0; i < EISA_MAXSLOT; i++, es_slotp++) {
		ic.intval = 0x15;
		ic.ax = 0xd800;
		ic.cx = (ushort)(i & 0xffff);
		ic.bx = ic.dx = ic.si = ic.di = ic.bp = ic.es = 0;
		status = doint();
		es_slotp->es_slotinfo.eax.word.ax = ic.ax;
		es_slotp->es_slotinfo.ebx.word.bx = ic.bx;
		es_slotp->es_slotinfo.ecx.word.cx = ic.cx;
		es_slotp->es_slotinfo.edx.word.dx = ic.dx;
		es_slotp->es_slotinfo.esi.word.si = ic.si;
		es_slotp->es_slotinfo.edi.word.di = ic.di;
		es_slotp->es_funcoffset = 0;

		if (btep->db_flag & BOOTTALK) {
			if (!status)
				if (es_slotp->es_slotinfo.eax.byte.ah)
				    printf("get_eisanvm: cflg = 0 ah = 0x%x\n",
					es_slotp->es_slotinfo.eax.byte.ah);
			if (status) {
				if (!es_slotp->es_slotinfo.eax.byte.ah)
				    printf("get_eisanvm: cflg != 0 ah == 0\n");
				else
				    printf("get_eisanvm:slot[%d] ah= 0x%x\n",
					i, es_slotp->es_slotinfo.eax.byte.ah);
			}
		}
	}

	es_funcp = (struct es_func *)es_slotp;
	es_slotp = (struct es_slot *)nvm_data;
	for (i = 0; i < EISA_MAXSLOT; i++, es_slotp++) {
		if (!es_slotp->es_slotinfo.eax.byte.ah) {
			es_slotp->es_funcoffset = (int)((char *)es_funcp -
								nvm_data);
			if (btep->db_flag & BOOTTALK) {
				printf("eisanvm: slot[%d] func addr= 0x%x"
					" cnt= %d\n",
				i, es_funcp, es_slotp->es_slotinfo.edx.byte.dh);
			}
			for (j = 0; j < es_slotp->es_slotinfo.edx.byte.dh;
			    j++) {
				ic.intval = 0x15;
				ic.bx = ic.dx = ic.di = ic.bp = ic.es = 0;
				ic.ax = 0xd801;
				ic.cx = (ushort)(((j&0xff)<<8)|(i & 0xff));
				ic.ds = ((paddr_t)(es_funcp->ef_buf)&0xffff0) >>
					4;
				ic.si = (paddr_t)(es_funcp->ef_buf) & 0xf;
				status = doint();
				es_funcp->eax.word.ax = ic.ax;
				es_funcp++;
			}
		}
	}
	(void) bsetprop(bop, "eisa-nvram", nvm_data, memory_needed, 0);
	rm_free(nvm_data, memory_needed);
}

paddr_t
get_fontptr(void)
{
	return ((paddr_t)sep->font_ptr);
}

#define	IBM_MCAR	0xe0	/* IBM Memory Controller Address reg port */
#define	IBM_CCR		0xe2	/* IBM Cache Controller Register */
#define	IBM_CSR		0xe3	/* IBM Cache Status Register */
#define	IBM_MCDR	0xe4	/* IBM Memory Controller Data register port */


#ifdef notdef
/* disable and flush L1 and L2 caches */
void
dcache_l1l2(void)
{
	register unchar regtmp;

	outb(IBM_MCAR, 0xa2);	/* point to the Cache/Timer control reg */
	regtmp = inb(IBM_MCDR);	/* read Cache/Timer control reg */
	outb(IBM_MCAR, 0xa2);	/* point to the cache/Timer control reg */
	outb(IBM_MCDR, regtmp | 0x01);	/* Disable L1 and L2 caches */

	regtmp = inb(IBM_CCR);		/* read cache control register */
	outb(IBM_CCR, regtmp | 0x80);   /* enable L1 cache */
	regtmp = inb(IBM_CCR);		/* read cache control register */
	outb(IBM_CCR, regtmp & 0x3f);   /* disable and flush L1 and L2 caches */
}


/* Enable L1 and L2 caches */
void
ecache_l1l2(void)
{
	register unchar regtmp;

	outb(IBM_MCAR, 0xa2);	/* point to the Cache/Timer control reg */
	regtmp = inb(IBM_MCDR);	/* read Cache/Timer control reg */
	outb(IBM_MCAR, 0xa2);	/* point to the cache/Timer control reg */
	outb(IBM_MCDR, regtmp & 0xfe);	/* Enable L1 and L2 caches */

	regtmp = (inb(IBM_CCR) | 0xc0); /* read cache control register and */
	regtmp &= 0xdf;			/* set/reset desired bits */
	outb(IBM_CCR, regtmp);		/* enable L1 & L2 caches */
}
#endif /* notdef */


/* return 1 if L2 cache exist, return zero if not */
int
ibm_l2(void)
{
	unchar regtmp;

	regtmp = inb(IBM_CSR);  /* read cache status register */

	if ((regtmp & 0x30) != 0)
		return (0);
	else
		return (1);
}

/*
 * use Bios call int 15h function E801h to get memory ranges
 * success - return 1, fail - return 0
 *
 * int 15h E801h - Big Memory Size, 16 bit
 * Entry:
 * 	AH = E8h
 *	AL = 01h
 * Exit:
 *	Carry	0 = E801 Supported
 *	AX	Memory 1 Mb to 16Mb, in 1Kb blocks
 *	BX	Memory above 16 Mb, in 64 Kb blocks
 *	CX	Configured memory 1 Mb to 16Mb, in 1Kb blocks
 *	DX	Configured memory above 16 Mb, in 64 Kb blocks
 */
int
int15_e801(map)
struct  bootmem	*map;
{
	extern struct	int_pb	ic;

#ifdef BOOT_DEBUG
	if (btep->db_flag & (BOOTTALK | MEMDEBUG))
		printf("Enter int15_e801\n");
#endif
	ic.intval = 0x15;
	ic.ax = 0xe801;
	ic.bx = ic.cx = ic.dx = ic.es = ic.bp = 0;
	if (doint()) {
#ifdef BOOT_DEBUG
		if (btep->db_flag & (BOOTTALK | MEMDEBUG))
			printf("int15_e801: invalid return\n");
#endif
		return (0);
	}
#ifdef BOOT_DEBUG
	if (btep->db_flag & (BOOTTALK | MEMDEBUG))
		printf("int15_e801: ax=0x%x bx=0x%x cx=0x%x dx=0x%x\n", ic.ax,
			ic.bx, ic.cx, ic.dx);
#endif
	/*
	 * 1M - top_bootmem is already on map, so break the entry and
	 * create one start at top_bootmem
	 */
	if ((ic.cx * 1024) > ((ulong) top_bootmem - MEM1M)) {
		map->base = (ulong) top_bootmem;
		map->extent = (ulong) (ic.cx * 1024 -
					((ulong)top_bootmem - MEM1M));
		map->flags = B_MEM_EXPANS;
		bip->memavailcnt++;
		/*
		 * if there isn't any gap below 16MB, then just combine
		 * cx and dx together
		 */
		if ((map->base + map->extent) == MEM16M) {
			map->extent += (ulong) (ic.dx * 64 * 1024);
#ifdef BOOT_DEBUG
			if (btep->db_flag & (BOOTTALK | MEMDEBUG))
				printf("map: base=0x%x extent=0x%x\n",
						map->base, map->extent);
#endif
			map++;
			map->extent = 0;
			return (1);
		}
#ifdef BOOT_DEBUG
		if (btep->db_flag & (BOOTTALK | MEMDEBUG))
			printf("map: base=0x%x extent=0x%x\n", map->base,
				map->extent);
#endif
		map++;
	}
	if (ic.dx) {
		map->base = MEM16M;
		map->extent = (ulong) (ic.dx * 64 * 1024);
		map->flags = B_MEM_EXPANS;
#ifdef BOOT_DEBUG
		if (btep->db_flag & (BOOTTALK | MEMDEBUG))
			printf("map: base=0x%x extent=0x%x\n", map->base,
				map->extent);
#endif
		bip->memavailcnt++;
		map++;
	}
	map->extent = 0;
	return (1);
}

/*
 * use Bios call int 15h function E820h to get memory ranges
 * success - return 1, fail - return 0
 *
 * int 15h E820h - System Memory Map, 32 bit
 * Entry:
 *	EBX	Continuation value
 *	ES:DI	Address of Address Range Descriptor
 *	ECX	Length of Address Range Descriptor (=> 20 bytes)
 *	EDX	"SMAP" signature
 * Exit:
 *	Carry	0 = E820 Supported
 *	EAX	"SMAP" signature
 *	ES:DI	Same value as entry
 *	ECX	Length of actual reported information in bytes
 *	EBX	Continuation value
 *
 *		Structure of Address Range Descriptor:
 *		Bytes 0-3	Low 32 bits of Base Address
 *		Bytes 4-7	High 32 bits of Base Address
 *		Bytes 8-11	Low 32 bits of Length in bytes
 *		Bytes 12-15	High 32 bits of Length in bytes
 *		Bytes 16-20	Type of Address Range:
 *				1 = AddressRangeMemory, available to OS
 *				2 = AddressRangeReserved, not available
 *				Other = Not defined, not available
 */
int
qsamap(map)
struct  bootmem	*map;
{
#define	SMAP_SIG		0x534d4150
#define	SMAP_MIN_BUF_SIZE	20
#define	ADDR_RANGE_MEMORY	1
#define	ADDR_RANGE_RESERVED	2

	extern int	doint15_32bit();
	extern struct	int_pb32 ic32;
	struct addr_rng_desc {
		uint64_t	baseaddr;
		uint64_t	len;
		int		type;
	};
	struct addr_rng_desc addr_rng;
	struct bootmem *mapsave;
	struct bootmem64 *map64;
	int mapcntsave;
	uint64_t base64, len64;
	uint base32, len32;
#ifdef	BOOT_DEBUG
	uint	*pp;
#endif

#ifdef BOOT_DEBUG
	if (btep->db_flag & (BOOTTALK | MEMDEBUG)) {
		printf("Enter QSAMAP\n");
		printf("top_realmem=0x%x max_bootaddr=0x%x top_bootmem=0x%x\n",
			top_realmem, max_bootaddr, top_bootmem);
	}
#endif
	ic32.ebx = 0;
	map64 = &mem64avail[0];
	mapsave = map;
	mapcntsave = bip->memavailcnt;

	do {
		ic32.eax = 0xe820;
		ic32.ecx = SMAP_MIN_BUF_SIZE;
		ic32.edx = SMAP_SIG;
		ic32.es = (ushort) (((uint) &addr_rng) >> 4);
		ic32.edi = ((uint) &addr_rng) & 0xF;

		if (doint15_32bit() || (ic32.eax != SMAP_SIG) ||
			(ic32.ecx != SMAP_MIN_BUF_SIZE)) {
#ifdef BOOT_DEBUG
			if (btep->db_flag & (BOOTTALK | MEMDEBUG))
				printf("int15_e820(): Invalid return\n");
#endif
			mapsave->extent = 0;
			bip->memavailcnt = mapcntsave;
			mem64avail[0].extent = 0;
			mem64availcnt = 0;
			return (0);
		}

#ifdef BOOT_DEBUG
		if (btep->db_flag & (BOOTTALK | MEMDEBUG)) {
			printf("int15_e820h: ");
			if (addr_rng.type == ADDR_RANGE_MEMORY)
				printf("Memory- ");
			else printf("Reserved- ");
			pp = (uint *) &addr_rng;
			printf("eax=0x%lx %ebx=0x%lx base=%lx:%lx "
				"len=%lx:%lx\n", ic32.eax, ic32.ebx,
				*(pp+1), *(pp), *(pp+3), *(pp+2));
		}
#endif

		if (addr_rng.type == ADDR_RANGE_MEMORY) {
			if (validate_entry(addr_rng.baseaddr, addr_rng.len,
				&base32, &len32, &base64, &len64) == 0) {
				mapsave->extent = 0;
				bip->memavailcnt = mapcntsave;
				mem64avail[0].extent = 0;
				mem64availcnt = 0;
				return (0);
			}
			if (len32) {
				if (bip->memavailcnt < B_MAXARGS) {
					map->base = base32;
					map->extent = len32;
					map->flags = B_MEM_EXPANS;
#ifdef BOOT_DEBUG
					if (btep->db_flag &
						(BOOTTALK | MEMDEBUG))
						printf("map: base=0x%lx "
							"extent=0x%lx\n",
							map->base, map->extent);
#endif
					map++;
					bip->memavailcnt++;
				} else
					printf("WARNING: out of bootmem entry "
						"for memory below 4Gb\n");
			}
			if (len64) {
				if (mem64availcnt < B_MAXMEM64) {
					map64->base = base64;
					map64->extent = len64;
					map64->flags = B_MEM_EXPANS;
#ifdef BOOT_DEBUG
					if (btep->db_flag &
						(BOOTTALK | MEMDEBUG)) {
						pp = (uint *) &map64->base;
						printf("map64: base=0x%lx:%lx "
							"extent=0x%lx:%lx\n",
							*(pp+1), *pp, *(pp+3),
							*(pp+2));
					}
#endif
					map64++;
					mem64availcnt++;
				} else
					printf("WARNING: out of boot64mem "
						"entry for memory above 4Gb\n");
			}
		}
	} while (ic32.ebx != 0);

#ifdef BOOT_DEBUG
	if (btep->db_flag & (BOOTTALK | MEMDEBUG))
		goany();
#endif

	if (mem64availcnt > 0) {
		extern int GenuineIntel();
		extern int has_cpuid();
		extern int pae_supported();

		/*
		 * Set mmu-modlist to mmu36 only if PAE and cmpxchg8b are
		 * supported.
		 */
		if (has_cpuid() && GenuineIntel() && pae_supported()) {
			(void) bsetprop(bop, "mmu-modlist", "mmu36", 0, 0);
		}
		/*
		 * Just in case users may select another mmu module that
		 * can handle this, memory lists above 4Gb are still passed
		 * to the kernel.
		 * By default, when mmu32 gets control, it will truncate all
		 * the memory above 4Gb and print out a warning message too.
		 */
	}

	return (1);	/* successful return */
}

/*
 * validate memory entry returned by int 15h function E820h with the ones
 * already on maps
 * success - return 1, fail - return 0
 */
int
validate_entry(uint64_t in_base, uint64_t in_len, uint *ret_base32p,
	uint *ret_len32p, uint64_t *ret_base64p, uint64_t *ret_len64p)
{
	int i;
	uint base32, len32;
	uint64_t base64, len64, begin, end, tail;

	if (in_base < MEM1M) {
		if ((in_base + in_len) <= MEM1M) {
			/* already on map */
			*ret_base32p = *ret_len32p = (uint) 0;
			*ret_base64p = *ret_len64p = (uint64_t) 0;
			return (1);
		}
		/* shouldn't happen, but just in case */
		in_len -= (MEM1M - in_base);
		in_base = MEM1M;
	}

	base32 = len32 = (uint) 0;
	base64 = len64 = (uint64_t) 0;

	if (in_base >= MEM4G) {
		base64 = in_base;
		len64 = in_len;
	} else if (in_base + in_len > MEM4G) {
		base32 = in_base;
		len32 = MEM4G - in_base;
		base64 = MEM4G;
		len64 = in_len - len32;
	} else {
		base32 = in_base;
		len32 = in_len;
	}

	if (len32) {
		/*
		 * memavail[0] points to memory belows 1M, since this
		 * has been checked above, it can be skipped here
		 */
		tail = (uint64_t) base32 + (uint64_t) len32;
		for (i = 1; i < bip->memavailcnt; i++) {
			begin = (uint64_t) bip->memavail[i].base;
			end = begin + (uint64_t) bip->memavail[i].extent;
			/*
			 * since a version of BIOS has been found
			 * returning duplicate entries, the following
			 * checks are added to avoid the problem
			 * also, this should take care the case that
			 * top_bootmem is already on map
			 */
			if (base32 >= begin && base32 < end) {
				if ((end - base32) >= len32) {
					base32 = len32 = 0;
					break;
				} else {
					len32 -= (uint) (end - base32);
					base32 = (uint) end;
				}
				tail = (uint64_t) base32 + (uint64_t) len32;
			} else if (tail > begin && tail <= end) {
				len32 -= (uint) (tail - begin);
				tail = (uint64_t) base32 + (uint64_t) len32;
			} else if (base32 < begin && tail > end) {
				/*
				 * just not worthwhile to handle this bios
				 * error, flag an error and return
				 * it will use other methods to size the memory
				 */
				printf("WARNING: int 15h E820h returned "
					"entries overlapping each other\n");
				return (0);
			}
		}
	}
	if (len64) {
		tail = base64 + len64;
		for (i = 0; i < mem64availcnt; i++) {
			begin = mem64avail[i].base;
			end = begin + mem64avail[i].extent;
			if (base64 >= begin && base64 < end) {
				if ((end - base64) >= len64) {
					base64 = len64 = 0;
					break;
				} else {
					len64 -= (end - base64);
					base64 = end;
				}
				tail = base64 + len64;
			} else if (tail > begin && tail <= end) {
				len64 -= (tail - begin);
				tail = base64 + len64;
			} else if (base64 < begin && tail > end) {
				/*
				 * just not worthwhile to handle this bios
				 * error, flag an error and return
				 */
				printf("int15 e820h returned duplicate "
					"entries\n");
				return (0);
			}
		}
	}
	*ret_base32p = base32;
	*ret_len32p = len32;
	*ret_base64p = base64;
	*ret_len64p = len64;
	return (1);
}
