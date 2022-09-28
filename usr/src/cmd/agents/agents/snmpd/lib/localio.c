/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)localio.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)localio.c	2.15 96/07/23 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1986, 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/SNMP/LOCALIO.C_V   2.0   31 Mar 1990 15:06:48  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/LOCALIO.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:48
 * Release 2.00
 * 
 *    Rev 1.4   27 Apr 1989 15:56:38
 * Removed unused variables
 * 
 *    Rev 1.3   12 Apr 1989 12:02:10
 * Added cast on SNMP_mem_alloc.
 * 
 *    Rev 1.2   23 Mar 1989 11:22:02
 * Revised Lcl_Open to return a null pointer if it can't obtain dynamic
 * memory for a local file descriptor.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:10
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:00
 * Initial revision.
*/

#include <libfuncs.h>

#include <localio.h>

#define then

/****************************************************************************
Local I/O package -- performs I/O like operations on data in a buffer.

Caveat: This version supports only input operations.
****************************************************************************/

/****************************************************************************
Lcl_Open -- Open a buffer for use as a local I/O stream

Parameters:
        LCL_FILE *	lfile
	uchar *		buffer
	int		nbytes

If lfile == (LCL_FILE *)0 a local file descriptor will be allocated.

Returns: (LCL_FILE *) if sucessful, (LCL_FILE *)0 if unsucessful.
****************************************************************************/
#if !defined(NO_PP)
LCL_FILE *
Lcl_Open(
    register LCL_FILE   *lfile,
    uchar		*buff,
    int			nbytes)
#else	/* NO_PP */
LCL_FILE *
Lcl_Open(lfile, buff, nbytes)
    register LCL_FILE   *lfile;
    uchar		*buff;
    int			nbytes;
#endif	/* NO_PP */
{
if (lfile == (LCL_FILE *)0)
   then {
        if ((lfile = (LCL_FILE *)SNMP_mem_alloc(sizeof(LCL_FILE)))
	    == (LCL_FILE *)0)
	   then return (LCL_FILE *)0;
	   else lfile->lcl_flags = LCL_MALLOC;
        }
   else lfile->lcl_flags = 0;

lfile->lbuf_start = buff;
lfile->lbuf_next = buff;
lfile->lbuf_end = (uchar *)(buff + nbytes);
return (lfile);
}
    
/****************************************************************************
Lcl_Close -- Close a local file descriptor

Parameters:
        LCL_FILE *	lfile

Returns: nothing
****************************************************************************/
#if !defined(NO_PP)
void
Lcl_Close(register LCL_FILE   *lfile)
#else	/* NO_PP */
void
Lcl_Close(lfile)
    register LCL_FILE   *lfile;
#endif	/* NO_PP */
{
if (lfile->lcl_flags & LCL_MALLOC) SNMP_mem_free((char *)lfile);
}

/****************************************************************************
Lcl_Getc -- Read a character from a local I/O stream

Parameters:
        LCL_FILE *	lfile

Returns: If sucessful: the character read, but in integer format
	 If unsucessful: -1
****************************************************************************/
#if !defined(GETC_MACRO)
#if !defined(NO_PP)
int
Lcl_Getc(register LCL_FILE *lfile)
#else	/* NO_PP */
int
Lcl_Getc(lfile)
    register LCL_FILE *lfile;
#endif	/* NO_PP */
{
if (lfile->lcl_flags & LCL_EOF) return -1;
if (lfile->lbuf_next < lfile->lbuf_end)
        return (*(lfile->lbuf_next++));
   else {
        lfile->lcl_flags |= LCL_EOF;
        return -1;
        }
/*NOTREACHED*/
}
#endif	/* GETC_MACRO */

/****************************************************************************
Lcl_Peekc -- Peek at a character from a local I/O stream, the seek pointer
	     is not advanced.

Parameters:
        LCL_FILE *	lfile

Returns: If sucessful: the character read, but in integer format
	 If unsucessful: -1
****************************************************************************/
#if !defined(NO_PP)
int
Lcl_Peekc(register LCL_FILE *lfile)
#else	/* NO_PP */
int
Lcl_Peekc(lfile)
    register LCL_FILE *lfile;
