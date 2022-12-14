/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)gsscred_utils.c 1.14     97/11/12 SMI"

/*
 *
 *  gsscred utility
 *  Manages mapping between a security principal name and unix uid
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "gsscred.h"
#include "gsscred_xfn.h"


/*
 * optional configuration file which will select
 * which mechanism to use to store the gsscred table.
 * Available options are: flat file, under xfn using
 *	xfn files, xfn nis, and xfn nis+.
 * If the configuration file is not present then xfn will
 * used with the system wide configured mechanism.
 */
static const char MECH_OID_FILE[] = "/etc/gss/mech";
static const char gsscred_config_file[] = "/etc/gss/gsscred.conf";
static const char xfn_mech_files[] = "xfn_files";
static const char xfn_mech_nis[] = "xfn_nis";
static const char xfn_mech_nisplus[] = "xfn_nisplus";
static const char mech_flat_file[] = "file";


/*
 * GSS export name constants
 */
static const char *expNameTokId = "\x04\x01";
static const int expNameTokIdLen = 2;
static const int mechOidLenLen = 2;

/*
 * Array of structures to store mechanism string <-> OID mapping.
 * Based on the definitions in MECH_OID_FILE.
 */
#define	MAX_OID_MECH_PAIRS 32

struct mechDesc {
    char *name;
    char *oidStr;
    gss_OID oid;
};

static struct mechDesc mechs[MAX_OID_MECH_PAIRS+1] = { {NULL, NULL, NULL }};


/* local functions */
static int _gsscred_read_mech_file(void);


/*
 * Internal utility routines.
 */

/*
 * _gsscred_read_mech_file()
 *
 * This routine fetches mech and oid from "/etc/gss/mech".
 * And builds an array of mechName, mechOidStr, mechOid.
 */
static int _gsscred_read_mech_file(void)
{
	char 	buf[256];	/* one line from the file */
	char	*name, *next;
	char	*mechname, *oid_str;
	char 	*line;
	FILE 	*fd;
	int index = 0;
	OM_uint32 minor;
	gss_buffer_desc oidStrDesc;

	/* check if we already read the file */
	if (mechs[0].name != NULL)
		return (1);

	fd = fopen(MECH_OID_FILE, "r");
	if (fd == (FILE *) 0) {
		fprintf(stderr,
			gettext("\nInternal error opening mechanism"
				" file [%s].\n"),
			MECH_OID_FILE);
		return (0);
	}
	/*
	 * For each line in the file parse it appropriately.
	 * File format : mechname	oid(n.n.n.n.n.n)
	 */
	while (!feof(fd)) {
		line = fgets(buf, 256, fd);
		if (line == NULL)
			break;

		/* Skip comments and blank lines */
		if ((*line == '#') || (*line == '\n'))
			continue;

		name = &(buf[0]);
		while (isspace(*name))
			name++;
		if (*name == '\0')	/* blank line */
			continue;
		mechname = name;	/* will contain mech name */
		while (!isspace(*mechname))
			mechname++;
		if (*mechname == '\0') {
			fprintf(stderr,
				gettext("\nInternal error, invalid format"
					" in [%s].\n"),
				MECH_OID_FILE);
			continue;
		}
		next = mechname+1;
		*mechname = '\0';	/* null terminate the mech name */
		mechs[index].name = strdup(name);

		name = next;
		while (isspace(*name))
			name++;
		if (*name == '\0') { 	/* end of line, no oid */
			fprintf(stderr,
				gettext("\nInternal error, invalid"
					" format in [%s].\n"),
				MECH_OID_FILE);
			continue;
		}
		oid_str = name;	/* will contain oid (n.n.n.n.n.n) */
		while (!isspace(*oid_str))
			oid_str++;
		*oid_str++ = '\0';	/* null terminate oid name */
		mechs[index].oidStr = strdup(name);

		oidStrDesc.value = name;
		oidStrDesc.length = strlen(name);
		if (gss_str_to_oid(&minor, &oidStrDesc, &mechs[index].oid)
			!= GSS_S_COMPLETE)
			continue;

		if (index++ >= MAX_OID_MECH_PAIRS) {
		    fprintf(stderr,
			    gettext("\nInternal error, invalid format"
				    " in [%s].\n"),
			    MECH_OID_FILE);
		    break;
		}
	}
	fclose(fd);
	return (1);
} /* ******* _gsscred_read_mech_file  ******* */



