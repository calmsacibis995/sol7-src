/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_default_msg.h	1.2 96/10/02 Sun Microsystems"

/******************************************************************************/
/* DMI SL default message strings                                             */
/******************************************************************************/
#ifndef OS_DEFAULT_MSG
#define OS_DEFAULT_MSG
/* default message strings */
/* $set DMI */
#define DEFAULT_CASE_D      "Default case in %s\n"
#define API_DEFAULT_CASE_D  "Default case in %s\n   Packet address is %x\n   Type %x   Flags %x\n"
#define OVL_UNKNOWN_FUNC_D  "Unknown function specified\n"
#define OS_SL_COPYRIGHT_D   "(C) Copyright IBM Corp. 1994, 1995.  All Rights Reserved.\n\n"
#define OS_SL_TERMINATED_D  "DMI service layer terminated.  RC = %x\n"
#define MEM_ALLOC_ERROR_D   "Error in memory allocation!!  %i bytes requested but none allocated.\n"
#define MEM_FREE_ERROR1_D   "Invalid address %x not freed.\n"
#define MEM_FREE_ERROR2_D   "%x not freed; not currently allocated\n"
#define MEM_FREE_ERROR3_D   "%x not freed, not in use\n"
#define TM_REQUEUE_ERROR_D  "REQUEUE error in os_tm.c: ----->Task not found???\n"
#define TM_DBUNLOCK_ERROR_D "MIF_componentLockClear() FAILED in tm_free()\n"
#define OS_SL_VER_D         "AIX 4.1 DMI Service Layer, Version 1.4\n"
#define MAPPER_VERSION_D    "\nIBM AIX DMI to SNMP Translator, version 1.00\n"

/* $set API */
#define WRONG_API_LEVEL_D   "This application is not linked with the correct DMI libraries.\n"

