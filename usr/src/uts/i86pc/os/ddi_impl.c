/*
 * Copyright (c) 1991-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_impl.c	1.82	98/01/24 SMI"

/*
 * PC specific DDI implementation
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/vm.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>

#include <sys/mman.h>
#include <sys/map.h>
#include <sys/psw.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/promif.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/archsystm.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/bitmap.h>

/*
 * DDI(Sun) Function and flag definitions:
 */
static caddr_t	kalloca(u_int, int, int, int, ddi_dma_attr_t *);
static void kfreea(caddr_t);

/*
 * Enable DDI_MAP_DEBUG for map debugging messages...
 * (c.f. rootnex.c)
 * #define	DDI_MAP_DEBUG
 */

#ifdef	DDI_MAP_DEBUG
int ddi_map_debug_flag = 1;
#define	ddi_map_debug	if (ddi_map_debug_flag) printf
#endif	DDI_MAP_DEBUG

/*
 * turn this on to force isa, eisa, and mca device to ignore the new
 * hardware nodes in the device tree (normally turned on only for
 * drivers that need it by setting the property "ignore-hardware-nodes"
 * in their driver.conf file).
 *
 * 7/31/96 -- Turned off globally.  Leaving variable in for the moment
 *		as safety valve.
 */
int ignore_hardware_nodes = 0;

/*
 * Local data
 */
static struct impl_bus_promops *impl_busp;

/*
 * i_ddi_bus_map:
 * Generic bus_map entry point, for byte addressable devices
 * conforming to the reg/range addressing model with no HAT layer
 * to be programmed at this level.
 */

int
i_ddi_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec tmp_reg, *rp;
	ddi_map_req_t mr = *mp;		/* Get private copy of request */
	int error;

	mp = &mr;

	/*
	 * First, if given an rnumber, convert it to a regspec...
	 */

	if (mp->map_type == DDI_MT_RNUMBER)  {

		int rnumber = mp->map_obj.rnumber;
#ifdef	DDI_MAP_DEBUG
		static char *out_of_range =
		    "i_ddi_bus_map: Out of range rnumber <%d>, device <%s>";
#endif	DDI_MAP_DEBUG

		rp = i_ddi_rnumber_to_regspec(rdip, rnumber);
		if (rp == (struct regspec *)0)  {
#ifdef	DDI_MAP_DEBUG
			cmn_err(CE_WARN, out_of_range, rnumber,
			    ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG
			return (DDI_ME_RNUMBER_RANGE);
		}

		/*
		 * Convert the given ddi_map_req_t from rnumber to regspec...
		 */

		mp->map_type = DDI_MT_REGSPEC;
		mp->map_obj.rp = rp;
	}

	/*
	 * Adjust offset and length correspnding to called values...
	 * XXX: A non-zero length means override the one in the regspec.
	 * XXX: (Regardless of what's in the parent's range)
	 */

	tmp_reg = *(mp->map_obj.rp);		/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;		/* Use tmp_reg in request */

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"i_ddi_bus_map: <%s,%s> <0x%x, 0x%x, 0x%d> "
		"offset %d len %d handle 0x%x\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len, mp->map_handlep);
#endif	DDI_MAP_DEBUG

	/*
	 * I/O or memory mapping
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 *	<bustype>1, addr=0, len=x>: x86-compatibility i/o
	 */

	if (rp->regspec_bustype > 1 && rp->regspec_addr != 0) {
		cmn_err(CE_WARN, "<%s,%s>: invalid register spec"
		    " <0x%x, 0x%x, 0x%x>\n", ddi_get_name(dip),
		    ddi_get_name(rdip), rp->regspec_bustype,
		    rp->regspec_addr, rp->regspec_size);
		return (DDI_ME_INVAL);
	}

	if (rp->regspec_bustype > 1 && rp->regspec_addr == 0) {
		/*
		 * compatibility i/o mapping
		 */
		rp->regspec_bustype += (u_int)offset;
	} else {
		/*
		 * Normal memory or i/o mapping
		 */
		rp->regspec_addr += (u_int)offset;
	}

	if (len != 0)
		rp->regspec_size = (u_int)len;

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"               <%s,%s> <0x%x, 0x%x, 0x%d> "
		"offset %d len %d\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len);
#endif	DDI_MAP_DEBUG

	/*
	 * If we had an MMU, this is where you'd program the MMU and hat layer.
	 * Since we're using the default function here, we do not have an MMU
	 * to program.
	 */

	/*
	 * Apply any parent ranges at this level, if applicable.
	 * (This is where nexus specific regspec translation takes place.
	 * Use of this function is implicit agreement that translation is
	 * provided via ddi_apply_range.)  Note that we assume that
	 * the request is within the parents limits.
	 */

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("applying range of parent <%s> to child <%s>...\n",
	    ddi_get_name(dip), ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG

	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0)
		return (error);

	/*
	 * Call my parents bus_map function with modified values...
	 */

	return (ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp));
}

/*
 * Creating register mappings and handling interrupts:
 */

struct regspec *
i_ddi_rnumber_to_regspec(dev_info_t *dip, int rnumber)
{
	if (rnumber >= sparc_pd_getnreg(DEVI(dip)))
		return ((struct regspec *)0);

	return (sparc_pd_getreg(DEVI(dip), rnumber));
}

/*
 * Static function to determine if a reg prop is enclosed within
 * a given a range spec.  (For readability: only used by i_ddi_aply_range.).
 */
static int
reg_is_enclosed_in_range(struct regspec *rp, struct rangespec *rangep)
{
	if (rp->regspec_bustype != rangep->rng_cbustype)
		return (0);

	if (rp->regspec_addr < rangep->rng_coffset)
		return (0);

	if (rangep->rng_size == 0)
		return (1);	/* size is really 2**(bits_per_word) */

	if ((rp->regspec_addr + rp->regspec_size - 1) <=
	    (rangep->rng_coffset + rangep->rng_size - 1))
		return (1);

	return (0);
}

/*
 * i_ddi_apply_range:
 * Apply range of dp to struct regspec *rp, if applicable.
 * If there's any range defined, it gets applied.
 */

int
i_ddi_apply_range(dev_info_t *dp, dev_info_t *rdip, struct regspec *rp)
{
	int nrange, b;
	struct rangespec *rangep;
	static char *out_of_range =
	    "Out of range register specification from device node <%s>\n";

	nrange = sparc_pd_getnrng(dp);
	if (nrange == 0)  {
#ifdef	DDI_MAP_DEBUG
		ddi_map_debug("    No range.\n");
#endif	DDI_MAP_DEBUG
		return (0);
	}

	/*
	 * Find a match, making sure the regspec is within the range
	 * of the parent, noting that a size of zero in a range spec
	 * really means a size of 2**(bitsperword).
	 */

	for (b = 0, rangep = sparc_pd_getrng(dp, 0); b < nrange; ++b, ++rangep)
		if (reg_is_enclosed_in_range(rp, rangep))
			break;		/* found a match */

	if (b == nrange)  {
		cmn_err(CE_WARN, out_of_range, ddi_get_name(rdip));
		return (DDI_ME_REGSPEC_RANGE);
	}

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("    Input:  %x.%x.%x\n", rp->regspec_bustype,
	    rp->regspec_addr, rp->regspec_size);
	ddi_map_debug("    Range:  %x.%x %x.%x %x\n",
	    rangep->rng_cbustype, rangep->rng_coffset,
	    rangep->rng_bustype, rangep->rng_offset, rangep->rng_size);
#endif	DDI_MAP_DEBUG

	rp->regspec_bustype = rangep->rng_bustype;
	rp->regspec_addr += rangep->rng_offset - rangep->rng_coffset;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("    Return: %x.%x.%x\n", rp->regspec_bustype,
	    rp->regspec_addr, rp->regspec_size);
#endif	DDI_MAP_DEBUG

	return (0);
}

