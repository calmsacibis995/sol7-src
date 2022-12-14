/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "mi_indicate.h"

bool_t
xdr_DmiNodeAddress(register XDR *xdrs, DmiNodeAddress *objp)
{

	register long *buf;

	if (!xdr_pointer(xdrs, (char **)&objp->address, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->rpc, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->transport, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiNodeAddress_t(register XDR *xdrs, DmiNodeAddress_t *objp)
{

	register long *buf;

	if (!xdr_DmiNodeAddress(xdrs, objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiDeliverEventIN(register XDR *xdrs, DmiDeliverEventIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->language, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->timestamp, sizeof (DmiTimestamp_t), (xdrproc_t) xdr_DmiTimestamp_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->rowData, sizeof (DmiMultiRowData_t), (xdrproc_t) xdr_DmiMultiRowData_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentAddedIN(register XDR *xdrs, DmiComponentAddedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->info, sizeof (DmiComponentInfo_t), (xdrproc_t) xdr_DmiComponentInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiComponentDeletedIN(register XDR *xdrs, DmiComponentDeletedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiLanguageAddedIN(register XDR *xdrs, DmiLanguageAddedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->language, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiLanguageDeletedIN(register XDR *xdrs, DmiLanguageDeletedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->language, sizeof (DmiString_t), (xdrproc_t) xdr_DmiString_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupAddedIN(register XDR *xdrs, DmiGroupAddedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->info, sizeof (DmiGroupInfo_t), (xdrproc_t) xdr_DmiGroupInfo_t))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiGroupDeletedIN(register XDR *xdrs, DmiGroupDeletedIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->compId))
		return (FALSE);
	if (!xdr_DmiId_t(xdrs, &objp->groupId))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_DmiSubscriptionNoticeIN(register XDR *xdrs, DmiSubscriptionNoticeIN *objp)
{

	register long *buf;

	if (!xdr_DmiUnsigned_t(xdrs, &objp->handle))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->sender, sizeof (DmiNodeAddress_t), (xdrproc_t) xdr_DmiNodeAddress_t))
		return (FALSE);
	if (!xdr_DmiBoolean_t(xdrs, &objp->expired))
		return (FALSE);
	if (!xdr_DmiRowData_t(xdrs, &objp->rowData))
		return (FALSE);
	return (TRUE);
}
