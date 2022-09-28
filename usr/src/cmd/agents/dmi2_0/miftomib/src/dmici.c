/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmici.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: dmici.c

    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (c) International Business Machines, Corp. 1994

    Description: Skeleton CI code for inclusion in DMIAPI.LIB

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    RCS Revision: $Header$

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/29/94  par    Creation date.

************************* INCLUDES ***********************************/

#include <string.h>
#include "dmiapi.h"

/************************ DEFINES ************************************/

#if defined(DOS_SL) || defined(WIN_SL)

#define memcpy _fmemcpy

#endif

/*********************************************************************/

/************************ PRIVATE ************************************/
/*
    The CiKeyListSource enumeration is used internally by DmiCiProcess() to
    direct the operation of the buildKeyList() procedure.  It is not used by
    the user-provided attribute access routines.
*/
enum CiKeyListSource {FromRequestBuffer, FromConfirmBuffer};

static DMI_UNSIGNED       DmiGetAttribute(DmiCiControl_t _FAR *dmiGetAttributeReq);
static DMI_UNSIGNED       DmiSetAttribute(DmiCiControl_t _FAR *dmiSetAttributeReq);
static DMI_UNSIGNED       DmiGetRow(DmiCiControl_t _FAR *dmiGetRow);
static DMI_UNSIGNED       DmiGetKey(DmiCiControl_t _FAR *dmiGetKey);
static DmiCiAttribute_t  _FAR *attributeFromKeyList(DMI_UNSIGNED attributeId,
                                                DMI_UNSIGNED keyCount,
                                                DmiCiAttribute_t _FAR *keyList);
static DMI_UNSIGNED       attributeValueSize(DmiCiAttribute_t _FAR *attribute);
static DmiCiAttribute_t  _FAR *buildKeyList(DmiCiControl_t _FAR *Command,enum CiKeyListSource);

/*********************************************************************/

DMI_UNSIGNED DMI_FUNC_ENTRY DmiCiProcess(DmiCiControl_t _FAR *CiCommandBlock, void _FAR *dmiCommand)
{
DMI_UNSIGNED status;

    status = SLERR_NO_ERROR;
    CiCommandBlock->ciCancelFlag = CiFalse;
/*
    The dmiCommand parameter is a pointer to the beginning of the request
    buffer.
*/
    CiCommandBlock->pRequestBuffer = dmiCommand;
/*
    A pointer to the confirm buffer can be found in the DmiMgmtCommand block
    in the request buffer.
*/
    CiCommandBlock->pConfirmBuffer = CiCommandBlock->pRequestBuffer -> dmiMgmtCommand.pCnfBuf;
/*
    A pointer to the DmiConfirm function can be found in the DmiCiCommand
    block, in the DmiMgmtCommand block, in the request buffer.
*/
    CiCommandBlock->DmiConfirmFunc = CiCommandBlock->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand.pConfirmFunc;

/*
    Based on the command code in the DmiMgmtCommand block in the request buffer,
    call the appropriate DMI handler.  The status returned is a status code
    defined in the error.h Service Layer module.
*/

        switch (CiCommandBlock->pRequestBuffer -> dmiMgmtCommand.iCommand) {
            case DmiGetAttributeCmd:
                status = DmiGetAttribute(CiCommandBlock);
                break;
            case DmiSetAttributeCmd:
            case DmiSetReserveAttributeCmd: /* FALL THROUGH */
            case DmiSetReleaseAttributeCmd: /* FALL THROUGH */
                status = DmiSetAttribute(CiCommandBlock);
                break;

/*
    For row commands, determine if the instrumentation code is being asked to
    build the key list in the confirm buffer, or fill in a table value in the
    confirm buffer.  If the key counts in the request and confirm buffers are
    not the same, then the instrumentation code needs to build a key list value.
*/
 
            case DmiGetFirstRowCmd:
            case DmiGetNextRowCmd: /* FALL THROUGH */
                if (CiCommandBlock->pRequestBuffer -> dmiGetRowReq.iGroupKeyCount >
                    CiCommandBlock->pConfirmBuffer -> dmiGetRowCnf.iGroupKeyCount) {
                    status = DmiGetKey(CiCommandBlock);
                    break;
                };
            case DmiGetRowCmd:     /* FALL THROUGH */
                    status = DmiGetRow(CiCommandBlock);
                break;
        }

/*
    If the command has not been canceled, inform the Service Layer that the
    instrumentation code has completed.
*/

    if (! CiCommandBlock->ciCancelFlag) {
        CiCommandBlock->dmiConfirm.iLevelCheck = DMI_LEVEL_CHECK;
        CiCommandBlock->dmiConfirm.pDmiMgmtCommand = (DMI_MgmtCommand_t _FAR *)CiCommandBlock->pRequestBuffer;
        CiCommandBlock->dmiConfirm.iStatus = status;
        (CiCommandBlock->DmiConfirmFunc)(&CiCommandBlock->dmiConfirm);
    }

/*
    Return to the Service Layer.
*/

/*    return status;  */

      return SLERR_NO_ERROR;   /* always return good status, let the callback handle any errors */
}