/*
 * i_ddi_map_fault: wrapper for bus_map_fault.
 */
int
i_ddi_map_fault(dev_info_t *dip, dev_info_t *rdip,
	struct hat *hat, struct seg *seg, caddr_t addr,
	struct devpage *dp, u_int pfn, u_int prot, u_int lock)
{
	dev_info_t *pdip;

	if (dip == NULL)
		return (DDI_FAILURE);

	pdip = (dev_info_t *)DEVI(dip)->devi_bus_map_fault;

	/* request appropriate parent to map fault */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_map_fault))(pdip,
	    rdip, hat, seg, addr, dp, pfn, prot, lock));
}

/*
 * i_ddi_get_intrspec:	convert an interrupt number to an interrupt
 *			specification. The interrupt number determines which
 *			interrupt will be returned if more than one exists.
 *			returns an interrupt specification if successful and
 *			NULL if the interrupt specification could not be found.
 *			If "name" is NULL, first (and only) interrupt
 *			name is searched for.  this is the wrapper for the
 *			bus function bus_get_intrspec.
 */
ddi_intrspec_t
i_ddi_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to return an interrupt specification */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_get_intrspec))(pdip,
	    rdip, inumber));
}

/*
 * i_ddi_add_intrspec:	Add an interrupt specification.	If successful,
 *			the parameters "iblock_cookiep", "device_cookiep",
 *			"int_handler", "int_handler_arg", and return codes
 *			are set or used as specified in "ddi_add_intr".
 */
int
i_ddi_add_intrspec(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to add an interrupt specification */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_add_intrspec))(pdip,
	    rdip, intrspec, iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg, kind));
}

/*
 * i_ddi_add_softintr - add a soft interrupt to the system
 */
int
i_ddi_add_softintr(dev_info_t *rdip, int preference, ddi_softintr_t *idp,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg)
{
	dev_info_t *rp;
	struct soft_intrspec *sspec;
	struct intrspec *ispec;
	int r;

	if (idp == NULL)
		return (DDI_FAILURE);
	sspec = (struct soft_intrspec *)kmem_zalloc(sizeof (*sspec), KM_SLEEP);
	sspec->si_devi = (struct dev_info *)rdip;
	ispec = &sspec->si_intrspec;
	if (preference > DDI_SOFTINT_MED) {
		ispec->intrspec_pri = 6;
	} else {
		ispec->intrspec_pri = 4;
	}
	rp = ddi_root_node();
	r = (*(DEVI(rp)->devi_ops->devo_bus_ops->bus_add_intrspec))(rp,
	    rdip, (ddi_intrspec_t)ispec, iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg, IDDI_INTR_TYPE_SOFT);

	if (r != DDI_SUCCESS) {
		kmem_free(sspec, sizeof (*sspec));
		return (r);
	}
	*idp = (ddi_softintr_t)sspec;
	return (DDI_SUCCESS);
}

extern void (*setsoftint)(int);
void
i_ddi_trigger_softintr(ddi_softintr_t id)
{
	struct soft_intrspec *sspec = (struct soft_intrspec *)id;
	struct intrspec *ip;

	ip = &sspec->si_intrspec;
	(*setsoftint)(ip->intrspec_pri);
}

void
i_ddi_remove_softintr(ddi_softintr_t id)
{
	struct soft_intrspec *sspec = (struct soft_intrspec *)id;
	struct intrspec *ispec = &sspec->si_intrspec;

	(void) rem_avsoftintr((void *)ispec, ispec->intrspec_pri,
		ispec->intrspec_func);
	kmem_free(sspec, sizeof (*sspec));
}


/*
 * i_ddi_remove_intrspec:
 *
 *	NOTE - this is nolonger a wrapper for bus_remove_intrspec
 *		It is believed that interrupts and busses are separate
 *		in the PC world and that the interrupt registration
 *		routine can be called directly.  If this assumption
 *		changes then this code will also have to change.
 */
void
i_ddi_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to remove an interrupt specification */
	(*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_remove_intrspec))(pdip,
	    rdip, intrspec, iblock_cookie);
}

/*
 * Functions for nexus drivers only:
 */

/*
 * config stuff
 */
dev_info_t *
i_ddi_add_child(dev_info_t *pdip, char *name, u_int nodeid, u_int unit)
{
	struct dev_info *devi;

	devi = (struct dev_info *)kmem_zalloc(sizeof (*devi), KM_SLEEP);
	devi->devi_node_name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(devi->devi_node_name,  name);
	devi->devi_nodeid = nodeid;
	devi->devi_instance = unit;

	/*
	 * Cache the value of the 'compatible' property from the prom,
	 * only if its a prom node.  NB: .conf file nodes must name a driver.
	 * NB: The binding name is set in ddi_append_dev.
	 */
	if (DEVI_PROM_NODE(nodeid)) {
		int length;
		static const char *compat = "compatible";

		if ((length = prom_getproplen((dnode_t)nodeid,
		    (caddr_t)compat)) > 0) {
			devi->devi_compat_names =
			    (char *)kmem_zalloc((size_t)length, KM_SLEEP);
			(void) prom_getprop((dnode_t)nodeid, (caddr_t)compat,
			    devi->devi_compat_names);
			devi->devi_compat_length = (size_t)length;
		}
	}

	mutex_init(&(devi->devi_lock), NULL, MUTEX_DEFAULT, NULL);
	ddi_append_dev(pdip, (dev_info_t *)devi);
	return ((dev_info_t *)devi);
}

