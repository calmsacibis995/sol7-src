/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)encode.c	2.16 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)encode.c	2.16 96/10/02 Sun Microsystems";
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

/* $Header:   E:/snmpv2/snmp/encode.c_v   2.1   23 Nov 1990 13:45:48  $	*/
/*
 * $Log:   E:/snmpv2/snmp/encode.c_v  $
 * 
 *    Rev 2.1   23 Nov 1990 13:45:48
 * Added a cast to calls to A_EncodeType() on the flags parameter
 * to satisfy Microsoft C version 6.
 * 
 *    Rev 2.0   31 Mar 1990 15:06:46
 * Release 2.00
 * 
 *    Rev 1.5   05 Jun 1989  0:11:16
 * Corrected: error in encoding unsigned integers which have the high-order
 * bit set (value >= 2,147,483,648).
 * 
 *    Rev 1.4   28 Apr 1989 16:51:44
 * Added protection against encoding null object identifiers.
 * 
 *    Rev 1.3   17 Mar 1989 21:41:34
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.2   19 Sep 1988 17:26:52
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:04
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:56
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <buffer.h>
#include <encode.h>

#define	then

/* Turn on the following define to make A_SizeOfSubId() into a macro */
#define SUBID_MACRO

#if !defined(NO_PP)
#if !defined(SUBID_MACRO)
static	ALENGTH_T	A_SizeOfSubId(OIDC_T);
#endif	/* SUBID_MACRO */
static	void		A_EncodeSubId(OIDC_T, EHELPER_T, OCTET_T *);
#else	/* NO_PP */
#if !defined(SUBID_MACRO)
static	ALENGTH_T	A_SizeOfSubId();
#endif	/* SUBID_MACRO */
static	void		A_EncodeSubId();
#endif	/* NO_PP */

/****************************************************************************
A_SizeOfInt -- Return total size that an integer would occupy when
               ASN.1 encoded (tag and length fields are not included).

Parameters:
        INT_32_T    The integer (signed 32 bit)

Returns: ALENGTH_T  Number of octets the integer would occupy if
		    in ASN.1 encoding
****************************************************************************/
#if !defined(NO_PP)
ALENGTH_T
A_SizeOfInt(INT_32_T i)
#else	/* NO_PP */
ALENGTH_T
A_SizeOfInt(i)
    INT_32_T i;
#endif	/* NO_PP */
{
if (i >= 0L)
   then return (i <= 0x0000007F ? 1 :		/* <= 127	*/
	       (i <= 0x00007FFF ? 2 :		/* <= 32767	*/
	       (i <= 0x007FFFFF ? 3 :		/* <= 8388607	*/
	        4)));				/* > 8388607	*/
   else return (i >= (INT_32_T)0xFFFFFF80 ? 1 :	/* >= -128	*/
	       (i >= (INT_32_T)0XFFFF8000 ? 2 :	/* >= -32768	*/
	       (i >= (INT_32_T)0XFF800000 ? 3 :	/* >= -8388608	*/
		4)));				/* < -8388608	*/
}

/****************************************************************************
A_SizeOfUnsignedInt -- Return total size that an unsigned integer would
		       occupy when ASN.1 encoded (tag and length fields
		       are not included).

Parameters:
        UINT_32_T    The integer (unsigned 32 bit)

Returns: ALENGTH_T  Number of octets the integer would occupy if
		    in ASN.1 encoding
****************************************************************************/
#if !defined(NO_PP)
ALENGTH_T
A_SizeOfUnsignedInt(UINT_32_T i)
#else	/* NO_PP */
ALENGTH_T
A_SizeOfUnsignedInt(i)
    UINT_32_T i;
#endif	/* NO_PP */
{
return (i <= 0x0000007F ? 1 :		/* <= 127	*/
       (i <= 0x00007FFF ? 2 :		/* <= 32768	*/
       (i <= 0x007FFFFF ? 3 :		/* <= 8388607	*/
       (i <= 0x7FFFFFFF ? 4 :		/* <= 2147483647*/
	5))));				/* >  2147483647*/
}

