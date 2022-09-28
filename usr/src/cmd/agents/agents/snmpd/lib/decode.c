/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)decode.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)decode.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/DECODE.C_V   2.0   31 Mar 1990 15:06:44  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/DECODE.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:44
 * Release 2.00
 * 
 *    Rev 1.9   14 Dec 1989 16:00:48
 * Added support for Borland Turbo C compiler
 * 
 *    Rev 1.8   04 Jul 1989 12:03:38
 * DecodeIntegerData() was improperly decoding negative integers
 * when the length was less than 4 bytes.
 * 
 *    Rev 1.7   27 Apr 1989 15:56:00
 * Removed unused variables
 * 
 *    Rev 1.6   12 Apr 1989 12:02:40
 * Added cast on value returned by SNMP_mem_alloc().
 * 
 *    Rev 1.5   23 Mar 1989 11:55:56
 * Merged the decode helper into the one routine that used it.
 * 
 *    Rev 1.4   18 Mar 1989 11:56:42
 * Unused octet string handling code removed.
 * Octet string decoding simplified and hardened against zero length strings.
 * 
 *    Rev 1.3   17 Mar 1989 21:41:56
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.2   19 Sep 1988 17:26:54
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.1   14 Sep 1988 17:54:20
 * Removed improper casts in calls to SNMP_mem_alloc().
 * Also moved includes of system includes into libfunc.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:56
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <decode.h>
#include <buffer.h>

#define	then

/****************************************************************************
NAME:  A_DecodeTypeValue

PURPOSE:  Decode the numeric part of an ASN.1 type from a stream.
	  The data stream is read using the local I/O package.
	  On exit, the stream pointer will be positioned to the byte
	  *AFTER* the type.

NOTE:	  The Class portion of the type is NOT decoded here, only the
	  value portion.
	  The user should call A_DecodeTypeClass *BEFORE* calling this
	  routine in order to get the class.

PARAMETERS:  LCL_FILE *	    A stream descriptor (already open)
	     int *	    Receives an error code, if any.

RETURNS:  ATVALUE_T	    The type value

RESTRICTIONS:  It is assumed that the stream does not reach EOF before the
		end of the field.
****************************************************************************/
#if !defined(NO_PP)
ATVALUE_T
A_DecodeTypeValue(LCL_FILE *	lfile,
		 int *		errp)
#else	/* NO_PP */
ATVALUE_T
A_DecodeTypeValue(lfile, errp)
	LCL_FILE *lfile;
	int	 *errp;
#endif	/* NO_PP */
{
register OCTET_T oct;

oct = ((OCTET_T) Lcl_Getc(lfile)) & ~A_IDCF_MASK;
if (Lcl_Eof(lfile))
   then {
	*errp = AE_PREMATURE_END;
	return (ATVALUE_T) 0;
	}

if (oct != 0x1F)    /* Are there extension bytes? */
   then {   /* No extensions, type is in oct */
	return (ATVALUE_T) oct;
	}
   else {   /* Type is in extension octets */
	register ATVALUE_T t = 0;
/**	while ((oct = ((OCTET_T)Lcl_Getc(lfile))) & 0x80) **/
	for(;;)
	    {
	    oct = (OCTET_T)Lcl_Getc(lfile);
	    if (Lcl_Eof(lfile))
	       then {
		    *errp = AE_PREMATURE_END;
		    return t;
		    }
	    if (!(oct & 0x80)) then break;  /* Hit final byte, we'll use*/
					    /* it at the end of the loop*/

	    t |= (ATVALUE_T)(oct & 0x7F);   /* Deal with a non-final byte */
	    t <<= 7;
	    }
	t |= (ATVALUE_T) oct;	/* Take care of the final byte (the one	*/
				/* without the 0x80 continuation bit.)	*/
	return t;
	}
/*NOTREACHED*/
}