int
i_ddi_remove_child(dev_info_t *dip, int lockheld)
{
	struct dev_info *pdev, *cdev;
	major_t major;
	struct devnames *dnp = &orphanlist;
	extern struct dev_ops **devopsp;

	if ((dip == (dev_info_t *)0) ||
	    (DEVI(dip)->devi_child != (struct dev_info *)0) ||
	    ((pdev = DEVI(dip)->devi_parent) == (struct dev_info *)0)) {
		return (DDI_FAILURE);
	}

	rw_enter(&(devinfo_tree_lock), RW_WRITER);
	cdev = pdev->devi_child;

	/*
	 * Remove 'dip' from its parents list of children
	 */
	if (cdev == (struct dev_info *)dip) {
		pdev->devi_child = cdev->devi_sibling;
	} else {
		while (cdev != (struct dev_info *)NULL) {
			if (cdev->devi_sibling == DEVI(dip)) {
				cdev->devi_sibling = DEVI(dip)->devi_sibling;
				cdev = DEVI(dip);
				break;
			}
			cdev = cdev->devi_sibling;
		}
	}
	rw_exit(&(devinfo_tree_lock));
	if (cdev == (struct dev_info *)NULL)
		return (DDI_FAILURE);

	/*
	 * Remove 'dip' from the list held on the devnamesp table
	 * if the 'dip' is not bound to a driver, remove it from
	 * the orphanlist instead.
	 *
	 * ASSERT the lock must not be held iff we are removing an
	 * node from the orphanlist.
	 */
	major = ddi_name_to_major(ddi_get_name(dip));
	if ((major != (major_t)-1) && (CB_DRV_INSTALLED(devopsp[major])))
		dnp = &(devnamesp[major]);

	ASSERT(((major == (major_t)-1) ? (lockheld == 0) : 1));

	if (lockheld == 0)
		LOCK_DEV_OPS(&(dnp->dn_lock));

	cdev = DEVI(dnp->dn_head);
	if (cdev == DEVI(dip))  {
			dnp->dn_head = (dev_info_t *)cdev->devi_next;
	} else while (cdev != NULL)  {
		if (cdev->devi_next == DEVI(dip))  {
			cdev->devi_next = DEVI(dip)->devi_next;
			break;
		}
		cdev = cdev->devi_next;
	}
	if (lockheld == 0)
		UNLOCK_DEV_OPS(&(dnp->dn_lock));

	/*
	 * Strip 'dip' clean and toss it over the side ..
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);
	impl_rem_hw_props(dip);
	mutex_destroy(&(DEVI(dip)->devi_lock));

	i_ddi_set_binding_name(dip, NULL);
	if (DEVI(dip)->devi_compat_names)
		kmem_free(DEVI(dip)->devi_compat_names,
		    DEVI(dip)->devi_compat_length);

	kmem_free(DEVI(dip)->devi_node_name,
	    (size_t)(strlen(DEVI(dip)->devi_node_name) + 1));

	kmem_free(dip, sizeof (struct dev_info));

	return (DDI_SUCCESS);
}

/*
 * Bind this devinfo node to a driver.
 * If compat is NON-NULL, first try that ... failing that,
 * use the node-name.
 *
 * If we find a binding, set the binding name to the the string we used
 * and return the major number of the driver binding. If we don't find
 * a binding, we just bind to our own name, so the binding is always
 * set.  We try to rebind all unbound nodes when we load drivers.
 */
major_t
i_ddi_bind_node_to_driver(dev_info_t *dip)
{
	major_t maj;

	char *p = 0;
	void *compat;
	size_t len;

	compat = (void *)(DEVI(dip)->devi_compat_names);
	len = DEVI(dip)->devi_compat_length;

	while ((p = prom_decode_composite_string(compat, len, p)) != 0) {
		if ((maj = ddi_name_to_major(p)) != -1) {
			i_ddi_set_binding_name(dip, p);
			return (maj);
		}
	}

	i_ddi_set_binding_name(dip, ddi_node_name(dip));
	maj = ddi_name_to_major(ddi_node_name(dip));	/* -1 if unbound */
	return (maj);
}

int
i_ddi_initchild(dev_info_t *prnt, dev_info_t *proto)
{
	int (*f)();
	int error;

	if (DDI_CF1(proto))
		return (DDI_SUCCESS);

	ASSERT(prnt);
	ASSERT(DEVI(prnt) == DEVI(proto)->devi_parent);

	/*
	 * The parent must be in cannonical form 2 in order to use its bus ops.
	 */
	if (impl_proto_to_cf2(prnt) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * The parent must have a bus_ctl operation.
	 */
	if ((DEVI(prnt)->devi_ops->devo_bus_ops == NULL) ||
	    (f = DEVI(prnt)->devi_ops->devo_bus_ops->bus_ctl) == NULL) {
		/*
		 * Release the dev_ops which were held in impl_proto_to_cf2().
		 */
		ddi_rele_devi(prnt);
		return (DDI_FAILURE);
	}

	/*
	 * Invoke the parent's bus_ctl operation with the DDI_CTLOPS_INITCHILD
	 * command to transform the child to cannonical form 1. If there
	 * is an error, ddi_remove_child should be called, to clean up.
	 */
	error = (*f)(prnt, prnt, DDI_CTLOPS_INITCHILD, proto, (void *)NULL);
	if (error != DDI_SUCCESS)
		ddi_rele_devi(prnt);
	else {
		/*
		 * Apply multi-parent/deep-nexus optimization to the new node
		 */
		ddi_optimize_dtree(proto);
	}

	return (error);
}

extern caddr_t lomem_alloc(u_int nbytes, ddi_dma_attr_t *attr,
	    int align, int cansleep);
extern void lomem_free(caddr_t kaddr);

/*
 * This should actually be called i_ddi_dma_mem_alloc. There should
 * also be an i_ddi_pio_mem_alloc. i_ddi_dma_mem_alloc should call
 * through the device tree with the DDI_CTLOPS_DMA_ALIGN ctl ops to
 * get alignment requirements for DMA memory. i_ddi_pio_mem_alloc
 * should use DDI_CTLOPS_PIO_ALIGN. Since we only have i_ddi_mem_alloc
 * so far which is used for both, DMA and PIO, we have to use the DMA
 * ctl ops to make everybody happy.
 */
int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attr,
	size_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_hdl_t *ap)
{
	caddr_t a;
	int iomin;
	ddi_acc_impl_t *iap;
	int physcontig;
	extern long lomempages;

#if defined(lint)
	dip = dip;
	accattrp = accattrp;
	streaming = streaming;
#endif

	/*
	 * Check legality of arguments
	 */
	if (length == 0 || kaddrp == NULL || attr == NULL) {
		return (DDI_FAILURE);
	}
	if (attr->dma_attr_minxfer == 0 || attr->dma_attr_align == 0 ||
		(attr->dma_attr_align & (attr->dma_attr_align - 1)) ||
		(attr->dma_attr_minxfer & (attr->dma_attr_minxfer - 1))) {
			return (DDI_FAILURE);
	}

	/*
	 * figure out most restrictive alignment requirement
	 */
	iomin = attr->dma_attr_minxfer;
	iomin = maxbit(iomin, attr->dma_attr_align);
	if (iomin == 0)
		return (DDI_FAILURE);

	ASSERT((iomin & (iomin - 1)) == 0);

	length = roundup(length, iomin);

	/*
	 * Determine if we need to satisfy the request for physically
	 * contiguous memory.
	 */
	if ((attr->dma_attr_sgllen == 1) ||
	    (btopr(length) > attr->dma_attr_sgllen))
		physcontig = 1;
	else
		physcontig = 0;

	/*
	 * Currently we can allocate contiguous memory of size more
	 * than a system page only from the reserved lomem pool.
	 * If the request is bigger than the lomem pool size then
	 * return failure. Also we can not satisfy the request if the
	 * alignment is greater than PAGESIZE.
	 */
	if ((physcontig && (btopr(length) > lomempages)) || (iomin > PAGESIZE))
		return (DDI_FAILURE);

	/*
	 * Allocate the requested amount from the system
	 */

	a = kalloca(length, iomin, cansleep, physcontig, attr);

	if ((*kaddrp = a) == 0)
		return (DDI_FAILURE);

	if (real_length) {
		*real_length = length;
	}
	if (ap) {
		/*
		 * initialize access handle
		 */
		iap = (ddi_acc_impl_t *)ap->ah_platform_private;
		iap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(ap);
	}
	return (DDI_SUCCESS);
}

/*
 * covert old DMA limits structure to DMA attribute structure
 * and continue
 */
int
i_ddi_mem_alloc_lim(dev_info_t *dip, ddi_dma_lim_t *limits,
	u_int length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	u_int *real_length, ddi_acc_hdl_t *ap)
{
	ddi_dma_attr_t dma_attr, *attrp;
	size_t rlen;
	int ret;

	if (limits == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * set up DMA attribute structure to pass to i_ddi_mem_alloc()
	 */
	attrp = &dma_attr;
	attrp->dma_attr_version = DMA_ATTR_V0;
	attrp->dma_attr_addr_lo = (uint64_t)limits->dlim_addr_lo;
	attrp->dma_attr_addr_hi = (uint64_t)limits->dlim_addr_hi;
	attrp->dma_attr_count_max = (uint64_t)limits->dlim_ctreg_max;
	attrp->dma_attr_align = 1;
	attrp->dma_attr_burstsizes = (uint_t)limits->dlim_burstsizes;
	attrp->dma_attr_minxfer = (uint32_t)limits->dlim_minxfer;
	attrp->dma_attr_maxxfer = (uint64_t)limits->dlim_reqsize;
	attrp->dma_attr_seg = (uint64_t)limits->dlim_adreg_max;
	attrp->dma_attr_sgllen = limits->dlim_sgllen;
	attrp->dma_attr_granular = (uint32_t)limits->dlim_granular;
	attrp->dma_attr_flags = 0;

	ret = i_ddi_mem_alloc(dip, attrp, (size_t)length, cansleep, streaming,
			accattrp, kaddrp, &rlen, ap);
	if (ret == DDI_SUCCESS) {
		if (real_length)
			*real_length = (u_int)rlen;
	}
	return (ret);
}

/* ARGSUSED */
void
i_ddi_mem_free(caddr_t kaddr, int stream)
{
	kfreea(kaddr);
}

/*
 * Miscellaneous implementation functions
 */

/*
 * Wrapper for ddi_prop_lookup_int_array().
 * This is handy because it returns the prop length in
 * bytes which is what most of the callers require.
 */

static int
get_prop_int_array(dev_info_t *di, char *pname, int **pval, u_int *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    DDI_PROP_DONTPASS, pname, pval, plen))
	    == DDI_PROP_SUCCESS) {
		*plen = (*plen) * (sizeof (int));
	}
	return (ret);
}



