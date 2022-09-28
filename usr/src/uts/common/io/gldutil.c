/*
 * Copyright (c) 1992, 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gldutil.c	1.15	97/05/22 SMI"

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
#include <sys/byteorder.h>
#include <sys/strsun.h>
#include <sys/dlpi.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/gld.h>
#include <sys/gldpriv.h>

#define	TR_SR_VAR(macinfo)	\
	(((gld_interface_pvt_t *)macinfo->gldm_interface)->data)

#define	TR_SR_HASH(macinfo)	((struct srtab **)TR_SR_VAR(macinfo))

#define	TR_SR_MUTEX(macinfo)	\
	(&((gld_interface_pvt_t *)macinfo->gldm_interface)->datalock)

#define	TR_SR_LOCK(macinfo)	mutex_enter(TR_SR_MUTEX(macinfo))

#define	TR_SR_UNLOCK(macinfo)	mutex_exit(TR_SR_MUTEX(macinfo))

static mac_addr_t ether_broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

extern void gld_bconvcopy(caddr_t src, caddr_t target, size_t n);

static struct srtab **tr_sr_hash(struct srtab **, mac_addr_t, int addr_length);
static struct srtab *tr_sr_lookup_entry(gld_mac_info_t *, mac_addr_t);
static struct srtab *tr_sr_lookup(gld_mac_info_t *, mac_addr_t);
static struct srtab *tr_sr_create_entry(gld_mac_info_t *, mac_addr_t, int);
static int tr_machdr_len(gld_mac_info_t *, char *, int *, int *);
static void tr_sr_clear(gld_mac_info_t *);

int
gld_interpret_ether(gld_mac_info_t *macinfo, mblk_t *mp, pktinfo_t *pktinfo)
{
	struct ether_mac_frm *e = (struct ether_mac_frm *)mp->b_rptr;
#if 0
	char	*off;
#endif

	bzero((void *)pktinfo, sizeof (*pktinfo));

	pktinfo->pktLen = msgdsize(mp);

	if (pktinfo->pktLen < 14)
		return (-1);

	/*
	 * Deal with the mac header:
	 * 	Copy the mac header.
	 *	Check to see if the mac is a broadcast or multicast address.
	 */

	mac_copy(e->ether_dhost, pktinfo->dhost);
	mac_copy(e->ether_shost, pktinfo->shost);

	if (mac_eq(pktinfo->dhost, ether_broadcast))
		pktinfo->isBroadcast = 1;
	else if (pktinfo->dhost[0] & 1)
		pktinfo->isMulticast = 1;

	pktinfo->isLooped = mac_eq(pktinfo->shost, macinfo->gldm_macaddr);
	pktinfo->isForMe = mac_eq(pktinfo->dhost, macinfo->gldm_macaddr);

	pktinfo->macLen = sizeof (struct ether_mac_frm);

	pktinfo->Sap = ntohs(e->ether_type);

	if (pktinfo->Sap <= GLD_802_SAP) {
		/*
		 * Packet is 802.3 so the ether type/length field
		 * specifies the number of bytes that should be present
		 * in the data field.  Additional bytes are padding, and
		 * should be removed
		 */
		int delta = pktinfo->pktLen -
		    (sizeof (struct ether_mac_frm) + pktinfo->Sap);

		if (delta > 0 && adjmsg(mp, -delta))
			pktinfo->pktLen -= delta;
	}

#if 0
	/*
	 * Including this code causes the Appletalk protocol to
	 * stop working.  Instead we will use the old behaviour
	 * of passing all 802.3 packets up all streams bound
	 * to a SAP <= 1500
	 */
	/*
	 * Check SAP/SNAP information.
	 * 	For ethernet devices this is not striclty speaking all that
	 * 	useful, since we do not honor the SNAP header, i.e. we
	 * 	seem to alway leave the snap header in place.
	 */

	if (pktinfo->Sap <= GLD_MAX_802_SAP) {

		struct	llc_snap_hdr *snaphdr;

		off = (char *)(e + 1);

		/*
		 * See if an IEEE 802.3 packet.
		 * Should be DSAP=0xaa, SSAP=0xaa, control=3
		 * then three padding bytes of zero,
		 * followed by a normal ethernet-type packet.
		 */

		snaphdr = (struct llc_snap_hdr *)off;
		pktinfo->hasLLC = 1;
		if (snaphdr->d_lsap == LSAP_SNAP &&
					snaphdr->s_lsap == LSAP_SNAP &&
					snaphdr->control == CNTL_LLC_UI) {
			pktinfo->hasSnap = 1;
			pktinfo->Sap = ntohs(snaphdr->type);
			pktinfo->hdrLen = sizeof (struct llc_snap_hdr);
		}
	}
