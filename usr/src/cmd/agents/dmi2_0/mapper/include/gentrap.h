/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)gentrap.h	1.1 96/08/05 Sun Microsystems"

#ifndef gentrapH
#define gentrapH

/********************************************************************/
/*                                                                                        */
/*  Header File Name: <gentrap.h>                                                      */
/*                                                                                        */
/*  Header File Description:  This header file defines constants and                  */
/*                              structures related to the gen trap function              */
/*                                                                                        */
/* Change Activity:                                                                      */
/*                                                                                        */
/* CFD List:                                                                             */
/*                                                                                        */
/*                                                                                        */
/* End Change Activity.                                                                 */
/*                                                                                        */
/********************************************************************/

/********************************************************************/
/* Event/Trap Constants                                                                */
/********************************************************************/

#define DMI_HW                     1              /* DMI HW Indication             */
#define DMI_SW                     2              /* DMI SW Indication             */

#define DMI_SpecTrap_Zero         0          /* Specific trap 0                        */
#define DMI_SpecTrap_InstallHW    1           /* HW Install Specific trap          */
#define DMI_SpecTrap_UnInstallHW  2          /* HW Uninstall Specific trap        */
#define DMI_SpecTrap_InstallSW    3           /* SW Install Specific trap            */
#define DMI_SpecTrap_UnInstallSW  4          /* SW Uninstall Specific trap        */
#define DMI_SpecTrap_Reserved5    5          /* Specific Trap 5 reserved         */
#define DMI_SpecTrap_Event          6          /*  DMI Event trap                      */
#define DMI_SpecTrap_ComponentAdded   7      /*Component Added Trap */
#define DMI_SpecTrap_ComponentDeleted 8      /*Component Deleted Trap */
#define DMI_SpecTrap_LanguageAdded    9      /*Language Added Trap */
#define DMI_SpecTrap_LanguageDeleted  10     /*Language Deleted Trap */
#define DMI_SpecTrap_GroupAdded       11     /*Group Added Trap */
#define DMI_SpecTrap_GroupDeleted     12     /*Group Deleted Trap */
#define DMI_SpecTrap_SubscriptionNotice  13  /*Subscription Notice Trap */
#define DMI_SpecTrap_EventOccurence   14     /*Event Generation Trap */
#define DMI_SpecTrap_Subscription_warning 15 /*Subscription Exp. Warning */
#define DMI_SpecTrap_Subscription_exp     16 /*Subscription Exp. notice */

#define SNMP_SMI_UNKNOWN             0x00 /* lookup SMI in MIB tree  */
#define SNMP_SMI_INTEGER             0x02 /* ...  SNMPv1 and SNMPv2  */
#define SNMP_SMI_Integer32           0x02 /* ...  SNMPv2             */
#define SNMP_SMI_BIT_STRING          0x03 /* ...  SNMPv2             */
#define SNMP_SMI_OCTET_STRING        0x04 /* ...                     */
#define SNMP_SMI_NULL                0x05 /* ...                     */
#define SNMP_SMI_OBJECT_IDENTIFIER   0x06 /* ...                     */
#define SNMP_SMI_DISPLAYSTRING        0x07 /* ...                     */
#define SNMP_SMI_SEQUENCE            0x30 /* ... (never in container)*/
#define SNMP_SMI_SEQUENCE_OF         0x30 /* ... (never in container)*/
#define SNMP_SMI_IpAddress           0x40 /* ...  SNMPv1 and SNMPv2  */
#define SNMP_SMI_Counter             0x41 /* ...  SNMPv1             */
#define SNMP_SMI_Counter32           0x41 /* ...  SNMPv2             */
#define SNMP_SMI_Gauge               0x42 /* ...  SNMPv1             */
#define SNMP_SMI_Gauge32             0x42 /* ...  SNMPv2             */
#define SNMP_SMI_TimeTicks           0x43 /* ...  SNMPv1 and SNMPv2  */
#define SNMP_SMI_Opaque              0x44 /* ...                     */
#define SNMP_SMI_NsapAddress         0x45 /* ...  SNMPv2             */
#define SNMP_SMI_Counter64           0x46 /* ...  SNMPv2             */
#define SNMP_SMI_UInteger32          0x47 /* ...  SNMPv2             */

