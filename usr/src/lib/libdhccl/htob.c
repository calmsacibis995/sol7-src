/*
 * htob.c: "Fill BOOTP structure from host structure".
 *
 * SYNOPSIS
 *    HostToBootp(const host *hp, struct bootp *bp, int overloadAllowed)
 *
 * DESCRIPTION
 *    Insert the RFC1048 vendor data for the host pointed to by "hp" into the
 *    bootp packet pointed by "bp".
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)htob.c 1.4 96/11/27 SMI"

#include <assert.h>
#include <string.h>
#include "hostdefs.h"
#include "hosttype.h"
#include "dcuttxt.h"
#include "dhcp.h"
#include "bootp.h"
#include "unixgen.h"
#include "utils.h"
#include "ca_time.h"
#include "catype.h"

#define	DHCP_MESSAGE_TYPE_FIRST 1

extern unsigned char vm_rfc1048[4]; /* Vendor magic cookie for RFC1048 */

#ifdef	SUPPORT_CMU
struct cmu_vend {
	char		v_magic[4];	/* magic number */
	uint32_t	v_flags;	/* flags/opcodes, etc. */
	struct in_addr	v_smask;	/* Subnet mask */
	struct in_addr	v_dgate;	/* Default gateway */
	struct in_addr	v_dns1, v_dns2;	/* Domain name servers */
	struct in_addr	v_ins1, v_ins2;	/* IEN-116 name servers */
	struct in_addr	v_ts1, v_ts2;	/* Time servers */
	int32_t		v_unused[6];	/* currently unused */
};

static void
do_cmu(struct bootp *bp, const struct Host *hp, int bad_cookie)
{
	struct cmu_vend vendp;
	struct in_addr_list *taddr;

	if (inhost(hp->flags, TAG_BOOTFILE)) {
		int len = strlen(hp->stags[TAG_BOOTFILE].dhccp);
		if ((len + 1) > sizeof (bp->file))
			logw(DCUTMSG07, hp->stags[TAG_BOOTFILE].dhccp);
		else
			strcpy((char *)bp->file, hp->stags[TAG_BOOTFILE].dhccp);
	}
	if (inhost(hp->flags, TAG_TFTP_SERVER_NAME)) {
		int len = strlen(hp->stags[TAG_TFTP_SERVER_NAME].dhccp);
		if ((len + 1) > sizeof (bp->sname))
			logw(DCUTMSG08, hp->stags[TAG_TFTP_SERVER_NAME].dhccp);
		else
			strcpy((char *)bp->sname,
			    hp->stags[TAG_TFTP_SERVER_NAME].dhccp);
	}

	if (bad_cookie)
		return;

	memset(&vendp, 0, sizeof (vendp));
	strcpy(vendp.v_magic, (char *)VM_CMU);

	if (inhost(hp->flags, TAG_SUBNET_MASK)) {
		vendp.v_smask = hp->stags[TAG_SUBNET_MASK].dhcia;
		vendp.v_flags = 1;
	}

	if (inhost(hp->flags, TAG_GATEWAYS)) {
		taddr = hp->stags[TAG_GATEWAYS].dhcial;
		if (taddr->addrcount > 0)
			vendp.v_dgate = taddr->addr[0];
	}

	if (inhost(hp->flags, TAG_DOMAIN_SERVERS)) {
		taddr = hp->stags[TAG_DOMAIN_SERVERS].dhcial;
		if (taddr->addrcount > 0) {
			vendp.v_dns1.s_addr = taddr->addr[0].s_addr;
			if (taddr->addrcount > 1)
				vendp.v_dns2.s_addr = taddr->addr[1].s_addr;
		}
	}

	if (inhost(hp->flags, TAG_NAME_SERVERS)) {
		taddr = hp->stags[TAG_NAME_SERVERS].dhcial;
		if (taddr->addrcount > 0) {
			vendp.v_ins1 = taddr->addr[0];
			if (taddr->addrcount > 1)
				vendp.v_ins2 = taddr->addr[1];
		}
	}

	if (inhost(hp->flags, TAG_TIME_SERVERS)) {
		taddr = hp->stags[TAG_TIME_SERVERS].dhcial;
		if (taddr->addrcount > 0) {
			vendp.v_ts1 = taddr->addr[0];
			if (taddr->addrcount > 1)
				vendp.v_ts2 = taddr->addr[1];
		}
	}
	memcpy(bp->vend, &vendp, sizeof (vendp));
}
#endif	/* SUPPORT_CMU */

/*
 * need():
 * Given the data type and a pointer to the data residing in a Host
 * structure, determine how many octets the item would require
 * when serialised.
 */
