/* Copyright 12/17/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisa.h	1.10 96/12/17 Sun Microsystems"

/* Header Description *************************************************/
/*                                                                    */
/*  Name:  dmisa.h                                                    */
/*                                                                    */
/*  Description:                                                      */
/*  This file is the main header file for the DMI subagent function.  */
/*                                                                    */
/* End Header Description *********************************************/
/*                                                                    */
/* Copyright **********************************************************/
/*                                                                    */
/* Copyright:                                                         */
/*   Licensed Materials - Property of IBM                             */
/*   This product contains "Restricted Materials of IBM"              */
/*   xxxx-xxx (C) Copyright IBM Corp. 1994.                           */
/*   All rights reserved.                                             */
/*   US Government Users Restricted Rights -                          */
/*   Use, duplication or disclosure restricted by GSA ADP Schedule    */
/*   Contract with IBM Corp.                                          */
/*   See IBM Copyright Instructions.                                  */
/*                                                                    */
/* End Copyright ******************************************************/
/*                                                                    */
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  941214    LAUBLI    New file                      */
/*                  950221    LAUBLI    Make semaphore handles global */
/*                  950317    LAUBLI    Add ref. codes for malloc     */
/*                                                                    */
/* End Change Log *****************************************************/
#ifndef DMISA_H
#define DMISA_H

#ifdef WIN32
#elif defined(OS2)
#define INCL_DOSPROCESS    /* Process and thread values */
#define INCL_DOSQUEUES       /* Queue values */
#define INCL_DOSSEMAPHORES   /* Semaphore values */
#define INCL_VIO
#define INCL_WIN
#include <os2.h>
#include <direct.h>
#else
#define APIENTRY

#include <server.h>                       /*DMI2.0 */
#include <dmi_error.hh>                   /*DMI2.0 */
#include <api.hh>                         /*DMI2.0 */
#include <miapi.hh>                       /*DMI2.0 */
#include <os_svc.h> 
#include <os_dmi.h> 
#include <fcntl.h>
#include <sys/mode.h>
#include <sys/conf.h>
#include <signal.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <ctype.h>
#include <malloc.h>
#include <process.h>
#include <time.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <sys/resource.h>
#include <rpc/pmap_clnt.h>
#include <rpc/svc_soc.h>
#include <errno.h>
#include <indication.h>
#include <thread.h>
#include "snmp_dpi.h"                     /* DPI 2.0 API definitions */
/*#include "dmiapi.h" */     /* pull in the include files for the service layer*/



/* #define DMISA_TRACE */
#ifdef WIN32
#define PID     DWORD     /* WIN32 is really screwy on some of its types... */
#define TID     DWORD     /*       "     "                                  */
#define HMTX    HANDLE    /*       "     "                                  */
#define HEV     HANDLE    /*       "     "                                  */
#define HPIPE   HANDLE    /*       "     "                                  */
#define HFILE   HANDLE    /*       "     "                                  */
#endif


#include "trace.h"       /* pull in the include file for the trace function*/
#ifdef DMISA_TRACE
#define DMISA_TRACE_LOG(level,data) trace(level,data)
#define DMISA_TRACE_LOGBUF(level) trace(level,logbuffer)

#define DMISA_TRACE_LOG1(level,string,p1)                { sprintf(logbuffer,string,p1);trace(level,logbuffer); }
#define DMISA_TRACE_LOG2(level,string,p1,p2)             { sprintf(logbuffer,string,p1,p2);trace(level,logbuffer); }
#define DMISA_TRACE_LOG3(level,string,p1,p2,p3)          { sprintf(logbuffer,string,p1,p2,p3);trace(level,logbuffer); }
#define DMISA_TRACE_LOG4(level,string,p1,p2,p3,p4)       { sprintf(logbuffer,string,p1,p2,p3,p4);trace(level,logbuffer); }
#define DMISA_TRACE_LOG5(level,string,p1,p2,p3,p4,p5)    { sprintf(logbuffer,string,p1,p2,p3,p4,p5);trace(level,logbuffer); }
#define DMISA_TRACE_LOG6(level,string,p1,p2,p3,p4,p5,p6) { sprintf(logbuffer,string,p1,p2,p3,p4,p5,p6);trace(level,logbuffer); }

#define LOG_BUF_LEN      106    /*  Length of BUF_LINE_LENGTH (see trace.h)*/
char   *logbuf;   /* buffer used in logging via trace routine.        */
#if defined(OS2) || defined(WIN32)
HEV     hevWriteStart;   /* Trace buffering event semaphore handle, added 950221.gwl */
HMTX    hmtxLogbuf;      /* Mutex semaphore handle for trace, added 950222.gwl */
HMTX    hmtxWritebuf;    /* Mutex semaphore handle for trace, added 950222.gwl */
#endif
void doSemTraceCleanup(void);
#else
#define DMISA_TRACE_LOG(level,data)   tracem(level, data) /**/
#define DMISA_TRACE_LOGBUF(level)    tracem(level, logbuffer) /**/

