/*
 * Copyright (c) 1987 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)support.c	1.38	98/01/13 SMI"

/* from SunOS 4.1 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/bootconf.h>
#include <sys/debug/debugger.h>
#include <sys/sysmacros.h>
#include <sys/openprom.h>
#include <sys/prom_plat.h>
#include <sys/fcntl.h>
#include <sys/cpr.h>

extern uint_t	cpr_decompress(uchar_t *, uint_t, uchar_t *);
extern char 	*strcat(char *, const char *);
extern void	prom_unmap(caddr_t, u_int);
extern int	cpr_ufs_open(char *, char *);
extern int	cpr_ufs_read(int, char *, int);
extern int	cpr_ufs_close(int);
extern int	cpr_statefile_read(int, caddr_t, int);

#ifdef sparc
extern struct bootops *bootops;
extern int pagesize;
#endif

extern int cpr_debug;

#define	MAX_READ (32 * 1024)	/* prom_read() is reading 32k at a time */
static u_char	read_buf[64 * 1024];	/* Might not need that much */
static u_char *read_ptr, *fptr;
static u_char *e_buf_addr = read_buf + sizeof (read_buf) - 1;
static size_t contains = 0;	/* bytes in the buffer */
static const struct prop_info cpr_prop_info[] = CPR_PROPINFO_INITIALIZER;
extern int cpr_set_properties(struct cprinfo *);

#if !defined(lint)
_exit()
{
	(void) prom_enter_mon();
	return (0);
}

#endif /* lint */

/*
 * A layer between cprboot and cpr_statefile_read.
 * Force a big read to ufs, so that the
 * extra ufs copy can be by passed.
 */
int
cpr_read(int fd, caddr_t buf, size_t count, u_int *rtn_bufp)
{
	size_t n;

	DEBUG4(errp("CPR_READ: Asked to read %lu (contains=%lu)\n", count,
	    contains));
	DEBUG4(errp("CPR_READ: fptr %x e_buf_addr %x\n", fptr, e_buf_addr));
	/*
	 * Don't see any way around duplicating this code except by
	 * doing the bcopy even when not necessary (or goto)
	 */
	if (contains >= count) {
		DEBUG4(errp("CPR_READ: Had data already, fptr %x\n", fptr));
		if (buf != NULL) {
			bcopy((char *)fptr, buf, count);
		} else {
			*rtn_bufp = (u_int)fptr;
			DEBUG4(errp("CPR_READ: Return ptr %x\n",
			    *rtn_bufp));
		}
		fptr += count;
		contains -= count;
		return ((int)count);
	}

	if (count > MAX_READ) {
		prom_printf("cpr_read asked to read %lu, max supported "
		    "read is %d\n", count, MAX_READ);
	    return (-1);
	}
	if (contains == 0) {
		read_ptr = fptr = read_buf;
	} else if ((read_ptr + MAX_READ - 1) > e_buf_addr) {
		DEBUG4(errp("Slide up: contains %lu fptr %x e_buf_addr "
		    "%x read_ptr %x\n", contains, fptr, e_buf_addr,
		    read_ptr));
		bcopy((char *)fptr, (caddr_t)read_buf, contains);
		read_ptr = read_buf + contains;
		fptr = read_buf;
	}

	while (contains < count) {
		/*
		 * Attempt to read 32k data at a time.
		 * ufs read detects read past EOF.
		 */
		if ((n = cpr_statefile_read(fd,
		    (caddr_t)read_ptr, MAX_READ)) < 0) {
			prom_printf("cpr_read: read error\n");
			return (-1);
		} else if (n == 0) {
			prom_printf("cpr_read: premature EOF, "
			    "%lu bytes short\n", count - contains);
			return (-1);
		} else {
			contains += n;
			read_ptr += n;
			DEBUG4(errp("CPR_READ: Just read %d bytes\n", n));
		}
	}
	if (buf != NULL) {
		bcopy((char *)fptr, buf, count);
	} else {
		*rtn_bufp = (u_int)fptr;
		DEBUG4(errp("CPR_READ: Return ptr %x\n",
		    *rtn_bufp));
	}
	fptr += count;
	DEBUG4(errp("CPR_READ: After read fptr %x\n", fptr));
	contains -= count;
	return ((int)count);
}