#endif
	return (0);
}

int
gld_interpret_fddi(gld_mac_info_t *macinfo, mblk_t *mp, pktinfo_t *pktinfo)
{
	struct fddi_mac_frame *f = (struct fddi_mac_frame *)mp->b_rptr;
	struct llc_snap_hdr snaphdr;

	bzero((void *)pktinfo, sizeof (*pktinfo));

	pktinfo->pktLen = msgdsize(mp);

	if (pktinfo->pktLen < 13)
		return (-1);

	/*
	 * Deal with the mac header:
	 * 	Copy the mac header.
	 *	Check to see if the mac is a broadcast or multicast address.
	 */

	cmac_copy(f->fddi_dhost, pktinfo->dhost, macinfo);
	cmac_copy(f->fddi_shost, pktinfo->shost, macinfo);

	if (mac_eq(pktinfo->dhost, ether_broadcast))
		pktinfo->isBroadcast = 1;
	else if (pktinfo->dhost[0] & 0x01)
		pktinfo->isMulticast = 1;

	pktinfo->isLooped = mac_eq(pktinfo->shost, macinfo->gldm_macaddr);
	pktinfo->isForMe = mac_eq(pktinfo->dhost, macinfo->gldm_macaddr);
	pktinfo->macLen = sizeof (struct fddi_mac_frame);

	/*
	 * Check SAP/SNAP information.
	 * 	For ethernet devices this is not striclty speaking all that
	 * 	useful, since we do not honor the SNAP header, i.e. we
	 * 	seem to alway leave the snap header in place.
	 */

	if ((f->fddi_fc & FDDI_LLC_MASK) == FDDI_LLC_FC) {
		pktinfo->hasLLC = 1;
		bcopy((char *)mp->b_rptr + sizeof (struct fddi_mac_frame),
			(char *)&snaphdr,
			sizeof (snaphdr));
		if (snaphdr.d_lsap == LSAP_SNAP &&
					snaphdr.s_lsap == LSAP_SNAP &&
					snaphdr.control == CNTL_LLC_UI) {
			pktinfo->hasSnap = 1;
		}
	} else {
		if ((f->fddi_fc & FDDI_SMTINFO) == FDDI_SMTINFO ||
					(f->fddi_fc & FDDI_NSA) == FDDI_NSA) {
			pktinfo->isSMT = 1;
		}
	}


	if (pktinfo->hasSnap)
		pktinfo->Sap = ntohs(snaphdr.type);
	else if (pktinfo->hasLLC)
		pktinfo->Sap = snaphdr.d_lsap;
	else
		/*
		 * What's the right thing to do here.
		 */
		pktinfo->Sap = 0;

	if (pktinfo->hasLLC) {
		if (pktinfo->hasSnap) {
			pktinfo->hdrLen = LLC_SNAP_HDR_LEN;
		} else {
			pktinfo->hdrLen = LLC_HDR1_LEN;
		}
	}
	return (0);
}

static mac_addr_t tokenbroadcastaddr2 = { 0xc0, 0x00, 0xff, 0xff, 0xff, 0xff };

