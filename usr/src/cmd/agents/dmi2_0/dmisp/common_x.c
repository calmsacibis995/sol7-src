/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "common.h"

bool_t
xdr_DmiSetMode(register XDR *xdrs, DmiSetMode *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiSetMode_t(register XDR *xdrs, DmiSetMode_t *objp)
{

	register long *buf;

	if (!xdr_DmiSetMode(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRequestMode(register XDR *xdrs, DmiRequestMode *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRequestMode_t(register XDR *xdrs, DmiRequestMode_t *objp)
{

	register long *buf;

	if (!xdr_DmiRequestMode(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiStorageType(register XDR *xdrs, DmiStorageType *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiStorageType_t(register XDR *xdrs, DmiStorageType_t *objp)
{

	register long *buf;

	if (!xdr_DmiStorageType(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessMode(register XDR *xdrs, DmiAccessMode *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessMode_t(register XDR *xdrs, DmiAccessMode_t *objp)
{

	register long *buf;

	if (!xdr_DmiAccessMode(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiDataType(register XDR *xdrs, DmiDataType *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiDataType_t(register XDR *xdrs, DmiDataType_t *objp)
{

	register long *buf;

	if (!xdr_DmiDataType(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileType(register XDR *xdrs, DmiFileType *objp)
{

	register long *buf;

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileType_t(register XDR *xdrs, DmiFileType_t *objp)
{

	register long *buf;

	if (!xdr_DmiFileType(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiId_t(register XDR *xdrs, DmiId_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiHandle_t(register XDR *xdrs, DmiHandle_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiCounter_t(register XDR *xdrs, DmiCounter_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiErrorStatus_t(register XDR *xdrs, DmiErrorStatus_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiCounter64_t(register XDR *xdrs, DmiCounter64_t objp)
{

	register long *buf;

	if (!xdr_vector(xdrs, (char *)objp, 2,
		sizeof (u_long), (xdrproc_t) xdr_u_long))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGauge_t(register XDR *xdrs, DmiGauge_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiUnsigned_t(register XDR *xdrs, DmiUnsigned_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiInteger_t(register XDR *xdrs, DmiInteger_t *objp)
{

	register long *buf;

	if (!xdr_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiInteger64_t(register XDR *xdrs, DmiInteger64_t objp)
{

	register long *buf;

	if (!xdr_vector(xdrs, (char *)objp, 2,
		sizeof (u_long), (xdrproc_t) xdr_u_long))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiBoolean_t(register XDR *xdrs, DmiBoolean_t *objp)
{

	register long *buf;

	if (!xdr_u_long(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiTimestamp(register XDR *xdrs, DmiTimestamp *objp)
{

	register long *buf;

	int i;
	if (!xdr_vector(xdrs, (char *)objp->year, 4,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->month, 2,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->day, 2,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->hour, 2,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->minutes, 2,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->seconds, 2,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->dot))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->microSeconds, 6,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->plusOrMinus))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->utcOffset, 3,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->padding, 3,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiTimestamp_t(register XDR *xdrs, DmiTimestamp_t *objp)
{

	register long *buf;

	if (!xdr_DmiTimestamp(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiString(register XDR *xdrs, DmiString *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->body.body_val, (u_int *) &objp->body.body_len, ~0,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiString_t(register XDR *xdrs, DmiString_t *objp)
{

	register long *buf;

	if (!xdr_DmiString(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiStringPtr_t(register XDR *xdrs, DmiStringPtr_t *objp)
{

	register long *buf;

	if (!xdr_pointer(xdrs, (char **)objp, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiOctetString(register XDR *xdrs, DmiOctetString *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->body.body_val, (u_int *) &objp->body.body_len, ~0,
		sizeof (char), (xdrproc_t) xdr_char))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiOctetString_t(register XDR *xdrs, DmiOctetString_t *objp)
{

	register long *buf;

	if (!xdr_DmiOctetString(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiDataUnion(register XDR *xdrs, DmiDataUnion *objp)
{

	register long *buf;

	if (!xdr_DmiDataType_t(xdrs, &objp->type))
		return (FALSE);
	switch (objp->type) {
	case MIF_COUNTER:
		if (!xdr_DmiCounter_t(xdrs, &objp->DmiDataUnion_u.counter))
			return (FALSE);
		break;
	case MIF_COUNTER64:
		if (!xdr_DmiCounter64_t(xdrs, objp->DmiDataUnion_u.counter64))
			return (FALSE);
		break;
	case MIF_GAUGE:
		if (!xdr_DmiGauge_t(xdrs, &objp->DmiDataUnion_u.gauge))
			return (FALSE);
		break;
	case MIF_INTEGER:
		if (!xdr_DmiInteger_t(xdrs, &objp->DmiDataUnion_u.integer))
			return (FALSE);
		break;
	case MIF_INTEGER64:
		if (!xdr_DmiInteger64_t(xdrs, objp->DmiDataUnion_u.integer64))
			return (FALSE);
		break;
	case MIF_OCTETSTRING:
		if (!xdr_pointer(xdrs, (char **)&objp->DmiDataUnion_u.octetstring, sizeof (DmiOctetString_t), (xdrproc_t) xdr_DmiOctetString_t))
			return (FALSE);
		break;
	case MIF_DISPLAYSTRING:
		if (!xdr_pointer(xdrs, (char **)&objp->DmiDataUnion_u.str, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
			return (FALSE);
		break;
	case MIF_DATE:
		if (!xdr_pointer(xdrs, (char **)&objp->DmiDataUnion_u.date, sizeof (DmiTimestamp_t), (xdrproc_t) xdr_DmiTimestamp_t))
			return (FALSE);
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_DmiDataUnion_t(register XDR *xdrs, DmiDataUnion_t *objp)
{

	register long *buf;

	if (!xdr_DmiDataUnion(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiEnumInfo(register XDR *xdrs, DmiEnumInfo *objp)
{

	register long *buf;

	if (!xdr_pointer(xdrs, (char **)&objp->name, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_DmiInteger_t(xdrs, &objp->value))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiEnumInfo_t(register XDR *xdrs, DmiEnumInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiEnumInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiEnumList(register XDR *xdrs, DmiEnumList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiEnumInfo_t), (xdrproc_t) xdr_DmiEnumInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiEnumList_t(register XDR *xdrs, DmiEnumList_t *objp)
{

	register long *buf;

	if (!xdr_DmiEnumList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeInfo(register XDR *xdrs, DmiAttributeInfo *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->id))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->name, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->pragma, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->description, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_DmiStorageType_t(xdrs, &objp->storage))
		return (FALSE);
	if (!xdr_DmiAccessMode_t(xdrs, &objp->access))
		return (FALSE);
	if (!xdr_DmiDataType_t(xdrs, &objp->type))
		return (FALSE);
	if (!xdr_DmiUnsigned_t(xdrs, &objp->maxSize))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->enumList, sizeof (DmiEnumList_t), (xdrproc_t) xdr_DmiEnumList_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeInfo_t(register XDR *xdrs, DmiAttributeInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiAttributeInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeData(register XDR *xdrs, DmiAttributeData *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->id))
		return (FALSE);
	if (!xdr_DmiDataUnion_t(xdrs, &objp->data))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeData_t(register XDR *xdrs, DmiAttributeData_t *objp)
{

	register long *buf;

	if (!xdr_DmiAttributeData(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeIds(register XDR *xdrs, DmiAttributeIds *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiId_t), (xdrproc_t) xdr_DmiId_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeIds_t(register XDR *xdrs, DmiAttributeIds_t *objp)
{

	register long *buf;

	if (!xdr_DmiAttributeIds(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupInfo(register XDR *xdrs, DmiGroupInfo *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->id))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->name, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->pragma, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->className, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->description, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->keyList, sizeof (DmiAttributeIds_t), (xdrproc_t) xdr_DmiAttributeIds_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupInfo_t(register XDR *xdrs, DmiGroupInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiGroupInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentInfo(register XDR *xdrs, DmiComponentInfo *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->id))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->name, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->pragma, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->description, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_DmiBoolean_t(xdrs, &objp->exactMatch))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentInfo_t(register XDR *xdrs, DmiComponentInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiComponentInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileDataInfo(register XDR *xdrs, DmiFileDataInfo *objp)
{

	register long *buf;

	if (!xdr_DmiFileType_t(xdrs, &objp->fileType))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->fileData, sizeof (DmiOctetString_t), (xdrproc_t) xdr_DmiOctetString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileDataInfo_t(register XDR *xdrs, DmiFileDataInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiFileDataInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiClassNameInfo(register XDR *xdrs, DmiClassNameInfo *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->id))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->className, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiClassNameInfo_t(register XDR *xdrs, DmiClassNameInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiClassNameInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeValues(register XDR *xdrs, DmiAttributeValues *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiAttributeData_t), (xdrproc_t) xdr_DmiAttributeData_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeValues_t(register XDR *xdrs, DmiAttributeValues_t *objp)
{

	register long *buf;

	if (!xdr_DmiAttributeValues(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRowRequest(register XDR *xdrs, DmiRowRequest *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->groupId))
		return (FALSE);
	if (!xdr_DmiRequestMode_t(xdrs, &objp->requestMode))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->keyList, sizeof (DmiAttributeValues_t), (xdrproc_t) xdr_DmiAttributeValues_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->ids, sizeof (DmiAttributeIds_t), (xdrproc_t) xdr_DmiAttributeIds_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRowRequest_t(register XDR *xdrs, DmiRowRequest_t *objp)
{

	register long *buf;

	if (!xdr_DmiRowRequest(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRowData(register XDR *xdrs, DmiRowData *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->groupId))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->className, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->keyList, sizeof (DmiAttributeValues_t), (xdrproc_t) xdr_DmiAttributeValues_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->values, sizeof (DmiAttributeValues_t), (xdrproc_t) xdr_DmiAttributeValues_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRowData_t(register XDR *xdrs, DmiRowData_t *objp)
{

	register long *buf;

	if (!xdr_DmiRowData(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeList(register XDR *xdrs, DmiAttributeList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiAttributeInfo_t), (xdrproc_t) xdr_DmiAttributeInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAttributeList_t(register XDR *xdrs, DmiAttributeList_t *objp)
{

	register long *buf;

	if (!xdr_DmiAttributeList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupList(register XDR *xdrs, DmiGroupList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiGroupInfo_t), (xdrproc_t) xdr_DmiGroupInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupList_t(register XDR *xdrs, DmiGroupList_t *objp)
{

	register long *buf;

	if (!xdr_DmiGroupList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentList(register XDR *xdrs, DmiComponentList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiComponentInfo_t), (xdrproc_t) xdr_DmiComponentInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentList_t(register XDR *xdrs, DmiComponentList_t *objp)
{

	register long *buf;

	if (!xdr_DmiComponentList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileDataList(register XDR *xdrs, DmiFileDataList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiFileDataInfo_t), (xdrproc_t) xdr_DmiFileDataInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileDataList_t(register XDR *xdrs, DmiFileDataList_t *objp)
{

	register long *buf;

	if (!xdr_DmiFileDataList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiClassNameList(register XDR *xdrs, DmiClassNameList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiClassNameInfo_t), (xdrproc_t) xdr_DmiClassNameInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiClassNameList_t(register XDR *xdrs, DmiClassNameList_t *objp)
{

	register long *buf;

	if (!xdr_DmiClassNameList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiStringList(register XDR *xdrs, DmiStringList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiStringPtr_t), (xdrproc_t) xdr_DmiStringPtr_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiStringList_t(register XDR *xdrs, DmiStringList_t *objp)
{

	register long *buf;

	if (!xdr_DmiStringList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileTypeList(register XDR *xdrs, DmiFileTypeList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiFileType_t), (xdrproc_t) xdr_DmiFileType_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiFileTypeList_t(register XDR *xdrs, DmiFileTypeList_t *objp)
{

	register long *buf;

	if (!xdr_DmiFileTypeList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiMultiRowRequest(register XDR *xdrs, DmiMultiRowRequest *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiRowRequest_t), (xdrproc_t) xdr_DmiRowRequest_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiMultiRowRequest_t(register XDR *xdrs, DmiMultiRowRequest_t *objp)
{

	register long *buf;

	if (!xdr_DmiMultiRowRequest(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiMultiRowData(register XDR *xdrs, DmiMultiRowData *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiRowData_t), (xdrproc_t) xdr_DmiRowData_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiMultiRowData_t(register XDR *xdrs, DmiMultiRowData_t *objp)
{

	register long *buf;

	if (!xdr_DmiMultiRowData(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessData(register XDR *xdrs, DmiAccessData *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->groupId))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->attributeId))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessData_t(register XDR *xdrs, DmiAccessData_t *objp)
{

	register long *buf;

	if (!xdr_DmiAccessData(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessDataList(register XDR *xdrs, DmiAccessDataList *objp)
{

	register long *buf;

	if (!xdr_array(xdrs, (char **)&objp->list.list_val, (u_int *) &objp->list.list_len, ~0,
		sizeof (DmiAccessData_t), (xdrproc_t) xdr_DmiAccessData_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiAccessDataList_t(register XDR *xdrs, DmiAccessDataList_t *objp)
{

	register long *buf;

	if (!xdr_DmiAccessDataList(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRegisterInfo(register XDR *xdrs, DmiRegisterInfo *objp)
{

	register long *buf;

	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_u_long(xdrs, &objp->prognum))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->accessData, sizeof (DmiAccessDataList_t), (xdrproc_t) xdr_DmiAccessDataList_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiRegisterInfo_t(register XDR *xdrs, DmiRegisterInfo_t *objp)
{

	register long *buf;

	if (!xdr_DmiRegisterInfo(xdrs, objp))
		return (FALSE);
	return (TRUE);
}