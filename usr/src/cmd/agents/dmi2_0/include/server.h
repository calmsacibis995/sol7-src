/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)server.h	1.3 96/09/11 Sun Microsystems"

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _SERVER_H_RPCGEN
#define	_SERVER_H_RPCGEN

#include <rpc/rpc.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif


struct DmiRegisterIN {
	DmiHandle_t handle;
};
typedef struct DmiRegisterIN DmiRegisterIN;

struct DmiRegisterOUT {
	DmiErrorStatus_t error_status;
	DmiHandle_t *handle;
};
typedef struct DmiRegisterOUT DmiRegisterOUT;

struct DmiUnregisterOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiUnregisterOUT DmiUnregisterOUT;

struct DmiUnregisterIN {
	DmiHandle_t handle;
};
typedef struct DmiUnregisterIN DmiUnregisterIN;

struct DmiGetVersionOUT {
	DmiErrorStatus_t error_status;
	DmiString_t *dmiSpecLevel;
	DmiString_t *description;
	DmiFileTypeList_t *fileTypes;
};
typedef struct DmiGetVersionOUT DmiGetVersionOUT;

struct DmiGetVersionIN {
	DmiHandle_t handle;
};
typedef struct DmiGetVersionIN DmiGetVersionIN;

struct DmiGetConfigOUT {
	DmiErrorStatus_t error_status;
	DmiString_t *language;
};
typedef struct DmiGetConfigOUT DmiGetConfigOUT;

struct DmiGetConfigIN {
	DmiHandle_t handle;
};
typedef struct DmiGetConfigIN DmiGetConfigIN;

struct DmiSetConfigOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiSetConfigOUT DmiSetConfigOUT;

struct DmiSetConfigIN {
	DmiHandle_t handle;
	DmiString_t *language;
};
typedef struct DmiSetConfigIN DmiSetConfigIN;

struct DmiListComponentsOUT {
	DmiErrorStatus_t error_status;
	DmiComponentList_t *reply;
};
typedef struct DmiListComponentsOUT DmiListComponentsOUT;

struct DmiListComponentsIN {
	DmiHandle_t handle;
	DmiRequestMode_t requestMode;
	DmiUnsigned_t maxCount;
	DmiBoolean_t getPragma;
	DmiBoolean_t getDescription;
	DmiId_t compId;
};
typedef struct DmiListComponentsIN DmiListComponentsIN;

struct DmiListComponentsByClassOUT {
	DmiErrorStatus_t error_status;
	DmiComponentList_t *reply;
};
typedef struct DmiListComponentsByClassOUT DmiListComponentsByClassOUT;

struct DmiListComponentsByClassIN {
	DmiHandle_t handle;
	DmiRequestMode_t requestMode;
	DmiUnsigned_t maxCount;
	DmiBoolean_t getPragma;
	DmiBoolean_t getDescription;
	DmiId_t compId;
	DmiString_t *className;
	DmiAttributeValues_t *keyList;
};
typedef struct DmiListComponentsByClassIN DmiListComponentsByClassIN;

struct DmiListLanguagesOUT {
	DmiErrorStatus_t error_status;
	DmiStringList_t *reply;
};
typedef struct DmiListLanguagesOUT DmiListLanguagesOUT;

struct DmiListLanguagesIN {
	DmiHandle_t handle;
	DmiUnsigned_t maxCount;
	DmiId_t compId;
};
typedef struct DmiListLanguagesIN DmiListLanguagesIN;

struct DmiListClassNamesOUT {
	DmiErrorStatus_t error_status;
	DmiClassNameList_t *reply;
};
typedef struct DmiListClassNamesOUT DmiListClassNamesOUT;

struct DmiListClassNamesIN {
	DmiHandle_t handle;
	DmiUnsigned_t maxCount;
	DmiId_t compId;
};
typedef struct DmiListClassNamesIN DmiListClassNamesIN;

struct DmiListGroupsOUT {
	DmiErrorStatus_t error_status;
	DmiGroupList_t *reply;
};
typedef struct DmiListGroupsOUT DmiListGroupsOUT;

struct DmiListGroupsIN {
	DmiHandle_t handle;
	DmiRequestMode_t requestMode;
	DmiUnsigned_t maxCount;
	DmiBoolean_t getPragma;
	DmiBoolean_t getDescription;
	DmiId_t compId;
	DmiId_t groupId;
};
typedef struct DmiListGroupsIN DmiListGroupsIN;