/****************************************************************************
NAME:  A_DecodeLength

PURPOSE:  Decode an ASN.1 length from a stream.
	  The data stream is read using the local I/O package.
	  On exit, the stream pointer will be positioned to the byte
	  *AFTER* the length.

PARAMETERS:  LCL_FILE *	    Stream descriptor
	     int *	    Receives an error code, if any.

RETURNS:  ALENGTH_T -- the length.
	  If the length is indefinite, (ALENGTH_T)-1 is returned.

RESTRICTIONS:  The stream must be open.
	       It is assumed that the stream will not reach EOF before the
	       length is decoded.
****************************************************************************/
#if !defined(NO_PP)
ALENGTH_T
A_DecodeLength(LCL_FILE *	lfile,
	       int *		errp)
#else	/* NO_PP */
ALENGTH_T
A_DecodeLength(lfile, errp)
	LCL_FILE *lfile;
	int	 *errp;
#endif	/* NO_PP */
{
OCTET_T oct;

oct = (OCTET_T)Lcl_Getc(lfile);
if (Lcl_Eof(lfile))
   then {
	*errp = AE_PREMATURE_END;
	return (ALENGTH_T) 0;
	}

/* Indefinite form? */
if (oct == 0x80)
   then {
	*errp = AE_INDEFINITE_LENGTH;
	return (ALENGTH_T) -1;
	}

if (!(oct & 0x80))  /* Short or long format? */
   then return (ALENGTH_T) oct;   /* Short format */
   else {   /* Long format */
	register OCTET_T lsize;
	register ALENGTH_T len = 0;

	lsize = oct & 0x7F;	/* Get # of bytes comprising length field */
	while (lsize-- != 0)
	    {
	    len <<= 8;
	    len |= (OCTET_T)Lcl_Getc(lfile);
	    if (Lcl_Eof(lfile))
	       then {
		    *errp = AE_PREMATURE_END;
		    return (ALENGTH_T) 0;
		    }
	    }
	return len;
	}
/*NOTREACHED*/
}

/********************
A_DecodeOctetStringData

PURPOSE:  Pull an octet string from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the data field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	ALENGTH_T	Length of octet string, from its ASN.1 header
	EBUFFER_T *	Control structure to receive the data.
	int *		Receives an error code, if any.

Returns: Nothing

Note:	On return, the "start_bp" component of the buffer structure
	points to a "malloc"-ed area in which the octet string is held.
	Note that the octet string is NOT null terminated, may contain
	internal nulls. A null pointer, (char *)0, is used if no area
	is malloc-ed.
	If the string is of zero length, a dummy buffer is established
	which appears to be have a static buffer of length zero at
	address zero.
********************/
#if !defined(NO_PP)
void
A_DecodeOctetStringData(LCL_FILE *	stream,
			ALENGTH_T	length,
			EBUFFER_T *	ebuffp,
			int *		errp)
#else	/* NO_PP */
void
A_DecodeOctetStringData(stream, length, ebuffp, errp)
	LCL_FILE	*stream;
	ALENGTH_T	length;
	EBUFFER_T	*ebuffp;
	int		*errp;
#endif	/* NO_PP */
{
if ((length != 0) && (length != (ALENGTH_T) -1))
   then {
	OCTET_T	*buffp;
	ALENGTH_T got;
	if ((buffp = (OCTET_T *)SNMP_mem_alloc(length)) == (OCTET_T *)0)
	   then return;
	EBufferSetup(BFL_IS_DYNAMIC, ebuffp, buffp, length);
	got = (ALENGTH_T)Lcl_Read(stream, ebuffp->next_bp, (int)length);
	if (got == length)
	   then {
		ebuffp->remaining -= got;
		ebuffp->next_bp += (unsigned short)got;
		}
	   else *errp = AE_PREMATURE_END;
	}
   else { /* Length is either zero or indeterminate */
	EBufferPreLoad(BFL_IS_STATIC, ebuffp, (OCTET_T *)0, 0);
	}
}

