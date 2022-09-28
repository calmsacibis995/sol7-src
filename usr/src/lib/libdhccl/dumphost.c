/*
 * dumphost.c: " Dump all the available information on the host".
 *
 * SYNOPSIS
 *    void dump_host       (FILE *fp, const Host* hp, int dumpFixed, int sep)
 *    void dump_dhcp_item  (FILE* fp, int type, const char *symbol, void* p,
 *                          int prefixOn, int sep, int limit)
 *
 * DESCRIPTION
 *    dump_host:
 *    Dump all the available information on the host pointed to by "hp".
 *    to the file pointed to by "fp".
 *
 *    dump_dhcp_item:
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)dumphost.c	1.5	96/11/25 SMI"

#include <ctype.h>
#include <stdio.h>
#include "hostdefs.h"
#include "hosttype.h"
#include "catype.h"
#include "dccommon.h"
#include "utils.h"

static void
list_ipaddresses(FILE *fp, struct in_addr_list *ipptr, int nl, int limit)
{
/*
 * Dump an entire struct in_addr_list of IP addresses to the indicated file.
 * The addresses are printed in standard ASCII "dot" notation and separated
 * from one another by a single space or by a newline if the "nl" argument
 * is non zero. Null lists produce no output (and no error).
 */
	register int count;
	register struct in_addr *addrptr;
	int sep = (nl ? '\n' : ' ');

	if (!ipptr)
		return;

	count = ipptr->addrcount;
	addrptr = ipptr->addr;
	if (count-- > 0) {
		fprintf(fp, "%s", ADDRSTR(addrptr++));
		while (count-- > 0 && --limit != 0)
			fprintf(fp, "%c%s", sep, ADDRSTR(addrptr++));
	}
}

static void
list_shortlist(FILE *fp, struct int16_list *ps, int limit)
{
	register int count;
	int16_t *pss;

	if (ps == NULL)
		return;

	count = ps->count;
	pss = ps->shary;
	if (count-- > 0) {
		fprintf(fp, "%d", *pss++);
		while (count-- > 0 && --limit != 0)
			fprintf(fp, " %d", *pss++);
	}
}

static void
list_stringlist(FILE *fp, struct string_list *pl, int nl, int limit)
{
	int i;
	int sep = (nl ? '\n' : ';');

	if (pl == NULL)
		return;
	for (i = 0; i < pl->count && limit-- != 0; i++) {
		if (i > 0)
			fputc(sep, fp);
		fprintf(fp, "\"%s\"", pl->string[i]);
	}
}

