/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_src.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_src.c

    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Source I/O Routines

    Author(s): Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        10/13/95 par    Re-written for performance and compatibility

************************* INCLUDES ***********************************/

#include <io.h>
#include <stdio.h>
#include "mif_db.h"
#include "pr_src.h"
#include "os_svc.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

static MIF_FileHandle_t SourceHandle;
static long          FileEndPos;
static long          FilePos;
static int           LastCharacter;
static unsigned long FileBlockSize = 10240;   /* try to read the file in 10K blocks */
static char          *SourceBuffer;           /* buffer used for reading the source file */

/*********************************************************************/

int PR_sourceCharGetNext(void)
{
unsigned char *c;
unsigned long WorkPos;
unsigned long Page = 0;

    if (SourceHandle == 0) return EOF;     /* bad handle */
    ++FilePos;                             /* advance the file pointer */
    if(FilePos == FileEndPos) return EOF;  /* we've hit the end of the file */
    WorkPos = FilePos;                     /* now let's work with an offset with the blocksize */
    while(WorkPos >= FileBlockSize){       /* back down to a local offset */
        Page++;
        WorkPos -= FileBlockSize;
    }
    if(WorkPos == 0)    /* we have rolled over to a new block, load it up */
        if(MIF_pageRead(SourceHandle,Page,SourceBuffer,FileBlockSize) != MIF_OKAY) return '\0';
    c = (unsigned char *)SourceBuffer + WorkPos;    /* point to the new location here */
    LastCharacter = *c;
    if (LastCharacter == '\r') return PR_sourceCharGetNext();
    else return LastCharacter;
}

int PR_sourceCharGetPrev(void)
{
unsigned char *c;
unsigned long WorkPos;
unsigned long Page = 0;

    if (SourceHandle == 0) return EOF;     /* bad handle */
    if(FilePos == 0) return '\0';          /* we've hit the beginning of the file */
    --FilePos;                             /* backup the file pointer */
    WorkPos = FilePos;                     /* now let's work with an offset with the blocksize */
    while(WorkPos >= FileBlockSize){       /* back down to a local offset */
        WorkPos -= FileBlockSize;
        Page++;
    }
    if(WorkPos == (FileBlockSize - 1))    /* we have rolled back to an old block, load it up */
        if(MIF_pageRead(SourceHandle,Page,SourceBuffer,FileBlockSize) != MIF_OKAY) return '\0';
    c = (unsigned char *)SourceBuffer + WorkPos;    /* point to the new location here */
    LastCharacter = *c;
    if (LastCharacter == '\r') return PR_sourceCharGetPrev();
    else return LastCharacter;
}

MIF_Status_t PR_sourceClose(void)
{
    OS_Free(SourceBuffer);    /* free up the read buffer */
    return MIF_close(SourceHandle);
}

MIF_Status_t PR_sourceOpen(char *filename)
{

    SourceBuffer = OS_Alloc(FileBlockSize);     /* allocate a block of memory for reading the source file */
    if(SourceBuffer == (char *)NULL) return MIF_NOT_FOUND;
    SourceHandle = MIF_open(filename, MIF_READ, 0);
    if (SourceHandle == 0) return MIF_NOT_FOUND;
    FilePos = (long) -1;  /* force the location to -1 so it will roll to 0 on the first read */
    FileEndPos = MIF_length(SourceHandle);
    LastCharacter = 0;
    return MIF_OKAY;
}