/*
    This procedure processes DmiGetAttribute commands.  It calls the user-
    written CiGetAttribute procedure, and copies the returned value into the
    confirm buffer.
*/

static DMI_UNSIGNED DmiGetAttribute(DmiCiControl_t _FAR *Command)
{
    struct DmiCiCommand         _FAR *dmiCiCommand;
    struct DmiGetAttributeData  _FAR *dmiGetAttributeData;
    DMI_UNSIGNED                    componentId;
    DMI_UNSIGNED                    groupId;
    DMI_UNSIGNED                    attributeId;
    DMI_UNSIGNED                    keyCount;
    DmiCiAttribute_t              _FAR  *keyList;
    DmiCiAttribute_t              _FAR  *attribute;
    DMI_UNSIGNED                    dataSize;
    void                      _FAR *dataPtr;
    struct DmiGetAttributeReq _FAR *dmiGetAttributeReq;
/*
    The DmiCiCommand block is pointed to by the DmiMgmtCommand block in the
    request buffer.
*/

    dmiGetAttributeReq = &(Command->pRequestBuffer -> dmiGetAttributeReq);
    dmiCiCommand = & Command->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand;

/*
    There can be many DmiGetAttributeData blocks within the request buffer.
    The current block, the command that the instrumentation needs to process
    at this time, is pointed to by the oCmdListEntry field in the DmiCiCommand
    block.  Since this is an offset, it is converted to a pointer by using the
    offset as an index into the request buffer.
*/

    dmiGetAttributeData = (struct DmiGetAttributeData _FAR *) &
        (Command->pRequestBuffer -> requestBuffer[dmiCiCommand -> oCmdListEntry]);

/*
    Build the needed information, prior to making the CiGetAttribute call.
*/

    componentId = dmiGetAttributeReq -> iComponentId;
    groupId = dmiGetAttributeData -> iGroupId;
    attributeId = dmiGetAttributeData -> iAttributeId;
    keyCount = dmiGetAttributeData -> iGroupKeyCount;
    keyList = buildKeyList(Command,FromRequestBuffer);

/*
    It is silly, but possible, that the Management Application is asking for
    the value of a key attribute with a keyed get attribute request.  For
    example, if there is a table of names indexed by an integer, the Management
    Application could ask, "What is the index of the line in the table whose
    index is 3?"  Obviously, the answer is 3.  The logic in the CiGetAttribute
    procedure could be time consuming.  If we do not have to call that
    procedure DONT!
*/

    if ((attribute = attributeFromKeyList(attributeId,
        keyCount, keyList)) == (DmiCiAttribute_t _FAR *) 0)
        attribute = (Command->CiGetAttribute)(componentId, groupId, attributeId,
                                   keyCount, keyList);
    
/*
    We now have an DmiCiAttribute_t object returned from the request buffer key list,
    or the user-written CiGetAttribute procedure.  Ensure that it is well-
    formed:  (1)  The size of the data object is not 0.  (2)  The attribute ID
    in the returned object matches the ID that was requested.  (3) The pointer
    to the value is non-NIL.
*/

    dataSize = attributeValueSize(attribute);
    if ((dataSize == 0) ||
        (attribute -> iAttributeId != attributeId) ||
        (attribute -> pAttributeValue == (void _FAR *) 0))
        return DBERR_ATTRIBUTE_NOT_FOUND;

/*
    Ensure that there is room in the confirm buffer for inserting the value.
    There are two items needed to be added to the confirm buffer:  a 
    DmiGetAttributeCnf block, and the value itself.  The iCnfBufLen in the 
    DmiCiCommand block gives the length of the confirm buffer beginning with
    the location of the DmiGetAttributeCnf block that is to be added.
*/

    if ((signed long) dmiCiCommand -> iCnfBufLen -
        sizeof(struct DmiGetAttributeCnf) - (signed long) dataSize < 0)
        return SLERR_BUFFER_FULL;

/*
    Get a pointer to the position within the confirm buffer where the data is
    to be written.  So as not to interfere with subsequent values that need to
    be inserted into the confirm buffer, values are copied to the very end of
    the buffer.  The pCnfBuf field in the DmiCiCommand block points to the
    location of the DmiGetAttributeCnf block that is to be added.
*/

    dataPtr = (char _FAR *) (dmiCiCommand -> pCnfBuf) +
        dmiCiCommand -> iCnfBufLen - dataSize;

/*
    Set the oAttributeValue field in the DmiGetAttributeCnf block with the
    offset of the data value.  The confirm buffer may have one or more
    DmiGetAttributeCnf blocks.  The current one is found by using the iCnfCount
    field in the DmiMgmtCommand block in the request buffer as an index into
    an array of DmiGetAttributeCnf blocks at the beginning of the confirm
    buffer.  To convert the data pointer to an offset, subtract the address of
    the beginning of the confirm buffer.
*/

    Command->pConfirmBuffer -> dmiGetAttributeCnf[
        dmiGetAttributeReq -> DmiMgmtCommand.iCnfCount].oAttributeValue =
        (char _FAR *) dataPtr - (Command->pConfirmBuffer -> confirmBuffer);

/*
    Copy the value into the confirm buffer.
*/

    memcpy(dataPtr, attribute -> pAttributeValue, (size_t) dataSize);

/*
    Return with a non-error status.
*/
    return SLERR_NO_ERROR;
}

