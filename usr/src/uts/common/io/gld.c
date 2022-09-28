/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gld.c	1.46	97/11/25 SMI"

/*
 * gld - Generic LAN Driver
 *
 * This is a utility module that provides generic facilities for
 * LAN	drivers.  The DLPI protocol and most STREAMS interfaces
 * are handled here.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ksynch.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/debug.h>

#define	BROKEN_PCMCIA 1

/*
 * x86 should use instance number also, but I think it's too risky
 * to change this so late in the 2.6 cycle.
 */
#if defined(i386)
	/* Old style PPA assignment in order of gld_register */
#else
#define	PPA_IS_INSTANCE 1	/* use instance number for PPA */
#endif

#ifdef	_DDICT

/*
 * This is not a strictly a driver, so ddict is not strictly
 * applicable, but we have run it anyway to improve somewhat
 * the cleanliness of this "misc" module.
 */

/* from sys/errno.h */
#define	ENOSR	63	/* out of streams resources		*/

/* from sys/modctl.h */
extern struct mod_ops mod_miscops;

/* from sys/time.h included in sys/kstat.h */
typedef	struct	_timespec {
	time_t		tv_sec;
	long		tv_nsec;
} timespec_t;
typedef	struct _timespec  timestruc_t;
extern	timestruc_t	hrestime;

/* from sys/strsun.h */
#define	DB_TYPE(mp)		((mp)->b_datap->db_type)

#include "sys/dlpi.h"
#include "sys/ethernet.h"
#include "sys/gld.h"
#include "sys/gldpriv.h"

#else	/* not _DDICT */

#include <sys/byteorder.h>
#include <sys/strsun.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/gld.h>
#include <sys/gldpriv.h>

#endif	/* not _DDICT */

#include <sys/ddi.h>
#include <sys/sunddi.h>

#ifdef GLD_DEBUG
int	gld_debug = 0;
#endif

/*
 * function prototypes, etc.
 */
int gld_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred);
int gld_close(queue_t *q, int flag, cred_t *cred);
int gld_wput(queue_t *q, mblk_t *mp);
int gld_wsrv(queue_t *q);
int gld_rsrv(queue_t *q);
int	gld_update_kstat(kstat_t *, int);
u_int	gld_intr();
int gld_sched(gld_mac_info_t *macinfo);

static int gld_start(queue_t *q, mblk_t *mp, int lockflavor, int);
static void gld_fastpath(gld_t *gld, queue_t *q, mblk_t *mp);
static void gldinsque(void *elem, void *pred);
static void gldremque(void *arg);
static void gld_flushqueue(queue_t *q);
#ifndef	PPA_IS_INSTANCE
static int gld_findppa(glddev_t *device);
#endif
static int gld_findminor(glddev_t *device);
static void gld_send_disable_multi(gld_t *gld, gld_mac_info_t *macinfo,
	gld_mcast_t *mcast);
static int gld_physaddr(queue_t *q, mblk_t *mp);
static int gld_bind(queue_t *q, mblk_t *mp);
static int gld_cmds(queue_t *q, mblk_t *mp);
static int gld_promisc(queue_t *q, mblk_t *mp, int on);
static mblk_t *gld_addudind(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo);

static int gld_setaddr(queue_t *q, mblk_t *mp);
static int gld_disable_multi(queue_t *q, mblk_t *mp);
static int gld_enable_multi(queue_t *q, mblk_t *mp);
static glddev_t *gld_devlookup(int major);
static int gldattach(queue_t *q, mblk_t *mp);
static void gld_initstats(gld_mac_info_t *macinfo);
static int gldunattach(queue_t *q, mblk_t *mp);
static int gld_unbind(queue_t *q, mblk_t *mp);
static int gld_unitdata(queue_t *q, mblk_t *mp);
static int gld_inforeq(queue_t *q, mblk_t *mp);
static int gld_multicast(mac_addr_t macaddr, gld_t *gld);
static int gld_paccept(gld_t *gld, pktinfo_t *pktinfo);
static int gld_accept(gld_t *gld, pktinfo_t *pktinfo);
static int gld_precv(gld_mac_info_t *macinfo, mblk_t *mp, int);
static int gld_sendup(gld_mac_info_t *macinfo, mblk_t *mp, int (*)(), int);
static void gld_passon(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo,
	void (*send)(queue_t *qp, mblk_t *mp));
int gld_recv(gld_mac_info_t *macinfo, mblk_t *mp);
static int gld_mcmatch(gld_t *gld, pktinfo_t *pktinfo);
void gld_bconvcopy(caddr_t src, caddr_t target, size_t n);

#ifdef _PRE_SOLARIS_2_6
extern void dlphysaddrack(queue_t *, mblk_t *, caddr_t, int);
extern int dluderrorind(queue_t *, mblk_t *, u_char *, u_long, u_long, int);
extern void dlokack(queue_t *, mblk_t *, u_long);
extern void dlbindack(queue_t *, mblk_t *, u_long, u_char *, int, int, u_long);
extern void dlerrorack(queue_t *, mblk_t *, u_long, u_long, u_long);
#endif

/*
 * Allocate and zero-out "number" structures each of type "structure" in
 * kernel memory.
 */
#define	GETSTRUCT(structure, number)   \
	((structure *) kmem_zalloc(\
		(u_int) (sizeof (structure) * (number)), KM_NOSLEEP))

#define	abs(a) ((a) < 0 ? -(a) : a)

uchar_t gldbroadcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * DLPI media types supported
 */
static char *gld_types[] = {
	"csmacd",	/* DL_CSMACD	 IEEE 802.3 CSMA/CD network */
	"tpb",		/* DL_TPB	 IEEE 802.4 Token Passing Bus */
	"tpr",		/* DL_TPR	 IEEE 802.5 Token Passing Ring */
	"metro",	/* DL_METRO	 IEEE 802.6 Metro Net */
	"ether",	/* DL_ETHER	 Ethernet Bus */
	"hdlc",		/* DL_HDLC	 ISO HDLC protocol support */
	"char",		/* DL_CHAR	 Character Synchronous protocol */
	"ctca",		/* DL_CTCA	 IBM Channel-to-Channel Adapter */
	"fddi",		/* DL_FDDI	 Fiber Distributed data interface */
	"other",	/* DL_OTHER	 Any other medium not listed above */
};

static char *gld_media[] = {
	"unknown",
	"aui",
	"bnc",
	"twpair",
	"fiber",
	"100baseT",
	"100vgAnyLan",
	"10baseT",
	"ring4",
	"ring16",
	"MII/PHY",
};

int gld_interpret_ether(gld_mac_info_t *, mblk_t *, pktinfo_t *);
int gld_interpret_fddi(gld_mac_info_t *, mblk_t *, pktinfo_t *);
int gld_interpret_tr(gld_mac_info_t *, mblk_t *, pktinfo_t *);

mblk_t *gld_fastpath_ether(gld_t *, mblk_t *);
mblk_t *gld_fastpath_fddi(gld_t *, mblk_t *);
mblk_t *gld_fastpath_tr(gld_t *, mblk_t *);

mblk_t *gld_unitdata_ether(gld_t *, mblk_t *);
mblk_t *gld_unitdata_fddi(gld_t *, mblk_t *);
mblk_t *gld_unitdata_tr(gld_t *, mblk_t *);

void gld_init_ether(gld_mac_info_t *);
void gld_init_fddi(gld_mac_info_t *);
void gld_init_tr(gld_mac_info_t *);

void gld_uninit_ether(gld_mac_info_t *);
void gld_uninit_fddi(gld_mac_info_t *);
void gld_uninit_tr(gld_mac_info_t *);

gld_interface_t interfaces[] = {

	/* Ethernet Bus */
	{
		DL_ETHER, 1514,
		gld_interpret_ether, gld_fastpath_ether, gld_unitdata_ether,
		gld_init_ether, gld_uninit_ether,
		IF_HDR_FIXED
	},

	/* Fiber Distributed data interface */
	{
		DL_FDDI, 4483,
		gld_interpret_fddi, gld_fastpath_fddi, gld_unitdata_fddi,
		gld_init_fddi, gld_uninit_fddi,
		IF_HDR_VAR
	},

	/* Token Ring interface */
	{
		DL_TPR, 17800,
		gld_interpret_tr, gld_fastpath_tr, gld_unitdata_tr,
		gld_init_tr, gld_uninit_tr,
		IF_HDR_VAR
	},

};

/*
 * bit reversal lookup table.
 */
static	uchar_t bit_rev[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0,
	0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4,
	0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
	0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca,
	0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6,
	0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1,
	0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9,
	0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
	0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3,
	0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7,
	0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf,
	0x3f, 0xbf, 0x7f, 0xff,
};

static struct glddevice gld_device_list;  /* Per-system root of GLD tables */

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modlmisc = {
	&mod_miscops,		/* Type of module - a utility provider */
	"Generic LAN Driver Utilities",
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlmisc, NULL
};


int
_init(void)
{
	register int e;


	/* initialize gld_device_list mutex */
	rw_init(&gld_device_list.gld_rwlock, NULL, RW_DRIVER, NULL);

	/* initialize device driver (per-major) list */
	gld_device_list.gld_next =
	    gld_device_list.gld_prev = &gld_device_list;
	e = mod_install(&modlinkage);
	if (e != 0) {
		rw_destroy(&gld_device_list.gld_rwlock);
	}
	return (e);
}

