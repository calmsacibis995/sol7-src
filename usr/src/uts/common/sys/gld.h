/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 */

/*
 * Warning - This file is not an approved Public Interface.
 *           It may change or disappear at any time.
 */

/*
 * gld - a generic LAN driver support system for drivers using the DLPI
 * interface.
 *
 * Copyrighted as an unpublished work. (c) Copyright 1992 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_GLD_H
#define	_SYS_GLD_H

#pragma ident	"@(#)gld.h	1.19	98/01/06 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	ETHERADDRL
#define	ETHERADDRL	6
#endif

/*
 * structure for driver statistics
 */
struct gld_stats {
	ulong_t	glds_multixmt;
	ulong_t	glds_multircv;	/* multicast but not broadcast */
	ulong_t	glds_brdcstxmt;
	ulong_t	glds_brdcstrcv;
	ulong_t	glds_blocked;	/* discard due to upstream being flow */
				/* controlled */
	ulong_t	glds_pktxmt;
	ulong_t	glds_pktrcv;
	ulong_t	glds_bytexmt;
	ulong_t	glds_bytercv;
	ulong_t	glds_errxmt;
	ulong_t	glds_errrcv;
	ulong_t	glds_collisions;
	ulong_t	glds_excoll;
	ulong_t	glds_defer;
	ulong_t	glds_frame;
	ulong_t	glds_crc;
	ulong_t	glds_overflow;
	ulong_t	glds_underflow;
	ulong_t	glds_short;
	ulong_t	glds_missed;
	ulong_t	glds_xmtlatecoll;
	ulong_t	glds_nocarrier;
	ulong_t	glds_noxmtbuf;
	ulong_t	glds_norcvbuf;
	ulong_t	glds_intr;
	ulong_t	glds_xmtretry;
};

/*
 * structure for names stat structure usage as required by "netstat"
 *
 * The below definition of gldkstat should not be used by the device
 * dependent driver, but is here for now.  It will be moved to gldpriv.h
 * in a future release.
 */
struct gldkstats {
	kstat_named_t	glds_pktxmt;
	kstat_named_t	glds_pktrcv;
	kstat_named_t	glds_errxmt;
	kstat_named_t	glds_errrcv;
	kstat_named_t	glds_collisions;
	kstat_named_t	glds_bytexmt;
	kstat_named_t	glds_bytercv;
	kstat_named_t	glds_multixmt;
	kstat_named_t	glds_multircv;	/* multicast but not broadcast */
	kstat_named_t	glds_brdcstxmt;
	kstat_named_t	glds_brdcstrcv;
	kstat_named_t	glds_blocked;	/* discard due to upstream flow */
					/* control */
	kstat_named_t	glds_excoll;
	kstat_named_t	glds_defer;
	kstat_named_t	glds_frame;
	kstat_named_t	glds_crc;
	kstat_named_t	glds_overflow;
	kstat_named_t	glds_underflow;
	kstat_named_t	glds_short;
	kstat_named_t	glds_missed;
	kstat_named_t	glds_xmtlatecoll;
	kstat_named_t	glds_nocarrier;
	kstat_named_t	glds_noxmtbuf;
	kstat_named_t	glds_norcvbuf;
	kstat_named_t	glds_intr;
	kstat_named_t	glds_xmtretry;
};

/* multicast structures */
/*
 * The below definition of gld_mcast_t should not be used by the device
 * dependent driver, but is here for now.  It will be moved to gldpriv.h
 * in a future release.  Neither should GLD_MAX_MULTICAST be used by
 * any device-dependent driver.
 */
typedef struct gld_multicast_addr {
	int		gldm_refcnt;	/* number of streams referring */
					/* to entry */
	unsigned char	gldm_addr[ETHERADDRL];
} gld_mcast_t;
#define	GLD_MAX_MULTICAST	16	/* default max multicast table size */

/*
 * gld_mac_info structure.  Used to define the per-board data for all
 * drivers.
 *
 * The below definition of gld_mac_info contains PRIVATE or obsolete
 * entries that should not be used by the device dependent driver, but
 * are here for now.  They will be moved to a private structure in a
 * future release.
 */