/*
    This procedure processes DmiSetAttribute, DmiSetReserve, and DmiSetRelease
    commands.  It calls the user-written CiSetAttribute, CiReserveAttribute,
    and CiReleaseAttribute procedures.
*/

static DMI_UNSIGNED DmiSetAttribute(DmiCiControl_t _FAR *Command)
{
    struct DmiCiCommand         _FAR *dmiCiCommand;
    struct DmiSetAttributeData  _FAR *dmiSetAttributeData;
    DMI_UNSIGNED                    componentId;
    DMI_UNSIGNED                    groupId;
    DMI_UNSIGNED                    attributeId;
    DMI_UNSIGNED                    keyCount;
    DmiCiAttribute_t           _FAR *keyList;
    void                       _FAR *attributeValue;
    struct DmiSetAttributeReq  _FAR *dmiSetAttributeReq;
/*
    The DmiCiCommand block is pointed to by the DmiMgmtCommand block in the
    request buffer.
*/
    dmiSetAttributeReq = &(Command->pRequestBuffer -> dmiSetAttributeReq);
    dmiCiCommand = & Command->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand;

/*
    There can be many DmiSetAttributeData blocks within the request buffer.
    The current block, the command that the instrumentation needs to process
    at this time, is pointed to by the oCmdListEntry field in the DmiCiCommand
    block.  Since this is an offset, it is converted to a pointer by using the
    offset as an index into the request buffer.
*/

    dmiSetAttributeData = (struct DmiSetAttributeData  _FAR *) &
        (Command->pRequestBuffer -> requestBuffer[dmiCiCommand -> oCmdListEntry]);

/*
    Build the needed information, prior to making the CiSetAttribute call.
*/

    componentId = dmiSetAttributeReq -> iComponentId;
    groupId = dmiSetAttributeData -> iGroupId;
    attributeId = dmiSetAttributeData -> iAttributeId;
    keyCount = dmiSetAttributeData -> iGroupKeyCount;
    keyList = buildKeyList(Command,FromRequestBuffer);

/*
    The oAttributeValue field in the DmiSetAttribute block contains the offset
    of the attribute value.  It is converted to a pointer by using the offset
    as an index into the request buffer.
*/

    attributeValue = (void  _FAR *) & (Command->pRequestBuffer -> requestBuffer[
        dmiSetAttributeData -> oAttributeValue]);

/*
    The iCommand field in the DmiMgmtCommand block in the request buffer
    contains the actual DMI command to be performed.  Call the appropriate
    user-written instrumentation procedure.  If the procedure fails, return to
    the caller with an error status.  Otherwise, return a non-error status.
*/

    switch (dmiSetAttributeReq -> DmiMgmtCommand.iCommand) {
        case DmiSetAttributeCmd:
            if (! (Command->CiSetAttribute)(componentId, groupId, attributeId,
                                 keyCount, keyList, attributeValue))
                return DBERR_ROW_NOT_FOUND;
            break;
        case DmiSetReserveAttributeCmd:
            if (! (Command->CiReserveAttribute)(componentId, groupId, attributeId,
                                     keyCount, keyList, attributeValue))
                return DBERR_ATTRIBUTE_NOT_FOUND;
            break;
        case DmiSetReleaseAttributeCmd:
            if (! (Command->CiReleaseAttribute)(componentId, groupId, attributeId,
                                     keyCount, keyList, attributeValue))
                return DBERR_ATTRIBUTE_NOT_FOUND;
            break;
    }

    return SLERR_NO_ERROR;
}