/* $
$ The following are the error messages returned by the DMTF Service Layer for AIX
$ during a new component compile.
$
$ NOTE: The order of this set is very important!  Altering this order without
$       also changing the code will cause the code to malfunction.
$
$set COMPONENT */
#ifdef MIFTOMIB
static char *compDefault[] = {
   "INFO (Line %i, Col %i) Not currently implemented\n",
   "INFO (Line %i, Col %i) There were template(s) defined, but not used\n",
   "INFO (Line %i, Col %i) Class string may be ill-formed\n",
   "WARNING (Line %i, Col %i) Illegal character in comment\n",
   "WARNING (Line %i, Col %i) Expecting a ')' in an attribute type statement\n",
   "WARNING (Line %i, Col %i) Required ComponentID group missing\n",
   "WARNING (Line %i, Col %i) This ID is reserved for the ComponentID group\n",
   "WARNING (Line %i, Col %i) Expecting the DMTF ComponentID class string for this group\n",
   "ERROR (Line %i, Col %i) Unexpected end of comment\n",
   "ERROR (Line %i, Col %i) Unexpected end of file in comment\n",
   "ERROR (Line %i, Col %i) Illegal character in escape sequence\n",
   "ERROR (Line %i, Col %i) Illegal digit in escape sequence\n",
   "ERROR (Line %i, Col %i) Illegal value in escape sequence\n",
   "ERROR (Line %i, Col %i) Missing digit in escape sequence\n",
   "ERROR (Line %i, Col %i) Illegal character within literal\n",
   "ERROR (Line %i, Col %i) Unexpected end of file in literal\n",
   "ERROR (Line %i, Col %i) Unexpected new line in literal\n",
   "ERROR (Line %i, Col %i) Illegal character in source stream\n",
   "ERROR (Line %i, Col %i) Only one component is allowed per MIF file\n",
   "ERROR (Line %i, Col %i) Illegal value encountered in attribute body\n",
   "ERROR (Line %i, Col %i) Expecting the name of this attribute\n",
   "ERROR (Line %i, Col %i) Expecting the ID of this attribute\n",
   "ERROR (Line %i, Col %i) Expecting the name or ID of this attribute\n",
   "ERROR (Line %i, Col %i) Expecting '=' in attribute body\n",
   "ERROR (Line %i, Col %i) Expecting a literal in attribute body\n",
   "ERROR (Line %i, Col %i) Expecting an integer in attribute body\n",
   "ERROR (Line %i, Col %i) Expecting a statement in attribute body\n",
   "ERROR (Line %i, Col %i) Expecting an overlay name for the attribute value\n",
   "ERROR (Line %i, Col %i) Expecting an attribute value\n",
   "ERROR (Line %i, Col %i) Attribute value has an illegal size\n",
   "ERROR (Line %i, Col %i) Access statement missing from attribute body\n",
   "ERROR (Line %i, Col %i) Type statement missing from attribute body\n",
   "ERROR (Line %i, Col %i) Value statement missing from attribute body\n",
   "ERROR (Line %i, Col %i) Size specifier missing for string attribute\n",
   "ERROR (Line %i, Col %i) Value conflicts with existing database\n",
   "ERROR (Line %i, Col %i) Duplicate statement encountered in attribute body\n",
   "ERROR (Line %i, Col %i) Cannot assign value to Write-Only attribute\n",
   "ERROR (Line %i, Col %i) Illegal value in enumeration body\n",
   "ERROR (Line %i, Col %i) Expecting a name for this enumeratin\n",
   "ERROR (Line %i, Col %i) Expecting an '=' in enumeration body\n",
   "ERROR (Line %i, Col %i) Expecting a literal in enumeration body\n",
   "ERROR (Line %i, Col %i) Expecting a statement in enumeration body\n",
   "ERROR (Line %i, Col %i) Expecting a value in enumeration body\n",
   "ERROR (Line %i, Col %i) This enumeration had no statements\n",
   "ERROR (Line %i, Col %i) Value conflicts with existing database\n",
   "ERROR (Line %i, Col %i) An enumeration of this name was already encountered\n",
   "ERROR (Line %i, Col %i) This feature is not currently implemented for enumerations\n",
   "ERROR (Line %i, Col %i) Illegal value in component body\n",
   "ERROR (Line %i, Col %i) Expecting component definition\n",
   "ERROR (Line %i, Col %i) Expecting the name of this component\n",
   "ERROR (Line %i, Col %i) Expecting the name of this component\n",
   "ERROR (Line %i, Col %i) Expecting '=' in component body\n",
   "ERROR (Line %i, Col %i) Expecting a literal in component body\n",
   "ERROR (Line %i, Col %i) Expecting a statement in component body\n",
   "ERROR (Line %i, Col %i) Expecting a block statement in component body\n",
   "ERROR (Line %i, Col %i) This component had no recognizable groups\n",
   "ERROR (Line %i, Col %i) Duplicate statement encountered in component body\n",
   "ERROR (Line %i, Col %i) Illegal value in table body\n",
   "ERROR (Line %i, Col %i) Expecting the name of this table\n",
   "ERROR (Line %i, Col %i) Expecting the class, ID, or name of this table\n",
   "ERROR (Line %i, Col %i) Expecting a '=' in table definition\n",
   "ERROR (Line %i, Col %i) Expecting a literal in table definition\n",
   "ERROR (Line %i, Col %i) Expecting an integer in table definition\n",
   "ERROR (Line %i, Col %i) Expecting a statement in table body\n",
   "ERROR (Line %i, Col %i) Expecting an overlay name for a table data entry\n",
   "ERROR (Line %i, Col %i) Expecting a table entry value\n",
   "ERROR (Line %i, Col %i) Table string value exceeds size specified in attribute\n",
   "ERROR (Line %i, Col %i) This table contained no data entries\n",
   "ERROR (Line %1, Col %2) Expecting additional values in table row\n",
   "ERROR (Line %i, Col %i) There is already a group or table with this ID\n",
   "ERROR (Line %i, Col %i) Duplicate stement encountered in table definition\n",
   "ERROR (Line %i, Col %i) Expecting the class name of a template\n",
   "ERROR (Line %i, Col %i) The number of values exceeds the number of attributes\n",
   "ERROR (Line %i, Col %i) Expecting a ',' or '}' after a table data entry\n",
   "ERROR (Line %i, Col %i) Tables must be associated with keyed groups\n",
   "ERROR (Line %i, Col %i) Row values cannot reside in both the database and instrumentation\n",
   "ERROR (Line %i, Col %i) Key is not unique\n",
   "ERROR (Line %i, Col %i) There is no default value in the template\n",
   "ERROR (Line %i, Col %i) Illegal value in group body\n",
   "ERROR (Line %i, Col %i) Expecting the name of this group\n",
   "ERROR (Line %i, Col %i) Expecting the ID of this group\n",
   "ERROR (Line %i, Col %i) Expecting the class, name, or ID of this group\n",
   "ERROR (Line %i, Col %i) Expecting '=' in group body\n",
   "ERROR (Line %i, Col %i) Expecting a literal in group body\n",
   "ERROR (Line %i, Col %i) Expecting an integer in group body\n",
   "ERROR (Line %i, Col %i) Expecting a statement in group body\n",
   "ERROR (Line %i, Col %i) Expecting a block statement in group body\n",
   "ERROR (Line %i, Col %i) This group had no recognizable attributes\n",
   "ERROR (Line %i, Col %i) Value conflicts with existing database\n",
   "ERROR (Line %i, Col %i) Duplicate statement encountered in group body\n",
   "ERROR (Line %i, Col %i) Class statement missing from group body\n",
   "ERROR (Line %i, Col %i) Unknown attribute ID specified in key statement\n",
   "ERROR (Line %i, Col %i) Expecting an attribute ID in key statement\n",
   "ERROR (Line %i, Col %i) Expecting '=' in key statement\n",
   "ERROR (Line %i, Col %i) Expecting an attribute ID in key statement\n",
   "ERROR (Line %i, Col %i) Duplicate ID encountered in key statement\n",
   "ERROR (Line %i, Col %i) Illegal value encountered in component paths body\n",
   "ERROR (Line %i, Col %i) Expecting a Name or Environment definition\n",
   "ERROR (Line %i, Col %i) Expecting a Name definition\n",
   "ERROR (Line %i, Col %i) Expecting '=' in component paths body\n",
   "ERROR (Line %i, Col %i) Expecting a literal in component paths body\n",
   "ERROR (Line %i, Col %i) Expecting a statement in component path body\n",
   "ERROR (Line %i, Col %i) Expecting 'End'\n",
   "ERROR (Line %i, Col %i) Expecting a path literal or \"Direct-Interface\"\n",
   "ERROR (Line %i, Col %i) Required definitions were missing from component path body\n",
   "ERROR (Line %i, Col %i) This attribute has no instrumentation for the current OS environment\n",
   "ERROR (Line %i, Col %i) Value conflicts with existing database\n",
   "ERROR (Line %i, Col %i) Duplicate statement in component paths body\n",
   "ERROR (Line %i, Col %i) Expecting: DOS, OS2, UNIX, WIN16, or WIN32\n",
   "ERROR (Line %i, Col %i) Templates must have unique class strings\n",
   "ERROR (Line %i, Col %i) Templates are required to be keyed\n",
   "ERROR (Line %i, Col %i) Unexpected EOF in source stream\n",
   "ERROR (Line %i, Col %i) Database fault during attribute creation\n",
   "ERROR (Line %1, Col %2) No memory to build attribute structure\n",
   "ERROR (Line %i, Col %i) Database fault during enumeration creation\n",
   "FATAL (Line %i, Col %i) Database fault during component creation\n",
   "FATAL (Line %i, Col %i) Database fault during table creation\n",
   "FATAL (Line %i, Col %i) Database fault during group creation\n",
   "ERROR (Line %1, Col %2) No memory to create group data structure\n",
   "FATAL (Line %i, Col %i) Database fault during key creation\n",
   "FATAL (Line %i, Col %i) Memory fault during key creation\n",
   "FATAL (Line %i, Col %i) Database fault during component path creation\n",
   "FATAL (Line %i, Col %i) Database fault\n",
   "FATAL (Line %i, Col %i) FATAL file ERROR\n",
   "FATAL (Line %i, Col %i) Unknown ERROR\n" };
#endif
#endif
