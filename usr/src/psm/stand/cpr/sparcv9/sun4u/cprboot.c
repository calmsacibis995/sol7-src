/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprboot.c	1.29	98/01/13 SMI"

/*
 * cprboot - Second pass of the resume bootstrapping process,
 * it is responsible for:
 *	1. cpr statefile /.CPR
 *	2. Read various headers and necessary information from the statefile
 *	3. Allocate enough virtual memory for the read kernel pages operation
 *	4. Read in kernel pages to their physical location
 *	5. Call cpr kernel entry procedure
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/pte.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_isa.h>	/* for dnode_t */
#include <sys/prom_plat.h>
#include <sys/stack.h>
#include <sys/privregs.h>

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/*
 * External procedure declarations
 */
extern uint_t cpr_decompress(uchar_t *, uint_t, uchar_t *);
extern void cpr_dtlb_wr_entry(int, caddr_t, int, tte_t *);
extern void exit_to_kernel(void *, csu_md_t *);
extern void cpr_itlb_wr_entry(int, caddr_t, int, tte_t *);
extern int cpr_open(char *, char *);
extern int cpr_statefile_open(char *, char *);
extern int cpr_statefile_close(int);
extern int cpr_read(int, caddr_t, size_t, u_int *);
extern void cpr_reset_read(void);

extern void cprboot_spinning_bar();
static int cpr_skip_bitmaps(int);

/*
 * Global variables
 */
static int totpages;
static u_int mapva, free_va;	/* temp virtual addresses used for mapping */
static cbd_t bitmap_desc;
static int compressed, no_compress;
static int good_pg;
static long machdep_len;	/* total length of machdep info data */
static int spin_cnt = 0;
static csu_md_t m_info;

int cpr_debug;

caddr_t    starttext, endtext, startdata, enddata;
caddr_t    newstack;

/*
 * Following is for freeing our memory
 */
extern	void _start();
extern	char _etext[];
extern	char _end[];		/* cprboot end address */
static	char dummy = 0;		/* dummy to find start of data segment */

#define	_sdata ((caddr_t)&dummy)


static int cprboot_cleanup_setup(caddr_t);
static void cpr_export_prom_words(caddr_t, size_t);


/*
 * cprboot pass in statefile path
 * A "" filesystem means that pathname points to a character special
 * device, (this is also the convention understood by cpr_open)
 * we don't do anything different here for the reusable statefile
 * case, so nobody bothers to tell us about it
 */