/****************************************************************************
A_SizeOfSubId -- Compute the number of bytes required to hold a
		 subidentifier from an object id.
		 ASN.1 encoded (tag and length fields are not included)

Parameters:
        OIDC_T

Returns: ALENGTH_T  Number of octets needed in ASN.1 encoding
****************************************************************************/
#if !defined(SUBID_MACRO)
#if !defined(NO_PP)
static
ALENGTH_T
A_SizeOfSubId(OIDC_T i)
#else	/* NO_PP */
static
ALENGTH_T
A_SizeOfSubId(i)
    OIDC_T i;
#endif	/* NO_PP */
{
#if defined(OIDC_32)
return (i <= 0x7F ? 1 : (i <= 0x3FFF ? 2 : (i <= 0x1FFFFF ? 3 :
       (i <= 0x0FFFFFFF ? 4 : 5))));
#else
return (i <= 127 ? 1 : (i <= 16383 ? 2 : 3));
#endif	/* OIDC_32 */
}
#else	/* SUBID_MACRO */
#if defined(OIDC_32)
#define A_SizeOfSubId(I)  (ALENGTH_T)((OIDC_T)(I) <= 0x7F ? 1 :		\
			  ((OIDC_T)(I) <= 0x3FFF ? 2 :		\
			  ((OIDC_T)(I) <= 0x1FFFFF ? 3 :	\
			  ((OIDC_T)(I) <= 0x0FFFFFFF ? 4 : 5))))
#else
#define A_SizeOfSubId(I)  (ALENGTH_T)((OIDC_T)(I) <= 127 ? 1 :	\
			  ((OIDC_T)(I) <= 16383 ? 2 : 3))
#endif	/* OIDC_32 */
#endif	/* SUBID_MACRO */

/****************************************************************************
A_SizeOfObjectId -- Return total size that an object ID would occupy when
                ASN.1 encoded (tag and length fields are not included)

Parameters:
	OBJ_ID_T *	Pointer to the internal object Id structure

Returns: ALENGTH_T  Number of octets the object ID would occupy if
		    in ASN.1 encoding

Note: It is assumed by this routine that the object identifier has at least
two components.
****************************************************************************/
#if !defined(NO_PP)
ALENGTH_T
A_SizeOfObjectId(OBJ_ID_T *	objp)
#else	/* NO_PP */
ALENGTH_T
A_SizeOfObjectId(objp)
    OBJ_ID_T	*objp;
#endif	/* NO_PP */
{
ALENGTH_T leng;
OIDC_T *cp = objp->component_list;
int i;
OIDC_T x;

if (objp->num_components == 0) then return (ALENGTH_T)0;

/* Compute the value of the first subidentifier from the values of the	*/
/* first two components.						*/
   {
   x = *cp++;
   x = x * 40 + *cp++;
   leng = A_SizeOfSubId(x);
   }

for (i = 2; i < objp->num_components; i++)
   {
   x = *cp++;
   leng += A_SizeOfSubId(x);
   }
return leng;
}