/*
 * cprboot:cpr_skip_bitmaps calls seek on the underlying file.
 * We have to forget that we've ever read the file so we'll do
 * another read to get in sync.  The alternative was to do 32K reads
 * for each bitmap descriptor
 */
void
cpr_reset_read(void)
{
	contains = 0;
	read_ptr = fptr = read_buf;
}

/*
 * Read the config file and pass back the file path, filesystem
 * device path.
 */
int
cpr_read_cprinfo(int fd, char *file_path, char *fs_path)
{
	struct cprconfig cf;

	if (cpr_ufs_read(fd, (char *)&cf, sizeof (struct cprconfig)) !=
	    sizeof (struct cprconfig) || cf.cf_magic != CPR_CONFIG_MAGIC)
		return (-1);

	(void) strcpy(file_path, cf.cf_path);
	(void) strcpy(fs_path, cf.cf_dev_prom);

	return (0);
}

/*
 * Read the location of the state file from the root filesystem.
 * Pass back to the caller the full device path of the filesystem
 * and the filename relative to that fs.  If the config file
 * doesn't exist, use default values for filesystem/file.
 */
int
cpr_locate_statefile(char *file_path, char *fs_path)
{
	int fd;
	char *bootpath = prom_bootpath();
	int rc;

	if ((fd = cpr_ufs_open(CPR_CONFIG, bootpath)) != -1) {
		rc = cpr_read_cprinfo(fd, file_path, fs_path);
		cpr_ufs_close(fd);
	} else
		rc = -1;

	return (rc);
}

/*
 * Open the "defaults" file in the root fs and read the values of the
 * properties saved during the checkpoint.  Restore the values to nvram.
 *
 * Note: an invalid magic number in the "defaults" file means that the
 * state file is bad or obsolete so our caller should not proceed with
 * the resume.
 */
int
cpr_reset_properties()
{
	struct cprinfo ci;
	int fd;
	char *bootpath = prom_bootpath();

	if ((fd = cpr_ufs_open(CPR_DEFAULT, bootpath)) == -1) {
		errp("cpr_reset_properties: Unable to open %s on %s.\n",
		    CPR_DEFAULT, bootpath);
		return (-1);
	}

	if (cpr_ufs_read(fd, (char *)&ci,
	    sizeof (struct cprinfo)) != sizeof (struct cprinfo)) {
		errp("cpr_reset_properties: Unable to read "
		    "old boot-file value.\n");
		(void) cpr_ufs_close(fd);
		return (-1);
	}
	if (ci.ci_magic != CPR_DEFAULT_MAGIC) {
		errp("cpr_reset_properties: Bad magic number in $s\n",
		    CPR_DEFAULT);
		(void) cpr_ufs_close(fd);
		return (-1);
	}

	(void) cpr_ufs_close(fd);

	return (cpr_set_properties(&ci));
}

/*
 * Set the the nvram properties to the values contained in the incoming
 * cprinfo structure.
 */
int
cpr_set_properties(struct cprinfo *ci)
{
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;
	int i;
	int failures = 0;
	const struct prop_info *pi = cpr_prop_info;

	stk = prom_stack_init(sp, OBP_STACKDEPTH);
	node = prom_findnode_byname(prom_nextnode(0), "options", stk);
	prom_stack_fini(stk);

	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		errp("cpr_set_bootinfo: Cannot find \"options\" node.\n");

		return (-1);
	}

	for (i = 0;
		i < sizeof (cpr_prop_info) / sizeof (struct prop_info); i++) {
		char *prop_value = (char *)ci + pi[i].pinf_offset;
		int prop_len = strlen(prop_value);

		/*
		 * Note: When doing a prom_setprop you must include the
		 * trailing NULL in the length argument, but when calling
		 * prom_getproplen() the NULL is excluded from the count!
		 */
		if (prom_setprop(node,
		    pi[i].pinf_name, prop_value, prop_len + 1) < 0 ||
		    prom_getproplen(node, pi[i].pinf_name) < prop_len) {
			errp("cpr_set_bootinfo: Can't set "
			    "property %s.\nval=%s\n",
			    pi[i].pinf_name, prop_value);
			failures++;
		}
	}

	return (failures ? -1 : 0);
}

/*
 * Read and verify cpr dump descriptor
 */
