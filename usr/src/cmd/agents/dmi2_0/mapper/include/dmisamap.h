/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisamap.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************/
/*                                                                    */
/*  dmisamap.h  -                                                     */
/*                                                                    */
/*  Author: Joonho Park                                               */
/*                                                                    */
/**********************************************************************/

#ifndef BUILD_INCLUDE
#define BUILD_INCLUDE

struct _line {
   char   * oid;        /* null terminated */
   int    sequentialKeys; /* nonzero means yes */
   int    field2;
   int    field3;
   int    field4;
   char   * compo_name; /* null terminated */
   int    key_count;
   char   * key;        /* null terminated */
   struct _line * next;
} line;

/* struct _line *mapPtr1;  ** for use by caller of buildMap function*/

int buildMap (struct _line **mapPtr1);

#endif