#endif	/* NO_PP */
{
if (lfile->lcl_flags & LCL_EOF) return -1;
if (lfile->lbuf_next < lfile->lbuf_end)
        return (*(lfile->lbuf_next));
   else {
        lfile->lcl_flags |= LCL_EOF;
        return -1;
        }
/*NOTREACHED*/
}

/****************************************************************************
Lcl_Read -- Read a set of characters from a local I/O stream.

Parameters:
        LCL_FILE *	lfile
	uchar *		ubuf
	int		nbytes

Returns: The number of bytes actually read.
****************************************************************************/
#if !defined(NO_PP)
int
Lcl_Read(
    LCL_FILE	*lfile,
    uchar	*ubuf,
    int		nbytes)
#else	/* NO_PP */
int
Lcl_Read(lfile, ubuf, nbytes)
    LCL_FILE	*lfile;
    uchar	*ubuf;
    int		nbytes;
#endif	/* NO_PP */
{
/* This is a quick implementation, to be replaced later */
int orig_nbytes = nbytes;
uchar c;
while (nbytes > 0)
    {
    c = (uchar)Lcl_Getc(lfile);
    if (Lcl_Eof(lfile)) break;
    *ubuf++ = c;
    --nbytes;
    }
return (orig_nbytes - nbytes);
}

/****************************************************************************
Lcl_Seek -- Move the seek pointer to a give position in the local I/O buffer.

Parameters:
        LCL_FILE *	lfile
	int		offset
	int		whence:
			0 to set pointer to offset bytes from the start
			1 to move pointer by offset bytes
			2 to set pointer to offset bytes from the end

Returns: 0 if sucessful, -1 if not.

Note: The "end" position is the byte AFTER the last one in the buffer
containing data.  Thus, a Lcl_Seek(.., 0, 2) will leave the caller at
the end-of-file.  The last byte is reached by Lcl_Seek(.., 1, 2).
****************************************************************************/
#if !defined(NO_PP)
int
Lcl_Seek(
    register LCL_FILE	*lfile,
    int			offset,
    int			whence)
#else	/* NO_PP */
int
Lcl_Seek(lfile, offset, whence)
    register LCL_FILE	*lfile;
    int			offset;
    int			whence;
#endif	/* NO_PP */
{
register uchar *next;

switch (whence)
    {
    case 0:
        next = (uchar *)(lfile->lbuf_start + offset);
        break;
    case 1:
        next = (uchar *)(lfile->lbuf_next + offset);
        break;
    case 2:
        next = (uchar *)(lfile->lbuf_end - offset);
        break;
    default:
        return -1;
    }

if ((next < lfile->lbuf_start) || (next > lfile->lbuf_end)) return -1;

if (next < lfile->lbuf_end) lfile->lcl_flags &= ~LCL_EOF;
lfile->lbuf_next = next;
return 0;
}

/****************************************************************************
Lcl_Resize -- Move the end-of-buffer position for the local I/O buffer.
	      The buffer may be extended or contracted.

Parameters:
        LCL_FILE *	lfile
	int		offset
	int		whence:
			0 to set new end to offset bytes from the start
			1 to move new end by offset bytes from the current
			  read/write position
			2 to set new end to offset bytes from the end

Returns: Previous value (relative to the start of the file).
	 -1 indicates an error.
****************************************************************************/
#if !defined(NO_PP)
int
Lcl_Resize(
    register LCL_FILE	*lfile,
    int			offset,
    int			whence)
#else	/* NO_PP */
int
Lcl_Resize(lfile, offset, whence)
    register LCL_FILE	*lfile;
    int			offset;
    int			whence;
#endif	/* NO_PP */
{
int	oldval;

oldval = lfile->lbuf_end - lfile->lbuf_start;

switch (whence)
    {
    case 0:
        lfile->lbuf_end = (uchar *)(lfile->lbuf_start + offset);
        break;
    case 1:
         lfile->lbuf_end = (uchar *)(lfile->lbuf_next + offset);
        break;
    case 2:
         lfile->lbuf_end = (uchar *)(lfile->lbuf_end - offset);
        break;
    default:
        return -1;
    }

if (lfile->lbuf_next < lfile->lbuf_end)
   then lfile->lcl_flags &= ~LCL_EOF;
   else lfile->lcl_flags |= LCL_EOF;

return oldval;
}
