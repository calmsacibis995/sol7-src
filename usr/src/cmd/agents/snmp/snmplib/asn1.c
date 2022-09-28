/* Copyright 04/24/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)asn1.c	1.4 97/04/24 Sun Microsystems"

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "snmp_msg.h"
#include "asn1.h"


/*
 * asn_parse_int - pulls a long out of an ASN int type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_int(
    register u_char	    *data,	/* IN - pointer to start of object */
    register int	    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char		    *type,	/* OUT - asn type of object */
    long		    *intp,	/* IN/OUT - pointer to start of output buffer */
    int			    intsize,    /* IN - size of output buffer */
	char *error_label)
{
/*
 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
 */
    register u_char *bufp = data;
    u_long	    asn_length = 0;
    register long   value = 0;


	error_label[0] = '\0';

    if (intsize != sizeof (long)){
	sprintf(error_label, ERR_MSG_NOT_LONG);
	return NULL;
    }
    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length, error_label);
    if (bufp == NULL){
	sprintf(error_label, ERR_MSG_BAD_LENGTH);
	return NULL;
    }
    if (asn_length + (bufp - data) > *datalength){
	sprintf(error_label, ERR_MSG_OVERFLOW);
	return NULL;
    }
    if (asn_length > intsize){
	sprintf(error_label, ERR_MSG_DONT_SUPPORT_LARGE_INT);
	return NULL;
    }
    *datalength -= (int)asn_length + (bufp - data);
    if (*bufp & 0x80)
	value = -1; /* integer is negative */
    while(asn_length--)
	value = (value << 8) | *bufp++;
    *intp = value;
    return bufp;
}

/*
 * asn_parse_unsigned_int - pulls an unsigned long out of an ASN int type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_unsigned_int(
    register u_char         *data,      /* IN - pointer to start of object */
    register int            *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char                  *type,      /* OUT - asn type of object */
    u_long                  *intp,      /* IN/OUT - pointer to start of output buffer */
    int                     intsize,    /* IN - size of output buffer */
    char                   *error_label)
{
/*
 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
 */
    register u_char *bufp = data;
    u_long          asn_length;
    register u_long value = 0;

    error_label[0] = '\0';

    if (intsize != sizeof (long)){
	sprintf(error_label, ERR_MSG_NOT_LONG);
	return NULL;
    }
    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length, error_label);
    if (bufp == NULL){
	sprintf(error_label, ERR_MSG_BAD_LENGTH);
        return NULL;
    }
    if (asn_length + (bufp - data) > *datalength){
	sprintf(error_label, ERR_MSG_OVERFLOW);
        return NULL;
    }
    if ((asn_length > (intsize + 1)) ||
        ((asn_length == intsize + 1) && *bufp != 0x00)){
	sprintf(error_label, ERR_MSG_DONT_SUPPORT_LARGE_INT);
        return NULL;
    }
    *datalength -= (int)asn_length + (bufp - data);
    if (*bufp & 0x80)
        value = -1; /* integer is negative */
    while(asn_length--)
        value = (value << 8) | *bufp++;
    *intp = value;
    return bufp;
}


/*
 * asn_build_int - builds an ASN object containing an integer.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_build_int(
    register u_char *data,	/* IN - pointer to start of output buffer */
    register int    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char	    type,	/* IN - asn type of object */
    register long   *intp,	/* IN - pointer to start of long integer */
    register int    intsize,    /* IN - size of *intp */
	char *error_label)
{
/*
 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
 */

    register long integer;
    register u_long mask;


	error_label[0] = '\0';

    if (intsize != sizeof (long))
	return NULL;
    integer = *intp;
    /*
     * Truncate "unnecessary" bytes off of the most significant end of this 2's complement integer.
     * There should be no sequence of 9 consecutive 1's or 0's at the most significant end of the
     * integer.
     */

/*
 * Olivier Reisacher 95/04/20
 */
/*
    mask = 0x1FF << ((8 * (sizeof(long) - 1)) - 1);
*/
	mask = ((u_long) 0x1FF) << ((8 * (sizeof(long) - 1)) - 1);

    /* mask is 0xFF800000 on a big-endian machine */
    while((((integer & mask) == 0) || ((integer & mask) == mask)) && intsize > 1){
	intsize--;
	integer <<= 8;
    }
    data = asn_build_header(data, datalength, type, intsize, error_label);
    if (data == NULL)
	return NULL;
    if (*datalength < intsize)
	return NULL;
    *datalength -= intsize;

/*
 * Olivier Reisacher 95/04/20
 */
/*
    mask = 0xFF << (8 * (sizeof(long) - 1));
*/
	mask = ((u_long) 0xFF) << (8 * (sizeof(long) - 1));

    /* mask is 0xFF000000 on a big-endian machine */
    while(intsize--){
	*data++ = (u_char)((integer & mask) >> (8 * (sizeof(long) - 1)));
	integer <<= 8;
    }
    return data;
}