int
_fini(void)
{
	register int e;

	e = mod_remove(&modlinkage);
	if (e == 0) {
		/* remove all mutex and locks */
		rw_destroy(&gld_device_list.gld_rwlock);
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * GLD service routines
 */

/*
 * gld_register -- called once per device instance (PPA)
 *
 * During its attach routine, a real device driver will register with GLD
 * so that later opens and dl_attach_reqs will work.  The arguments are the
 * devinfo pointer, the device name, and a macinfo structure describing the
 * physical device instance.
 *
 * This routine makes the unwarrented assumption that it is not called
 * simultaneously for multiple instances of the same major number.
 */
int
gld_register(dev_info_t *devinfo, char *devname, gld_mac_info_t *macinfo)
{
	int nintrs = 0, nregs = 0;
	int mediatype = 0;
	char *media;
	int major = ddi_name_to_major(devname), i;
	glddev_t *glddev = gld_devlookup(major);
	char minordev[32];

	/*
	 *  Allocate per-driver (major) data structure if necessary
	 */
	if (glddev == NULL) {
		/* first occurrence of this device name (major number) */
		glddev = GETSTRUCT(glddev_t, 1);
		if (glddev == NULL)
			return (DDI_FAILURE);
		(void) strcpy(glddev->gld_name, devname);
		glddev->gld_major = major;
		glddev->gld_mac_next = glddev->gld_mac_prev =
			(gld_mac_info_t *)&glddev->gld_mac_next;
		glddev->gld_str_next = glddev->gld_str_prev =
			(gld_t *)&glddev->gld_str_next;
		/*
		 * create the file system device node
		 */
		if (ddi_create_minor_node(devinfo, devname, S_IFCHR,
					0, DDI_NT_NET, 0) == DDI_FAILURE) {
			cmn_err(CE_WARN, "GLD: %s%d:  "
			    "ddi_create_minor_node failed",
			    ddi_get_name(devinfo), ddi_get_instance(devinfo));
			kmem_free(glddev, sizeof (glddev_t));
			return (DDI_FAILURE);
		}
		rw_init(&glddev->gld_rwlock, NULL, RW_DRIVER, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldinsque(glddev, gld_device_list.gld_prev);
		rw_exit(&gld_device_list.gld_rwlock);
		glddev->gld_multisize = ddi_getprop(DDI_DEV_T_NONE,
		    devinfo, 0, "multisize", GLD_MAX_MULTICAST);
	}
	glddev->gld_ndevice++;

	/*
	 *  Per-instance initialization
	 */

	/* add interrupt handler */
	if (macinfo->gldm_intr != NULL &&
#ifdef	BROKEN_PCMCIA
	    macinfo->gldm_irq_index >= 0 &&
#endif
	    ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS &&
	    macinfo->gldm_irq_index >= 0 && macinfo->gldm_irq_index < nintrs) {
		if (ddi_intr_hilevel(devinfo, macinfo->gldm_irq_index)) {
			cmn_err(CE_WARN, "GLD: %s: hi level interrupt",
				    devname);
			goto failure;
		}
		if (ddi_add_intr(devinfo, macinfo->gldm_irq_index,
		    &macinfo->gldm_cookie, NULL, gld_intr,
		    (caddr_t)macinfo) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  XXX 'goto' ifdeffed out until all gld dependent drivers have
	 *  been modified to correctly handle new error checking logic.
	 *
	 */
			goto failure;
#endif
		}
	}

	/* map the device memory */
	/* **** This code bogusly assumes only one memory address range **** */
	if (
#ifdef	BROKEN_PCMCIA
	    macinfo->gldm_reg_index >= 0 &&
#endif
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS &&
	    macinfo->gldm_reg_index >= 0 && macinfo->gldm_reg_index < nregs) {
		if (ddi_map_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  XXX 'goto' ifdeffed out until all gld dependent drivers have
	 *  been modified to correctly handle new error checking logic.
	 *
	 */
			goto late_failure;
#endif
		}
	}

	/* Initialize per-instance data structures */
	mediatype =
	    (macinfo->gldm_type < (sizeof (gld_types)/sizeof (char *))) ?
		macinfo->gldm_type : DL_OTHER;
	if (macinfo->gldm_media > (sizeof (gld_media) / sizeof (char *)))
		macinfo->gldm_media = GLDM_UNKNOWN;
	media = gld_media[macinfo->gldm_media];

	/*
	 * Set up interface pointer. These are device class specific pointers
	 * used to handle FDDI/TR/ETHER specific packets.
	 *
	 * Do not allocate set the pointer in the glddev
	 *
	 */
	for (i = 0; i < sizeof (interfaces)/sizeof (*interfaces); i++)
		if (mediatype == interfaces[i].mac_type) {
			macinfo->gldm_interface = (caddr_t)
				kmem_zalloc(sizeof (gld_interface_pvt_t),
						KM_SLEEP);
			if (!macinfo->gldm_interface)
				break;
			((gld_interface_pvt_t *)macinfo->gldm_interface)->
				interfacep = &interfaces[i];
			break;
		}

	if (!macinfo->gldm_interface)
		goto late_late_failure;

	if (macinfo->gldm_ident == NULL)
		macinfo->gldm_ident = "LAN Driver";

	macinfo->gldm_dev = glddev;
	if (!(macinfo->gldm_options & GLDOPT_DRIVER_PPA))
#ifndef	PPA_IS_INSTANCE
		macinfo->gldm_ppa = gld_findppa(glddev);
#else
		macinfo->gldm_ppa = ddi_get_instance(devinfo);
#endif
	(void) sprintf(minordev, "%s%ld", devname, macinfo->gldm_ppa);
	(void) ddi_create_minor_node(devinfo, minordev, S_IFCHR,
				macinfo->gldm_ppa + 1, DDI_NT_NET, 0);
	macinfo->gldm_devinfo = devinfo;

	mutex_init(&macinfo->gldm_maclock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);

	gld_initstats(macinfo);		/* calls ifp->init */
	ddi_set_driver_private(devinfo, (caddr_t)macinfo);

	glddev->gld_type = macinfo->gldm_type;
	glddev->gld_minsdu = macinfo->gldm_minpkt;
	/*
	 * set token ring mtu bit pattern based on the value.
	 */
	glddev->gld_maxsdu = macinfo->gldm_maxpkt;

	/* add ourselves to this major device's linked list of instances */
	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	gldinsque(macinfo, glddev->gld_mac_prev);
	rw_exit(&glddev->gld_rwlock);

	/*
	 * need to indicate we are NOW ready to process interrupts
	 * any interrupt before this is set is for someone else
	 */
	macinfo->gldm_GLD_flags |= GLD_INTR_READY;

	/* log local ethernet address */
	(void) localetheraddr((struct ether_addr *)macinfo->gldm_macaddr, NULL);

	/* now put announcement into the message buffer */

#if defined(GLD_DEBUG)
	cmn_err(CE_CONT, "%s%ld (@0x%x): %s: %s (%s) %s\n", devname,
		macinfo->gldm_ppa,
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
			"ioaddr", 0),
		macinfo->gldm_ident,
		gld_types[mediatype], media,
		ether_sprintf((struct ether_addr *)macinfo->gldm_macaddr));
#else
	cmn_err(CE_CONT, "!%s%ld (@0x%x): %s: %s (%s) %s\n", devname,
		macinfo->gldm_ppa,
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
			"ioaddr", 0),
		macinfo->gldm_ident,
		gld_types[mediatype], media,
		ether_sprintf((struct ether_addr *)macinfo->gldm_macaddr));
#endif

	ddi_report_dev(devinfo);
	return (DDI_SUCCESS);

late_late_failure:
	    if (macinfo->gldm_reg_index >= 0 && macinfo->gldm_reg_index < nregs)
		ddi_unmap_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len);

late_failure:
	if (macinfo->gldm_intr != NULL && macinfo->gldm_irq_index >= 0 &&
	    macinfo->gldm_irq_index < nintrs)
		ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
				macinfo->gldm_cookie);

failure:
	glddev->gld_ndevice--;
	if (glddev->gld_ndevice == 0) {
		ddi_remove_minor_node(devinfo, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldremque(glddev);
		rw_exit(&gld_device_list.gld_rwlock);
		rw_destroy(&glddev->gld_rwlock);
		kmem_free(glddev, sizeof (glddev_t));
	}

	return (DDI_FAILURE);

}

/*
 * gld_unregister (macinfo)
 * remove the macinfo structure from local structures
 * this is cleanup for a driver to be unloaded
 */
int
gld_unregister(gld_mac_info_t *macinfo)
{
	glddev_t *glddev = macinfo->gldm_dev;
	dev_info_t *devinfo = macinfo->gldm_devinfo;
	int nintrs, nregs;
	gld_interface_t *ifp;
	int multisize = sizeof (gld_mcast_t) * glddev->gld_multisize;

#ifdef DEBUG
	/* XXX -- Ensure no streams are open on this PPA */
#endif

	/* remove the interrupt handler */
	if (macinfo->gldm_intr != NULL &&
#ifdef	BROKEN_PCMCIA
	    macinfo->gldm_irq_index >= 0 &&
#endif
	    ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS &&
	    macinfo->gldm_irq_index >= 0 && macinfo->gldm_irq_index < nintrs)
		ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
		    macinfo->gldm_cookie);

	/* unmap the device memory */
	if (
#ifdef	BROKEN_PCMCIA
	    macinfo->gldm_reg_index >= 0 &&
#endif
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS &&
	    macinfo->gldm_memp != NULL &&
	    macinfo->gldm_reg_index >= 0 && macinfo->gldm_reg_index < nregs)
		ddi_unmap_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len);

	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	gldremque(macinfo);
	rw_exit(&glddev->gld_rwlock);

	/* destroy the mutex for interrupt locking */
	mutex_destroy(&macinfo->gldm_maclock);

	kstat_delete(macinfo->gldm_kstatp);
	macinfo->gldm_kstatp = NULL;

	ddi_set_driver_private(devinfo, (caddr_t)NULL);

	/* We now have one fewer instance for this major device */
	glddev->gld_ndevice--;
	if (glddev->gld_ndevice == 0) {
		ddi_remove_minor_node(macinfo->gldm_devinfo, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldremque(glddev);
		rw_exit(&gld_device_list.gld_rwlock);
		rw_destroy(&glddev->gld_rwlock);
		kmem_free(glddev, sizeof (glddev_t));
	}
	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;

	(*ifp->uninit)(macinfo);

	kmem_free(macinfo->gldm_interface, sizeof (gld_interface_pvt_t));
	macinfo->gldm_interface = (caddr_t)NULL;
	if (macinfo->gldm_mcast != NULL)
		kmem_free(macinfo->gldm_mcast, multisize);
	macinfo->gldm_mcast = NULL;
	return (DDI_SUCCESS);
}

/*
 * gld_initstats
 */
static void
gld_initstats(gld_mac_info_t *macinfo)
{
	struct gldkstats *sp;
	glddev_t *glddev;
	kstat_t *ksp;
	gld_interface_t *ifp;

	glddev = macinfo->gldm_dev;

	if ((ksp = kstat_create(glddev->gld_name, macinfo->gldm_ppa,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct gldkstats) / sizeof (kstat_named_t),
	    (uchar_t)KSTAT_FLAG_VIRTUAL)) == NULL) {
		cmn_err(CE_WARN, "failed to create kstat structure for %s",
		    glddev->gld_name);
		return;
	}
	macinfo->gldm_kstatp = ksp;

	ksp->ks_update = gld_update_kstat;
	ksp->ks_private = (void *)macinfo;

	ksp->ks_data = (void *)&macinfo->gldm_kstats;

	sp = &macinfo->gldm_kstats;
	kstat_named_init(&sp->glds_pktrcv, "ipackets", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_pktxmt, "opackets", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_errrcv, "ierrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_errxmt, "oerrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_bytexmt, "obytes", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_bytercv, "rbytes", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_multixmt, "multixmt", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_multircv, "multircv", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_brdcstxmt, "brdcstxmt", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_brdcstrcv, "brdcstrcv", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_blocked, "blocked", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_noxmtbuf, "noxmtbuf", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_norcvbuf, "norcvbuf", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_xmtretry, "xmtretry", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_intr, "intr", KSTAT_DATA_LONG);

	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;

	(*ifp->init)(macinfo);

	kstat_install(ksp);
}