int
main(void *cookie, char *pathname, char *filesystem, caddr_t loadbase)
{
	struct cpr_terminator ct_tmp;	/* cprboot version of terminator */
	int n_read = 0, rpg;
	int fd, compress, cnt;
	caddr_t machdep_buf;
	uint16_t wst32, wst64;
	struct sun4u_tlb *utp;
	char *fmt, *str;
	cdd_t cdump;		/* cpr verison of dump descriptor */

	prom_init("cprboot", cookie);

	DEBUG4(errp("\nstart 0x%x, 0x%x etext 0x%x data 0x%x end 0x%x\n",
		(caddr_t)_start, loadbase, _etext, _sdata, _end));

	ct_tmp.tm_cprboot_start.tv_sec = prom_gettime() / 1000;

	if ((fd = cpr_statefile_open(pathname, filesystem)) == -1) {
		if (filesystem[0] != '\0')
			errp("Can't open %s on %s, please reboot\n",
			    pathname, filesystem);
		else
			errp("Can't open %s, please reboot\n", pathname);
		return (-1);
	}

	/*
	 * Devices get left at block 1
	 */
	if (filesystem[0] == '\0')
		(void) prom_seek(fd, 0LL);

	if (cpr_read_cdump(fd, &cdump, CPR_MACHTYPE_4U))
		return (-1);
	totpages = cdump.cdd_dumppgsize;

	if ((machdep_len = cpr_get_machdep_len(fd)) == -1)
		return (-1);

	if ((machdep_buf =
	    prom_alloc((caddr_t)0, (size_t)machdep_len, 0)) == (caddr_t)0) {
		errp("Can't alloc machdep buffer\n");
		return (-1);
	}

	if (cpr_read_machdep(fd, machdep_buf, machdep_len) == -1)
		return (-1);
	bcopy(machdep_buf, &m_info, sizeof (m_info));

	/*
	 * check for valid stack bias and wstate
	 */
	fmt = "found bad statefile data: %s (0x%x), expect 0x%x or 0x%x\n";
	if (m_info.ksb != 0x0 && m_info.ksb != V9BIAS64) {
		errp(fmt, "stack bias", m_info.ksb, 0, V9BIAS64);
		return (-1);
	}
	wst32 = WSTATE(WSTATE_U32, WSTATE_K32);
	wst64 = WSTATE(WSTATE_U32, WSTATE_K64);
	if (m_info.kwstate != wst32 && m_info.kwstate != wst64) {
		errp(fmt, "wstate", m_info.kwstate, wst32, wst64);
		return (-1);
	}

	if (cpr_skip_bitmaps(fd) != 0)
		return (-1);

	/*
	 * Reserve prom memory to do mapping operations
	 */
	if ((mapva = (u_int)prom_map(0, 0, MMU_PAGESIZE)) == 0) {
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	if ((free_va = (u_int)prom_map(0, 0,
		(CPR_MAXCONTIG * MMU_PAGESIZE))) == 0) {
		errp("prom_map operation failed, please reboot\n");
		return (-1);
	}

	/*
	 * Read in pages
	 */
	str = "cprboot:";
	DEBUG1(errp("%s reading pages... ", str));
	while (good_pg < totpages) {
		if ((rpg = cpr_read_phys_page(fd, free_va, &compress)) != -1) {
			n_read++;
			good_pg += rpg;
			if (compress)
				compressed += rpg;
			else
				no_compress += rpg;
			spin_cnt++;
			if ((spin_cnt & 0x23) == 1)
				cprboot_spinning_bar();
			DEBUG4(errp("cpr_read_phys_page: read %d pages\n",
				rpg));
		} else {
			errp("read phy page error: read=%d good=%d total=%d\n",
				n_read, good_pg, totpages);
			errp("\n Please reboot\n");
			return (-1);
		}
	}
	DEBUG1(errp(" \b\n"));

	DEBUG4(errp("Read=%d totpages=%d no_compress=%d compress=%d\n",
		good_pg, totpages, no_compress, compressed));

	if (cpr_read_terminator(fd, &ct_tmp, mapva))
		return (-1);

	(void) cpr_statefile_close(fd);

	/*
	 * Map the kernel entry page
	 */
	if (prom_map((caddr_t)(m_info.func & MMU_PAGEMASK),
	    PN_TO_ADDR(m_info.func_pfn), MMU_PAGESIZE) == NULL) {
		errp("failed to map resume kernel page, please reboot\n");
		return (-1);
	}


	for (cnt = 0, utp = m_info.dtte; cnt < CPR_MAX_TLB; cnt++, utp++) {
		u_longlong_t pa;
		caddr_t va;
		u_int size;

#define	PFN_TO_ADDR(x)	((u_longlong_t)(x) << MMU_PAGESHIFT)

		if (utp->va_tag == NULL)
			continue;
		va = (caddr_t)utp->va_tag;
		pa = PFN_TO_ADDR(TTE_TO_TTEPFN(&utp->tte));
		size = TTEBYTES(utp->tte.tte_size);
		if (prom_map_phys(-1, size, va, pa) != 0)
			errp("Unable to prom map 0x%x 0x%x\n", va, size);
	}

	fmt = "va_tag 0x%p ctx 0x%x tte 0x%x.%x\n";
	for (cnt = 0, utp = m_info.dtte; cnt < CPR_MAX_TLB; cnt++, utp++) {
		if (utp->va_tag == NULL)
			continue;
		DEBUG4(errp("\ndtte offset %d, dtlb entry %d:\n",
		    cnt, utp->no));
		DEBUG4(errp(fmt, utp->va_tag, utp->ctx,
		    utp->tte.tte_inthi, utp->tte.tte_intlo));
		cpr_dtlb_wr_entry(utp->no, (caddr_t)utp->va_tag,
		    utp->ctx, &utp->tte);
	}

	for (cnt = 0, utp = m_info.itte; cnt < CPR_MAX_TLB; cnt++, utp++) {
		if (utp->va_tag == NULL)
			continue;
		DEBUG4(errp("\nitte offset %d, itlb entry %d:\n",
		    cnt, utp->no));
		DEBUG4(errp(fmt, utp->va_tag, utp->ctx,
		    utp->tte.tte_inthi, utp->tte.tte_intlo));
		cpr_itlb_wr_entry(utp->no, (caddr_t)utp->va_tag,
		    utp->ctx, &utp->tte);
	}

	/*
	 * Map curthread
	 */
	fmt = "prom_map operation failed, please reboot\n";
	if (prom_map((caddr_t)(m_info.thrp & MMU_PAGEMASK),
	    PN_TO_ADDR(m_info.thrp_pfn), MMU_PAGESIZE) == NULL) {
		DEBUG4(errp("failed to map cpr_thread\n"));
		errp(fmt);
		return (-1);
	}

	cpr_export_prom_words(machdep_buf, machdep_len);
	prom_free(machdep_buf, machdep_len);

	DEBUG4(errp(
	    "resume func 0x%lx pfn 0x%lx tp 0x%lx pctx 0x%lx sctx 0x%lx ",
	    m_info.func, m_info.func_pfn, m_info.thrp,
	    m_info.mmu_ctx_pri, m_info.mmu_ctx_sec));

	DEBUG4(errp("mapbuf_va 0x%lx mapbuf_pfn 0x%lx mapbuf_size 0x%lx\n",
	    m_info.mapbuf_va, m_info.mapbuf_pfn, m_info.mapbuf_size));

	/* must do before reading the prom mappings */
	if (cprboot_cleanup_setup(loadbase) == 0)
		return (-1);

	if (prom_map((caddr_t)m_info.mapbuf_va, PN_TO_ADDR(m_info.mapbuf_pfn),
	    m_info.mapbuf_size) == NULL) {
		errp("failed to map translation buffer, please reboot\n");
		return ((size_t)-1);
	}

	DEBUG5(errp("starttext 0x%p, endtext 0x%p, size 0x%p\n",
	    starttext, endtext, (endtext - starttext)));
	DEBUG5(errp("startdata 0x%p, enddata 0x%p, size 0x%p\n",
	    startdata, enddata, (enddata - startdata)));
	DEBUG1(errp("%s resume pc 0x%p\n", str, m_info.func));
	DEBUG1(errp("%s exit_to_kernel(0x%p, 0x%p)\n", str, cookie, &m_info));

	/*
	 * call cpr kernel entry handler
	 */
	exit_to_kernel(cookie, &m_info);

	return (0);
}


static int
cprboot_cleanup_setup(caddr_t loadbase)
{
	caddr_t	sspace;
	size_t size;

	size = MMU_PAGESIZE;
	sspace = prom_alloc((caddr_t)0, size, MMU_PAGESIZE);
	DEBUG5(errp("\ncprboot: allocated newstack 0x%p, bytes 0x%x\n",
	    sspace, size));
	if (sspace == (caddr_t)0) {
		errp("stack space alloc error, please reboot\n");
		return (0);
	}

	if (loadbase == (caddr_t)0)
		loadbase = (caddr_t)_start;

	starttext = (caddr_t)((uintptr_t)loadbase & MMU_PAGEMASK);
	endtext = (caddr_t)PAGE_ROUNDUP((uintptr_t)_etext);

	startdata = (caddr_t)((uintptr_t)_sdata & MMU_PAGEMASK);
	enddata = (caddr_t)PAGE_ROUNDUP((uintptr_t)_end);

	newstack = sspace + size;

	return (1);
}


/*
 * For sun4u, I don't think we need the bitmap anymore,
 * since we have claimed all the memory that we need already.
 * Instead of reading the bitmaps, we just do seek to skip them.
 * No more prom_alloc() need to be done for the actual bitmaps.
 */
static int
cpr_skip_bitmaps(int fd)
{
	u_longlong_t offset;
	cbd_t *dp;

	/*
	 * file offset from start of file
	 * If there is machdep info, add size of machdep info to offset.
	 */
	offset = sizeof (cdd_t) + sizeof (cmd_t) + machdep_len;

	/*
	 * Even though we've been doing buffered reads so far, we're going
	 * to drop directly to callin the prom so we don't do 32K reads
	 * each time.
	 */
	if (prom_seek(fd, offset) == -1) {
		errp("cpr_skip_bitmaps: bitmap seek error\n");
		return (-1);
	}

	dp = &bitmap_desc;
	if (prom_read(fd, (caddr_t)dp, sizeof (*dp), 0, 0) < sizeof (*dp)) {
		errp("cpr_skip_bitmap: err reading bitmap desc\n");
		return (-1);
	}

	if (dp->cbd_magic != CPR_BITMAP_MAGIC) {
		errp("cpr_skip_bitmap: bitmap BAD MAGIC 0x%x\n",
		    dp->cbd_magic);
		return (-1);
	}

	offset += sizeof (cbd_t) + (dp->cbd_size * 2);
	DEBUG4(errp("cpr_skip_bitmaps: seek to offset %llu\n", offset));
	if (prom_seek(fd, offset) == -1) {
		errp("cpr_skip_bitmaps: bitmap seek error\n");
		return (-1);
	}

	/*
	 * Reset cpr_read's buffer so it will do another read
	 * from the current postion.
	 */
	cpr_reset_read();

	return (0);
}

/*
 * During early startup of a normal (non-cpr) boot, the kernel uses
 * prom_interpret() in several places to define some Forth words to the
 * prom.  We must duplicate those definitions in this standalone boot
 * program before taking the snapshot of the prom's translations.  This
 * can't be done in the resumed kernel because it might change the
 * prom's mappings.
 */
static void
cpr_export_prom_words(caddr_t buf, size_t length)
{
	size_t str_len;

	buf += sizeof (csu_md_t);
	length -= sizeof (csu_md_t);

	/*
	 * The variable length machdep section for sun4u consists
	 * of a sequence of null-terminated strings stored contiguously.
	 *
	 * The first string defines Forth words which help the prom
	 * handle kernel translations.
	 *
	 * The second string defines Forth words required by kadb to
	 * interface with the prom when a trap is taken.
	 */
	while (length) {
		prom_interpret(buf, 0, 0, 0, 0, 0);
		str_len = strlen(buf) + 1;	/* include the null */
		length -= str_len;
		buf += str_len;
	}
}