struct DmiListAttributesOUT {
	DmiErrorStatus_t error_status;
	DmiAttributeList_t *reply;
};
typedef struct DmiListAttributesOUT DmiListAttributesOUT;

struct DmiListAttributesIN {
	DmiHandle_t handle;
	DmiRequestMode_t requestMode;
	DmiUnsigned_t maxCount;
	DmiBoolean_t getPragma;
	DmiBoolean_t getDescription;
	DmiId_t compId;
	DmiId_t groupId;
	DmiId_t attribId;
};
typedef struct DmiListAttributesIN DmiListAttributesIN;

struct DmiAddComponentOUT {
	DmiErrorStatus_t error_status;
	DmiId_t compId;
	DmiStringList_t *errors;
};
typedef struct DmiAddComponentOUT DmiAddComponentOUT;

struct DmiAddComponentIN {
	DmiHandle_t handle;
	DmiFileDataList_t *fileData;
};
typedef struct DmiAddComponentIN DmiAddComponentIN;

struct DmiAddLanguageOUT {
	DmiErrorStatus_t error_status;
	DmiStringList_t *errors;
};
typedef struct DmiAddLanguageOUT DmiAddLanguageOUT;

struct DmiAddLanguageIN {
	DmiHandle_t handle;
	DmiFileDataList_t *fileData;
	DmiId_t compId;
};
typedef struct DmiAddLanguageIN DmiAddLanguageIN;

struct DmiAddGroupOUT {
	DmiErrorStatus_t error_status;
	DmiId_t groupId;
	DmiStringList_t *errors;
};
typedef struct DmiAddGroupOUT DmiAddGroupOUT;

struct DmiAddGroupIN {
	DmiHandle_t handle;
	DmiFileDataList_t *fileData;
	DmiId_t compId;
};
typedef struct DmiAddGroupIN DmiAddGroupIN;

struct DmiDeleteComponentOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiDeleteComponentOUT DmiDeleteComponentOUT;

struct DmiDeleteComponentIN {
	DmiHandle_t handle;
	DmiId_t compId;
};
typedef struct DmiDeleteComponentIN DmiDeleteComponentIN;

struct DmiDeleteLanguageOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiDeleteLanguageOUT DmiDeleteLanguageOUT;

struct DmiDeleteLanguageIN {
	DmiHandle_t handle;
	DmiString_t *language;
	DmiId_t compId;
};
typedef struct DmiDeleteLanguageIN DmiDeleteLanguageIN;

struct DmiDeleteGroupOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiDeleteGroupOUT DmiDeleteGroupOUT;

struct DmiDeleteGroupIN {
	DmiHandle_t handle;
	DmiId_t compId;
	DmiId_t groupId;
};
typedef struct DmiDeleteGroupIN DmiDeleteGroupIN;

struct DmiAddRowOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiAddRowOUT DmiAddRowOUT;

struct DmiAddRowIN {
	DmiHandle_t handle;
	DmiRowData_t *rowData;
};
typedef struct DmiAddRowIN DmiAddRowIN;

struct DmiDeleteRowOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiDeleteRowOUT DmiDeleteRowOUT;

struct DmiDeleteRowIN {
	DmiHandle_t handle;
	DmiRowData_t *rowData;
};
typedef struct DmiDeleteRowIN DmiDeleteRowIN;

struct DmiGetMultipleOUT {
	DmiErrorStatus_t error_status;
	DmiMultiRowData_t *rowData;
};
typedef struct DmiGetMultipleOUT DmiGetMultipleOUT;

struct DmiGetMultipleIN {
	DmiHandle_t handle;
	DmiMultiRowRequest_t *request;
};
typedef struct DmiGetMultipleIN DmiGetMultipleIN;

struct DmiSetMultipleOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiSetMultipleOUT DmiSetMultipleOUT;

struct DmiSetMultipleIN {
	DmiHandle_t handle;
	DmiSetMode_t setMode;
	DmiMultiRowData_t *rowData;
};
typedef struct DmiSetMultipleIN DmiSetMultipleIN;