gld_update_kstat(kstat_t *ksp, int rw)
{
	gld_mac_info_t *macinfo;
	struct gldkstats *gsp;
	struct gld_stats *stats;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	macinfo = (gld_mac_info_t *)ksp->ks_private;
	ASSERT(macinfo != NULL);

	gsp = &macinfo->gldm_kstats;
	stats = &macinfo->gldm_stats;

	mutex_enter(&macinfo->gldm_maclock);

	if (macinfo->gldm_gstat)
		(*macinfo->gldm_gstat)(macinfo);

	gsp->glds_pktxmt.value.ul = stats->glds_pktxmt;
	gsp->glds_pktrcv.value.ul = stats->glds_pktrcv;
	gsp->glds_errxmt.value.ul = stats->glds_errxmt;
	gsp->glds_errrcv.value.ul = stats->glds_errrcv;
	gsp->glds_collisions.value.ul = stats->glds_collisions;
	gsp->glds_bytexmt.value.ul = stats->glds_bytexmt;
	gsp->glds_bytercv.value.ul = stats->glds_bytercv;
	gsp->glds_multixmt.value.ul = stats->glds_multixmt;
	gsp->glds_multircv.value.ul = stats->glds_multircv;
	gsp->glds_brdcstxmt.value.ul = stats->glds_brdcstxmt;
	gsp->glds_brdcstrcv.value.ul = stats->glds_brdcstrcv;
	gsp->glds_blocked.value.ul = stats->glds_blocked;
	gsp->glds_excoll.value.ul = stats->glds_excoll;
	gsp->glds_defer.value.ul = stats->glds_defer;
	gsp->glds_frame.value.ul = stats->glds_frame;
	gsp->glds_crc.value.ul = stats->glds_crc;
	gsp->glds_overflow.value.ul = stats->glds_overflow;
	gsp->glds_underflow.value.ul = stats->glds_underflow;
	gsp->glds_short.value.ul = stats->glds_short;
	gsp->glds_missed.value.ul = stats->glds_missed;
	gsp->glds_xmtlatecoll.value.ul = stats->glds_xmtlatecoll;
	gsp->glds_nocarrier.value.ul = stats->glds_nocarrier;
	gsp->glds_noxmtbuf.value.ul = stats->glds_noxmtbuf;
	gsp->glds_norcvbuf.value.ul = stats->glds_norcvbuf;
	gsp->glds_intr.value.ul = stats->glds_intr;
	gsp->glds_xmtretry.value.ul = stats->glds_xmtretry;

	mutex_exit(&macinfo->gldm_maclock);
	return (0);
}

/*
 * gld_open (q, dev, flag, sflag, cred)
 * generic open routine.  Hardware open will call this. The
 * hardware open passes in the gldevice structure (one per device class) as
 * well as all of the normal open parameters.
 */
/*ARGSUSED2*/
int
gld_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	gld_t  *gld;
	ushort	minordev;
	long ppa;
	glddev_t *glddev;
	gld_mac_info_t *mac;

	ASSERT(q);

	/* Find our per-major glddev_t structure */
	glddev = gld_devlookup(getmajor(*dev));
	ppa = getminor(*dev) & GLD_PPA_MASK;
	if (glddev == NULL)
		return (ENXIO);

	ASSERT(q->q_ptr == NULL);	/* Clone device gives us a fresh Q */

	/*
	 * get a per-stream structure and link things together so we
	 * can easily find them later.
	 */
	if ((gld = (gld_t *)kmem_zalloc(sizeof (gld_t), KM_SLEEP)) == NULL)
		return (ENOSR);

	/*
	 * fill in the structure and state info
	 */
	gld->gld_state = DL_UNATTACHED;
	if (ppa == GLD_USE_STYLE2) {
		gld->gld_style = DL_STYLE2;
	} else {
		gld->gld_style = DL_STYLE1;
		/* the PPA is actually 1 less than the minordev */
		ppa--;
		for (mac = glddev->gld_mac_next;
		    mac != (gld_mac_info_t *)(&glddev->gld_mac_next);
		    mac = mac->gldm_next) {
			ASSERT(mac);
			if (mac->gldm_ppa == ppa) {
				/*
				 * we found the correct PPA
				 */
				gld->gld_mac_info = mac;

				/* now ready for action */
				gld->gld_state = DL_UNBOUND;

				gld->gld_stats = &mac->gldm_stats;
				mutex_enter(&mac->gldm_maclock);
				if (mac->gldm_nstreams == 0) {
					/* reset and setup */
					(*mac->gldm_reset) (mac);
					/* now make sure it is running */
					(*mac->gldm_start) (mac);
				}
				mac->gldm_nstreams++;
				mutex_exit(&mac->gldm_maclock);
				break;
			}
		}
		if (gld->gld_state == DL_UNATTACHED) {
			kmem_free(gld, sizeof (gld_t));
			return (ENXIO);
		}
	}

	/*
	 * Serialize access through open/close this will serialize across all
	 * gld devices, but open and close are not frequent so should not
	 * induce much, if any delay.
	 */
	rw_enter(&glddev->gld_rwlock, RW_WRITER);

	/* find a free minor device number(per stream) and clone our dev */
	minordev = gld_findminor(glddev);
	*dev = makedevice(getmajor(*dev), minordev);

	gld->gld_qptr = q;
	WR(q)->q_ptr = q->q_ptr = (caddr_t)gld;
	gld->gld_minor = minordev;
	gld->gld_device = glddev;
	gldinsque(gld, glddev->gld_str_prev);

	rw_exit(&glddev->gld_rwlock);
	qprocson(q);		/* start the queues running */
	qenable(WR(q));
	return (0);
}

/*
 * gld_close(q) normal stream close call checks current status and cleans up
 * data structures that were dynamically allocated
 */
/*ARGSUSED1*/
int
gld_close(queue_t *q, int flag, cred_t *cred)
{
	gld_t	*gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;
	gld_mac_info_t *macinfo;

	ASSERT(q);
	ASSERT(gld);

	qprocsoff(q);

	macinfo = gld->gld_mac_info;

	if (gld->gld_state == DL_IDLE || gld->gld_state == DL_UNBOUND) {
		gld->gld_state = DL_UNBOUND;
		(void) gldunattach(q, NULL);
	}

	/* disassociate the stream from the device */
	q->q_ptr = WR(q)->q_ptr = NULL;
	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	ASSERT(gld->gld_next);
	gldremque(gld);			/* remove from active list */

	/* gldm_last is protected by gld_rwlock, not gldm_maclock */
	if (macinfo != NULL && macinfo->gldm_last == gld)
		macinfo->gldm_last = NULL;

	rw_exit(&glddev->gld_rwlock);

	kmem_free(gld, sizeof (gld_t));

	return (0);
}

/*
 * gld_wput (q, mp)
 * general gld stream write put routine. Receives ioctl's from
 * user level and data from upper modules and processes them immediately.
 * M_PROTO/M_PCPROTO are queued for later processing by the service
 * procedure.
 */

int
gld_wput(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)(q->q_ptr);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_wput(%x %x): type %x",
		    q, mp, DB_TYPE(mp));
#endif
	switch (DB_TYPE(mp)) {

	case M_DATA:
		/* fast data / raw support */
		if ((gld->gld_flags & (GLD_RAW | GLD_FAST)) == 0 ||
						gld->gld_state != DL_IDLE) {
			merror(q, mp, EPROTO);
			break;
		}
		if (q->q_first) {
			(void) putq(q, mp);
			qenable(q);
		} else if (gld_start(q, mp, GLD_TRYLOCK, 0) != 0) {
			(void) putq(q, mp);
			qenable(q);
		}
		break;
	case M_IOCTL:
		(void) putq(q, mp);
		qenable(q);
		break;

	case M_FLUSH:		/* canonical flush handling */
		if (*mp->b_rptr & FLUSHW)
			flushq(q, 0);
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
		} else
			freemsg(mp);
		break;

		/* for now, we will always queue */
	case M_PROTO:
	case M_PCPROTO:
		(void) putq(q, mp);
		qenable(q);
		break;

	default:
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld: Unexpected packet type from queue: %x",
			    DB_TYPE(mp));
#endif
		freemsg(mp);
	}
	return (0);
}

/*
 * gld_wsrv - Incoming messages are processed according to the DLPI protocol
 * specification
 */

int
gld_wsrv(queue_t *q)
{
	mblk_t *mp;
	register gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	union DL_primitives *prim;
	int	err;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_wsrv(%x)", q);
#endif

	if (q->q_first == NULL)
		return (0);

	/* 
	 * Acquire the maclock prior to the getq().
	 *
	 * Note: This is done to minimize a race condition between our getq()
	 * and another thread's gld_wput() call to gld_start() which could
	 * race into gld_start() before us, thus an out-of-order packet on
	 * the wire.
	 *
	 * The macinfo can be NULL if we're an un-DL_ATTACHed Style 2 open
	 */
	if (macinfo)
		mutex_enter(&macinfo->gldm_maclock);

	while ((mp = getq(q)) != NULL) {
		switch (DB_TYPE(mp)) {
		case M_IOCTL:
			if (macinfo)
				mutex_exit(&macinfo->gldm_maclock);
			(void) gld_ioctl(q, mp);
			if (macinfo)
				mutex_enter(&macinfo->gldm_maclock);
			break;
		case M_PROTO:	/* Will be an DLPI message of some type */
		case M_PCPROTO:
			if (macinfo)
				mutex_exit(&macinfo->gldm_maclock);
			if ((err = gld_cmds(q, mp)) != GLDE_OK) {
				if (err == GLDE_RETRY)
					return (0); /* quit while we're ahead */
				prim = (union DL_primitives *)mp->b_rptr;
				dlerrorack(q, mp, prim->dl_primitive, err, 0);
			}
			if (macinfo)
				mutex_enter(&macinfo->gldm_maclock);
			break;
		case M_DATA:
			ASSERT(macinfo);
			/*
			 * retry of a previously processed
			 * UNITDATA_REQ or is a RAW message from
			 * above
			 */
			if (gld_start(q, mp, GLD_HAVELOCK, 0) != 0) {
				(void) putbq(q, mp);
				if (macinfo)
					mutex_exit(&macinfo->gldm_maclock);
				return (0);
			}
			break;

			/* This should never happen */
		default:
#ifdef GLD_DEBUG
			if (gld_debug & GLDERRS)
				cmn_err(CE_WARN,
				    "gld_wsrv: db_type(%x) not supported",
				    mp->b_datap->db_type);
#endif
			freemsg(mp);	/* unknown types are discarded */
			break;
		}
	}

	gld->gld_flags &= ~GLD_XWAIT; /* Q empty -- no need for gld_sched */
	if (macinfo)
		mutex_exit(&macinfo->gldm_maclock);
	return (0);
}

