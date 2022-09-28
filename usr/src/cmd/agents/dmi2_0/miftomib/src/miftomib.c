/* Copyright 01/22/97 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)miftomib.c	1.8 97/01/22 Sun Microsystems"


/**********************************************************************
    Filename: miftomib.c

    Copyright (c) Intel, Inc. 1992,1993,1994

    Description: MIF to MIB Translator

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/miftomib.c 1.2 1994/10/28 08:46:34 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        8/25/94  aip    Creation date.
        10/28/94 aip    msvc-compliant.
	8/19/96  jwy	map file
	10/17/96 jwy	command line arg change
	10/18/96 jwy	get rid of "-e" option

************************* INCLUDES ***********************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <db_api.h>
/*#include "mif_io.h"  $MED*/
#include <pr_err.h>
/*#include "pr_list.h" $MED*/
#include <pr_main.h>
#include <pr_parse.h>
#include <pr_src.h>
#include <pr_tok.h>
/*#include "pr_bif.h"  $MED*/
#include "biftomib.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_MIB_EXTENSION "mib"

#ifdef AIX
#define _MAX_PATH    512
#define _MAX_FNAME   512
#define _MAX_EXT     512
#endif

/*********************************************************************/

/************************ GLOBALS ************************************/

FILE             *BIF;

char             DH_mifDbName[1];

char   Environment[512]; /* $MED got from dmisl.c */

/*********************************************************************/

/************************ PRIVATE ************************************/

static unsigned short PR_mibBuild(char *mibFile,
    char *enterpriseName, char *enterpriseValue); /* $MED */

/* jwy 8-16-96 */
static unsigned short PR_mapBuild(char *mapFile,
    char *enterpriseValue); /* $MED */

void GetEnvPath(char *stringin,char *stringout); /* $MED */
/*********************************************************************/

void print_usage()
{
        fprintf(stderr, "Usage: miftomib  "
                        "\"[<mif name>=]{<value> <value> ...}\" <mif pathname> "
                        "[<mib pathname>]\n");
}