/*
 * gsscred_read_config_file
 *
 * function to read the optional gsscred configuration file
 * which specifies which backend to use to store the gsscred
 * table.  When the configuration file is not found, the
 * xfn is used with system defined xfn back-end.
 *
 * function returns -1 to use the file; any other value is to
 * be interpreted by xfn. 0 is the default xfn backend.
 */
int
gsscred_read_config_file(void)
{
	FILE *fp;
	char line[255], *tmpPtr, *mechName;
	int backend = 0;

	/*
	* check for optional configuration file
	* which would select what mechanism to use under.
	*
	* the file has following format:
	* #comment
	* mech
	*/
	if ((fp = fopen(gsscred_config_file, "r")) != NULL) {
		while (fgets(line, 255, fp) != NULL) {

			/* check for comments or blanks */
			if (*line == '#' || *line == '\n')
				continue;

			/* skip whitespace chars */
			tmpPtr = line;
			while (isspace(*tmpPtr))
				tmpPtr++;

			/* check for blank line */
			if (*tmpPtr == '\0')
				continue;

			mechName = tmpPtr;

			/* trim, the input */
			while (!isspace(*tmpPtr))
				tmpPtr++;
			*tmpPtr = '\0';

			/* should contain a mech specification keyword */
			if (strcmp(mechName, xfn_mech_files) == 0)
				backend = 3;
			else if (strcmp(mechName, xfn_mech_nis) == 0)
				backend = 2;
			else if (strcmp(mechName, xfn_mech_nisplus) == 0)
				backend = 1;
			else if (strcmp(mechName, mech_flat_file) == 0)
				backend = GSSCRED_FLAT_FILE;

			break;
		}
		fclose(fp);
	}

	return (backend);
} /* gsscred_read_config_file */


/*
 * Converts a string named mechanism to a gss_OID.
 */
gss_OID
gsscred_MechNameToOid(const char *mech)
{
	struct mechDesc *aMech;

	/* ensure that we have all the mechanism definitions */
	if (!_gsscred_read_mech_file())
		return (0);

	aMech = mechs;
	while (aMech->name != NULL) {
		if (strcmp(aMech->name, mech) == 0)
			return (aMech->oid);
		aMech++;
	}
	return (NULL);
} /* ******* gsscred_MechNameToOid ****** */



/*
 * gsscred_MakeName
 *
 * construct a principal name in the GSS_C_NT_EXPORT_NAME format.
 */
int gsscred_MakeName(const gss_OID mechOid, const char *name,
		const char *nameOidStr, gss_buffer_t nameOut)
{
	gss_OID nameOid;
	gss_name_t intName;
	OM_uint32 minor, major;
	gss_buffer_desc aName = GSS_C_EMPTY_BUFFER, oidStr;

	nameOut->length = 0;
	nameOut->value = NULL;

	/* we need to import the name, then canonicalize it, then export it */
	if (nameOidStr == NULL)
		nameOid = (gss_OID)GSS_C_NT_USER_NAME;
	else {
		oidStr.length = strlen(nameOidStr);
		oidStr.value = (void *)nameOidStr;
		if (gss_str_to_oid(&minor, &oidStr, &nameOid) !=
			GSS_S_COMPLETE) {
			fprintf(stderr,
				gettext("\nInvalid name oid supplied [%s].\n"),
				nameOidStr);
			return (0);
		}
	}

	/* first import the name */
	aName.length = strlen(name);
	aName.value = (void*)name;
	major = gss_import_name(&minor, &aName, nameOid, &intName);
	if (nameOidStr != NULL) {
		free(nameOid->elements);
		free(nameOid);
	}

	if (major != GSS_S_COMPLETE) {
		fprintf(stderr,
			gettext("\nInternal error importing name [%s].\n"),
			name);
		return (0);
	}

	/* now canonicalize the name */
	if (gss_canonicalize_name(&minor, intName, mechOid, NULL)
	    != GSS_S_COMPLETE) {
		fprintf(stderr,
			gettext("\nInternal error canonicalizing name"
				" [%s].\n"),
			name);
		gss_release_name(&minor, &intName);
		return (0);
	}

	/* now convert to export format */
	if (gss_export_name(&minor, intName, nameOut) != GSS_S_COMPLETE) {
		fprintf(stderr,
			gettext("\nInternal error exporting name [%s].\n"),
			name);
		gss_release_name(&minor, &intName);
		return (0);
	}

	gss_release_name(&minor, &intName);
	return (1);
} /* *******  makeName ****** */