/****************************************************************************
A_EncodeType -- Encode an ASN.1 type field into buffer.

Parameters:
        ATVALUE_T        The type value
        OCTET_T          A_IDCF_MASK flag values
        EHELPER_T        Function to be called to take generated data
	OCTET_T *	 Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing
****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeType(ATVALUE_T	id,
	     OCTET_T	flags,
	     EHELPER_T	func,
	     OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeType(id, flags, func, funcparm)
    ATVALUE_T	id;
    OCTET_T	flags;
    ALENGTH_T   (*func)();
    OCTET_T     *funcparm;
#endif	/* NO_PP */
{
flags &= A_IDCF_MASK;
if (id <= (ATVALUE_T)30)
   then {
	OCTET_T c;
        c = flags | (OCTET_T) id;
        (void)(*func)((EBUFFER_T *)funcparm, &c, (ALENGTH_T)sizeof(OCTET_T));
        }
   else {
        /* Build a partial reverse order version of the result and then */
        /* reverse it again back to correct order */
        OCTET_T buff[5], reverse[4];  /* Can't handle more than 4 octets */
	OCTET_T *bp = buff;
        OCTET_T *rp = reverse;
        unsigned short int count = 0;      /* Should never exceed 4 */
	ALENGTH_T cnt;
        *bp++ = (flags & A_IDCF_MASK) | 0x1F;
        while (id > (ATVALUE_T)0)
            {
            *rp++ = (OCTET_T) id & 0x7F;
            id >>= 7;
            count++;
            }
        cnt = count + 1;
        while ((count--) > 1)
            {
            *bp++ = *(--rp) | 0x80;
            }
        *bp++ = *(--rp);
        (void)(*func)((EBUFFER_T *)funcparm, buff, cnt);
        }
}

/****************************************************************************
A_EncodeLength -- Encode an ASN.1 definite form length field into buffer.

Parameters:
        ALENGTH_T        Length to be encoded
        EHELPER_T        Function to be called to take generated data
	OCTET_T *	 Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing
****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeLength(ALENGTH_T	leng,
	       EHELPER_T	func,
	       OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeLength(leng, func, funcparm)
    ALENGTH_T	leng;
    ALENGTH_T   (*func)();
    OCTET_T     *funcparm;
#endif	/* NO_PP */
{
if (leng <= (ALENGTH_T)127)
   then {
	OCTET_T c;
        c = (OCTET_T) leng;
        (void)(*func)((EBUFFER_T *)funcparm, &c, (ALENGTH_T)sizeof(OCTET_T));
        }
   else {
        OCTET_T buff[OCTETS_PER_INT32+1];
        OCTET_T reverse[OCTETS_PER_INT32];
        OCTET_T *bp = buff;
        OCTET_T *rp = reverse;
        unsigned short int count = 0; /* Never exceeds OCTETS_PER_INT32 */
	ALENGTH_T cnt;
        while (leng > (ALENGTH_T) 0)
            {
            *rp++ = (OCTET_T) leng;
	    /*lint -e704	*/
            leng >>= 8;
	    /*lint +e704	*/
            count++;
            }
        *bp++ = (OCTET_T)(((OCTET_T) count) | (OCTET_T) 0x80);
	cnt = count + 1;
        while ((count--) > 0)
            {
            *bp++ = *(--rp);
            }
        (void)(*func)((EBUFFER_T *)funcparm, buff, cnt);
        }
}

/****************************************************************************
A_EncodeInt -- generate ASN.1 format of integer (WITH TYPE & LENGTH)

Parameters:
    ATVALUE_T        The type value
    OCTET_T          A_IDC_MASK flag values
    INT_32_T	     The integer to convert (signed 32 bit)
    EHELPER_T        Function to be called to take generated data
    OCTET_T *	     Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing

****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeInt(ATVALUE_T	id,
	    OCTET_T	flags,
	    INT_32_T	value,
	    EHELPER_T	func,
	    OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeInt(id, flags, value, func, funcparm)
    ATVALUE_T	id;
    OCTET_T	flags;
    INT_32_T	value;
    ALENGTH_T   (*func)();
    OCTET_T     *funcparm;
#endif	/* NO_PP */
{
ALENGTH_T leng;
OCTET_T   *rp;
OCTET_T	  buff[OCTETS_PER_INT32];

leng = A_SizeOfInt(value);
A_EncodeType(id, (OCTET_T)(flags & A_IDC_MASK), func, funcparm);
A_EncodeLength(leng, func, funcparm);
rp = buff + (unsigned short int)leng;
for(;;)
    {
    *(--rp) = (OCTET_T) value;
    if (rp == buff) break;
    /*lint -e704	*/
    value >>= 8;
    /*lint +e704	*/
    }

(void)(*func)((EBUFFER_T *)funcparm, buff, leng);
}