void main(int argc, char **argv)
{
    char                 enterpriseName[256];
    char                 enterpriseValue[256]; /* $MED */
    char                 *binPath;
    char                 mifPathname[_MAX_PATH];
    char                 mifFilename[_MAX_FNAME];
    char                 mifExtension[_MAX_EXT];
    char                 mibPathname[_MAX_PATH];
    char                 mibFilename[_MAX_FNAME];
    char                 mibExtension[_MAX_EXT];
    char                 *p;
    char                 *q;
    static char          mifFile[_MAX_PATH];
    static char          mibFile[_MAX_PATH];
    static char          mapFile[_MAX_PATH];
    static char          errorLog[_MAX_PATH];
    static char          *fileNameTable[]        = {mifFile, NULL,
                                                    NULL, errorLog}; /* $MED */
    PR_ErrorClass_t      errorClass;
    MIF_Bool_t           foundOptionE;
    MIF_Bool_t           processingOptionE;
    int                  severity;
    MIF_Bool_t           foundEnterprise;
    MIF_Bool_t           foundMifName;
    MIF_Bool_t           foundMibName;
    int                  i;
    int                  j;
    int		         strLen;

    /* $MED next 3 lines from dmisl.c */
    memset(Environment,0,512);
    GetEnvPath("DMIPATH",Environment);

    enterpriseName[0]='\0';

#ifdef AIX
    chdir(Environment);
#else
    DosSetCurrentDir(Environment);  /* set up the environment path as our current working dir */
#endif

    if ((argc < 2) || (argc > 6)) {
	print_usage();
        exit(1);
    }
    binPath = argv[0];
    severity = 1;
    foundOptionE = MIF_FALSE;
    processingOptionE = MIF_FALSE;
    foundEnterprise = MIF_FALSE;
    foundMifName = MIF_FALSE;
    foundMibName = MIF_FALSE;
    for (i = 1; i < argc; ++i) {
        ++argv;
        if (**argv == '-') {
            if (processingOptionE) {
                fprintf(stderr, "Expecting severity level.\n");
                exit(1);
            }
            for (p = *argv; *p != '\0'; ++p) {
                ++p;
                switch (*p) {
                    case 'e':
                    case 'E':
                        if (foundOptionE || processingOptionE) {
                            fprintf(stderr, "E option already specified.\n");
                            exit(1);
                        }
                        processingOptionE = MIF_TRUE;
                        break;
                    case 'v':
                    case 'V':
                        fprintf(stderr, "Miftomib:\t%s\nMIB Generator:\t%s\n",
                                PARSER_VERSION, BIF_TO_MIB_VERSION);
                        exit(0);
                        break;
		    case 'h':
			print_usage();
			exit(0);
                    default:
                        fprintf(stderr, "Illegal option '%c'\n", *p);
                        exit(1);
                        break;
                }
            }
        } else if (processingOptionE) {
            if ((strlen(*argv) == 1) && (**argv >= '0') && (**argv <= '2')) {
                severity = **argv - '0';
                foundOptionE = MIF_TRUE;
                processingOptionE = MIF_FALSE;
            } else {
                fprintf(stderr, "Expecting 0-2.\n");
                exit(1);
            }
        } else {
            if (foundMibName) {
                fprintf(stderr, "Too many filenames specified.\n");
                exit(1);
            } else if (foundMifName) {
                if (((p = strrchr(*argv, '\\')) != (char *) 0) ||
                    ((p = strrchr(*argv, '/')) != (char *) 0)) {
                    strncpy(mibPathname, *argv, p - *argv + 1);
                    mibPathname[p - *argv + 1] = '\0';
                    ++p;
                } else {
                    mibPathname[0] = '\0';
                    p = *argv;
                }
                if ((q = strchr(p, '.')) != (char *) 0) {
                    strncpy(mibFilename, p, q - p);
                    mibFilename[q - p] = '\0';
                    strcpy(mibExtension, q + 1);
                } else {
                    strcpy(mibFilename, p);
                    strcpy(mibExtension, PR_MIB_EXTENSION);
                }
                foundMibName = MIF_TRUE;
            } else if (foundEnterprise) {
                if (((p = strrchr(*argv, '\\')) != (char *) 0) ||
                    ((p = strrchr(*argv, '/')) != (char *) 0)) {
                    strncpy(mifPathname, *argv, p - *argv + 1);
                    mifPathname[p - *argv + 1] = '\0';
                    ++p;
                } else {
                    mifPathname[0] = '\0';
                    p = *argv;
                }
                if ((q = strchr(p, '.')) != (char *) 0) {
                    strncpy(mifFilename, p, q - p);
                    mifFilename[q - p] = '\0';
                    strcpy(mifExtension, q + 1);
                } else {
                    strcpy(mifFilename, p);
                    strcpy(mifExtension, PR_SOURCE_EXTENSION);
                }
                foundMifName = MIF_TRUE;
            } else {
                p = *argv;
                if (strchr(p, '=') == (char *) 0) {
/*
                    fprintf(stderr, "Bad enterprise specification.\n");
                    exit(1);
*/
                }else{
                	if ((*p == '"') || (*p == '\''))
                   	 ++p;
               	 	for (j = 0; p[j] != '='; ++j)
               	     		enterpriseName[j] = p[j];
               		enterpriseName[j] = '\0';
               		p = strchr(p, '=') + 1;
		}
                strncpy(enterpriseValue, p, sizeof(enterpriseValue)); /* $MED Validate? */
                foundEnterprise = MIF_TRUE;
            }
        }
    }

    if(enterpriseName[0]=='\0' && foundMifName){
	strcpy(enterpriseName,mifFilename);
    }

    if (! foundMifName) {
        fprintf(stderr, "Filename not specified.\n");
        exit(1);
    }
    if (foundMibName)
        sprintf(mibFile, "%s%s.%s", mibPathname, mibFilename,
                mibExtension);
    else
        sprintf(mibFile, "%s%s.%s", mifPathname, mifFilename,
                PR_MIB_EXTENSION);

    sprintf(mifFile, "%s%s.%s", mifPathname, mifFilename,
            mifExtension);

#define PR_ERROR_EXTENSION    "err"     /* $MED move this */
    sprintf(errorLog, "%s%s.%s", mifPathname, mifFilename,
            PR_ERROR_EXTENSION);

    /* done parsing the input... $MED */

    PR_ParserInit();            /* clean up the memory we used...     $M    */

    if (PR_sourceOpen(mifFile) != MIF_OKAY) {
        fprintf(stderr, "Cannot open mif: %s.\n", mifFile);
        exit(1);
    }

    if (PR_errorLogCreate(errorLog) != MIF_OKAY) {
        fprintf(stderr, "Cannot create error log: %s.\n", errorLog);
        PR_sourceClose();
        exit(1);
    }

    errorClass = PR_tokenize();
    if (errorClass == 0) errorClass = PR_parse();
    PR_sourceClose(); /* moved this after parsing $MED */
    PR_errorLogClose();
    if (errorClass > 0) {
#if 0
$MED got rid of this for now
        switch (severity) {
            case 0:
                break;
            case 1:
                LI_list(binPath, (char *) 0, (char *) 0, errorLog);
                break;
            case 2:
                LI_list(binPath, mifFile, (char *) 0, errorLog);
                break;
        }
#endif
        if (errorClass > PR_WARN) {
            fprintf(stderr,"Errors encountered during parsing -- "
                    "component not translated.\n");
            exit(1);
        }
    }
    /* got rid of call to PR_bifBuild here $MED */

    /* can't rid of call to PR_mibBuild here $MED */
    if (PR_mibBuild(mibFile, enterpriseName, enterpriseValue) != 0) { /* $MED */
        fprintf(stderr, "Errors encountered during translation.\n");
    }

    strcpy(mapFile,mibFile);
    strLen = strlen(mibFile);
    mapFile[strLen-1] = 'p';
    mapFile[strLen-2] = 'a';
    mapFile[strLen-3] = 'm';
    if (PR_mapBuild(mapFile,enterpriseValue) != 0){
        fprintf(stderr, "Errors encountered during map file creation.\n");
    }

    PR_tokenTableClose();
    exit(0);
}