/*
 * Constructs a part of the GSS_NT_EXPORT_NAME
 * Only the mechanism independent name part is created.
 */
int
gsscred_MakeNameHeader(const gss_OID mechOid, gss_buffer_t outNameHdr)
{
	unsigned char *buf = NULL;

	/* determine the name of buffer needed */
	outNameHdr->length = mechOidLenLen + expNameTokIdLen + mechOid->length;
	if ((outNameHdr->value = (void*) malloc(outNameHdr->length)) == NULL) {
		outNameHdr->length = 0;
		return (0);
	}

	/* start by putting the token id */
	buf = (unsigned char *) outNameHdr->value;
	memset(outNameHdr->value, '\0', outNameHdr->length);
	memcpy(buf, expNameTokId, expNameTokIdLen);
	buf += expNameTokIdLen;

	/* next 2 bytes contain the mech oid length */
	*buf++ = (mechOid->length & 0xFF00) >> 8;
	*buf++ = (mechOid->length & 0x00FF);

	/* now add the mechanism oid */
	memcpy(buf, mechOid->elements, mechOid->length);

	/* we stop here because the rest is mechanism specific */
	return (1);
} /* gsscred_MakeNameHeader */


/*
 * Converts the supplied string to HEX.
 * The passed in buffer must be twice as long as the input buffer.
 * Long form is used (i.e. '\0' will become '00').  This is needed
 * to enable proper re-parsing of names.
 */
int
gsscred_AsHex(gss_buffer_t dataIn, gss_buffer_t dataOut)
{
	int i;
	char *out, *in;
	unsigned int tmp;

	if (dataOut->length < ((dataIn->length *2) + 1))
		return (0);

	out = (char *)dataOut->value;
	in = (char *)dataIn->value;
	dataOut->length = 0;

	for (i = 0; i < dataIn->length; i++) {
		tmp = (unsigned int)(*in++)&0xff;
		sprintf(out, "%02X", tmp);
		out++;
		out++;
	}
	dataOut->length = out - (char *)dataOut->value;
	*out = '\0';

	return (1);
} /* ******* gsscred_AsHex ******* */


/*
 * GSS entry point for retrieving user uid mappings.
 * The name buffer contains a principal name in exported format.
 */
int
gss_getGssCredEntry(const gss_buffer_t expName, uid_t *uid)
{
	int tableSource;
	unsigned char *buf;
	gss_buffer_desc mechOidDesc = GSS_C_EMPTY_BUFFER,
		mechHexOidDesc = GSS_C_EMPTY_BUFFER,
		expNameHexDesc = GSS_C_EMPTY_BUFFER;
	char oidHexBuf[256], expNameHexBuf[1024];
	GssCredEntry *userInfo;


	tableSource = gsscred_read_config_file();

	/*
	 * for xfn, we must first construct, a hex mechansim oid string
	 */
	if (expName->length < (expNameTokIdLen + mechOidLenLen))
	    return (0);

	buf = (unsigned char *)expName->value;
	buf += expNameTokIdLen;

	/* extract the mechanism oid length */
	mechOidDesc.length = (*buf++ << 8);
	mechOidDesc.length |= (*buf++);
	if (expName->length <
		(expNameTokIdLen + mechOidLenLen + mechOidDesc.length))
		return (0);

	mechOidDesc.value = (void *)buf;

	/* convert the oid buffer to hex */
	mechHexOidDesc.value = (void*) oidHexBuf;
	mechHexOidDesc.length = sizeof (oidHexBuf);
	if (!gsscred_AsHex(&mechOidDesc, &mechHexOidDesc))
		return (0);

	/* also need to convert the name buffer into hex */
	expNameHexDesc.value = expNameHexBuf;
	expNameHexDesc.length = sizeof (expNameHexBuf);
	if (!gsscred_AsHex(expName, &expNameHexDesc))
		return (0);

	if (tableSource == GSSCRED_FLAT_FILE)
		return (file_getGssCredUid(&expNameHexDesc, uid));

	/* must be stored in xfn */
	userInfo = xfn_getGssCredEntry(tableSource,
				(const char *)mechHexOidDesc.value,
				(const char *)expNameHexDesc.value,
				NULL, NULL);
	if (!userInfo)
		return (0);

	*uid = userInfo->unix_uid;
	free(userInfo->principal_name);
	free(userInfo->comment);
	return (1);
} /* gss_getGssCredEntry */
