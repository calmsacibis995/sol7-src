/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi_print.hh	1.3 96/09/11 Sun Microsystems"

#ifndef _DMI_PRINT_HH_
#define _DMI_PRINT_HH_

typedef FILE* StoreType;

extern void print_mif(StoreType st,Component *comp,int indent);
extern void print_component_and_group_only(StoreType st,Component *comp,int indent);
extern void print_group_only(StoreType st,Component *comp,int indent);
extern void print_group(StoreType st,Group *group,int indent);
extern void print_table_only(StoreType st,Component *comp,int indent);

#endif