typedef
struct gld_mac_info {
	struct gld_mac_info *gldm_next, *gldm_prev;	/* GLD PRIVATE */
	struct gld	*gldm_last;	/* last scheduled stream -- GLD */
					/* PRIVATE */
	struct glddevice *gldm_dev;	/* pointer to device base -- GLD */
					/* PRIVATE */
	long		gldm_version;	/* Currently UNUSED, must be zero */
	short		gldm_GLD_flags;	/* GLD PRIVATE */
	short		gldm_options;	/* GLD register options -- driver set */
	dev_info_t	*gldm_devinfo;	/* SET BY GLD, DRIVER MAY USE */
	kmutex_t	gldm_intrlock;	/* UNUSED, GOING AWAY */
	kmutex_t	gldm_maclock;	/* SET BY GLD, DRIVER MAY USE */
	ddi_iblock_cookie_t gldm_cookie;	/* SET BY GLD, DRIVER MAY USE */
	long		gldm_flags;	/* PRIVATE TO DRIVER (obsolete) */
	long		gldm_state;	/* PRIVATE TO DRIVER (obsolete) */
	long		gldm_maxpkt;
	long		gldm_minpkt;
	char		*gldm_ident;
	long		gldm_type;
	unsigned long	gldm_media;
	long		gldm_addrlen;	/* usually 6 but could be 2 */
	long		gldm_saplen;
	unsigned char	gldm_macaddr[ETHERADDRL];
	unsigned char	gldm_vendor[ETHERADDRL];
	unsigned char	gldm_broadcast[ETHERADDRL];
	long		gldm_ppa;	/* PPA number -- GLD PRIVATE */
	off_t		gldm_reg_offset;	/* used to find base of real */
						/* shared ram (obsolete) */
	long		gldm_nstreams;	/* GLD PRIVATE */
	unsigned short	gldm_nprom;	/* num streams in promiscuous */
					/* mode--GLD PRIVATE */
	unsigned short	gldm_nprom_multi;	/* num streams in promiscuous */
						/* multicast mode -- GLD PVT */
	long		gldm_port;	/* I/O port address -- PRIVATE TO */
					/* DRIVER (obsolete) */
	caddr_t		gldm_memp;	/* SET BY GLD, DRIVER MAY USE */
	long		gldm_reg_index;	/* SET BY DRIVER FOR GLD (obsolete) */
	off_t		gldm_reg_len;	/* SET BY DRIVER FOR GLD (obsolete) */
	long		gldm_irq_index;	/* SET BY DRIVER FOR GLD (2.6 MBZ) */
	caddr_t		gldm_interface;	/* Interface type specific -- GLD */
	gld_mcast_t	*gldm_mcast;	/* per device multicast table -- GLD */
					/* PRIVATE */
	struct gld_stats gldm_stats;
	struct gldkstats gldm_kstats;	/* GLD PRIVATE */
	kstat_t		*gldm_kstatp;	/* GLD PRIVATE */
	caddr_t		gldm_private;	/* board private data -- PRIVATE TO */
					/* DRIVER */
	int		(*gldm_reset)();	/* reset procedure */
	int		(*gldm_start)();	/* start board */
	int		(*gldm_stop)();		/* stop board completely */
	int		(*gldm_saddr)();	/* set physical address */
	int		(*gldm_send)();		/* transmit procedure */
	int		(*gldm_prom)();		/* set promiscuous mode */
	int		(*gldm_gstat)();	/* get board statistics */
	int		(*gldm_ioctl)();	/* Driver specific ioctls */
	int		(*gldm_sdmulti)();	/* set/delete multicast */
						/* address */
	uint_t		(*gldm_intr)();		/* interrupt handler */
	int		(*NOgldm_inform)();	/* UNUSED, GOING AWAY */
} gld_mac_info_t;

/* gldm_GLD_flags */
#define	GLD_INTR_READY 0x0001	/* safe to call interrupt routine, PRIVATE */
#define	GLD_INTR_WAIT 0x0002	/* waiting for interrupt to do scheduling */

/* gldm_options */
#define	GLDOPT_FAST_RECV	0x40
#define	GLDOPT_CMD_ACK		0x20	/* DO NOT USE */
#define	GLDOPT_DONTFREE		0x10
#define	GLDOPT_CANONICAL_ADDR	0x08
#define	GLDOPT_RW_LOCK		0x04	/* DO NOT USE */
#define	GLDOPT_DRIVER_PPA	0x02
#define	GLDOPT_PCMCIA		0x01	/* DO NOT USE */

