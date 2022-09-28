/*
 * htof.c: "Write DHCP received configuration to disk".
 *
 * SYNOPSIS
 *	void HostToFile(const Host*, FILE*)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)htof.c 1.2 96/11/20 SMI"

#include <stdio.h>
#include <string.h>
#include "hostdefs.h"
#include "hosttype.h"
#include "magic.h"

static int
putfp(int tag, int type, const void *p, FILE *fp)
{
	short len;
	unsigned char utag = tag;
	int written = 1;
	int i;

	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		fwrite(&utag, sizeof (utag), 1, fp);
		fwrite(p, 1, 1, fp);
		break;
	case TYPE_INT16:
		fwrite(&utag, sizeof (utag), 1, fp);
		fwrite(p, sizeof (int16_t), 1, fp);
		break;
	case TYPE_INT32:
	case TYPE_TIME:
		fwrite(&utag, sizeof (utag), 1, fp);
		fwrite(p, sizeof (int32_t), 1, fp);
		break;
	case TYPE_ADDR:
		fwrite(&utag, sizeof (utag), 1, fp);
		fwrite(p, sizeof (struct in_addr), 1, fp);
		break;
	case TYPE_ADDR_LIST:
		fwrite(&utag, sizeof (utag), 1, fp);
		{
			struct in_addr_list *pil = *(struct in_addr_list **)p;
			len =  pil->addrcount*sizeof (struct in_addr);
			fwrite(&len, sizeof (len), 1, fp);
			fwrite(pil->addr, len, 1, fp);
		}
		break;
	case TYPE_CHAR_PTR:
		fwrite(&utag, sizeof (utag), 1, fp);
		{
			char *s = *(char **)p;
			len = strlen(s);
			fwrite(&len, sizeof (len), 1, fp);
			fwrite(s, len, 1, fp);
		}
		break;
	case TYPE_INT16_LIST:
		fwrite(&utag, sizeof (utag), 1, fp);
		{
			struct int16_list *psl = *(struct int16_list **)p;
			len = psl->count*sizeof (int16_t);
			fwrite(&len, sizeof (len), 1, fp);
			fwrite(psl->shary, sizeof (int16_t), psl->count, fp);
		}
		break;
	case TYPE_BINDATA:
		fwrite(&utag, sizeof (utag), 1, fp);
		{
			struct shared_bindata *psb =
			    *(struct shared_bindata **)p;
			len = psb->length;
			fwrite(&len, sizeof (len), 1, fp);
			fwrite(psb->data, len, 1, fp);
		}
		break;
	case TYPE_STRING_LIST:
		fwrite(&utag, sizeof (utag), 1, fp);
		{
			struct string_list *pl = *(struct string_list **)p;
			len = pl->count;
			fwrite(&len, sizeof (len), 1, fp);
			for (i = 0; i < pl->count; i++) {
				len = strlen(pl->string[i]) + 1;
				fwrite(&len, sizeof (len), 1, fp);
				fwrite(pl->string[i], len, 1, fp);
			}
		}
		break;
	default:
		written = 0;
		break;
	}
	return (written);
}

void
HostToFile(const Host* hp, FILE *fp)
{
	register short tag;
	const HTSTRUCT *pht;
	const VTSTRUCT *vendor = 0;
	const void *p;
	unsigned char count;
	MAGIC_COOKIE cookie;
	int32_t offsetGenericCount, offsetVendorCount;

	MAKE_COOKIE(cookie, CF_COOKIE, MAJOR_VERSION, MINOR_VERSION);
	fwrite(cookie, sizeof (cookie), 1, fp);
	fwrite(&hp->hw, sizeof (hp->hw), 1, fp);
	fwrite(&hp->Siaddr, sizeof (hp->Siaddr), 1, fp);
	fwrite(&hp->Yiaddr, sizeof (hp->Yiaddr), 1, fp);
	fwrite(&hp->lease_expires, sizeof (hp->lease_expires), 1, fp);

	offsetGenericCount = ftell(fp);
	count = 0;
	fwrite(&count, sizeof (count), 1, fp); /* reserve place in file */

	for (tag = 1; tag <= LAST_TAG; tag++) {
		if (inhost(hp->flags, tag)) {
			pht = find_bytag(tag, VENDOR_INDEPENDANT);
			if ((pht != 0) && (tag != TAG_DHCP_MESSAGE_TYPE) &&
			    (tag != TAG_DHCP_SIZE) &&
			    (tag != TAG_REQUEST_LIST) &&
			    (tag != TAG_OVERLOAD)) {
				p = hp->stags + tag;
				count += putfp(tag, pht->type, p, fp);
			}
		}
	}
	offsetVendorCount = ftell(fp);

	/* Write count of generic tag items: */
	fseek(fp, offsetGenericCount, 0);
	fwrite(&count, sizeof (count), 1, fp);

	fseek(fp, offsetVendorCount, 0);
	count = 0;
	fwrite(&count, sizeof (count), 1, fp); /* reserve place in file */

	if (inhost(hp->flags, TAG_CLASS_ID))
		vendor = findVT(hp->stags[TAG_CLASS_ID].dhccp);
	if (vendor) {
		for (tag = 1; tag <= vendor->hightag; tag++) {
			if (inhost(hp->flags, VENDOR_TAG(tag))) {
				pht = find_byindex(vendor->htindex[tag]);
				if (pht != 0) {
					p = hp->vtags + tag;
					count += putfp(tag, pht->type, p, fp);
				}
			}
		}
	}

	/* Write count of Vendor specific tag items */
	fseek(fp, offsetVendorCount, 0);
	fwrite(&count, sizeof (count), 1, fp);
}