/****************************************************************************
A_EncodeUnsignedInt -- generate ASN.1 format of integer (WITH TYPE & LENGTH)
		       where the local form of the integer is unsigned.

Parameters:
    ATVALUE_T        The type value
    OCTET_T          A_IDC_MASK flag values
    UINT_32_T	     The integer to convert (unsigned 32 bit)
    EHELPER_T        Function to be called to take generated data
    OCTET_T *	     Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing

****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeUnsignedInt(ATVALUE_T	id,
		    OCTET_T	flags,
		    UINT_32_T	value,
  		    EHELPER_T	func,
		    OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeUnsignedInt(id, flags, value, func, funcparm)
    ATVALUE_T	id;
    OCTET_T	flags;
    UINT_32_T	value;
    ALENGTH_T   (*func)();
    OCTET_T     *funcparm;
#endif	/* NO_PP */
{
ALENGTH_T leng, xleng;
OCTET_T   *rp;
OCTET_T	  buff[OCTETS_PER_INT32+1];

leng = A_SizeOfUnsignedInt(value);
A_EncodeType(id, (OCTET_T)(flags & A_IDC_MASK), func, funcparm);
A_EncodeLength(leng, func, funcparm);
rp = buff + (unsigned short int)leng;
/* If the unsigned number takes 5 octets, the high order octet is merely */
/* a zero byte to hold the zero sign.					 */
for(xleng = leng; xleng--;)
    {
    *(--rp) = (OCTET_T) value;
    value >>= 8;  /* This better be shifting zeros into the high end!	*/
    }

(void)(*func)((EBUFFER_T *)funcparm, buff, leng);
}

/****************************************************************************
A_EncodeOctetString -- Generate ASN.1 format of octet string (WITH TYPE &
		       LENGTH)

Parameters:
    ATVALUE_T        The type value
    OCTET_T          A_IDC_MASK flag values
    OCTET_T *	     Address of the string
    ALENGTH_T	     Length of the string
    EHELPER_T        Function to be called to take generated data
    OCTET_T *	     Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeOctetString(ATVALUE_T	id,
		    OCTET_T	flags,
		    OCTET_T *	osp,
		    ALENGTH_T	oslen,
	            EHELPER_T	func,
		    OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeOctetString(id, flags, osp, oslen, func, funcparm)
    ATVALUE_T	id;
    OCTET_T	flags;
    OCTET_T	*osp;
    ALENGTH_T	oslen;
    ALENGTH_T	(*func)();
    OCTET_T	*funcparm;
#endif	/* NO_PP */
{
/* Do a primitive encoding */
A_EncodeType(id, (OCTET_T)(flags & A_IDC_MASK), func, funcparm);
A_EncodeLength(oslen, func, funcparm);
if (oslen != 0) then (void)(*func)((EBUFFER_T *)funcparm, osp, oslen);
}

/****************************************************************************
A_EncodeSubId -- generate ASN.1 format of a subidentifier from an
		 object identifier

Parameters:
    OIDC_T	The subidentifier to encode
    EHELPER_T        Function to be called to take generated data
    OCTET_T *	    	Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing

****************************************************************************/
#if !defined(NO_PP)
static
void
A_EncodeSubId(OIDC_T		value,
	      EHELPER_T		func,
	      OCTET_T *		funcparm)
#else	/* NO_PP */
static
void
A_EncodeSubId(value, func, funcparm)
    OIDC_T		value;
    ALENGTH_T		(*func)();
    OCTET_T    		*funcparm;
#endif	/* NO_PP */
{
ALENGTH_T leng;
OCTET_T   *rp;
OCTET_T	  buff[OCTETS_PER_INT32];
OCTET_T   last;

leng = A_SizeOfSubId(value);

for(rp = buff + leng, last = 0x00; rp != buff;)
    {
    *(--rp) = (OCTET_T)(value & 0x007F) | last;
    value >>= 7;
    last = 0x80;
    }

(void)(*func)((EBUFFER_T *)funcparm, buff, leng);
}