struct prop_ispec {
	u_int	pri, vec;
};

/*
 * Create a ddi_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and either an "intr" or "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is assumed to be a list of at least one triple
 *
 *	<bustype, address, size>*1
 *
 * The "intr" property is assumed to be a list of at least one duple
 *
 *	<SPARC ipl, vector#>*1
 *
 * The "interrupts" property is assumed to be a list of at least one
 * n-tuples that describes the interrupt capabilities of the bus the device
 * is connected to.  For SBus, this looks like
 *
 *	<SBus-level>*1
 *
 * (This property obsoletes the 'intr' property).
 *
 * The "ranges" property is optional.
 */
static int
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd)
{
	register struct ddi_parent_private_data *pdptr;
	register int n;
	int *reg_prop, *rgstr_prop, *rng_prop, *intr_prop, *irupts_prop;
	u_int reg_len, rgstr_len, rng_len, intr_len, irupts_len;
	int has_registers = 0;

	*ppd = pdptr = (struct ddi_parent_private_data *)
			kmem_zalloc(sizeof (*pdptr), KM_SLEEP);

	/*
	 * Handle the 'reg'/'registers' properties.
	 * "registers" overrides "reg", but requires that "reg" be exported,
	 * so we can handle wildcard specifiers.  "registers" implies an
	 * sbus style device.  "registers" implies that we insert the
	 * correct value in the regspec_bustype field of each spec for a real
	 * (non-pseudo) device node.
	 */

	if (get_prop_int_array(child, "reg", &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		reg_len = 0;
	}
	if (get_prop_int_array(child, "registers", &rgstr_prop,
	    &rgstr_len) != DDI_PROP_SUCCESS) {
		rgstr_len = 0;
	}

	if (rgstr_len != 0)  {

		if ((ddi_get_nodeid(child) != DEVI_PSEUDO_NODEID) &&
		    (reg_len != 0))  {

			/*
			 * Convert wildcard "registers" for a real node...
			 * (Else, this is the wildcard prototype node)
			 */
			struct regspec *rp = (struct regspec *)reg_prop;
			u_int slot = rp->regspec_bustype;
			int i;

			rp = (struct regspec *)rgstr_prop;
			n = rgstr_len / sizeof (struct regspec);
			for (i = 0; i < n; ++i, ++rp)
				rp->regspec_bustype = slot;
		}

		if (reg_len != 0)
			ddi_prop_free((void *)reg_prop);

		reg_prop = rgstr_prop;
		reg_len = rgstr_len;
		++has_registers;
	}

	if ((n = reg_len) != 0)  {
		pdptr->par_nreg = n / (int)sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * See if I have a range (adding one where needed - this
	 * means to add one for sbus node in sun4c, when romvec > 0,
	 * if no range is already defined in the PROM node.
	 * (Currently no sun4c PROMS define range properties,
	 * but they should and may in the future.)  For the SBus
	 * node, the range is defined by the SBus reg property.
	 */
	if (get_prop_int_array(child, "ranges", &rng_prop, &rng_len)
	    == DDI_PROP_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}

	/*
	 * Handle the 'intr' and 'interrupts' properties
	 */

	/*
	 * For backwards compatibility
	 * we first look for the 'intr' property for the device.
	 */
	if (get_prop_int_array(child, "intr", &intr_prop, &intr_len)
	    != DDI_PROP_SUCCESS) {
		intr_len = 0;
	}

	/*
	 * If we're to support bus adapters and future platforms cleanly,
	 * we need to support the generalized 'interrupts' property.
	 */
	if (get_prop_int_array(child, "interrupts", &irupts_prop,
	    &irupts_len) != DDI_PROP_SUCCESS) {
		irupts_len = 0;
	} else if (intr_len != 0) {
		/*
		 * If both 'intr' and 'interrupts' are defined,
		 * then 'interrupts' wins and we toss the 'intr' away.
		 */
		ddi_prop_free((void *)intr_prop);
		intr_len = 0;
	}

	if (intr_len != 0) {

		/*
		 * Translate the 'intr' property into an array
		 * an array of struct intrspec's.  There's not really
		 * very much to do here except copy what's out there.
		 */

		struct intrspec *new;
		struct prop_ispec *l;

		n = pdptr->par_nintr =
			intr_len / sizeof (struct prop_ispec);
		l = (struct prop_ispec *)intr_prop;
		new = pdptr->par_intr = (struct intrspec *)
		    kmem_zalloc(n * sizeof (struct intrspec), KM_SLEEP);
		while (n--) {
			new->intrspec_pri = l->pri;
			new->intrspec_vec = l->vec;
			new++;
			l++;
		}
		ddi_prop_free((void *)intr_prop);

	} else if ((n = irupts_len) != 0) {
		size_t size;
		int *out;

		/*
		 * Translate the 'interrupts' property into an array
		 * of intrspecs for the rest of the DDI framework to
		 * toy with.  Only our ancestors really know how to
		 * do this, so ask 'em.  We massage the 'interrupts'
		 * property so that it is pre-pended by a count of
		 * the number of integers in the argument.
		 */
		size = sizeof (int) + n;
		out = kmem_alloc(size, KM_SLEEP);
		*out = n / sizeof (int);
		bcopy((caddr_t)irupts_prop, (caddr_t)(out + 1), (size_t)n);
		ddi_prop_free((void *)irupts_prop);
		if (ddi_ctlops(child, child, DDI_CTLOPS_XLATE_INTRS,
		    out, pdptr) != DDI_SUCCESS) {
			cmn_err(CE_CONT,
			    "Unable to translate 'interrupts' for %s%d\n",
			    DEVI(child)->devi_binding_name,
			    DEVI(child)->devi_instance);
		}
		kmem_free(out, size);
	}
	return (has_registers);
}


/*
 * Called from the bus_ctl op of sunbus (sbus, obio, etc) nexus drivers
 * to implement the DDI_CTLOPS_INITCHILD operation.  That is, it names
 * the children of sun busses based on the reg spec.
 *
 * Handles the following properties:
 *	Property		value
 *	  Name			type
 *	on_cpu		cpu type flag (defined in cpu.h)
 *	not_on_cpu	cpu type flag (defined in cpu.h)
 *	reg		register spec
 *	registers	wildcard s/w sbus register spec (.conf file property)
 *	intr		old-form interrupt spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 */

static char *cantmerge = "Cannot merge %s devinfo node %s@%s";

int
impl_ddi_sunbus_initchild(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	char name[MAXNAMELEN];
	int has_registers;
	dev_info_t *parent, *och;
	void impl_ddi_sunbus_removechild(dev_info_t *);

	/*
	 * Fill in parent-private data and this function returns to us
	 * an indication if it used "registers" to fill in the data.
	 */
	has_registers = make_ddi_ppd(child, &pdptr);
	ddi_set_parent_data(child, (caddr_t)pdptr);
	parent = ddi_get_parent(child);

	/*
	 * If this is a s/w node defined with the "registers" property,
	 * this means that this is an "sbus" style device and that this
	 * is a wildcard specifier, whose properties get applied to all
	 * previously defined h/w nodes with the same name and same parent.
	 *
	 * XXX: This functionality is "sbus" class nexus specific...
	 * XXX: and should be a function of that nexus driver only!
	 */

	if ((has_registers) && (ddi_get_nodeid(child) == DEVI_PSEUDO_NODEID)) {

		major_t major;
		int need_init;

		/*
		 * Find all previously defined h/w children with the same name
		 * and same parent and copy the property lists from the
		 * prototype node into the h/w nodes and re-inititialize them.
		 */

		if ((major = ddi_name_to_major(ddi_get_name(child))) == -1)  {
			impl_ddi_sunbus_removechild(child);
			return (DDI_NOT_WELL_FORMED);
		}

		for (och = devnamesp[major].dn_head;
		    (och != NULL) && (och != child);
		    och = ddi_get_next(och))  {

			if ((ddi_get_parent(och) != parent) ||
			    (ddi_get_nodeid(och) == DEVI_PSEUDO_NODEID))
				continue;
			if (strcmp(ddi_get_name(och), ddi_get_name(child)) != 0)
				continue;

			if (DEVI(och)->devi_sys_prop_ptr ||
			    DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och)) {
				cmn_err(CE_WARN, cantmerge, "wildcard",
				    ddi_get_name(och), ddi_get_name_addr(och));
				continue;
			}

			need_init = DDI_CF1(och) ? 1 : 0;
			if (need_init)
				(void) ddi_uninitchild(och);
			copy_prop(DEVI(child)->devi_drv_prop_ptr,
			    &(DEVI(och)->devi_drv_prop_ptr));
			copy_prop(DEVI(child)->devi_sys_prop_ptr,
			    &(DEVI(och)->devi_sys_prop_ptr));
			if (need_init)
				(void) ddi_initchild(parent, och);
		}

		/*
		 * We can toss the wildcard node...
		 */
		impl_ddi_sunbus_removechild(child);
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * Force the name property to be generated from the "reg" property...
	 * (versus the "registers" property, so we always match the obp
	 * namespace no matter what the .conf file said.)
	 */

	name[0] = '\0';
	if ((has_registers) && (ddi_get_nodeid(child) != DEVI_PSEUDO_NODEID)) {

		int *reg_prop;
		u_int reg_len;

		if (get_prop_int_array(child, "reg", &reg_prop, &reg_len) ==
		    DDI_PROP_SUCCESS)  {

			struct regspec *rp = (struct regspec *)reg_prop;

			(void) sprintf(name, "%x,%x", rp->regspec_bustype,
			    rp->regspec_addr);
			ddi_prop_free((void *)reg_prop);
		}

	} else if (sparc_pd_getnreg(child) > 0) {
		(void) sprintf(name, "%x,%x",
		    (u_int)sparc_pd_getreg(child, 0)->regspec_bustype,
		    (u_int)sparc_pd_getreg(child, 0)->regspec_addr);
	}

	ddi_set_name_addr(child, name);
	/*
	 * If another child already exists by this name,
	 * merge these properties onto that one.
	 * NOTE - This property override/merging depends on several things:
	 * 1) That hwconf nodes are 'named' (ddi_initchild()) before prom
	 *	devinfo nodes.
	 * 2) That ddi_findchild() will call ddi_initchild() for all
	 *	siblings with a matching devo_name field.
	 * 3) That hwconf devinfo nodes come "after" prom devinfo nodes.
	 *
	 * Then "och" should be a prom node with no attached properties.
	 */
	if ((och = ddi_findchild(parent, ddi_get_name(child), name)) != NULL &&
	    och != child) {
		if (ddi_get_nodeid(och) == DEVI_PSEUDO_NODEID ||
		    ddi_get_nodeid(child) != DEVI_PSEUDO_NODEID ||
		    DEVI(och)->devi_sys_prop_ptr ||
		    DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och)) {
			cmn_err(CE_WARN, cantmerge, "hwconf",
			    ddi_get_name(child), name);
			impl_ddi_sunbus_removechild(child);
			return (DDI_NOT_WELL_FORMED);
		}
		/*
		 * Move "child"'s properties to "och." and allow the node
		 * to be init-ed (this handles 'reg' and 'intr/interrupts'
		 * in hwconf files overriding those in a hw node)
		 *
		 * Note that 'och' is not yet in canonical form 2, so we
		 * can happily transform it to prototype form and recreate it.
		 */
		(void) ddi_uninitchild(och);
		DEVI(och)->devi_sys_prop_ptr = DEVI(child)->devi_sys_prop_ptr;
		DEVI(och)->devi_drv_prop_ptr = DEVI(child)->devi_drv_prop_ptr;
		DEVI(child)->devi_sys_prop_ptr = NULL;
		DEVI(child)->devi_drv_prop_ptr = NULL;
		(void) ddi_initchild(parent, och);
		/*
		 * To get rid of this child...
		 */
		impl_ddi_sunbus_removechild(child);
		return (DDI_NOT_WELL_FORMED);
	}
	return (DDI_SUCCESS);
}

void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;
	register size_t n;

	if ((pdptr = (struct ddi_parent_private_data *)
	    ddi_get_parent_data(dip)) != NULL)  {
		if ((n = (size_t)pdptr->par_nintr) != 0) {
			/*
			 * Note that kmem_free is used here (instead of
			 * ddi_prop_free) because the contents of the
			 * property were placed into a separate buffer and
			 * mucked with a bit before being stored in par_intr.
			 * The actual return value from the prop lookup
			 * was freed with ddi_prop_free previously.
			 */
			kmem_free(pdptr->par_intr, n *
			    sizeof (struct intrspec));
		}

		if ((n = (size_t)pdptr->par_nrng) != 0)
			ddi_prop_free((void *)pdptr->par_rng);

		if ((n = pdptr->par_nreg) != 0)
			ddi_prop_free((void *)pdptr->par_reg);

		kmem_free(pdptr, sizeof (*pdptr));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);
	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);
}

