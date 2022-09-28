/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)mif_db.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: mif_io.c

    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: Common MIF I/O Routines

    Author(s): Alvin I. Pivowar
               Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        06/22/93 PAR    Modified for use with OS/2 system level calls
        7/27/93  aip    Fixed file positioning arithmetic.
                        Added casts where appropriate.
        8/5/93   aip    Changed unsigned to unsigned short and
                        int to short.
        12/6/93  aip    Changed unsigned to unsigned short.
		12/08/93 sfh	Modify to build asm/sl/db version.
		12/21/93 sfh	Remove _PASCAL.
        12/21/93 aip    Added call to MIF_headerFlush in MIF_close.
		03/18/94 sfh	Fix lines within OS2 ifdefs broken by some unknown method.
        05/18/94 par    Corrected the copyright notice.
        09/09/94 par    Modified the calls to MIF_cahceFlush() reflect the new
                        options. (WriteThrough)

************************* INCLUDES ***********************************/

#include <stdio.h>
#include <string.h>
#ifndef OS2_SL_BUILD
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#endif
#include "mif_db.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef struct {
            MIF_FileHandle_t fileHandle;
            unsigned short   page;
            MIF_Bool_t       modified;
            char             buffer[MIF_PAGE_SIZE];
        } MIF_CacheTable_t, *MIF_CacheTable_pt;

/*********************************************************************/

/************************ PRIVATE ************************************/

static MIF_Status_t     MIF_headerRead(MIF_FileHandle_t fileHandle);
static MIF_Status_t     MIF_headerWrite(MIF_FileHandle_t fileHandle);
static MIF_Status_t     MIF_cachePage(MIF_FileHandle_t fileHandle,unsigned short pageNumber);
static MIF_CacheTable_t *MIF_lruSet(MIF_CacheTable_t *slot);
static MIF_Status_t     MIF_pageWrite(MIF_FileHandle_t fileHandle, unsigned short pageNumber, char *buffer, unsigned short bufferLength);
static MIF_Status_t     MIF_headerFlush(MIF_FileHandle_t fileHandle);
static MIF_Status_t     MIF_headerHighWaterMarkSet(MIF_FileHandle_t fileHandle,MIF_Pos_t pos);
static unsigned short   MIF_headerMagicGet(MIF_FileHandle_t fileHandle);
static MIF_Status_t     MIF_headerMagicSet(MIF_FileHandle_t fileHandle,unsigned short value);
static MIF_Status_t     MIF_headerTimeSet(MIF_FileHandle_t fileHandle, time_t ltime);
static MIF_Status_t     MIF_headerVersionSet(MIF_FileHandle_t fileHandle, char *version);


/*********************************************************************/

/************************ GLOBALS ************************************/

static MIF_FileHandle_t LastHeaderRead;
static MIF_Header_t     MIF_Header;
static MIF_CacheTable_t CacheTable[MIF_CACHE_SLOT_COUNT];
static MIF_CacheTable_pt LruList[MIF_CACHE_SLOT_COUNT];
static short            SlotsUsed;
static CacheFirstTime = MIF_TRUE;

/*********************************************************************/

MIF_Status_t MIF_close(MIF_FileHandle_t fileHandle)
{
    if (fileHandle) {
        if (MIF_cacheFlush(fileHandle,MIF_CACHE_FLUSH) != MIF_OKAY)
            return MIF_FILE_ERROR;
        MIF_headerFlush(fileHandle);
#ifdef OS2_SL_BUILD
        if(DosClose(fileHandle) == 0)
#elif AIX
        if (fclose(fileHandle) == 0)
#else
        if (_close(fileHandle) == 0)
#endif
            return MIF_OKAY;
        else
            return MIF_FILE_ERROR;

    } else
        return MIF_NOT_OPEN;
}

