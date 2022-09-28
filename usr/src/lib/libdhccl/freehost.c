/*
 * freehost.c: "Free reference counted structures".
 *
 * SYNOPSIS
 *    void free_host(struct host *)
 *
 * DESCRIPTION
 *    Frees the entire host data structure given.  Does nothing if the passed
 *    pointer is NULL.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)freehost.c 1.3 96/11/20 SMI"

#include "hostdefs.h"
#include "hosttype.h"
#include "dccommon.h"
#include <malloc.h>
#include <string.h>

static void
free_item(int type, void* p)
{
	switch (type) {
	case TYPE_CHAR_PTR:
		if (*(char **)p)
			free(*(char **)p);
		break;
	case TYPE_ADDR_LIST:
		del_iplist(*(struct in_addr_list **)p);
		break;
	case TYPE_BINDATA:
		del_bindata(*(struct shared_bindata **)p);
		break;
	case TYPE_INT16_LIST:
		if (*(int16_t **)p) free(*(void **)p);
		break;
	case TYPE_STRING_LIST:
		del_stringlist(*(struct string_list **)p);
		break;
	}
}

void
free_host_members(struct Host *hp)
{
	register int tag;
	void *p;
	const HTSTRUCT *pht;
	const VTSTRUCT *vendor = 0;

	if (!hp)
		return;
	if (inhost(hp->flags, TAG_CLASS_ID))
		vendor = findVT(hp->stags[TAG_CLASS_ID].dhccp);

	if (vendor) {
		for (tag = 1; tag <= vendor->hightag; tag++) {
			pht = find_byindex(vendor->htindex[tag]);
			p = hp->vtags + tag;
			if (!pht)
				free_item(TYPE_BINDATA, p);
			else
				free_item(pht->type, p);
		}
	}

	for (tag = 1; tag <= LAST_TAG; tag++) {
		pht = find_bytag(tag, VENDOR_INDEPENDANT);
		p = hp->stags + tag;
		if (!pht)
			free_item(TYPE_BINDATA, p);
		else
			free_item(pht->type, p);
	}

	if (hp->id)
		free(hp->id);
	memset(hp, 0, sizeof (struct Host));
}

void
free_host(struct Host *hp)
{
	free_host_members(hp);
	free((char *)hp);
}