void
drv_usecwait(clock_t count)
{
	int tens = 0;

	if (count > 10)
		tens = count/10;
	tens++;			/* roundup; wait at least 10 microseconds */
	while (tens > 0) {
		tenmicrosec();
		tens--;
	}
}


/*
 * Code to search hardware layer (PROM), if it exists,
 * on behalf of child.
 *
 * if input dip != child_dip, then call is on behalf of child
 * to search PROM, do it via ddi_prop_search_common() and ascend only
 * if allowed.
 *
 * if input dip == ch_dip (child_dip), call is on behalf of root driver,
 * to search for PROM defined props only.
 *
 * Note that the PROM search is done only if the requested dev
 * is either DDI_DEV_T_ANY or DDI_DEV_T_NONE. PROM properties
 * have no associated dev, thus are automatically associated with
 * DDI_DEV_T_NONE.
 *
 * Modifying flag DDI_PROP_NOTPROM inhibits the search in the h/w layer.
 *
 * Returns DDI_PROP_FOUND_1275 if found to indicate to framework
 * that the property resides in the prom.
 */
/*ARGSUSED*/
int
impl_ddi_bus_prop_op(dev_t dev, dev_info_t *dip, dev_info_t *ch_dip,
    ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int	len;
	caddr_t	buffer;

	/*
	 * If requested dev is DDI_DEV_T_NONE or DDI_DEV_T_ANY, then
	 * look in caller's PROM if it's a self identifying device...
	 * XXX: For x86, we use DEVI_PROM_NODE(nodeid)
	 * to indicate a self identifying device with PROM support.
	 *
	 * Note that this is very similar to ddi_prop_op, but we
	 * search the PROM instead of the s/w defined properties,
	 * and we are called on by the parent driver to do this for
	 * the child.
	 */

	if (((dev == DDI_DEV_T_NONE) || (dev == DDI_DEV_T_ANY)) &&
	    DEVI_PROM_NODE(DEVI(ch_dip)->devi_nodeid) &&
	    ((mod_flags & DDI_PROP_NOTPROM) == 0))  {
		len = prom_getproplen((dnode_t)DEVI(ch_dip)->devi_nodeid,
			name);
		if (len == -1)
			return (DDI_PROP_NOT_FOUND);

		/*
		 * If exists only request, we're done
		 */
		if (prop_op == PROP_EXISTS)
			return (DDI_PROP_FOUND_1275);

		/*
		 * If length only request or prop length == 0, get out
		 */
		if ((prop_op == PROP_LEN) || (len == 0))  {
			*lengthp = len;
			return (DDI_PROP_FOUND_1275);
		}

		/*
		 * Allocate buffer if required... (either way `buffer'
		 * is receiving address).
		 */

		switch (prop_op)  {

		case PROP_LEN_AND_VAL_ALLOC:

			buffer = (caddr_t)kmem_alloc((size_t)len,
			    mod_flags & DDI_PROP_CANSLEEP ?
			    KM_SLEEP : KM_NOSLEEP);
			if (buffer == NULL)  {
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer;
			break;

		case PROP_LEN_AND_VAL_BUF:

			if (len > (*lengthp))  {
				*lengthp = len;
				return (DDI_PROP_BUF_TOO_SMALL);
			}

			buffer = valuep;
			break;
		}

		/*
		 * Call the PROM function to do the copy.
		 */
		(void) prom_getprop((dnode_t)DEVI(ch_dip)->devi_nodeid,
			name, buffer);
		*lengthp = len;	/* return the actual length to the caller */
		(void) impl_fix_props(dip, ch_dip, name, len, buffer);
		return (DDI_PROP_FOUND_1275);
	}

	return (DDI_PROP_NOT_FOUND);
}


/*
 * Return an integer in native machine format from an OBP 1275 integer
 * representation, which is big-endian, with no particular alignment
 * guarantees.  intp points to the OBP data, and n the number of bytes.
 *
 * Byte-swapping may be needed on some implementations.
 */
int
impl_ddi_prop_int_from_prom(u_char *intp, int n)
{
	int	i = 0;

	ASSERT(n > 0 && n <= 4);

	intp += n;
	while (n-- > 0) {
		i = (i << 8) | *(--intp);
	}

	return (i);
}

/*
 * Someday this add/delete pair might manipulate a list.
 * Today they manipulate a one-item list.
 */
void
impl_bus_add_promops(struct impl_bus_promops *ops_p)
{
	impl_busp = ops_p;
}

/*ARGSUSED*/
void
impl_bus_delete_promops(struct impl_bus_promops *ops_p)
{
	impl_busp = NULL;
}

/*
 * impl_bus_initialprobe
 *	Modload the prom simulator, then let it probe to verify existence
 *	and type of PCI support.
 */
void
impl_bus_initialprobe(void)
{
	(void) modload("misc", "pci_autoconfig");
	if (impl_busp != (struct impl_bus_promops *)NULL) {
		(void) impl_busp->ib_probe();
	}
}

/*
 * This is temporary, but absolutely necessary.  If we are being
 * booted with a device tree created by the DevConf project's bootconf
 * program, then we have device information nodes that reflect
 * reality.  At this point in time in the Solaris release schedule, the
 * kernel drivers aren't prepared for reality.  They still depend on their
 * own ad-hoc interpretations of the properties created when their .conf
 * files were interpreted. These drivers use an "ignore-hardware-nodes"
 * property to prevent them from using the nodes passed up from the bootconf
 * device tree.
 *
 * Trying to assemble root file system drivers as we are booting from
 * devconf will fail if the kernel driver is basing its name_addr's on the
 * psuedo-node device info while the bootpath passed up from bootconf is using
 * reality-based name_addrs.  We help the boot along in this case by
 * looking at the pre-bootconf bootpath and determining if we would have
 * successfully matched if that had been the bootpath we had chosen.
 *
 * Note that we only even perform this extra check if we've booted
 * using bootconf's 1275 compliant bootpath, this is the boot device, and
 * we're trying to match the name_addr specified in the 1275 bootpath.
 */

#define	MAXCOMPONENTLEN	32

int
x86_old_bootpath_name_addr_match(dev_info_t *cdip, char *caddr, char *naddr)
{
	/*
	 *  There are multiple criteria to be met before we can even
	 *  consider allowing a name_addr match here.
	 *
	 *  1) We must have been booted such that the bootconf program
	 *	created device tree nodes and properties.  This can be
	 *	determined by examining the 'bootpath' property.  This
	 *	property will be a non-null string iff bootconf was
	 *	involved in the boot.
	 *
	 *  2) The module that we want to match must be the boot device.
	 *
	 *  3) The instance of the module we are thinking of letting be
	 *	our match must be ignoring hardware nodes.
	 *
	 *  4) The name_addr we want to match must be the name_addr
	 *	specified in the 1275 bootpath.
	 */
	static char bootdev_module[MAXCOMPONENTLEN];
	static char bootdev_oldmod[MAXCOMPONENTLEN];
	static char bootdev_newaddr[MAXCOMPONENTLEN];
	static char bootdev_oldaddr[MAXCOMPONENTLEN];
	static int  quickexit;

	char *daddr;
	int dlen;

	char	*lkupname;
	int	rv = DDI_FAILURE;

	if ((ddi_getlongprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"devconf-addr", (caddr_t)&daddr, &dlen) == DDI_PROP_SUCCESS) &&
	    (ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"ignore-hardware-nodes", -1) != -1)) {
		if (strcmp(daddr, caddr) == 0) {
			return (DDI_SUCCESS);
		}
	}

	if (quickexit)
		return (rv);

	if (bootdev_module[0] == '\0') {
		char *addrp, *eoaddrp;
		char *busp, *modp, *atp;
		char *bp1275, *bp;
		int  bp1275len, bplen;

		bp1275 = bp = addrp = eoaddrp = busp = modp = atp = NULL;

		if (ddi_getlongprop(DDI_DEV_T_ANY,
		    ddi_root_node(), 0, "bootpath",
		    (caddr_t)&bp1275, &bp1275len) != DDI_PROP_SUCCESS ||
		    bp1275len <= 1) {
			/*
			 * We didn't boot from bootconf so we never need to
			 * do any special matches.
			 */
			quickexit = 1;
			if (bp1275)
				kmem_free(bp1275, bp1275len);
			return (rv);
		}

		if (ddi_getlongprop(DDI_DEV_T_ANY,
		    ddi_root_node(), 0, "boot-path",
		    (caddr_t)&bp, &bplen) != DDI_PROP_SUCCESS || bplen <= 1) {
			/*
			 * No fallback position for matching. This is
			 * certainly unexpected, but we'll handle it
			 * just in case.
			 */
			quickexit = 1;
			kmem_free(bp1275, bp1275len);
			if (bp)
				kmem_free(bp, bplen);
			return (rv);
		}

		/*
		 *  Determine boot device module and 1275 name_addr
		 *
		 *  bootpath assumed to be of the form /bus/module@name_addr
		 */
		if (busp = strchr(bp1275, '/')) {
			if (modp = strchr(busp + 1, '/')) {
				if (atp = strchr(modp + 1, '@')) {
					*atp = '\0';
					addrp = atp + 1;
					if (eoaddrp = strchr(addrp, '/'))
						*eoaddrp = '\0';
				}
			}
		}

		if (modp && addrp) {
			(void) strncpy(bootdev_module, modp + 1,
			    MAXCOMPONENTLEN);
			bootdev_module[MAXCOMPONENTLEN - 1] = '\0';

			(void) strncpy(bootdev_newaddr, addrp, MAXCOMPONENTLEN);
			bootdev_newaddr[MAXCOMPONENTLEN - 1] = '\0';
		} else {
			quickexit = 1;
			kmem_free(bp1275, bp1275len);
			kmem_free(bp, bplen);
			return (rv);
		}

		/*
		 *  Determine fallback name_addr
		 *
		 *  10/3/96 - Also save fallback module name because it
		 *  might actually be different than the current module
		 *  name.  E.G., ISA pnp drivers have new names.
		 *
		 *  bootpath assumed to be of the form /bus/module@name_addr
		 */
		addrp = NULL;
		if (busp = strchr(bp, '/')) {
			if (modp = strchr(busp + 1, '/')) {
				if (atp = strchr(modp + 1, '@')) {
					*atp = '\0';
					addrp = atp + 1;
					if (eoaddrp = strchr(addrp, '/'))
						*eoaddrp = '\0';
				}
			}
		}

		if (modp && addrp) {
			(void) strncpy(bootdev_oldmod, modp + 1,
			    MAXCOMPONENTLEN);
			bootdev_module[MAXCOMPONENTLEN - 1] = '\0';

			(void) strncpy(bootdev_oldaddr, addrp, MAXCOMPONENTLEN);
			bootdev_oldaddr[MAXCOMPONENTLEN - 1] = '\0';
		}

		/* Free up the bootpath storage now that we're done with it. */
		kmem_free(bp1275, bp1275len);
		kmem_free(bp, bplen);

		if (bootdev_oldaddr[0] == '\0') {
			quickexit = 1;
			return (rv);
		}
	}

	if (((lkupname = ddi_get_name(cdip)) != NULL) &&
	    (strcmp(bootdev_module, lkupname) == 0 ||
	    strcmp(bootdev_oldmod, lkupname) == 0) &&
	    ((ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"ignore-hardware-nodes", -1) != -1) ||
		ignore_hardware_nodes) &&
	    strcmp(bootdev_newaddr, caddr) == 0 &&
	    strcmp(bootdev_oldaddr, naddr) == 0) {
		rv = DDI_SUCCESS;
	}

	return (rv);
}

