/* Copyright 09/12/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)miexample.x	1.2 96/09/12 Sun Microsystems"

/*M*
//
//  RCS:
//      $Workfile:  client.  x $
//      $Revision:  2.0        $
//      $Modtime:   2/27/96    $
//      $Author:    DMTF       $
//
//  Purpose:
//
//      Describe the DMTF's Management Interface in an RPCGEN that is
//      suitable for building remote management using the ONC RPC
//      client/server model.  This file is compiled with the RPCGEN
//      compiler to produce the following files:
//
//                client.h          C-style interface header file 
//                client_c.c        Stub code for the managed system
//                client_s.c        Stub code for the managing application
//
//  Contents:
// 
//      The following information is described in version 2.0
//      of the Desktop Management Interface Specification.
// 
//  Data Structures:
//
//      DmiNodeAddress              Node address for indication originators
//
//  Indication Delivery:
//
//      DmiDeliverEvent             Deliver event data to an application
//      DmiComponentAdded           A component was added to the database
//      DmiComponentDeleted         A component was deleted from the database
//      DmiLanguageAdded            A component language mapping was added
//      DmiLanguageDeleted          A component language mapping was deleted
//      DmiGroupAdded               A group was added to a component
//      DmiGroupDeleted             A group was deleted from a component
//      DmiSubscriptionNotice       Information about an indication subscription

*M*/

/*# include "common.x" */



/*********************************************************************
 * DmiNodeAddress
 *********************************************************************/

/*D*
//  Name:       DmiNodeAddress
//  Purpose:    Addressing information for indication originators
//  Context:    Passed to indication delivery functions
//  Fields:     
//      address      Transport-dependent node address
//      rpc          Identifies the RPC (DCE, ONC, etc)
//      transport    Identifies the transport (TPC/IP, SPX, etc)
*D*/

struct DmiNodeAddress {
    DmiString_t*  address;
    DmiString_t*  rpc;
    DmiString_t*  transport;
};
typedef struct DmiNodeAddress DmiNodeAddress_t;



/*********************************************************************
 * DmiDeliverEvent
 *********************************************************************/

/*F*
//  Name:       DmiDeliverEvent
//  Purpose:    Deliver event data to an application
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle       An opaque ID returned to the application
//      sender       Address of the node delivering the indication
//      language     Language encoding for the indication data
//      compId       Component reporting the event
//      timestamp    Event generation time
//      rowData      Standard and context-specific indication data
*F*/

struct DmiDeliverEventIN {
    DmiUnsigned_t       handle;
    DmiNodeAddress_t*   sender;
    DmiString_t*        language;
    DmiId_t             compId;
    DmiTimestamp_t*     timestamp;
    DmiMultiRowData_t*  rowData;
};


/*********************************************************************
 * DmiComponentAdded
 *********************************************************************/

/*F*
//  Name:       DmiComponentAdded
//  Purpose:    A component was added to the database
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle    An opaque ID returned to the application
//      sender    Address of the node delivering the indication
//      info      Information about the component added
*F*/

struct DmiComponentAddedIN {
    DmiUnsigned_t        handle;
    DmiNodeAddress_t*    sender;
    DmiComponentInfo_t*  info;
};


/*********************************************************************
 * DmiComponentDeleted
 *********************************************************************/

/*F*
//  Name:       DmiComponentDeleted
//  Purpose:    A component was deleted from the database
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle    An opaque ID returned to the application
//      sender    Address of the node delivering the indication
//      compId    Component deleted from the database
*F*/

struct DmiComponentDeletedIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiId_t            compId;
};


/*********************************************************************
 * DmiLanguageAdded
 *********************************************************************/

/*F*
//  Name:       DmiLanguageAdded
//  Purpose:    A component language mapping was added
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle      An opaque ID returned to the application
//      sender      Address of the node delivering the indication
//      compId      Component with new language mapping
//      language    language-code|territory-code|encoding
*F*/

struct DmiLanguageAddedIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiId_t            compId;
    DmiString_t*       language;
};


/*********************************************************************
 * DmiLanguageDeleted
 *********************************************************************/

/*F*
//  Name:       DmiLanguageDeleted
//  Purpose:    A component language mapping was deleted
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle      An opaque ID returned to the application
//      sender      Address of the node delivering the indication
//      compId      Component with deleted language mapping
//      language    language-code|territory-code|encoding
*F*/

struct DmiLanguageDeletedIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiId_t            compId;
    DmiString_t*       language;
};


/*********************************************************************
 * DmiGroupAdded
 *********************************************************************/

/*F*
//  Name:       DmiGroupAdded
//  Purpose:    A group was added to a component
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle    An opaque ID returned to the application
//      sender    Address of the node delivering the indication
//      compId    Component with new group added
//      info      Information about the group added
*F*/

struct DmiGroupAddedIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiId_t            compId;
    DmiGroupInfo_t*    info;
};


/*********************************************************************
 * DmiGroupDeleted
 *********************************************************************/

/*F*
//  Name:       DmiGroupDeleted
//  Purpose:    A group was deleted from a component
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle     An opaque ID returned to the application
//      sender     Address of the node delivering the indication
//      compId     Component with the group deleted
//      groupId    Group deleted from the component
*F*/

struct DmiGroupDeletedIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiId_t            compId;
    DmiId_t            groupId; 
};


/*********************************************************************
 * DmiSubscriptionNotice
 *********************************************************************/

/*F*
//  Name:       DmiSubscriptionNotice
//  Purpose:    Information about an indication subscription
//  Context:    Indication Delivery
//  Returns:    
//  Parameters: 
//      handle     An opaque ID returned to the application
//      expired    True=expired; False=expiration pending
//      rowData    Row information to identify the subscription
*F*/

struct DmiSubscriptionNoticeIN {
    DmiUnsigned_t      handle;
    DmiNodeAddress_t*  sender;
    DmiBoolean_t       expired;
    DmiRowData_t       rowData;
};


program DMI2_CLIENT {
	version RMI2_CLIENT_VERSION {
		DmiErrorStatus_t _DmiDeliverEvent( DmiDeliverEventIN ) = 0x100;
		DmiErrorStatus_t _DmiComponentAdded( DmiComponentAddedIN ) = 0x101;
		DmiErrorStatus_t _DmiComponentDeleted( DmiComponentDeletedIN ) = 0x102;
		DmiErrorStatus_t _DmiLanguageAdded( DmiLanguageAddedIN ) = 0x103;
		DmiErrorStatus_t _DmiLanguageDeleted( DmiLanguageDeletedIN ) = 0x104;
		DmiErrorStatus_t _DmiGroupAdded( DmiGroupAddedIN ) = 0x105;
		DmiErrorStatus_t _DmiGroupDeleted( DmiGroupDeletedIN ) = 0x106;
		DmiErrorStatus_t _DmiSubscriptionNotice( DmiSubscriptionNoticeIN ) = 0x107;   
	} = 0x1;
} = 123456789;