#define DMISA_TRACE_LOG1(level,string,p1)                { sprintf(logbuffer,string,p1);tracem(level,logbuffer); }
#define DMISA_TRACE_LOG2(level,string,p1,p2)             { sprintf(logbuffer,string,p1,p2);tracem(level,logbuffer); }
#define DMISA_TRACE_LOG3(level,string,p1,p2,p3)          { sprintf(logbuffer,string,p1,p2,p3);tracem(level,logbuffer); }
#define DMISA_TRACE_LOG4(level,string,p1,p2,p3,p4)       { sprintf(logbuffer,string,p1,p2,p3,p4);tracem(level,logbuffer); }
#define DMISA_TRACE_LOG5(level,string,p1,p2,p3,p4,p5)    { sprintf(logbuffer,string,p1,p2,p3,p4,p5);tracem(level,logbuffer); }
#define DMISA_TRACE_LOG6(level,string,p1,p2,p3,p4,p5,p6) { sprintf(logbuffer,string,p1,p2,p3,p4,p5,p6);tracem(level,logbuffer); }
#if 0
#define DMISA_TRACE_LOG1(level,string,p1)                /**/
#define DMISA_TRACE_LOG2(level,string,p1,p2)             /**/
#define DMISA_TRACE_LOG3(level,string,p1,p2,p3)          /**/
#define DMISA_TRACE_LOG4(level,string,p1,p2,p3,p4)       /**/
#define DMISA_TRACE_LOG5(level,string,p1,p2,p3,p4,p5)    /**/
#define DMISA_TRACE_LOG6(level,string,p1,p2,p3,p4,p5,p6) /**/
#endif

#endif

#define INT64_IS_OCTETSTRING  /* When commented out, INTEGER64 is seen as COUNTER64*/
                              /* from SNMP; otherwise, seen as 8-byte OCTETSTRING  */
#define COUNTER64_IS_COUNTER  /* When commented out, COUNTER64 is seen as COUNTER64*/
                              /* from SNMP; otherwise, seen as COUNTER (for SNMPv1)*/

#ifdef WIN32      /* LLR 09-11-95 added for WIN32 */
#define COPYRIGHT1 "\nIBM Windows NT DMI to SNMP Translator, version 1.00\n"
#elif defined OS2
#define COPYRIGHT1 "\nIBM OS/2 DMI to SNMP Translator, version 1.00\n"
#else
#define COPYRIGHT1 "\nSUN DMI to SNMP Translator, version 1.00\n"
#endif
#define COPYRIGHT2 "Copyright 1996, 1997 Sun Microsystems, Inc. All Rights Reserved.\n"


#ifdef WIN32
#define DMI_SLEEP(t) Sleep(t)	   /* LLR 09-11-95 added for NT */
#elif defined OS2
#define DMI_SLEEP(t) DosSleep(t)
#else
#define DMI_SLEEP(t) sleep(t/2000)
#endif

#define DEFAULT_COMMUNITY  "public"
#define DMI_SUBAGENT_NAME  "SUN DMI SUBAGENT"

#define DMI_SUBAGENT          "1.3.6.1.4.1.42" 
#define SUN_ARCH_DMI          "1.3.6.1.4.1.42.2000."
#define SUN_ARCH_DMI_1        "1.3.6.1.4.1.42.2000.1.1."
#if 0
#define IBM_ARCH_DMI_1        "1.3.6.1.4.1.2.5.11.1."
#endif
#define SUN_ARCH_DMI_COMPMIB  "1.3.6.1.4.1.42.2000.1.1.1."
#define PRIVATE               "1.3.6.1.4.1."
#define MAX_VAR_BINDS_PER_PACKET    1 /* test well before raising this beyond 1*/

/*#define DPI_AGENT_WAIT_TIMEOUT    120 */   /* seconds, used on DPI OPEN */
#define DPI_AGENT_WAIT_TIMEOUT      120000000L
#define DPI_REG_TIMEOUT             0    /* seconds, except zero means use DPI OPEN value*/
#define DPI_UNREG_TIMEOUT          15    /* seconds                   */
#define DPI_WAIT_FOREVER           -1    /* wait forever              */
#define INDICATION_TIMEOUT      40000    /* milliseconds, wait for indicate semaphore*/
#define CONNECT_RETRY_TIME      10000    /* milliseconds              */
#define DMI_UNREG_REREG_PAUSE   95000    /* milliseconds (must be greater than */
                                         /*    DMI_REQUEST_TIME to allow DMI unreg)*/
#define DMI_REQUEST_TIMEOUT     90000    /* milliseconds                       */
#define KEY_LIST_DECAY_TIME       120    /* Maximum seconds between getnexts to any DMI table*/
                        /* decrease it         in order to avoid pulling all key values*/
#define RESERVE_LIST_DECAY_PERIOD 180    /* Maximum seconds until an attribute reserved*/
                                         /*    for a SET function is considered old and*/
                                         /*    will be released at the next opportunity*/
#define RESERVE_LIST_CLEANUP_PAUSE 70000 /* milliseconds              */

#define ID_BUFFER_SIZE           4096    /* in bytes                  */
#define REQBUFSIZE               4000UL  /* in bytes                  */
#define CNFBUFSIZE               4076UL  /* in bytes. Must be bigger than SNMP_DPI_BUFSIZE (now at 4096)*/
#define REGCNFSIZE               1000UL  /* in bytes                  */
#define ONE_BEFORE_GROUPID  "1."
#define ONE_AFTER_GROUPID   "1."
#define OID_INSTANCE_LEN    37 /* length of "1.gggggggggg.1.aaaaaaaaaa.cccccccccc."*/
#define DMI_REQ_HANDLE    1111 /* first DMI request transaction identifier, to be incremented thereafter*/
#define M_USER  100
#define DMISA_MAX_SUBID_COUNT 128  /* maximum number of sub-IDs in an OID*/
#ifndef ERROR_TIMEOUT
   #define ERROR_TIMEOUT      640
   #define ERROR_INTERRUPT     95
#endif
#define DMISA_NULL           NULL
#define DMISA_TRUE              1
#define DMISA_FALSE             0
#define DMISA_lessThan         -1
#define DMISA_equalTo           0
#define DMISA_greaterThan       1
#define DMISA_THIS             50  /* Used to identify component, group, or attribute ID of key*/
#define DMISA_FIRST            51  /*   "                             */
#define DMISA_NEXT             52  /*   "                             */
#define DMISA_LOWEST           53  /* Used to retrieve keys           */
#define DMISA_REQ_COLDSTART     1
#define DMISA_REQ_WARMSTART     2
#define DMISA_IMMEDIATE      4443  /* Used to clean reserve list      */
#define DMISA_TIMED          4444  /* Used to clean reserve list      */

