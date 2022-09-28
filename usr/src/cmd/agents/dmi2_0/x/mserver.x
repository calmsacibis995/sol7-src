/*M*
//
//  RCS:
//      $Workfile:  server.x   $
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
//                server.h          C-style interface header file 
//                server_c.c        Stub code for the rmi client
//                server_s.c        Stub code for the rmi server
//
//  Contents:
// 
//      The following information is described in version 2.0
//      of the Desktop Management Interface Specification.
// 
//  Initialization:
//
//      DmiRegister                 Register a session with a remote system
//      DmiUnregister               Unregister a previously registered session
//      DmiGetVersion               Get DMI Service Provider version information
//      DmiGetConfig                Get session configuration parameters
//      DmiSetConfig                Set session configuration parameters
//
//  Discovery:
//
//      DmiListComponents           List component properties
//      DmiListComponentsByClass    List components matching certain criteria
//      DmiListLanguages            List a component's language strings
//      DmiListClassNames           List a component's class names and group ids
//      DmiListGroups               List group properties
//      DmiListAttributes           List attribute properties
//
//  Operation:
//
//      DmiAddRow                   Add a new row to a table
//      DmiDeleteRow                Delete a row from a table
//      DmiGetMultiple            Get a collection of attribute values
//      DmiSetMultiple            Set a collection of attribute values
//
//  Database Administration [optional]:
//
//      DmiAddComponent             Add a new component to the DMI database
//      DmiAddLanguage              Add a new language mapping for a component
//      DmiAddGroup                 Add a new group to a component
//      DmiDeleteComponent          Delete a component from the DMI database
//      DmiDeleteLanguage           Delete a language mapping for a component
//      DmiDeleteGroup              Delete a group from a component
*M*/




/*********************************************************************
 * DmiRegister
 *********************************************************************/

/*F*
//  Name:       DmiRegister
//  Purpose:    Register a session with a remote system
//  Context:    Initialization
//  Returns:    
//  Parameters: 
//      handle    On completion, an open session handle
//
//  Notes:      The client provides the address of the handle
//              parameter and the server fills it in.  All commands
//              except DmiRegister() require a valid handle, so
//              this must be the first command sent to the DMI server.
*F*/

struct DmiRegisterIN {
	DmiHandle_t   handle;
};

struct DmiRegisterOUT {
	DmiErrorStatus_t error_status;
    DmiHandle_t*  handle;
};

/*********************************************************************
 * DmiUnregister
 *********************************************************************/

/*F*
//  Name:       DmiUnregister
//  Purpose:    Unregister a previously registered session
//  Context:    Initialization
//  Returns:    
//  Parameters: 
//      handle    An open session handle to be closed
*F*/

struct DmiUnregisterOUT {
	DmiErrorStatus_t error_status;
};

struct DmiUnregisterIN {
    DmiHandle_t  handle; 
};

/*********************************************************************
 * DmiGetVersion
 *********************************************************************/

/*F*
//  Name:       DmiGetVersion
//  Purpose:    Get DMI Service Provider version information
//  Context:    Initialization
//  Returns:    
//  Parameters: 
//      handle           An open session handle
//      dmiSpecLevel     The DMI Specification version
//      description      The OS-specific Service Provider version
//      fileTypes        The file types supported for MIF installation
//
//  Notes:      1. The client must free the dmiSpecLevel string
//              2. The client must free the description string
*F*/

struct DmiGetVersionOUT {
	DmiErrorStatus_t error_status;
    DmiString_t*        dmiSpecLevel;
    DmiString_t*        description;
    DmiFileTypeList_t*  fileTypes;
};

struct DmiGetVersionIN {
    DmiHandle_t          handle;
};


/*********************************************************************
 * DmiGetConfig
 *********************************************************************/

/*F*
//  Name:       DmiGetConfig
//  Purpose:    Get session configuration parameters
//  Context:    Initialization
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      language    language-code|territory-code|encoding
//
//  Notes:      The client must free the language string
*F*/

