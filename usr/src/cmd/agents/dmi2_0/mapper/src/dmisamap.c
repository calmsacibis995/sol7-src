/* Copyright 11/15/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisamap.c	1.10 96/11/15 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisamap.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to extract all attribute entries   */
/*  from all .MAP files of the specified format on all local hard     */
/*  disks.                                                            */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        buildMap(...)               */
/*                                        qDisk(...)                  */
/*                                        forAllMatch(...)            */
/*                                        processFile(...)            */
/*                                        creatBuffer(...)            */
/*                                        extractOid(...)             */
/*                                        createMap(...)              */
/*                                        oidCmp(...)                 */
/*                                        getNumber(...)              */
/*                                                                    */
/* End Module Description *********************************************/
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
/*                  940601    PARKJ     New module                    */
/*                  940705    LAUBLI    Added OidCmp performance enh. */
/*                  940711    LAUBLI    Append period to OID prefix   */
/*                  940817    LAUBLI    Close open .MAP files         */
/*                  940817    LAUBLI    Added error handling, tracing */
/*                  941109    LAUBLI    Added freeing of buffers      */
/*                  941109    LAUBLI    Added trace for some condit'ns*/
/*                  950207    LAUBLI    Added trace on .MAP search    */
/*                  950209    LAUBLI    Fixed a minor memory leak by  */
/*                                      including good records preceding*/
/*                                      a bad record in a file.       */
/* End Change Log *****************************************************/
#define        INCL_DOSFILEMGR     /* File Manager Values */
#define        INCL_DOSMISC
#include <ctype.h>

#include "dmisa.h"
/* error handling, added 8/17/94                                      */
#include "dmisamap.h"
#include <stddef.h>
#ifdef OS2
#elif defined WIN32
#elif defined SOLARIS2
#include <dirent.h>
#else
#undef offsetof
#define offsetof        __offsetof
#include <dirent.h>
#endif

extern char logbuffer[];          /* tracing, added 8/17/94  */
#ifdef DMISA_TRACE
extern unsigned char logbuffer[];          /* tracing, added 8/17/94  */
#endif

static FILE *inputFile;
static char inputLine[1000];
static char origLine[1000];

struct _line *mapPtr;
struct _line *mapTail;
struct _line *bufTail;
struct _line *bufPtr;

char   *oid;         /* null terminated */
int    sequentialKeys;
int    field2;
int    field3;
int    field4;
char   * compo_name; /* null terminated */
int    key_count;
char   * key;        /* null terminated */

static int  qDisk(int i);
static int  forAllMatch(char * );
static int  processFile(char *);
static int  creatBuffer(char *inputLine);
static int  extractOid(char **oid, char **inputLine);
static void creatMap(void);
static int  oidCmp(char *A,char *B);
static int  getNumber(char **oid);
#ifdef OS2
static void GetEnvPath(char *stringin,char *stringout);
#endif

#define DISABLE_ERRORPOPUPS 0x00000002
#define ENABLE_ERRORPOPUPS 0x00000001
#define INVALID_OID 1

#ifdef OS2
static  FILEFINDBUF3  findBuffer;
static  char fileMask[255];
static  char searchPath[260];

   /*********************/
   /* Options variables */
   /*********************/

static  int dirFlag     = 0;

char           drvtb[] = {
               'a',
               'b',
               'c',
               'd',
               'e',
               'f',
               'g',
               'h',
               'i',
               'j',
               'k',
               'l',
               'm',
               'n',
               'o',
               'p',
               'q',
               'r',
               's',
               't',
               'u',
               'v',
               'w',
               'x',
               'y',
               'z'};


UCHAR          DeviceName[8];
ULONG          Ordinal;
ULONG          FSAInfoLevel;
ULONG          FSInfoLevel;
FSQBUFFER2     DataBuffer;
ULONG          DataBufferLen;
UCHAR          FSInfoBuf[100];
ULONG          FSInfoBufSize;

struct FSINFO_LEVEL1
{
   ULONG  FileSystemID;
   ULONG  SectorNum;
   ULONG  UnitNum;
   ULONG  UnitAvail;
   USHORT BytesNum;

}  FSInfo_Level1;

PVOID pFSInfoL1 = &FSInfo_Level1;
char   Environment[512];