/* Program operational states                                         */
#define DMISA_NONE           0  /* no connection with DMI or DPI interfaces, no XlateList*/
#define DMI_ONLY             1  /* DMI registered                     */
#define DMI_XLATE            2  /* DMI registered & XlateList built   */
#define DMI_XLATE_DPIOPEN    3  /* DMI reg'd, XlateList built, and DPI opened*/
#define DMI_XLATE_DPICONN    4  /* DMI reg'd, Xlate built and DPI opened & connected*/
#define DMI_XLATE_DPIREG     5  /* DMI reg'd, Xlate built and DPI opened, connected, & reg'd*/

/*#define REGISTERED      0*/
#define UNREGISTERED    1
#define SUBREGISTERED   2
#define LCNOTFOUND      1
#define LGNOTFOUND      2
#define LANOTFOUND      3
#define PARTFOUND       4
#define TODMI           1
#define TODPI           2
#define MAX_ULONG       0xFFFFFFFF
#define DMISA_MAXID     MAX_ULONG

#define DMISA_COMPDESC   7
#define DMISA_GROUPDESC  8
#define DMISA_ATTRDESC   9

#define ROUTINE_ORDER    0x0000EE00
#define VIO_HANDLE         0
#define DATE_LENGTH_dmi   28
#define DATE_LENGTH_snmp  25

/* DMI State Machine States                                           */
#define DMI_BROKE          1    /* used?                              */

/* FUNCTION RETURN CODES:                                             */
/*                         Notes:                                     */
/*                         1. Return codes ending in 9 are reserved for programming errors.*/
/*                         2. Return codes ending in 8 are reserved for malloc errors.*/
/*                         3. Return codes ending in 7 are reserved for DPI interface errors.*/
/*                         4. Return codes ending in 6 are reserved for DMI interface errors.*/
/*                         5. Return codes ending in 5 are reserved for cases where genErr*/
/*                            is to be returned to the SNMP agent via the DPI interface,*/
/*                            but reserved only in functions used in GET, GETNEXT, and SET*/
/*                            operations.                             */
/*                                                                    */
#define DMISA_ERROR_noError                0
#define DMISA_ERROR_noSuchObject           1
#define DMISA_ERROR_noSuchInstance         2
#define DMISA_ERROR_noSuchInstanceOrObject 3
#define DMISA_ERROR_endOfMibView           4
#define DMISA_ERROR_genErr                 5  /* ?? or separate per function?*/
#define DMISA_ERROR_dmiBroke               6  /* will result in reinitialization of DMI*/
                                 /* (may conflict in the future with DMI iStatus field)*/
#define DMISA_ERROR_dpiBroke               7  /* will result in reinitialization of DPI*/
#define DMISA_ERROR_outOfMemory            8
#define DMISA_ERROR_programmingError       9

#define INIT_SUB_AGENT_noError                    0    /* Initialization*/
#define INIT_SUB_AGENT_rpcregfailed              90    /*added 960711-jiten*/
#define INIT_SUB_AGENT_createSemFailed           91
#define INIT_SUB_AGENT_outOfMemory               98             /* added 950317.gwl */

#define DPI_CONNECT_AND_OPEN_noError              0    /* MAIN and DPI return codes (100's)*/
#define DPI_CONNECT_AND_OPEN_getHostNameFailed  101
#define DPI_CONNECT_AND_OPEN_connectFailed      102
#define DPI_CONNECT_AND_OPEN_openFailed         103

#define DPI_REGISTER_allRegistered                0
#define DPI_REGISTER_someRegistered             111
#define DPI_REGISTER_noneRegistered             112

#define DPI_REGISTER_ONE_noError                  0
#define DPI_REGISTER_ONE_alreadyRegistered      121
#define DPI_REGISTER_ONE_makeFailed             122
#define DPI_REGISTER_ONE_sendFailed             123
#define DPI_REGISTER_ONE_awaitFailed            124
#define DPI_REGISTER_ONE_parseFailed            125
#define DPI_REGISTER_ONE_notResponse            131
#define DPI_REGISTER_ONE_errorInResponse        132

#define DPI_UNREG_noError                         0

#define DPI_UNREG_ONE_noError                     0
#define DPI_UNREG_ONE_makePacketFailed          151
#define DPI_UNREG_ONE_sendFailed                152
#define DPI_UNREG_ONE_awaitFailed               153
#define DPI_UNREG_ONE_noPacketReceived          154
#define DPI_UNREG_ONE_notResponse               155
#define DPI_UNREG_ONE_unexpectedRC              161

#define UNREG_BY_DPI_noError                      0
#define UNREG_BY_DPI_failed                     171

#define DPI_CLOSE_noError                         0
#define DPI_CLOSE_failed                        181

#define DPI_CLOSE_BY_AGENT_dpiDown              191

#define DO_GET_noError                            0    /* GET return codes (200's)*/
#define DO_GET_dpiBroke                         207

#define GET_PARSE_INSTANCE_fullySpecified         0
#define GET_PARSE_INSTANCE_fullySpecified_noKey   0
#define GET_PARSE_INSTANCE_notFullySpecified    211
#define GET_PARSE_INSTANCE_programmingError     219

#define GET_FIND_GC_PAIR_pairFound                   0
#define GET_FIND_GC_PAIR_pairNotFound_groupMatch   221
#define GET_FIND_GC_PAIR_pairNotFound_noGroupMatch 222