void
dump_dhcp_item(FILE *fp, int type, const char *symbol, const void *p,
    int prefixOn, int sep, int limit)
{
	switch (type) {
	case TYPE_BOOLEAN:
	case TYPE_U_CHAR:
		if (prefixOn)
			fprintf(fp, "%s=%d%c", symbol, *(unsigned char *)p,
			    sep);
		else
			fprintf(fp, "%d%c", *(unsigned char *)p, sep);
		break;

	case TYPE_INT16:
		if (prefixOn)
			fprintf(fp, "%s=%d%c", symbol, *(int16_t *)p, sep);
		else
			fprintf(fp, "%d%c", *(int16_t *)p, sep);
		break;

	case TYPE_INT32:
	case TYPE_TIME:
		if (prefixOn)
			/* XXXX ## BUG ## */
			fprintf(fp, "%s=%d%c", symbol, *(int32_t *)p, sep);
		else
			/* XXXX ##BUG## */
			fprintf(fp, "%d%c", *(int32_t *)p, sep);
		break;

	case TYPE_CHAR_PTR:
		if (prefixOn)
			fprintf(fp, "%s=%s%c", symbol, *(char **)p, sep);
		else
			fprintf(fp, "%s%c", *(char **)p, sep);
		break;

	case TYPE_ADDR:
		if (prefixOn)
			fprintf(fp, "%s=%s%c", symbol, ADDRSTR(p), sep);
		else
			fprintf(fp, "%s%c", ADDRSTR(p), sep);
		break;

	case TYPE_ADDR_LIST:
		if (prefixOn)
			fprintf(fp, "%s=", symbol);
		list_ipaddresses(fp, *(struct in_addr_list **)p,
		    sep == '\n', limit);
		fputc(sep, fp);
		break;

	case TYPE_INT16_LIST:
		if (prefixOn)
			fprintf(fp, "%s=", symbol);
		list_shortlist(fp, *(struct int16_list **)p, limit);
		fputc(sep, fp);
		break;

	case TYPE_BINDATA:
		{
			unsigned char *pc;
			unsigned l, len;

			if (prefixOn)
				fprintf(fp, "%s=", symbol);
			pc = (*(struct shared_bindata **)p)->data;
			len = (*(struct shared_bindata **)p)->length;
			for (l = len; l != 0; l--, pc++) {
				if (!isprint(*pc))
					break;
			}
			pc = (*(struct shared_bindata **)p)->data;
			if (l == 0) {
				fputc('"', fp);
				for (; len != 0; len--, pc++)
				fputc(*pc, fp);
				fputc('"', fp);
			} else {
				for (; len != 0; len--)
					fprintf(fp, "%02x", (int)*pc++);
			}
			fputc(sep, fp);
		}
		break;

	case TYPE_STRING_LIST:
		if (prefixOn)
			fprintf(fp, "%s=", symbol);
		list_stringlist(fp, *(struct string_list **)p, sep == '\n',
		    limit);
		fputc(sep, fp);
		break;
	}
}