int
gld_interpret_tr(gld_mac_info_t *macinfo, mblk_t *mp, pktinfo_t *pktinfo)
{
	struct tr_mac_frm_nori *mh = (struct tr_mac_frm_nori *)mp->b_rptr;
	struct llc_snap_hdr *snaphdr;
	int maclen;
	int sr = 0;

	bzero((void *)pktinfo, sizeof (*pktinfo));

	pktinfo->pktLen = msgdsize(mp);

	if (pktinfo->pktLen < ACFCDASA_LEN)
		return (-1);

	/*
	 * Deal with the mac header:
	 * 	Copy the mac addresses.
	 *	Check to see if the mac is a broadcast or multicast address.
	 */

	mac_copy(mh->tr_dhost, pktinfo->dhost);
	mac_copy(mh->tr_shost, pktinfo->shost);

	if (mac_eq(pktinfo->dhost, ether_broadcast) ||
				mac_eq(pktinfo->dhost, tokenbroadcastaddr2))
		pktinfo->isBroadcast = 1;
	else if (pktinfo->dhost[0] & TR_FN_ADDR)
		pktinfo->isMulticast = 1;

	pktinfo->isLooped = mac_eq(pktinfo->shost, macinfo->gldm_macaddr);
	pktinfo->isForMe = mac_eq(pktinfo->dhost, macinfo->gldm_macaddr);

	/*
	 * Check SAP/SNAP information.
	 * 	For ethernet devices this is not striclty speaking all that
	 * 	useful, since we do not honor the SNAP header, i.e. we
	 * 	seem to alway leave the snap header in place.
	 */

	if (pktinfo->hasLLC = tr_machdr_len(macinfo, (char *)mp->b_rptr,
						&maclen, &sr)) {
		snaphdr = (struct llc_snap_hdr *)(mp->b_rptr + maclen);
		if (snaphdr->d_lsap == LSAP_SNAP &&
					snaphdr->s_lsap == LSAP_SNAP &&
					snaphdr->control == CNTL_LLC_UI) {
			pktinfo->hasSnap = 1;
		}
	}
	pktinfo->macLen = maclen;

	if (pktinfo->hasSnap)
		pktinfo->Sap = ntohs(snaphdr->type);
	else if (pktinfo->hasLLC)
		pktinfo->Sap = snaphdr->d_lsap;
	else
		/*
		 * What's the right thing to do here.
		 */
		pktinfo->Sap = 0;

	if (pktinfo->hasLLC) {
		if (pktinfo->hasSnap) {
			pktinfo->hdrLen = LLC_SNAP_HDR_LEN;
		} else {
			pktinfo->hdrLen = LLC_HDR1_LEN;
		}
	}

	pktinfo->shost[0] &= ~TR_SR_ADDR;

	return (0);
}


/*
 *	stuffs length of mac and ri fields into *lenp
 *	returns:
 *		0: mac frame
 *		1: llc frame
 */
static int
tr_machdr_len(gld_mac_info_t *macinfo, char *e, int *lenp, int *source_routing)
{
	struct tr_mac_frm *mh;
	struct tr_ri *rh;
	struct srtab *sr;
	u_char fc;

	mh = (struct tr_mac_frm *)e;
	rh = (struct tr_ri *)&mh->tr_ri;
	fc = mh->tr_fc;

	if (mh->tr_shost[0] & TR_SR_ADDR) {
		*lenp = ACFCDASA_LEN + rh->len;
		*source_routing = 1;
	} else {
		*lenp = ACFCDASA_LEN;
		*source_routing = 0;
	}

	TR_SR_LOCK(macinfo);

	/*
	 * Check Source Routine
	 * 	Update source routing information.
	 */
	/*
	 * BUG: need to pass a real mtu index.
	 */
	if (sr = tr_sr_create_entry(macinfo, mh->tr_shost, 1)) {

		if (rh->rt == RT_SRF)
			sr->sr_timer = 0;

		if (! *source_routing) {
			if (sr->sr_flags == SRF_PENDING) {
				sr->sr_flags = SRF_LOCAL;
				sr->sr_ri.len = 0;
			}

		} else if (sr->sr_flags == SRF_PENDING ||
				rh->len < sr->sr_ri.len) {

			ASSERT(rh->len <= sizeof (struct tr_ri));
			bcopy((caddr_t)rh, (caddr_t)&sr->sr_ri, rh->len);
			sr->sr_ri.dir = (rh->dir ? 0 : 1);
			sr->sr_ri.res = 0;
			sr->sr_ri.rt = RT_SRF;
			sr->sr_ri.mtu = rh->mtu;

			if (sr->sr_ri.len <= 2) {
				sr->sr_flags = SRF_LOCAL;
				sr->sr_ri.len = 0;
			} else {
				sr->sr_flags = SRF_RESOLVED;
			}
		}
	}

	TR_SR_UNLOCK(macinfo);

	if ((fc & TR_MAC_MASK) == 0)
		return (0);		/* it's a MAC frame */
	else
		return (1);		/* it's an LLC frame */
}

