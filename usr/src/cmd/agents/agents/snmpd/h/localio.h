/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)localio.h	2.16 96/07/25 Sun Microsystems"
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
/*****************************************************************************
 *     Copyright (c) 1986,1988	Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/H/LOCALIO.H_V   2.0   31 Mar 1990 15:11:26  $	*/
/*
 * $Log:   E:/SNMPV2/H/LOCALIO.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:26
 * Release 2.00
 * 
 *    Rev 1.2   19 Sep 1988 17:48:36
 * Added header and version identification lines.  No code changes.
*/

#if (!defined(localio_inc))
#define localio_inc

typedef unsigned char uchar;

typedef struct LCL_FILE_S
	{
	uchar	lcl_flags;
	uchar	*lbuf_start;
	uchar	*lbuf_next;
	uchar	*lbuf_end;	/* Address AFTER last byte */
	} LCL_FILE;

#define LCL_MALLOC  0x01    /* LCL_FILE was malloc-ed at open */
#define LCL_EOF	    0x80    /* EOF encountered */

#define Lcl_Eof(L) ((((LCL_FILE *)(L))->lcl_flags&LCL_EOF)?-1:0)
			 
#define Lcl_Tell(L) (int)(((LCL_FILE *)(L))->lbuf_next- \
		     ((LCL_FILE *)(L))->lbuf_start)

#define GETC_MACRO
#if defined(GETC_MACRO)
/* The following is an macro version of this routine Lcl_Getc */
#define Lcl_Getc(L)	(int)(	\
	(((LCL_FILE *)(L))->lcl_flags & LCL_EOF) ? -1 :	\
	((((LCL_FILE *)(L))->lbuf_next < ((LCL_FILE *)(L))->lbuf_end) ?	\
		(*(((LCL_FILE *)(L))->lbuf_next++)) :	\
		((((LCL_FILE *)(L))->lcl_flags |= LCL_EOF), -1)))
#endif	/* GETC_MACRO */

#if !defined(NO_PP)
extern	LCL_FILE    *Lcl_Open(LCL_FILE *, uchar *, int);
extern	void	     Lcl_Close(LCL_FILE *);
#if !defined(GETC_MACRO)
extern	int	     Lcl_Getc(LCL_FILE *);
#endif	/* GETC_MACRO */
extern	int	     Lcl_Peekc(LCL_FILE *);
extern	int	     Lcl_Read(LCL_FILE *, uchar *, int);
extern	int	     Lcl_Seek(LCL_FILE *, int, int);
extern	int	     Lcl_Resize(LCL_FILE *, int, int);
#else	/* NO_PP */
extern	LCL_FILE    *Lcl_Open();
extern	void	     Lcl_Close();
#if !defined(GETC_MACRO)
extern	int	     Lcl_Getc();
#endif	/* GETC_MACRO */
extern	int	     Lcl_Peekc();
extern	int	     Lcl_Read();
extern	int	     Lcl_Seek();
extern	int	     Lcl_Resize();
#endif	/* NO_PP */

#endif