/*
    This procedure processes DmiGetFirstRow, DmiGetNextRow, and DmiGetRow
    commands.  Due to the atomic nature of the Service Layer, only one value
    within the requested row of the table is needed.  Thus, this procedure
    will call the user-written CiGetAttribute procedure to get the value.
*/

static DMI_UNSIGNED DmiGetRow(DmiCiControl_t _FAR *Command)
{
    struct DmiGetAttributeCnf  _FAR *dmiGetAttributeCnf;
    DMI_UNSIGNED                   componentId;
    DMI_UNSIGNED                   groupId;
    DMI_UNSIGNED                   attributeId;
    DMI_UNSIGNED                   keyCount;
    DmiCiAttribute_t           _FAR *keyList;
    DmiCiAttribute_t           _FAR *attribute;
    struct DmiCiCommand        _FAR *dmiCiCommand;
    DMI_UNSIGNED                   dataSize;
    void                       _FAR *dataPtr;
    struct DmiGetRowReq        _FAR *dmiGetRowReq;
/*
    The DmiCiCommand block is pointed to by the DmiMgmtCommand block in the
    request buffer.
*/

    dmiGetRowReq = &(Command->pRequestBuffer->dmiGetRowReq);
    dmiCiCommand = & Command->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand;

/*
    The confirm buffer contains a DmiGetRowCnf block, followed by one or more
    DmiGetAttributeCnf blocks.  The pCnfBuf filed in the DmiCiCommand block
    points to the DmiGetAttributeCnf block associated with the attribute that
    is being currently requested.
*/

    dmiGetAttributeCnf = (struct DmiGetAttributeCnf _FAR *)
        (dmiCiCommand -> pCnfBuf);

/*
    Build the needed information, prior to making the CiSetAttribute call.
    Note that the attribute ID comes from the confirm buffer.  The Service
    Layer fills in the DmiGetAttributeCnf block in the confirm buffer prior
    to calling DmiCiInvoke. Note also, that the key list is being formed from
    the confirm buffer, not the request buffer.  Using this "trick", the
    logic below is simplified, since we only have to use the user-written
    CiGetAttribute procedure.
*/

    componentId = dmiGetRowReq -> iComponentId;
    groupId = dmiGetAttributeCnf -> iGroupId;
    attributeId = dmiGetAttributeCnf -> iAttributeId;
    keyCount = dmiGetRowReq -> iGroupKeyCount;
    keyList = buildKeyList(Command,FromConfirmBuffer);

/*
    It is silly, but possible, that the Management Application is asking for
    the value of a key attribute with a keyed get attribute request.  For
    example, if there is a table of names indexed by an integer, the Management
    Application could ask, "What is the index of the line in the table whose
    index is 3?"  Obviously, the answer is 3.  The logic in the CiGetAttribute
    procedure could be time consuming.  If we do not have to call that
    procedure DONT!
*/

    if ((attribute = attributeFromKeyList(attributeId, keyCount, keyList)) ==
        (DmiCiAttribute_t _FAR *) 0)
        attribute = (Command->CiGetAttribute)(componentId, groupId, attributeId,
                                   keyCount, keyList);

/*
    We now have an DmiCiAttribute_t object returned from the confirm buffer key list,
    or the user-written CiGetAttribute procedure.  Ensure that it is well-
    formed:  (1)  The size of the data object is not 0.  (2)  The attribute ID
    in the returned object matches the ID that was requested.  (3) The pointer
    to the value is non-NIL.
*/

    dataSize = attributeValueSize(attribute);
    if ((dataSize == 0) ||
        (attribute -> iAttributeId != attributeId) ||
        (attribute -> pAttributeValue == (void _FAR *) 0))
        return DBERR_ROW_NOT_FOUND;

/*
    Ensure that there is room in the confirm buffer for inserting the value.
    There are two items needed to be added to the confirm buffer:  a 
    DmiGetAttributeCnf block, and the value itself.  The iCnfBufLen in the 
    DmiCiCommand block gives the length of the confirm buffer beginning with
    the location of the DmiGetAttributeCnf block that is to be added.
*/

    if ((signed long) dmiCiCommand -> iCnfBufLen -
        sizeof(struct DmiGetAttributeCnf) - (signed long) dataSize < 0)
        return SLERR_BUFFER_FULL;

/*
    Get a pointer to the position within the confirm buffer where the data is
    to be written.  So as not to interfere with subsequent values that need to
    be inserted into the confirm buffer, values are copied to the very end of
    the buffer.  The pCnfBuf field in the DmiCiCommand block points to the
    location of the DmiGetAttributeCnf block that is to be added.
*/

    dataPtr = (char _FAR *) (dmiCiCommand -> pCnfBuf) +
        dmiCiCommand -> iCnfBufLen - dataSize;

/*
    Set the oAttributeValue field in the DmiGetAttributeCnf block with the
    offset of the data value.  To convert the data pointer to an offset,
    subtract the address of the beginning of the confirm buffer.
*/

    dmiGetAttributeCnf -> oAttributeValue = (char _FAR *) dataPtr -
        Command->pConfirmBuffer -> confirmBuffer;

/*
    Copy the value into the confirm buffer.
*/

    memcpy(dataPtr, attribute -> pAttributeValue, (size_t) dataSize);

/*
    Increment the iAttributeCount filed in the DmiGetRowCnf block in the confirm
    buffer.  This indicates to the Service Layer that the instrumentation code
    has successfully augmented the confirm buffer with a new DmiGetAttributeCnf
    block.
*/

    ++(Command->pConfirmBuffer -> dmiGetRowCnf.iAttributeCount);

/*
    Return with a non-error status.
*/

    return SLERR_NO_ERROR;
}