static int
gld_start(queue_t *q, mblk_t *mp, int lockflavor, int rw_amreader)
{
	mblk_t *nmp;
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	int max_pkt, pkt_size;
	gld_interface_t *ifp;
	pktinfo_t pktinfo;

	ASSERT(gld->gld_state == DL_IDLE);
	ASSERT(macinfo != NULL);

	pkt_size = msgdsize(mp);
	max_pkt = ((gld_interface_pvt_t *)macinfo->gldm_interface)->
	    interfacep->mtu_size;
	if (pkt_size > max_pkt) {
		/* Drop this oversize packet */
		freemsg(mp);
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld: dropped oversize (%d) packet, max %d",
			    pkt_size, max_pkt);
#endif
		return (0);
	}

	if (macinfo->gldm_nprom > 0)
		nmp = dupmsg(mp);
	else
		nmp = NULL;

	/*
	 * Since we send the messages up directly from the interrupt
	 * thread (gld_sendup) there is a possibility that the
	 * message bounces back while holding the mutex lock.
	 * We need to only try the lock in this case otherwise
	 * we panic with mutex_re-enter.
	 */
	if (lockflavor == GLD_LOCK)
		mutex_enter(&macinfo->gldm_maclock);
	else if (lockflavor == GLD_TRYLOCK)
		if (!mutex_tryenter(&macinfo->gldm_maclock)) {
			if (nmp)
				freemsg(nmp);
			/*
			 * want interrupt for rescheduling.
			 *
			 * We take a small risk setting GLD_INTR_WAIT without
			 * holding the mutex.  The worst that can happen is
			 * that we are delayed a little until the Streams
			 * scheduler gets around to us.  This can happen if
			 * gld_sched turns off the bit and then examines the
			 * queue between the time we set the bit and our caller
			 * does the putq.  This will be extremely rare, and is
			 * not fatal.
			 */
			macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
			gld->gld_flags |= GLD_XWAIT; /* for gld_sched */
			return (-1);
		}

	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;
	if ((*ifp->interpreter)(macinfo, mp, &pktinfo)) {
		cmn_err(CE_WARN, "gld_start: %s: outgoing pkt (len %d) "
		    "doesn't look good", macinfo->gldm_ident, pktinfo.pktLen);
	}

	if ((*macinfo->gldm_send) (macinfo, mp)) {
		macinfo->gldm_stats.glds_xmtretry++;
		macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
		gld->gld_flags |= GLD_XWAIT; /* for gld_sched */
		if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
			mutex_exit(&macinfo->gldm_maclock);
		if (nmp)
			freemsg(nmp);	/* free the dupped message */
		return (-1);
	}

	if (pktinfo.isBroadcast)
		macinfo->gldm_stats.glds_brdcstxmt++;
	else if (pktinfo.isMulticast)
		macinfo->gldm_stats.glds_multixmt++;
	macinfo->gldm_stats.glds_bytexmt += pktinfo.pktLen;
	macinfo->gldm_stats.glds_pktxmt++;

	/*
	 * Loopback case. The message needs to be returned back on
	 * the read side. This would silently fail if the dumpmsg fails
	 * above. This is probably OK, if there is no memory to dup the
	 * block, then there isn't much we could do anyway.
	 */
	if (nmp)
		(void) gld_precv(macinfo, nmp, rw_amreader);

	if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
		mutex_exit(&macinfo->gldm_maclock);

	/*
	 * The device may be using the mblk for direct dma, in which case
	 * it will ask us not to free the mblk. The device would now be
	 * responsible for freeing this message block, once the dma is
	 * completed.
	 */
	if (!(macinfo->gldm_options & GLDOPT_DONTFREE))
		freemsg(mp);	/* free on success */
	return (0);
}

/*
 * gld_rsrv (q)
 *	simple read service procedure
 *	purpose is to avoid the time it takes for packets
 *	to move through IP so we can get them off the board
 *	as fast as possible due to limited PC resources.
 */

int
gld_rsrv(queue_t *q)
{
	mblk_t *mp;

	while ((mp = getq(q)) != NULL) {
		if (canputnext(q)) {
			putnext(q, mp);
		} else {
			freemsg(mp);
		}
	}
	return (0);
}

/*
 * gld_multicast used to determine if the address is a multicast address for
 * this user.
 */
static int
gld_multicast(mac_addr_t macaddr, gld_t *gld)
{
	register int i;

	if (!gld->gld_mcast)
		return (0);

	ASSERT(mutex_owned(&gld->gld_mac_info->gldm_maclock));

	for (i = 0; i < gld->gld_multicnt; i++) {
		if (gld->gld_mcast[i] && gld->gld_mcast[i]->gldm_refcnt) {
			if (mac_eq(gld->gld_mcast[i]->gldm_addr, macaddr))
				return (1);
		}
	}
	return (0);
}

/*
 * gld_ioctl (q, mp)
 * handles all ioctl requests passed downstream. This routine is
 * passed a pointer to the message block with the ioctl request in it, and a
 * pointer to the queue so it can respond to the ioctl request with an ack.
 */

int
gld_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	register gld_t *gld;
	gld_mac_info_t *macinfo;
	cred_t *cred;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_ioctl(%x %x)", q, mp);
#endif
	gld = (gld_t *)q->q_ptr;
	iocp = (struct iocblk *)mp->b_rptr;
	cred = iocp->ioc_cr;
	switch (iocp->ioc_cmd) {
	case DLIOCRAW:		/* raw M_DATA mode */
		if (cred == NULL || drv_priv(cred) == 0) {
			/* Only do if we have permission to avoid problems */
			gld->gld_flags |= GLD_RAW;
			DB_TYPE(mp) = M_IOCACK;
			qreply(q, mp);
		} else
			miocnak(q, mp, 0, EPERM);
		break;

	case DL_IOC_HDR_INFO:
				/* fastpath */
		gld_fastpath(gld, q, mp);
		break;
	default:
		macinfo	 = gld->gld_mac_info;
		if (macinfo != NULL && macinfo->gldm_ioctl != NULL) {
			mutex_enter(&macinfo->gldm_maclock);
			(*macinfo->gldm_ioctl) (q, mp);
			mutex_exit(&macinfo->gldm_maclock);
		} else
			miocnak(q, mp, 0, EINVAL);
		break;
	}
	return (0);
}

/*
 * gld_cmds (q, mp)
 *	process the DL commands as defined in dlpi.h
 *	note that the primitives return status which is passed back
 *	to the service procedure.  If the value is GLDE_RETRY, then
 *	it is assumed that processing must stop and the primitive has
 *	been put back onto the queue.  If the value is any other error,
 *	then an error ack is generated by the service procedure.
 */
static int
gld_cmds(queue_t *q, mblk_t *mp)
{
	register union DL_primitives *dlp;
	gld_t *gld = (gld_t *)(q->q_ptr);
	int result;

	dlp = (union DL_primitives *)mp->b_rptr;
#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE,
		    "gld_cmds(%x, %x):dlp=%x, dlp->dl_primitive=%d",
		    q, mp, dlp, dlp->dl_primitive);
#endif
	switch (dlp->dl_primitive) {
	case DL_BIND_REQ:
		result = gld_bind(q, mp);
		break;

	case DL_UNBIND_REQ:
		result = gld_unbind(q, mp);
		break;

	case DL_UNITDATA_REQ:
		result = gld_unitdata(q, mp);
		break;

	case DL_INFO_REQ:
		result = gld_inforeq(q, mp);
		break;

	case DL_ATTACH_REQ:
		if (gld->gld_style == DL_STYLE2)
			result = gldattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_DETACH_REQ:
		if (gld->gld_style == DL_STYLE2)
			result = gldunattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_ENABMULTI_REQ:
		result = gld_enable_multi(q, mp);
		break;

	case DL_DISABMULTI_REQ:
		result = gld_disable_multi(q, mp);
		break;

	case DL_PHYS_ADDR_REQ:
		result = gld_physaddr(q, mp);
		break;

	case DL_SET_PHYS_ADDR_REQ:
		result = gld_setaddr(q, mp);
		break;

	case DL_PROMISCON_REQ:
		result = gld_promisc(q, mp, 1);
		break;
	case DL_PROMISCOFF_REQ:
		result = gld_promisc(q, mp, 0);
		break;
	case DL_XID_REQ:
	case DL_XID_RES:
	case DL_TEST_REQ:
	case DL_TEST_RES:
		result = DL_NOTSUPPORTED;
		break;
	default:
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_cmds: unknown M_PROTO message: %d",
			    dlp->dl_primitive);
#endif
		result = DL_BADPRIM;
	}
	return (result);
}

/*
 * gld_bind - determine if a SAP is already allocated and whether it is legal
 * to do the bind at this time
 */
static int
gld_bind(queue_t *q, mblk_t *mp)
{
	int	sap;
	register dl_bind_req_t *dlp;
	gld_t  *gld = (gld_t *)q->q_ptr;

	ASSERT(gld);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_bind(%x %x)", q, mp);
#endif

	dlp = (dl_bind_req_t *)mp->b_rptr;
	sap = dlp->dl_sap;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: lsap=%x", sap);
#endif

	ASSERT(gld->gld_qptr == RD(q));
	if (gld->gld_state != DL_UNBOUND) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_bind: bound or not attached (%d)",
				gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	if (dlp->dl_service_mode != DL_CLDLS) {
		return (DL_UNSUPPORTED);
	}
	if (dlp->dl_xidtest_flg & (DL_AUTO_XID | DL_AUTO_TEST)) {
		return (DL_NOAUTO);
	}
	if (sap > GLDMAXETHERSAP)
		return (DL_BADSAP);

	/* if we fall through, then the SAP is legal */
	gld->gld_sap = sap;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: ok - sap = %d", gld->gld_sap);
#endif

	/* ACK the BIND */

	dlbindack(q, mp, sap, gld->gld_mac_info->gldm_macaddr, 6, 0, 0);

	gld->gld_state = DL_IDLE;	/* bound and ready */
	return (GLDE_OK);
}

/*
 * gld_unbind - perform an unbind of an LSAP or ether type on the stream.
 * The stream is still open and can be re-bound.
 */
static int
gld_unbind(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unbind(%x %x)", q, mp);
#endif

	if (gld->gld_state != DL_IDLE) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unbind: wrong state (%d)",
				gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	gld->gld_sap = 0;
	gld_flushqueue(q);	/* flush the queues */
	dlokack(q, mp, DL_UNBIND_REQ);
	gld->gld_state = DL_UNBOUND;
	return (GLDE_OK);
}

/*
 * gld_inforeq - generate the response to an info request
 */
