/*M*
//
//  RCS:
//      $Workfile:  common.x   $
//      $Revision:  2.0        $
//      $Modtime:   2/27/96    $
//      $Author:    DMTF       $
//
//  Purpose:
//
//      Describe data structures and types for the DMTF's Management
//      Interface in an RPCGEN that is suitable for building remote
//      management using the ONC RPC client/server model.  This
//      file is included in the client.x and server.x files.
//
//  Contents:
// 
//      The following information is described in version 2.0
//      of the Desktop Management Interface Specification.
// 
//  Enumerated Types:
//
//      DmiSetMode                  Define set operations
//      DmiRequestMode              Define sequential access modes
//      DmiStorageType              Define the storage type for an attribute
//      DmiAccessMode               Define the access modes for an attribute
//      DmiDataType                 Define the data types referenced by DmiDataUnion
//      DmiFileDataInfo             Define the DMI mapping file types
//
//  Data Structures:
//
//      DmiTimestamp                Describes the DMI timestamp structure
//      DmiString                   Describes the DMI string representation
//      DmiOctetString              Describes the DMI octet string representation
//      DmiDataUnion                Discriminated union of DMI data types
//      DmiEnumInfo                 Associates an integer value with descriptive text
//      DmiAttributeInfo            Holds information about an attribute
//      DmiAttributeData            Describes an attribute id, type, and value
//      DmiGroupInfo                Holds information about a group
//      DmiComponentInfo            Holds information about a component
//      DmiFileDataInfo             Holds language file type and mapping data
//      DmiClassNameInfo            Holds a group's id and class string
//      DmiRowRequest               Identifies { component, group, row, ids } to get
//      DmiRowData                  Identifies { component, group, row, values } to set
//
//      DmiAttributeIds             Describes a conformant array of DmiId
//      DmiAttributeValues          Describes a conformant array of DmiAttributeData
//      DmiEnumList                 Describes a conformant array of DmiEnumInfo
//      DmiAttributeList            Describes a conformant array of DmiAttributeInfo
//      DmiGroupList                Describes a conformant array of DmiGroupInfo
//      DmiComponentList            Describes a conformant array of DmiComponentInfo
//      DmiFileDataList             Describes a conformant array of DmiFileDataInfo
//      DmiClassNameList            Describes a conformant array of DmiClassNameInfo
//      DmiStringList               Describes a conformant array of DmiString
//      DmiFileTypeList             Describes a conformant array of DmiFileType
//      DmiMultiRowRequest          Describes a conformant array of DmiRowRequest
//      DmiMultiRowData             Describes a conformant array of DmiRowData
*M*/

# ifndef DMI_API
# define DMI_API
# endif



/*********************************************************************
 * DmiSetMode
 *********************************************************************/

/*D*
//  Name:       DmiSetMode
//  Purpose:    Define set operations
//  Context:    DmiSetAttributes()
//  Fields:
//      DMI_SET        Set data values
//      DMI_RESERVE    Reserve resources for a set operation
//      DMI_RELEASE    Release previously reserved resources
*D*/

enum DmiSetMode {
    DMI_SET,
    DMI_RESERVE,
    DMI_RELEASE
};
typedef enum DmiSetMode DmiSetMode_t;

/*********************************************************************
 * DmiRequestMode
 *********************************************************************/

/*D*
//  Name:       DmiRequestMode
//  Purpose:    Define sequential access modes
//  Context:    Field in DmiRowRequest,
//  Context:    DmiListComponents(), DmiListComponentsByClass(),
//  Context:    DmiListGroups(), DmiListAttributes(),
//  Fields:
//      DMI_UNIQUE    Access the specified item (or table row)
//      DMI_FIRST     Access the first item
//      DMI_NEXT      Access the next item
*D*/

enum DmiRequestMode {
    DMI_UNIQUE,
    DMI_FIRST,
    DMI_NEXT
};
typedef enum DmiRequestMode DmiRequestMode_t;


/*********************************************************************
 * DmiStorageType
 *********************************************************************/