int
need(int type, const void *p)
{
	register int size = 0;

	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		size = sizeof (uint8_t);
		break;
	case TYPE_INT16:
		size = sizeof (uint16_t);
		break;
	case TYPE_INT32:
	case TYPE_TIME:
		size = sizeof (uint32_t);
		break;
	case TYPE_CHAR_PTR:
		if (p != NULL && *(char **)p != NULL)
			size = strlen(*(char **)p);
		break;
	case TYPE_ADDR:
		size = sizeof (struct in_addr);
		break;
	case TYPE_ADDR_LIST:
		if (p != NULL && *(struct in_addr_list **)p != NULL) {
			struct in_addr_list *pil = *(struct in_addr_list **)p;
			size = pil->addrcount * sizeof (struct in_addr);
		}
		break;
	case TYPE_INT16_LIST:
		if (p != NULL && *(struct in516_list **)p != NULL) {
			struct int16_list *psl = *(struct int16_list **)p;
			size = psl->count * sizeof (int16_t);
		}
		break;
	case TYPE_BINDATA:
		if (p != NULL && *(struct shared_bindata **)p != NULL) {
			struct shared_bindata *psb =
			    *(struct shared_bindata **)p;
			size = psb->length;
		}
		break;
	case TYPE_TAG_LIST:
		if (p != NULL) {
			int tag;
			unsigned char *prv = (unsigned char *)p;
			for (size = 0, tag = 1; tag <= LAST_TAG; tag++) {
				if (inhost(prv, tag))
					size++;
			}
		}
		break;
	case TYPE_STRING_LIST:
		if (p != NULL && *(struct string_list **)p != NULL) {
			struct string_list *pl = *(struct string_list **)p;
			int i;
			for (size = 0, i = 0; i < pl->count; i++)
				size += strlen(pl->string[i]) + 1;
		}
		break;

	}
	return (size);
}

void
serializeItem(int type, int len, const void *p, unsigned char *vp)
{
	short si;
	int32_t  li;

	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		*vp = *(unsigned char *)p;
		break;
	case TYPE_INT16:
		si = *(int16_t *)p;
		si = htons(si);
		memcpy(vp, &si, 2);
		break;
	case TYPE_INT32:
	case TYPE_TIME:
		li = *(int32_t *)p;
		li = htonl(li);
		memcpy(vp, &li, 4);
		break;
	case TYPE_ADDR:
		memcpy(vp, p, len);
		break;
	case TYPE_ADDR_LIST:
		{
			struct in_addr *in = (*(struct in_addr_list **)p)->addr;
			memcpy(vp, in, len);
		}
		break;
	case TYPE_CHAR_PTR:
		memcpy(vp, *(char **)p, len);
		break;
	case TYPE_INT16_LIST:
		{
			int count = len / sizeof (int16_t);
			int16_t *ps = (*(struct int16_list **)p)->shary;
			while (count-- > 0) {
				si = *ps;
				si = htons(si);
				memcpy(vp, &si, 2);
				vp += sizeof (int16_t);
				ps++;
			}
			vp -= len;
		}
		break;
	case TYPE_BINDATA:
		{
			unsigned char *s = (*(struct shared_bindata **)p)->data;
			memcpy(vp, s, len);
		}
		break;
	case TYPE_LEASE_ID:
		memcpy(vp, p, len);
		break;
	case TYPE_TAG_LIST:
		{
			unsigned char *bits = (unsigned char *)p;
			int tag;
			for (tag = 1; tag <= LAST_TAG; tag++) {
				if (inhost(bits, tag))
					*vp++ = (unsigned char)tag;
			}
		}
		break;
	case TYPE_STRING_LIST:
		{
			struct string_list *pl = *(struct string_list **)p;
			int i;
			for (i = 0; i < pl->count; i++) {
				strcpy((char *)vp, pl->string[i]);
				vp += strlen(pl->string[i]) + 1;
			}
		}
		break;
	}
}