#define GET_TRANSLATE_KEY_noError                 0

#define GET_ATTRIBUTE_getOk                       0
#define GET_ATTRIBUTE_cnfBufAddressability      241
#define GET_ATTRIBUTE_tooBig                    244
#define GET_ATTRIBUTE_genErr                    245
#define GET_ATTRIBUTE_dmiBroke                  246
#define GET_ATTRIBUTE_dpiBroke                  247

#define GET_COMP_MIB_getOk                        0
#define GET_COMP_MIB_noSuchInstance             251
#define GET_COMP_MIB_noSuchInstanceOrObject     252
#define GET_COMP_MIB_noSuchObject               253
#define GET_COMP_MIB_tooBig                     254
#define GET_COMP_MIB_genErr                     255
#define GET_COMP_MIB_dmiBroke                   256
#define GET_COMP_MIB_dpiBroke                   257

#define NO_SUCH_INSTANCE_noError                  0
#define NO_SUCH_INSTANCE_dpiBroke               267

#define NO_SUCH_OBJECT_noError                    0
#define NO_SUCH_OBJECT_dpiBroke                 277

#define DO_GET_NEXT_noError                       0    /* GETNEXT return codes (300's, 400's)*/
#define DO_GET_NEXT_dpiBroke                    307

#define GN_GET_NEXT_MIF_OBJECT_gottit             0
#define GN_GET_NEXT_MIF_OBJECT_dpiBroke         317

#define GN_PARSE_INSTANCE_parseOk                 0
#define GN_PARSE_INSTANCE_badOid                321

#define GN_FIND_NEXT_OBJECT_found                 0

#define GN_FIND_GROUP_found                       0
#define GN_FIND_GROUP_foundNoChange             341
#define GN_FIND_GROUP_notFound                  342

#define GN_FIND_ATTRIBUTE_found                   0
#define GN_FIND_ATTRIBUTE_notFound              351
#define GN_FIND_ATTRIBUTE_dmiBroke              356

#define GN_FIND_COMPONENT_found                   0
#define GN_FIND_COMPONENT_foundEqualToCompi     361
#define GN_FIND_COMPONENT_foundNextAfterCompi   362
#define GN_FIND_COMPONENT_notFound              363
#define GN_FIND_COMPONENT_dmiBroke              366

#define GN_FIND_KEY_found                         0
#define GN_FIND_KEY_noKeyExists                 371
#define GN_FIND_KEY_notFound                    372

#define GN_GET_OBJECT_getOk                       0
#define GN_GET_OBJECT_tooBig                    381
#define GN_GET_OBJECT_cnfBufAddressability      382
#define GN_GET_OBJECT_genErr                    385
#define GN_GET_OBJECT_dmiBroke                  386
#define GN_GET_OBJECT_dpiBroke                  387
#define GN_GET_OBJECT_outOfMemory               388

#define END_OF_MIB_VIEW_noError                   0
#define END_OF_MIB_VIEW_dpiBroke                397

#define GET_NEXT_COMP_MIB_noError                 0
#define GET_NEXT_COMP_MIB_tooBig                401
#define GET_NEXT_COMP_MIB_genErr                405
#define GET_NEXT_COMP_MIB_outOfMemory           408

#define GET_KEY_VALUE_noError                     0
#define GET_KEY_VALUE_noneHigher                411
#define GET_KEY_VALUE_otherError                412
#define GET_KEY_VALUE_outOfMemory               418

#define TRANSLATE_DMI_KEY_TO_INTERNAL_noError       0
#define TRANSLATE_DMI_KEY_TO_INTERNAL_otherError  421
#define TRANSLATE_DMI_KEY_TO_INTERNAL_outOfMemory 428

#define GET_DMI_KEY_SIZE_noError                  0
#define GET_DMI_KEY_SIZE_badAttributeType       431

#define GET_INTERNAL_KEY_SIZE_noError             0
#define GET_INTERNAL_KEY_SIZE_otherError        441

#define PULL_KEY_VALUES_noError                   0
#define PULL_KEY_VALUES_noErrorOldListStillGood 451
#define PULL_KEY_VALUES_otherError              452
#define PULL_KEY_VALUES_cannotGetToThem         453
#define PULL_KEY_VALUES_cnfBufAddressability    454
#define PULL_KEY_VALUES_dmiBroke                456
#define PULL_KEY_VALUES_outOfMemory             458

#define TRANSLATE_INTERNAL_KEY_TO_DMI_noError      0
#define TRANSLATE_INTERNAL_KEY_TO_DMI_otherError 461

#define COMPARE_KEY_VALUES_equal                  0
#define COMPARE_KEY_VALUES_lower                471
#define COMPARE_KEY_VALUES_higher               472
#define COMPARE_KEY_VALUES_error                473

#define FILL_DUMMY_KEY_noError                    0
#define FILL_DUMMY_KEY_otherError               481
#define FILL_DUMMY_KEY_outOfMemory              488

#define DO_SET_noError                            0    /* SET return codes (500's, 600's)*/
#define DO_SET_dpiBroke                         507

#define SET_ATTRIBUTE_noError                     0
/*#define SET_ATTRIBUTE_commitFailed                                  */
/*#define SET_ATTRIBUTE_undoFailed                                    */
/*#define SET_ATTRIBUTE_noAccess                                      */
/*#define SET_ATTRIBUTE_noCreation                                    */
/*#define SET_ATTRIBUTE_notWritable                                   */
/*#define SET_ATTRIBUTE_wrongType                                     */
/*#define SET_ATTRIBUTE_wrongEncoding                                 */
/*#define SET_ATTRIBUTE_wrongLength                                   */
/*#define SET_ATTRIBUTE_wrongValue                                    */
/*#define SET_ATTRIBUTE_inconsistentName                              */
/*#define SET_ATTRIBUTE_inconsistentValue                             */
/*#define SET_ATTRIBUTE_resourceUnavailable                           */
#define SET_ATTRIBUTE_otherError                551
#define SET_ATTRIBUTE_badPacketType             552
#define SET_ATTRIBUTE_genErr                    555
#define SET_ATTRIBUTE_dmiBroke                  556