void
dump_host(FILE *fp, const Host* hp, int dumpFixed, int sep)
{
	int tag, vtag;
	const void *p;
	int prefixOn = 1;
	const HTSTRUCT* pht;
	const VTSTRUCT *vendor = 0;

	if (hp == NULL)
		return;

	if (dumpFixed) {
		if (hp->id)
			fprintf(fp, "%s%c", hp->id, sep);

		if (hp->hw.hlen > 0 && inhost(hp->flags, TAG_HWADDR)) {
			pht = find_bytag(TAG_HWTYPE, VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%u%c",
			    (pht && pht->symbol) ? pht->symbol : "ht",
			    hp->hw.htype, sep);
			pht = find_bytag(TAG_HWADDR, VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "ha",
			    addrstr(hp->hw.chaddr, hp->hw.hlen, TRUE, '.'),
			    sep);
		}

		pht = find_bytag(TAG_CIADDR, VENDOR_INDEPENDANT);
		fprintf(fp, "%s=%s%c",
		    (pht && pht->symbol) ? pht->symbol : "ci",
		    ADDRSTR(&hp->Ciaddr), sep);
		pht = find_bytag(TAG_GIADDR, VENDOR_INDEPENDANT);
		fprintf(fp, "%s=%s%c",
		    (pht && pht->symbol) ? pht->symbol : "gi",
		    ADDRSTR(&hp->Giaddr), sep);
		pht = find_bytag(TAG_SIADDR, VENDOR_INDEPENDANT);
		fprintf(fp, "%s=%s%c",
		    (pht && pht->symbol) ? pht->symbol : "si",
		    ADDRSTR(&hp->Siaddr), sep);
		pht = find_bytag(TAG_YIADDR, VENDOR_INDEPENDANT);
		fprintf(fp, "%s=%s%c",
		    (pht && pht->symbol) ? pht->symbol : "yi",
		    ADDRSTR(&hp->Yiaddr), sep);
		if (inhost(hp->flags, TAG_NWADDR)) {
			pht = find_bytag(TAG_NWADDR, VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "nw",
			    ADDRSTR(&hp->stags[TAG_NWADDR].dhcia), sep);
		}

		showvec(fp, "flags=", (unsigned char *)hp->flags,
		    8 * sizeof (HFLAGS));
		fputc(sep, fp);

		if (inhost(hp->flags, TAG_HOMEDIR)) {
			pht = find_bytag(TAG_HOMEDIR, VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "hd",
			    hp->stags[TAG_HOMEDIR].dhccp, sep);
		}
		if (inhost(hp->flags, TAG_TFTP_ROOT)) {
			pht = find_bytag(TAG_TFTP_ROOT, VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "tf",
			    hp->stags[TAG_TFTP_ROOT].dhccp, sep);
		}
		if (inhost(hp->flags, TAG_REPLY_OVERRIDE)) {
			pht = find_bytag(TAG_REPLY_OVERRIDE,
			    VENDOR_INDEPENDANT);
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "ra",
			    ADDRSTR(&hp->stags[TAG_REPLY_OVERRIDE].dhcia), sep);
		}
		if (inhost(hp->flags, TAG_WANTS_NAME)) {
			pht = find_bytag(TAG_WANTS_NAME, VENDOR_INDEPENDANT);
			fprintf(fp, "%s%c",
			    (pht && pht->symbol) ? pht->symbol : "hn", sep);
		}
		if (inhost(hp->flags, TAG_VENDOR_COOKIE)) {
			char *q;
			pht = find_bytag(TAG_VENDOR_COOKIE, VENDOR_INDEPENDANT);
			switch (hp->stags[TAG_VENDOR_COOKIE].dhcu1) {
			case RFC_COOKIE:
				q = "rfc1048";
				break;
			case CMU_COOKIE:
				q = "cmu";
				break;
			case AUTO_COOKIE:
				q = "auto";
				break;
			case NO_COOKIE:
				q = "none";
				break;
			default:
				q = "unknown";
				break;
			}
			fprintf(fp, "%s=%s%c",
			    (pht && pht->symbol) ? pht->symbol : "hn", q, sep);
		}
	}

	for (tag = 1; tag <= LAST_TAG; tag++) {
		if (inhost(hp->flags, tag) && (tag != TAG_DHCP_MESSAGE_TYPE) &&
		    (tag != TAG_REQUEST_LIST)) {
			pht = find_bytag(tag, VENDOR_INDEPENDANT);
			p = hp->stags + tag;
			if (pht) {
				dump_dhcp_item(fp, pht->type,
				    pht->symbol == 0 ? "" : pht->symbol,
				    p, prefixOn, sep, -1);
			} else {
				char buf[16];
				sprintf(buf, "T%.3d", tag);
				dump_dhcp_item(fp, TYPE_BINDATA, buf, p,
				    prefixOn, sep, -1);
			}
		}
	}

	if (inhost(hp->flags, TAG_CLASS_ID))
		vendor = findVT(hp->stags[TAG_CLASS_ID].dhccp);
	else
		vendor = findVTbyindex(-1); /* get the default vendor */

	if (vendor) {
		for (tag = 1; tag <= vendor->hightag; tag++) {
			vtag = VENDOR_TAG(tag);
			if (!inhost(hp->flags, vtag))
				continue;
			if (vendor->htindex[tag] < 0)
				continue;
			pht = find_byindex(vendor->htindex[tag]);
			p = hp->stags + vtag;
			if (pht) {
				dump_dhcp_item(fp, pht->type,
				    pht->symbol == 0 ? "" : pht->symbol, p,
				    prefixOn, sep, -1);
			} else {
				char buf[16];
				sprintf(buf, "T%d", vtag);
				dump_dhcp_item(fp, TYPE_BINDATA, buf, p,
				    prefixOn, sep, -1);
			}
		}
	}

	if (inhost(hp->flags, TAG_DHCP_MESSAGE_TYPE))
		fprintf(fp, "mt=%d (%s)%c",
		    hp->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1,
		    DHCPmessageTypeString(
		    hp->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1), sep);

	if (inhost(hp->flags, TAG_REQUEST_LIST)) {
		showvec(fp, "rv=", (unsigned char *)hp->request_vector,
		    8 * sizeof (HFLAGS));
		fputc(sep, fp);
	}

	fputc('\n', fp);
}