int
cpr_read_cdump(int fd, cdd_t *cdp, u_short mach_type)
{
	char *str;
	int nread;

	str = "cpr_read_cdump:";
	nread = cpr_read(fd, (caddr_t)cdp, sizeof (*cdp), 0);
	if (nread != sizeof (*cdp)) {
		errp("%s Error reading cpr dump descriptor\n", str);
		return (-1);
	}

	if (cdp->cdd_magic != CPR_DUMP_MAGIC) {
		errp("%s bad dump Magic 0x%x, expected 0x%x\n",
		    str, cdp->cdd_magic, CPR_DUMP_MAGIC);
		return (-1);
	}

	if (cdp->cdd_version != CPR_VERSION) {
		errp("%s bad cpr version %d, expected %d\n",
		    str, cdp->cdd_version, CPR_VERSION);
		return (-1);
	}

	if (cdp->cdd_machine != mach_type) {
		errp("%s bad machine type 0x%x, expected 0x%x\n",
		    str, cdp->cdd_machine, mach_type);
		return (-1);
	}

	if (cdp->cdd_bitmaprec <= 0) {
		errp("%s bad bitmap %d\n", str, cdp->cdd_bitmaprec);
		return (-1);
	}

	if (cdp->cdd_dumppgsize <= 0) {
		errp("%s Bad pg tot %d\n", str, cdp->cdd_dumppgsize);
		return (-1);
	}

	cpr_debug = cdp->cdd_debug;

	return (0);
}

/*
 * Read and verify cpr dump terminator
 */
int
cpr_read_terminator(int fd, ctrm_t *ctp, u_int mapva)
{
	ctrm_t ct_saved, *cp;	/* terminator from the statefile */

	if ((cpr_read(fd, (caddr_t)&ct_saved, sizeof (ct_saved), 0)) !=
	    sizeof (ct_saved)) {
		errp("cpr_read_terminator: err reading cpr terminator\n");
		return (-1);
	}

	if (ct_saved.magic != CPR_TERM_MAGIC) {
		errp("cpr_read_terminator: bad terminator magic %x (v.s %x)\n",
		    ct_saved.magic, CPR_TERM_MAGIC);
		return (-1);
	}

	prom_unmap((caddr_t)mapva, MMU_PAGESIZE);

	prom_map_plat((caddr_t)mapva, PN_TO_ADDR(ct_saved.pfn),
		MMU_PAGESIZE, (u_int)ct_saved.va);

	/*
	 * Add the offset to reach the terminator in the kernel so that we
	 * can directly change the restored kernel image.
	 */
	cp = (ctrm_t *)(mapva + (ct_saved.va & MMU_PAGEOFFSET));

	cp->real_statef_size = ct_saved.real_statef_size;
	cp->tm_shutdown = ct_saved.tm_shutdown;
	cp->tm_cprboot_start.tv_sec = ctp->tm_cprboot_start.tv_sec;
	cp->tm_cprboot_end.tv_sec = prom_gettime() / 1000;

	prom_unmap((caddr_t)mapva, MMU_PAGESIZE);

	return (0);
}

/*
 * Read the machdep descriptor and return the length of the machdep
 * section which follows.
 */
ssize_t
cpr_get_machdep_len(int fd)
{
	cmd_t cmach;

	if (cpr_read(fd, (caddr_t)&cmach, sizeof (cmd_t), 0) !=
	    sizeof (cmd_t)) {
		errp("cpr_get_machdep_len: Err reading cpr machdep "
		    "descriptor");
		return ((ssize_t)-1);
	}

	if (cmach.md_magic != CPR_MACHDEP_MAGIC) {
		errp("cpr_get_machdep_len: Bad machdep magic %x\n",
			cmach.md_magic);
		return ((ssize_t)-1);
	}

	return ((ssize_t)cmach.md_size);
}

/*
 * Read opaque platform specific info.
 */
int
cpr_read_machdep(int fd, caddr_t bufp, size_t len)
{
	if (cpr_read(fd, bufp, len, 0) != len) {
		errp("cpr_read_machdep: failed to read machdep info\n");
		return (-1);
	}

	return (0);
}


/*
 * Read in kernel pages
 * Return pages read otherwise -1 for failure
 */