struct DmiGetAttributeOUT {
	DmiErrorStatus_t error_status;
	DmiDataUnion_t *value;
};
typedef struct DmiGetAttributeOUT DmiGetAttributeOUT;

struct DmiGetAttributeIN {
	DmiHandle_t handle;
	DmiId_t compId;
	DmiId_t groupId;
	DmiId_t attribId;
	DmiAttributeValues_t *keyList;
};
typedef struct DmiGetAttributeIN DmiGetAttributeIN;

struct DmiSetAttributeOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiSetAttributeOUT DmiSetAttributeOUT;

struct DmiSetAttributeIN {
	DmiHandle_t handle;
	DmiId_t compId;
	DmiId_t groupId;
	DmiId_t attribId;
	DmiAttributeValues_t *keyList;
	DmiSetMode_t setMode;
	DmiDataUnion_t *value;
};
typedef struct DmiSetAttributeIN DmiSetAttributeIN;

struct DmiRegisterCiIN {
	DmiRegisterInfo_t *regInfo;
};
typedef struct DmiRegisterCiIN DmiRegisterCiIN;

struct DmiRegisterCiOUT {
	DmiErrorStatus_t error_status;
	DmiHandle_t *handle;
	DmiString_t *dmiSpecLevel;
};
typedef struct DmiRegisterCiOUT DmiRegisterCiOUT;

struct DmiUnregisterCiIN {
	DmiHandle_t handle;
};
typedef struct DmiUnregisterCiIN DmiUnregisterCiIN;

struct DmiUnregisterCiOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiUnregisterCiOUT DmiUnregisterCiOUT;

struct DmiOriginateEventIN {
	DmiId_t compId;
	DmiString_t *language;
	DmiTimestamp_t *timestamp;
	DmiRowData_t *rowData;
};
typedef struct DmiOriginateEventIN DmiOriginateEventIN;

struct DmiOriginateEventOUT {
	DmiErrorStatus_t error_status;
};
typedef struct DmiOriginateEventOUT DmiOriginateEventOUT;

#define	DMI2_SERVER ((unsigned long)(300598))
#define	DMI2_SERVER_VERSION ((unsigned long)(0x1))

