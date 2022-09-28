#ifndef _LISTING_HH
#define _LISTING_HH

/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)listing.hh	1.4 96/10/07 Sun Microsystems"
#ifdef __cplusplus
extern "C" {
#endif 	

extern bool_t
dmilistcomponents(DmiListComponentsIN *argp, DmiListComponentsOUT *result);

extern bool_t
dmilistgroups(DmiListGroupsIN *argp, DmiListGroupsOUT *result);

extern bool_t
dmilistattributes(DmiListAttributesIN *argp, DmiListAttributesOUT *result); 

extern bool_t
dmilistclassnames(DmiListClassNamesIN *argp, DmiListClassNamesOUT *result);

extern bool_t
dmilistcomponentsbyclass(DmiListComponentsByClassIN *argp,
						 DmiListComponentsByClassOUT *result);

extern bool_t
dmilistlanguages(DmiListLanguagesIN *argp, DmiListLanguagesOUT *result); 
#ifdef __cplusplus
}
#endif 	

#endif