/****************************************************************************
A_EncodeObjectId -- generate ASN.1 format of Object ID (WITH TYPE & LENGTH)

Parameters:
    ATVALUE_T        The type value
    OCTET_T          A_IDC_MASK flag values
    OBJ_ID_T *	     Pointer to the internal object Id structure
    EHELPER_T        Function to be called to take generated data
    OCTET_T *	     Parameter to be passed unchanged to the function.

Notes:  The function whose address is passed as a parameter is called zero
or more times to take away some accumulated data.  The function is called
with these parameters:
        OCTET_T *   The parameter (funcparm) passed to this routine
        OCTET_T *   The buffer where the data resides
        ALENGTH_T   The number of octets in the buffer.

The function should return the number of octets consumed, type ALENGTH_T.
The function should return a zero if it has taken all the data it wants.

Returns: nothing

****************************************************************************/
#if !defined(NO_PP)
void
A_EncodeObjectId(ATVALUE_T	id,
		 OCTET_T	flags,
		 OBJ_ID_T *	objp,
  		 EHELPER_T	func,
		 OCTET_T *	funcparm)
#else	/* NO_PP */
void
A_EncodeObjectId(id, flags, objp, func, funcparm)
    ATVALUE_T	id;
    OCTET_T	flags;
    OBJ_ID_T	*objp;
    ALENGTH_T   (*func)();
    OCTET_T     *funcparm;
#endif	/* NO_PP */
{
ALENGTH_T leng;
OIDC_T *cp = objp->component_list;
int i;

leng = A_SizeOfObjectId(objp);
A_EncodeType(id, (OCTET_T)(flags & A_IDC_MASK), func, funcparm);
A_EncodeLength(leng, func, funcparm);

if (leng == (ALENGTH_T)0) then return;

/* Merge the first two components of the object identifier to form the	*/
/* first subidentifier.							*/
   {
   OIDC_T x;
   x = *cp++;
   x = x * 40 + *cp++;
   A_EncodeSubId(x, func, funcparm);
   }

/* Handle the remaining components, each as its own subidentifier.	*/
for (i = 2; i < objp->num_components; i++)
   {
   A_EncodeSubId(*cp++, func, funcparm);
   }
}

/****************************************************************************

NAME:  A_EncodeHelper

PURPOSE:  Collect encoded data from the ASN.1 encoding routines and
	  place it into a buffer.

PAREMETERS:
        EBUFFER_T * The "opaque" parameter (funcparm) passed to the encoding
		    routines.
        OCTET_T *   The buffer where the encoded data resides
        ALENGTH_T   The number of encoded octets in the buffer.

RETURNS:  Returns the number of octets consumed, zero is returned if
          no more data is desired.  (This may not, however, prevent
	  subsequent calls.)

RESTRICTIONS:  
	Can not handle length > 64K

BUGS:  

****************************************************************************/
#if !defined(NO_PP)
ALENGTH_T
A_EncodeHelper(EBUFFER_T *	ebp,
	       OCTET_T *	bp,
	       ALENGTH_T	leng)
#else	/* NO_PP */
ALENGTH_T
A_EncodeHelper(ebp, bp, leng)
	EBUFFER_T *ebp;
        OCTET_T   *bp;
        ALENGTH_T leng;
#endif	/* NO_PP */
{
ALENGTH_T actual;
actual = min(leng, ebp->remaining);
switch (actual)
   {
   case 0:
	break;

   case 1:
	*(ebp->next_bp++) = *bp;
	ebp->remaining--;
	break;

   case 2:
	*(ebp->next_bp++) = *bp++;
	*(ebp->next_bp++) = *bp;
	ebp->remaining -= 2;
	break;

   default:
	(void) memcpy(ebp->next_bp, bp, (unsigned int)actual);
	ebp->remaining -= actual;
	ebp->next_bp += (unsigned short)actual;
	break;
   }
return (actual);
}