#if defined(__STDC__) || defined(__cplusplus)
#define	_DmiRegister ((unsigned long)(0x200))
extern  DmiRegisterOUT * _dmiregister_0x1(DmiRegisterIN *, CLIENT *);
extern  DmiRegisterOUT * _dmiregister_0x1_svc(DmiRegisterIN *, struct svc_req *);
#define	_DmiUnregister ((unsigned long)(0x201))
extern  DmiUnregisterOUT * _dmiunregister_0x1(DmiUnregisterIN *, CLIENT *);
extern  DmiUnregisterOUT * _dmiunregister_0x1_svc(DmiUnregisterIN *, struct svc_req *);
#define	_DmiGetVersion ((unsigned long)(0x202))
extern  DmiGetVersionOUT * _dmigetversion_0x1(DmiGetVersionIN *, CLIENT *);
extern  DmiGetVersionOUT * _dmigetversion_0x1_svc(DmiGetVersionIN *, struct svc_req *);
#define	_DmiGetConfig ((unsigned long)(0x203))
extern  DmiGetConfigOUT * _dmigetconfig_0x1(DmiGetConfigIN *, CLIENT *);
extern  DmiGetConfigOUT * _dmigetconfig_0x1_svc(DmiGetConfigIN *, struct svc_req *);
#define	_DmiSetConfig ((unsigned long)(0x204))
extern  DmiSetConfigOUT * _dmisetconfig_0x1(DmiSetConfigIN *, CLIENT *);
extern  DmiSetConfigOUT * _dmisetconfig_0x1_svc(DmiSetConfigIN *, struct svc_req *);
#define	_DmiListComponents ((unsigned long)(0x205))
extern  DmiListComponentsOUT * _dmilistcomponents_0x1(DmiListComponentsIN *, CLIENT *);
extern  DmiListComponentsOUT * _dmilistcomponents_0x1_svc(DmiListComponentsIN *, struct svc_req *);
#define	_DmiListComponentsByClass ((unsigned long)(0x206))
extern  DmiListComponentsByClassOUT * _dmilistcomponentsbyclass_0x1(DmiListComponentsByClassIN *, CLIENT *);
extern  DmiListComponentsByClassOUT * _dmilistcomponentsbyclass_0x1_svc(DmiListComponentsByClassIN *, struct svc_req *);
#define	_DmiListLanguages ((unsigned long)(0x207))
extern  DmiListLanguagesOUT * _dmilistlanguages_0x1(DmiListLanguagesIN *, CLIENT *);
extern  DmiListLanguagesOUT * _dmilistlanguages_0x1_svc(DmiListLanguagesIN *, struct svc_req *);
#define	_DmiListClassNames ((unsigned long)(0x208))
extern  DmiListClassNamesOUT * _dmilistclassnames_0x1(DmiListClassNamesIN *, CLIENT *);
extern  DmiListClassNamesOUT * _dmilistclassnames_0x1_svc(DmiListClassNamesIN *, struct svc_req *);
#define	_DmiListGroups ((unsigned long)(0x209))
extern  DmiListGroupsOUT * _dmilistgroups_0x1(DmiListGroupsIN *, CLIENT *);
extern  DmiListGroupsOUT * _dmilistgroups_0x1_svc(DmiListGroupsIN *, struct svc_req *);
#define	_DmiListAttributes ((unsigned long)(0x20a))
extern  DmiListAttributesOUT * _dmilistattributes_0x1(DmiListAttributesIN *, CLIENT *);
extern  DmiListAttributesOUT * _dmilistattributes_0x1_svc(DmiListAttributesIN *, struct svc_req *);
#define	_DmiAddRow ((unsigned long)(0x20b))
extern  DmiAddRowOUT * _dmiaddrow_0x1(DmiAddRowIN *, CLIENT *);
extern  DmiAddRowOUT * _dmiaddrow_0x1_svc(DmiAddRowIN *, struct svc_req *);
#define	_DmiDeleteRow ((unsigned long)(0x20c))
extern  DmiDeleteRowOUT * _dmideleterow_0x1(DmiDeleteRowIN *, CLIENT *);
extern  DmiDeleteRowOUT * _dmideleterow_0x1_svc(DmiDeleteRowIN *, struct svc_req *);
#define	_DmiGetMultiple ((unsigned long)(0x20d))
extern  DmiGetMultipleOUT * _dmigetmultiple_0x1(DmiGetMultipleIN *, CLIENT *);
extern  DmiGetMultipleOUT * _dmigetmultiple_0x1_svc(DmiGetMultipleIN *, struct svc_req *);
#define	_DmiSetMultiple ((unsigned long)(0x20e))
extern  DmiSetMultipleOUT * _dmisetmultiple_0x1(DmiSetMultipleIN *, CLIENT *);
extern  DmiSetMultipleOUT * _dmisetmultiple_0x1_svc(DmiSetMultipleIN *, struct svc_req *);
#define	_DmiAddComponent ((unsigned long)(0x20f))
extern  DmiAddComponentOUT * _dmiaddcomponent_0x1(DmiAddComponentIN *, CLIENT *);
extern  DmiAddComponentOUT * _dmiaddcomponent_0x1_svc(DmiAddComponentIN *, struct svc_req *);
#define	_DmiAddLanguage ((unsigned long)(0x210))
extern  DmiAddLanguageOUT * _dmiaddlanguage_0x1(DmiAddLanguageIN *, CLIENT *);
extern  DmiAddLanguageOUT * _dmiaddlanguage_0x1_svc(DmiAddLanguageIN *, struct svc_req *);
#define	_DmiAddGroup ((unsigned long)(0x211))
extern  DmiAddGroupOUT * _dmiaddgroup_0x1(DmiAddGroupIN *, CLIENT *);
extern  DmiAddGroupOUT * _dmiaddgroup_0x1_svc(DmiAddGroupIN *, struct svc_req *);
#define	_DmiDeleteComponent ((unsigned long)(0x212))
extern  DmiDeleteComponentOUT * _dmideletecomponent_0x1(DmiDeleteComponentIN *, CLIENT *);
extern  DmiDeleteComponentOUT * _dmideletecomponent_0x1_svc(DmiDeleteComponentIN *, struct svc_req *);
#define	_DmiDeleteLanguage ((unsigned long)(0x213))
extern  DmiDeleteLanguageOUT * _dmideletelanguage_0x1(DmiDeleteLanguageIN *, CLIENT *);
extern  DmiDeleteLanguageOUT * _dmideletelanguage_0x1_svc(DmiDeleteLanguageIN *, struct svc_req *);
#define	_DmiDeleteGroup ((unsigned long)(0x214))
extern  DmiDeleteGroupOUT * _dmideletegroup_0x1(DmiDeleteGroupIN *, CLIENT *);
extern  DmiDeleteGroupOUT * _dmideletegroup_0x1_svc(DmiDeleteGroupIN *, struct svc_req *);
#define	_DmiGetAttribute ((unsigned long)(0x215))
extern  DmiGetAttributeOUT * _dmigetattribute_0x1(DmiGetAttributeIN *, CLIENT *);
extern  DmiGetAttributeOUT * _dmigetattribute_0x1_svc(DmiGetAttributeIN *, struct svc_req *);
#define	_DmiSetAttribute ((unsigned long)(0x216))
extern  DmiSetAttributeOUT * _dmisetattribute_0x1(DmiSetAttributeIN *, CLIENT *);
extern  DmiSetAttributeOUT * _dmisetattribute_0x1_svc(DmiSetAttributeIN *, struct svc_req *);
extern int dmi2_server_0x1_freeresult(SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define	_DmiRegister ((unsigned long)(0x200))
extern  DmiRegisterOUT * _dmiregister_0x1();
extern  DmiRegisterOUT * _dmiregister_0x1_svc();
#define	_DmiUnregister ((unsigned long)(0x201))
extern  DmiUnregisterOUT * _dmiunregister_0x1();
extern  DmiUnregisterOUT * _dmiunregister_0x1_svc();
#define	_DmiGetVersion ((unsigned long)(0x202))
extern  DmiGetVersionOUT * _dmigetversion_0x1();
extern  DmiGetVersionOUT * _dmigetversion_0x1_svc();
#define	_DmiGetConfig ((unsigned long)(0x203))
extern  DmiGetConfigOUT * _dmigetconfig_0x1();
extern  DmiGetConfigOUT * _dmigetconfig_0x1_svc();
#define	_DmiSetConfig ((unsigned long)(0x204))
extern  DmiSetConfigOUT * _dmisetconfig_0x1();
extern  DmiSetConfigOUT * _dmisetconfig_0x1_svc();
#define	_DmiListComponents ((unsigned long)(0x205))
extern  DmiListComponentsOUT * _dmilistcomponents_0x1();
extern  DmiListComponentsOUT * _dmilistcomponents_0x1_svc();
#define	_DmiListComponentsByClass ((unsigned long)(0x206))
extern  DmiListComponentsByClassOUT * _dmilistcomponentsbyclass_0x1();
extern  DmiListComponentsByClassOUT * _dmilistcomponentsbyclass_0x1_svc();
#define	_DmiListLanguages ((unsigned long)(0x207))
extern  DmiListLanguagesOUT * _dmilistlanguages_0x1();
extern  DmiListLanguagesOUT * _dmilistlanguages_0x1_svc();
#define	_DmiListClassNames ((unsigned long)(0x208))
extern  DmiListClassNamesOUT * _dmilistclassnames_0x1();
extern  DmiListClassNamesOUT * _dmilistclassnames_0x1_svc();
#define	_DmiListGroups ((unsigned long)(0x209))
extern  DmiListGroupsOUT * _dmilistgroups_0x1();
extern  DmiListGroupsOUT * _dmilistgroups_0x1_svc();
#define	_DmiListAttributes ((unsigned long)(0x20a))
extern  DmiListAttributesOUT * _dmilistattributes_0x1();
extern  DmiListAttributesOUT * _dmilistattributes_0x1_svc();
#define	_DmiAddRow ((unsigned long)(0x20b))
extern  DmiAddRowOUT * _dmiaddrow_0x1();
extern  DmiAddRowOUT * _dmiaddrow_0x1_svc();
#define	_DmiDeleteRow ((unsigned long)(0x20c))
extern  DmiDeleteRowOUT * _dmideleterow_0x1();
extern  DmiDeleteRowOUT * _dmideleterow_0x1_svc();
#define	_DmiGetMultiple ((unsigned long)(0x20d))
extern  DmiGetMultipleOUT * _dmigetmultiple_0x1();
extern  DmiGetMultipleOUT * _dmigetmultiple_0x1_svc();
#define	_DmiSetMultiple ((unsigned long)(0x20e))
extern  DmiSetMultipleOUT * _dmisetmultiple_0x1();
extern  DmiSetMultipleOUT * _dmisetmultiple_0x1_svc();
#define	_DmiAddComponent ((unsigned long)(0x20f))
extern  DmiAddComponentOUT * _dmiaddcomponent_0x1();
extern  DmiAddComponentOUT * _dmiaddcomponent_0x1_svc();
#define	_DmiAddLanguage ((unsigned long)(0x210))
extern  DmiAddLanguageOUT * _dmiaddlanguage_0x1();
extern  DmiAddLanguageOUT * _dmiaddlanguage_0x1_svc();
#define	_DmiAddGroup ((unsigned long)(0x211))
extern  DmiAddGroupOUT * _dmiaddgroup_0x1();
extern  DmiAddGroupOUT * _dmiaddgroup_0x1_svc();
#define	_DmiDeleteComponent ((unsigned long)(0x212))
extern  DmiDeleteComponentOUT * _dmideletecomponent_0x1();
extern  DmiDeleteComponentOUT * _dmideletecomponent_0x1_svc();
#define	_DmiDeleteLanguage ((unsigned long)(0x213))
extern  DmiDeleteLanguageOUT * _dmideletelanguage_0x1();
extern  DmiDeleteLanguageOUT * _dmideletelanguage_0x1_svc();
#define	_DmiDeleteGroup ((unsigned long)(0x214))
extern  DmiDeleteGroupOUT * _dmideletegroup_0x1();
extern  DmiDeleteGroupOUT * _dmideletegroup_0x1_svc();
#define	_DmiGetAttribute ((unsigned long)(0x215))
extern  DmiGetAttributeOUT * _dmigetattribute_0x1();
extern  DmiGetAttributeOUT * _dmigetattribute_0x1_svc();
#define	_DmiSetAttribute ((unsigned long)(0x216))
extern  DmiSetAttributeOUT * _dmisetattribute_0x1();
extern  DmiSetAttributeOUT * _dmisetattribute_0x1_svc();
extern int dmi2_server_0x1_freeresult();
#endif /* K&R C */

#define	DMI2_CSERVER ((unsigned long)(0x30000000))
#define	DMI2_CSERVER_VERSION ((unsigned long)(0x1))

#if defined(__STDC__) || defined(__cplusplus)
#define	_DmiRegisterCi ((unsigned long)(0x300))
extern  DmiRegisterCiOUT * _dmiregisterci_0x1(DmiRegisterCiIN *, CLIENT *);
extern  DmiRegisterCiOUT * _dmiregisterci_0x1_svc(DmiRegisterCiIN *, struct svc_req *);
#define	_DmiUnregisterCi ((unsigned long)(0x301))
extern  DmiUnregisterCiOUT * _dmiunregisterci_0x1(DmiUnregisterCiIN *, CLIENT *);
extern  DmiUnregisterCiOUT * _dmiunregisterci_0x1_svc(DmiUnregisterCiIN *, struct svc_req *);
#define	_DmiOriginateEvent ((unsigned long)(0x302))
extern  DmiOriginateEventOUT * _dmioriginateevent_0x1(DmiOriginateEventIN *, CLIENT *);
extern  DmiOriginateEventOUT * _dmioriginateevent_0x1_svc(DmiOriginateEventIN *, struct svc_req *);
extern int dmi2_cserver_0x1_freeresult(SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define	_DmiRegisterCi ((unsigned long)(0x300))
extern  DmiRegisterCiOUT * _dmiregisterci_0x1();
extern  DmiRegisterCiOUT * _dmiregisterci_0x1_svc();
#define	_DmiUnregisterCi ((unsigned long)(0x301))
extern  DmiUnregisterCiOUT * _dmiunregisterci_0x1();
extern  DmiUnregisterCiOUT * _dmiunregisterci_0x1_svc();
#define	_DmiOriginateEvent ((unsigned long)(0x302))
extern  DmiOriginateEventOUT * _dmioriginateevent_0x1();
extern  DmiOriginateEventOUT * _dmioriginateevent_0x1_svc();
extern int dmi2_cserver_0x1_freeresult();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_DmiRegisterIN(XDR *, DmiRegisterIN*);
extern  bool_t xdr_DmiRegisterOUT(XDR *, DmiRegisterOUT*);
extern  bool_t xdr_DmiUnregisterOUT(XDR *, DmiUnregisterOUT*);
extern  bool_t xdr_DmiUnregisterIN(XDR *, DmiUnregisterIN*);
extern  bool_t xdr_DmiGetVersionOUT(XDR *, DmiGetVersionOUT*);
extern  bool_t xdr_DmiGetVersionIN(XDR *, DmiGetVersionIN*);
extern  bool_t xdr_DmiGetConfigOUT(XDR *, DmiGetConfigOUT*);
extern  bool_t xdr_DmiGetConfigIN(XDR *, DmiGetConfigIN*);
extern  bool_t xdr_DmiSetConfigOUT(XDR *, DmiSetConfigOUT*);
extern  bool_t xdr_DmiSetConfigIN(XDR *, DmiSetConfigIN*);
extern  bool_t xdr_DmiListComponentsOUT(XDR *, DmiListComponentsOUT*);
extern  bool_t xdr_DmiListComponentsIN(XDR *, DmiListComponentsIN*);
extern  bool_t xdr_DmiListComponentsByClassOUT(XDR *, DmiListComponentsByClassOUT*);
extern  bool_t xdr_DmiListComponentsByClassIN(XDR *, DmiListComponentsByClassIN*);
extern  bool_t xdr_DmiListLanguagesOUT(XDR *, DmiListLanguagesOUT*);
extern  bool_t xdr_DmiListLanguagesIN(XDR *, DmiListLanguagesIN*);
extern  bool_t xdr_DmiListClassNamesOUT(XDR *, DmiListClassNamesOUT*);
extern  bool_t xdr_DmiListClassNamesIN(XDR *, DmiListClassNamesIN*);
extern  bool_t xdr_DmiListGroupsOUT(XDR *, DmiListGroupsOUT*);
extern  bool_t xdr_DmiListGroupsIN(XDR *, DmiListGroupsIN*);
extern  bool_t xdr_DmiListAttributesOUT(XDR *, DmiListAttributesOUT*);
extern  bool_t xdr_DmiListAttributesIN(XDR *, DmiListAttributesIN*);
extern  bool_t xdr_DmiAddComponentOUT(XDR *, DmiAddComponentOUT*);
extern  bool_t xdr_DmiAddComponentIN(XDR *, DmiAddComponentIN*);
extern  bool_t xdr_DmiAddLanguageOUT(XDR *, DmiAddLanguageOUT*);
extern  bool_t xdr_DmiAddLanguageIN(XDR *, DmiAddLanguageIN*);
extern  bool_t xdr_DmiAddGroupOUT(XDR *, DmiAddGroupOUT*);
extern  bool_t xdr_DmiAddGroupIN(XDR *, DmiAddGroupIN*);
extern  bool_t xdr_DmiDeleteComponentOUT(XDR *, DmiDeleteComponentOUT*);
extern  bool_t xdr_DmiDeleteComponentIN(XDR *, DmiDeleteComponentIN*);
extern  bool_t xdr_DmiDeleteLanguageOUT(XDR *, DmiDeleteLanguageOUT*);
extern  bool_t xdr_DmiDeleteLanguageIN(XDR *, DmiDeleteLanguageIN*);
extern  bool_t xdr_DmiDeleteGroupOUT(XDR *, DmiDeleteGroupOUT*);
extern  bool_t xdr_DmiDeleteGroupIN(XDR *, DmiDeleteGroupIN*);
extern  bool_t xdr_DmiAddRowOUT(XDR *, DmiAddRowOUT*);
extern  bool_t xdr_DmiAddRowIN(XDR *, DmiAddRowIN*);
extern  bool_t xdr_DmiDeleteRowOUT(XDR *, DmiDeleteRowOUT*);
extern  bool_t xdr_DmiDeleteRowIN(XDR *, DmiDeleteRowIN*);
extern  bool_t xdr_DmiGetMultipleOUT(XDR *, DmiGetMultipleOUT*);
extern  bool_t xdr_DmiGetMultipleIN(XDR *, DmiGetMultipleIN*);
extern  bool_t xdr_DmiSetMultipleOUT(XDR *, DmiSetMultipleOUT*);
extern  bool_t xdr_DmiSetMultipleIN(XDR *, DmiSetMultipleIN*);
extern  bool_t xdr_DmiGetAttributeOUT(XDR *, DmiGetAttributeOUT*);
extern  bool_t xdr_DmiGetAttributeIN(XDR *, DmiGetAttributeIN*);
extern  bool_t xdr_DmiSetAttributeOUT(XDR *, DmiSetAttributeOUT*);
extern  bool_t xdr_DmiSetAttributeIN(XDR *, DmiSetAttributeIN*);
extern  bool_t xdr_DmiRegisterCiIN(XDR *, DmiRegisterCiIN*);
extern  bool_t xdr_DmiRegisterCiOUT(XDR *, DmiRegisterCiOUT*);
extern  bool_t xdr_DmiUnregisterCiIN(XDR *, DmiUnregisterCiIN*);
extern  bool_t xdr_DmiUnregisterCiOUT(XDR *, DmiUnregisterCiOUT*);
extern  bool_t xdr_DmiOriginateEventIN(XDR *, DmiOriginateEventIN*);
extern  bool_t xdr_DmiOriginateEventOUT(XDR *, DmiOriginateEventOUT*);

#else /* K&R C */
extern bool_t xdr_DmiRegisterIN();
extern bool_t xdr_DmiRegisterOUT();
extern bool_t xdr_DmiUnregisterOUT();
extern bool_t xdr_DmiUnregisterIN();
extern bool_t xdr_DmiGetVersionOUT();
extern bool_t xdr_DmiGetVersionIN();
extern bool_t xdr_DmiGetConfigOUT();
extern bool_t xdr_DmiGetConfigIN();
extern bool_t xdr_DmiSetConfigOUT();
extern bool_t xdr_DmiSetConfigIN();
extern bool_t xdr_DmiListComponentsOUT();
extern bool_t xdr_DmiListComponentsIN();
extern bool_t xdr_DmiListComponentsByClassOUT();
extern bool_t xdr_DmiListComponentsByClassIN();
extern bool_t xdr_DmiListLanguagesOUT();
extern bool_t xdr_DmiListLanguagesIN();
extern bool_t xdr_DmiListClassNamesOUT();
extern bool_t xdr_DmiListClassNamesIN();
extern bool_t xdr_DmiListGroupsOUT();
extern bool_t xdr_DmiListGroupsIN();
extern bool_t xdr_DmiListAttributesOUT();
extern bool_t xdr_DmiListAttributesIN();
extern bool_t xdr_DmiAddComponentOUT();
extern bool_t xdr_DmiAddComponentIN();
extern bool_t xdr_DmiAddLanguageOUT();
extern bool_t xdr_DmiAddLanguageIN();
extern bool_t xdr_DmiAddGroupOUT();
extern bool_t xdr_DmiAddGroupIN();
extern bool_t xdr_DmiDeleteComponentOUT();
extern bool_t xdr_DmiDeleteComponentIN();
extern bool_t xdr_DmiDeleteLanguageOUT();
extern bool_t xdr_DmiDeleteLanguageIN();
extern bool_t xdr_DmiDeleteGroupOUT();
extern bool_t xdr_DmiDeleteGroupIN();
extern bool_t xdr_DmiAddRowOUT();
extern bool_t xdr_DmiAddRowIN();
extern bool_t xdr_DmiDeleteRowOUT();
extern bool_t xdr_DmiDeleteRowIN();
extern bool_t xdr_DmiGetMultipleOUT();
extern bool_t xdr_DmiGetMultipleIN();
extern bool_t xdr_DmiSetMultipleOUT();
extern bool_t xdr_DmiSetMultipleIN();
extern bool_t xdr_DmiGetAttributeOUT();
extern bool_t xdr_DmiGetAttributeIN();
extern bool_t xdr_DmiSetAttributeOUT();
extern bool_t xdr_DmiSetAttributeIN();
extern bool_t xdr_DmiRegisterCiIN();
extern bool_t xdr_DmiRegisterCiOUT();
extern bool_t xdr_DmiUnregisterCiIN();
extern bool_t xdr_DmiUnregisterCiOUT();
extern bool_t xdr_DmiOriginateEventIN();
extern bool_t xdr_DmiOriginateEventOUT();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_SERVER_H_RPCGEN */