/*
 * asn_build_unsigned_int - builds an ASN object containing an integer.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_build_unsigned_int(
    register u_char *data,      /* IN - pointer to start of output buffer */
    register int    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char          type,       /* IN - asn type of object */
    register u_long *intp,      /* IN - pointer to start of long integer */
    register int    intsize,    /* IN - size of *intp */
    char            *error_label)
{
/*
 * ASN.1 integer ::= 0x02 asnlength byte {byte}*
 */

    register u_long integer;
    register u_long mask;
    int add_null_byte = 0;

    error_label[0] = '\0';

    if (intsize != sizeof (long))
        return NULL;
    integer = *intp;
    mask = ((u_long) 0xFF) << (8 * (sizeof(long) - 1));
    /* mask is 0xFF000000 on a big-endian machine */
    if ((u_char)((integer & mask) >> (8 * (sizeof(long) - 1))) & 0x80){
        /* if MSB is set */
        add_null_byte = 1;
        intsize++;
    } else {
        /*
         * Truncate "unnecessary" bytes off of the most significant end of this 2's complement integer.
         * There should be no sequence of 9 consecutive 1's or 0's at the most significant end of the
         * integer.
         */
        mask = ((u_long) 0x1FF) << ((8 * (sizeof(long) - 1)) - 1);
        /* mask is 0xFF800000 on a big-endian machine */
        while(((integer & mask) == 0) && intsize > 1){
            intsize--;
            integer <<= 8;
        }
    }
    data = asn_build_header(data, datalength, type, intsize, error_label);
    if (data == NULL)
        return NULL;
    if (*datalength < intsize)
        return NULL;
    *datalength -= intsize;
    if (add_null_byte == 1){
        *data++ = '\0';
        intsize--;
    }
    mask = ((u_long) 0xFF) << (8 * (sizeof(long) - 1));
    /* mask is 0xFF000000 on a big-endian machine */
    while(intsize--){
        *data++ = (u_char)((integer & mask) >> (8 * (sizeof(long) - 1)));
        integer <<= 8;
    }
    return data;
}


/*
 * asn_parse_string - pulls an octet string out of an ASN octet string type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "string" is filled with the octet string.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_string(
    u_char	    *data,	    /* IN - pointer to start of object */
    register int    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    *type,	    /* OUT - asn type of object */
    u_char	    *string,	    /* IN/OUT - pointer to start of output buffer */
    register int    *strlength,     /* IN/OUT - size of output buffer */
	char *error_label)
{
/*
 * ASN.1 octet string ::= primstring | cmpdstring
 * primstring ::= 0x04 asnlength byte {byte}*
 * cmpdstring ::= 0x24 asnlength string {string}*
 * This doesn't yet support the compound string.
 */
    register u_char *bufp = data;
    u_long	    asn_length = 0;


	error_label[0] = '\0';

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length, error_label);
    if (bufp == NULL)
	return NULL;
    if (asn_length + (bufp - data) > *datalength){
	sprintf(error_label, ERR_MSG_OVERFLOW);
	return NULL;
    }
    if (asn_length > *strlength){
	sprintf(error_label, ERR_MSG_DONT_SUPPORT_LARGE_STR);
	return NULL;
    }
    memcpy(string, bufp, (int) asn_length);
    *strlength = (int)asn_length;
    *datalength -= (int)asn_length + (bufp - data);
    return bufp + asn_length;
}


/*
 * asn_build_string - Builds an ASN octet string object containing the input string.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_build_string(
    u_char	    *data,	    /* IN - pointer to start of object */
    register int    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    type,	    /* IN - ASN type of string */
    u_char	    *string,	    /* IN - pointer to start of input buffer */
    register int    strlength,	    /* IN - size of input buffer */
	char *error_label)
{
/*
 * ASN.1 octet string ::= primstring | cmpdstring
 * primstring ::= 0x04 asnlength byte {byte}*
 * cmpdstring ::= 0x24 asnlength string {string}*
 * This code will never send a compound string.
 */

	error_label[0] = '\0';

    data = asn_build_header(data, datalength, type, strlength, error_label);
    if (data == NULL)
	return NULL;
    if (*datalength < strlength)
	return NULL;
    memcpy(data, string, strlength);
    *datalength -= strlength;
    return data + strlength;
}


/*
 * asn_parse_header - interprets the ID and length of the current object.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 */