/*
    This procedure is called when the Service Layer needs the value of a key
    from the instrumentation code.  Depending on the command being processed,
    CiGetAttribute or CiGetNextAttribute will be called.  The value of that key
    will be copied into the key list in the confirm buffer.
*/

static DMI_UNSIGNED DmiGetKey(DmiCiControl_t _FAR *CommandBlock)
{
DMI_UNSIGNED                componentId;
DMI_UNSIGNED                groupId;
DMI_UNSIGNED                attributeId;
DMI_UNSIGNED                requestKeyCount;
DMI_UNSIGNED                confirmKeyCount;
struct DmiGroupKeyData  _FAR *confirmKeyList;
DmiCiAttribute_t        _FAR *ciKeyList;
DmiCiAttribute_t        _FAR *attribute = (DmiCiAttribute_t _FAR *)NULL;   
struct DmiCiCommand     _FAR *dmiCiCommand;
DMI_UNSIGNED                dataSize;
void                    _FAR *dataPtr;
struct DmiGetRowReq     _FAR *dmiGetRowReq;
/*
    Get the component and group Ids from the DmiGetRowReq block in the request
    buffer.
*/

    dmiGetRowReq = &(CommandBlock->pRequestBuffer -> dmiGetRowReq);
    componentId = dmiGetRowReq -> iComponentId;
    groupId = dmiGetRowReq -> iGroupId;

/*
    Get the length of the key list in the request buffer.
*/

    requestKeyCount = dmiGetRowReq -> iGroupKeyCount;

/*
    Get the length and pointer to the (partially completed) key list in the
    confirm buffer.
*/

    confirmKeyCount = CommandBlock->pConfirmBuffer -> dmiGetRowCnf.iGroupKeyCount;
    confirmKeyList = (struct DmiGroupKeyData _FAR *) &
        (CommandBlock->pConfirmBuffer -> confirmBuffer[
        CommandBlock->pConfirmBuffer -> dmiGetRowCnf.oGroupKeyList]);
/*
    The Service Layer has filled in the key list with all of the fields except
    the value offset.  Using the confirmKeyCount as an index into the confirm
    key list, we can extract the attribute ID that is being currently requested.
*/

    attributeId = confirmKeyList[confirmKeyCount].iAttributeId;

/*
    Using the iCommand field in the DmiMgmtCommand block in the request buffer,
    switch on the command to be processed.
*/

    switch (dmiGetRowReq -> DmiMgmtCommand.iCommand) {

/*
    If the command is a DmiGetFirst command, then an attribute value can be
    obtained from the CiGetAttribute procedure using NO KEY.  This is because
    in DMI terms the first row of a table is equivalent to getting a non-
    keyed attribute.
*/

        case DmiGetFirstRowCmd:
            ciKeyList = (DmiCiAttribute_t _FAR *) 0;
            attribute = (CommandBlock->CiGetAttribute)(componentId, groupId, attributeId,
                                       0, ciKeyList);
            break;
/*
    If the command is a DmiGetNext command, then we need the attribute in the
    NEXT row of the table.  Use the CiGetNextAttribute procedure.  Note that
    this is the only place where this procedure is called.
*/

        case DmiGetNextRowCmd:
            ciKeyList = buildKeyList(CommandBlock,FromRequestBuffer);
            attribute = (CommandBlock->CiGetNextAttribute)(componentId, groupId, attributeId,
                                           requestKeyCount, ciKeyList);
            break;
/*
    If the command is a DmiGetRowCmd, then we get the requested attribute from
    the request buffer itself.  This is possible, because we are being asked to
    provide a key value, and that value is already present in the key list in
    the request buffer.
*/

        case DmiGetRowCmd:
            ciKeyList = buildKeyList(CommandBlock,FromRequestBuffer);
            attribute = attributeFromKeyList(attributeId,
                                             requestKeyCount, ciKeyList);
            break;
    }

/*
    We now have an DmiCiAttribute_t object returned from the request buffer key list,
    or the user-written CiGetAttribute procedure.  Ensure that it is well-
    formed:  (1)  The size of the data object is not 0.  (2)  The attribute ID
    in the returned object matches the ID that was requested.  (3) The pointer
    to the value is non-NIL.
*/

    dataSize = attributeValueSize(attribute);
    if ((dataSize == 0) ||
        (attribute -> iAttributeId != attributeId) ||
        (attribute -> pAttributeValue == (void _FAR *) 0)){
        switch(dmiGetRowReq -> DmiMgmtCommand.iCommand){
            case DmiGetFirstRowCmd:
                return DBERR_ATTRIBUTE_NOT_FOUND;
            case DmiGetNextRowCmd:
                return DBERR_ROW_NOT_FOUND;
            case DmiGetRowCmd:
                return DBERR_ILLEGAL_KEYS;
        }
    }

/*
    The DmiCiCommand block is pointed to by the DmiMgmtCommand block in the
    request buffer.
*/

    dmiCiCommand = & CommandBlock->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand;

/*
    Ensure that there is room in the confirm buffer for inserting the value.
    There are two items needed to be added to the confirm buffer:  a 
    DmiGetAttributeCnf block, and the value itself.  The iCnfBufLen in the 
    DmiCiCommand block gives the length of the confirm buffer beginning with
    the location of the DmiGetAttributeCnf block that is to be added.
*/

    if ((signed long) dmiCiCommand -> iCnfBufLen -
        sizeof(struct DmiGetAttributeCnf) - (signed long) dataSize < 0)
        return SLERR_BUFFER_FULL;

/*
    Get a pointer to the position within the confirm buffer where the data is
    to be written.  So as not to interfere with subsequent values that need to
    be inserted into the confirm buffer, values are copied to the very end of
    the buffer.  The pCnfBuf field in the DmiCiCommand block points to the
    location of the DmiGetAttributeCnf block that is to be added.
*/

    dataPtr = (char _FAR *) (dmiCiCommand -> pCnfBuf) +
        dmiCiCommand -> iCnfBufLen - dataSize;
/*
    Update the oKeyValue field in the current DmiGroupKeyData block.  The data
    pointer is converted to an offset by subtracting from it the address of
    the beginning of the confirm buffer.
*/

    confirmKeyList[confirmKeyCount].oKeyValue = (char _FAR *) dataPtr -
        CommandBlock->pConfirmBuffer -> confirmBuffer;

/*
    Copy the data value to the confirm buffer.
*/

    memcpy(dataPtr, attribute -> pAttributeValue, (size_t) dataSize);

/*
    Increment the iGroupKeyCount filed in the DmiGetRowCnf block in the confirm
    buffer.  This indicates to the Service Layer that the instrumentation code
    has successfully augmented the confirm buffer with a new DmiGroupKeyData
    block.
*/

    ++ (CommandBlock->pConfirmBuffer -> dmiGetRowCnf.iGroupKeyCount);

/*
    Return with a non-error status.
*/

    return SLERR_NO_ERROR;
}