int
i_ddi_poke(dev_info_t *dip, size_t size, void *addr, void *val)
{
	return (common_ddi_poke(dip, size, addr, val));
}

int
i_ddi_peek(dev_info_t *dip, size_t size, void *addr, void *val_p)
{
	return (common_ddi_peek(dip, size, addr, val_p));
}

/*
 * Support for allocating DMAable memory to implement
 * ddi_dma_mem_alloc(9F) interface.
 */

#define	MAX_MEM_RANGES	4

static kmutex_t kdmac_lock;
static int kdmacadebug = 0;
static int kdmacused, kdmactotasked, kdmactotused;

static	struct	mem_range {
	u_long		pfn_lo;
	u_long		pfn_hi;
} memranges[] = {
	{0xFFFFF, (u_long)0xFFFFFFFF},	/* pfn range 4G and above */
	{0x80000, 0xFFFFF},	/* pfn range for 2G-4G */
	{0x01000, 0x7FFFF},	/* pfn range for 16M-2G */
	{0x00000, 0x00FFF},	/* pfn range for 0-16M */
};

/* number of memory ranges */
static int nranges = sizeof (memranges)/sizeof (struct mem_range);

/* indexed by memory range and highbit(size-1) */
static struct kmem_cache *kmem_io_table[MAX_MEM_RANGES][PAGESHIFT + 1];
static struct kmem_backend *kmem_io_backend[MAX_MEM_RANGES];

