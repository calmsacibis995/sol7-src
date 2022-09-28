/*
 * tags.c: "Read and parse file describing DHCP options".
 *
 * SYNOPSIS
 * void       scanTagFile    (const char *tagfile, const char *specificVendor)
 * void      init_dhcp_types (const char *configdir, const char *specificVendor)
 * HTSTRUCT       *find_bytag       (int tag, int vindex)
 * HTSTRUCT       *find_bytag       (int tag, int vindex)
 * HTSTRUCT       *find_bysym       (const char *sym)
 * HTSTRUCT       *find_bylongname  (const char *name)
 * const HTSTRUCT *find_byindex     (int i)
 * const VTSTRUCT *findVT           (const char *vendorName)
 * const VTSTRUCT *findVTbyindex    (int vtindex)
 * void            setDefaultVendor (const char *vendorName)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)tags.c 1.5 96/11/26 SMI"

#include <ctype.h>
#include "hosttype.h"
#include "hostdefs.h"
#include "dcuttxt.h"
#include "dccommon.h"
#include <string.h>
#include <stdio.h>
#include "unixgen.h"
#include "utils.h"
#include "ca_vbuf.h"

#define	DTG 16
#define	OPTION_NUMBER_TOKEN	0
#define	OPTION_SHORT_NAME_TOKEN	1
#define	VENDOR_TAG_TOKEN	2
#define	OPTION_TYPE_TOKEN	3
#define	OPTION_LONG_NAME_TOKEN	4
#define	TOKEN_ARRAY_SIZE	(OPTION_LONG_NAME_TOKEN + 1)

#define	TAGFILE "/dhcptags"

static TFSTRUCT taginfo;
static int line;
static int defaultVendorIndex = -1;

/*
 * These "official" names (some of them anyway) are used in the boot
 * scripts in rc2.d, rcS.d. As shipped, they are the same as the
 * names in the dhctags file. When dhcpinfo looks up a symbol,
 * it compares first the official name, and then the name which
 * came from dhcptags(4). This is too prevent sys-admins from
 * screwing up the Boot by changing the symbol in dhcptags
 * without correspondingly changing the symbol in rcS/, rc2/
 */
static const char *officialNames[] = {
	0,
	"Subnet",
	"UTCoffst",
	"Router",
	"Timeserv",
	"IEN116ns",
	"DNSserv",
	"Logserv",
	"Cookie",
	"Lprserv",
	"Impress",
	"Resource",
	"Hostname",
	"Bootsize",
	"Dumpfile",
	"DNSdmain",
	"Swapserv",
	"Rootpath",
	"ExtendP",
	"IpFwdF",
	"NLrouteF",
	"PFilter",
	"MaxIpSiz",
	"IpTTL",
	"PathTO",
	"PathTbl",
	"MTU",
	"SameMtuF",
	"Broadcst",
	"MaskDscF",
	"MaskSupF",
	"RDiscvyF",
	"RSolictS",
	"StaticRt",
	"TrailerF",
	"ArpTimeO",
	"EthEncap",
	"TcpTTL",
	"TcpKaInt",
	"TcpKaGbF",
	"NISdmain",
	"NISservs",
	"NTPservs",
	0,
	"NetBNms",
	"NetBDsts",
	"NetBNdT",
	"NetBScop",
	"XFontSrv",
	"XDispMgr",
	"RequestIP",
	"LeaseTim",
	"Overload",
	"MsgType",
	"ServerIP",
	"ReqVec",
	"Message",
	"MaxMsgSz",
	"T1Time",
	"T2Time",
	"Vendor",
	"ClientID",
	"NW_dmain",
	"NWIPOpts",
	"NIS+dom",
	"NIS+serv",
	"TFTPsrvN",
	"OptBootF",
	"MblIPAgt",
	"SMTPserv",
	"POP3serv",
	"NNTPserv",
	"WWWservs",
	"Fingersv",
	"IRCservs",
	"STservs",
	"STDAservs",
	"UserClass",
};

