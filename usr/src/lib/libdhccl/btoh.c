/*
 * btoh.c: "Unpack the BOOTP packet".
 *
 * SYNOPSIS
 *    void BootpToHost(const struct bootp *bp, Host *hp, int bootpsize)
 *
 * DESCRIPTION
 *    Unpack a BOOTP packet into a Host structure. All fields in Host are
 *    initially set to zero, so to avoid memory leaks any references to
 *    dynamically allocated memory in Host must be free'd by the calling
 *    function.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)btoh.c	1.3	96/11/27 SMI"

#include <string.h>
#include "dhcp.h"
#include "hosttype.h"
#include "hostdefs.h"
#include "dcuttxt.h"
#include "bootp.h"
#include "unixgen.h"
#include "utils.h"

unsigned char vm_rfc1048[4] = VM_RFC1048; /* Vendor magic cookie for RFC1048 */

void
deserializeItem(int type, int taglen, void* p, const unsigned char * vp)
{
	int i;
	int c;
	unsigned count;
	int16_t *ps;
	int32_t *pl;
	char *pch;
	struct in_addr_list *pia;
	struct shared_bindata *psb;
	struct int16_list *psi;
	struct string_list *psl;

	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		memcpy(p, vp, 1);
		break;
	case TYPE_INT16:
		memcpy(p, vp, 2);
		ps = (int16_t *)p;
		*ps = ntohs(*ps); /* byte ordering */
		break;
	case TYPE_INT32:
	case TYPE_TIME:
		memcpy(p, vp, 4);
		pl = (int32_t *)p;
		*pl = ntohl(*pl); /* byte ordering */
		break;
	case TYPE_ADDR:
		memcpy(p, vp, 4);
		break;
	case TYPE_CHAR_PTR:
		pch = (char *)xmalloc(taglen + 1);
		*(char **)p = pch;
		if (pch) {
			strncpy(pch, (char *)vp, taglen);
			pch[taglen] = (char)NULL;
		}
		break;
	case TYPE_ADDR_LIST:
		i = sizeof (struct in_addr_list) + taglen -
		    sizeof (struct in_addr);
		pia = (struct in_addr_list *)xmalloc(i);
		*(struct in_addr_list **)p = pia;
		if (pia) {
			pia->addrcount = taglen / sizeof (struct in_addr);
			pia->linkcount = 1;
			memcpy(pia->addr, vp, taglen);
		}
		break;
		case TYPE_INT16_LIST:
			i = sizeof (struct int16_list) + taglen -
			    sizeof (int16_t);
			psi = (struct int16_list *)xmalloc(i);
			*(struct int16_list **)p = psi;
			if (psi) {
				psi->count = taglen / sizeof (int16_t);
				memcpy(psi->shary, vp, taglen);
				for (c = 0; c < psi->count; c++)
					psi->shary[c] = ntohs(psi->shary[c]);
			}
			break;
		case TYPE_BINDATA:
			i = sizeof (struct shared_bindata) + taglen - 1;
			psb = (struct shared_bindata *)xmalloc(i);
			*(struct shared_bindata **)p = psb;
			if (psb) {
				psb->length = taglen;
				psb->linkcount = 1;
				memcpy(psb->data, vp, taglen);
			}
			break;
		case TYPE_LEASE_ID:
			memcpy(p, vp, taglen);
			break;
		case TYPE_STRING_LIST:
			count = 0;
			for (pch = (char *)vp; pch < ((char *)vp + taglen);
			    pch += strlen(pch) + 1) {
				count++;
			}
			i = sizeof (struct string_list) + (count - 1) *
			    sizeof (char *);
			psl = (struct string_list *)xmalloc(i);
			*(struct string_list **)p = psl;
			if (psl) {
				psl->count = count;
				psl->linkcount = 1;
				i = 0;
				for (i = 0, pch = (char *)vp; pch <
				    ((char *)vp + taglen);
				    pch += strlen(pch) + 1) {
					psl->string[i++] = xstrdup(pch);
				}
			}
			break;
		case TYPE_TAG_LIST:
		default:
			break;
	}
}

/*
 * void vendor_specific(const unsigned char *vp, Host *hp)
 * -------------
 * gets the vendor options from tag-value memory pointed to by vp and
 * puts it in struct host pointed to by hp.
 */