/*D*
//  Name:       DmiStorageType
//  Purpose:    Define the storage type for an attribute
//  Context:    Field in DmiAttributeInfo
//  Fields:
//      MIF_COMMON      Value is from a small set of possibilities
//      MIF_SPECIFIC    Value is from a large set of possibilities
*D*/

enum DmiStorageType {
    MIF_COMMON,
    MIF_SPECIFIC
};
typedef enum DmiStorageType DmiStorageType_t;


/*********************************************************************
 * DmiAccessMode
 *********************************************************************/

/*D*
//  Name:       DmiAccessMode
//  Purpose:    Define the access modes for an attribute
//  Context:    Field in DmiAttributeInfo
//  Fields:     
//      MIF_UNKNOWN        Unknown access mode
//      MIF_READ_ONLY      Read access only
//      MIF_READ_WRITE     Readable and writable
//      MIF_WRITE_ONLY     Write access only
//      MIF_UNSUPPORTED    Attribute is not supported
*D*/

enum DmiAccessMode {
    MIF_UNKNOWN_ACCESS,
    MIF_READ_ONLY,
    MIF_READ_WRITE,
    MIF_WRITE_ONLY,
    MIF_UNSUPPORTED
};
typedef enum DmiAccessMode DmiAccessMode_t;


/*********************************************************************
 * DmiDataType
 *********************************************************************/

/*D*
//  Name:       DmiDataType
//  Purpose:    Define the data types referenced by DmiDataUnion
//  Context:    
//  Fields:     
//      MIF_DATATYPE_0       RESERVED
//      MIF_COUNTER          32-bit unsigned integer that never decreases
//      MIF_COUNTER64        64-bit unsigned integer that never decreases
//      MIF_GAUGE            32-bit unsigned integer may increase or decrease
//      MIF_DATATYPE_4       RESERVED
//      MIF_INTEGER          32-bit signed integer; no semantics known
//      MIF_INTEGER64        64-bit signed integer; no semantics known
//      MIF_OCTETSTRING      String of n octets, not necessarily displayable
//      MIF_DISPLAYSTRING    Displayable string of n octets
//      MIF_DATATYPE_9       RESERVED
//      MIF_DATATYPE_10      RESERVED
//      MIF_DATE             28-octet displayable string (yyyymmddHHMMSS.uuuuuu+ooo)
*D*/

enum DmiDataType {
    MIF_DATATYPE_0,
    MIF_COUNTER,
    MIF_COUNTER64,
    MIF_GAUGE,
    MIF_DATATYPE_4,
    MIF_INTEGER,
    MIF_INTEGER64,
    MIF_OCTETSTRING,
    MIF_DISPLAYSTRING,
    MIF_DATATYPE_9,
    MIF_DATATYPE_10,
    MIF_DATE
};
typedef enum DmiDataType DmiDataType_t;

/*
 * Aliases for the standard data types
 */

# define MIF_INT     MIF_INTEGER
# define MIF_INT64   MIF_INTEGER64
# define MIF_STRING  MIF_DISPLAYSTRING


/*********************************************************************
 * DmiFileType
 *********************************************************************/

/*D*
//  Name:       DmiFileType
//  Purpose:    Define the DMI mapping file types
//  Context:    Field in DmiFileDataInfo
//  Fields:     
//      DMI_FILETYPE_0            RESERVED
//      DMI_FILETYPE_1            RESERVED
//      DMI_MIF_FILE_NAME         File data is DMI MIF file name
//      DMI_MIF_FILE_DATA         File data is DMI MIF data
//      SNMP_MAPPING_FILE_NAME    File data is SNMP MAPPING file name
//      SNMP_MAPPING_FILE_DATA    File data is SNMP MAPPING data
//      DMI_GROUP_FILE_NAME       File data is DMI GROUP MIF file name
//      DMI_GROUP_FILE_DATA       File data is DMI GROUP MIF data
//      MS_FILE_NAME              File data is Microsoft-format file name
//      MS_FILE_DATA              File data is Microsoft-format data
*D*/