mblk_t *
gld_unitdata_ether(gld_t *gld, mblk_t *mp)
{
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_rptr;
	gld_mac_info_t *macinfo;
	struct gld_dlsap *gldp;
	mblk_t	*nmp, *omp = NULL;
	struct	ether_mac_frm *ehdr;
	mac_addr_t dhost;

	macinfo = gld->gld_mac_info;
	/*
	 * make a valid header for transmission
	 */
	gldp = DLSAP(dlp, dlp->dl_dest_addr_offset);
	mac_copy(gldp->glda_addr, dhost);

	/* need a buffer big enough for the headers */
	if (DB_REF(mp) <= 1 && MBLKHEAD(mp) >= sizeof (*ehdr)) {
		mp->b_rptr -= sizeof (*ehdr);
		DB_TYPE(mp) = M_DATA; /* ether/gld header is data */
	} else {
		if ((nmp = allocb(sizeof (*ehdr), BPRI_MED)) == NULL)
			return (nmp);

		DB_TYPE(nmp) = M_DATA; /* ether/gld header is data */
		nmp->b_wptr = nmp->b_rptr + sizeof (*ehdr);
		linkb(nmp, mp->b_cont);
		omp = mp;
		mp = nmp;
	}

	ehdr = (struct ether_mac_frm *)mp->b_rptr;
	if (gld->gld_sap <= GLD_MAX_802_SAP)
		ehdr->ether_type = ntohs(msgdsize(mp) - sizeof (*ehdr));
	else {
#ifdef _PRE_SOLARIS_2_6
#define	BUG_1262033
#endif
#ifndef BUG_1262033
		if (dlp->dl_dest_addr_length >= 8) {
			ehdr->ether_type = htons(gldp->glda_sap);
			if (ehdr->ether_type == 0)
			    ehdr->ether_type = htons(gld->gld_sap);
		} else
#endif
			ehdr->ether_type = htons(gld->gld_sap);
	}
	if (omp != NULL)
		freeb(omp);
	mac_copy(dhost, ehdr->ether_dhost);
	mac_copy(macinfo->gldm_macaddr, ehdr->ether_shost);
	return (mp);
}

static struct	llc_snap_hdr llc_snap_def = {
					0xaa,			/* DLSAP */
					0xaa,			/* SLSAP */
					0x03,			/* Control */
					0x00, 0x00, 0x00,	/* Org[3] */
					0x00			/* Type */
				};

mblk_t *
gld_unitdata_fddi(gld_t *gld, mblk_t *mp)
{
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_rptr;
	gld_mac_info_t *macinfo;
	struct gld_dlsap *gldp;
	mblk_t	*nmp;
	struct	fddi_mac_frame *fhdr;
	int	hdrlen = sizeof (struct fddi_mac_frame);
	mac_addr_t dhost;

	macinfo = gld->gld_mac_info;

	gldp = DLSAP(dlp, dlp->dl_dest_addr_offset);
	mac_copy(gldp->glda_addr, dhost);

	if (gld->gld_sap > GLD_MAX_802_SAP)
		hdrlen += sizeof (struct llc_snap_hdr);

	if (DB_REF(mp) <= 1 && MBLKHEAD(mp) >= hdrlen) {
		mp->b_rptr -= hdrlen;
		DB_TYPE(mp) = M_DATA; /* ether/gld header is data */
	} else {
		if ((nmp = allocb(hdrlen, BPRI_MED)) == NULL)
			return (nmp);
		DB_TYPE(nmp) = M_DATA; /* fddi/gld header is data */
		nmp->b_wptr = nmp->b_rptr + hdrlen;
		linkb(nmp, mp->b_cont);
		freeb(mp);
		mp = nmp;
	}
	fhdr = (struct fddi_mac_frame *)mp->b_rptr;
	fhdr->fddi_fc = FDDI_LLC_FC;
	if (gld->gld_sap > GLD_MAX_802_SAP) {
		struct llc_snap_hdr *snap;
		snap  = (struct llc_snap_hdr *)(nmp->b_rptr +
			sizeof (*fhdr));

		*snap = llc_snap_def;
		snap->type = ntohs(gld->gld_sap);
	}
	cmac_copy((caddr_t)dhost, (caddr_t)fhdr->fddi_dhost, macinfo);
	cmac_copy((caddr_t)macinfo->gldm_macaddr,
		(caddr_t)fhdr->fddi_shost, macinfo);
	return (mp);
}