/*
    This utility procedure returns a pointer to a specific DmiCiAttribute_t object
    in a key list.  The requested attribute's ID is given, and if present,
    a pointer to that DmiCiAttribute_t structure will be returned.  Otherwise,
    the attribute is not in the key list, and a NIL pointer is returned.
*/

static DmiCiAttribute_t _FAR *attributeFromKeyList(DMI_UNSIGNED attributeId,
                                                DMI_UNSIGNED keyCount,
                                                DmiCiAttribute_t _FAR *keyList)
{
    DMI_UNSIGNED i;

    if (keyList == (DmiCiAttribute_t _FAR *) 0)
        return (DmiCiAttribute_t _FAR *) 0;

    for (i = 0; i < keyCount; ++i)
        if (keyList[i].iAttributeId == attributeId)
            return &keyList[i];

    return (DmiCiAttribute_t _FAR *) 0;
}

/*
    This utility procedure returns the size of the data object pointed to by
    a DmiCiAttribute_t object.
*/

static DMI_UNSIGNED attributeValueSize(DmiCiAttribute_t _FAR *attribute)
{
    DMI_STRING _FAR *string;

/*
    If a NIL DmiCiAttribute_t object is provided, return 0.
*/

    if (attribute == (DmiCiAttribute_t _FAR *) 0) return 0;

    switch (attribute -> iAttributeType) {
        case MIF_COUNTER:
        case MIF_GAUGE:         /* FALL THROUGH */
        case MIF_INTEGER:       /* FALL THROUGH */
            return 4;
        case MIF_COUNTER64:
        case MIF_INTEGER64:     /* FALL THROUGH */
            return 8; 
        case MIF_DATE:
            return 28;
/*
    The data object is a DMI string type.  The length of the data object, then
    is the length of the string (in bytes), plus the size of the length field
    that is at the head of the string.
*/

        case MIF_OCTETSTRING:
        case MIF_DISPLAYSTRING: /* FALL THROUGH */
            string = (DMI_STRING _FAR *) (attribute -> pAttributeValue);

/*
    If there is no string object (the pointer is NIL), then return 0.
*/

            if (string == (DMI_STRING _FAR *) 0)
                return 0;
            return sizeof(string -> length) + string -> length;
        default:
            return 0;
    }
}