static const TSSTRUCT tsstd[] = {
	"byte",		TYPE_U_CHAR,
	"octet",	TYPE_U_CHAR,
	"int8",		TYPE_U_CHAR,
	"int16",	TYPE_INT16,
	"int32",	TYPE_INT32,
	"time", 	TYPE_TIME,
	"string", 	TYPE_CHAR_PTR,
	"ip", 		TYPE_ADDR,
	"iplist",	TYPE_ADDR_LIST,
	"int16list",	TYPE_INT16_LIST,
	"opaque",	TYPE_BINDATA,
	"boolean",	TYPE_BOOLEAN,
	"stringlist",	TYPE_STRING_LIST
};

static int
cmpsymbol(const HTSTRUCT *a, const HTSTRUCT *b)
{
	return (strcasecmp(a->symbol, b->symbol));
}

const char *
tsValueToString(int val)
{
	register int i;

	for (i = 0; i < sizeof (tsstd) / sizeof (tsstd[0]); i++) {
		if (tsstd[i].val == val)
			return (tsstd[i].str);
	}
	return ("");
}

static int
tsStringToValue(const char *str)
{
	register int i;

	for (i = 0; i < sizeof (tsstd) / sizeof (tsstd[0]); i++) {
		if (!strcasecmp(str, tsstd[i].str))
			return (tsstd[i].val);
	}
	return (TYPE_UNKNOWN);
}

static void
parseEntry(int ntokens, char *const p[], TFSTRUCT *tfp, const char *vendor,
    const char *dhcptagsfile)
{
	int16_t vindex;
	int32_t ntag, tag;
	VTSTRUCT *vtp;
	HTSTRUCT *htp;
	int type;
	int rc;
	int lowestValidTag, highestValidTag;

	rc = sscanf(p[OPTION_NUMBER_TOKEN], "%d", &tag);
	if (rc != 1) {
		logw(DCUTMSG00, p[OPTION_NUMBER_TOKEN], line, dhcptagsfile);
		return;
	}

	type = tsStringToValue(p[OPTION_TYPE_TOKEN]);
	if (type == TYPE_UNKNOWN) {
		logw(DCUTMSG01, p[OPTION_TYPE_TOKEN], line, dhcptagsfile);
		return;
	}

	/* Lookup the vendor */

	if (!strcasecmp(p[VENDOR_TAG_TOKEN], "pseudo")) {
		vindex = DHCP_PSEUDO;
		vtp = tfp->vtp;
		lowestValidTag  = PSEUDO_TAG(OPTION_NUMBER_TOKEN);
		highestValidTag = LAST_PSEUDO_TAG;
	} else if (p[VENDOR_TAG_TOKEN][0] == '-' &&
	    p[VENDOR_TAG_TOKEN][1] == '\0') {
		vindex = VENDOR_INDEPENDANT;
		vtp = tfp->vtp + 1;
		lowestValidTag = 1;
		highestValidTag = 254;
	} else if (vendor && strcmp(p[VENDOR_TAG_TOKEN], vendor) == 0) {
		return;
	} else {
		for (vindex = FIRST_VENDOR; vindex < tfp->vt_count; vindex++) {
			if (strcmp(p[VENDOR_TAG_TOKEN],
			    tfp->vtp[vindex].vendorClass) == 0)
				break;
		}
		if (vindex < tfp->vt_count)
			vtp = tfp->vtp + vindex;
		else {
			tfp->vt_count++;
			tfp->vtp = (VTSTRUCT *)xrealloc(tfp->vtp,
			    tfp->vt_count * sizeof (VTSTRUCT));
			vtp = tfp->vtp + vindex;
			vtp->vendorClass = xstrdup(p[VENDOR_TAG_TOKEN]);
			vtp->hightag = 0;
			vtp->htindex = 0;
			vtp->selfindex = vindex;
		}
		lowestValidTag = VENDOR_TAG(0);
		highestValidTag = VENDOR_TAG(255);
	}

	if (tag < lowestValidTag || tag > highestValidTag) {
		logw(DCUTMSG02, p[OPTION_NUMBER_TOKEN], line, dhcptagsfile,
		    lowestValidTag, highestValidTag);
		return;
	}

	ntag = NORMALIZE_TAG(tag);

	/* Add to list */
	if (ntag > vtp->hightag)
		vtp->hightag = ntag;

	if (tfp->ht_count >= tfp->ht_slots) {
		tfp->ht_slots += DTG;
		tfp->htp = (HTSTRUCT *)xrealloc(tfp->htp,
		    tfp->ht_slots * sizeof (HTSTRUCT));
	}

	htp = tfp->htp + tfp->ht_count;
	tfp->ht_count++;
	htp->tag = tag;
	htp->type = type;
	htp->symbol = xstrdup(p[OPTION_SHORT_NAME_TOKEN]);
	htp->group = 0;
	htp->vindex = vindex;
	htp->longname = 0;
	htp->offname = 0;
	if (vindex == VENDOR_INDEPENDANT &&
	    tag < sizeof (officialNames) / sizeof (officialNames[0])) {
		htp->offname = (char *)officialNames[tag];
	}

	/* Copy the long name but omit the double quotes (if any) */
	if (ntokens >= OPTION_LONG_NAME_TOKEN) {
		const char *q = p[OPTION_LONG_NAME_TOKEN];
		int len = strlen(q);
		if (len > 0) {
			if (q[len-1] == '"')
				len--;
			if (q[0] == '"') {
				q++;
				len--;
			}
			if (len > 0) {
				htp->longname = xmalloc(1 + len);
				strncpy(htp->longname, q, len);
				htp->longname[len] = '\0';
			}
		}
	}
}