MIF_FileHandle_t MIF_create(char *filename, unsigned short magicNumber,
                            char *version)
{
    MIF_FileHandle_t fileHandle;
    time_t           ltime;
    MIF_Pos_t        page1;

#ifdef OS2_SL_BUILD
ULONG ActionTaken;

    if(DosOpen(filename,&fileHandle,&ActionTaken,0L,
            FILE_NORMAL,(OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS),
            (OPEN_FLAGS_RANDOMSEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE | OPEN_FLAGS_NOINHERIT),
            (PEAOP2)NULL) ) return 0;

#elif AIX
    if ((fileHandle = (MIF_FileHandle_t ) fopen(filename, "w+b")) == NULL)
        return 0;
 
#else
    if ((fileHandle = (MIF_FileHandle_t )_open(filename,
        _O_BINARY | _O_CREAT | _O_RDWR, _S_IREAD | _S_IWRITE)) == -1)
        return 0;
#endif
    if (magicNumber != 0) {
        if (MIF_headerMagicSet(fileHandle, magicNumber) != MIF_OKAY)
            return 0;
        if (MIF_headerVersionSet(fileHandle, version) != MIF_OKAY)
            return 0;
        time(&ltime);
        if (MIF_headerTimeSet(fileHandle, ltime) != MIF_OKAY)
            return 0;
        page1.page = 1;
        page1.offset = 0;
        if (MIF_headerHighWaterMarkSet(fileHandle, page1) != MIF_OKAY)
            return 0;
    }
    return fileHandle;
}

unsigned long MIF_length(MIF_FileHandle_t fileHandle)
{
#ifdef AIX
    fpos_t length;
#else
    unsigned long length;
#endif
 
#ifdef OS2_SL_BUILD
    FILESTATUS3 FileInfo;

    DosQueryFileInfo(fileHandle,FIL_STANDARD,&FileInfo,sizeof(FILESTATUS3));
    length = FileInfo.cbFile;
#elif AIX
    /* this is more than just a little strange ... only way to find filelength is
       to fseek to the end of the file, so save current position, do the fseek,
       then restore the file pointer */
    fpos_t currentPosition;
    fgetpos(fileHandle, &currentPosition);
    fseek(fileHandle, 0, 2);
    fgetpos(fileHandle, &length);
    fsetpos(fileHandle, &currentPosition);
#else
    length = _filelength(fileHandle);
#endif
    return length;
}

MIF_FileHandle_t MIF_open(char *filename, MIF_IoMode_t ioMode,
                          unsigned short magicNumber)
{
    unsigned short         rwMode;
    MIF_FileHandle_t fileHandle;

#ifdef OS2_SL_BUILD
ULONG ActionTaken;

    if(ioMode == MIF_READ) rwMode = OPEN_ACCESS_READONLY;
    else rwMode = OPEN_ACCESS_READWRITE;
    if(DosOpen(filename,&fileHandle,&ActionTaken,(ULONG)0,
            FILE_NORMAL,(OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS),
            (OPEN_FLAGS_RANDOMSEQUENTIAL | OPEN_SHARE_DENYNONE | rwMode | OPEN_FLAGS_NOINHERIT),
            (PEAOP2)NULL) != 0) return 0;
#elif AIX
    char rwModeC[3];
 
    if (ioMode == MIF_READ)
        strcpy(rwModeC, "rb");
    else
        strcpy(rwModeC, "r+b");
    if ((fileHandle = (MIF_FileHandle_t ) fopen(filename, rwModeC)) == NULL) 
        return 0;
#else
    if (ioMode == MIF_READ)
        rwMode = _O_RDONLY;
    else
        rwMode = _O_RDWR;
    if ((fileHandle = (MIF_FileHandle_t) _open(filename,
        _O_BINARY | rwMode)) == -1)
        return 0;
#endif
    if ((magicNumber != 0) && (MIF_headerMagicGet(fileHandle) != magicNumber)) {
#ifdef OS2_SL_BUILD
        DosClose(fileHandle);
#elif AIX
        fclose(fileHandle);
#else
        _close(fileHandle);
#endif
        return 0;
    }
    return fileHandle;
}

