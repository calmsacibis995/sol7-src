/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _DCCOMMON_H
#define	_DCCOMMON_H

#pragma ident	"@(#)dccommon.h	1.4	96/11/26 SMI"

#include <stdio.h>
#include <hostdefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct HTstruct;
struct bootp;
struct in_addr_list;
struct string_list;

#ifdef	__CODE_UNUSED
void append_item(const struct HTstruct *, void *, const void *);
void copy_host(struct Host *, const struct Host *, const u_char *);
#endif	/* __CODE_UNUSED */
void BootpToHost(const struct bootp *, struct Host *, int);
const char *dccommon_txt(int);
void del_bindata(struct shared_bindata *);
void del_iplist(struct in_addr_list *);
void del_stringlist(struct string_list *);
void deserializeItem(int, int, void *, const unsigned char *);
const char *DHCPmessageTypeString(int);
void dump_dhcp_item(FILE *, int, const char *, const void *, int, int, int);
void dump_host(FILE *, const struct Host *, int, int);
void FileToHost(FILE *, struct Host *);
void free_host(struct Host *);
void free_host_members(struct Host *);
void HostToBootp(const struct Host *, struct bootp *, int, int);
void HostToFile(const struct Host *, FILE *);
void init_dhcp_types(const char *, const char *);
int need(int, const void *);
void serializeItem(int, int, const void *, unsigned char *);

#ifdef	__cplusplus
}
#endif

#endif /* _DCCOMMON_H */