mblk_t *
gld_unitdata_tr(gld_t *gld, mblk_t *mp)
{
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_rptr;
	mblk_t *nmp, *newmp;
	struct tr_mac_frm_nori *thdr;
	int rtsize = 0, srpresent = 0;
	int llclen;
	struct srtab *sr = (struct srtab *)NULL;
	struct gld_dlsap *gldp;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	mac_addr_t destmac;
	int hdrlen;

	gldp = DLSAP(dlp, dlp->dl_dest_addr_offset);
	mac_copy(gldp->glda_addr, destmac);

	TR_SR_LOCK(macinfo);
	if (sr = tr_sr_lookup(macinfo, destmac))
		rtsize = sr->sr_ri.len;
	else
		rtsize = 2;


	llclen = (gld->gld_sap > GLD_MAX_802_SAP) ? LLC_SNAP_HDR_LEN : 0;
	hdrlen = sizeof (struct tr_mac_frm_nori) + llclen + rtsize;
	/*
	 * Create tokenring and llcsnap header (if needed) by either
	 * prepending it onto the next mblk if possible, or reusing
	 * the M_PROTO block if not.
	 */
	nmp = mp->b_cont;
	if (DB_REF(nmp) == 1 && MBLKHEAD(nmp) >= hdrlen &&
					((ulong) nmp->b_rptr & 0x1) == 0) {
		freeb(mp);
		mp = nmp;

	} else {

		if (DB_REF(mp) == 1 && MBLKSIZE(mp) >= hdrlen) {

			mp->b_rptr = mp->b_wptr = DB_LIM(mp);
			DB_TYPE(mp) = M_DATA;

		} else {
			/*
			 * Allocate new mblk
			 */
			if ((newmp = allocb(hdrlen, BPRI_MED)) == NULL) {
				TR_SR_UNLOCK(macinfo);
				return (newmp);
			}

			newmp->b_rptr = newmp->b_wptr = DB_LIM(newmp);
			DB_TYPE(newmp) = M_DATA;

			freeb(mp);
			linkb(newmp, nmp);
			mp = newmp;
		}
	}

	bzero((void *)(mp->b_rptr - hdrlen), hdrlen);

	/*
	 * llc snap header anyone ?
	 */
	if (llclen) {
		struct llc_snap_hdr *snap;

		mp->b_rptr -= LLC_SNAP_HDR_LEN;
		snap = (struct llc_snap_hdr *)mp->b_rptr;

		*snap = llc_snap_def;
		snap->type = htons(gld->gld_sap);
	}

	/*
	 * fill in source route header
	 */
	if (!sr) {
		/*
		 * must be broadcast, make it an all paths explorer
		 * or Unknown?
		 */
		struct tr_ri *rtp;

		mp->b_rptr -= 2;
		rtp = (struct tr_ri *)mp->b_rptr;

		rtp->len = 2;
		rtp->dir = 0;
		/*
		 * BUG: handle mtu indexing.
		 * rtp->mtu = trp->tr_mtuindex;
		 */
		rtp->rt = RT_APE;
		srpresent = 1;

	} else if (!(sr->sr_flags & SRF_LOCAL)) {

		mp->b_rptr -= sr->sr_ri.len;
		bcopy((caddr_t)&sr->sr_ri, (caddr_t)mp->b_rptr, sr->sr_ri.len);
		srpresent = 1;
	}

	TR_SR_UNLOCK(macinfo);

	/*
	 * fill in token ring header
	 */
	mp->b_rptr -= ACFCDASA_LEN;
	thdr = (struct tr_mac_frm_nori *)mp->b_rptr;
	thdr->tr_ac = TR_AC;
	thdr->tr_fc = TR_LLC_FC;
	/* GAB */
	mac_copy(destmac, thdr->tr_dhost);
	mac_copy(macinfo->gldm_macaddr, thdr->tr_shost);

	if (srpresent)
		thdr->tr_shost[0] |= TR_SR_ADDR;
	else
		thdr->tr_shost[0] &= ~TR_SR_ADDR;
	return (mp);
}