int
cpr_read_phys_page(int fd, u_int free_va, int *compress)
{
	u_int len = 0;
	cpd_t cpgdesc;		/* cpr page descriptor */
	caddr_t datap;
	physaddr_t cpr_pa;
	u_int rtn_bufp;		/* ptr return from cpr_read() */
	extern u_int cpr_sum(u_char *, int);

	/* First read page descriptor */
	if ((cpr_read(fd, (caddr_t)&cpgdesc, sizeof (cpd_t), &rtn_bufp)) !=
	    sizeof (cpd_t)) {
		errp("cpr_read_phys_page: Error reading page desc\n");
		return (-1);
	}

	if (cpgdesc.cpd_magic != CPR_PAGE_MAGIC) {
		errp("cpr_read_phys_page: Page BAD MAGIC cpg=%x\n", &cpgdesc);
		errp("BAD MAGIC (%x) should be (%x)\n",
			cpgdesc.cpd_magic, CPR_PAGE_MAGIC);
		return (-1);
	}

	/*
	 * Get physical address, should be page aligned
	 */
	cpr_pa = PN_TO_ADDR(cpgdesc.cpd_pfn);

	DEBUG4(errp("about to read: pa=0x%x pfn=0x%x len=%d\n",
		cpr_pa, cpgdesc.cpd_pfn, cpgdesc.cpd_length));

	/*
	 * XXX: Map the physical page to the virtual address,
	 * and read into the virtual.
	 * XXX: There is potential problem in the OBP, we just
	 * want to predefine an virtual address for it, so that
	 * OBP won't mess up any of its own memory allocation.
	 * REMEMBER to change it later !
	 */
	prom_unmap((caddr_t)free_va, (cpgdesc.cpd_page * MMU_PAGESIZE));

	prom_map_plat((caddr_t)free_va, cpr_pa,
		(cpgdesc.cpd_page * MMU_PAGESIZE), cpgdesc.cpd_va);

	/*
	 * Copy non-compressed data directly to the mapped vitrual;
	 * for compressed data, decompress them directly from
	 * the cpr_read() buffer to the mapped virtual.
	 */
	if (!(cpgdesc.cpd_flag & CPD_COMPRESS))
		datap = (caddr_t)free_va;
	else
		datap = NULL;

	if (cpr_read(fd, datap, cpgdesc.cpd_length, &rtn_bufp)
		!= cpgdesc.cpd_length) {
		errp("cpr_read_phys_page: Err reading page: len %d\n",
			cpgdesc.cpd_length);
		return (-1);
	}

	/* Decompress data into physical page directly */
	if (cpgdesc.cpd_flag & CPD_COMPRESS) {
		if ((cpgdesc.cpd_flag & CPD_CSUM) &&
		    (cpgdesc.cpd_csum !=
		    cpr_sum((u_char *)rtn_bufp, cpgdesc.cpd_length))) {
			errp("cpr_read_phys_page: bad checksum on compressed"
			    " data\n");
			DEBUG4(errp("Read csum %x, expected %x\n",
			    cpr_sum((u_char *)rtn_bufp, cpgdesc.cpd_length),
			    cpgdesc.cpd_csum));
			return (-1);
		}
#if !defined(lint)
		len = cpr_decompress((u_char *)rtn_bufp, cpgdesc.cpd_length,
			(uchar_t *)free_va);
#endif /* lint */
		if (len != (cpgdesc.cpd_page * MMU_PAGESIZE)) {
			errp("cpr_read_page: bad decompressed len %d "
				"compressed len %d\n", len, cpgdesc.cpd_length);
			return (-1);
		}
		*compress = 1;
	} else {
		*compress = 0;
	}
	if ((cpgdesc.cpd_flag & CPD_USUM) && (cpgdesc.cpd_usum !=
	    cpr_sum((u_char *)free_va, cpgdesc.cpd_page * MMU_PAGESIZE))) {
		errp("cpr_read_phys_page: bad checksum on uncompressed data\n");
		DEBUG4(errp("Read usum %x, expected %x\n",
		    cpr_sum((u_char *)rtn_bufp, cpgdesc.cpd_length),
		    cpgdesc.cpd_usum));
		return (-1);
	}

	DEBUG4(errp("Read: pa=%x pfn=%x\n", cpr_pa, cpgdesc.cpd_pfn));

	return (cpgdesc.cpd_page);

}