#define SET_EXTRACT_TYPE_noError                  0
#define SET_EXTRACT_TYPE_unexpectedType         561

#define SET_CHECK_ILLEGAL_noError                 0
#define SET_CHECK_ILLEGAL_gotError              571
#define SET_CHECK_ILLEGAL_genErr                575
#define SET_CHECK_ILLEGAL_dmiBroke              576

#define SET_COMP_MIB_noError                      0
#define SET_COMP_MIB_commitFailed               581
#define SET_COMP_MIB_undoFailed                 582
#define SET_COMP_MIB_noAccess                   583
#define SET_COMP_MIB_noCreation                 584
#define SET_COMP_MIB_notWritable                591
#define SET_COMP_MIB_wrongType                  592
/*#define SET_COMP_MIB_wrongEncoding                                  */
#define SET_COMP_MIB_wrongLength                593
#define SET_COMP_MIB_wrongValue                 594
/*#define SET_COMP_MIB_inconsistentName                               */
/*#define SET_COMP_MIB_inconsistentValue                              */
#define SET_COMP_MIB_resourceUnavailable        601
#define SET_COMP_MIB_badPacketType              602
#define SET_COMP_MIB_genErr                     605
#define SET_COMP_MIB_dmiBroke                   606

#define SET_ADD_TO_RESERVE_LIST_noError           0
#define SET_ADD_TO_RESERVE_LIST_outOfMemory     618             /* added 950317.gwl */

#define SET_REMOVE_FROM_RESERVE_LIST_noError      0

#define SET_CLEAN_RESERVE_LIST_noError            0

#define BUILD_XLATE_noError                       0    /* XlateList return codes  (700's)*/

#define BUILD_MAP_noError                         0

#define PROCESS_FILE_noError                      0

#define CREAT_BUFFER_noError                      0
#define CREAT_BUFFER_invalidOid                 711
#define CREAT_BUFFER_outOfMemory                718

#define EXTRACT_OID_noError                       0
#define EXTRACT_OID_invalidOid                  721
#define EXTRACT_OID_outOfMemory                 728

#define ADD_XLATE_noError                         0
#define ADD_XLATE_outOfMemory                   738

#define BUILD_OID_LIST_noError                    0
#define BUILD_OID_LIST_outOfMemory              748

#define CREATE_COMP_LIST_noError                  0
#define CREATE_COMP_LIST_dmiBroke               756
#define CREATE_COMP_LIST_outOfMemory            758

#define CREATE_GC_PAIRS_noError                   0
#define CREATE_GC_PAIRS_dmiBroke                766
#define CREATE_GC_PAIRS_outOfMemory             768

#define GET_GC_PAIRS_noError                      0
#define GET_GC_PAIRS_dmiBroke                   776
#define GET_GC_PAIRS_outOfMemory                778

#define DO_DMI_SETUP_noError                      0    /* Low-level routines (800's, 900's)*/
#define DO_DMI_SETUP_createSemFailed            801
#define DO_DMI_SETUP_createQueueFailed          802

#define DMI_INDICATE_SETUP_noError                0

#define PREPARE_FOR_DMI_INVOKE_noError            0
#define PREPARE_FOR_DMI_INVOKE_resetSemFailed   821

#define DMI_REGISTER_noError                      0
#define DMI_REGISTER_failed                     831
#define DMI_REGISTER_badLevelCheck              832
#define DMI_REGISTER_badCmdHandle               833
#define DMI_REGISTER_outOfMemory                838

#define DMI_UNREGISTER_noError                    0
#define DMI_UNREGISTER_failed                   841
#define DMI_UNREGISTER_outOfMemory              848

#define PUT_DMI_RESPONSE_noError                       0
#define PUT_DMI_RESPONSE_writeQErr               -1
#define PUT_DMI_RESPONSE_postSemErr              -2

#define AWAIT_DMI_RESPONSE_noError                0
#define AWAIT_DMI_RESPONSE_interruptSem         861
#define AWAIT_DMI_RESPONSE_postSemError         862
#define AWAIT_DMI_RESPONSE_otherSemError        863
#define AWAIT_DMI_RESPONSE_readQueueFailed      864
#define AWAIT_DMI_RESPONSE_otherQueueError      865
#define AWAIT_DMI_RESPONSE_timeoutSem           866 /*(dmiBroke assumed, so xx6 is appropriate)*/

/*For the translation of DPI keys to DMI keys                         */
#define KEY_FROM_DPI_noError                      0
#define KEY_FROM_DPI_tooShort                   871
#define KEY_FROM_DPI_tooLong                    872
#define KEY_FROM_DPI_illegalKey                 873
#define KEY_FROM_DPI_otherError                 874
#define KEY_FROM_DPI_outOfMemory                878

#define KEY_TO_DPI_noError                        0
#define KEY_TO_DPI_otherError                   881
#define KEY_TO_DPI_outOfMemory                  888

#define XLATE_TYPE_noError                        0
#define XLATE_TYPE_noError_opaqueType           891
#define XLATE_TYPE_unexpectedType               892
#define XLATE_TYPE_mifUnknownType               893
#define XLATE_TYPE_otherError                   894

#define ISSUE_LIST_COMP_noError                   0
#define ISSUE_LIST_COMP_failed                  900
#define ISSUE_LIST_COMP_badLevelCheck           901
#define ISSUE_LIST_COMP_badCmdHandle            902
#define ISSUE_LIST_COMP_outOfMemory             908