u_char *
asn_parse_header(
    u_char	    *data,	/* IN - pointer to start of object */
    int		    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char	    *type,	/* OUT - ASN type of object */
	char *error_label)
{
    register u_char *bufp = data;
    register	    header_len;
    u_long	    asn_length = 0;


	error_label[0] = '\0';

    /* this only works on data types < 30, i.e. no extension octets */
    if (IS_EXTENSION_ID(*bufp)){
	sprintf(error_label, ERR_MSG_CANT_PROCESS_LONG_ID);
	return NULL;
    }
    *type = *bufp;
    bufp = asn_parse_length(bufp + 1, &asn_length, error_label);
    if (bufp == NULL)
	return NULL;
    header_len = bufp - data;
    if (header_len + asn_length > *datalength){
	sprintf(error_label, ERR_MSG_ASN_LEN_TOO_LONG);
	return NULL;
    }
    *datalength = (int)asn_length;
    return bufp;
}

/*
 * asn_build_header - builds an ASN header for an object with the ID and
 * length specified.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  This only works on data types < 30, i.e. no extension octets.
 *  The maximum length is 0xFFFF;
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 */
u_char *
asn_build_header(
    register u_char *data,	/* IN - pointer to start of object */
    int		    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    u_char	    type,	/* IN - ASN type of object */
    int		    length,	/* IN - length of object */
	char *error_label)
{
	error_label[0] = '\0';

    if (*datalength < 1)
	return NULL;
    *data++ = type;
    (*datalength)--;
    return asn_build_length(data, datalength, length, error_label);
    
}

/*
 * asn_parse_length - interprets the length of the current object.
 *  On exit, length contains the value of this length field.
 *
 *  Returns a pointer to the first byte after this length
 *  field (aka: the start of the data field).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_length(
    u_char  *data,	/* IN - pointer to start of length field */
    u_long  *length,	/* OUT - value of length field */
	char *error_label)
{
    register u_char lengthbyte = *data;


	error_label[0] = '\0';

    if (lengthbyte & ASN_LONG_LEN){
	lengthbyte &= ~ASN_LONG_LEN;	/* turn MSb off */
	if (lengthbyte == 0){
		sprintf(error_label, ERR_MSG_DONT_SUPPORT_INDEF_LEN);
	    return NULL;
	}
	if (lengthbyte > sizeof(long)){
		sprintf(error_label, ERR_MSG_DONT_SUPPORT_SUCH_LEN);
	    return NULL;
	}
	memcpy(length, data + 1, (int)lengthbyte);
	*length = ntohl(*length);
	*length >>= (8 * ((sizeof *length) - lengthbyte));
	return data + lengthbyte + 1;
    } else { /* short asnlength */
	*length = (long)lengthbyte;
	return data + 1;
    }
}

u_char *
asn_build_length(
    register u_char *data,	/* IN - pointer to start of object */
    int		    *datalength,/* IN/OUT - number of valid bytes left in buffer */
    register int    length,	/* IN - length of object */
	char *error_label)
{
    u_char    *start_data = data;


	error_label[0] = '\0';

    /* no indefinite lengths sent */
    if (length < 0x80){
	*data++ = (u_char)length;
    } else if (length <= 0xFF){
	*data++ = (u_char)(0x01 | ASN_LONG_LEN);
	*data++ = (u_char)length;
    } else { /* 0xFF < length <= 0xFFFF */
	*data++ = (u_char)(0x02 | ASN_LONG_LEN);
	*data++ = (u_char)((length >> 8) & 0xFF);
	*data++ = (u_char)(length & 0xFF);
    }
    if (*datalength < (data - start_data)){
	sprintf(error_label, ERR_MSG_BUILD_LENGTH);
	return NULL;
    }
    *datalength -= (data - start_data);
    return data;

}

