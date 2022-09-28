/*
 *	oid.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)oid.c	1.1	97/11/19 SMI"

#include <string.h>
#include "dh_gssapi.h"

int
__OID_equal(const gss_OID_desc * const oid1, const gss_OID_desc * const oid2)
{
	if (oid1->length != oid2->length)
		return (0);
	return (memcmp(oid1->elements, oid2->elements, oid1->length) == 0);
}


int
__OID_nel(const gss_OID_desc * const oid)
{
	int i;
	unsigned char *p = (unsigned char *)oid->elements;
	unsigned char *e = p + oid->length;

	for (i = 0; p < e; i++) {
		while (*p & 0x80) {
			p++;
			if (p == e)
				return (-1);
		}
		p++;
	}

	return (i);
}

OM_uint32
__OID_copy_desc(gss_OID dest, const gss_OID_desc * const source)
{
	dest->length = 0;
	dest->elements = (void *)New(char, source->length);
	if (dest->elements == NULL)
		return (DH_NOMEM_FAILURE);

	dest->length = source->length;
	memcpy(dest->elements, source->elements, dest->length);

	return (DH_SUCCESS);
}

OM_uint32
__OID_copy(gss_OID *dest, const gss_OID_desc * const source)
{
	gss_OID oid = New(gss_OID_desc, 1);

	*dest = NULL;

	if (oid == NULL)
		return (DH_NOMEM_FAILURE);

	if (__OID_copy_desc(oid, source) != DH_SUCCESS) {
		Free(oid);
		return (DH_NOMEM_FAILURE);
	}

	*dest = oid;
	return (DH_SUCCESS);
}

int
__OID_is_member(gss_OID_set set, const gss_OID_desc * const element)
{
	int i;

	for (i = 0; i < set->count; i++)
		if (__OID_equal(element, &set->elements[i]))
			return (TRUE);

	return (FALSE);
}

OM_uint32
__OID_copy_set(gss_OID_set *dest, gss_OID_set source)
{
	gss_OID_set set;
	int i;

	*dest = GSS_C_NO_OID_SET;

	set = New(gss_OID_set_desc, 1);
	if (set == NULL)
		return (DH_NOMEM_FAILURE);
	set->elements = New(gss_OID_desc, source->count);
	if (set->elements == NULL) {
		Free(set);
		return (DH_NOMEM_FAILURE);
	}
	set->count = source->count;

	for (i = 0; i < source->count; i++)
		if (__OID_copy_desc(&set->elements[i], &source->elements[i])
		    != DH_SUCCESS)
			break;

	if (i != source->count) {
		for (; i >= 0; i--)
			Free(set->elements[i].elements);
		Free(set->elements);
		Free(set);
		return (DH_NOMEM_FAILURE);
	}

	*dest = set;

	return (DH_SUCCESS);
}

OM_uint32
__OID_copy_set_from_array(gss_OID_set *dest,
    const gss_OID_desc *array[], size_t nel)
{
	gss_OID_set set;
	int i;

	*dest = GSS_C_NO_OID_SET;

	set = New(gss_OID_set_desc, 1);
	if (set == NULL)
		return (DH_NOMEM_FAILURE);
	set->elements = New(gss_OID_desc, nel);
	if (set->elements == NULL) {
		Free(set);
		return (DH_NOMEM_FAILURE);
	}
	set->count = nel;

	for (i = 0; i < set->count; i++)
		if (__OID_copy_desc(&set->elements[i], array[i])
		    != DH_SUCCESS)
			break;

	if (i != set->count) {
		for (; i >= 0; i--)
			Free(set->elements[i].elements);
		Free(set->elements);
		Free(set);
		return (DH_NOMEM_FAILURE);
	}

	*dest = set;

	return (DH_SUCCESS);
}

OM_uint32
__OID_to_OID_set(gss_OID_set *set, const gss_OID_desc * const oid)
{
	int rc;
	gss_OID_set s;

	if (set == NULL)
		return (DH_SUCCESS);

	if ((s = New(gss_OID_set_desc, 1)) == NULL)
		return (DH_NOMEM_FAILURE);

	s->count = 1;
	if (rc = __OID_copy(&s->elements, oid)) {
		Free(s);
		return (rc);
	}

	*set = s;

	return (DH_SUCCESS);
}