mblk_t *
gld_fastpath_ether(gld_t *gld, mblk_t *mp)
{
	struct ether_mac_frm *ehdr;
	dl_unitdata_req_t *udp;
	mblk_t	*nmp;

	udp = (dl_unitdata_req_t *)(mp->b_cont->b_rptr);

	if ((nmp = allocb(sizeof (struct ether_mac_frm), BPRI_MED)) == NULL)
		return (nmp);

	nmp->b_wptr += sizeof (struct ether_mac_frm);
	ehdr = (struct ether_mac_frm *)(nmp->b_rptr);
	mac_copy(mp->b_cont->b_rptr + udp->dl_dest_addr_offset,
					ehdr->ether_dhost);
	mac_copy(gld->gld_mac_info->gldm_macaddr, ehdr->ether_shost);
#ifndef BUG_1262033
	if (udp->dl_dest_addr_length >= 8) {
		ehdr->ether_type = htons(DLSAP(udp,
		    udp->dl_dest_addr_offset)->glda_sap);
		if (ehdr->ether_type == 0)
			ehdr->ether_type = htons(gld->gld_sap);
	} else
#endif
		ehdr->ether_type = htons(gld->gld_sap);
	return (nmp);
}

mblk_t *
gld_fastpath_fddi(gld_t *gld, mblk_t *mp)
{
	struct fddi_mac_frame *fhdr;
	dl_unitdata_req_t *udp;
	mblk_t	*nmp;

	udp = (dl_unitdata_req_t *)(mp->b_cont->b_rptr);

	nmp = allocb(sizeof (struct fddi_mac_frame) +
			sizeof (struct llc_snap_hdr), BPRI_MED);

	if (nmp == NULL)
		return (nmp);

	fhdr = (struct fddi_mac_frame *)nmp->b_rptr;
	nmp->b_wptr += sizeof (struct fddi_mac_frame);
	if (gld->gld_sap > GLD_MAX_802_SAP) {
		struct llc_snap_hdr *snap;
		snap  = (struct llc_snap_hdr *)nmp->b_wptr;

		*snap = llc_snap_def;
		snap->type = htons(gld->gld_sap);
		nmp->b_wptr += sizeof (*snap);
	}
	cmac_copy(mp->b_cont->b_rptr + udp->dl_dest_addr_offset,
	    fhdr->fddi_dhost, gld->gld_mac_info);
	cmac_copy(gld->gld_mac_info->gldm_macaddr,
	    fhdr->fddi_shost, gld->gld_mac_info);
	fhdr->fddi_fc = FDDI_LLC_FC;
	return (nmp);
}