static unsigned short PR_mibBuild( /* got rid of char* bifFile parm $MED */
                                  char *mibFile,
                                  char *enterpriseName,
                                  char *enterpriseValue) /* $MED */
{

/*
    Now move the objects
*/

    if (bifToMib(mibFile, enterpriseName, enterpriseValue) != BifToMibSuccess) /* $MED */
        return 1;

/*
    Close the BIF
*/

/*//  BIF_close(BIF); $MED*/

    return 0;
}

static unsigned short PR_mapBuild( char *mapFile,
				   char *enterpriseValue)
{
  	FILE *fp;
	int i, seenDigit=0;
	DMI_STRING *str;

    	fp = fopen(mapFile, "w");
    	if (fp == (FILE *) 0){
        	fprintf(stderr,"Cannot open %s for writing.", mapFile);
		return 1;
	}
	
	/* convert  */
	if(enterpriseValue){
		fprintf(fp,"\"");
		for(i=0;i<strlen(enterpriseValue);i++){
			if(isdigit(enterpriseValue[i])){
				seenDigit = 1;
				fprintf(fp,"%c",enterpriseValue[i]);
			}else if((seenDigit==1) && (enterpriseValue[i] != '}')){
				fprintf(fp,".");
				seenDigit=0;
			}
		}

#if 0
/*removing the extra ".1" appended to the oid in the mapfile */
		if(seenDigit==1) fprintf(fp,".");
		/* append 1 after oid */
		fprintf(fp,"1\"\t");
#endif
                fprintf(fp,"\"\t");
	}

	/* print component name */
	str = (DMI_STRING*)PR_componentName();
	if(str){
		fprintf(fp,"\"");
		for(i=0;i<str->length;i++)
			fprintf(fp,"%c",str->body[i]);
		fprintf(fp,"\"");
	}

	fclose(fp);
  	return 0;
}

/* #include "biftodmi.h" $MED doesn't exist anymore */
/* stole the next 2 lines from the old biftodmi.h $MED */
typedef enum {BifToDmiFailure, BifToDmiSuccess} BifToDmiStatus_t;
BifToDmiStatus_t bifToDmi(void);

BifToDmiStatus_t bifToDmi(void)
{
    return BifToDmiFailure;
}

/* $MED new stuff from here down */


void GetEnvPath(char *stringin,char *stringout)
{
char *value;

    value = getenv(stringin);    /* get value of environment variable */
    if (value == NULL) stringout[0] = 0;   /* not in the env space */
    else strcpy (stringout,value); /* copy environment variable value to output buffer */
}

/* $MED made these changes to port to new parser                             */
/* - got rid of #include mif_io.h, pr_bif.h cuz these don't exist anymore    */
/* - got rid of all references to bifFile cuz bifs are history               */
/* - killed all refs to tokenTable                                           */
/* - got rid of #include biftodmi.h                                          */
/* -                                                                         */
/* -                                                                         */
/* -                                                                         */
/* -                                                                         */