/*********************************************************************/
/* The SNMPv1 generic trap types (INTEGER)                           */
/*********************************************************************/
#define SNMP_Trap_ColdStart                    0
#define SNMP_Trap_WarmStart                  1
#define SNMP_Trap_LinkDown                   2
#define SNMP_Trap_LinkUp                      3
#define SNMP_Trap_AuthenticationFailure       4
#define SNMP_Trap_EgpNeighborLoss            5
#define SNMP_Trap_EnterpriseSpecific          6

/*********************************************************************/
/* Error codes for parse_dmi_indicate and generate_trap                               */
/*********************************************************************/
#define Parse_NoErr           0
#define Parse_DmiLvl_Err     20
#define Parse_Rtnfunc_Err    21
#define Parse_Invindic_Err    22
#define Parse_Invcomp_Err   23
#define Parse_Memory_Err   24
#define CA_EVENT_OK           0
#define CA_EVENT_IGNORE   -1
#define Varbind_build_error  -1
#define Varbind_build_ok      0
/*#define SUCCESS               0 */
#define FAILURE              -1
#define SET_COMPLETE      0


/********************************************************************/
/* Misc Constants                                                                       */
/********************************************************************/

#define Ca_400                    "Client Access"             /* Client Access Group name */
#define Ca_SW                     "Software"                  /* Software Identifier           */
#define Ca_Event                   "Event type"             /* Client Access Event Attribute Name */
#define Ca_PR                       "Problem Record"         /* Client Access Enum Problem Record */
#define Ca_Notify                  "Notify Sent"                /* Client Access Enum Notify Sent */
#define Ca_EV                       "Event"                       /* Client Access Enum Event */
#define Ca_Trap                    6                               /* Enterprise Specific Trap    */
#define MaxNumVarbinds         100                /* max vb's that will be pass */
#define MaxNumOidchars         256              /* max num of chars in an OID  */
#define MaxNumVbchars         1200              /* max num of chars in vb data */
#define MaxNameSize             256               /*  max size of a Name Sring     */
/********************************************************************/
/********************************************************************/
/* Trap Structure / typedefs for structures                                             */
/********************************************************************/

typedef struct {                          /* 8 byte signed integer   */
  long int           high;                /*    - Most Significant   */
  unsigned long int  low;                 /*    - Least Significant  */
}                          Trap_Int64;    /* 8 byte signed integer   */
typedef long  int          Trap_Int32;    /* 4 byte signed integer   */
typedef unsigned long  int Trap_U_Int32; /* 4 byte unsigned integer   */

typedef struct {                         /* snmp variable binding   */
  char       vb_oid[MaxNumOidchars];  /*    -  OID                */
  char       vb_inst[MaxNumOidchars];  /*    -  Instance         */
  int         vb_datatype;               /*    -  data type          */
  int        vb_datalen;                 /* $MED data length         */
  union {
      char        vb_data[MaxNumVbchars];/* -  data (char string) */
      Trap_Int64  vb_u64;               /*    -  data 64b int      */
      Trap_Int32  vb_u32;               /*    -  data 32b int      */
      Trap_U_Int32  vb_uu32;             /*    -  data 32b int      */
         } data;
}                          Varbind;       /* snmp variable binding   */

typedef struct {                          /* trap data (incoming)          */
  char     community[256];                /*    - community name         */
  char     enterprise[256];                 /*    - enterprise OID       */
  int       trap_generic;                 /*    - generic TRAP type      */
  int       trap_specific;                /*    - specific TRAP type     */
  int       varBind_count;               /*    - number of varBinds      */
  Varbind  varBinds_a[MaxNumVarbinds];/*   - array of varbinds         */
}  Trap_In;                               /* trap data (incoming)           */

typedef Trap_In *Trap_In_ptr;


#endif

