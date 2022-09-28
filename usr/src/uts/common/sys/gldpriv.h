/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 */

/*
 * gldpriv.h - Private interfaces/structures needed by gld.c
 *
 * Copyrighted as an unpublished work. (c) Copyright 1995 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_GLDPRIV_H
#define	_SYS_GLDPRIV_H

#pragma ident	"@(#)gldpriv.h	1.7	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


typedef uchar_t mac_addr_t[6];


/*
 * Compare two Ethernet addresses - assumes that the two given
 * pointers can be referenced as shorts.  On architectures
 * where this is not the case, use bcmp instead.  Note that unlike
 * bcmp, we return zero if they are different.
 */

/*
 * We can not depend on a definitive requirements for alignment on this
 * architecture. We will guess using the machine types.
 */
#ifndef	_ALIGNMENT_REQUIRED
#if defined(i386)
#define	_ALIGNMENT_REQUIRED	0
#elif defined(sparc) || defined(__sparc)
#define	_SHORT_ALIGNMENT	2
#else

/*
 * This expensive, but not important any new architecture
 * will have a definitive alignment requirement liste in
 * isa_defs.h
 */

#define	_SHORT_ALIGNMENT	4
#endif
#endif

/*
 * For 2.5 and beyond we use definitive feature test macros.
 * We depend on long being 4 bytes and short, 2. Once this
 * assumption is rendered wrong, we've got bigger problems.
 */


#if (_ALIGNMENT_REQUIRED == 0)

#define	mac_eq(a, b)							\
	(((short *)(b))[2] == ((short *)(a))[2] &&			\
	((long *)(b))[0] == ((long *)(a))[0])

#define	mac_copy(a, b) {						\
	((long *)(b))[0] = ((long *)(a))[0];				\
	((short *)(b))[2] = ((short *)(a))[2];				\
	}

#elif (_SHORT_ALIGNMENT == 2)

#define	mac_eq(a, b)							\
	(((short *)(b))[2] == ((short *)(a))[2] &&			\
	((short *)(b))[1] == ((short *)(a))[1] &&			\
	((short *)(b))[0] == ((short *)(a))[0])

#define	mac_copy(a, b) {						\
	((short *)(b))[0] = ((short *)(a))[0];				\
	((short *)(b))[1] = ((short *)(a))[1];				\
	((short *)(b))[2] = ((short *)(a))[2];				\
	}

#else /* Alignment requirements too restrictive */

#define	mac_eq(a, b) (bcmp((caddr_t)a, (caddr_t)b, 6) == 0)
#define	mac_copy(a, b) (bcopy((caddr_t)a, (caddr_t)b, 6))

#endif

#define	cmac_copy(a, b, macinfo) {					\
	    if ((macinfo)->gldm_options & GLDOPT_CANONICAL_ADDR)	\
		gld_bconvcopy((caddr_t)(a), (caddr_t)(b), 6);		\
	    else							\
		mac_copy((a), (b));					\
	}

/*
 * Structure of a FDDI MAc frame.
 */
struct	fddi_mac_frame {
	uchar_t		fddi_fc;
	mac_addr_t	fddi_dhost;
	mac_addr_t	fddi_shost;
};

struct llc_snap_hdr {
	uchar_t d_lsap;			/* destination service access point */
	uchar_t s_lsap;			/* source link service access point */
	uchar_t control;		/* short control field */
	uchar_t org[3];			/* Ethernet style organization field */
	ushort_t type;			/* Ethernet style type field */
};

#define	FDDI_LLC_FC	0x50
#define	FDDI_LLC_MASK	0x78
#define	FDDI_NSA	0x4F
#define	FDDI_SMTINFO	0x41

#define	FDDI_VOID_FC	0x40	/* Fddi void FC */
#define	FDDI_LLC_FC	0x50	/* Fddi llc FC */
#define	FDDI_BEACON_FC	0xC2	/* Fddi beacon FC */
#define	FDDI_CLAIM_FC	0xC3	/* Fddi claim FC */

/*
 * MAC, LLC, and SNAP defines
 */
#define	MAC_ADDR_LEN		6	/* Length of 802(.3/.4/.5) address */
#define	ACFC_LEN		2	/* Length of AC + FC field */
#define	LLC_SAP_LEN		1	/* Length of sap only field */
#define	LLC_CNTRL_LEN		1	/* Length of control field */
#define	LLC_LSAP_LEN		2	/* Length of sap/type field  */
#define	LLC_SNAP_LEN		5	/* Length of LLC SNAP fields */
#define	LLC_8022_HDR_LEN	3	/* Full length of plain 802.2 header */
#define	LLC_SNAP_HDR_LEN	8	/* Full length of SNAP header */

/* Length of MAC address fields */
#define	MAC_HDR_LEN	(ACFC_LEN+MAC_ADDR_LEN+MAC_ADDR_LEN)

/* Length of 802.2 LLC Header */
#define	LLC_HDR_LEN	(MAC_HDR_LEN+LLC_SAP_LEN+LLC_SAP_LEN+LLC_CNTRL_LEN)

/* Length of extended LLC header with SNAP fields */
#define	LLC_EHDR_LEN	(LLC_HDR_LEN + LLC_SNAP_LEN)

#define	MAX_ROUTE_FLD	30	/* Maximum of 30 bytes of routing info */
#define	MAX_RDFLDS	14	/* changed to 14 from 8 as per IEEE */