/*
 * asn_parse_objid - pulls an object indentifier out of an ASN object identifier type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "objid" is filled with the object identifier.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_objid(
    u_char	    *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    *type,	    /* OUT - ASN type of object */
    Subid	    *objid,	    /* IN/OUT - pointer to start of output buffer */
    int		    *objidlength,   /* IN/OUT - number of sub-id's in objid */
	char *error_label)
{
/*
 * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
 * subidentifier ::= {leadingbyte}* lastbyte
 * leadingbyte ::= 1 7bitvalue
 * lastbyte ::= 0 7bitvalue
 */
    register u_char *bufp = data;
    register Subid *oidp = objid + 1;
    register u_long subidentifier;
    register long   length;
    u_long	    asn_length = 0;


	error_label[0] = '\0';

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length, error_label);
    if (bufp == NULL)
	return NULL;
    if (asn_length + (bufp - data) > *datalength){
	sprintf(error_label, ERR_MSG_OVERFLOW);
	return NULL;
    }
    *datalength -= (int)asn_length + (bufp - data);

    length = asn_length;
    (*objidlength)--;	/* account for expansion of first byte */
    while (length > 0 && (*objidlength)-- > 0){
	subidentifier = 0;
	do {	/* shift and add in low order 7 bits */
	    subidentifier = (subidentifier << 7) + (*(u_char *)bufp & ~ASN_BIT8);
	    length--;
	} while (*(u_char *)bufp++ & ASN_BIT8);	/* last byte has high bit clear */
	if (subidentifier > (u_long)MAX_SUBID){
		sprintf(error_label, ERR_MSG_SUBIDENTIFIER_TOO_LONG);
	    return NULL;
	}
	*oidp++ = (Subid)subidentifier;
    }

    /*
     * The first two subidentifiers are encoded into the first component
     * with the value (X * 40) + Y, where:
     *	X is the value of the first subidentifier.
     *  Y is the value of the second subidentifier.
     */
    subidentifier = (u_long)objid[1];
    objid[1] = (u_char)(subidentifier % 40);
    objid[0] = (u_char)((subidentifier - objid[1]) / 40);

    *objidlength = (int)(oidp - objid);
    return bufp;
}

/*
 * asn_build_objid - Builds an ASN object identifier object containing the input string.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_build_objid(
    register u_char *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    type,	    /* IN - ASN type of object */
    Subid	    *objid,	    /* IN - pointer to start of input buffer */
    int		    objidlength,    /* IN - number of sub-id's in objid */
	char *error_label)
{
/*
 * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
 * subidentifier ::= {leadingbyte}* lastbyte
 * leadingbyte ::= 1 7bitvalue
 * lastbyte ::= 0 7bitvalue
 */
    u_char buf[MAX_OID_LEN];
    u_char *bp = buf;
    Subid objbuf[MAX_OID_LEN];
    Subid *op = objbuf;
    register int    asnlength;
    register u_long subid, mask, testmask;
    register int bits, testbits;


	error_label[0] = '\0';

    memcpy(objbuf, objid, objidlength * sizeof(Subid));
    /* transform size in bytes to size in subid's */
    /* encode the first two components into the first subidentifier */
    op[1] = op[1] + (op[0] * 40);
    op++;
    objidlength--;

    while(objidlength-- > 0){
	subid = *op++;
	mask = 0x7F; /* handle subid == 0 case */
	bits = 0;
	/* testmask *MUST* !!!! be of an unsigned type */
	for(testmask = 0x7F, testbits = 0; testmask != 0; testmask <<= 7, testbits += 7){
	    if (subid & testmask){	/* if any bits set */
		mask = testmask;
		bits = testbits;
	    }
	}
	/* mask can't be zero here */
	for(;mask != 0x7F; mask >>= 7, bits -= 7){
	    if (mask == 0x1E00000)  /* fix a mask that got truncated above */
		mask = 0xFE00000;
	    *bp++ = (u_char)(((subid & mask) >> bits) | ASN_BIT8);
	}
	*bp++ = (u_char)(subid & mask);
    }
    asnlength = bp - buf;
    data = asn_build_header(data, datalength, type, asnlength, error_label);
    if (data == NULL)
	return NULL;
    if (*datalength < asnlength)
	return NULL;
    memcpy(data, buf, asnlength);
    *datalength -= asnlength;
    return data + asnlength;
}

/*
 * asn_parse_null - Interprets an ASN null type.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_parse_null(
    u_char	    *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    *type,	    /* OUT - ASN type of object */
	char *error_label)
{
/*
 * ASN.1 null ::= 0x05 0x00
 */
    register u_char   *bufp = data;
    u_long	    asn_length = 0;


	error_label[0] = '\0';

    *type = *bufp++;
    bufp = asn_parse_length(bufp, &asn_length, error_label);
    if (bufp == NULL)
	return NULL;
    if (asn_length != 0){
	sprintf(error_label, ERR_MSG_MALFORMED_NULL);
	return NULL;
    }
    *datalength -= (bufp - data);
    return bufp + asn_length;
}


/*
 * asn_build_null - Builds an ASN null object.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 */
u_char *
asn_build_null(
    u_char	    *data,	    /* IN - pointer to start of object */
    int		    *datalength,    /* IN/OUT - number of valid bytes left in buffer */
    u_char	    type,	    /* IN - ASN type of object */
	char *error_label)
{
/*
 * ASN.1 null ::= 0x05 0x00
 */
	error_label[0] = '\0';

    return asn_build_header(data, datalength, type, 0, error_label);
}