static void
initialiseTF(TFSTRUCT *tfp)
{
	tfp->vt_count = 2; /* "pseudo" tags and standard tags */

	tfp->ht_count = 0;
	tfp->ht_slots = 256;

	tfp->vtp = (VTSTRUCT *)xmalloc(tfp->vt_count * sizeof (VTSTRUCT));
	tfp->htp = (HTSTRUCT *)xmalloc(tfp->ht_slots * sizeof (HTSTRUCT));
	tfp->vtp[0].vendorClass = xstrdup("pseudo");
	tfp->vtp[1].vendorClass = xstrdup("standard");
	tfp->vtp[0].hightag = tfp->vtp[1].hightag = -1;
	tfp->vtp[0].htindex = tfp->vtp[1].htindex = 0;
	tfp->vtp[0].selfindex = 0;
	tfp->vtp[1].selfindex = 1;
}

static void
startScan(const char *dhcptagsfile, TFSTRUCT *tfp, const char *vendor)
{
	register int i;
	register int j;
	FILE *f;
	char *q, *p[TOKEN_ARRAY_SIZE + 1];
	int ntokens;
	VTSTRUCT *vtp;
	HTSTRUCT *htp;
	struct Vbuf b;

	f = fopen(dhcptagsfile, "r");
	if (f == 0) {
		loge(DCUTMSG04, dhcptagsfile, SYSMSG);
		exit(8);
	}

	initialiseTF(tfp);

	b.len = 0;
	b.dbuf = 0;
	while (xgets(f, &b) != EOF) {
		q = b.dbuf;
		line++;
		while (*q != '\0' && isspace(*q))
			q++;
		if (*q == '\0' || *q == '\n' || *q == '#')
			continue;
		ntokens = tokeniseString(q, p, 1, TOKEN_ARRAY_SIZE);
		if (ntokens != TOKEN_ARRAY_SIZE) {
			logw(DCUTMSG03, line, dhcptagsfile);
			continue;
		}
		parseEntry(ntokens, p, tfp, vendor, dhcptagsfile);
		for (i = 0; i < ntokens; i++)
			free(p[i]);
	}
	free(b.dbuf);
	fclose(f);

	/*  Sort the list by symbol */

	qsort(tfp->htp, tfp->ht_count, sizeof (HTSTRUCT),
	(int (*)(const void *, const void *))cmpsymbol);

	for (i = 0; i < tfp->vt_count; i++) {
		vtp = tfp->vtp + i;
		vtp->htindex = (int16_t *)xmalloc(
		    (1 + vtp->hightag) * sizeof (int16_t));
		for (j = 0; j <= vtp->hightag; j++)
			vtp->htindex[j] = -1;
	}

	for (i = 0; i < tfp->ht_count; i++) {
		htp = tfp->htp + i;
		vtp = tfp->vtp + htp->vindex;
		vtp->htindex[NORMALIZE_TAG(htp->tag)] = i;
	}
}