MIF_Status_t MIF_pageRead(MIF_FileHandle_t fileHandle,
                          unsigned long pageNumber, char *buffer,
                          unsigned long bufferLength)
{
unsigned long offset;
#ifdef OS2_SL_BUILD
unsigned long NewPos;
#elif AIX
size_t bytesRead;
#endif

    if (fileHandle == 0)
        return MIF_NOT_OPEN;
    offset = ((unsigned long) pageNumber) * bufferLength;
#ifdef OS2_SL_BUILD
    if(DosSetFilePtr(fileHandle,offset,FILE_BEGIN,&NewPos)) return MIF_FILE_ERROR;
    if(DosRead(fileHandle,buffer,bufferLength,&NewPos)) return MIF_FILE_ERROR;
#elif AIX
    if (fseek(fileHandle, offset, 0) == -1)
        return MIF_FILE_ERROR;
    if ( (bytesRead = fread((void *) buffer, sizeof(char), bufferLength, fileHandle)) == -1)
        return MIF_FILE_ERROR;
#else
    if (_lseek(fileHandle, offset, SEEK_SET) == -1)
        return MIF_FILE_ERROR;
    if (_read(fileHandle, buffer, bufferLength) == -1)
        return MIF_FILE_ERROR;
#endif
    return MIF_OKAY;
}

static MIF_Status_t MIF_pageWrite(MIF_FileHandle_t fileHandle,
                           unsigned short pageNumber, char *buffer,
                           unsigned short bufferLength)
{
    unsigned long offset;
#ifdef OS2_SL_BUILD
    unsigned long NewPos;
#elif AIX
    size_t bytesWritten;
#endif

    if (fileHandle == 0)
        return MIF_NOT_OPEN;
    offset = ((unsigned long) pageNumber) * bufferLength;
#ifdef OS2_SL_BUILD
    if(DosSetFilePtr(fileHandle,offset,FILE_BEGIN,&NewPos)) return MIF_FILE_ERROR;
    if(DosWrite(fileHandle,buffer,bufferLength,&NewPos)) return MIF_FILE_ERROR;
#elif AIX
    if (fseek(fileHandle, offset, 0) == -1)
        return MIF_FILE_ERROR;
    if ( (bytesWritten = fwrite((void *) buffer, sizeof(char), bufferLength, fileHandle)) == -1)
        return MIF_FILE_ERROR;
    if ( bytesWritten != bufferLength ) { /* ??? */ }
#else
    if (_lseek(fileHandle, offset, SEEK_SET) == -1)
        return MIF_FILE_ERROR;
    if (_write(fileHandle, buffer, bufferLength) == -1)
        return MIF_FILE_ERROR;
#endif
    return MIF_OKAY;
}

MIF_Status_t MIF_remove(char *filename)
{
#ifdef OS2_SL_BUILD
    if(DosDelete(filename)) return MIF_FILE_ERROR;
#else
    if (remove(filename) == -1) return MIF_FILE_ERROR;
#endif
    return MIF_OKAY;
}

static MIF_Status_t MIF_headerFlush(MIF_FileHandle_t fileHandle)
{
    if (fileHandle == LastHeaderRead)
        LastHeaderRead = 0;
    return MIF_OKAY;
}

MIF_Pos_t MIF_headerHighWaterMarkGet(MIF_FileHandle_t fileHandle)
{
    MIF_Pos_t pos;

    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY) {
            pos.page = 0;
            pos.offset = 0;
            return pos;
        }
    return MIF_Header.highWaterMark;
}

static MIF_Status_t MIF_headerHighWaterMarkSet(MIF_FileHandle_t fileHandle,
                                        MIF_Pos_t pos)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    MIF_Header.highWaterMark.page = pos.page;
    MIF_Header.highWaterMark.offset = pos.offset;
    return MIF_headerWrite(fileHandle);
}

static unsigned short MIF_headerMagicGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return 0;
    return MIF_Header.magic;
}

static MIF_Status_t MIF_headerMagicSet(MIF_FileHandle_t fileHandle,
                                unsigned short value)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    MIF_Header.magic = value;
    return MIF_headerWrite(fileHandle);
}