struct DmiGetConfigOUT {
	DmiErrorStatus_t  error_status;
    DmiString_t*  language;
};

struct DmiGetConfigIN {
    DmiHandle_t    handle;
};
                                 

/*********************************************************************
 * DmiSetConfig
 *********************************************************************/

/*F*
//  Name:       DmiSetConfig
//  Purpose:    Set session configuration parameters
//  Context:    Initialization
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      language    language-code|territory-code|encoding
*F*/

struct DmiSetConfigOUT {
	DmiErrorStatus_t  error_status;  
};

struct DmiSetConfigIN {
    DmiHandle_t   handle;
    DmiString_t*  language;
};



/*********************************************************************
 * Dmilistcomponents
 *********************************************************************/

/*F*
//  Name:       DmiListComponents
//  Purpose:    List component properties
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle            An open session handle
//      requestMode       Unique, first, or next component ?
//      maxCount          Maximum number to return, or 0 for all
//      getPragma         Get optional pragma string ?
//      getDescription    Get optional component description ?
//      compId            Component to start with (see requestMode)
//      reply             List of components
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListComponentsOUT {
	DmiErrorStatus_t  error_status;
    DmiComponentList_t*  reply;
};

struct DmiListComponentsIN {
    DmiHandle_t           handle;
    DmiRequestMode_t      requestMode;
    DmiUnsigned_t         maxCount;
    DmiBoolean_t          getPragma ;
    DmiBoolean_t          getDescription;
    DmiId_t               compId;
};


/*********************************************************************
 * DmiListComponentsByClass
 *********************************************************************/

/*F*
//  Name:       DmiListComponentsByClass
//  Purpose:    List components matching certain criteria
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle            An open session handle
//      requestMode       Unique, first, or next component ?
//      maxCount          Maximum number to return, or 0 for all
//      getPragma         Get optional pragma string ?
//      getDescription    Get optional component description ?
//      compId            Component to start with (see requestMode)
//      className         Group class name string to match
//      keyList           Group row keys to match, or null
//      reply             List of components
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListComponentsByClassOUT {
	DmiErrorStatus_t  error_status;
    DmiComponentList_t*   reply;
};

struct DmiListComponentsByClassIN {
    DmiHandle_t            handle;
    DmiRequestMode_t       requestMode;
    DmiUnsigned_t          maxCount;
    DmiBoolean_t           getPragma;
    DmiBoolean_t           getDescription;
    DmiId_t                compId;
    DmiString_t*           className;
    DmiAttributeValues_t*  keyList;
};


/*********************************************************************
 * DmiListLanguages
 *********************************************************************/

/*F*
//  Name:       DmiListLanguages
//  Purpose:    List a component's language strings
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle         An open session handle
//      maxCount       Maximum number to return, or 0 for all
//      compId         Component to access
//      reply          List of language strings
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListLanguagesOUT {
	DmiErrorStatus_t  error_status;
    DmiStringList_t*  reply; 
};

struct DmiListLanguagesIN {
    DmiHandle_t        handle;
    DmiUnsigned_t      maxCount;
    DmiId_t            compId;
};


/*********************************************************************
 * DmiListClassNames
 *********************************************************************/

/*F*
//  Name:       DmiListClassNames
//  Purpose:    List a component's class names and group ids
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle         An open session handle
//      maxCount       Maximum number to return, or 0 for all
//      compId         Component to access
//      reply          List of class names and group ids
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListClassNamesOUT {
	DmiErrorStatus_t  error_status;
    DmiClassNameList_t*  reply;
};
  
struct DmiListClassNamesIN {
    DmiHandle_t           handle;
    DmiUnsigned_t         maxCount;
    DmiId_t               compId;
};


/*********************************************************************
 * DmiListGroups
 *********************************************************************/

