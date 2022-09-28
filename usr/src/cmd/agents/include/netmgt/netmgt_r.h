/* Copyright 1988 - 07/26/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)netmgt_release.h	2.40 96/07/26 Sun Microsystems"
#endif
#endif

/*
   Sun considers its source code as an unpublished, proprietary trade
   secret, and it is available only under strict license provisions.
   This copyright notice is placed here only to protect Sun in the event
   the source is deemed a published work.  Disassembly, decompilation,
   or other means of reducing the object code to human readable form is
   prohibited by the license agreement under which this code is provided
   to the user or company in possession of this copy.
  
   RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
   Government is subject to restrictions as set forth in subparagraph
   (c)(1)(ii) of the Rights in Technical Data and Computer Software
   clause at DFARS 52.227-7013 and in similar clauses in the FAR and
   NASA FAR Supplement.
*/
#ifndef lint
#ifndef _release_h
#define _release_h

static char product_name[]="Product:Site/Domain/SunNet Manager" ;
#ifdef SVR4
#ifdef i386
static char release[]="Release:2.3 FCS - Solaris X86" ;
#else
static char release[]="Release:2.3 FCS - Solaris 2" ;
#endif
#else
static char release[]="Release:2.3 FCS - Solaris 1" ;
#endif
#endif /* _release_h */
#endif /* lint */
