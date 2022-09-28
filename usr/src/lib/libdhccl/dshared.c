/*
 * dshared.c: "Delete reference counted structures".
 *
 * SYNOPSIS
 *    void del_bindata(struct shared_bindata*)
 *    void del_iplist (struct in_addr_list*)
 *    void del_int16list(struct int16_list*)
 *    void del_stringlist (struct string_list*)
 *
 * DESCRIPTION
 *    Each of these functions decrements the linkcount member of the relevant
 *    structure. If the count reaches zero the memory associated with the
 *    data is freed. If the pointer is null they do nothing.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)dshared.c 1.3 96/11/26 SMI"

#include "hostdefs.h"
#include <malloc.h>

void
del_bindata(struct shared_bindata *dataptr)
{
	if (dataptr && --dataptr->linkcount <= 0)
		free((char *)dataptr);
}

void
del_iplist(struct in_addr_list *iplist)
{
	if (iplist && --iplist->linkcount <= 0)
		free((char *)iplist);
}

#ifdef	__CODE_UNUSED
void
del_int16list(struct int16_list *i2list)
{
	if (i2list)
		free((char *)i2list);
}
#endif	/* __CODE_UNUSED */

void
del_stringlist(struct string_list *pl)
{
	int i;

	if (pl == NULL || --pl->linkcount > 0)
		return;
	for (i = 0; i < pl->count; i++)
		free(pl->string[i]);
	free((char *)pl);
}