/********************
A_DecodeOctetString

PURPOSE:  Pull an octet string from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the octet string's type field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	EBUFFER_T *	Control structure to receive the data.
	int *		Receives an error code, if any.

Returns: Nothing

Note:	On return, the "start_bp" component of the buffer structure
	points to a "malloc"-ed area in which the octet string is held.
	Note that the octet string is NOT null terminated, may contain
	internal nulls. A null pointer, (char *)0, is used if no area
	is malloc-ed.
********************/
#if !defined(NO_PP)
void
A_DecodeOctetString(LCL_FILE *	stream,
		    EBUFFER_T *	ebuffp,
		    int *	errp)
#else	/* NO_PP */
void
A_DecodeOctetString(stream, ebuffp, errp)
	LCL_FILE	*stream;
	EBUFFER_T	*ebuffp;
	int		*errp;
#endif	/* NO_PP */
{
ALENGTH_T	os_length;

(void) A_DecodeTypeValue(stream, errp);
os_length = A_DecodeLength(stream, errp);
if (*errp == 0)
   then {
	A_DecodeOctetStringData(stream, os_length, ebuffp, errp);
	}
   else { /* On a decoding error, pretend we have a zero length string */
	EBufferPreLoad(BFL_IS_STATIC, ebuffp, (OCTET_T *)0, 0);
	}
}

/********************
A_DecodeIntegerData

PURPOSE:  Pull an integer from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the data field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	ALENGTH_T	Length of contents field, from the ASN.1 header
	int *		Receives an error code, if any.

Returns: INT_32_T	(See note below)

NOTE: If the received value is really unsigned, then the caller should
merely cast the value returned by this procedure to an UINT_32_T.

WARNING: If the integer occupies more than 4 octets, then high order precision
will be lost, including the sign bit.  For unsigned values in which the
basic value occupies all 4 octets, the sign octet, containing a zero sign
bit, will be lost but will not damage the returned value.
********************/
#if !defined(NO_PP)
INT_32_T
A_DecodeIntegerData(LCL_FILE *	stream,
		    ALENGTH_T	length,
		    int *	errp)
#else	/* NO_PP */
INT_32_T
A_DecodeIntegerData(stream, length, errp)
	LCL_FILE	*stream;
	ALENGTH_T	length;
	int		*errp;
#endif	/* NO_PP */
{
INT_32_T ivalue = 0;
int	 firstone = 1;
OCTET_T	 oct;

while(length-- != 0)
   {
   oct = (OCTET_T) Lcl_Getc(stream);
   if (Lcl_Eof(stream))
      then {
	   *errp = AE_PREMATURE_END;
	   return ivalue;
	   }

   /* See whether we are receiving something that has the sign bit set */
   if (firstone)
      then {
	   firstone = 0;
	   if (oct & (OCTET_T)0x80)
	      then ivalue = (INT_32_T)-1;
	   }
/*lint -e703	*/
   ivalue <<= 8;
/*lint +e703	*/
   ivalue |= oct;	/* 'oct' better not be sign extended!!!	*/
   }
return ivalue;
}

/********************
A_DecodeInteger

PURPOSE:  Pull an integer from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the integer's ASN.1 type field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	int *		Receives an error code, if any.

Returns: INT_32_T;
********************/
#if !defined(NO_PP)
INT_32_T
A_DecodeInteger(LCL_FILE *	stream,
		int *		errp)
#else	/* NO_PP */
INT_32_T
A_DecodeInteger(stream, errp)
	LCL_FILE	*stream;
	int		*errp;
#endif	/* NO_PP */
{
(void) A_DecodeTypeValue(stream, errp);
return A_DecodeIntegerData(stream, A_DecodeLength(stream, errp), errp);
}

/********************
A_DecodeObjectIdData

PURPOSE:  Pull an object identifier from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the data field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	ALENGTH_T	Length of contents field, from the ASN.1 header
	OBJ_ID_T *	Object identifier structure to receive the object id.
			The "component_list" will be "malloc"ed.
			component_list == (OIDC_T *)0 indicates that
			there is no component_list.
	int *		Receives an error code, if any.

Returns: Nothing
********************/
#if !defined(NO_PP)
void
A_DecodeObjectIdData(LCL_FILE *	stream,
		     ALENGTH_T	length,
		     OBJ_ID_T *	objp,
		     int *	errp)