static int
gld_inforeq(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	mblk_t *nmp;
	dl_info_ack_t *dlp;
	int	bufsize;
	glddev_t *glddev;
	gld_mac_info_t *macinfo;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_inforeq(%x %x)", q, mp);
#endif
	gld = (gld_t *)q->q_ptr;
	ASSERT(gld);
	glddev = gld->gld_device;

	bufsize = sizeof (dl_info_ack_t) + 2 * (ETHERADDRL + 2);

	nmp = mexchange(q, mp, bufsize, M_PCPROTO, DL_INFO_ACK);

	if (nmp) {
		nmp->b_wptr = nmp->b_rptr + sizeof (dl_info_ack_t);
		dlp = (dl_info_ack_t *)nmp->b_rptr;
		bzero((caddr_t)dlp, sizeof (dl_info_ack_t));
		dlp->dl_primitive = DL_INFO_ACK;
		dlp->dl_service_mode = DL_CLDLS;
		dlp->dl_current_state = gld->gld_state;
		dlp->dl_provider_style = gld->gld_style;

		if (gld->gld_state == DL_IDLE || gld->gld_state == DL_UNBOUND) {
			macinfo = gld->gld_mac_info;
			ASSERT(macinfo != NULL);
			dlp->dl_min_sdu = macinfo->gldm_minpkt;
			dlp->dl_max_sdu = macinfo->gldm_maxpkt;
			dlp->dl_mac_type = macinfo->gldm_type;

			/* copy macaddr and sap */
			if (gld->gld_state == DL_IDLE)
				dlp->dl_addr_offset = sizeof (dl_info_ack_t);
			else
				dlp->dl_addr_offset = NULL;

			dlp->dl_addr_length = macinfo->gldm_addrlen;
			dlp->dl_sap_length = macinfo->gldm_saplen;
			dlp->dl_addr_length += abs(dlp->dl_sap_length);
			nmp->b_wptr += dlp->dl_addr_length +
				abs(macinfo->gldm_saplen);
			if (dlp->dl_addr_offset != NULL)
			    mac_copy((caddr_t)macinfo->gldm_macaddr,
				((caddr_t)dlp) + dlp->dl_addr_offset);

			if (gld->gld_state == DL_IDLE) {
				/*
				 * save the correct number of bytes in the DLSAP
				 * we currently only handle negative sap lengths
				 * so a positive one will just get ignored.
				 */
				switch (macinfo->gldm_saplen) {
				case -1:
					*(((caddr_t)dlp) +
					    dlp->dl_addr_offset +
					    dlp->dl_addr_length +
					    dlp->dl_sap_length) =
						gld->gld_sap;
					break;
				case -2:
					*(ushort *)(((caddr_t)dlp) +
					    dlp->dl_addr_offset +
					    dlp->dl_addr_length +
					    dlp->dl_sap_length) =
						gld->gld_sap;
					break;
				}

				dlp->dl_brdcst_addr_offset =
				    dlp->dl_addr_offset + dlp->dl_addr_length;
			} else {
				dlp->dl_brdcst_addr_offset =
				    sizeof (dl_info_ack_t);
			}
			/* copy broadcast addr */
			dlp->dl_brdcst_addr_length = macinfo->gldm_addrlen;
			nmp->b_wptr += dlp->dl_brdcst_addr_length;
			cmac_copy((caddr_t)macinfo->gldm_broadcast,
				((caddr_t)dlp) + dlp->dl_brdcst_addr_offset,
				macinfo);
		} else {
			/*
			 *** these are probably all bogus since we ***
			 *** don't have an attached device. ***
			 */
			dlp->dl_min_sdu = glddev->gld_minsdu;
			dlp->dl_max_sdu = glddev->gld_maxsdu;
			dlp->dl_mac_type = glddev->gld_type;
			dlp->dl_addr_offset = NULL;
/* *** VIOLATION *** */ dlp->dl_addr_length = 8;	/* ETHERADDRL + 2 */
			dlp->dl_sap_length = -2;

			dlp->dl_brdcst_addr_offset = sizeof (dl_info_ack_t);
			dlp->dl_brdcst_addr_length = ETHERADDRL;
			nmp->b_wptr += dlp->dl_brdcst_addr_length;
			mac_copy((caddr_t)gldbroadcastaddr,
				((caddr_t)dlp) + dlp->dl_brdcst_addr_offset);
		}
		dlp->dl_version = DL_VERSION_2;
		qreply(q, nmp);
	}
	return (GLDE_OK);
}

/*
 * gld_bconvcopy()
 * This is essentialy bcopy, with the ability to bit reverse the
 * the source bytes. The MAC addresses bytes as transmitted by FDDI
 * interfaces are bit reversed.
 */

void
gld_bconvcopy(register caddr_t src, register caddr_t target, size_t n)
{
	while (n--)
		*target++ = bit_rev[(uchar_t)*src++];
}

/*
 * gld_bitconvert()
 * Convert the bit order by swaping all the bits, using a
 * lookup table.
 */
static void
gld_bitconvert(register u_char *rptr, register size_t n)
{
	register char	nb;

	while (n--) {
		/*
		 * this is not guaranteed to work on all compilers
		 * *rptr = bit_rev[*rptr++];
		 */
		nb = bit_rev[*rptr];
		*rptr++ = nb;
	}
}

/*
 * gld_unitdata (q, mp)
 * send a datagram.  Destination address/lsap is in M_PROTO
 * message (first mblock), data is in remainder of message.
 *
 */
static int
gld_unitdata(queue_t *q, mblk_t *mp)
{
	register gld_t *gld = (gld_t *)q->q_ptr;
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_rptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	long	msglen;
	mblk_t	*nmp;
	gld_interface_t *ifp;

	ASSERT(macinfo != NULL);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unitdata(%x %x)", q, mp);
#endif

	if (gld->gld_state != DL_IDLE) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: wrong state (%d)",
				gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}

	msglen = msgdsize(mp);
	if (msglen == 0 || msglen > macinfo->gldm_maxpkt) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: bad msglen (%d)",
				msglen);
#endif
		dluderrorind(q, mp,
		    (u_char *)DLSAP(dlp, dlp->dl_dest_addr_offset),
		    dlp->dl_dest_addr_length, DL_BADDATA, 0);
		return (GLDE_OK);
	}

	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;

	/*
	 * make a valid header for transmission
	 */
	if ((nmp = (*ifp->mkunitdata)(gld, mp)) == NULL) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: mkunitdata failed.",
				msglen);
#endif
		dlerrorack(q, mp, DL_UNITDATA_REQ, DL_SYSERR, ENOSR);
		return (GLDE_OK);
	}

	if (gld_start(q, nmp, GLD_LOCK, 0) != 0) {
		(void) putbq(q, nmp);
		return (GLDE_RETRY);		/* GLDE_OK works too */
	}
	return (GLDE_OK);
}

/*
 * gld_recv (macinfo, mp)
 * called with an ethernet packet in a mblock; must decide whether
 * packet is for us and which streams to queue it to.
 */
int
gld_recv(gld_mac_info_t *macinfo, mblk_t *mp)
{
	return (gld_sendup(macinfo, mp, gld_accept, 0));
}

/*
 * gld_precv (macinfo, mp, rw_amreader)
 * called with an ethernet packet in a mblock; must decide whether
 * packet is for us and which streams to queue it to.
 */
static int
gld_precv(gld_mac_info_t *macinfo, mblk_t *mp, int rw_amreader)
{
	return (gld_sendup(macinfo, mp, gld_paccept, rw_amreader));
}

/*
 * gld_sendup (macinfo, mp)
 * called with an ethernet packet in a mblock; must decide whether
 * packet is for us and which streams to queue it to.
 */

static int
gld_sendup(gld_mac_info_t *macinfo, mblk_t *mp, int (*acceptfunc)(),
	int rw_amreader)
{
	gld_t *gld;
	gld_t *fgld = 0;
	glddev_t *glddev;
	mblk_t *nmp = NULL;
	pktinfo_t pktinfo;
	gld_interface_t *ifp;
	void	(*send)(queue_t *qp, mblk_t *mp);
	int	(*cansend)(queue_t *qp);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_sendup(%x, %x)", mp, macinfo);
#endif

	ASSERT(macinfo != NULL);
	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	if (macinfo == NULL) {
		freemsg(mp);
		return (0);
	}

	glddev = macinfo->gldm_dev;
	if (macinfo->gldm_options & GLDOPT_FAST_RECV) {
		send = (void (*)(queue_t *, mblk_t *))putq;
		cansend = canput;
	} else {
		send = putnext;
		cansend = canputnext;
	}

	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;

	if ((*ifp->interpreter)(macinfo, mp, &pktinfo)) {
		freemsg(mp);
		macinfo->gldm_stats.glds_errrcv++;
		return (0);
	}

#ifdef GLD_DEBUG
	if ((gld_debug & GLDRECV) &&
			(!(gld_debug & GLDNOBR) ||
			(!pktinfo.isBroadcast && !pktinfo.isMulticast))) {

		cmn_err(CE_CONT, "gld_sendup: machdr=<%x:%x:%x:%x:%x:%x -> "
					"%x:%x:%x:%x:%x:%x>\n",
			pktinfo.shost[0], pktinfo.shost[1], pktinfo.shost[2],
			pktinfo.shost[3], pktinfo.shost[4], pktinfo.shost[5],
			pktinfo.dhost[0], pktinfo.dhost[1], pktinfo.dhost[2],
			pktinfo.dhost[3], pktinfo.dhost[4], pktinfo.dhost[5]);
		cmn_err(CE_CONT, "gld_sendup: Snap: %s Sap: %4x Len: %4d "
				"Hdr: %d,%d isMulticast: %s\n",
				pktinfo.hasSnap ? "Y" : "N",
				pktinfo.Sap,
				pktinfo.pktLen,
				pktinfo.macLen,
				pktinfo.hdrLen,
				pktinfo.isMulticast ? "Y" : "N");
	}
#endif
	/* Grab the rw lock if we don't already have it */
	if (!rw_amreader)
		rw_enter(&glddev->gld_rwlock, RW_READER);

	for (gld = glddev->gld_str_next;
		gld != (gld_t *)&glddev->gld_str_next;
		gld = gld->gld_next) {
		if (gld->gld_qptr == NULL || gld->gld_state != DL_IDLE ||
		    gld->gld_mac_info != macinfo) {
			continue;
		}

#ifdef GLD_DEBUG
		if ((gld_debug & GLDRECV) &&
				(!(gld_debug & GLDNOBR) ||
				(!pktinfo.isBroadcast && !pktinfo.isMulticast)))
			cmn_err(CE_NOTE,
			    "gld_sendup: queue sap: %4x promis: %s %s %s",
			    gld->gld_sap,
			    gld->gld_flags & GLD_PROM_PHYS ? "phys " : "     ",
			    gld->gld_flags & GLD_PROM_SAP  ? "sap  " : "     ",
			    gld->gld_flags & GLD_PROM_MULT ? "multi" : "     ");
#endif

		if ((*acceptfunc)(gld, &pktinfo)) {
			/* sap matches */
			/*
			 * Uppers stream is not accepting messages, i.e.
			 * it is flow controlled, therefore we will
			 * drop the message altogether
			 */
			if (!(*cansend)(gld->gld_qptr)) {
#ifdef GLD_DEBUG
				if (gld_debug & GLDRECV)
					cmn_err(CE_WARN,
					    "gld_sendup: canput failed");
#endif
				gld->gld_stats->glds_blocked++;
				qenable(gld->gld_qptr);
				continue;
			}
			/*
			 * we are trying to avoid an extra dumpmsg() here.
			 * If this is the first eligible queue, remember the
			 * queue and send up the mssage after the loop.
			 */
			if (!fgld) {
				fgld = gld;
				continue;
			}
			nmp = dupmsg(mp);
			gld_passon(gld, mp, &pktinfo, send);
			mp = nmp;
			if (mp == NULL)
				break;	/* couldn't get resources; drop it */
		}
	}
	if (fgld && mp) {
		gld_passon(fgld, mp, &pktinfo, send);
		mp = 0;
	}
	if (!rw_amreader)
		rw_exit(&glddev->gld_rwlock);

	if (mp != NULL)
		freemsg(mp);

	/* XXX -- This counts looped back packets, OK? */
	if (pktinfo.isBroadcast)
		macinfo->gldm_stats.glds_brdcstrcv++;
	else if (pktinfo.isMulticast)
		macinfo->gldm_stats.glds_multircv++;

	macinfo->gldm_stats.glds_bytercv += pktinfo.pktLen;
	macinfo->gldm_stats.glds_pktrcv++;

	return (0);
}