mblk_t *
gld_fastpath_tr(gld_t *gld, mblk_t *mp)
{
	mblk_t *nmp;
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_cont->b_rptr;
	struct tr_mac_frm_nori *thdr;
	struct srtab *sr;
	int size = 0;
	int itsabroadcast = 0;
	int localsr = 0;
	struct gld_dlsap *gldp;

	gldp = DLSAP(dlp, dlp->dl_dest_addr_offset);

	TR_SR_LOCK(gld->gld_mac_info);

	if (sr = tr_sr_lookup(gld->gld_mac_info, gldp->glda_addr)) {
		size = sr->sr_ri.len;
		if (sr->sr_ri.len == 0)
			localsr = 1;
	} else if (mac_eq(ether_broadcast, gldp->glda_addr) ||
			mac_eq(tokenbroadcastaddr2, gldp->glda_addr)) {
		itsabroadcast = 1;
		size = 2;
	}

	/*
	 * Allocate a new mblk to hold the tokenring and llcsnap header.
	 */
	size += sizeof (struct tr_mac_frm) + LLC_SNAP_HDR_LEN;
	if ((nmp = allocb(size, BPRI_MED)) == NULL) {
		TR_SR_UNLOCK(gld->gld_mac_info);
		return (nmp);
	}

	nmp->b_wptr = nmp->b_datap->db_lim;
	nmp->b_rptr = nmp->b_wptr;
	bzero((void *)(nmp->b_rptr - size), size);

	/*
	 * fill in llc snap header
	 */
	if (gld->gld_sap > GLD_MAX_802_SAP) {
		struct llc_snap_hdr *snap;

		nmp->b_rptr -= LLC_SNAP_HDR_LEN;
		snap  = (struct llc_snap_hdr *)nmp->b_rptr;

		*snap = llc_snap_def;
		snap->type = htons(gld->gld_sap);
	}

	/*
	 * fill in source route header
	 */
	if (sr) {
		nmp->b_rptr -= sr->sr_ri.len;
		bcopy((caddr_t)&sr->sr_ri, (caddr_t)nmp->b_rptr, sr->sr_ri.len);

	} else if (itsabroadcast) {
		struct tr_ri *rtp;

		nmp->b_rptr -= 2;
		rtp = (struct tr_ri *)mp->b_rptr;

		rtp->rt = RT_STF;
		rtp->len = 2;
		rtp->dir = 0;
		/*
		 * BUG: handle mtu indexing.
		 * rtp->mtu = trp->tr_mtuindex;
		 */
	}

	TR_SR_UNLOCK(gld->gld_mac_info);

	/*
	 * Fill in the token ring header.
	 */
	nmp->b_rptr -= ACFCDASA_LEN;
	thdr = (struct tr_mac_frm_nori *)nmp->b_rptr;
	thdr->tr_ac = TR_AC;
	thdr->tr_fc = TR_LLC_FC;
	/* GAB */
	mac_copy(gldp->glda_addr, thdr->tr_dhost);
	mac_copy(gld->gld_mac_info->gldm_macaddr, thdr->tr_shost);

	/*
	 * should this match the criteria used for filling in
	 * the source routing information.
	 *
	 * see unitdata since it does nearly the same thing,
	 * should we use RT_STF or RT_APE at the same time.
	 */
	if ((sr || itsabroadcast) && !localsr)
		thdr->tr_shost[0] |= TR_SR_ADDR;
	else
		thdr->tr_shost[0] &= ~TR_SR_ADDR;

	return (nmp);
}

void
gld_init_ether(gld_mac_info_t *macinfo)
{
	struct gldkstats *sp;
	sp = &macinfo->gldm_kstats;

	kstat_named_init(&sp->glds_collisions, "collisions", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_nocarrier, "nocarrier", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_defer, "defer", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_frame, "framing", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_crc, "crc", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_overflow, "oflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_underflow, "uflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_missed, "missed", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_xmtlatecoll, "late_collisions",
					KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_short, "short", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_excoll, "excollisions", KSTAT_DATA_ULONG);
}

void
gld_init_fddi(gld_mac_info_t *macinfo)
{
	struct gldkstats *sp;
	sp = &macinfo->gldm_kstats;

	kstat_named_init(&sp->glds_nocarrier, "nocarrier", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_defer, "defer", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_frame, "framing", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_crc, "crc", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_overflow, "oflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_underflow, "uflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_missed, "missed", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_short, "short", KSTAT_DATA_ULONG);
}

void
gld_init_tr(gld_mac_info_t *macinfo)
{
	struct gldkstats *sp;
	sp = &macinfo->gldm_kstats;

	mutex_init(TR_SR_MUTEX(macinfo), NULL, MUTEX_DRIVER, NULL);

	TR_SR_VAR(macinfo) = kmem_zalloc(sizeof (struct srtab *)*SR_HASH_SIZE,
				KM_SLEEP);

	kstat_named_init(&sp->glds_nocarrier, "nocarrier", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_defer, "defer", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_frame, "framing", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_crc, "crc", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_overflow, "oflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_underflow, "uflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_missed, "missed", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_short, "short", KSTAT_DATA_ULONG);
}