static int
vendor_specific(const Host* hp, unsigned char **pb)
{
#define	DVP	(255) /* not randomly choosen -- see note below */
	register int tag;
	register int i;
	static unsigned char *base = NULL;
	unsigned char *b;
	static int bsize = 0;
	int len;
	int size;
	const VTSTRUCT *vendor = NULL;
	const HTSTRUCT *pht;

	vendor = findVT(hp->stags[TAG_CLASS_ID].dhccp);
	if (vendor == 0)
		return (0);

	if (!base) {
		base = (unsigned char *)xrealloc(base, DVP);
		bsize += DVP;
	}

	b = base;
	i = 0;
	size = DVP;
	b[i++] = TAG_VENDOR_SPECIFIC;
	b[i++] = 0;

	/*
	 * The maximum size (255) of a vendor option tag field is limited by
	 * the byte-sized length flag. For our simple algorithm for filling
	 * the vendor specific parameters values, this limits us to 252
	 * vendor specific tags. --> 255 = 1 for the list length, +252, 1
	 * for each of the possible 251 tags, +1 for the overall tag
	 * TAG_VENDOR_SPECIFIC, +1 (finally) for the total length of the
	 * vendor specific parts.
	 */
	for (tag = 1; tag <= vendor->hightag; tag++) {
		const void *p;
		if (!inhost(hp->flags, VENDOR_TAG(tag)))
			continue;
		pht = find_byindex(vendor->htindex[tag]);
		if (!pht)
			continue;
		p = hp->vtags + tag;
		len = need(pht->type, p);
		if (len <= 0)
			continue;

		if ((i + len + 2) > size) {
			/*
			 * Overflowed maximum size of vendor option. Need to
			 * start another
			 */
			b[1] = i - 2;
			base = (unsigned char *)xrealloc(base, bsize += DVP);

			b = base + i;

			i = 0;
			size = DVP;
			b[i++] = TAG_VENDOR_SPECIFIC;
			b[i++] = 0;
		}

		if ((i + len + 2) <= size) {
			b[i++] = (unsigned char)tag;
			b[i++] = (unsigned char)len;
			serializeItem(pht->type, len, p, b + i);
			i += len;
		}
#if 0
		else
			;

		/*
		 * We need to do something about this case (only
		 * occurs if option data length > 255). For the
		 * moment do nothing.
		 */
#endif
	}
	b[1] = i - 2;
	*pb = base;
	return (i + (b - base));
}