static void *kmem_getpages_io_above_4g(size_t, u_int);
static void *kmem_getpages_io_below_4g(size_t, u_int);
static void *kmem_getpages_io_below_2g(size_t, u_int);
static void *kmem_getpages_io_below_16m(size_t, u_int);
static int memrange_num(u_long);
static void *kmem_alloc_io(size_t, int, int, int, ddi_dma_attr_t *);
static void kmem_free_io(void *, size_t, int);

static void *(* kmem_getpages_io_funcs[MAX_MEM_RANGES])(size_t, u_int) = {
		kmem_getpages_io_above_4g,
		kmem_getpages_io_below_4g,
		kmem_getpages_io_below_2g,
		kmem_getpages_io_below_16m,
};

/*
 * Allocate from the system, aligned on a specific boundary.
 *
 * It is assumed that alignment, if non-zero, is a simple power of two.
 *
 * It is also assumed that the power of two alignment is
 * less than or equal to the system's pagesize.
 */
static caddr_t
kalloca(u_int nbytes,
	int align,
	int cansleep,
	int memflags,
	ddi_dma_attr_t *attr)
{
	uintptr_t addr, raddr, alloc, *saddr;

	if (kdmacadebug) {
		printf("kalloca: request is %d%%%d slp=%d physcontig=%d\n",
		    nbytes, align, cansleep, memflags);
	}

	/*
	 * Validate and fiddle with alignment
	 */

	if (align < 16)
		align = 16;
	if (align & (align - 1))
		return ((caddr_t)0);

	/*
	 * Round up the allocation to an alignment boundary,
	 * and add in padding for both alignment and storage
	 * of the 'real' address and length.
	 */

	alloc = roundup(nbytes, align) + (sizeof (uintptr_t *) * 4) + align;

	/*
	 * Allocate the requested space from the desired space with NO WAIT.
	 */
	addr = (uintptr_t)kmem_alloc_io(alloc, KM_NOSLEEP, memflags,
		align, attr);

	if (addr == NULL) {
		/*
		 * Failed to allocate it from the kmem backend page
		 * cache. Try to allocate it from lomem pool. We don't
		 * use any padding if we are allocating it from the
		 * lomem pool.
		 */
		addr = (uintptr_t)lomem_alloc(nbytes, attr, align, 0);

		if ((addr != NULL) || (cansleep == 0))
			return ((caddr_t)addr);
		/*
		 * Failed to allocate it from the kmem backend page cache
		 * as well as lomempage pool. Wait for the kmem backend
		 * page allocator or lomem allocator to find something
		 * because the caller wants to wait.
		 */
		if ((roundup(nbytes, align) >= PAGESIZE) && (memflags != 0))
			/*
			 * We need contiguous buffer of more than a page.
			 * Currently we can get it only from the lomem pool,
			 * so wait for lomem_alloc to find something.
			 */
			return (lomem_alloc(nbytes, attr, align, 1));

		/* we wait for kmem cache allocator to find something */
		addr = (uintptr_t)
			kmem_alloc_io(alloc, KM_SLEEP, memflags, align, attr);
	}

	raddr = addr;

	if (addr) {
		mutex_enter(&kdmac_lock);
		kdmactotasked += nbytes;
		kdmactotused += alloc;
		kdmacused += alloc;
		mutex_exit(&kdmac_lock);
		if (addr & (align - 1)) {
			addr += 16;
			addr = roundup(addr, align);
		} else
			addr += align;

		saddr = (uintptr_t *)addr;
		saddr[-1] = raddr;
		saddr[-2] = alloc;
		saddr[-3] = memflags;
		/* save the memrange index to use it in kmem_free_io() */
		saddr[-4] = memrange_num(btop(attr->dma_attr_addr_hi));
	}

	if (kdmacadebug) {
		printf("kalloca: %d%%%d from heap got %x.%x returns %x\n",
		    nbytes, align, (int)raddr,
		    (int)(raddr + alloc), (int)addr);
	}

	return ((caddr_t)addr);
}