enum DmiFileType {
    DMI_FILETYPE_0,
    DMI_FILETYPE_1,
    DMI_MIF_FILE_NAME,
    DMI_MIF_FILE_DATA,
    SNMP_MAPPING_FILE_NAME,
    SNMP_MAPPING_FILE_DATA,
    DMI_GROUP_FILE_NAME,
    DMI_GROUP_FILE_DATA,
    MS_FILE_NAME,
    MS_FILE_DATA
};
typedef enum DmiFileType DmiFileType_t;



/*********************************************************************
 * DMI Data Types
 *********************************************************************/

typedef unsigned long   DmiId_t;
typedef unsigned long   DmiHandle_t;
typedef unsigned long   DmiCounter_t;
typedef unsigned long   DmiErrorStatus_t;
typedef unsigned long   DmiCounter64_t[2];
typedef unsigned long   DmiGauge_t;
typedef unsigned long   DmiUnsigned_t;
typedef long            DmiInteger_t;
typedef unsigned long   DmiInteger64_t[2];
typedef unsigned long   DmiBoolean_t;


/*********************************************************************
 * DmiTimestamp
 *********************************************************************/

/*D*
//  Name:       DmiTimestamp
//  Purpose:    Describes the DMI timestamp structure
//  Context:    Field in DmiDataUnion
//  Fields:     
//      year            The year ('1996')
//      month           The month ('1'..'12')
//      yay             The day of the month ('1'..'23')
//      hour            The hour ('0'..'23')
//      minutes         The minutes ('0'..'59')
//      seconds         The seconds ('0'..'60'); includes leap seconds
//      dot             A dot ('.')
//      microSeconds    Microseconds ('0'..'999999')
//      plusOrMinus     '+' for east, or '-' west of UTC
//      utcOffset       Minutes ('0'..'720') from UTC
//      padding         Unused padding for 4-byte alignment
*D*/

struct DmiTimestamp {
    char  year          [4];
    char  month         [2];
    char  day           [2];
    char  hour          [2];
    char  minutes       [2];
    char  seconds       [2];
    char  dot;
    char  microSeconds  [6];
    char  plusOrMinus;
    char  utcOffset     [3];
    char  padding       [3];
};
typedef struct DmiTimestamp DmiTimestamp_t;


/*********************************************************************
 * DmiString
 *********************************************************************/

/*D*
//  Name:       DmiString
//  Purpose:    Describes the DMI string representation
//  Context:    Field in DmiDataUnion
//  Fields:     
//      size    Number of octets in the string body
//      body    String contents
//
//  Notes:      For displaystrings, the string is null terminated,
//              and the null character is included in the size.
*D*/

struct DmiString {
    char  body<>;
};
typedef struct DmiString DmiString_t;
typedef DmiString_t* DmiStringPtr_t;


/*********************************************************************
 * DmiOctetString
 *********************************************************************/

/*D*
//  Name:       DmiOctetString
//  Purpose:    Describes the DMI octet string representation
//  Context:    Field in DmiDataUnion
//  Fields:     
//      size    Number of octets in the string body
//      body    String contents
*D*/

struct DmiOctetString {
    char  body<>;
};
typedef struct DmiOctetString DmiOctetString_t;


/*********************************************************************
 * DmiDataUnion
 *********************************************************************/

/*D*
//  Name:       DmiDataUnion
//  Purpose:    Discriminated union of DMI data types
//  Context:    Field in DmiAttributeData
//  Fields:     
//      type    Discriminator for the union
//      value   Union of DMI attribute data types 
*D*/

union DmiDataUnion switch (DmiDataType_t type) {
    case MIF_COUNTER:       DmiCounter_t       counter;
    case MIF_COUNTER64:     DmiCounter64_t     counter64;
    case MIF_GAUGE:         DmiGauge_t         gauge;
    case MIF_INTEGER:       DmiInteger_t       integer;
    case MIF_INTEGER64:     DmiInteger64_t     integer64;
    case MIF_OCTETSTRING:   DmiOctetString_t*  octetstring;
    case MIF_DISPLAYSTRING: DmiString_t*       str;
    case MIF_DATE:          DmiTimestamp_t*    date;
};
typedef union DmiDataUnion DmiDataUnion_t;