#define ISSUE_LIST_GROUP_noError                  0
#define ISSUE_LIST_GROUP_failed                 910
#define ISSUE_LIST_GROUP_badLevelCheck          911
#define ISSUE_LIST_GROUP_badCmdHandle           912
#define ISSUE_LIST_GROUP_outOfMemory            918

#define ISSUE_LIST_ATTRIBUTE_noError              0
#define ISSUE_LIST_ATTRIBUTE_failed             920
#define ISSUE_LIST_ATTRIBUTE_badLevelCheck      921
#define ISSUE_LIST_ATTRIBUTE_badCmdHandle       922
#define ISSUE_LIST_ATTRIBUTE_outOfMemory        928

#define ISSUE_LIST_DESCRIPTION_noError            0
#define ISSUE_LIST_DESCRIPTION_failed           930
#define ISSUE_LIST_DESCRIPTION_badLevelCheck    931
#define ISSUE_LIST_DESCRIPTION_badCmdHandle     932
#define ISSUE_LIST_DESCRIPTION_outOfMemory      938

#define ISSUE_GET_ATTRIBUTE_noError               0
#define ISSUE_GET_ATTRIBUTE_failed              940
#define ISSUE_GET_ATTRIBUTE_badLevelCheck       941
#define ISSUE_GET_ATTRIBUTE_badCmdHandle        942
#define ISSUE_GET_ATTRIBUTE_outOfMemory         948

#define ISSUE_GET_ROW_noError                     0
#define ISSUE_GET_ROW_failed                    950
#define ISSUE_GET_ROW_badLevelCheck             951
#define ISSUE_GET_ROW_badCmdHandle              952
#define ISSUE_GET_ROW_outOfMemory               958

#define BUILD_SET_ATTRIBUTE_noError               0
#define BUILD_SET_ATTRIBUTE_failed              960
#define BUILD_SET_ATTRIBUTE_outOfMemory         968

#define ISSUE_SET_ATTRIBUTE_noError               0
#define ISSUE_SET_ATTRIBUTE_failed              970
#define ISSUE_SET_ATTRIBUTE_badLevelCheck       971
#define ISSUE_SET_ATTRIBUTE_badCmdHandle        972
#define ISSUE_SET_ATTRIBUTE_outOfMemory         978

#define FIND_ATTRIBUTE_ACCESS_noError             0
#define FIND_ATTRIBUTE_ACCESS_notFound          981
#define FIND_ATTRIBUTE_ACCESS_otherError        982
#define FIND_ATTRIBUTE_ACCESS_dmiBroke          986

#define FIND_ATTRIBUTE_TYPE_noError               0
#define FIND_ATTRIBUTE_TYPE_notFound            991
#define FIND_ATTRIBUTE_TYPE_otherError          992
#define FIND_ATTRIBUTE_TYPE_dmiBroke            996

#define TRACE_KEY_noError                         0
#define TRACE_KEY_outOfMemory                  1008

#define SUBSCRIBE_WITH_SP_noError                 0
#define SUBSCRIBE_WITH_SP_Error                   1

#define ADD_INDFILTER_TO_SP_noError               0
#define ADD_INDFILTER_TO_SP_Error                 1 

#ifndef OS2
typedef unsigned char BYTE;
#endif

typedef struct _dmiInt64{ /* 8 byte integer, used for signed & unsigned */
      unsigned long int low;                 /*    - Least Significant  */
      unsigned long int high;                /*    - Most Significant   */
} DmiInt64Str;            /* 8 byte integer, used for signed & unsigned */

typedef struct _keyData{
    ULONG        Length;       /* length of this element, including the value...*/
    DMI_UNSIGNED iAttributeId;
    DMI_UNSIGNED iAttributeType;
    USHORT       KeyLength;       /* length of the key data           */
    BYTE         KeyValue[1];     /* this is either a MIF string or a numeric value...*/
} *KeyChainPtr,KeyChainStr;

typedef struct _keyList{
    ULONG Length;      /* total length of the tag list                */
    ULONG KeyCount;    /* total number of keys in this list           */
    KeyChainStr FirstKey;
} KeyListStr, *KeyListPtr;

typedef struct _oidprefix{
    struct _oidprefix *pNextOidPre;
    struct _gcpair    *pNextGCPair;  /* ptr. to first Group/Component pair*/
    int  regTreeIndex;
    char *pOidPrefix;
    ULONG iRegFlag;   /* registered with DMI ???                      */
    ULONG iCompNameCt;
    int   sequentialKeys; /* nonzero means assume keys are sequential */
    char *pCompName;  /* This is the 1st; others may follow.          */
                      /* Use ptr to array of char ptrs.               */
}OidPrefix_t;

typedef struct _gcpair{
    struct _gcpair *pNextGCPair;
    ULONG iGroupId;
    ULONG iCompId;      /* component ID                               */
    ULONG iKeyCount;
}GCPair_t, *GCPair_p;  /* Group/Component pair                        */

typedef struct thr_param{
unsigned long compid;
char          *compname;
OidPrefix_t   *xlatelist;
}Thr_Param;

typedef struct _keypair{
    ULONG iKeyAttrId;
    ULONG iKeyAttrType;
}GCKey_t, *GCKey_p;

typedef struct _keyvaluelist{                /* used for getnext      */
    struct _keyvaluelist *pNextValue;
}KeyValueList_t;

typedef struct _reservelist{
    struct _reservelist   *Next;
    char                  *OidPtr;      /* ptr. to OID of the object  */
    time_t                 TimeReserved;
    ULONG                  AttributeType;
    DmiSetAttributeIN     *DmiRequestBlock;
}ReserveList_t;