/*
 * The below definitions should not be used, but are here for now because
 * some older drivers may (incorrectly) use them.  They will be removed
 * or moved to gldpriv.h in a future release.
 */

/* flags for mac info (hardware) status */
#define	GLD_PROMISC	0x0010	/* hardware is in promiscous mode */
#define	GLD_IN_INTR	0x0020	/* in the interrupt mutex area */

/* flags for physical promiscuous state */
#define	GLD_MAC_PROMISC_PHYS	0x0001	/* receive all packets */
#define	GLD_MAC_PROMISC_MULTI	0x0002	/* receive all multicast packets */

/* PPA number mask */
#define	GLD_PPA_MASK	0x3f
#define	GLD_PPA_INIT	0x40
#define	GLD_USE_STYLE2	0

/*
 * gld structure.  Used to define the per-stream information required to
 * implement DLPI.
 */
typedef struct gld {
	struct gld	*gld_next, *gld_prev;
	mblk_t		*gld_mb;
	long		gld_state;
	long		gld_style;
	long		gld_minor;
	long		gld_type;
	long		gld_sap;
	long		gld_flags;	/* flags used for controlling things */
	long		gld_multicnt;	/* number of multicast addresses for */
					/* stream */
	gld_mcast_t	**gld_mcast;	/* multicast table if multicast is */
					/* enabled */
	queue_t		*gld_qptr;
	kmutex_t	gld_lock;
	struct gld_mac_info *gld_mac_info;
	struct gld_stats *gld_stats;
	struct glddevice *gld_device;
} gld_t;

/* gld_flag bits */
#define	GLD_RAW		0x0001	/* lower stream is in RAW mode */
#define	GLD_FAST	0x0002	/* use "fast" path */
#define	GLD_PROM_PHYS	0x0004	/* stream is in physical promiscuous mode */
#define	GLD_PROM_SAP	0x0008
#define	GLD_PROM_MULT	0x0010
#define	GLD_XWAIT	0x0020	/* waiting for transmitter */
#define	GLD_LOCKED	0x0040	/* queue is locked (mutex) */
#define	GLD_WAIT_ACK	0x0080	/* device busy, we are waiting for an ack */

/* special case SAP values */
#define	GLD_802_SAP	1500
#define	GLDMAXETHERSAP	0xFFFF
#define	GLD_MAX_802_SAP 0xFF

/*
 * media type This identifies the media/connector used by the LAN type of the
 * driver.  Possible types will be defined per the DLPI type defined in
 * gldm_type.  The below definitions should be used by the device dependent
 * drivers to set gldm__media.
 */
/* if driver cannot determine media/connector type  */
#define	GLDM_UNKNOWN	0

#define	GLDM_AUI	1
#define	GLDM_BNC	2
#define	GLDM_TP		3
#define	GLDM_FIBER	4
#define	GLDM_100BT	5
#define	GLDM_VGANYLAN	6
#define	GLDM_10BT	7
#define	GLDM_RING4	8
#define	GLDM_RING16	9

/*
 * definitions for the per driver class structure
 *
 * The glddevice structure should not be used by the device dependent
 * drivers.  It will be moved to gldpriv.h in a future release.
 */
typedef struct glddevice {
	struct glddevice *gld_next, *gld_prev;
	char		gld_name[16];	/* name of device */
	long		gld_status;
	krwlock_t	gld_rwlock;	/* used to serialize read/write locks */
	int		gld_minors;
	int		gld_major;
	int		gld_multisize;
	int		gld_type;	/* for use before attach */
	int		gld_minsdu;
	int		gld_maxsdu;
	gld_mac_info_t	*gld_mac_next, *gld_mac_prev;	/* the various mac */
							/* layers */
	int		gld_ndevice;	/* number of devices linked */
	int		gld_nextppa;	/* number to use for next PPA default */
	gld_t		*gld_str_next, *gld_str_prev;	/* open streams */
} glddev_t;

#define	GLD_ATTACHED	0x0001	/* board is attached so mutexes are */
				/* initialized */


/*
 * definitions for debug tracing
 */