static void
vendor_specific(const unsigned char *vp, Host *hp, int vendorIndex)
{
	int tag, taglen, bytesleft;
	HTSTRUCT *pht;
	void *p;

	/* Skip over vp[0] (=TAG_VENDOR_SPECIFIC) and get byte count */
	bytesleft = vp[1];
	vp += 2;

	/* Cycle through vendor specific area */
	while (bytesleft > 0) {
		tag = vp[0];
		if (tag == TAG_END)
			return;
		taglen = vp[1];
		pht = find_bytag(VENDOR_TAG(tag), vendorIndex);
		if (pht) {
		    p = hp->vtags + tag;
		    if (p) {
			onhost(hp->flags, VENDOR_TAG(tag));
			deserializeItem(pht->type, taglen, p, vp + 2);
		    }
		}

		bytesleft -= (taglen + 2);
		vp += (taglen + 2);
	}
}

void
BootpToHost(const struct bootp *bp, Host *hp, int bootpsize)
{
	const unsigned char *vp;
	int i, tag, taglen, bytesleft, over_load, in_overload;
	int has_vendor, vendorIndex;
	HTSTRUCT *pht;

	/* make sure the host structure is clean */
	memset(hp, 0, sizeof (struct Host));

	/* get the hardware address */
	for (i = 0; i < bp->hlen; ++i)
		hp->hw.chaddr[i] = bp->chaddr[i];
	hp->hw.htype = bp->htype;
	hp->hw.hlen = bp->hlen;
	if (hp->Hwaddr != NULL)
		(void) free(hp->Hwaddr);
	hp->Hwaddr = xstrdup(addrstr(hp->hw.chaddr, hp->hw.hlen, TRUE, ':'));
	onhost(hp->flags, TAG_HWADDR);

	/* get the IP address */
	hp->Ciaddr = bp->ciaddr;
	hp->Giaddr = bp->giaddr;
	hp->Siaddr = bp->siaddr;
	hp->Yiaddr = bp->yiaddr;
	onhost(hp->flags, TAG_CIADDR);
	onhost(hp->flags, TAG_GIADDR);
	onhost(hp->flags, TAG_SIADDR);
	onhost(hp->flags, TAG_YIADDR);

	/* unpack the vendor extensions */
	vp = bp->vend;
	bytesleft = bootpsize - VBPLEN;

	/* make sure the magic cookie is there */
	if (!memcmp(vm_rfc1048, vp, 4)) {
		hp->stags[TAG_VENDOR_COOKIE].dhcu1 = RFC_COOKIE;
		onhost(hp->flags, TAG_VENDOR_COOKIE);
	}
#if SUPPORT_CMU
	else {
		if (!memcmp(VM_CMU, vp, 4)) {
			hp->stags[TAG_VENDOR_COOKIE].dhcu1 = CMU_COOKIE;
			onhost(hp->flags, TAG_VENDOR_COOKIE);
		}
	}
#endif
	else
		return;

	/* skip over the cookie */
	vp += 4;
	bytesleft -= 4;

	has_vendor = 0;

	in_overload = 0;
	over_load = OVERLOAD_NONE;

newblock:
	while (bytesleft > 0) {
		tag = vp[0];
		if (tag == TAG_END)
		    break;
		if (tag == TAG_PAD)
		    taglen = -1;
		else
		    taglen = vp[1];

		pht = find_bytag(tag, VENDOR_INDEPENDANT);

		/* Skip padding and vendor specific (handled in second loop) */
		if (tag != TAG_PAD) {
			if (tag == TAG_VENDOR_SPECIFIC) {
				has_vendor = 1;
			} else if (tag == TAG_OVERLOAD) {
#if DEBUG
				logb("overload = %d\n", vp[2]);
#endif
				if (!in_overload)
					over_load = vp[2];
				else
					loge(DCUTMSG05);
			} else if (tag == TAG_REQUEST_LIST) {
			    for (i = 0; i < taglen; i++)
				onhost(hp->request_vector, vp[2+i]);
			    onhost(hp->flags, TAG_REQUEST_LIST);
			} else {
			    void* p = hp->stags + tag;
			    onhost(hp->flags, tag);
			    deserializeItem(pht == 0 ? TYPE_BINDATA : pht->type,
				taglen, p, vp + 2);
			}
		}

		bytesleft -= (taglen + 2);
		vp += (taglen + 2);
	}
	if (in_overload && (tag != TAG_END))
		logw(DCUTMSG06);

	switch (over_load) {
	case OVERLOAD_FILE:
		bytesleft = sizeof (bp->file);
		vp = bp->file;
		over_load = OVERLOAD_NONE;
		in_overload = 1;
		goto newblock;
	case OVERLOAD_SNAME:
		bytesleft = sizeof (bp->sname);
		vp = bp->sname;
		over_load = OVERLOAD_NONE;
		in_overload = 1;
		goto newblock;
	case OVERLOAD_BOTH:
		/*
		 * First unpack file. Set over_load so that next time
		 * we exit loop, we will unpack sname.
		 */
		bytesleft = sizeof (bp->file);
		vp = bp->file;
		over_load = OVERLOAD_SNAME;
		in_overload = 1;
		goto newblock;
	default:
		break;
	}

	/* check for boot file if not overloaded */
	if (!in_overload && bp->file[0] != '\0') {
		int l = 1 + strlen((char *)bp->file);
		l = (l > sizeof (bp->file) ? l : sizeof (bp->file));
		hp->stags[TAG_BOOTFILE].dhccp = (char *)xmalloc(l);
		if (hp->stags[TAG_BOOTFILE].dhccp) {
		    onhost(hp->flags, TAG_BOOTFILE);
		    strncpy(hp->stags[TAG_BOOTFILE].dhccp, (char *)bp->file, l);
		}
	}

	/* check for boot server name if not overloaded */
	if (!in_overload && bp->sname[0] != '\0') {
		int l = 1 + strlen((char *)bp->sname);
		l = (l > sizeof (bp->sname) ? l : sizeof (bp->sname));
		hp->stags[TAG_TFTP_SERVER_NAME].dhccp = (char *)xmalloc(l);
		if (hp->stags[TAG_TFTP_SERVER_NAME].dhccp) {
		    onhost(hp->flags, TAG_TFTP_SERVER_NAME);
		    strncpy(hp->stags[TAG_TFTP_SERVER_NAME].dhccp,
			(char *)bp->sname, l);
		}
	}

	/* Get the vendor options */
	if (!has_vendor)
		return;
	if (!inhost(hp->flags, TAG_CLASS_ID))
		vendorIndex = VENDOR_UNSPECIFIED;
	else {
		const VTSTRUCT *vtp = findVT(hp->stags[TAG_CLASS_ID].dhccp);
		if (vtp)
			vendorIndex = vtp->selfindex;
		else
			return;
	}

	/* unpack the vendor specific options */
	vp = bp->vend;
	bytesleft = bootpsize - VBPLEN;

	/* pull out the cookie */
	vp += 4;
	bytesleft -= 4;

	in_overload = 0;
	over_load = OVERLOAD_NONE;

vendoroverload:
	while (bytesleft > 0) {
		tag = vp[0];
		if (tag == TAG_END)
			break;
		if (tag == TAG_PAD)
			taglen = -1;
		else
			taglen = vp[1];

		/* Unpack vendor specific parameters */
		if (tag == TAG_VENDOR_SPECIFIC) {
			vendor_specific(vp, hp, vendorIndex);
		}

		/* Don't need this -- see below */
		if (tag == TAG_OVERLOAD) {
			if (!in_overload) {
				over_load = vp[2];
			}
		}
		bytesleft -= (taglen + 2);
		vp += (taglen + 2);
	}
	switch (over_load) {
	case OVERLOAD_FILE:
		bytesleft = sizeof (bp->file);
		vp = bp->file;
		over_load = OVERLOAD_NONE;
		in_overload = 1;
		goto vendoroverload;
	case OVERLOAD_SNAME:
		bytesleft = sizeof (bp->sname);
		vp = bp->sname;
		over_load = OVERLOAD_NONE;
		in_overload = 1;
		goto vendoroverload;
	case OVERLOAD_BOTH:
		/*
		 * First unpack file. Set over_load so that next time
		 * we exit loop, we will unpack sname.
		 */
		bytesleft = sizeof (bp->file);
		vp = bp->file;
		over_load = OVERLOAD_SNAME;
		in_overload = 1;
		goto vendoroverload;
	default:
		break;
	}
}