typedef struct queue_entry {
   PVOID pReqBuf;
   PVOID pQueueEntry;   /* same value as pointer to the queue entry (pQueueEntry)*/
#ifndef OS2
   PVOID pNext;
#endif
}QueueEntryStr, *QueueEntryPtr;


extern OidPrefix_t    *XlateList;
extern KeyValueList_t *KeyValueList;
extern ReserveList_t  *ReserveList;        /* Set to NULL if no list exists  */
extern int             WatchReserveListFlag; /* set to nonzero if there is a ReserveList thread */

extern char   *ResponseBuffer;             /* buffer used to issue our requests*/
extern ULONG   DmiHandle;                   /* application handle returned by the service layer*/
extern char    SpecLevel[256];             /* buffer used for the spec level of the service layer*/
extern char    SLDescription[256];         /* implmentation description      */
extern char    SP_hostname[256];           /* The hostname of the SP */
extern ULONG   CompCounter,GroupCounter,AttributeCounter,AttrCounter;
extern ULONG   SComponent,SGroup,SAttribute;
extern snmp_dpi_u64   value4 ;

extern int     ExceptionProcFlag;
extern int     Dmi_Registered_Flag;  /* used to tolerate an  rc=300 on 1st DosResetEventSem*/
extern int     Dpi_Registered_Flag;
extern int     Dpi_Opened_Flag;

/*This is relocated to dmisa.c as it gives multiply defined errors */
/*#ifndef WIN32 
extern int     Connected = FALSE;
#endif*/

extern int     DmiState;
extern unsigned long    DpiAgentWaitTimeout;
extern int     DpiWaitForAgentTimeout;
extern int     DpiRegTimeout;
extern int     DpiHandle;
extern ULONG   DmiReqHandle;

#ifdef WIN32			/* LLR 09-11-95 added section for NT              */
#define QUE_NAME "DMISA.QUE"
extern PID     OwningPID;    /* PID of queue owner */
extern ULONG   ElementCode;  /* Request a particular element */
extern BOOL    NoWait;       /* No wait if queue is empty */
extern BYTE    ElemPriority; /* Priority of element being added */

extern UCHAR   SemName[40];  /* Semaphore name */
extern HEV     IndSemHandle;    /* Indication buffering event1 semaphore handle, added 950221.gwl */
extern HEV     queueMutex;    /* Confirm buffering event semaphore handle */
extern HEV     Ind3SemHandle;   /* Indication buffering event2 semaphore handle, added 950221.gwl */
extern HMTX    DmiAccessMutex;  /* Mutual exclusion semaphore handle */
extern ULONG   flAttr;       /* Creation attributes */
extern BOOL    fState;       /* Initial state of the semaphore */
extern ULONG   ulPostCt;     /* Post count for the event semaphore */
extern DWORD   ulIndTimeout; /* Number of milliseconds to wait for Indicate to complete */
extern QueueEntryPtr   pQueueHead;     /* head of confirm queue  */


#elif defined OS2
#define QUE_NAME "\\QUEUES\\DMISA.QUE"
extern PID     OwningPID;    /* PID of queue owner */
extern HQUEUE  QueueHandle;  /* Queue handle */
extern REQUESTDATA    QRequest;   /* Request-identification data */
extern ULONG   ElementCode;  /* Request a particular element */
extern BOOL32  NoWait;       /* No wait if queue is empty */
extern BYTE    ElemPriority; /* Priority of element being added */

extern UCHAR   SemName[40];  /* Semaphore name */
extern HEV     CnfSemHandle;    /* Confirm buffering event semaphore handle */
extern HEV     IndSemHandle;    /* Indication buffering event1 semaphore handle, added 950221.gwl */
extern HEV     Ind3SemHandle;   /* Indication buffering event2 semaphore handle, added 950221.gwl */
extern HMTX    DmiAccessMutex;  /* Mutual exclusion semaphore handle */
extern ULONG   flAttr;       /* Creation attributes */
extern BOOL32  fState;       /* Initial state of the semaphore */
extern ULONG   ulPostCt;     /* Post count for the event semaphore */
#else
extern mutex_t DmiAccessMutex; /* Mutual exclusion semaphore handle */
extern QueueEntryPtr   pQueueHead;     /* head of confirm queue  */
extern mutex_t queueMutex;     /* Mutual exclusion semaphore handle */
extern cond_t  queueCond;      /* Event semaphore handle */
extern mutex_t indicateMutex;                 /* mutex semaphore */
extern cond_t  indicateCond;                  /* condition semaphore */
#endif
extern ULONG   ulDmiTimeout; /* Number of milliseconds to wait for DMI response */
extern ULONG   ulIndTimeout; /* Number of milliseconds to wait for Indicate to complete */
extern ULONG   ulIndWaitTimeout; /* Number of milliseconds to wait for Indicate to arrive */

/*********************************************************************/
/* FUNCTION PROTOTYPES                                               */
/*********************************************************************/
extern int  dmiRegister(ULONG *istatus);
extern int  dmiUnregister(int exceptionprocflag, ULONG DmiReqHandle);
extern int  doDmiSetup(void);
extern void doSemCleanup(void);

extern int  buildXlate(OidPrefix_t **ppXlateList);
extern void freeXlate(void);
extern int  addToXlate(int compi, char *CompName, OidPrefix_t *xlatelist);
extern OidPrefix_t *deleteFromXlate(int compi, OidPrefix_t *xlatelist);
extern void         deleteGroupFromXlate(int groupi, OidPrefix_t *xlatelist);
extern char *findOidPrefix(int compi, OidPrefix_t *xlatelist);