#define	GLDTRACE	0x0001	/* basic procedure level tracing */
#define	GLDERRS		0x0002	/* trace errors */
#define	GLDRECV		0x0004	/* trace receive path */
#define	GLDSEND		0x0008	/* trace send path */
#define	GLDPROT		0x0010	/* trace DLPI protocol */
#define	GLDNOBR		0x0020	/* do no show broadcast messages */

/*
 * The below definitions should not be used, but are here for now because
 * some older drivers may (incorrectly) use them.  They will be removed
 * or moved to gldpriv.h in a future release.
 */

/*
 * other definitions
 */
#define	GLDE_OK		-1	/* internal procedure status is OK */
#define	GLDE_NOBUFFER	0x1001	/* couldn't allocate a buffer */
#define	GLDE_RETRY	0x1002	/* want to retry later */


/*
 * definitions for module_info
 */
#define	GLDIDNUM	0x8020

#define	ismulticast(cp) ((*(caddr_t)(cp)) & 0x01)

/* define structure for DLSAP value parsing */
struct gld_dlsap {
	unsigned char   glda_addr[ETHERADDRL];
	unsigned short  glda_sap;
};

#define	DLSAP(p, offset) ((struct gld_dlsap *)((caddr_t)(p)+offset))

/* union used in calculating hash values */
union gldhash {
	unsigned long   value;
	struct {
		unsigned	a0:1;
		unsigned	a1:1;
		unsigned	a2:1;
		unsigned	a3:1;
		unsigned	a4:1;
		unsigned	a5:1;
		unsigned	a6:1;
		unsigned	a7:1;
		unsigned	a8:1;
		unsigned	a9:1;
		unsigned	a10:1;
		unsigned	a11:1;
		unsigned	a12:1;
		unsigned	a13:1;
		unsigned	a14:1;
		unsigned	a15:1;
		unsigned	a16:1;
		unsigned	a17:1;
		unsigned	a18:1;
		unsigned	a19:1;
		unsigned	a20:1;
		unsigned	a21:1;
		unsigned	a22:1;
		unsigned	a23:1;
		unsigned	a24:1;
		unsigned	a25:1;
		unsigned	a26:1;
		unsigned	a27:1;
		unsigned	a28:1;
		unsigned	a29:1;
		unsigned	a30:1;
		unsigned	a31:1;
	} bits;
};

/*
 * miscellaneous linkage glue
 * May be used by the device dependent driver.
 */
#define	DEPENDS_ON_GLD	char _depends_on[] = "misc/gld"

/*
 * defines to make porting older ISC LLC drivers to GLD easier
 * Should not be used in any new drivers.
 */
#define	llcp_int gldm_irq
#define	LLC_ADDR_LEN ETHERADDRL
#define	GLD_EHDR_SIZE sizeof (struct ether_header)
#define	LOW(x) ((x)&0xFF)
#define	HIGH(x) (((x)>>8)&0xFF)

#if defined(_KERNEL)
extern int gld_open(queue_t *q, dev_t *dev, int flag, int sflag,
		cred_t *cred);
extern int gld_close(queue_t *q, int flag, cred_t *cred);
extern int gld_wput(queue_t *q, mblk_t *mp);
extern int gld_wsrv(queue_t *q);
extern int gld_rsrv(queue_t *q);
extern int gld_ioctl(queue_t *q, mblk_t *mp);
extern int gld_recv(gld_mac_info_t *macinfo, mblk_t *mp);
extern int gld_register(dev_info_t *, char *, gld_mac_info_t *);
extern int gld_unregister(gld_mac_info_t *);
extern int gld_sched(gld_mac_info_t *macinfo);
extern uchar_t  gldbroadcastaddr[];
extern ulong_t  gldcrc32(uchar_t *);
#endif

/*
 * EISA support functions
 *
 * The below definitions should not be used, but are here for now because
 * some older drivers may (incorrectly) use them.  They will be removed
 * or moved to gldpriv.h in a future release.
 */

#define	gldnvm(ptr) ((NVM_SLOTINFO *)ptr)
#define	gld_boardid(nvm) (*(ushort_t *)(gldnvm(nvm)->boardid))
#define	gld_check_boardid(nvm, id) (gld_boardid(nvm) == id)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_GLD_H */