void
scanTagFile(const char *tagfile, const char *specificVendor)
{
	startScan(tagfile, &taginfo, specificVendor);
}

TFSTRUCT *
getTagInfo(void)
{
	return (&taginfo);
}

void
init_dhcp_types(const char *configdir, const char *specificVendor)
{
	char *tagfile;
	int len;

	len = strlen(configdir) + sizeof (TAGFILE);
	tagfile = (char *)xmalloc(len);
	sprintf(tagfile, "%s%s", configdir, TAGFILE);
	scanTagFile(tagfile, specificVendor);
	free(tagfile);
}

HTSTRUCT *
find_bytag(int tag, int vindex)
{
	VTSTRUCT *vtp;

	if (IS_PSEUDO(tag))
		vindex = 0;
	else if (vindex == 0)
		vindex = 1;
	else if (vindex == VENDOR_UNSPECIFIED)
		vindex = defaultVendorIndex;
	if (vindex < 0 || vindex >= taginfo.vt_count)
		return (0);
	vtp = taginfo.vtp + vindex;
	tag = NORMALIZE_TAG(tag);
	if (tag < 0 || tag > vtp->hightag || vtp->htindex[tag] < 0)
		return (0);
	return (taginfo.htp + vtp->htindex[tag]);
}

HTSTRUCT *
find_bysym(const char *sym)
{
	register int i;
	HTSTRUCT *htp;

	for (i = 0; i < taginfo.ht_count; i++) {
		htp = taginfo.htp + i;
		if (htp->offname != 0 && strcasecmp(sym, htp->offname) == 0)
			return (htp);
		if (htp->symbol != 0 && strcasecmp(sym, htp->symbol) == 0)
			return (htp);
	}
	return (0);
}

HTSTRUCT *
find_bylongname(const char *name)
{
	register int i;
	HTSTRUCT *htp;

	for (i = 0; i < taginfo.ht_count; i++) {
		htp = taginfo.htp+i;
		if (htp->symbol && htp->longname != 0 &&
		    !strcasecmp(name, htp->longname))
			return (htp);
	}
	return (0);
}

const HTSTRUCT *
find_byindex(int i)
{
	if (i < 0 || i >= taginfo.ht_count)
		return (0);
	return (taginfo.htp + i);
}

const VTSTRUCT *
findVT(const char *vendorName)
{
	register int i;

	if (vendorName == 0 || vendorName[0] == '\0')
		return (taginfo.vtp);
	else {
		for (i = 2; i < taginfo.vt_count; i++) {
			if (strcmp(taginfo.vtp[i].vendorClass, vendorName) == 0)
				return (taginfo.vtp + i);
		}
	}
	return (0);
}

const VTSTRUCT *
findVTbyindex(int vtindex)
{
	register const VTSTRUCT *retval = NULL;

	if (vtindex < 0) {
		if (defaultVendorIndex >= 0)
			retval = taginfo.vtp + defaultVendorIndex;
	} else if (vtindex < taginfo.vt_count)
		retval = taginfo.vtp + vtindex;
	return (retval);
}

void
setDefaultVendor(const char *vendorName)
{
	const VTSTRUCT *vtp;

	if (vendorName == 0)
		return;
	vtp = findVT(vendorName);
	if (vtp)
		defaultVendorIndex = vtp->selfindex;
}