/*********************************************************************
 * DmiEnumInfo
 *********************************************************************/

/*D*
//  Name:       DmiEnumInfo
//  Purpose:    Associates an integer value with descriptive text
//  Context:    Element in DmiEnumList
//  Fields:     
//      name    Enumeration name 
//      value   Enumeration value
*D*/

struct DmiEnumInfo {
    DmiString_t*    name;
    DmiInteger_t    value;
};
typedef struct DmiEnumInfo DmiEnumInfo_t;


/*********************************************************************
 * DmiEnumList
 *********************************************************************/

/*D*
//  Name:       DmiEnumList
//  Purpose:    Describes a conformant array of DmiEnumInfo
//  Context:    DmiEnumAttributes()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiEnumList {
    DmiEnumInfo_t  list<>;
};
typedef struct DmiEnumList DmiEnumList_t;



/*********************************************************************
 * DmiAttributeInfo
 *********************************************************************/

/*D*
//  Name:       DmiAttributeInfo
//  Purpose:    Holds information about an attribute
//  Context:    Element in DmiAttributeList
//  Fields:     
//      id             Attribute ID
//      name           Attribute name string
//      pragma         Attribute pragma string            [optional]
//      description    Attribute description string       [optional]
//      storage        Common or specific storage
//      access         Readonly, read-write, etc
//      type           Counter, integer, etc
//      maxSize        Maximum length of the attribute
//      enumList       EnumList for enumerated types      [optional]
*D*/

struct DmiAttributeInfo {
    DmiId_t                id;
    DmiString_t*           name;
    DmiString_t*           pragma;
    DmiString_t*           description;
    DmiStorageType_t       storage;
    DmiAccessMode_t        access;
    DmiDataType_t          type;
    DmiUnsigned_t          maxSize;
    DmiEnumList_t*    enumList;
};
typedef struct DmiAttributeInfo DmiAttributeInfo_t;


/*********************************************************************
 * DmiAttributeData
 *********************************************************************/

/*D*
//  Name:       DmiAttributeData
//  Purpose:    Describes an attribute id, type, and value
//  Context:    Element in DmiAttributeValues
//  Fields:     
//      id      Attribute ID
//      data    Attribute type and value
*D*/

struct DmiAttributeData {
    DmiId_t           id;
    DmiDataUnion_t    data;
};
typedef struct DmiAttributeData DmiAttributeData_t;

/*********************************************************************
 * DmiAttributeIds
 *********************************************************************/

/*D*
//  Name:       DmiAttributeIds
//  Purpose:    Describes a conformant array of DmiId
//  Context:    Field in DmiRowRequest
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiAttributeIds {
    DmiId_t  list<>;
};
typedef struct DmiAttributeIds DmiAttributeIds_t;


/*********************************************************************
 * DmiGroupInfo
 *********************************************************************/

/*D*
//  Name:       DmiGroupInfo
//  Purpose:    Holds information about a group
//  Context:    Element in DmiGroupList
//  Fields:     
//      id             Group ID
//      name           Group name string
//      pragma         Group pragma string                 [optional]
//      className      Group class name string
//      description    Group description string            [optional]
//      keyList        Attribute IDs for table row keys    [optional]
*D*/

struct DmiGroupInfo {
    DmiId_t                    id;
    DmiString_t*               name;
    DmiString_t*               pragma;
    DmiString_t*               className;
    DmiString_t*               description;
	DmiAttributeIds_t*    keyList;
};
typedef struct DmiGroupInfo DmiGroupInfo_t;


/*********************************************************************
 * DmiComponentInfo
 *********************************************************************/

/*D*
//  Name:       DmiComponentInfo
//  Purpose:    Holds information about a component
//  Context:    Element in DmiComponentList
//  Fields:     
//      id             Component ID
//      name           Component name string
//      pragma         Component pragma string         [optional]
//      description    Component description string    [optional]
//      exactMatch 
//         idl_true  = Exact match
//         idl_false = Possible match
*D*/