static void
gld_passon(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo,
	void (*send)(queue_t *qp, mblk_t *mp))
{
	int	hdr_len = pktinfo->macLen;

	hdr_len += (pktinfo->hasSnap ? pktinfo->hdrLen : 0);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_passon(%x, %x, %x)", gld, mp, pktinfo);

	if ((gld_debug & GLDRECV) && (!(gld_debug & GLDNOBR) ||
	    (!pktinfo->isBroadcast && !pktinfo->isMulticast)))
		cmn_err(CE_NOTE, "gld_passon: q: %x mblk: %x minor: %d sap: %x",
		    gld->gld_qptr->q_next, mp, gld->gld_minor, gld->gld_sap);
#endif
	if ((gld->gld_flags & GLD_FAST) && !pktinfo->isMulticast &&
	    !pktinfo->isBroadcast) {
		mp->b_rptr += hdr_len;
		(*send)(gld->gld_qptr, mp);
	} else if (gld->gld_flags & GLD_RAW) {
		(*send)(gld->gld_qptr, mp);
	} else {
		mp = gld_addudind(gld, mp, pktinfo);
		if (mp)
			(*send)(gld->gld_qptr, mp);
	}
}

/*
 * gldattach(q, mp)
 * DLPI DL_ATTACH_REQ
 * this attaches the stream to a PPA
 */
static int
gldattach(queue_t *q, mblk_t *mp)
{
	dl_attach_req_t *at;
	gld_mac_info_t *mac;
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev;

	at = (dl_attach_req_t *)mp->b_rptr;

	if (gld->gld_state != DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}
	glddev = gld->gld_device;
	for (mac = glddev->gld_mac_next;
		mac != (gld_mac_info_t *)&glddev->gld_mac_next;
		mac = mac->gldm_next) {
		ASSERT(mac);
		if (mac->gldm_ppa == at->dl_ppa) {

			ASSERT(!gld->gld_mac_info);

			/*
			 * we found the correct PPA
			 */
			gld->gld_mac_info = mac;

			gld->gld_state = DL_UNBOUND; /* now ready for action */
			gld->gld_stats = &mac->gldm_stats;

			/*
			 * We must hold the mutex to prevent multiple calls
			 * to the reset and start routines.
			 */
			mutex_enter(&mac->gldm_maclock);
			if (mac->gldm_nstreams == 0) {
				/* reset and setup */
				(*mac->gldm_reset) (mac);
				/* now make sure it is running */
				(*mac->gldm_start) (mac);
			}
			mac->gldm_nstreams++;
			mutex_exit(&mac->gldm_maclock);

			dlokack(q, mp, DL_ATTACH_REQ);
			return (GLDE_OK);
		}
	}
	return (DL_BADPPA);
}

/*
 * gldunattach(q, mp)
 * DLPI DL_DETACH_REQ
 * detaches the mac layer from the stream
 */
static int
gldunattach(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	int	state = gld->gld_state;
	int	i, change = 0;

	if (state != DL_UNBOUND)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);

	if (gld->gld_mcast) {
		for (i = 0; i < gld->gld_multicnt; i++) {
			gld_mcast_t *mcast;

			if ((mcast = gld->gld_mcast[i]) != NULL) {
				/* disable from stream and possibly lower */
				gld_send_disable_multi(gld, gld->gld_mac_info,
							mcast);
			}
		}
		kmem_free(gld->gld_mcast,
		    sizeof (gld_mcast_t *) * gld->gld_multicnt);
		gld->gld_mcast = NULL;
		gld->gld_multicnt = 0;
	}

	mutex_enter(&macinfo->gldm_maclock);

	/* cleanup remnants of promiscuous mode */
	if (gld->gld_flags & GLD_PROM_PHYS &&
	    --macinfo->gldm_nprom == 0)
		change++;

	if (gld->gld_flags & GLD_PROM_MULT &&
	    --macinfo->gldm_nprom_multi == 0)
		change++;

	if (change)
		(*macinfo->gldm_prom)(macinfo,
		    (macinfo->gldm_nprom_multi ? GLD_MAC_PROMISC_MULTI : 0) |
		    (macinfo->gldm_nprom ? GLD_MAC_PROMISC_PHYS : 0));

	/* cleanup mac layer if last stream */
	if (--macinfo->gldm_nstreams == 0) {
		(*macinfo->gldm_stop) (macinfo);
	}

	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	/* make sure no references to this gld for gld_sched */
	if (macinfo->gldm_last == gld) {
		macinfo->gldm_last = NULL;
	}
	gld->gld_mac_info = NULL;

	gld->gld_stats = NULL;
	/* XXX -- Should we reset RAW or FAST here ??? */
	gld->gld_flags &= ~(GLD_PROM_PHYS | GLD_PROM_SAP | GLD_PROM_MULT);
	gld->gld_sap = 0;
	gld->gld_state = DL_UNATTACHED;

	rw_exit(&glddev->gld_rwlock);

	mutex_exit(&macinfo->gldm_maclock);

	if (mp) {
		dlokack(q, mp, DL_DETACH_REQ);
	}
	return (GLDE_OK);
}

/*
 * gld_enable_multi (q, mp)
 * enables multicast address on the stream if the mac layer
 * isn't enabled for this address, enable at that level as well.
 */
static int
gld_enable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	glddev_t *glddev;
	gld_mac_info_t *macinfo;
	mac_addr_t maddr;
	dl_enabmulti_req_t *multi;
	gld_mcast_t *mcast;
	int	status = DL_BADADDR;
	int	i;

#if defined(GLD_DEBUG)
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_enable_multi(%x, %x)", q, mp);
	}
#endif

	gld = (gld_t *)q->q_ptr;
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	if (macinfo->gldm_sdmulti == NULL) {
		return (DL_UNSUPPORTED);
	}

	glddev = macinfo->gldm_dev;
	multi = (dl_enabmulti_req_t *)mp->b_rptr;
	mac_copy((multi + 1), maddr);

	/*
	 * check to see if this multicast address is valid if it is, then
	 * check to see if it is already in the per stream table and the per
	 * device table if it is already in the per stream table, if it isn't
	 * in the per device, add it.  If it is, just set a pointer.  If it
	 * isn't, allocate what's necessary.
	 */

	if (MBLKL(mp) >= sizeof (dl_enabmulti_req_t) &&
	    MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length)) {
		if (!ismulticast(maddr)) {
			return (DL_BADADDR);
		}
		/* request appears to be valid */
		/* does this address appear in current table? */
		if (gld->gld_mcast == NULL) {
			/* no mcast addresses -- allocate table */
			gld->gld_mcast = GETSTRUCT(gld_mcast_t *,
						    glddev->gld_multisize);
			if (gld->gld_mcast == NULL)
				return (DL_SYSERR);
			gld->gld_multicnt = glddev->gld_multisize;
		} else {
			for (i = 0; i < gld->gld_multicnt; i++) {
				if (gld->gld_mcast[i] &&
				    mac_eq(gld->gld_mcast[i]->gldm_addr,
					maddr)) {
					ASSERT(gld->gld_mcast[i]->gldm_refcnt);
					/* this is a match -- just succeed */
					dlokack(q, mp, DL_ENABMULTI_REQ);
					return (GLDE_OK);
				}
			}
		}
		/*
		 * there wasn't one so check to see if the mac layer has one
		 */
		mutex_enter(&macinfo->gldm_maclock);
		if (macinfo->gldm_mcast == NULL) {
			macinfo->gldm_mcast = GETSTRUCT(gld_mcast_t,
							glddev->gld_multisize);
			if (macinfo->gldm_mcast == NULL) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_SYSERR);
			}
		}
		for (mcast = NULL, i = 0; i < glddev->gld_multisize; i++) {
			if (macinfo->gldm_mcast[i].gldm_refcnt &&
			    mac_eq(macinfo->gldm_mcast[i].gldm_addr, maddr)) {
				mcast = &macinfo->gldm_mcast[i];
				break;
			}
		}
		if (mcast == NULL) {
			/* find an empty slot to fill in */
			for (i = 0; i < glddev->gld_multisize; i++) {
				if (macinfo->gldm_mcast[i].gldm_refcnt == 0) {
				    mcast = &macinfo->gldm_mcast[i];
				    mac_copy(maddr, mcast->gldm_addr);
					break;
				}
			}
		}
		if (mcast != NULL) {
			if (macinfo->gldm_options & GLDOPT_CANONICAL_ADDR)
				gld_bitconvert(maddr, ETHERADDRL);
			for (i = 0; i < gld->gld_multicnt; i++) {
				if (gld->gld_mcast[i] == NULL) {
					gld->gld_mcast[i] = mcast;
					if (!mcast->gldm_refcnt++) {
						/* set mcast in hardware */
						(*macinfo->gldm_sdmulti)
						    (macinfo, maddr, 1);
					}
					mutex_exit(&macinfo->gldm_maclock);
					dlokack(q, mp, DL_ENABMULTI_REQ);
					return (GLDE_OK);
				}
			}
		}
		mutex_exit(&macinfo->gldm_maclock);
		status = DL_TOOMANY;
	}
	return (status);
}


/*
 * gld_disable_multi (q, mp)
 * disable the multicast address on the stream if last
 * reference for the mac layer, disable there as well
 */
static int
gld_disable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	gld_mac_info_t *macinfo;
	mac_addr_t maddr;
	dl_enabmulti_req_t *multi;
	int	status = DL_BADADDR, i;
	gld_mcast_t *mcast;

#if defined(GLD_DEBUG)
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_enable_multi(%x, %x)", q, mp);
	}
#endif

	gld = (gld_t *)q->q_ptr;
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	if (macinfo->gldm_sdmulti == NULL) {
		return (DL_UNSUPPORTED);
	}
	multi = (dl_enabmulti_req_t *)mp->b_rptr;
	mac_copy((mac_addr_t *)(multi + 1), maddr);

	if (MBLKL(mp) >= sizeof (dl_enabmulti_req_t) &&
	    MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length)) {
		/* request appears to be valid */
		/* does this address appear in current table? */
		if (gld->gld_mcast != NULL) {
			for (i = 0; i < gld->gld_multicnt; i++)
				if (((mcast = gld->gld_mcast[i]) != NULL) &&
				    mac_eq(mcast->gldm_addr, maddr)) {
					ASSERT(mcast->gldm_refcnt);
					gld_send_disable_multi(gld, macinfo,
								mcast);
					gld->gld_mcast[i] = NULL;
					dlokack(q, mp, DL_DISABMULTI_REQ);
					return (GLDE_OK);
				}
		}
		status = DL_NOTENAB; /* not an enabled address */
	}
	return (status);
}