/*
    This utility procedure builds a Ci-style key list from a Service Layer
    key list.  The primary difference is that the Ci-style key list is more
    C like, i.e. it is an arry of stuctures with pointers to the data values.
    The Service Layer key list is either in the request buffer or the confirm
    buffer, is at different offsets depending on command type, and contains
    offsets to the data values.

    This procedure takes a single parameter which directs it to build the
    key list from the request buffer or the confirm buffer.  Note that this
    parameter is only used for the DMI row commands.
*/

static DmiCiAttribute_t _FAR *buildKeyList(DmiCiControl_t _FAR *Command,enum CiKeyListSource keyListSource)
{
/*
    This is where the Ci-style key lists are built.  Note that the
    size of this array is fairly small since the actual values are not
    copied from the Service Layer request and confirm buffers.  The value
    pointers in the array of DmiCiAttribute_t objects point back into the buffers
    themselves.
*/

struct DmiCiCommand         _FAR *dmiCiCommand;
struct DmiGetAttributeData  _FAR *dmiGetAttributeData;
struct DmiSetAttributeData  _FAR *dmiSetAttributeData;
DMI_UNSIGNED                     keyCount = 0;
struct DmiGroupKeyData      _FAR *slKeyList = (struct DmiGroupKeyData _FAR *)NULL;  
DMI_UNSIGNED                     i;

/*
    The DmiCiCommand block is pointed to by the DmiMgmtCommand block in the
    request buffer.
*/

    dmiCiCommand = & Command->pRequestBuffer -> dmiMgmtCommand.DmiCiCommand;

/*
   Based on the iCommand field in the DmiMgmtCommand block in the request
   buffer, switch to the code to extract the length and pointer of the Service
   Layer key list.
*/

    switch (Command->pRequestBuffer -> dmiMgmtCommand.iCommand) {

/*
    For a DmiGetAttribute command the key list information is in the current
    DmiGetAttributeData block.  A pointer to that block is obtained, by using
    the oCmdListEntry field in the DmiCiCommand block in the request buffer.
*/

        case DmiGetAttributeCmd:
            dmiGetAttributeData = (struct DmiGetAttributeData _FAR *) &
                (Command->pRequestBuffer -> requestBuffer[
                dmiCiCommand -> oCmdListEntry]);
            keyCount = dmiGetAttributeData -> iGroupKeyCount;
            slKeyList = (struct DmiGroupKeyData _FAR *) &
                (Command->pRequestBuffer -> requestBuffer[
                dmiGetAttributeData -> oGroupKeyList]);
            break;

/*
    For the DMI set commands the key list information is in the current
    DmiSetAttributeData block.  A pointer to that block is obtained, by using
    the oCmdListEntry field in the DmiCiCommand block in the request buffer.
*/

        case DmiSetAttributeCmd:
        case DmiSetReserveAttributeCmd: /* FALL THROUGH */
        case DmiSetReleaseAttributeCmd: /* FALL THROUGH */
            dmiSetAttributeData = (struct DmiSetAttributeData _FAR *) &
                (Command->pRequestBuffer -> requestBuffer[
                dmiCiCommand -> oCmdListEntry]);
            keyCount = dmiSetAttributeData -> iGroupKeyCount;
            slKeyList = (struct DmiGroupKeyData _FAR *) &
                (Command->pRequestBuffer -> requestBuffer[
                dmiSetAttributeData -> oGroupKeyList]);
            break;
/*
    For the DMI row commands there are two key lists.  The incoming one in the
    request buffer, and the outgoing one in the confirm buffer.  When the
    instrumentation code is being asked to build the outgoing key list, the
    incoming key list is used to indicate the row where the new key value is
    to be taken.  Once the outgoing key list is buit however, requests for
    attribute values within that new row can use the outgoing key list.  This
    simplifies the instrumentation since DmiGetNextRow and DmiGetRow can use
    CiGetAttribute for the row values.
*/

        case DmiGetRowCmd:
        case DmiGetFirstRowCmd: /* FALL THROUGH */
        case DmiGetNextRowCmd:  /* FALL THROUGH */

/*
    If the key list is to be taken from the request buffer, then the length and
    pointer are in the DmiGetRowReq block.
*/

            if (keyListSource == FromRequestBuffer) {
                keyCount = Command->pRequestBuffer -> dmiGetRowReq.iGroupKeyCount;
                slKeyList = (struct DmiGroupKeyData _FAR *) &
                    (Command->pRequestBuffer -> requestBuffer[
                    Command->pRequestBuffer -> dmiGetRowReq.oGroupKeyList]);

/*
    If the key list is to be taken from the confirm buffer, then the length and
    pointer are in the DmiGetRowCnf block.
*/

            } else {
                keyCount = Command->pConfirmBuffer -> dmiGetRowCnf.iGroupKeyCount;
                slKeyList = (struct DmiGroupKeyData _FAR *) &
                (Command->pConfirmBuffer -> confirmBuffer[
                Command->pConfirmBuffer -> dmiGetRowCnf.oGroupKeyList]);
            }
            break;
    }

/*
    At this point, we have a pointer to a Service Layer key list and a count
    of the items in that list.  If the key count is 0, then there is no key
    list.
*/

    if (keyCount == 0)
        return (DmiCiAttribute_t _FAR *) 0;

/*
    Iterate through the key list from the Service Layer while building the
    DmiCiAttribute_t keylist.  Note that the ID and type fields are copied, but the
    data is not.  The offset to the data is converted into a pointer, and the
    pointer stored into the DmiCiAttribute_t object.
*/

    for (i = 0; i < keyCount; ++i) {
        Command->ciKeyList[i].iAttributeId = slKeyList[i].iAttributeId;
        Command->ciKeyList[i].iAttributeType = slKeyList[i].iAttributeType;
        if (keyListSource == FromRequestBuffer)
            Command->ciKeyList[i].pAttributeValue = (void _FAR *) &
                (Command->pRequestBuffer -> requestBuffer[slKeyList[i].oKeyValue]);
        else
            Command->ciKeyList[i].pAttributeValue = (void _FAR *) &
                (Command->pConfirmBuffer -> confirmBuffer[slKeyList[i].oKeyValue]);
    }

/*
    return the newly constructed Ci-style key list to the caller.
*/

    return Command->ciKeyList;
}

/*********************************************************************/