/*F*
//  Name:       DmiListGroups
//  Purpose:    List group properties
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle            An open session handle
//      requestMode       Unique, first, or next group ?
//      maxCount          Maximum number to return, or 0 for all
//      getPragma         Get optional pragma string ?
//      getDescription    Get optional group description ?
//      compId            Component to access
//      groupId           Group to start with (see requestMode)
//      reply             List of groups
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListGroupsOUT {
	DmiErrorStatus_t  error_status;
    DmiGroupList_t*   reply;
};

struct DmiListGroupsIN {
    DmiHandle_t        handle;
    DmiRequestMode_t   requestMode;
    DmiUnsigned_t      maxCount;
    DmiBoolean_t       getPragma;
    DmiBoolean_t       getDescription;
    DmiId_t            compId;
    DmiId_t            groupId;
};


/*********************************************************************
 * DmiListAttributes
 *********************************************************************/

/*F*
//  Name:       DmiListAttributes
//  Purpose:    List attribute properties
//  Context:    Discovery
//  Returns:    
//  Parameters: 
//      handle            An open session handle
//      requestMode       Unique, first, or next attribute ?
//      maxCount          Maximum number to return, or 0 for all
//      getPragma         Get optional pragma string ?
//      getDescription    Get optional attribute description ?
//      compId            Component to access
//      groupId           Group to access
//      attribId          Attribute to start with (see requestMode)
//      reply             List of attributes
//
//  Notes:      The client must free the reply structure
*F*/

struct DmiListAttributesOUT {
	DmiErrorStatus_t  error_status;
    DmiAttributeList_t*  reply;
};

struct DmiListAttributesIN {
    DmiHandle_t           handle;
    DmiRequestMode_t      requestMode;
    DmiUnsigned_t         maxCount;
    DmiBoolean_t          getPragma;
    DmiBoolean_t          getDescription;
    DmiId_t               compId;
    DmiId_t               groupId;
    DmiId_t               attribId;
};



/*********************************************************************
 * DmiAddComponent
 *********************************************************************/

/*F*
//  Name:       DmiAddComponent
//  Purpose:    Add a new component to the DMI database
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      fileData    MIF file data for the component
//      compId      On completion, the SP-allocated component I
//      errors      Installation error messages
*F*/

struct DmiAddComponentOUT {
	DmiErrorStatus_t  error_status;  
    DmiId_t           compId;
    DmiStringList_t*   errors;
};

struct DmiAddComponentIN {
    DmiHandle_t         handle;
    DmiFileDataList_t*  fileData;
};


/*********************************************************************
 * DmiAddLanguage
 *********************************************************************/

/*F*
//  Name:       DmiAddLanguage 
//  Purpose:    Add a new language mapping for a component
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      fileData    Language mapping file for the component
//      compId      Component to access
//      errors      Installation error messages
*F*/

struct DmiAddLanguageOUT {
	DmiErrorStatus_t  error_status;
    DmiStringList_t*   errors;
};

struct DmiAddLanguageIN {
    DmiHandle_t         handle;
    DmiFileDataList_t*  fileData;
    DmiId_t             compId;
};


/*********************************************************************
 * DmiAddGroup
 *********************************************************************/

/*F*
//  Name:       DmiAddGroup 
//  Purpose:    Add a new group to a component
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      fileData    MIF file data for the group definition
//      compId      Component to access
//      groupId     On completion, the SP-allocated group ID
//      errors      Installation error messages
*F*/

struct DmiAddGroupOUT {
	DmiErrorStatus_t   error_status;
    DmiId_t            groupId;
    DmiStringList_t*   errors;
};

struct DmiAddGroupIN {
    DmiHandle_t         handle;
    DmiFileDataList_t*  fileData;
    DmiId_t             compId;
};



/*********************************************************************
 * DmiDeleteComponent
 *********************************************************************/

/*F*
//  Name:       DmiDeleteComponent
//  Purpose:    Delete a component from the DMI database
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle    An open session handle
//      compId    Component to delete
*F*/