/*
 * gld_send_disable_multi(gld, macinfo, mcast)
 * this function is used to disable a multicast address if the reference
 * count goes to zero. The disable request will then be forwarded to the
 * lower stream.
 */

static void
gld_send_disable_multi(gld_t *gld, gld_mac_info_t *macinfo,
	gld_mcast_t *mcast)
{
#ifdef lint
	gld = gld;
#endif
	ASSERT(mcast != NULL);
	ASSERT(macinfo != NULL);
	ASSERT(mcast->gldm_refcnt);

	if (mcast == NULL) {
		return;
	}
	if (macinfo == NULL) {
		return;
	}
	if (!mcast->gldm_refcnt) {
		return;			/* "cannot happen" */
	}

	mutex_enter(&macinfo->gldm_maclock);

	if (--mcast->gldm_refcnt > 0) {
		mutex_exit(&macinfo->gldm_maclock);
		return;
	}

	/*
	 * This should be converted from canonical form to device form
	 * The refcnt is now zero so we can trash the data.
	 */
	if (macinfo->gldm_options & GLDOPT_CANONICAL_ADDR)
		gld_bitconvert(mcast->gldm_addr, ETHERADDRL);

	(*macinfo->gldm_sdmulti) (macinfo, mcast->gldm_addr, 0);

	mutex_exit(&macinfo->gldm_maclock);
}

/*
 * gld_findminor(device)
 * searches the per device class list of STREAMS for
 * the first minor number not used.  Note that we currently don't allocate
 * minor 0.
 * This routine looks slow to me.
 */

static int
gld_findminor(glddev_t *device)
{
	gld_t  *next;
	int	minor;

	for (minor = GLD_PPA_INIT; minor > 0; minor++) {
		for (next = device->gld_str_next;
		    next != (gld_t *)&device->gld_str_next;
		    next = next->gld_next) {
			if (minor == next->gld_minor)
				goto nextminor;
		}
		return (minor);
nextminor:
		/* don't need to do anything */
		;
	}
	/*NOTREACHED*/
}

#ifndef PPA_IS_INSTANCE
/*
 * gld_findppa(device)
 * searches the per device class list of device instances for
 * the first PPA number not used.
 *
 * This routine looks slow to me, but nobody cares, since it seldom executes.
 */

static int
gld_findppa(glddev_t *device)
{
	gld_mac_info_t	*next;
	int ppa;

	for (ppa = 0; ppa >= 0; ppa++) {
		for (next = device->gld_mac_next;
		    next != (gld_mac_info_t *)&device->gld_mac_next;
		    next = next->gldm_next) {
			if (ppa == next->gldm_ppa)
				goto nextppa;
		}
		return (ppa);
nextppa:
		/* don't need to do anything */
		;
	}
	/*NOTREACHED*/
}
#endif

/*
 * gld_addudind(gld, mp, pktinfo)
 * format a DL_UNITDATA_IND message to be sent to the user
 */
static mblk_t *
gld_addudind(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo)
{
	gld_mac_info_t		*macinfo = gld->gld_mac_info;
	dl_unitdata_ind_t	*dludindp;
	dladdr_t		*dladdrp;
	mblk_t			*nmp;
	int			size;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_addudind(%x, %x, %x)", gld, mp, pktinfo);
#endif
	/*
	 * Allocate the DL_UNITDATA_IND M_PROTO header, if allocation fails
	 * might as well discard since we can't go further
	 */
	size = sizeof (dl_unitdata_ind_t) + 2 * sizeof (dladdr_t);
	if ((nmp = allocb(size, BPRI_MED)) == NULL) {
		freemsg(mp);
		return ((mblk_t *)NULL);
	}
	DB_TYPE(nmp) = M_PROTO;
	nmp->b_wptr = nmp->b_datap->db_lim;
	nmp->b_rptr = nmp->b_wptr - size;

	/* step past mac header */
	mp->b_rptr += pktinfo->macLen;

	/*
	 * NOTE: I need to fix the rptr, it has to move forward
	 * based on the packet and the target sap, it is not sufficient
	 * to just move it forward, the Value of the sap has to be
	 * taken into account.
	 */

	if (gld->gld_sap > GLD_MAX_802_SAP)
		mp->b_rptr += pktinfo->hdrLen;

	/*
	 * now setup the DL_UNITDATA_IND header
	 */
	dludindp = (dl_unitdata_ind_t *)nmp->b_rptr;
	dludindp->dl_primitive = DL_UNITDATA_IND;
	dludindp->dl_dest_addr_length = macinfo->gldm_addrlen +
					abs(macinfo->gldm_saplen);
	dludindp->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_src_addr_length = macinfo->gldm_addrlen +
					abs(macinfo->gldm_saplen);
	dludindp->dl_src_addr_offset = dludindp->dl_dest_addr_offset +
					dludindp->dl_dest_addr_length;

	dludindp->dl_group_address = (pktinfo->isMulticast ||
					pktinfo->isBroadcast);

	dladdrp = (dladdr_t *)(nmp->b_rptr + dludindp->dl_dest_addr_offset);
	mac_copy(pktinfo->dhost, dladdrp->dl_phys);
	dladdrp->dl_sap = pktinfo->Sap;

	dladdrp = (dladdr_t *)(nmp->b_rptr + dludindp->dl_src_addr_offset);
	mac_copy(pktinfo->shost, dladdrp->dl_phys);
	dladdrp->dl_sap = pktinfo->Sap;

	linkb(nmp, mp);
	return (nmp);
}

/*
 * gld_physaddr()
 *	get the current or factory physical address value
 */
static int
gld_physaddr(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	mac_addr_t addr;

	if (MBLKL(mp) < DL_PHYS_ADDR_REQ_SIZE) {
		return (DL_BADPRIM);
	}
	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	switch (prim->physaddr_req.dl_addr_type) {
	case DL_FACT_PHYS_ADDR:
		mac_copy((caddr_t)macinfo->gldm_vendor, (caddr_t)addr);
		break;
	case DL_CURR_PHYS_ADDR:
		mac_copy((caddr_t)macinfo->gldm_macaddr, (caddr_t)addr);
		break;
	default:
		return (DL_BADPRIM);
	}
	dlphysaddrack(q, mp, (caddr_t)addr, macinfo->gldm_addrlen);
	return (GLDE_OK);
}

/*
 * gld_setaddr()
 *	change the hardware's physical address to a user specified value
 */
static int
gld_setaddr(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	struct ether_addr *addr;

	if (MBLKL(mp) < DL_SET_PHYS_ADDR_REQ_SIZE) {
		return (DL_BADPRIM);
	}
	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	if (prim->set_physaddr_req.dl_addr_length != macinfo->gldm_addrlen) {
		return (DL_BADADDR);
	}

	/*
	 * TBD:
	 * Are there any bound streams other than the one I'm on?
	 * If so, disallow the set.
	 */

	/* now do the set at the hardware level */
	addr = (struct ether_addr *)
		(mp->b_rptr + prim->set_physaddr_req.dl_addr_offset);
	mutex_enter(&macinfo->gldm_maclock);
	cmac_copy(addr, macinfo->gldm_macaddr, macinfo);
	(*macinfo->gldm_saddr) (macinfo);
	mutex_exit(&macinfo->gldm_maclock);
	dlokack(q, mp, DL_SET_PHYS_ADDR_REQ);
	return (GLDE_OK);
}

/*
 * gld_promisc (q, mp, on)
 *	enable or disable the use of promiscuous mode with the hardware
 */
static int
gld_promisc(queue_t *q, mblk_t *mp, int on)
{
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	int	change = 0;
	gld_t  *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_promisc(%x, %x, %x)", q, mp, on);
#endif

	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	ASSERT(mp != NULL);

	mutex_enter(&macinfo->gldm_maclock);
	if (on) {
		switch (prim->promiscon_req.dl_level) {
		case DL_PROMISC_PHYS:
			if (!(gld->gld_flags & GLD_PROM_PHYS)) {
				if (macinfo->gldm_nprom++ == 0)
					change++;
				gld->gld_flags |= GLD_PROM_PHYS;
			}
			break;
		case DL_PROMISC_MULTI:
			if (!(gld->gld_flags & GLD_PROM_MULT)) {
				if (macinfo->gldm_nprom_multi++ == 0)
					change++;
				gld->gld_flags |= GLD_PROM_MULT;
			}
			break;
		case DL_PROMISC_SAP:
			gld->gld_flags |= GLD_PROM_SAP;
			break;
		default:
			mutex_exit(&macinfo->gldm_maclock);
			return (DL_UNSUPPORTED);	/* this is an error */
		}
	} else {
		switch (prim->promiscon_req.dl_level) {
		case DL_PROMISC_PHYS:
			if (!(gld->gld_flags & GLD_PROM_PHYS)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			} else {
				if (--macinfo->gldm_nprom == 0)
					change++;
				gld->gld_flags &= ~GLD_PROM_PHYS;
			}
			break;
		case DL_PROMISC_MULTI:
			if (!(gld->gld_flags & GLD_PROM_MULT)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			} else {
				if (--macinfo->gldm_nprom_multi == 0)
					change++;
				gld->gld_flags &= ~GLD_PROM_MULT;
			}
			break;
		case DL_PROMISC_SAP:
			if (!(gld->gld_flags & GLD_PROM_SAP)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			}
			gld->gld_flags &= ~GLD_PROM_SAP;
			break;
		default:
			mutex_exit(&macinfo->gldm_maclock);
			return (DL_UNSUPPORTED);	/* this is an error */
		}
	}
	if (change) {
		(*macinfo->gldm_prom)(macinfo,
		    (macinfo->gldm_nprom_multi ? GLD_MAC_PROMISC_MULTI : 0) |
		    (macinfo->gldm_nprom ? GLD_MAC_PROMISC_PHYS : 0));
	}
	mutex_exit(&macinfo->gldm_maclock);

	/* in requested state; return success */
	dlokack(q, mp, on ? DL_PROMISCON_REQ : DL_PROMISCOFF_REQ);
	return (GLDE_OK);
}

/*
 * gld_sched (macinfo)
 *
 * This routine scans the streams that refer to a specific macinfo
 * structure and causes the STREAMS scheduler to try to run them if
 * they are marked as waiting for the transmit buffer.	The first such
 * message found will be queued to the hardware if possible.
 *
 * This routine is called at interrupt time after each interrupt is
 * delivered to the driver; it should be made to run Fast.
 *
 */