#else	/* NO_PP */
void
A_DecodeObjectIdData(stream, length, objp, errp)
	LCL_FILE	*stream;
	ALENGTH_T	length;
	OBJ_ID_T	*objp;
	int		*errp;
#endif	/* NO_PP */
{
int content_offset;	/* Offset in stream where the contents begins */
int left;		/* Count of unused contents bytes */
int subids;		/* Number of subidentifiers	*/
int subid_num;
OIDC_T	subid_val;	/* Value of a subidentifier */
OIDC_T	*cp;
unsigned char	c;

objp->num_components = 0;
objp->component_list = (OIDC_T *)0;

/* Remember where the contents begins */
content_offset = Lcl_Tell(stream);

/* Count the number of components */
for(subids = 0, left = (int)length; left > 0; left--)
   {
   c = (unsigned char)Lcl_Getc(stream);
   if (Lcl_Eof(stream))
      then {
	   *errp = AE_PREMATURE_END;
	   return;
	   }

   /* Skip all bytes but ones which are the last in a subidentifier.	*/
   /* In other words skip all which have the high order bit set.	*/
   if (c & 0x80) then continue;
   subids++;
   }

(void) Lcl_Seek(stream, content_offset, 0);

/* Null object id if no subidentifier fields */
if (subids == 0) return;

/* Try to get space for the components list */
cp = (OIDC_T *)SNMP_mem_alloc((unsigned int)(sizeof(OIDC_T) * (subids + 1)));
if (cp == (OIDC_T *)0)
   then {
	return;
	}
objp->num_components = subids + 1;
objp->component_list = cp;

/* Decode the subids and components */
for(subid_num = 0; subid_num < subids; subid_num++)
   {
   /* Decode the subidentifier */
   for(subid_val = 0;;)
      {
      c = (unsigned char)Lcl_Getc(stream);
      if (Lcl_Eof(stream))
	 then {
	      *errp = AE_PREMATURE_END;
	      return;
	      }
      subid_val <<= 7;
      subid_val |= (OIDC_T)(c & 0x7F);
      if (!(c & 0x80)) break;
      }

   /* Is this the first subidentifier?			*/
   /* i.e. the one that contains TWO components?	*/
   if (subid_num == 0)
      then {
	   if (subid_val < 40)
	      then {
		   *cp++ = 0;
		   *cp++ = subid_val;
		   }
	      else {
		   if (subid_val < 80)
		      then {
			   *cp++ = 1;
			   *cp++ = subid_val - 40;
			   }
		      else {
			   *cp++ = 2;
			   *cp++ = subid_val - 80;
			   }
		   }
	   }
      else { /* subid_num != 0, i.e. this is not the first subidentifier */
	   *cp++ = subid_val;
	   }
   }

return;
}

/********************
A_DecodeObjectId

PURPOSE:  Pull an object identifer from an ASN.1 stream.
	  The data stream is read using the local I/O package.
	  On entry stream pointer should be positioned to the first byte
	  of the object identifier's ASN.1 type field.
	  On exit, the stream pointer will be positioned to at the start
	  of the next ASN.1 type field.

Parameters:
	LCL_FILE *	Stream descriptor
	OBJ_ID_T *	Object identifier structure to receive the object id.
			The "component_list" will be "malloc"ed.
			component_list == (OIDC_T *)0 indicates that
			there is no component_list.
	int *		Receives an error code, if any.
		
Returns: Nothing
********************/
#if !defined(NO_PP)
void
A_DecodeObjectId(LCL_FILE *	stream,
		 OBJ_ID_T *	objp,
		 int *		errp)
#else	/* NO_PP */
void
A_DecodeObjectId(stream, objp, errp)
	LCL_FILE	*stream;
	OBJ_ID_T	*objp;
	int		*errp;
#endif	/* NO_PP */
{
ALENGTH_T leng;
(void) A_DecodeTypeValue(stream, errp);
leng = A_DecodeLength(stream, errp);
if (*errp == 0)
   then A_DecodeObjectIdData(stream, leng, objp, errp);
return;
}