extern int  doGet(snmp_dpi_hdr *phdr, snmp_dpi_get_packet *ppack, OidPrefix_t *XlateList);
extern int  dpiGetCompMib(snmp_dpi_set_packet **ppvarbind,snmp_dpi_get_packet *ppack);
extern int  doGetNext(snmp_dpi_hdr *phdr, snmp_dpi_next_packet *ppack, OidPrefix_t *XlateList);
extern int  dpiGetNextCompMib(snmp_dpi_set_packet **ppvarbind,snmp_dpi_next_packet *ppack);
extern int  doSet(snmp_dpi_hdr *phdr, snmp_dpi_set_packet *ppack, OidPrefix_t *XlateList);
extern int  dpiSetCompMib(snmp_dpi_set_packet **ppvarbind,snmp_dpi_set_packet *ppack);
extern int  getParseInstance(char *instance, ULONG *groupi, ULONG *attributei,
        ULONG *compi, char **keystring);
extern int  getFindGCPair(GCPair_t **GCPair, OidPrefix_t *xlatelist, char *pgroup,
        ULONG groupi, ULONG compi);
extern int  getTranslateKey(GCPair_t *gcpair, char *keystring, ULONG *keyblocklen,
        DMI_GroupKeyData_t **ppkeyblock);

extern void swap64(char *InValue,char *OutValue);

extern int  issueListComp(DmiListComponentsOUT **listcompout, ULONG dummy, ULONG compi, SHORT flag,
        ULONG maxCount);
extern int  issueListGroup(DmiListGroupsOUT **listgroupout, ULONG compi, ULONG groupi, SHORT flag,
        ULONG maxCount);
extern int  issueListAttribute(DmiListAttributesOUT **listattrout , ULONG compi, ULONG groupi,
        ULONG attribi, SHORT flag, ULONG maxCount);
extern int  issueListDesc(char * description, ULONG compi, ULONG groupi, ULONG attri,
        SHORT flag);
extern int  issueGetAttribute(DmiGetAttributeOUT **getattrout, ULONG compi, ULONG groupi, ULONG attribi,
        ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *keyblock);
extern int  issueGetRow(DmiGetMultipleOUT **getmultout, ULONG compi, ULONG groupi, ULONG attribi,
        ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *keyblock, SHORT flag);
extern int  buildSetAttribute(DmiSetAttributeIN **setattribin , ULONG groupi,
        ULONG attribi, ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *keyblock,
        ULONG attribvaluelen, ULONG attributetype, void *attributevalue);
extern int  issueSetAttribute(DmiSetAttributeIN **setattribin, ULONG compi, ULONG cmd);
extern int  findAttributeAccess(int *access, ULONG compi, ULONG groupi, ULONG attributei);
extern int  findAttributeType(ULONG *type, ULONG compi, ULONG groupi, ULONG attributei);

extern int  keyFromDpi(GCPair_p gcptr, char *oidkey, DMI_GroupKeyData_t **dmikey, ULONG *dmikeylen);
extern int  keyToDpi(DMI_GroupKeyData_t *dmikey, int numofkeys, char **oidkey);
extern void freeKeyValueList(void);
extern int  getKeyValue(int command, GCPair_t *gcpair, DMI_GroupKeyData_t *pattern,
        DMI_GroupKeyData_t **wantedkey, ULONG *keysize);
extern int getSequentialKeyValue(int command, GCPair_t *gcpair,
            DMI_GroupKeyData_t *pattern, DMI_GroupKeyData_t **wantedkey,
            ULONG *pkeysize);
extern int  setCleanReserveList(int mode);
extern int  xlateType(ULONG *outtype, ULONG inType, int toDpiOrDmi, ULONG *length);
extern ULONG dmiLength(int intype);
extern int  prepareForDmiInvoke(void);
extern int  awaitDmiResponse(QueueEntryStr **ppqueueentry);
extern void pitoa(ULONG n, char s[]);  /* Translate ULONG integer to ascii - */
                                /* Character string s MUST be 11 or more characters in length*/
extern void APIENTRY dmiConfirmHandler(void *);
extern void APIENTRY exitRoutine(void);
extern void APIENTRY dmiIndicateHandler (void *indBlock);
#if defined(OS2) || defined(AIX325) || defined(WIN32)
extern void waitForIndication(void);
#else
extern void *waitForIndication(void *nevermind);
#endif
extern int  parseDmiIndicate (DMI_Indicate_t *dmiBlock_prt);
extern void *indBlock;
extern int  dmiIndication (void);
extern int  dmiIndicateSetup (void);
extern int  handleStartTrap (int);

extern int  groupkeydata_to_keylist(DMI_GroupKeyData_t *,DmiAttributeValues_t *, int );
extern void free_listcompout(DmiListComponentsOUT *listcompout); 
extern void free_listgroupout(DmiListGroupsOUT *listgroupout); 
extern void free_listattrout(DmiListAttributesOUT *listattrout); 
extern void free_attrvalues(DmiAttributeValues_t *keylist); 
extern void free_getattrout(DmiGetAttributeOUT *getattrout); 
extern void free_getmultout(DmiGetMultipleOUT *getmultout); 
extern void free_setAttributeIN(DmiSetAttributeIN *setattrin); 
extern void free_listclassnameout(DmiClassNameList_t *classname);

extern int  register_callback_rpc(void );
extern DmiErrorStatus_t  handle_CompLangGrpIndication(int event_type, void *argp);
extern DmiErrorStatus_t  handle_Event_SubsNotice(int event_type, void *argp);
extern int  Ind_SubscribeWithSP();
extern int  Ind_AddFilterToSP();
extern void *thr_addToXlate(void *thr_arg);
extern void *thr_rebuildXlate(void *th_arg);
extern void make_daemon();
#endif