struct DmiDeleteComponentOUT {
	DmiErrorStatus_t   error_status;
};

struct DmiDeleteComponentIN {
    DmiHandle_t  handle;
    DmiId_t      compId;
};


/*********************************************************************
 * DmiDeleteLanguage
 *********************************************************************/

/*F*
//  Name:       DmiDeleteLanguage 
//  Purpose:    Delete a language mapping for a component
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      language    language-code|territory-code|encoding
//      compId      Component to access
*F*/

struct DmiDeleteLanguageOUT {
	DmiErrorStatus_t  error_status;
};

struct DmiDeleteLanguageIN {
    DmiHandle_t   handle;
    DmiString_t*  language;
    DmiId_t       compId;
};


/*********************************************************************
 * DmiDeleteGroup
 *********************************************************************/

/*F*
//  Name:       DmiDeleteGroup 
//  Purpose:    Delete a group from a component
//  Context:    Database Administration
//  Returns:    
//  Parameters: 
//      handle     An open session handle
//      compId     Component containing group
//      groupId    Group to delete
*F*/

struct DmiDeleteGroupOUT {
	DmiErrorStatus_t   error_status;
};

struct DmiDeleteGroupIN {
    DmiHandle_t  handle;
    DmiId_t      compId;
    DmiId_t      groupId;
};



/*********************************************************************
 * DmiAddRow
 *********************************************************************/

/*F*
//  Name:       DmiAddRow
//  Purpose:    Add a new row to a table
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle    An open session handle
//      rowData   Attribute values to set
*F*/

struct DmiAddRowOUT {
	DmiErrorStatus_t  error_status;
};

struct DmiAddRowIN {
    DmiHandle_t    handle;
    DmiRowData_t*  rowData;
};


/*********************************************************************
 * DmiDeleteRow
 *********************************************************************/

/*F*
//  Name:       DmiDeleteRow 
//  Purpose:    Delete a row from a table
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle     An open session handle
//      rowData    Row { component, group, key } to delete
*F*/

struct DmiDeleteRowOUT {
	DmiErrorStatus_t error_status; 
};

struct DmiDeleteRowIN {
    DmiHandle_t    handle;
    DmiRowData_t*  rowData;
};


/*********************************************************************
 * DmiGetMultiple
 *********************************************************************/

/*F*
//  Name:       DmiGetMultiple
//  Purpose:    Get a collection of attribute values
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle     An open session handle
//      request    Attributes to get
//      rowData    Requested attribute values
// 
//  Notes:      1. The request may be for a SINGLE row (size = 1)
//              2. An empty id list for a row means "get all attributes"
//              3. The client must free the rowData structure
*F*/

struct DmiGetMultipleOUT {
	DmiErrorStatus_t  error_status; 
    DmiMultiRowData_t*    rowData; 
};
 
struct DmiGetMultipleIN {
    DmiHandle_t            handle;
    DmiMultiRowRequest_t*  request;
};


/*********************************************************************
 * DmiSetMultiple
 *********************************************************************/

/*F*
//  Name:       DmiSetMultiple
//  Purpose:    Set a collection of attributes
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle     An open session handle
//      setMode    Set, reserve, or release ?
//      rowData    Attribute values to set
*F*/

struct DmiSetMultipleOUT {
	DmiErrorStatus_t  error_status;
};

struct DmiSetMultipleIN {
    DmiHandle_t         handle;
    DmiSetMode_t        setMode;
    DmiMultiRowData_t*  rowData;
};

/*********************************************************************
 * DmiGetAttribute 
*********************************************************************/

/*F*
//  Name:       DmiGetAttribute
//  Purpose:    Get a single attribute value
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      compId      Component to access
//      groupId     Group within component
//      attribId    Attribute within group
//      keyList     Keylist to specify a table row    [optional]
//      value       Attribute value returned
*F*/