struct DmiComponentInfo {
    DmiId_t         id;
    DmiString_t*    name;
    DmiString_t*    pragma;
    DmiString_t*    description;
    DmiBoolean_t    exactMatch;
};
typedef struct DmiComponentInfo DmiComponentInfo_t;


/*********************************************************************
 * DmiFileDataInfo
 *********************************************************************/

/*D*
//  Name:       DmiFileDataInfo
//  Purpose:    Holds language file type and mapping data
//  Context:    Element in DmiFileDataList
//  Fields:     
//      fileType    MIF file, SNMP mapping file, etc
//      fileData    The file info (name -or- contents)
*D*/

struct DmiFileDataInfo {
    DmiFileType_t        fileType;
    DmiOctetString_t*    fileData;
};
typedef struct DmiFileDataInfo DmiFileDataInfo_t;


/*********************************************************************
 * DmiClassNameInfo
 *********************************************************************/

/*D*
//  Name:       DmiClassNameInfo
//  Purpose:    Holds a group's id and class string
//  Context:    Element in DmiClassNameList
//  Fields:     
//      id           Group ID
//      className    Group class name string
*D*/

struct DmiClassNameInfo {
    DmiId_t       id;
    DmiString_t*  className;
};
typedef struct DmiClassNameInfo DmiClassNameInfo_t;

/*********************************************************************
 * DmiAttributeValues
 *********************************************************************/

/*D*
//  Name:       DmiAttributeValues
//  Purpose:    Describes a conformant array of DmiAttributeData
//  Context:    Field in DmiRowRequest, DmiRowData
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiAttributeValues {
    DmiAttributeData_t  list<>;
};
typedef struct DmiAttributeValues DmiAttributeValues_t;



/*********************************************************************
 * DmiRowRequest
 *********************************************************************/

/*D*
//  Name:       DmiRowRequest
//  Purpose:    Identifies { component, group, row, ids } to get
//  Context:    Element in DmiMultiRowRequest
//  Fields:     
//      compId         Component ID
//      groupId        Group ID
//      requestMode    Get from specified row, first row, or next row
//      keyList        Array of values for key attributes
//      ids            Array of IDs for data attributes
*D*/

struct DmiRowRequest {
    DmiId_t                       compId;
    DmiId_t                       groupId;
    DmiRequestMode_t              requestMode;
    DmiAttributeValues_t*    keyList;
    DmiAttributeIds_t*       ids;
};
typedef struct DmiRowRequest DmiRowRequest_t;


/*********************************************************************
 * DmiRowData
 *********************************************************************/

/*D*
//  Name:       DmiRowData
//  Purpose:    Identifies { component, group, row, values } to set
//  Context:    Element in DmiMultiRowData
//  Fields:     
//      compId       Component ID
//      groupId      Group ID
//      className    Group class name for events, or 0    [optional]
//      keyList      Array of values for key attributes
//      values       Array of values for data attributes
//
//  Notes:      This structure is used for setting attributes, getting
//              attributes, and for providing indication data.  The
//              className string is only required when returning
//              indication data.  For other uses, the field can be 0.
*D*/

struct DmiRowData {
    DmiId_t                       compId;
    DmiId_t                       groupId;
    DmiString_t*                  className;
    DmiAttributeValues_t*    keyList;
    DmiAttributeValues_t*    values;
};
typedef struct DmiRowData DmiRowData_t;





/*********************************************************************
 * DmiAttributeList
 *********************************************************************/

/*D* 
//  Name:       DmiAttributeList
//  Purpose:    Describes a conformant array of DmiAttributeInfo
//  Context:    DmiListAttributes()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiAttributeList {

    DmiAttributeInfo_t  list<>;
};
typedef struct DmiAttributeList DmiAttributeList_t;


/*********************************************************************
 * DmiGroupList
 *********************************************************************/