#elif defined WIN32
static  WIN32_FIND_DATA  findBuffer;
static  char fileMask[255];

char   Environment[512];
#endif

/**********************************************************************/
/*                          BEGINNING OF CODE                         */
/**********************************************************************/
/* Function:  buildMap()                                              */
/*                                                                    */
/* Note: Caller of this function must free storage in linked list     */
/*       passed back by this function in *mapPtr1.                    */
/**********************************************************************/
int buildMap (struct _line **mapPtr1)
   {
   struct _line * aPtr;
   struct _line * bPtr;
   int   rc;
   int   drivenum;
#ifdef DMISA_TRACE
   int   ln;
#endif
#ifdef WIN32
   WIN32_FIND_DATA  FindBuffer;
   BOOL   fFoundLocalMapFile = FALSE;
   HANDLE FileHandle;
   char MapFileName[256];
   int stempline;

#elif defined OS2
   HDIR FileHandle = 0x0001;
   ULONG FindCount = 1;
   char MapFileName[256];
   FILEFINDBUF3 FindBuffer;
   BOOL fFoundLocalMapFile = FALSE;
#elif defined SOLARIS2
   char newPath[256];
   DIR *DirectoryPointer;
   struct dirent *entry;
   struct dirent *Result;
   char *pdot;
#else
   char newPath[256];
   DIR *DirectoryPointer;
   struct dirent Entry;
   struct dirent *Result;
   char *pdot;
#endif
   struct _line templine; /* $MED */

   mapPtr = NULL;
   mapTail = NULL;
#ifdef WIN32

   memset(Environment,0,512);
   GetPrivateProfileString("dmica", "NETVIEW_PATH","", Environment,
                            sizeof(Environment),"sva.ini");

   SetCurrentDirectory(Environment);
   strcat(Environment,"\\");
   strcat(Environment,"bin");
   strcat(Environment,"\\");
   strcat(Environment,"agent");
   strcat(Environment,"\\");
/**********************************************************************************/
/*                                                                                */
/* The FindNextFile is not working correctly.  I have hard coded the map file to  */
/* sva.ini .  At a later time this code can be updated to read as input multiple  */
/* files.                                                                         */
/* LLR 09-28-95                                                                   */
/**********************************************************************************/

   DMISA_TRACE_LOG(LEVEL2,"Looking for map in DMIPATH directory");
   sprintf(MapFileName,"%ssva.map",Environment);
//   sprintf(MapFileName,"%s*.map",Environment);

   FileHandle = FindFirstFile(MapFileName, &FindBuffer);
   if (FileHandle != ((TID) -1) )
      rc = 0;
   else
      rc = 1;

    if (rc == 0) {
       sprintf(MapFileName,"%s%s",Environment,FindBuffer.cFileName);
       fFoundLocalMapFile = TRUE;
       DMISA_TRACE_LOG(LEVEL2,"Found a mapfile in DMIPATH directory!");
       rc = processFile(MapFileName);
    }

//   while(rc == 0){   /* we have a good return code from the find function */
//       sprintf(MapFileName,"%s%s",Environment,FindBuffer.cFileName);
//       fFoundLocalMapFile = TRUE;
//       DMISA_TRACE_LOG(LEVEL2,"Found a mapfile in DMIPATH directory!");
//       rc = processFile(MapFileName);
//       if (rc != PROCESS_FILE_noError) return rc;
//       rc = FindNextFile (FileHandle, &FindBuffer);
//   }

   if (fFoundLocalMapFile){
     FindClose(FileHandle);
   }


#elif defined OS2
   dirFlag = 1;

   memset(Environment,0,512);
   GetEnvPath("NETVIEW_PATH",Environment);
   rc = DosSetCurrentDir(Environment);
   strcat(Environment,"\\");
   strcat(Environment,"bin");
   strcat(Environment,"\\");
   strcat(Environment,"agent");
   strcat(Environment,"\\");

   DMISA_TRACE_LOG(LEVEL2,"Looking for map in DMIPATH directory");
   sprintf(MapFileName,"%s\*.map",Environment);
   rc = DosFindFirst(MapFileName,&FileHandle,0,(PVOID)&FindBuffer,sizeof(FindBuffer),&FindCount,FIL_STANDARD);
   while(rc == 0){   /* we have a good return code from the find function */
       sprintf(MapFileName,"%s%s",Environment,FindBuffer.achName);
       fFoundLocalMapFile = TRUE;
       DMISA_TRACE_LOG(LEVEL2,"Found a mapfile in DMIPATH directory!");
       rc = processFile(MapFileName);
       if (rc != PROCESS_FILE_noError) return rc;
       rc = DosFindNext(FileHandle,(PVOID)&FindBuffer,sizeof(FindBuffer),&FindCount);
   }

   if (!fFoundLocalMapFile) { /* couldn't find any local map-files... */
      for ( drivenum = 1; drivenum < 27; drivenum++ ) { /* Look through all local disks (hrd & flpy)*/
         rc = qDisk( drivenum );
      }
   }
#elif defined SOLARIS2
#if 0
   DirectoryPointer = opendir("."); /* search current directory */
#endif
   DirectoryPointer = opendir("/var/dmi/map"); /* search current directory */
   Result = (struct dirent *)malloc(sizeof(struct dirent) +_POSIX_PATH_MAX);
   entry = readdir_r(DirectoryPointer, Result);
   while (entry != NULL&& Result != NULL) {
       pdot = strchr(Result->d_name,'.');
       if ( (pdot != NULL) /* there's a dot */
         && (pdot == Result->d_name + strlen( Result->d_name)- 4)
         && (!strncasecmp(pdot,".MAP",4)) ) {
            memset(newPath, 0, sizeof(newPath));
            sprintf(newPath,"%s/%s",
               "/var/dmi/map",
               Result->d_name);
            
            processFile(newPath);
#if 0
            processFile(Result->d_name);
#endif
       }
       entry = readdir_r(DirectoryPointer, Result);
   } /* endwhile */
   rc =closedir(DirectoryPointer);
   if (Result) free(Result);
#else
   DirectoryPointer = opendir("."); /* search current directory */

   rc =readdir_r(DirectoryPointer, &Entry, &Result);
   while (rc == 0 && Result != NULL) {
       pdot = strchr(Result->d_name,'.');
       if ( (pdot != NULL) /* there's a dot */
         && (pdot == Result->d_name + Result->d_namlen - 4)
         && (!strcasecmp(pdot,".MAP",4)) )
            processFile(Result->d_name);
       rc = readdir_r(DirectoryPointer, &Entry, &Result);
   } /* endwhile */
   rc =closedir(DirectoryPointer);
#endif

#ifdef WIN32
   *mapPtr1 =  mapPtr;                           /* LLR 09-18-95 correct return variable handling */
#else
   mapPtr1[0] = mapPtr;
#endif

   if ( mapPtr == NULL ) return BUILD_MAP_noError;

   aPtr = mapPtr;
   while ( aPtr->next != NULL ) {
      bPtr = aPtr->next;
      while ( bPtr != NULL ) {
         rc = oidCmp(aPtr->oid,bPtr->oid);
         if ( rc > 0 ) {
   #ifdef WIN32
            stempline = (size_t) (((size_t)sizeof(templine)) - ((size_t) sizeof(struct _line *)));

            memcpy(&templine, aPtr, stempline);
            memcpy(aPtr, bPtr, stempline);
            memcpy(bPtr, &templine, stempline);
        #else
            memcpy(&templine, aPtr, offsetof(struct _line,next));
            memcpy(aPtr, bPtr, offsetof(struct _line,next));
            memcpy(bPtr, &templine, offsetof(struct _line,next));
        #endif
         }
         bPtr = bPtr->next;
      }
      aPtr = aPtr->next;
   }
#ifdef DMISA_TRACE
   DMISA_TRACE_LOG(LEVEL4,"Sorted .MAP list:"); /* tracing, added 8/17/94 */
   while (mapPtr != NULL ) {
      ln = sprintf(logbuffer,"%d %d %d %d %d ", mapPtr->sequentialKeys,
                                                mapPtr->field2,
                                                mapPtr->field3,
                                                mapPtr->field4,
                                                mapPtr->key_count);
      strncpy(logbuffer + ln, mapPtr->oid, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      ln += strlen(mapPtr->oid);
      strncpy(logbuffer + ln, " ", (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      ln += strlen(" ");
      strncpy(logbuffer + ln, mapPtr->compo_name, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN -ln : 0);
      DMISA_TRACE_LOGBUF(LEVEL3);
      mapPtr = mapPtr->next;
   }
#endif
   return BUILD_MAP_noError;
}

#ifdef OS2
/**********************************************************************/
/* Function:  qDisk()                                                 */
/*                                                                    */
/* Description:                                                       */
/**********************************************************************/
static int qDisk(int drive_num) {

APIRET         rc;

   DosError(DISABLE_ERRORPOPUPS);
   rc = DosQueryFSInfo((ULONG)drive_num,
                   (ULONG)FSIL_ALLOC,
                   (PVOID)pFSInfoL1,
                   (ULONG)18L);

   DosError(ENABLE_ERRORPOPUPS);

   if ( rc == 0 ) {
      DeviceName[0] = drvtb[drive_num-1];
      DeviceName[1] = '\0';

      strcat(DeviceName,":");

      FSAInfoLevel = 1;

      DataBufferLen = sizeof(FSQBUFFER2);

      rc = DosQueryFSAttach(DeviceName,Ordinal,
                FSAInfoLevel,&DataBuffer,&DataBufferLen);
      if ( DataBuffer.iType == 4 ) {
         DMISA_TRACE_LOG1(LEVEL2,"%c: -- remote drive.",drvtb[drive_num-1]);
      } else {
         strcpy(fileMask, "*.map");
         strcat(DeviceName,"\\");
         strcpy(searchPath, DeviceName);
         DMISA_TRACE_LOG1(LEVEL2,"Searching drive %c:",drvtb[drive_num-1]);
         rc = forAllMatch(searchPath);
      }
   }
   return 0;
}

                /****************************************/
                /*           forAllMatch                */
                /*                                      */
                /* Go through each file that match the  */
                /* file mask.                           */
                /*                                      */
                /* Do subdirectory search if requested. */
                /*                                      */
                /****************************************/

/**********************************************************************/
/* Function:  forAllMatch()                                           */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/* Return codes: 0 if no error                                        */
/*               rc from DosFindFirst function if error               */
/*               rc from subfunction                                  */
/**********************************************************************/
int forAllMatch(char * path)
{
   HDIR         findHandle  = 0xFFFFFFFF;  /* Let OS/2 get us the handle */
   int          findCount   = 1;
   USHORT       rc;
   char         newPath[256];

   /*  Recursively process all subdirectory  */

   if ( path[strlen(path)-1] == ':' || path[strlen(path)-1] == '\\' )
      strcat (path,"*.*");
   else
      strcat (path,"\\*.*");

   if (dirFlag)  /* Subdirectory search requested */
   {
      rc = DosFindFirst(path        ,
                        &findHandle ,
                        FILE_DIRECTORY  ,
                        &findBuffer ,
                        (ULONG)sizeof(findBuffer) ,
                        (PULONG)&findCount ,
                        (ULONG)0x0001);

      if (rc)
         {
         DMISA_TRACE_LOG2(LEVEL4, "FindFirst failed:  Path = %s, rc = %d", path, rc); // added 950207.gwl
         return rc;
         }

      while (!rc)
         {
         if (strcmp(findBuffer.achName,".") && strcmp(findBuffer.achName,".."))
            {
            if (findBuffer.attrFile  == FILE_DIRECTORY )
               {
               strcpy(newPath,path) ;
               newPath[strlen(path)-3] = '\0';
               strcat(newPath,findBuffer.achName) ;
               forAllMatch(newPath)  ;
               }
            }
         rc = DosFindNext(findHandle,
                          &findBuffer,
                          sizeof(findBuffer),
                          (PULONG) &findCount);
         }
      }

   /* When we get here, we have in path, the path to the directory we are  */
   /* going to work with.                                                  */
   /* Process all normal files that match the fileMask in the current directory */

   findHandle  = 0xFFFFFFFF;

   strcpy(newPath,path) ;

   newPath[strlen(path)-3] = '\0';

   strcat(newPath,fileMask);

   findCount = 1 ;

   rc = DosFindFirst(newPath,
                     &findHandle,
                     (ULONG)0L,
                     &findBuffer,
                     (ULONG)sizeof(findBuffer),
                     (PULONG)&findCount,
                     (ULONG)0x0001);

   path[strlen(path)-3] = '\0';

   DMISA_TRACE_LOG2(LEVEL4, "rc = %d in %s", rc, newPath);

   while (!rc)    /* Process each file that matches the mask */
   {
      strcpy(newPath,path);
      strcat(newPath,findBuffer.achName);
#ifdef DMISA_TRACE
      strncpy(logbuffer,newPath, LOG_BUF_LEN);  /* For tracing        */
      DMISA_TRACE_LOGBUF(LEVEL2);
#endif
      rc = processFile(newPath);
      if (rc != PROCESS_FILE_noError) return rc;  /* error handling, added 8/17/94*/
      rc = DosFindNext(findHandle,
                       &findBuffer,
                       sizeof(findBuffer),
                       (PULONG)&findCount);
   }
   return 0;
}
#endif

/**********************************************************************/
/* Function:  processFile()                                           */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/* Return codes: PROCESS_FILE_noError                                 */
/*               CREAT_BUFFER_outOfMemory                             */
/*               EXTRACT_OID_outOfMemory                              */
/**********************************************************************/
int processFile(char * filename)
{
   int rc;

#ifdef DMISA_TRACE
   int ln;

   strncpy(logbuffer,filename,LOG_BUF_LEN);
   DMISA_TRACE_LOGBUF(LEVEL4);
#endif
   bufPtr = NULL;
   if ( (inputFile = fopen(filename,"r") ) != NULL ) {
      memset((char *)&inputLine, 0, sizeof(inputLine));
      while  (fgets(inputLine,1000,inputFile) != NULL )
      {
         strcpy(origLine,inputLine) ;

         inputLine[strlen(inputLine)-1] = '\0' ;
         rc = creatBuffer(inputLine);
         if ( rc != CREAT_BUFFER_noError ) {
            if (rc != CREAT_BUFFER_invalidOid) {      /* changed 050209.gwl */
               DMISA_TRACE_LOG1(LEVEL1,"Error in retrieving data from .MAP file (rc = %d)",rc);
               return rc;    /* Error handling, added 8/17/94    */
            } else {
               DMISA_TRACE_LOG1(LEVEL1,"Error in .MAP file format.",rc);
            }
         }
        memset(&inputLine[0], 0, sizeof(inputLine));
      }

      if (bufPtr != NULL) {
         creatMap();            /*  (bufPtr);                         */
      }
      fclose(inputFile);        /* added 8/15/94 by GWL               */
#ifdef DMISA_TRACE
   } else {
      ln = sprintf(logbuffer,"Unable to open ");
      strncpy(logbuffer + ln, filename, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      DMISA_TRACE_LOG(LEVEL1, logbuffer);
#endif
   }
   return PROCESS_FILE_noError;
}


/**********************************************************************/
/* Function:  creatBuffer()                                           */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/* Notes:                                                             */
/*   1. This function and its subfunction allocate but do not free    */
/*      blocks of storage.  It is the responsibility of the caller to */
/*      ensure the storage will be freed (e.g, by calling freeXlate). */
/*                                                                    */
/* Return codes: CREAT_BUFFER_noError                                 */
/*               CREAT_BUFFER_invalidOid                              */
/*               CREAT_BUFFER_outOfMemory                             */
/*               EXTRACT_OID_outOfMemory                              */
/**********************************************************************/
int creatBuffer(char *inputLine)
{
   int    rc,i;
   char   digit[20];
   char   *cur;
   struct _line* lineInfo;

   /* let's parse the inputline */

   /* these checks for inputline not null are meaningless $MED */

   while ( isspace(*inputLine) && inputLine != NULL) inputLine++;
   if ( *inputLine != '"' ) return CREAT_BUFFER_noError;

   rc = extractOid(&oid, &inputLine);  /* inputLine is altered by extractOid*/
   if ( rc == EXTRACT_OID_outOfMemory ) {
      return rc;                       /* error handling, added 8/17/94*/
   } else if ( rc == EXTRACT_OID_invalidOid ) {
      if (oid) free(oid);
      return CREAT_BUFFER_invalidOid;  /* error handling, added 8/17/94*/
   }

   inputLine++;
#if 0
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   if ((*inputLine == 'N') || (*inputLine == 'n'))
      sequentialKeys = 0;
   else
#endif
      sequentialKeys = 1;


#if 0
   while ( !(isspace(*inputLine)) && inputLine != NULL ) inputLine++;
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;

   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   if ( !isdigit(*inputLine)) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   i = 0;
   while ( isdigit(*inputLine) && inputLine != NULL ) {
      digit[i] = *inputLine;
      inputLine++;
      i++;
   }
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   digit[i] = '\0';
   field2 = atoi(digit);
#endif
   field2 = 0;


#if 0
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   if ( !isdigit(*inputLine)) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   i = 0;
   while ( isdigit(*inputLine) && inputLine != NULL ) {
      digit[i] = *inputLine;
      inputLine++;
      i++;
   }
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   digit[i] = '\0';
   field3 = atoi(digit);
#endif
   field3 = 0;


#if 0
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   if ( !isdigit(*inputLine)) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   i = 0;
   while ( isdigit(*inputLine) && inputLine != NULL ) {
      digit[i] = *inputLine;
      inputLine++;
      i++;
   }
   if (inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   digit[i] = '\0';
   field4 = atoi(digit);
#endif
   field4 = 0;

   /* let's get the component name now */

   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;

   if ( *inputLine != '"' ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   inputLine++;

   cur = inputLine;
   i = 0;
   while ( *inputLine != '"' && inputLine != NULL ) {
      i++;
      inputLine++;
   }

   if ( inputLine == NULL ) {
      free(oid);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   compo_name = malloc(i+1);
   if (!compo_name) {
      free(oid);
      return CREAT_BUFFER_outOfMemory;  /* error handling, added 8/17/94*/
   }
   strncpy(compo_name,cur,i);
   compo_name[i] = '\0';

   inputLine++;


#if 0
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;
   if (inputLine == NULL ) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   if ( !isdigit(*inputLine)) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   i = 0;
   while ( isdigit(*inputLine) && inputLine != NULL ) {
      digit[i] = *inputLine;
      inputLine++;
      i++;
   }
   if (inputLine == NULL ) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }
   digit[i] = '\0';
   key_count = atoi(digit);
#endif
   key_count = 0;
   key = NULL;
 
#if 0
   while ( isspace(*inputLine) && inputLine != NULL ) inputLine++;
   if (inputLine == NULL ) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   if ( !isdigit(*inputLine)) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_invalidOid;  /* changed from -1 941109.GWL  */
   }

   i = strlen(inputLine);
   key = malloc(i+1);
   if (!key) {
      free(oid);
      free(compo_name);
      return CREAT_BUFFER_outOfMemory;       /* error handling, added 8/17/94*/
   }
   strcpy(key,inputLine);
   *(key+i) = '\0';
#endif

   lineInfo = malloc(sizeof ( struct _line ) );
   if (!lineInfo) {
      free(oid);
      free(compo_name);
      free(key);
      return CREAT_BUFFER_outOfMemory;  /* error handling, added 8/17/94*/
   }
   lineInfo->oid = oid;
   lineInfo->compo_name = compo_name;
   lineInfo->key = key;

   lineInfo->sequentialKeys = sequentialKeys;
   lineInfo->field2 = field2;
   lineInfo->field3 = field3;
   lineInfo->field4 = field4;
   lineInfo->key_count = key_count;
   lineInfo->next = NULL;

   if ( bufPtr == NULL ) {
      bufPtr = lineInfo;
      bufTail = lineInfo;
   } else {
      bufTail->next = lineInfo;
      bufTail = bufTail->next;
   }
   lineInfo->next = NULL;

   return CREAT_BUFFER_noError;        /* error handling, added 8/17/94*/
}


/**********************************************************************/
/* Function:  extractOid()                                            */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/* Return codes: EXTRACT_OID_noError                                  */
/*               EXTRACT_OID_invalidOid                               */
/*               EXTRACT_OID_outOfMemory                              */
/**********************************************************************/
int extractOid(char **oid, char **inputLine)
{

   char *ptr;
   int  oidlen, lastcharnotperiod;

   oid[0] = NULL;                               /* added 941109.GWL   */
   while (!isdigit(*inputLine[0]) &&
             inputLine[0] != NULL) (inputLine[0])++; /* go to first digit position */
   if ( inputLine[0] == NULL ) return EXTRACT_OID_invalidOid; /* error handling, changed 8/17/94*/

   ptr = inputLine[0];                          /* remember where we started  */
   oidlen = 0;                               /* count how long an oid is   */
   lastcharnotperiod = 1;                    /* added by GWL*/
   while ( *inputLine[0] != '"' && inputLine[0] != NULL ) {
      if ( isdigit(*inputLine[0]) ) {
         lastcharnotperiod = 1;              /* added by GWL */
         oidlen++;
         (inputLine[0])++;
      } else if ( *inputLine[0] == '.' && lastcharnotperiod) {
         lastcharnotperiod = 0;              /* added by GWL */
         oidlen++;                           /* added by GWL */
         (inputLine[0])++;                        /* added by GWL */
      } else return EXTRACT_OID_invalidOid; /* error handling, changed 8/17/94*/
   }

   if (inputLine[0] == NULL) return EXTRACT_OID_invalidOid; /* error handling, changed 8/17/94*/

   oid[0] = malloc(oidlen + 1 + lastcharnotperiod); /* changed by GWL */
   if (!oid[0]) return EXTRACT_OID_outOfMemory; /* error handling, added 8/17/94*/
   strncpy(oid[0],ptr,(oidlen + lastcharnotperiod));/* changed by GWL */
   *(oid[0]+oidlen+lastcharnotperiod-1) = '.';      /* added by GWL */
   *(oid[0]+oidlen+lastcharnotperiod) = '\0';       /* changed by GWL */
   return EXTRACT_OID_noError;          /* error handling, changed 8/17/94*/
}


/**********************************************************************/
/* Function:  creatMap()                                              */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/**********************************************************************/
void creatMap(void)
{

   if ( mapPtr == NULL )
      mapPtr = bufPtr;
   else
      mapTail->next = bufPtr;

   mapTail = bufTail;
}


/**********************************************************************/
/* Function:  oidCmp()                                                */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/* Return codes:  0  A equals B                                       */
/*               -1  A is less then B                                 */
/*                1  A is greater than B                              */
/**********************************************************************/
int oidCmp(char *A,char *B)
{

   int oidsA[100];
   int oidsB[100];
   int i;

   if (strcmp(A,B) == 0) {
      return(0);                /* added 7/11/94 by GWL */
   }

   for ( i = 0; i < 100; i++ ) {
      oidsA[i] = 0;
      oidsB[i] = 0;
   }

   while( !isdigit(*A) ) A++;

   for ( i = 0; *A != '\0'; i++ )
      oidsA[i] = getNumber(&A);

   while( !isdigit(*B) ) B++;

   for ( i = 0; *B != '\0'; i++ )
      oidsB[i] = getNumber(&B);

   for ( i = 0; i < 100; i++ ) {
      if ( oidsA[i] > oidsB[i] ) return(1);
      if ( oidsA[i] < oidsB[i] ) return(-1);
   }
   return(0);
}


/**********************************************************************/
/* Function:  getNumber()                                             */
/*                                                                    */
/* Description:                                                       */
/*                                                                    */
/**********************************************************************/
int getNumber(char **oid) {
   int cnt;
   char number[20];

 /*  while ( !isdigit( *oid[0]) && oid[0] != NULL ) oid[0]++;         */
 /*  if ( *oid[0] == '\0' ) return;                                   */

   cnt = 0;
   while ( isdigit(*oid[0]) ) {
      number[cnt] = (*oid[0]);
      cnt++;
      oid[0]++;
   }
   oid[0]++; /* to get past the period, added by GWL */
   number[cnt] = '\0';
   return(atoi(number));
}

#ifdef OS2
static void GetEnvPath(char *stringin,char *stringout)
{
    char *value;
    value = getenv(stringin);    /* get value of environment variable */
    if (value == NULL) stringout[0] = 0;   /* not in the env space */
    else strcpy (stringout,value); /* copy environment variable value to output buffer */
}
#endif