static MIF_Status_t MIF_headerRead(MIF_FileHandle_t fileHandle)
{
    MIF_Pos_t headerPos = {0, 0};
    MIF_Header_t *header;

    if ((header = (MIF_Header_t *)
        MIF_resolve(fileHandle, headerPos, MIF_READ)) == (MIF_Header_t *) 0)
        return MIF_FILE_ERROR;
    MIF_Header.magic = header -> magic;
    strncpy(MIF_Header.version, header -> version, sizeof(header -> version));
    MIF_Header.ltime = header -> ltime;
    MIF_Header.highWaterMark.page = header -> highWaterMark.page;
    MIF_Header.highWaterMark.offset = header -> highWaterMark.offset;
    LastHeaderRead = fileHandle;
    return MIF_OKAY;
}

time_t MIF_headerTimeGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return (time_t) 0;
    return MIF_Header.ltime;
}

static MIF_Status_t MIF_headerTimeSet(MIF_FileHandle_t fileHandle, time_t ltime)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    MIF_Header.ltime = ltime;
    return MIF_headerWrite(fileHandle);
}

char *MIF_headerVersionGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return (char *) 0;
    return MIF_Header.version;
}

static MIF_Status_t MIF_headerVersionSet(MIF_FileHandle_t fileHandle, char *version)
{
    if (fileHandle != LastHeaderRead)
        if (MIF_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    strcpy(MIF_Header.version, version);
    return MIF_headerWrite(fileHandle);
}

static MIF_Status_t MIF_headerWrite(MIF_FileHandle_t fileHandle)
{
    MIF_Pos_t headerPos = {0, 0};
    MIF_Header_t *header;

    if (fileHandle != LastHeaderRead)
        return MIF_FILE_ERROR;
    if ((header = (MIF_Header_t *)
         MIF_resolve(fileHandle, headerPos, MIF_WRITE)) == (MIF_Header_t *) 0)
        return MIF_FILE_ERROR;
    header -> magic = MIF_Header.magic;
    strncpy(header -> version, MIF_Header.version, sizeof(header -> version));
    header -> ltime = MIF_Header.ltime;
    header -> highWaterMark.page = MIF_Header.highWaterMark.page;
    header -> highWaterMark.offset = MIF_Header.highWaterMark.offset;
    return MIF_OKAY;
}

MIF_Pos_t MIF_alloc(MIF_FileHandle_t fileHandle, unsigned short size)
{
    MIF_Pos_t pos;
    MIF_Pos_t newPos;

    MIF_allocAlign(fileHandle, size);
    pos = MIF_headerHighWaterMarkGet(fileHandle);
    if ((pos.page == 0) && (pos.offset == 0))
        return pos;
    newPos.page = pos.page;
    newPos.offset = (unsigned short) (pos.offset + (unsigned short) size);
    if (newPos.offset > MIF_PAGE_SIZE) {
        ++newPos.page;
        newPos.offset -= MIF_PAGE_SIZE;
    }
    if (MIF_headerHighWaterMarkSet(fileHandle, newPos) != MIF_OKAY) {
        pos.page = 0;
        pos.offset = 0;
    }
    return pos;
}

void MIF_allocAlign(MIF_FileHandle_t fileHandle, unsigned short size)
{
    MIF_Pos_t pos;

    pos = MIF_headerHighWaterMarkGet(fileHandle);
    if ((pos.page == 0) && (pos.offset == 0))
        return;
    if (pos.offset + size > MIF_PAGE_SIZE) {
        ++pos.page;
        pos.offset = 0;
        MIF_headerHighWaterMarkSet(fileHandle, pos);
    }
    return;
}

MIF_Pos_t MIF_allocClear(MIF_FileHandle_t fileHandle, unsigned short size)
{
    MIF_Pos_t pos;
    MIF_Pos_t nullPos;
    char      *c;
    short     i;

    nullPos.page = 0;
    nullPos.offset = 0;
    pos = MIF_alloc(fileHandle, size);
    if ((pos.page == 0) && (pos.offset == 0))
        return nullPos;
    if ((c = (char *) MIF_resolve(fileHandle, pos, MIF_WRITE)) == (char *) 0)
        return nullPos;
    for (i = 0; i < (short) size; ++i)
        c[i] = 0;
    return pos;
}


MIF_Status_t MIF_cacheFlush(MIF_FileHandle_t fileHandle,MIF_cacheMode_t Mode)
{
short i;
MIF_CacheTable_t **Slot = LruList;

    for (i = 0; i < SlotsUsed; i++,Slot++) {
        if ((fileHandle == 0) || ((*Slot)->fileHandle == fileHandle)) {
            if ((*Slot)->modified == MIF_TRUE) {
                if (MIF_pageWrite(fileHandle,(*Slot)->page,
                    (*Slot)->buffer, MIF_PAGE_SIZE) != MIF_OKAY)
                    return MIF_FILE_ERROR;
                (*Slot)->modified = MIF_FALSE;
            }
            if(Mode == MIF_CACHE_FLUSH) (*Slot)->fileHandle = 0;
        }
    }
    return MIF_OKAY;
}

static MIF_Status_t MIF_cachePage(MIF_FileHandle_t fileHandle,
                                  unsigned short pageNumber)
{
short i;
MIF_CacheTable_t *SlotEntry;   /* set this to the base of the list */
MIF_CacheTable_t **Slot = LruList;

    for (i = 0; i < SlotsUsed; i++,Slot++) {
        if (((*Slot)->fileHandle == fileHandle) &&
            ((*Slot)->page == pageNumber)) {
            MIF_lruSet((*Slot));
            return MIF_OKAY;
        }
    }
    i = SlotsUsed;
    if(i == MIF_CACHE_SLOT_COUNT) i--;
    SlotEntry = LruList[i];   /* point to the first unused, or the last one */
    if (SlotEntry->modified){
        if (MIF_pageWrite(SlotEntry->fileHandle,SlotEntry->page, SlotEntry->buffer,MIF_PAGE_SIZE) != MIF_OKAY)
           return MIF_FILE_ERROR;
        SlotEntry->modified = MIF_FALSE;   /* reset this block */
    }
    SlotEntry = MIF_lruSet(SlotEntry);
    SlotEntry->fileHandle = fileHandle;
    SlotEntry->page = pageNumber;
    SlotEntry->modified = MIF_FALSE;
    if(SlotsUsed < MIF_CACHE_SLOT_COUNT) SlotsUsed++;
    return MIF_pageRead(fileHandle, pageNumber, SlotEntry->buffer,MIF_PAGE_SIZE);
}

static MIF_CacheTable_t *MIF_lruSet(MIF_CacheTable_t *slot)
{
short i;
MIF_CacheTable_t **Entry = LruList;  /* set this to the base of the list */

    for (i = 0; i < SlotsUsed; i++,Entry++)
        if ((*Entry) == slot) break;
    if(i != 0){
        for (; i > 0; i--,Entry--) (*Entry) = *(Entry - 1);
        (*Entry) = slot;
    }
    return (*Entry);    /* return the actual ptr to this slot */
}

void *MIF_resolve(MIF_FileHandle_t fileHandle, MIF_Pos_t pos, MIF_IoMode_t mode)
{
short i;
MIF_CacheTable_t **Slot = LruList;

    if(CacheFirstTime == MIF_TRUE){
        for(i = 0;i != MIF_CACHE_SLOT_COUNT;i++,Slot++)
             (*Slot) = &CacheTable[i];
        CacheFirstTime = MIF_FALSE;    /* reset this so we only do this once */
        Slot =  LruList;  /* reset this for the normal case */
    }
    if (fileHandle == 0) return (void *) 0;

    if (MIF_cachePage(fileHandle, pos.page) != MIF_OKAY) return (void *) 0;
    if (mode & MIF_WRITE) (*Slot)->modified = MIF_TRUE;
    return (void *) ((*Slot)->buffer + pos.offset);
}