struct DmiGetAttributeOUT {
	DmiErrorStatus_t   error_status; 
    DmiDataUnion_t*    value; 
};
 
struct DmiGetAttributeIN {
    DmiHandle_t            handle;
    DmiId_t                compId;
	DmiId_t                groupId;
	DmiId_t                attribId;
	DmiAttributeValues_t*  keyList; 
};


/*********************************************************************
 * DmiSetAttribute
 *********************************************************************/

/*F*
//  Name:       DmiSetAttribute
//  Purpose:    Set a single attribute value
//  Context:    Operation
//  Returns:    
//  Parameters: 
//      handle      An open session handle
//      compId      Component to access
//      groupId     Group within component
//      attribId    Attribute within group
//      keyList     Keylist to specify a table row    [optional]
//      setMode     Set, reserve, or release ?
//      value       Attribute value to set
*F*/
struct DmiSetAttributeOUT {
	DmiErrorStatus_t   error_status; 
};

struct DmiSetAttributeIN {
	DmiHandle_t            handle;
	DmiId_t                compId;
	DmiId_t                groupId;
	DmiId_t                attribId;
    DmiAttributeValues_t*  keyList;
	DmiSetMode_t           setMode;
	DmiDataUnion_t*        value;
}; 



program DMI2_SERVER {
	version DMI2_SERVER_VERSION {
		DmiRegisterOUT _DmiRegister ( DmiRegisterIN ) = 0x200;
		DmiUnregisterOUT _DmiUnregister ( DmiUnregisterIN ) = 0x201;
		DmiGetVersionOUT _DmiGetVersion ( DmiGetVersionIN ) = 0x202;
		DmiGetConfigOUT _DmiGetConfig ( DmiGetConfigIN ) = 0x203;
		DmiSetConfigOUT _DmiSetConfig ( DmiSetConfigIN ) = 0x204;
		DmiListComponentsOUT _DmiListComponents ( DmiListComponentsIN ) = 0x205;
		DmiListComponentsByClassOUT _DmiListComponentsByClass ( DmiListComponentsByClassIN ) = 0x206;
		DmiListLanguagesOUT _DmiListLanguages ( DmiListLanguagesIN ) = 0x207;
		DmiListClassNamesOUT _DmiListClassNames ( DmiListClassNamesIN ) = 0x208;
		DmiListGroupsOUT _DmiListGroups ( DmiListGroupsIN ) = 0x209;
		DmiListAttributesOUT _DmiListAttributes ( DmiListAttributesIN ) = 0x20a;
		DmiAddRowOUT _DmiAddRow ( DmiAddRowIN ) = 0x20b;
		DmiDeleteRowOUT _DmiDeleteRow ( DmiDeleteRowIN ) = 0x20c;
		DmiGetMultipleOUT _DmiGetMultiple ( DmiGetMultipleIN ) = 0x20d;
		DmiSetMultipleOUT _DmiSetMultiple ( DmiSetMultipleIN ) = 0x20e;
		DmiAddComponentOUT _DmiAddComponent ( DmiAddComponentIN ) = 0x20f;
		DmiAddLanguageOUT _DmiAddLanguage ( DmiAddLanguageIN ) = 0x210;
		DmiAddGroupOUT _DmiAddGroup ( DmiAddGroupIN ) = 0x211;
		DmiDeleteComponentOUT _DmiDeleteComponent ( DmiDeleteComponentIN ) = 0x212;
		DmiDeleteLanguageOUT _DmiDeleteLanguage ( DmiDeleteLanguageIN ) = 0x213;
		DmiDeleteGroupOUT _DmiDeleteGroup ( DmiDeleteGroupIN ) = 0x214;
		DmiGetAttributeOUT _DmiGetAttribute ( DmiGetAttributeIN ) = 0x215;
		DmiSetAttributeOUT _DmiSetAttribute ( DmiSetAttributeIN ) = 0x216;
	} = 0x1;
} = 300598;