static void
kfreea(caddr_t addr)
{
	uintptr_t *saddr, raddr;
	u_int nbytes;
	int rindex;
	extern int islomembuf(void *);

	if (islomembuf((void *)addr)) {
		/* buffer is allocated from lomem pages */
		lomem_free(addr);
		return;
	}

	/* get the real address and size */
	saddr = (uintptr_t *)(((uintptr_t)addr) & ~0x3);
	raddr = saddr[-1];
	nbytes = (u_int)saddr[-2];
	rindex = saddr[-4];
	if (kdmacadebug) {
		printf("kfreea: freeing %p (real %lx.%lx) from heap area\n",
		    (void *)addr, raddr, raddr + nbytes);
	}
	mutex_enter(&kdmac_lock);
	kdmacused -= nbytes;
	mutex_exit(&kdmac_lock);

	kmem_free_io((caddr_t)raddr, nbytes, rindex);
}

/*ARGSUSED*/
static void *
kmem_alloc_io(size_t size, int flags, int memflags, int align,
	ddi_dma_attr_t *attr)
{
	int index = highbit(size - 1);
	int num;
	char name1[40];
	char name2[40];

	num = memrange_num(btop(attr->dma_attr_addr_hi));

	/*
	 * create page cache list if it is not already created.
	 */
	if ((size <= PAGESIZE) && (kmem_io_table[num][index] == NULL)) {
		(void) sprintf(name1, "kmem_alloc_io_%d_%d", num, 1 << index);
		mutex_enter(&kdmac_lock);
		if (kmem_io_backend[num] == NULL) {
			(void) sprintf(name2, "kmem_dma_memory_backend_%d",
				num);
			kmem_io_backend[num] = kmem_backend_create(
				name2, kmem_getpages_io_funcs[num],
				kmem_freepages, PAGESIZE, KMEM_CLASS_WIRED);
		}
		if (kmem_io_table[num][index] == NULL)
			kmem_io_table[num][index] = kmem_cache_create(name1,
			    1 << index, 1 << index, NULL, NULL, NULL,
			    NULL, kmem_io_backend[num], 0);
		mutex_exit(&kdmac_lock);
	}

	if (size <= PAGESIZE) {
		/* Allocate it from the backend page cache */
		return (kmem_cache_alloc(kmem_io_table[num][index], flags));
	}

	if (memflags == 0) {	/* (size > PAGESIZE) && !physcontg */
		/*
		 * create kmem backend page allocator if it is not
		 * already done.
		 */
		mutex_enter(&kdmac_lock);
		if (kmem_io_backend[num] == NULL) {
			(void) sprintf(name2, "kmem_dma_memory_backend_%d",
				num);
			kmem_io_backend[num] = kmem_backend_create(
				name2, kmem_getpages_io_funcs[num],
				kmem_freepages, PAGESIZE, KMEM_CLASS_WIRED);
		}
		mutex_exit(&kdmac_lock);

		/* Allocate using backend page allocator */
		return (kmem_backend_alloc(kmem_io_backend[num],
					size, flags));
	}

	return (NULL);
}

static void
kmem_free_io(void *buf, size_t size, int rindex)
{
	ASSERT(rindex < nranges);

	if (size > PAGESIZE)
		kmem_backend_free(kmem_io_backend[rindex], buf, size);
	else
		kmem_cache_free(kmem_io_table[rindex][highbit(size - 1)], buf);
}

/* returns the index of the highest memory range for this pfn */
static int
memrange_num(pfn_t pfn)
{
	int n = 0;

	while (n < nranges) {
		if (memranges[n].pfn_hi <= pfn)
			return (n);
		n++;
	}

	return (0);
}

/* get pages from above 4G (i.e almost any where) */
void *
kmem_getpages_io_above_4g(size_t npages, u_int flag)
{
	/*
	 * dummy DMA attribute structure; we care only the following:
	 *	dma_attr_addr_lo
	 *	dma_attr_addr_hi
	 *	dma_attr_align
	 */
	static ddi_dma_attr_t dma_attr_above_4g = {
		DMA_ATTR_V0,
		(unsigned long long)0,			/* dma_attr_addr_lo */
		0x00000fffffffffffLL,			/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,					/* dma_attr_align */
		1,
		1,
		0xffffffffULL,
		0xffffffffULL,
		0x1,
		1,
		0
	};

	if (kdmacadebug)
		cmn_err(CE_CONT,
		    "?kmem_getpages_io_above_4g: need %lu pages\n", npages);

	return (kmem_getpages_io(npages, flag, 0, &dma_attr_above_4g));
}

/* get pages from below 4G */
void *
kmem_getpages_io_below_4g(size_t npages, u_int flag)
{
	/*
	 * dummy DMA attribute structure; we care only the following:
	 *	dma_attr_addr_lo
	 *	dma_attr_addr_hi
	 *	dma_attr_align
	 */
	static ddi_dma_attr_t dma_attr_below_4g = {
		DMA_ATTR_V0,
		(unsigned long long)0,			/* dma_attr_addr_lo */
		0xffffffffULL,				/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,					/* dma_attr_align */
		1,
		1,
		0xffffffffULL,
		0xffffffffULL,
		0x1,
		1,
		0
	};

	if (kdmacadebug)
		cmn_err(CE_CONT,
		    "?kmem_getpages_io_below_4g: need %lu pages\n", npages);

	return (kmem_getpages_io(npages, flag, 0, &dma_attr_below_4g));
}

/* get pages from below 2G */
void *
kmem_getpages_io_below_2g(size_t npages, u_int flag)
{
	/*
	 * dummy DMA attribute structure; we care only the following:
	 *	dma_attr_addr_lo
	 *	dma_attr_addr_hi
	 *	dma_attr_align
	 */
	static ddi_dma_attr_t dma_attr_below_2g = {
		DMA_ATTR_V0,
		(unsigned long long)0x0,		/* dma_attr_addr_lo */
		(unsigned long long)0x7fffffff,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,					/* dma_attr_align */
		1,
		1,
		0xffffffffULL,
		0xffffffffULL,
		0x1,
		1,
		0
	};

	if (kdmacadebug)
		cmn_err(CE_CONT,
		    "?kmem_getpages_io_below_2g: need %lu pages\n", npages);

	return (kmem_getpages_io(npages, flag, 0, &dma_attr_below_2g));
}

/* get pages from below 16M */
void *
kmem_getpages_io_below_16m(size_t npages, u_int flag)
{
	/*
	 * dummy DMA attribute structure; we care only the following:
	 *	dma_attr_addr_lo
	 *	dma_attr_addr_hi
	 *	dma_attr_align
	 */
	static ddi_dma_attr_t dma_attr_below_16m = {
		DMA_ATTR_V0,
		(unsigned long long)0,			/* dma_attr_addr_lo */
		(unsigned long long)0x00ffffff,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,					/* dma_attr_align */
		1,
		1,
		0xffffffffULL,
		0xffffffffULL,
		0x1,
		1,
		0
	};

	if (kdmacadebug)
		cmn_err(CE_CONT,
		    "?kmem_getpages_io_below_16m: need %lu pages\n", npages);

	return (kmem_getpages_io(npages, flag, 0, &dma_attr_below_16m));
}
