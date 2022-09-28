/*
 * ftoh.c: "Convert DHCP configuration file for internal use".
 *
 * SYNOPSIS
 *	void FileToHost(FILE*, Host *)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)ftoh.c	1.4	96/11/27 SMI"

#include "hostdefs.h"
#include "hosttype.h"
#include "dcuttxt.h"
#include "unixgen.h"
#include "utils.h"
#include <string.h>

static void
getfp(int tag, int type, void *p, FILE *fp, Host* hp)
{
	int i, n;
	short len;
	char *pch;
	struct in_addr_list	*pia;
	struct shared_bindata	*psb;
	struct int16_list	*psi;
	struct string_list	*pl;

	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		fread(p, sizeof (unsigned char), 1, fp);
		break;
	case TYPE_INT16:
		fread(p, sizeof (int16_t), 1, fp);
		break;
	case TYPE_INT32:
	case TYPE_TIME:
		fread(p, sizeof (int32_t), 1, fp);
		break;
	case TYPE_ADDR:
		fread(p, sizeof (struct in_addr), 1, fp);
		break;
	case TYPE_CHAR_PTR:
		fread(&len, sizeof (len), 1, fp);
		*(char **)p = pch = (char *) xmalloc(len+1);
		if (pch) {
			fread(pch, len, 1, fp);
			pch[len] = '\0';
		}
		break;
	case TYPE_ADDR_LIST:
		fread(&len, sizeof (len), 1, fp);
		i = sizeof (struct in_addr_list) + len -
		    sizeof (struct in_addr);
		pia = *(struct in_addr_list **)p =
		    (struct in_addr_list *)xmalloc(i);
		if (pia) {
			pia->addrcount = len / sizeof (struct in_addr);
			pia->linkcount = 1;
			fread(pia->addr, len, 1, fp);
		}
		break;
	case TYPE_INT16_LIST:
		fread(&len, sizeof (len), 1, fp);
		i = sizeof (struct int16_list) + len - sizeof (int16_t);
		psi = *(struct int16_list **)p =
		    (struct int16_list *)xmalloc(i);
		if (psi) {
			psi->count = len / sizeof (int16_t);
			fread(psi->shary, len, 1, fp);
		}
		break;
	case TYPE_BINDATA:
		fread(&len, sizeof (len), 1, fp);
		i = sizeof (struct shared_bindata) + len - 1;
		psb = *(struct shared_bindata **)p =
		    (struct shared_bindata *)xmalloc(i);
		if (psb) {
			psb->length = len;
			psb->linkcount = 1;
			fread(psb->data, len, 1, fp);
		}
		break;
	case TYPE_STRING_LIST:
		fread(&len, sizeof (len), 1, fp);
		i = sizeof (struct string_list) + (len - 1) * sizeof (char *);
		pl = *(struct string_list **)p =
		    (struct string_list *)xmalloc(i);
		if (pl) {
			pl->count = len;
			pl->linkcount = 1;
			for (n = 0; n < pl->count; n++) {
				fread(&len, sizeof (len), 1, fp);
				pl->string[n] = (char *)xmalloc(len);
				if (pl->string[n])
					fread(pl->string[n], len, 1, fp);
			}
		}
		break;
	default:
		return;
	}
	onhost(hp->flags, tag);
}

void
FileToHost(FILE *fp, Host* hp)
{
	unsigned char utag;
	void *p;
	unsigned char count;
	const HTSTRUCT *pht;
	const VTSTRUCT *vendor = NULL;

	memset(hp, 0, sizeof (struct Host));

	fread(&hp->hw, sizeof (hp->hw), 1, fp);
	if (hp->Hwaddr != NULL)
		(void) free(hp->Hwaddr);
	hp->Hwaddr = xstrdup(addrstr(hp->hw.chaddr, hp->hw.hlen, TRUE, ':'));
	onhost(hp->flags, TAG_HWADDR);
	fread(&hp->Siaddr, sizeof (hp->Siaddr), 1, fp);
	if (hp->Siaddr.s_addr != 0)
		onhost(hp->flags, TAG_SIADDR);
	fread(&hp->Yiaddr, sizeof (hp->Yiaddr), 1, fp);
	if (hp->Yiaddr.s_addr != 0)
		onhost(hp->flags, TAG_YIADDR);
	fread(&hp->lease_expires, sizeof (hp->lease_expires), 1, fp);

	fread(&count, sizeof (count), 1, fp);
	while (count-- > 0) {
		if (fread(&utag, sizeof (utag), 1, fp) != 1) {
			loge(DCUTMSG09);
			exit(1);
		}
		pht = find_bytag(utag, VENDOR_INDEPENDANT);
		if (pht) {
			p = hp->stags + utag;
			getfp(utag, pht->type, p, fp, hp);
		} else {
			loge(DCUTMSG10);
			exit(1);
		}
	}

	if (inhost(hp->flags, TAG_CLASS_ID))
		vendor = findVT(hp->stags[TAG_CLASS_ID].dhccp);

	fread(&count, sizeof (count), 1, fp);
	if (vendor) {
		while (count-- > 0) {
			if (fread(&utag, sizeof (utag), 1, fp) != 1) {
				loge(DCUTMSG09);
				exit(1);
			}
			pht = find_byindex(vendor->htindex[utag]);
			if (pht) {
				p = hp->vtags + utag;
				getfp(VENDOR_TAG(utag), pht->type, p, fp, hp);
			} else {
				loge(DCUTMSG10);
				exit(1);
			}
		}
	}
}