void
HostToBootp(const struct Host *hp, struct bootp *bp, int overloadAllowed,
    int bootpsize)
{
	register int tag;
	int len;
	unsigned char *over_load = NULL, *b;

	/* Vendor options area: */
	int bytesleft;
	unsigned char *of_vp = NULL; /* file area overload: */
	int of_bytesleft = sizeof (bp->file);
	unsigned char *os_vp = NULL; /* sname area overload */
	int os_bytesleft = sizeof (bp->sname);
	unsigned char *vp;
	int overload_tag_space;

	memset(bp, 0, sizeof (struct bootp));

	bp->hlen = haddrlength(hp->hw.htype);
	bp->htype  = hp->hw.htype;
	memcpy(bp->chaddr, hp->hw.chaddr, bp->hlen);

	bp->ciaddr = hp->Ciaddr;
	bp->giaddr = hp->Giaddr;
	bp->siaddr = hp->Siaddr;
	bp->yiaddr = hp->Yiaddr;

	vp = bp->vend;
	bytesleft = bootpsize - VBPLEN;

#if SUPPORT_CMU
	if (hp->stags[TAG_VENDOR_COOKIE].dhcu1 == CMU_COOKIE) {
		do_cmu(bp, hp, FALSE);
		return;
	} else if (hp->stags[TAG_VENDOR_COOKIE].dhcu1 == AUTO_COOKIE) {
		do_cmu(bp, hp, TRUE); /* cookie bad, but send file, sname */
		return; /* no options in this event! */
	} else { /* still want to support DHCP clients */
		memcpy(vp, vm_rfc1048, 4); /* Copy in the RFC1048 cookie */
		vp += 4;
		bytesleft -= 4;
	}
#else
	memcpy(vp, vm_rfc1048, 4); /* Copy in the RFC1048 cookie */
	vp += 4;
	bytesleft -= 4;
#endif /* SUPPORT_CMU */

	/*
	 * This mod is not a bug fix, but at least one sniffer (Network
	 * General's) vendor has made the assumption that the message type
	 * has to be the first option. When this is not so, the sniffer
	 * describes the packet as a BOOTP reply instead of a DHCP reply.
	 * This isn't fatal, but leads to confusion. This fix should eliminate
	 * a few hundred support calls!
	 */
#if DHCP_MESSAGE_TYPE_FIRST
	if (inhost(hp->flags, TAG_DHCP_MESSAGE_TYPE)) {
		*vp++ = TAG_DHCP_MESSAGE_TYPE;
		*vp++ = 1;
		*vp++ = hp->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1;
		bytesleft -= 3;
	}
#endif

	/*
	 * If we can overload, we need to reserve 3 spare bytes whenever
	 * we try to insert an option.
	 */
	overload_tag_space = (overloadAllowed > OVERLOAD_NONE) ? 3 : 0;

	/*
	 * Fill with the Vendor specific first since they will probably be
	 * the largest and the largest area to be filled is the
	 * bootp vendor option area
	 */
	if (inhost(hp->flags, TAG_CLASS_ID)) {
		len = vendor_specific(hp, &b);
		if ((len + overload_tag_space) < bytesleft) {
			memcpy(vp, b, len);
			vp += len;
			bytesleft -= len;
		} else
			logw(DCUTMSG11, "vendor options");
	}

	/* Insert remaining data in tag order: */
	for (tag = 1; tag <= LAST_TAG; tag++) {
		HTSTRUCT *pht;
		const void *p;
		int type;

		if (!inhost(hp->flags, tag))
			continue;
#if DHCP_MESSAGE_TYPE_FIRST
		if (tag == TAG_DHCP_MESSAGE_TYPE)
			continue;
#endif
		pht = find_bytag(tag, VENDOR_INDEPENDANT);
		if (tag == TAG_REQUEST_LIST) {
			p = hp->request_vector;
			type = TYPE_TAG_LIST;
		} else if (!pht) {
			p = hp->stags + tag;
			type = TYPE_BINDATA;
		} else {
			p = hp->stags + tag;
			type = pht->type;
		}
		len = need(type, p);
		if (len <= 0)
			continue;

		/*
		 * First deal with boot file name. In DHCP the bootfile is
		 * now (since April 1995) a tagged item and may be loaded
		 * into the packet in the normal way. But if the client
		 * is a BOOTP client (overloadAllowed set to OVERLOAD_NONE)
		 * the tag will not be recognised and the boot image name
		 * must be placed in the "file" field of the BOOTP packet.
		 */
		if (tag == TAG_BOOTFILE && overloadAllowed == OVERLOAD_NONE) {
			if (len+1 > sizeof (bp->file))
				logw(DCUTMSG12, hp->stags[tag].dhccp);
			else
				strcpy((char *)bp->file, hp->stags[tag].dhccp);
			continue;
		} else if (tag == TAG_TFTP_SERVER_NAME &&
		    overloadAllowed == OVERLOAD_NONE) {
			/* Next treat the BOOTP TFTP server name similarly: */
			if ((len + 1) > sizeof (bp->sname))
				logw(DCUTMSG13, hp->stags[tag].dhccp);
			else
				strcpy((char *)bp->sname, hp->stags[tag].dhccp);
			continue;
		} else if ((len + 2 + overload_tag_space) < bytesleft) {
			/*
			 * Next try to put in vendor option area. Remember
			 * to leave one byte available for TAG_END to be
			 * inserted at the end.
			 */
			*vp++ = (unsigned char)tag;
			*vp++ = (unsigned char)len;
			serializeItem(type, len, p, vp);
			bytesleft -= (len + 2);
			vp += len;
			continue;
		} else if ((len + 2) < of_bytesleft &&
		    overloadAllowed >= OVERLOAD_FILE) {
			/* Next try to put into file overload area. */
			if (of_vp == NULL) {
				*vp++ = TAG_OVERLOAD;
				*vp++ = 1;
				over_load = vp;
				*vp++ = OVERLOAD_FILE;
				bytesleft -= 3;
				of_vp = bp->file;
			}
			*of_vp++ = (unsigned char)tag;
			*of_vp++ = (unsigned char)len;
			serializeItem(type, len, p, of_vp);
			of_bytesleft -= (len + 2);
			of_vp += len;
			continue;
		} else if ((len + 2) < os_bytesleft &&
		    overloadAllowed >= OVERLOAD_SNAME) {
			/*
			 * Finally try to put into sname overload area:
			 * NOTE - assuming file area is larger than sname area
			 * so that of_vp (and therefore over_load) will be
			 * non-NULL if we get here. With the current bootp
			 * packet, this will be true if overloadAllowed
			 * is NOT OVERLOAD_SNAME
			 */
			if (os_vp == NULL) {
				*over_load = OVERLOAD_BOTH;
				os_vp = bp->file;
			}
			*os_vp++ = (unsigned char)tag;
			*os_vp++ = (unsigned char)len;
			serializeItem(type, len, p, os_vp);
			os_bytesleft -= (len + 2);
			os_vp += len;
			continue;
		}

		/* It just doesn't fit anywhere: */
		if (pht)
			logw(DCUTMSG11, pht->longname);
		else
			logw(DCUTMSG14, tag);
	}

	/*
	 * Must be guaranteed to reach here with bytesleft > 0 so that we
	 * have room for the TAG_END. This also applies to file and sname
	 * fields below
	 */
	assert(bytesleft > 0);
	*vp = TAG_END;

	/*
	 * Insert TAG_END into file and sname fields (assuming field had
	 * any options inserted into them. Pad with TAG_PAD.
	 */
	if (of_vp != NULL) {
		assert(of_bytesleft > 0);
		*of_vp++ = TAG_END;
		memset(of_vp, TAG_PAD, of_bytesleft - 1);
	}
	if (os_vp != NULL) {
		assert(os_bytesleft > 0);
		*os_vp++ = TAG_END;
		memset(os_vp, TAG_PAD, os_bytesleft - 1);
	}
}