/*D*
//  Name:       DmiGroupList
//  Purpose:    Describes a conformant array of DmiGroupInfo
//  Context:    DmiListGroups()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiGroupList {
    DmiGroupInfo_t  list<>;
};
typedef struct DmiGroupList DmiGroupList_t;


/*********************************************************************
 * DmiComponent
 *********************************************************************/

/*D*
//  Name:       DmiComponentList
//  Purpose:    Describes a conformant array of DmiComponentInfo
//  Context:    DmiListComponents(), DmiListComponentsByClass()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiComponentList {
    DmiComponentInfo_t  list<>;
};
typedef struct DmiComponentList DmiComponentList_t;


/*********************************************************************
 * DmiFileDataList
 *********************************************************************/

/*D*
//  Name:       DmiFileDataList
//  Purpose:    Describes a conformant array of DmiFileDataInfo
//  Context:    DmiAddComponent(), DmiAddLanguage(), DmiAddGroup()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiFileDataList {
    DmiFileDataInfo_t  list<>;
};
typedef struct DmiFileDataList DmiFileDataList_t;


/*********************************************************************
 * DmiClassNameList
 *********************************************************************/

/*D*
//  Name:       DmiClassNameList
//  Purpose:    Describes a conformant array of DmiClassNameInfo
//  Context:    DmiListClassNames()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiClassNameList {
    DmiClassNameInfo_t  list<>;
};
typedef struct DmiClassNameList DmiClassNameList_t;


/*********************************************************************
 * DmiStringList
 *********************************************************************/

/*D*
//  Name:       DmiStringList
//  Purpose:    Describes a conformant array of DmiStrings
//  Context:    DmiListLanguages()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiStringList {
    DmiStringPtr_t  list<>;
};
typedef struct DmiStringList DmiStringList_t;


/*********************************************************************
 * DmiFileTypeList
 *********************************************************************/

/*D*
//  Name:       DmiFileTypeList
//  Purpose:    Describes a conformant array of DmiFileType entries
//  Context:    DmiGetVersion()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/


struct DmiFileTypeList {
    DmiFileType_t  list<>;
};
typedef struct DmiFileTypeList DmiFileTypeList_t;


/*********************************************************************
 * DmiMultiRowRequest
 *********************************************************************/

/*D*
//  Name:       DmiMultiRowRequest
//  Purpose:    Describes a conformant array of DmiRowRequest
//  Context:    DmiGetAttributes()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiMultiRowRequest {
    DmiRowRequest_t  list<>;
};
typedef struct DmiMultiRowRequest DmiMultiRowRequest_t;


/*********************************************************************
 * DmiMultiRowData
 *********************************************************************/

/*D*
//  Name:       DmiMultiRowData
//  Purpose:    Describes a conformant array of DmiRowData
//  Context:    DmiGetAttributes(), DmiSetAttributes()
//  Fields:     
//      size    Array elements
//      list    Array data
*D*/

struct DmiMultiRowData {
    DmiRowData_t  list<>;
};
typedef struct DmiMultiRowData DmiMultiRowData_t;




/* the following is the add-on for component interface rpc calls*/
struct DmiAccessData {
    DmiId_t 	groupId;
    DmiId_t 	attributeId;
};
typedef struct DmiAccessData   DmiAccessData_t;

struct DmiAccessDataList {
    DmiAccessData_t 	list<>;
 } ;

typedef struct DmiAccessDataList DmiAccessDataList_t; 

struct DmiRegisterInfo {
    DmiId_t			compId;
	u_long          prognum; 
/*    CiGetAttribute*		ciGetAttribute;
    CiGetNextAttribute*	ciGetNextAttribute;
    CiReserveAttribute*	ciReserveAttribute;
    CiReleaseAttribute*	ciReleaseAttribute;
    CiSetAttribute*		ciSetAttribute;
    CiAddRow*			ciAddRow; 
    CiDeleteRow*		ciDeleteRow;*/
    DmiAccessDataList_t*	accessData;
 };

typedef struct DmiRegisterInfo DmiRegisterInfo_t; 