int
gld_sched(gld_mac_info_t *macinfo)
{
	register gld_t *gld, *first;
	mblk_t *mp = NULL;
	glddev_t *glddev = macinfo->gldm_dev;

	ASSERT(macinfo != NULL);
	ASSERT(glddev != NULL);
	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	rw_enter(&glddev->gld_rwlock, RW_READER);

	/*
	 * Be very careful messing with the handling of GLD_INTR_WAIT.
	 * It is not fully mutex protected.  See comment in gld_start().
	 */
	macinfo->gldm_GLD_flags &= ~GLD_INTR_WAIT;

	gld = macinfo->gldm_last;  /* The last one on which we failed */
	if (gld == NULL)
		gld = glddev->gld_str_next;	/* start with the first one */

	first = gld;		/* Remember where we started this time */
	do {
		if (gld == (gld_t *)&(glddev->gld_str_next))
			continue;	/* This is the list head, not a gld_t */
		if (gld->gld_mac_info != macinfo)
			continue;	/* This is not our device */
		if ((gld->gld_flags & GLD_XWAIT) == 0)
			continue;

		while ((mp = getq(WR(gld->gld_qptr)))) {
			if (DB_TYPE(mp) != M_DATA) {
				/*
				 * We can't help here -- put the mp
				 * back on the Q, which automatically
				 * does a qenable.
				 */
				(void) putbq(WR(gld->gld_qptr), mp);
				qenable(WR(gld->gld_qptr)); /* superfluous?? */
				mp = NULL;
				break;
			}

			if (gld_start(WR(gld->gld_qptr), mp, GLD_HAVELOCK, 1))
				break;
		}
		if (mp) {
			(void) putbq(WR(gld->gld_qptr), mp);
			macinfo->gldm_last = gld;
			macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
			break;
		}
		gld->gld_flags &= ~GLD_XWAIT;
		qenable(WR(gld->gld_qptr)); /* superfluous?? */

	} while ((gld = gld->gld_next) != first);	/* until we come */
							/* full circle */

	rw_exit(&glddev->gld_rwlock);
	return (0);
}

/*
 * gld_flushqueue (q)
 *	used by DLPI primitives that require flushing the queues.
 *	essentially, this is DL_UNBIND_REQ.
 */
static void
gld_flushqueue(queue_t *q)
{
	/* flush all data in both queues */
	flushq(q, FLUSHDATA);
	flushq(WR(q), FLUSHDATA);
	/* flush all the queues upstream */
	(void) putctl1(q, M_FLUSH, FLUSHRW);
}

/*
 * gld_intr (macinfo)
 *	run all interrupt handlers through here in order to simplify
 *	the mutex code and speed up scheduling.
 */
u_int
gld_intr(gld_mac_info_t *macinfo)
{
	int claimed;
	ASSERT(macinfo != NULL);
	if (!(macinfo->gldm_GLD_flags & GLD_INTR_READY))
		return (DDI_INTR_UNCLAIMED);   /* our mutex isn't inited yet! */

	mutex_enter(&macinfo->gldm_maclock);
	claimed = (*macinfo->gldm_intr)(macinfo);
	if (claimed == DDI_INTR_CLAIMED)
		(void) gld_sched(macinfo);
	mutex_exit(&macinfo->gldm_maclock);
	return (claimed);
}

/*
 * gld_devlookup (major)
 * search the device table for the device with specified
 * major number and return a pointer to it if it exists
 */
static glddev_t *
gld_devlookup(int major)
{
	struct glddevice *dev;

	for (dev = gld_device_list.gld_next;
	    dev != &gld_device_list;
	    dev = dev->gld_next) {
		ASSERT(dev);
		if (dev->gld_major == major)
			return (dev);
	}
	return (NULL);
}

/*
 * gldcrc32 (addr)
 * this provides a common multicast hash algorithm by doing a
 * 32bit CRC on the 6 octets of the address handed in.	Used by the National
 * chip set for Ethernet for multicast address filtering.  May be used by
 * others as well.
 */

ulong_t
gldcrc32(uchar_t *addr)
{
	register int i, j;
	union gldhash crc;
	unsigned char fb, ch;

	crc.value = (ulong_t)0xFFFFFFFF; /* initialize as the HW would */

	for (i = 0; i < LLC_ADDR_LEN; i++) {
		ch = addr[i];
		for (j = 0; j < 8; j++) {
			fb = crc.bits.a31 ^ ((ch >> j) & 0x01);
			crc.bits.a25 ^= fb;
			crc.bits.a22 ^= fb;
			crc.bits.a21 ^= fb;
			crc.bits.a15 ^= fb;
			crc.bits.a11 ^= fb;
			crc.bits.a10 ^= fb;
			crc.bits.a9 ^= fb;
			crc.bits.a7 ^= fb;
			crc.bits.a6 ^= fb;
			crc.bits.a4 ^= fb;
			crc.bits.a3 ^= fb;
			crc.bits.a1 ^= fb;
			crc.bits.a0 ^= fb;
			crc.value = (crc.value << 1) | fb;
		}
	}
	return (crc.value);
}

/*
 * version of insque/remque for use by this driver
 */
struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
	/* rest of structure */
};

static void
gldinsque(void *elem, void *pred)
{
	register struct qelem *pelem = elem;
	register struct qelem *ppred = pred;
	register struct qelem *pnext = ppred->q_forw;

	pelem->q_forw = pnext;
	pelem->q_back = ppred;
	ppred->q_forw = pelem;
	pnext->q_back = pelem;
}

static void
gldremque(void *arg)
{
	register struct qelem *pelem = arg;
	register struct qelem *elem = arg;

	pelem->q_forw->q_back = pelem->q_back;
	pelem->q_back->q_forw = pelem->q_forw;
	elem->q_back = elem->q_forw = NULL;
}

#ifdef notdef
/* VARARGS */
static void
glderror(dip, fmt, a1, a2, a3, a4, a5, a6)
	dev_info_t *dip;
	char   *fmt, *a1, *a2, *a3, *a4, *a5, *a6;
{
	static long last;
	static char *lastfmt;

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt))
		return;
	last = hrestime.tv_sec & ~1;
	lastfmt = fmt;

	cmn_err(CE_CONT, "%s%d:  ",
		ddi_get_name(dip), ddi_get_instance(dip));
	cmn_err(CE_CONT, fmt, a1, a2, a3, a4, a5, a6);
	cmn_err(CE_CONT, "\n");
}
#endif

#ifdef notdef
/* debug helpers -- remove in final version */
gld_dumppkt(mp)
	mblk_t *mp;
{
	int	i, total = 0, len;

	while (mp != NULL && total < 64) {
		cmn_err(CE_NOTE, "mb type %d len %d (%x/%x)", DB_TYPE(mp),
			MBLKL(mp), mp->b_rptr, mp->b_wptr);
		for (i = 0, len = MBLKL(mp); i < len; i++)
			cmn_err(CE_CONT, " %x", mp->b_rptr[i]);
		total += len;
		cmn_err(CE_CONT, "\n");
		mp = mp->b_cont;
	}
}
#endif

#ifdef notdef
void
gldprintf(str, a, b, c, d, e, f, g, h, i)
	char   *str;
{
	char	buff[256];

	(void) sprintf(buff, str, a, b, c, d, e, f, g, h, i);
	gld_prints(buff);
}
#endif

#ifdef notdef
void
gld_prints(char *s)
{
	if (!s)
		return;		/* sanity check for s == 0 */
	while (*s)
		cnputc (*s++, 0);
}
#endif

static void
gld_fastpath(gld_t *gld, queue_t *q, mblk_t *mp)
{
	dl_unitdata_req_t *udp;
	int len;
	gld_interface_t *ifp;
	mblk_t *nmp;
	gld_mac_info_t *macinfo;

	/*
	 * sanity check - we want correct state and valid message
	 */
	if (gld->gld_state != DL_IDLE || (mp->b_cont == NULL) ||
	    ((dl_unitdata_req_t *)(mp->b_cont->b_rptr))->dl_primitive !=
	    DL_UNITDATA_REQ) {
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	udp = (dl_unitdata_req_t *)(mp->b_cont->b_rptr);

	len = gld->gld_mac_info->gldm_addrlen +
					abs(gld->gld_mac_info->gldm_saplen);

	if (!MBLKIN(mp->b_cont, udp->dl_dest_addr_offset,
					udp->dl_dest_addr_length) ||
					udp->dl_dest_addr_length != len) {
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	macinfo = gld->gld_mac_info;

	ifp = ((gld_interface_pvt_t *)macinfo->gldm_interface)->interfacep;

	if ((nmp = (*ifp->mkfastpath)(gld, mp)) == NULL) {
		miocnak(q, mp, 0, ENOMEM);
		return;
	}

	gld->gld_flags |= GLD_FAST;

	/*
	 * Link new mblk in after the "request" mblks.
	 */

	linkb(mp, nmp);
	miocack(q, mp, msgdsize(mp->b_cont), 0);
}

/*
 * The saps are defined as matching
 *	if the packet sap and the streams sap are exactly the same
 *  or	the streams is in the SAP promiscuos mode
 *  or  both saps are less than or equal to GLD_802_SAP
 */
#define	SAPMATCH(sap, type, flags)					\
	((sap == type) ? 1 :						\
	((flags & GLD_PROM_SAP) ? 1 :					\
	((sap <= GLD_802_SAP) && (sap >= 0) && (type <= GLD_802_SAP)) ? 1 : 0))

/*
 * This function validates a packet for sending up a particular
 * stream. The message header has been parsed and its characteristic
 * are recorded in the pktinfo data structure. The streams stack info
 * are preseneted in gld data structures.
 */

static
gld_accept(gld_t *gld, pktinfo_t *pktinfo)
{
	/*
	 * if the saps do not match do not bother checking
	 * further.
	 */
	if (SAPMATCH(gld->gld_sap, pktinfo->Sap, gld->gld_flags) == 0) {
		return (0);
	}

	/*
	 * We don't accept any packet from the hardware if we originated it.
	 * (Contrast gld_paccept, the send-loopback accept function.)
	 */
	if (pktinfo->isLooped) {
		return (0);
	}

	/*
	 * If the packet is broadcast or sent to us directly we will accept it.
	 * Also we will accept multicast packets requested by the stream.
	 */

	if (pktinfo->isForMe || pktinfo->isBroadcast ||
	    gld_mcmatch(gld, pktinfo)) {
		return (1);
	}

	/*
	 * Finally, accept anything else if we're in promiscuous mode
	 */

	if (gld->gld_flags & GLD_PROM_PHYS) {
		return (1);
	}

	return (0);
}

static
gld_paccept(gld_t *gld, pktinfo_t *pktinfo)
{
	return SAPMATCH(gld->gld_sap, pktinfo->Sap, gld->gld_flags) &&
			(gld->gld_flags & GLD_PROM_PHYS);
}

/*
 * Return TRUE if the given multicast address is one
 * of those that this particular Stream is interested in.
 */
static int
gld_mcmatch(gld_t *gld, pktinfo_t *pktinfo)
{
	/*
	 * Return FALSE if not a multicast address.
	 */
	if (!pktinfo->isMulticast)
		return (0);

	/*
	 * Check if all multicasts have been enabled for this Stream
	 */
	if (gld->gld_flags & GLD_PROM_MULT)
		return (1);

	/*
	 * Return FALSE if no multicast addresses enabled for this Stream.
	 */
	if (!gld->gld_mcast)
		return (0);

	/*
	 * Otherwise, find it in the table.
	 */
	return (gld_multicast(pktinfo->dhost, gld));
}