/*ARGSUSED*/
void
gld_uninit_ether(gld_mac_info_t *macinfo)
{
}

/*ARGSUSED*/
void
gld_uninit_fddi(gld_mac_info_t *macinfo)
{
}

void
gld_uninit_tr(gld_mac_info_t *macinfo)
{
	mutex_destroy(TR_SR_MUTEX(macinfo));
	tr_sr_clear(macinfo);
	kmem_free(TR_SR_HASH(macinfo), sizeof (struct srtab *) * SR_HASH_SIZE);
}

/*
 * The source route routines follow
 */

static struct srtab **
tr_sr_hash(struct srtab **sr_hash_tbl, mac_addr_t addr, int addr_length)
{
	u_int hashval = 0;

	while (--addr_length >= 0)
		hashval ^= *addr++;

	return (&sr_hash_tbl[hashval % SR_HASH_SIZE]);
}

static struct srtab *
tr_sr_lookup_entry(gld_mac_info_t *macinfo, mac_addr_t macaddr)
{
	struct srtab *sr;

	for (sr = *tr_sr_hash(TR_SR_HASH(macinfo), macaddr, ETHERADDRL);
	    sr; sr = sr->sr_next)
		if (sr->sr_instance == macinfo && mac_eq(macaddr, sr->sr_mac))
			return (sr);

	return ((struct srtab *)0);
}

static struct srtab *
tr_sr_lookup(gld_mac_info_t *macinfo, mac_addr_t macaddr)
{
	struct srtab *sr;

	if (!(sr = tr_sr_lookup_entry(macinfo, macaddr)))
		return ((struct srtab *)0);

	if (sr->sr_flags & (SRF_LOCAL | SRF_RESOLVED | SRF_PERMANENT))
		return (sr);

	return ((struct srtab *)0);
}

static struct srtab *
tr_sr_create_entry(gld_mac_info_t *macinfo, mac_addr_t macaddr, int mtuindex)
{
	struct srtab *sr;
	struct srtab **srp;
	struct tr_ri *rtp;

	/*
	 * do not make entries for broadcast addresses
	 */
	if (mac_eq(macaddr, ether_broadcast) ||
				mac_eq(macaddr, tokenbroadcastaddr2))
		return ((struct srtab *)0);

	srp = tr_sr_hash(TR_SR_HASH(macinfo), macaddr, ETHERADDRL);

	for (sr = *srp; sr; sr = sr->sr_next)
		if (sr->sr_instance == macinfo &&
				mac_eq(macaddr, sr->sr_mac))
			return (sr);

	if (!(sr = kmem_zalloc(sizeof (struct srtab), KM_NOSLEEP))) {
		cmn_err(CE_NOTE, "tr: tr_sr_create_entry kmem_alloc failed");
		return (sr);
	}

	sr->sr_flags = SRF_PENDING;
	bcopy((caddr_t)macaddr, (caddr_t)sr->sr_mac, ETHERADDRL);
	sr->sr_instance = macinfo;
	sr->sr_timer = 0;

	rtp = &sr->sr_ri;

	rtp->len = 2;
	rtp->dir = 0;
	rtp->mtu = (uchar_t)mtuindex;
	rtp->rt = RT_APE;	/* all paths explorer */

	sr->sr_next = *srp;
	*srp = sr;

	return (sr);
}

static void
tr_sr_clear(gld_mac_info_t *macinfo)
{
	int i;
	struct srtab **sr_hash_tbl = TR_SR_HASH(macinfo);
	struct srtab **srp, *sr;

	/*
	 * Walk through the table, deleting stuff that is old
	 */
	for (i = 0; i < SR_HASH_SIZE; i++) {
		for (srp = &sr_hash_tbl[i]; (sr = *srp) != NULL; ) {
			*srp = sr->sr_next;
			kmem_free((char *)sr, sizeof (struct srtab));
		}
	}
}