#define	RT_SRF		0x0		/* specifically routed frame */
#define	RT_APE		0x4		/* all paths explorer frame */
#define	RT_STE		0x6		/* spanning tree explorer frame */
#define	RT_STF		0x7		/* spanning tree routed frame */

/*
 * Source Routing Route Information field.
 */
struct tr_ri {
#if defined(_LITTLE_ENDIAN)
	uchar_t len:5;			/* length */
	uchar_t rt:3;			/* routing type */
	uchar_t res:4;			/* reserved */
	uchar_t mtu:3;			/* largest frame */
	uchar_t dir:1;			/* direction bit */
	struct tr_rd {			/* route designator fields */
		ushort_t bridge:4;
		ushort_t ring:12;
	} rd[MAX_RDFLDS];
#elif defined(_BIG_ENDIAN)
	uchar_t rt:3;			/* routing type */
	uchar_t len:5;			/* length */
	uchar_t dir:1;			/* direction bit */
	uchar_t mtu:3;			/* largest frame */
	uchar_t res:4;			/* reserved */
	struct tr_rd {			/* route designator fields */
		ushort_t ring:12;
		ushort_t bridge:4;
	} rd[MAX_RDFLDS];
#else
#error	"what endian is this machine!"
#endif
};

/*
 * Structure of a Token Ring MAC frame including full routing info
 */

struct tr_mac_frm_nori {
	uchar_t		tr_ac;
	uchar_t		tr_fc;
	mac_addr_t	tr_dhost;
	mac_addr_t	tr_shost;
};

struct tr_mac_frm {
	uchar_t		tr_ac;
	uchar_t		tr_fc;
	mac_addr_t	tr_dhost;
	mac_addr_t	tr_shost;
	struct tr_ri	tr_ri;
};

typedef struct pktinfo {
	uint_t		isBroadcast:1;
	uint_t		isMulticast:1;
	uint_t		isSMT:1;
	uint_t		isLooped:1;
	uint_t		isForMe:1;
	uint_t		hasLLC:1;
	uint_t		hasSnap:1;
	uint_t		Sap;
	uint_t		macLen;
	uint_t		hdrLen;
	uint_t		pktLen;
	mac_addr_t	dhost;
	mac_addr_t	shost;
} pktinfo_t;

/*
 * Describes characteristics of the Media Access Layer.
 * The mac_type is one of the supported DLPI media
 * types (see <sys/dlpi.h>).
 * The mtu_size is the size of the largest frame.
 * The header length is returned by a function to
 * allow for variable header size - for ethernet it's
 * just a constant 14 octets.
 * The interpreter is the function that "knows" how
 * to interpret the frame.
 */

typedef struct {
	uint_t	mac_type;
	uint_t	mtu_size;
	int	(*interpreter)(gld_mac_info_t *, mblk_t *, pktinfo_t *);
	mblk_t	*(*mkfastpath)(gld_t *, mblk_t *);
	mblk_t	*(*mkunitdata)(gld_t *, mblk_t *);
	void	(*init)(gld_mac_info_t *);
	void	(*uninit)(gld_mac_info_t *);
	uint_t	mac_hdr_fixed_size;
} gld_interface_t;

typedef	struct {
	gld_interface_t	*interfacep;
	kmutex_t	datalock;	/* data lock */
	caddr_t		data;
	} gld_interface_pvt_t;

#define	IF_HDR_FIXED	0
#define	IF_HDR_VAR	1

#define	MAX_RDFLDS	14		/* changed to 14 from 8 as per IEEE */
#define	TR_FN_ADDR	0x80		/* dest addr is functional */
#define	TR_SR_ADDR	0x80		/* MAC utilizes source route */
#define	ACFCDASA_LEN	14		/* length of AC|FC|DA|SA */
#define	TR_MAC_MASK	0xc0
#define	TR_AC		0x10		/* Token Ring access control */
#define	TR_LLC_FC	0x40		/* Token Ring llc frame control */
#define	LSAP_SNAP	0xaa
#define	LLC_SNAP_HDR_LEN	8
#define	LLC_HDR1_LEN	3		/* DON'T use sizeof(struct llc_hdr1) */
#define	CNTL_LLC_UI	0x03		/* un-numbered information packet */

/*
 * Source Route Cache table
 */
#define	SR_HASH_SIZE	256		/* Number of bins */
#define	SR_TIMEOUT	8		/* in mins */

/*
 * Source route table info
 */
struct srtab {
	struct srtab	*sr_next;		/* next in linked list */
	gld_mac_info_t	*sr_instance;		/* associated tr structure */
	mac_addr_t	sr_mac;			/* MAC address */
	struct		tr_ri sr_ri;		/* routing information */
	uint_t		sr_flags;		/* defined below */
	uint_t		sr_timer;
};

/*
 * Values for st_flags;
 */
#define	SRF_PENDING	0x01		/* waiting for response */
#define	SRF_RESOLVED	0x02		/* all is well */
#define	SRF_LOCAL	0x04		/* local, don't use source route */
#define	SRF_DYING	0x08		/* wainting for delete */
#define	SRF_PERMANENT	0x10		/* permanent entry do not delete */

#define	GLD_LOCK	0
#define	GLD_TRYLOCK	1
#define	GLD_HAVELOCK	2

struct ether_mac_frm {
	mac_addr_t	ether_dhost;
	mac_addr_t	ether_shost;
	ushort_t	ether_type;
};

typedef	struct {
	mac_addr_t	dl_phys;
	ushort_t	dl_sap;
} dladdr_t;

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_GLDPRIV_H */
